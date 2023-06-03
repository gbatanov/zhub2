#include "../version.h"

#include <mutex>
#include <set>
#include <inttypes.h>
#include <optional>
#include <vector>
#include <map>
#include <queue>
#include <string>
#include <array>
#include <thread>
#include <chrono>
#include <math.h>
#include <any>
#include <sstream>
#include <termios.h>

#ifdef WITH_SIM800
#include "../modem.h"
extern GsmModem *gsmmodem;
#endif

#ifdef WITH_TELEGA
#include "../telebot32/src/tlg32.h"
extern std::unique_ptr<Tlg32> tlg32;
#endif

#include "../../gsb_utils/gsbutils.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../common.h"
#include "command.h"
#include "zigbee.h"
#include "../main.h"

using zigbee::IEEEAddress;
using zigbee::NetworkAddress;
using zigbee::NetworkConfiguration;
using zigbee::zcl::Attribute;
using zigbee::zcl::Cluster;
using zigbee::zcl::Frame;

extern std::atomic<bool> Flag;
extern gsbutils::TTimer ikeaMotionTimer;

std::mutex mtx_timer1;
uint16_t timer1_counter = 0;
uint64_t last_seen = 0;

std::atomic<int> kitchenSensorState{-1};
std::atomic<int> coridorSensorState{-1};

using gsb_utils = gsbutils::SString;
using namespace zigbee;

Zhub::Zhub() : Controller()
{
#ifdef WITH_TELEGA
    tlg_in = std::make_shared<gsbutils::Channel<TlgMessage>>(2);
    tlg_out = std::make_shared<gsbutils::Channel<TlgMessage>>(2);
    tlgInThread = new std::thread(&Zhub::handle, this);
#endif
}
Zhub::~Zhub()
{
#ifdef WITH_TELEGA
    tlg_in->stop();
    tlg_out->stop();
    if (tlgInThread->joinable())
        tlgInThread->join();
#endif
    disconnect();
}

// Старт приложения
void Zhub::start(std::vector<uint8_t> rfChannels)
{
    init();
    // Старт Zigbee-сети
    gsbutils::dprintf(1, "Zhub::start_network \n");
    while (!start_network(rfChannels) && Flag.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    if (!Flag.load())
        return;

#ifdef TEST
    // Датчик движения ИКЕА
    std::vector<uint16_t> idsOnoff{0x0000};
    uint16_t shortAddr = getShortAddrByMacAddr((zigbee::IEEEAddress)0x0c4314fffe17d8a8);
    read_attribute(shortAddr, zigbee::zcl::Cluster::ON_OFF, idsOnoff);
#else
    switch_relay(0x54ef4410005b2639, 0); // выключаем при старте реле "Туалет занят"
    // Получим состояние кранов (v2.21.506 - отдается только статус, напряжение/ток не отдается )
    std::vector<uint16_t> idsOnoff{0x0000};
    uint16_t shortAddr = getShortAddrByMacAddr((zigbee::IEEEAddress)0xa4c138d9758e1dcd);
    read_attribute(shortAddr, zigbee::zcl::Cluster::ON_OFF, idsOnoff);
    shortAddr = getShortAddrByMacAddr((zigbee::IEEEAddress)0xa4c138373e89d731);
    read_attribute(shortAddr, zigbee::zcl::Cluster::ON_OFF, idsOnoff);
#endif
}
//
std::string Zhub::get_device_list(bool as_html)
{
    std::string result = "";
    for (auto inf : end_devices_address_map_)
    {
        char buff[32]{0};
        uint16_t shortAddr = inf.first;
        uint64_t IEEEaddr = static_cast<const uint64_t>(inf.second);
        snprintf(buff, 32, "0x%04x", shortAddr);
        try
        {
            DeviceInfo di = EndDevice::KNOWN_DEVICES.at(IEEEaddr);
            if (as_html)
                result += "<p>";
            result += "Device " + std::string(buff) + " " + di.humanName;
            if (as_html)
                result += "</p>";
            else
                result += "\n";
        }
        catch (std::out_of_range &e)
        {
            if (as_html)
                result += "<p>";
            result += "Device " + std::string(buff) + " отсутствует в списке известных устройств";
            if (as_html)
                result += "</p>";
            else
                result += "\n";
        }
    }
    return result;
}

// обработка команды от активного IAS_ZONE устройства
// в моей конструкции - датчики протечек
// нужно выключить краны и реле стиральной машины
//{0xa4c138d9758e1dcd, {"Water Valve", "TUYA", "Valve", "Кран 1 ГВ", zigbee::zcl::Cluster::ON_OFF}},
//{0xa4c138373e89d731, {"Water Valve", "TUYA", "Valve", "Кран 2 ХВ", zigbee::zcl::Cluster::ON_OFF}}
//{0x54ef441000193352, {"lumi.switch.n0agl1", "Xiaomi", "SSM-U01", "Реле 2(стиральная машина)", zigbee::zcl::Cluster::ON_OFF}}
// контактор стиральной машины нормально-замкнутый,
// при включении исполнительного реле контактор выключается
void Zhub::ias_zone_command(uint8_t cmnd, uint16_t one)
{
    uint64_t executive_devices[3]{
        0xa4c138d9758e1dcd,
        0xa4c138373e89d731,
        0x54ef441000193352

    };
    uint8_t cmd = cmnd; // автоматически только выключаем,
    // команду включения/переключения используем только через веб-апи
    if (one)
    {
        // одно устройство, команда из веб-апи
        auto device = devices_.find(0x54ef441000193352); // реле контактора стиралки
        if (device != devices_.end() && device->second->getNetworkAddress() == one)
        {
            cmd = cmd == 0 ? 0x1 : 0x0;
        }

        sendCommandToOnOffDevice(one, cmd);
        gsbutils::dprintf(1, "Close device 0x%04x\n", one);
    }
    else
    {
        // все устройства
        for (uint64_t mac_addr : executive_devices)
        {
            uint8_t cmd1 = cmd;
            if (mac_addr == 0x54ef441000193352)
                cmd1 = cmd == 0 ? 0x1 : 0x0;
            auto device = devices_.find(mac_addr);
            if (device != devices_.end())
            {
                std::time_t ts = std::time(0); // get time now
                device->second->set_last_action((uint64_t)ts);

                // краны закрываются медленно, запускаем все в параллельных потоках
                // это бывает крайне редко, исключительный случай, поэтому потоки можно детачить
                std::thread of_action([this, cmd1, device]
                                      {
                                                        this->sendCommandToOnOffDevice(device->second->getNetworkAddress(), cmd1);
                                                        gsbutils::dprintf(1, "Control device 0x%04x\n", device->second->getNetworkAddress()); });
                of_action.detach();
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
    }
}
void Zhub::ias_zone_command(uint8_t cmnd, uint64_t mac_addr)
{
    if (!mac_addr)
        ias_zone_command(cmnd, (uint16_t)0);
    else
    {
        auto shortAddr = devices_.find(mac_addr);
        if (shortAddr != devices_.end())
            ias_zone_command(cmnd, static_cast<uint16_t>(shortAddr->second->getNetworkAddress()));
    }
}

// Обработчик датчиков открытия дверей
void Zhub::handle_sonoff_door(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd)
{
    std::time_t ts = std::time(0); // get time now
    ed->set_last_action((uint64_t)ts);

    if (ed->getIEEEAddress() == 0x00124b002512a60b) // sensor2
    {
        ed->set_current_state(cmd ? "Opened" : "Closed");
        // управляем реле 3, включаем подсветку в большой комнате в шкафу
        switch_relay(0x54ef44100018b523, cmd);
    }
    else if (ed->getIEEEAddress() == 0x00124b00250bba63) // sensor 3
    {
        ed->set_current_state(cmd ? "Opened" : "Closed");
        std::string alarm_msg = "Закрыт ящик ";
        // информируем об открытии верхнего ящика
        if (cmd == 0x01)
            alarm_msg = "Открыт ящик ";
        tlg32->send_message(alarm_msg);
    }
    else if (ed->getIEEEAddress() == 0x00124b0025485ee6) // sensor 1 датчик света в туалете
    {
        // с версии 2.22.517 логику инвертируем
        ed->set_current_state(cmd ? "Closed" : "Opened");
        // с версии 2.22.517 логику инвертируем
        // свет "Туалет занят" в коридоре включаем только в период с 9 до 22 часов дня
        // выключаем в любое время (чтобы погасить ручное включение)
        // включаем/выключаем реле 0x54ef4410005b2639 - реле включения света "Занято"
        if (cmd == 0x01)
        {
            gsbutils::dprintf(6, "Выключение реле Туалет занят\n");
            switch_relay(0x54ef4410005b2639, 0);
            // switch_relay(0x54ef4410005b2639, 0);
        }
        else if (cmd == 0)
        {
            uint8_t h = gsbutils::DDate::get_hour_of_day();
            if (h > 7 && h < 23)
            {
                gsbutils::dprintf(6, "Включение реле Туалет занят\n");
                switch_relay(0x54ef4410005b2639, 1);
                // switch_relay(0x54ef4410005b2639, 1);
            }
        }
    }
    else
    {
        gsbutils::dprintf(2, "Unknown handle_sonoff_door\n");
    }
}
// Обработчик срабатываний датчиков движения (датчики движения Sonoff, датчики присутствия, датчики движения в кастомных сборках)
// Датчик Sonoff посылает сигнал "On" при первом обнаружении движения
// и сигнал "Off" через 1 минуту после прекращения движения
// cmd - 1 - motion, 0 - no motion (если в устройстве сигнал инвертирован,
//                                  он должен быть возвращен к правильному уровню ДО вызова этой функции)
void Zhub::handle_motion(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd)
{
    int dbg = 7;
    std::string state = "";
    int8_t cur_motion = ed->get_motion_state();
    // Зафиксируем последнюю активность датчика движения
    // Фиксируем в активности только включение датчика
    // Поскольку датчик движения есть и в кастомах, которые шлют сообщения периодически,
    // нужно проверять текущее состояние и фиксировать изменение
    // на движение включаем что-то и ставим признак, что в доме кто-то есть для алгоритма отключения воды при пропадании электричества

    if (cmd != cur_motion)
    {
        if (cmd == 1)
        {
            setLastMotionSensorActivity(std::time(nullptr));
            std::time_t ts = std::time(0); // get time now
            ed->set_last_action((uint64_t)ts);
        }
    }
    ed->set_motion_state(cmd);                           // числовое значение
    ed->set_current_state(cmd ? "Motion" : "No motion"); // текстовое значение

    uint64_t mac_address = (uint64_t)ed->getIEEEAddress();

    if (mac_address == 0x00124b0025137475) // датчик движения 1 (коридор) Sonoff
    {
        int8_t lum = -1;

        //  включаем/выключаем  свет в коридоре 0x54ef4410001933d3
        //  работает в паре с custom2
        std::shared_ptr<zigbee::EndDevice> custom2 = get_device_by_mac((zigbee::IEEEAddress)0x00124b0014db2724);
        if (custom2)
            lum = custom2->get_luminocity();

        gsbutils::dprintf(dbg, "Датчик в коридоре. cmd = %d, lum = %d\n", cmd, lum);
        if (cmd == 1)
        {
            if (lum != 1)
            {
                gsbutils::dprintf(dbg, "Датчик в коридоре. Включаем реле. \n");
                switch_relay(0x54ef4410001933d3, 1);
                coridorSensorState.store(1);
            }
        }
        else if (cmd == 0)
        {
            if (custom2)
                cur_motion = custom2->get_motion_state();
            if (1 != cur_motion)
            {
                gsbutils::dprintf(dbg, "Датчик в коридоре. Выключаем реле.\n");
                switch_relay(0x54ef4410001933d3, 0);
                coridorSensorState.store(0);
            }
        }
    }
    else if (mac_address == 0x00124b0014db2724)
    {
        // датчик движения в custom2 (прихожая)
        // тут надо учитывать освещенность при включении и состояние датчика движения в коридоре
        int8_t lum = ed->get_luminocity();
        gsbutils::dprintf(6, "Датчик в custom2. cmd = %d, lum = %d\n", cmd, lum);

        std::shared_ptr<zigbee::EndDevice> relay = get_device_by_mac((zigbee::IEEEAddress)0x54ef4410001933d3);
        std::string relayCurrentState = relay->get_current_state();

        if (cmd == 1 && relayCurrentState != "On")
        {
            // поскольку датчик иногда ложно срабатывает, на ночь игнорируем его срабатывание
            uint8_t h = gsbutils::DDate::get_hour_of_day();
            if (h > 7 && h < 23)
            {
                if (lum != 1)
                {
                    gsbutils::dprintf(6, "Датчик в прихожей. Включаем реле. \n");

                    switch_relay(0x54ef4410001933d3, 1);
                    coridorSensorState.store(1);
                }
            }
        }

        else if (cmd == 0 && relayCurrentState != "Off")
        {
            std::shared_ptr<zigbee::EndDevice> motion1 = get_device_by_mac((zigbee::IEEEAddress)0x00124b0025137475);
            if (motion1)
                cur_motion = motion1->get_motion_state();
            if (cur_motion != 1)
            {
                gsbutils::dprintf(6, "Датчик в прихожей. Выключаем реле. \n");

                switch_relay(0x54ef4410001933d3, 0);
                coridorSensorState.store(0);
            }
        }
    }
    else if (mac_address == 0x00124b0009451438)
    {
        std::shared_ptr<zigbee::EndDevice> relay = get_device_by_mac((zigbee::IEEEAddress)0x00158d0009414d7e);
        std::string relayCurrentState = relay->get_current_state(1);
        // датчик присутствия 1, включаем/выключаем свет на кухне - реле 7 endpoint 1
        if (cmd == 1 && relayCurrentState != "On")
        {
            gsbutils::dprintf(6, "Включаем свет на кухне");
            switch_relay(0x00158d0009414d7e, 1);
            kitchenSensorState.store(1);
            switch_relay(0x00158d0009414d7e, 1);
        }
        else if (cmd == 0 && relayCurrentState != "Off")
        {
            gsbutils::dprintf(6, "Выключаем свет на кухне");
            switch_relay(0x00158d0009414d7e, 0);
            kitchenSensorState.store(0);
            switch_relay(0x00158d0009414d7e, 0);
        }
    }
#ifdef DEBUG
    else if (mac_address == 0x00124b002444d159)
    {
        // датчик движения 3, детская
        gsbutils::dprintf(7, "Motion3 %s \n", cmd == 1 ? "On" : "Off");
    }
    else if (mac_address == 0x00124b0024455048)
    {
        // датчик движения 2 (комната)
        gsbutils::dprintf(7, "Motion2 %s \n", cmd == 1 ? "On" : "Off");
    }
#endif
    else if (mac_address == 0x0c4314fffe17d8a8)
    {
        // датчик движения IKEA
        gsbutils::dprintf(6, "IKEA MotionSensor %s \n", cmd == 1 ? "On" : "Off");
        // выключится таймером
        if (cmd == 1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(120));
            handle_motion(ed, 0);
            // ikeaMotionTimer.run(180);
        }
    }
    else if (mac_address == 0x00124b0007246963)
    {
        // датчик движения в Custom3(детская)
    }
}

// обработка команды от активного ALARMS устройства
void Zhub::alarms_command(zigbee::Message message)
{
    gsbutils::dprintf(7, "Zhub::alarms_command\n");
}

// обработка команды от активного ONOFF устройства
// 0x01 On
// 0x02 Toggle
// 0x40 Off with effect
// 0x41 On with recall global scene
// 0x42 On with timed off  payload:0x00 (исполняем безусловно) 0x08 0x07(ON на 0x0708 (180,0)секунд) 0x00 0x00
void Zhub::onoff_command(zigbee::Message message)
{
#ifdef DEBUG
    gsbutils::dprintf(1, "Zhub::onoff_command address 0x%04x command 0x%02x \n", message.source.address, (uint8_t)message.zcl_frame.command);
#endif
    //  message.source.address - источник
    std::shared_ptr<zigbee::EndDevice> ed = get_device_by_short_addr(message.source.address);
    if (!ed)
    {
        gsbutils::dprintf(1, "Незарегистрированное устройство\n");
        return;
    }
    uint64_t mac_address = (uint64_t)ed->getIEEEAddress();
    uint8_t cmd = (uint8_t)message.zcl_frame.command;
    ed->set_current_state(cmd ? "On" : "Off");
    std::time_t ts = std::time(0); // get time now
    ed->set_last_action((uint64_t)ts);

    if (mac_address == 0x8cf681fffe0656ef)
    {
        // Кнопка ИКЕА
        if (ed->check_last_power_query())
            get_power(message.source.address, zigbee::zcl::Cluster::POWER_CONFIGURATION);
        ikea_button_action(cmd);
    }
    else if (mac_address == 0x0c4314fffe17d8a8)
    {
        if (ed->check_last_power_query())
            get_power(message.source.address, zigbee::zcl::Cluster::POWER_CONFIGURATION);
        // Датчик движения IKEA. У этих датчиков нет команды выключения. Само устройство должно выключиться по таймеру.
        // Время в payload(1),payload(2) в десятых долях секунды
        handle_motion(ed, 1);
    }
    else if (mac_address == 0x00124b0028928e8a)
    {
        // Кнопка Sonoff1
        switch (cmd)
        {
        case 0:
        {
            ed->set_current_state("Long click"); // 1 - double , 2 - single, 0 - long
            // Буду выключать реле освещения и другие, входящие в список выключений по кнопке
            switch_off_with_list();
        }
        break;
        case 1:
            ed->set_current_state("Double click"); // 1 - double , 2 - single, 0 - long
            break;
        case 2:
            ed->set_current_state("Single click"); // 1 - double , 2 - single, 0 - long
            break;
        }
    }
    else if (mac_address == 0x00124b00253ba75f)
    {
        // Кнопка Sonoff 2 будет вызывать звонок на двойное нажатие
        // Выключаем реле по списку при длительном нажатии
        switch (cmd)
        {
        case 0:
            ed->set_current_state("Long click"); // 1 - double , 2 - single, 0 - long
            // Буду выключать реле освещения и другие, входящие в список выключений по кнопке
            switch_off_with_list();

            break;
        case 1:
        {
            ed->set_current_state("Double click"); // 1 - double , 2 - single, 0 - long
            std::string alarm_msg = "Вызов с кнопки ";
            ringer();
#ifdef WITH_TELEGA
            tlg32->send_message(alarm_msg);
#endif
        }
        break;
        case 2:
            ed->set_current_state("Single click"); // 1 - double , 2 - single, 0 - long
            break;
        }
    }
}
// обработка команды LEVEL от активного ON_OFF устройства
// Сейчас это кнопка ИКЕА
void Zhub::level_command(zigbee::Message message)
{
    //  message.source.address - источник
    std::shared_ptr<zigbee::EndDevice> ed = get_device_by_short_addr(message.source.address); // кнопка IKEA - 0x8cf681fffe0656ef
    if (!ed)
    {
        gsbutils::dprintf(1, "Незарегистрированное устройство\n");
        return;
    }
    std::time_t ts = std::time(0); // get time now
    ed->set_last_action((uint64_t)ts);

    uint8_t cmd1 = 0;
    if (message.zcl_frame.payload.size() > 0)
        uint8_t cmd1 = (uint8_t)message.zcl_frame.payload.at(0);
    uint8_t cmd = (uint8_t)message.zcl_frame.command; // 5 - Удержание + 7 - кнопка отпущена, 1 - Удержание -

    gsbutils::dprintf(1, "Кнопка ИКЕА level command: %d %d \n", cmd1, cmd);

    switch (cmd)
    {
    case 1: // Удержание
        gsbutils::dprintf(1, "Кнопка ИКЕА: удержание минус\n");
        ed->set_current_state("Minus down");

        break;
    case 5:
        ed->set_current_state("Plus down");
        gsbutils::dprintf(1, "Кнопка ИКЕА: удержание плюс\n");

        break;
    case 7:
    {
        ed->set_current_state("No act");
        gsbutils::dprintf(1, "Кнопка ИКЕА: удержание прекратилось\n");
    }
    break;
    }
}
// обработка сообщения высокой температуры
void Zhub::height_temperature(uint16_t short_addr)
{
    // TODO: отправить сообщение в телеграм или смс
    // выключить устройство
    gsbutils::dprintf(7, "Zhub::height_temperature\n");
}

// Обработка кратковременных нажатий (Вкл/Выкл) кнопки ИКЕА
// Включаем/выключаем подсветку шкафа в комнате
void Zhub::ikea_button_action(uint8_t cmd)
{
    gsbutils::dprintf(1, "Кнопка ИКЕА: " + std::string(cmd == 1 ? "Включить\n" : "Выключить\n"));
    // управляем реле 3, включаем подсветку в большой комнате в шкафу
    switch_relay(0x54ef44100018b523, cmd);
}

// Отдает состояние датчиков протечки для СМС-сообщения
std::string Zhub::get_leak_state()
{
    std::string state{};

    uint64_t leak_devices[4]{
        0x00158d0006e469a4,
        0x00158d0006f8fc61,
        0x00158d0006b86b79,
        0x00158d0006ea99db};

    for (uint64_t mac_addr : leak_devices)
    {
        auto device = devices_.find(mac_addr);
        if (device != devices_.end())
        {
            state = state + device->second->get_current_state() + " ";
        }
    }
    return state;
}
// Отдает состояние датчиков движения для СМС-сообщения
// Кухню пока исключаю из процесса из-за нестабильности работы датчика
std::string Zhub::get_motion_state()
{
    std::string state{};

    std::vector<uint64_t> motion_devices{
        0x00124b0025137475,  // 1 коридор
        0x00124b0024455048,  // 2 комната
        0x00124b002444d159,  // 3 детская
                             //        0x00124b0009451438,  //  кухня
                             //       0x0c4314fffe17d8a8, // ikea
        0x00124b0014db2724,  // прихожая
        0x00124b0007246963}; // балкон

    for (uint64_t mac_addr : motion_devices)
    {
        auto device = devices_.find(mac_addr);
        if (device != devices_.end())
        {
            state = state + device->second->get_current_state() + " ";
        }
    }
    return state;
}

// Если на момент опроса есть хотя бы один датчик движения в статусе Motion,
// фиксируем факт наличия движения
// Кухню пока исключаю из процесса из-за нестабильности работы датчика
void Zhub::check_motion_activity()
{
    std::string what{"Motion"};
    std::string::size_type pos{};
    std::string state = get_motion_state(); // содержит Motion No motion через пробел, проверяем Motion

    if (state.npos != (pos = state.find(what.data(), 0, what.length())))
    {
        setLastMotionSensorActivity(std::time(nullptr));
    }
}

#define EMPTY_ROW \
    if (as_html)  \
        result = result + "<tr class='empty'><td colspan='8'><hr></td></tr>";

#ifdef TEST
#define EDCHECK (ed && ed->deviceInfo.test)
#else
#define EDCHECK (ed && ed->deviceInfo.available)
#endif
/// Текущее состояние устройств
/// используют http_server telegram
std::string Zhub::show_device_statuses(bool as_html)
{
    std::string result = "";
    char buff[1024]{0};

    const std::vector<uint64_t> ClimatSensors = {0x00124b000b1bb401};
    const std::vector<uint64_t> WaterSensors = {0x00158d0006e469a4, 0x00158d0006f8fc61, 0x00158d0006b86b79, 0x00158d0006ea99db};
    const std::vector<uint64_t> WaterValves = {0xa4c138d9758e1dcd, 0xa4c138373e89d731};
    const std::vector<uint64_t> MotionSensors = {0x00124b0007246963, 0x00124b0014db2724, 0x00124b0025137475, 0x00124b0024455048, 0x00124b002444d159, 0x00124b0009451438, 0x0c4314fffe17d8a8};
    const std::vector<uint64_t> DoorSensors = {0x00124b0025485ee6, 0x00124b002512a60b, 0x00124b00250bba63};
    const std::vector<uint64_t> Relays = {0x54ef44100019335b, 0x54ef441000193352, 0x54ef4410001933d3, 0x54ef44100018b523, 0x54ef4410005b2639, 0x54ef441000609dcc, 0x00158d0009414d7e};
    const std::vector<uint64_t> SmartPlugs = {0x70b3d52b6001b4a4, 0x70b3d52b6001b5d9, 0x70b3d52b60022ac9, 0x70b3d52b60022cfd};
    const std::vector<uint64_t> Buttons = {0x00124b0028928e8a, 0x00124b00253ba75f, 0x8cf681fffe0656ef};

    std::vector<const std::vector<uint64_t> *> allDevices{&ClimatSensors, &MotionSensors, &WaterSensors, &DoorSensors, &Relays, &SmartPlugs, &WaterValves, &Buttons};

    try
    {
        if (as_html)
        {
            result = result + "<table>";
            result = result + "<tr><th>Адрес</th><th>Название</th><th>Статус</th><th>LQ</th><th>Температура<th>Питание</th><th>Last seen/action</th></tr>";
        }

        for (const std::vector<uint64_t> *dev : allDevices)
        {
            EMPTY_ROW

            for (uint64_t di : *dev)
            {
                std::shared_ptr<zigbee::EndDevice> ed = get_device_by_mac((zigbee::IEEEAddress)di);
                if EDCHECK
                {
                    result = result + show_one_type(ed, as_html);
                }
            }
        }

        EMPTY_ROW

        if (as_html)
        {
            result = result + "</table>";
            result = result + "<table>";
            result = result + "<th>Комната</th><th>Температура</th><th>Влажность</th><th>Давление</th>";
            std::shared_ptr<zigbee::EndDevice> ed = get_device_by_mac((zigbee::IEEEAddress)0x00124b000b1bb401);
            if (ed)
            {
                result = result + "<tr><td>Балкон</td>";
                if ((int)ed->get_temperature() > -90)
                    result = result + "<td>" + std::to_string((int)ed->get_temperature()) + "</td>";
                else
                    result = result + "<td>&nbsp;</td>";
                if ((int)ed->get_humidity() > -90)
                    result = result + "<td>" + std::to_string((int)ed->get_humidity()) + "</td>";
                else
                    result = result + "<td>&nbsp;</td>";
                if ((int)ed->get_pressure() > -90)
                    result = result + "<td>" + std::to_string((int)(ed->get_pressure())) + "</td>";
                else
                    result = result + "<td>&nbsp;</td></tr>";
            }
            ed = get_device_by_mac((zigbee::IEEEAddress)0x00124b0007246963);
            if (ed)
            {
                result = result + "<tr><td>Детская</td>";
                if ((int)ed->get_temperature() > -90)
                    result = result + "<td>" + std::to_string((int)ed->get_temperature()) + "</td>";
                else
                    result = result + "<td>&nbsp;</td>";
                if ((int)ed->get_humidity() > -90)
                    result = result + "<td>" + std::to_string((int)ed->get_humidity()) + "</td>";
                else
                    result = result + "<td>&nbsp;</td>";
                if ((int)ed->get_pressure() > -90)
                    result = result + "<td>" + std::to_string((int)(ed->get_pressure())) + "</td>";
                else
                    result = result + "<td>&nbsp;</td></tr>";
            }
            result = result + "</table>";

            return result;
        }

        // Дальше выводится только в телеграм
        float temp = get_board_temperature();
        if (temp)
        {
            size_t len = snprintf(buff, 1024, "Температура платы управления: %0.1f \n", temp);
            buff[len] = 0;
            result = result + std::string(buff);
        }
        std::time_t lastMotionSensorAction = getLastMotionSensorActivity();
        std::tm tm = *std::localtime(&lastMotionSensorAction);
        size_t len = std::strftime(buff, sizeof(buff) / sizeof(buff[0]), " %Y-%m-%d %H:%M:%S", &tm);
        buff[len] = 0;
        result = result + "Последнее движение в " + std::string(buff) + "\n";
    }
    catch (std::exception &e)
    {
        result = result + "Error " + std::string(e.what()) + "\n";
    }

    return result;
}

std::string Zhub::show_one_type(std::shared_ptr<zigbee::EndDevice> ed, bool as_html)
{
    std::string result{};
    char buff[1024]{0};
    if (as_html)
    {
        result = result + "<tr>";
    }
    size_t len = snprintf(buff, 1024, "0x%04x", ed->getNetworkAddress());
    buff[len] = 0;
    if (as_html)
    {
        result = result + "<td class='addr'>" + std::string(buff) + "</td><td>" + ed->get_human_name() + "</td><td>";
        result = result + ed->get_current_state();
        if (ed->get_device_type() == 11)
            result = result + "/" + ed->get_current_state(2);
        result = result + "</td><td class='lq'>" + std::to_string(ed->get_linkquality()) + "</td>";
    }
    else
    {
        result = result + std::string(buff) + " " + ed->get_human_name() + " " + ed->get_current_state();
        if (ed->get_device_type() == 11)
            result = result + "/" + ed->get_current_state(2);
        result = result + " LQ: " + std::to_string(ed->get_linkquality());
    }
    if ((int)ed->get_temperature() > -90)
    {
        if (as_html)
        {
            result = result + "<td class='temp'>" + std::to_string((int)ed->get_temperature()) + "</td>";
        }
        else
            result = result + " Tmp: " + std::to_string((int)ed->get_temperature());
    }
    else
    {
        if (as_html)
            result = result + "<td>&nbsp;</td>";
    }

    std::string power_src = "";
    if (ed->deviceInfo.powerSource == zigbee::zcl::Attributes::PowerSource::BATTERY)
    {
        std::string bat = ed->show_battery_voltage();
        std::string batRm = ed->show_battery_remain();
        power_src = bat;
        if (batRm.size() > 0)
        {
            if (bat.size() > 0)
            {
                power_src += " / ";
            }
            power_src += batRm;
        }
        if (power_src.size() == 0)
            power_src = "Battery";
    }
    else
    {
        std::string voltage = ed->show_mains_voltage();
        std::string curr = ed->show_current();
        power_src = voltage;
        if (curr.size() > 0)
        {
            if (voltage.size() > 0)
            {
                power_src += " / ";
            }
            power_src += curr;
        }
        if (power_src.size() == 0)
            power_src = "SinglePhase";
    }

    if (as_html)
    {
        result = result + "<td>&nbsp;" + power_src + "</td>";
    }
    else
    {
        if (power_src.size() > 0)
            result = result + " Power: " + power_src;
    }

    uint64_t last_seen_ts = ed->get_last_seen();
    if (last_seen < last_seen_ts)
        last_seen = last_seen_ts;

    std::string last_seen_str = ed->show_last(0) + " / " + ed->show_last(1);
    if (as_html)
    {
        result = result + "<td>&nbsp;" + last_seen_str + "</td>";
    }
    else
    {
        if (last_seen_str.size() > 0)
            result = result + " Last seen: " + last_seen_str;
    }

    buff[0] = 0;
    if (as_html)
        result = result + "</tr>";
    else
        result = result + "\n";
    return result;
}

// Выключение реле по списку при долгом нажатии на кнопки Sonoff1 Sonoff2
inline void Zhub::switch_off_with_list()
{
    for (auto &mac_addr : EndDevice::OFF_LIST)
    {
        switch_relay(mac_addr, 0, 1);
        if (mac_addr == 0x00158d0009414d7e) // реле на кухне имеет два канала
            switch_relay(mac_addr, 0, 2);
    }
}

#ifdef WITH_TELEGA
// Здесь реализуется вся логика обработки принятых сообщений из телеграм
void Zhub::handle()
{
    while (Flag.load())
    {
        TlgMessage msg = tlg_in->read();
        if (!msg.text.empty())
        {
            TlgMessage answer{};
            answer.chat.id = msg.from.id;

            DBGLOG("Входное сообщение %s: %s \n", msg.from.firstName.c_str(), msg.text.c_str());
            if (!tlg32->client_valid(msg.from.id))
            {
                answer.text = "Я не понял Вас.\n";
                bool ret = tlg32->send_message(answer);
            }
            if (msg.text.starts_with("/start"))
                answer.text = "Привет, " + msg.from.firstName;
            else if (msg.text.starts_with("/stat"))
            {
                std::string stat = show_statuses();
                if (stat.size() == 0)
                    answer.text = "Нет ответа\n";
                else
                    answer.text = stat;
            }
            else if (msg.text.starts_with("/balance"))
            {
#ifdef WITH_SIM800
                if (gsmmodem->get_balance())
                    answer.text = "Запрос баланса отправлен\n";
#else
                answer.text = "SIM800 не подключен\n";
#endif
            }
            else if (msg.text.starts_with("/join"))
            {
                //            zhub->permitJoin(60s)
                answer.text = "Join 60 sec.\n";
            }
            else
                answer.text = "Я не понял Вас.\n";

            tlg_out->write(answer);
        }
    }
}
#endif
