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

#include "../../gsb_utils/gsbutils.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../common.h"
#include "command.h"
#include "zigbee.h"

#ifdef WITH_SIM800
#include "../modem.h"
extern GsmModem *gsmmodem;
#endif

#ifdef WITH_TELEGA
#include "../telebot32/src/tlg32.h"
extern std::unique_ptr<Tlg32> tlg32;
#endif

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

Coordinator::Coordinator() : Controller()
{
}
Coordinator::~Coordinator()
{
    disconnect();
}

// Старт координатора
// Если не передана конфигурация, берется дефолтная
void Coordinator::start(NetworkConfiguration configuration)
{
    // Старт Zigbee-сети
    gsbutils::dprintf(1, "Coordinator::startNetwork \n");

    while (!startNetwork(configuration) && Flag.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    if (!Flag.load())
        return;

    // получаем из файла связь сетевого и мак адресов, создаем объекты
    getDevicesMapFromFile(true);

    // разрешить прием входящих сообщений
    isReady_ = true;
    permitJoin(60s); // разрешаем джойн  при запуске на 1 минуту
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

// Функция вызывается периодически  для устройств, не передающих эти параметры самостоятельно.
// Ответ разбираем  в OnMessage
void Coordinator::getPower(zigbee::NetworkAddress address, Cluster cluster)
{
    gsbutils::dprintf(7, "Coordinator::getPower\n");

    Cluster cl = cluster;
    std::vector<uint16_t> pwr_attr_ids{};
    zigbee::Endpoint endpoint{address, 1};
    if (cl == Cluster::POWER_CONFIGURATION) // 0x0001
    {
        pwr_attr_ids.push_back(zigbee::zcl::Attributes::PowerConfiguration::MAINS_VOLTAGE);   // 0x0000 Напряжение основного питания в 0,1 В UINT16
        pwr_attr_ids.push_back(zigbee::zcl::Attributes::PowerConfiguration::BATTERY_VOLTAGE); // 0x0020 возвращает напряжение батарейки в десятых долях вольта UINT8
        pwr_attr_ids.push_back(zigbee::zcl::Attributes::PowerConfiguration::BATTERY_REMAIN);  // 0x0021 Остаток заряда батареи в процентах
    }
    else
    {
        cl = Cluster::BASIC;                                                  // 0x0000
        pwr_attr_ids.push_back(zigbee::zcl::Attributes::Basic::POWER_SOURCE); // 0x0007
    }
    Frame frame;
    // команда READ_ATTRIBUTES выполняется только с FrameType::GLOBAL
    frame.frame_control.type = zigbee::zcl::FrameType::GLOBAL;
    frame.command = zigbee::zcl::GlobalCommands::READ_ATTRIBUTES; // 0x00

    frame.frame_control.direction = zigbee::zcl::FrameDirection::FROM_CLIENT_TO_SERVER;
    frame.frame_control.disable_default_response = false;
    frame.frame_control.manufacturer_specific = false;
    frame.manufacturer_code = 0;

    frame.transaction_sequence_number = generateTransactionSequenceNumber();

    //  в payload список запрашиваемых аттрибутов
    for (uint16_t id : pwr_attr_ids)
    {
        frame.payload.push_back(LOWBYTE(id));
        frame.payload.push_back(HIGHBYTE(id));
    }

    sendMessage(endpoint, cluster, frame, 10s);
}

// Предполагаем, что мы физически не можем одновременно спаривать два устройства
// На самом деле, может быть и сразу кучей!!!
void Coordinator::onJoin(zigbee::NetworkAddress network_address, zigbee::IEEEAddress mac_address)
{
    int dbg = 1;

    gsbutils::dprintf(dbg, "Coordinator::onJoin mac_address 0x%" PRIx64 "\n", mac_address);
    joinDevice(network_address, mac_address, true);

    std::shared_ptr<zigbee::EndDevice> ed = get_device_by_mac(mac_address);
    if (!ed)
        return;

    // bind срабатывает только при спаривании/переспаривании устройства, повторный биндинг дает ошибку
    // ON_OFF попробуем конфигурить всегда
    // для кастомных устройств репортинг не надо конфигурить!!!
    bind(network_address, mac_address, 1, zigbee::zcl::Cluster::ON_OFF);
    if (ed->get_device_type() != 4)
        configureReporting(network_address, zigbee::zcl::Cluster::ON_OFF); //

#ifdef TEST
    // smart plug  - репортинга нет, данные получаем только по запросу аттрибутов
    if (ed->get_device_type() == 10)
    {
        bind(network_address, mac_address, 1, zigbee::zcl::Cluster::ELECTRICAL_MEASUREMENTS);
        configureReporting(network_address, zigbee::zcl::Cluster::ELECTRICAL_MEASUREMENTS, 0x0505, Attribute::DataType::UINT16, 0x00);
        configureReporting(network_address, zigbee::zcl::Cluster::ELECTRICAL_MEASUREMENTS, 0x0508, Attribute::DataType::UINT16, 0x00);
    }
/*
    // краны репортинг отключил, после настройки кран перестал репортить
    if ((uint64_t)mac_address == 0xa4c138373e89d731 || (uint64_t)mac_address == 0xa4c138d9758e1dcd)
    {
        bind(network_address, mac_address, 1, zigbee::zcl::Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER);
        configureReporting(network_address, zigbee::zcl::Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER);
        bind(network_address, mac_address, 1, zigbee::zcl::Cluster::TUYA_SWITCH_MODE_0);
        configureReporting(network_address, zigbee::zcl::Cluster::TUYA_SWITCH_MODE_0);
    }
    */
#endif

    // Датчики движения и открытия дверей Sonoff
    if (ed->get_device_type() == 2 || ed->get_device_type() == 3)
    {
        bind(network_address, mac_address, 1, zigbee::zcl::Cluster::IAS_ZONE);
        configureReporting(network_address, zigbee::zcl::Cluster::IAS_ZONE); //
    }
    // датчики движения IKEA
    if (ed->get_device_type() == 8)
    {
        bind(network_address, mac_address, 1, zigbee::zcl::Cluster::IAS_ZONE);
        configureReporting(network_address, zigbee::zcl::Cluster::IAS_ZONE); //
    }

    // для икеевских устройств необходимо запрашивать питание, когда от них приходит сообщение
    // у них есть POLL INTERVAL во время которого они могут принять комманду на исполнение
    if (ed->get_device_type() == 7 || ed->get_device_type() == 8)
    {
        getPower(network_address, zigbee::zcl::Cluster::POWER_CONFIGURATION);
    }
    // у остальных устройств надо сделать привязку, чтобы получить параметры питания (оставшийся заряд батареи)
    bind(network_address, mac_address, 1, zigbee::zcl::Cluster::POWER_CONFIGURATION);
    if (ed->get_device_type() != 4)
        configureReporting(network_address, zigbee::zcl::Cluster::POWER_CONFIGURATION);

#ifdef TEST
    // пока спящий датчик не проявится сам, инфу об эндпойнтах не получить, только при спаривании
    activeEndpoints(network_address);
    simpleDescriptor(network_address, 1); // TODO: получить со всех эндпойнотов, полученных на предыдущем этапе
#endif
    getIdentifier(network_address); // Для многих устройств этот запрос обязателен!!!! Без него не работатет устройство, только регистрация в сети
    gsbutils::dprintf(1, showDeviceInfo(network_address));
}

// Ничего не делаем, список поддерживается вручную
// из координатора запись удаляется самим координатором
void Coordinator::onLeave(NetworkAddress network_address, IEEEAddress mac_address)
{
    int dbg = 4;
    gsbutils::dprintf(dbg, "Coordinator::onLeave 0x%04x 0x%" PRIx64 " \n", (uint16_t)network_address, (uint64_t)mac_address);
}

/// Создаем новое устройство
/// Сохраняем ссылку на него в мапе
/// Перед добавлением старое устройство (если есть в списках) удаляем
/// TODO: перейти на фабрику классов по типам устройств
/// @param norec писать или нет в файл, по умолчанию - нет
void Coordinator::joinDevice(zigbee::NetworkAddress network_address, zigbee::IEEEAddress mac_address, bool norec)
{
    if (!norec)
        std::lock_guard<std::mutex> lg(mtxJoin);
    std::shared_ptr<EndDevice> ed = nullptr;
    // Надо проверить, есть ли в списке устройство с таким мак-адресом,
    // если нет, создаем устройство и добавляем в списки
    // если есть, проверяем его сетевой адрес,
    // если он совпадает, то ничего не делаем
    // если не совпадает, значит устройство получило новый адрес, выполняем процедуру с удалением старого адреса
    try
    {
        auto device = devices_.find(mac_address); // IEEEAddress служит ключом в мапе
        if (device != devices_.end())
        {
            gsbutils::dprintf(3, "Device exists\n");
            // устройство есть в списке устройств
            ed = device->second;
            zigbee::NetworkAddress shortAddr = ed->getNetworkAddress();
            if (shortAddr != network_address)
            {
                ed->setNetworkAddress(network_address);
                {
                    while (end_devices_address_map_.count(shortAddr))
                    {
                        end_devices_address_map_.erase(shortAddr);
                    }
                    end_devices_address_map_.insert({network_address, mac_address});
                }
                if (!norec)
                    setDevicesMapToFile();
            }
        }
        else
        {
            gsbutils::dprintf(3, "Device not exists\n");
            // устройство не найдено в списке устройств
            ed = std::make_shared<EndDevice>(network_address, mac_address);

            if (ed)
            {
                devices_.insert({mac_address, ed});
                end_devices_address_map_.insert({network_address, mac_address});
                if (!norec)
                    setDevicesMapToFile();
                gsbutils::dprintf(7, "Call ed->init()\n");
                ed->init();
            }
        }
    }
    catch (std::exception &e)
    {
        gsbutils::dprintf(1, "Coordinator::joinDevice error %s \n", e.what());
    }
}

std::string Coordinator::showDeviceInfo(zigbee::NetworkAddress network_address)
{
    std::string result = "";

    auto mac_address = end_devices_address_map_.find(network_address); // find ищет по ключу, а не по значению !!!

    if (mac_address != end_devices_address_map_.end())
    {
        std::shared_ptr<zigbee::EndDevice> ed = get_device_by_mac(mac_address->second);
        if (ed && !ed->getModelIdentifier().empty())
        {
            result = result + ed->getModelIdentifier();
            if (!ed->getManufacturer().empty())
            {
                result = result + " | " + ed->getManufacturer();
            }
            if (!ed->getProductCode().empty())
            {
                result = result + " | " + ed->getProductCode();
            }
            result = result + "\n";
        }
    }
    else
    {
        // такое бывает, после перезапуска программы - shortAddr уже есть у координатора, но объект не создан - TODO: уже не должно быть
        gsbutils::dprintf(1, "Coordinator::showDeviceInfo: shortAddr 0x%04x не найден в мапе адресов\n", network_address);
    }

    return result;
}

// Вызываем при Join and Leave
bool Coordinator::setDevicesMapToFile()
{

    std::string prefix = "/usr/local";

    int dbg = 4;
#ifdef TEST
    dbg = 2;
    std::string filename = prefix + "/etc/zhub2/map_addr_test.cfg";
#else
    std::string filename = prefix + "/etc/zhub2/map_addr.cfg";
#endif
    std::FILE *fd = std::fopen(filename.c_str(), "w");
    if (!fd)
    {
        gsbutils::dprintf(1, "Coordinator::setDevicesMapToFile: error opening file\n");
        return false;
    }

    for (auto m : end_devices_address_map_)
    {
        fprintf(fd, "%04x %" PRIx64 " \n", m.first, m.second); // лидирующие нули у PRIx64 не записываются, надо учитывать при чтении
        gsbutils::dprintf(dbg, "%04x %" PRIx64 " \n", m.first, m.second);
    }

    fclose(fd);
    gsbutils::dprintf(dbg, "Coordinator::setDevicesMapToFile: write map to file\n");

    return true;
}

// Вызываем сразу после старта конфигуратора
// заполняем std::map<zigbee::NetworkAddress, zigbee::IEEEAddress> end_devices_address_map_
// Для варианта на компе и малине делаю запись в файл и чтение из файла, ибо с памятью CC2531 есть проблемы
bool Coordinator::getDevicesMapFromFile(bool with_reg)
{

    int dbg = 7;
    gsbutils::dprintf(3, "Coordinator::getDevicesMap: START\n");

    std::map<uint16_t, uint64_t> item_data = readMapFromFile();

    for (auto b : item_data)
    {
        gsbutils::dprintf_c(dbg, "0x%04x 0x%" PRIx64 "\n", b.first, b.second);
        joinDevice(b.first, b.second); // тут запись в файл не требуется
    }
    gsbutils::dprintf(3, "Coordinator::getDevicesMap: FINISH\n");

    return true;
}

//
std::string Coordinator::get_device_list(bool as_html)
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

std::map<uint16_t, uint64_t> Coordinator::readMapFromFile()
{

    std::map<uint16_t, uint64_t> item_data{};
#ifdef TEST
    std::string filename = "/usr/local/etc/zhub2/map_addr_test.cfg";
#else
    std::string filename = "/usr/local/etc/zhub2/map_addr.cfg";
#endif
    std::FILE *fd = std::fopen(filename.c_str(), "r");

    if (fd)
    {
        unsigned int short_addr;
        uint64_t mac_addr;
        int r;
        do
        {
            r = fscanf(fd, "%04x %" PRIx64 "", &short_addr, &mac_addr);
            if (short_addr)
                item_data.insert({(uint16_t)short_addr, mac_addr});
            fgetc(fd);
        } while (r != EOF);
        fclose(fd);
    }
    else
    {
        gsbutils::dprintf(1, "File not found\n");
    }

    return item_data;
}
// обработка команды от активного IAS_ZONE устройства
// в моей конструкции - датчики протечек
// нужно выключить краны и реле стиральной машины
//{0xa4c138d9758e1dcd, {"Water Valve", "TUYA", "Valve", "Кран 1 ГВ", zigbee::zcl::Cluster::ON_OFF}},
//{0xa4c138373e89d731, {"Water Valve", "TUYA", "Valve", "Кран 2 ХВ", zigbee::zcl::Cluster::ON_OFF}}
//{0x54ef441000193352, {"lumi.switch.n0agl1", "Xiaomi", "SSM-U01", "Реле 2(стиральная машина)", zigbee::zcl::Cluster::ON_OFF}}
// контактор стиральной машины нормально-замкнутый,
// при включении исполнительного реле контактор выключается
void Coordinator::ias_zone_command(uint8_t cmnd, uint16_t one)
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
void Coordinator::ias_zone_command(uint8_t cmnd, uint64_t mac_addr)
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
void Coordinator::handle_sonoff_door(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd)
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
void Coordinator::handle_motion(std::shared_ptr<zigbee::EndDevice> ed, uint8_t cmd)
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
            std::this_thread::sleep_for(std::chrono::seconds(60));
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
void Coordinator::alarms_command(zigbee::Message message)
{
    gsbutils::dprintf(7, "Coordinator::alarms_command\n");
}

// обработка команды от активного ONOFF устройства
// 0x01 On
// 0x02 Toggle
// 0x40 Off with effect
// 0x41 On with recall global scene
// 0x42 On with timed off  payload:0x00 (исполняем безусловно) 0x08 0x07(ON на 0x0708 (180,0)секунд) 0x00 0x00
void Coordinator::onoff_command(zigbee::Message message)
{
#ifdef DEBUG
    gsbutils::dprintf(1, "Coordinator::onoff_command address 0x%04x command 0x%02x \n", message.source.address, (uint8_t)message.zcl_frame.command);
#endif
    //  message.source.address - источник
    std::shared_ptr<zigbee::EndDevice> ed = getDeviceByShortAddr(message.source.address);
    if (!ed)
        return;
    uint64_t mac_address = (uint64_t)ed->getIEEEAddress();
    uint8_t cmd = (uint8_t)message.zcl_frame.command;
    ed->set_current_state(cmd ? "On" : "Off");
    std::time_t ts = std::time(0); // get time now
    ed->set_last_action((uint64_t)ts);

    if (mac_address == 0x8cf681fffe0656ef)
    {
        // Кнопка ИКЕА
        getPower(message.source.address, zigbee::zcl::Cluster::POWER_CONFIGURATION);
        ikea_button_action(cmd);
    }
    else if (mac_address == 0x0c4314fffe17d8a8)
    {
        getPower(message.source.address, zigbee::zcl::Cluster::POWER_CONFIGURATION);
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
void Coordinator::level_command(zigbee::Message message)
{
    //  message.source.address - источник
    std::shared_ptr<zigbee::EndDevice> ed = getDeviceByShortAddr(message.source.address); // кнопка IKEA - 0x8cf681fffe0656ef
                                                                                          //   EndDevice *ed = get_device_by_mac(0x8cf681fffe0656ef);
    if (!ed)
        return;

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
void Coordinator::height_temperature(uint16_t short_addr)
{
    // TODO: отправить сообщение в телеграм или смс
    // выключить устройство
    gsbutils::dprintf(7, "Coordinator::height_temperature\n");
}

// Обработка кратковременных нажатий (Вкл/Выкл) кнопки ИКЕА
// Включаем/выключаем подсветку шкафа в комнате
void Coordinator::ikea_button_action(uint8_t cmd)
{
    gsbutils::dprintf(1, "Кнопка ИКЕА: " + std::string(cmd == 1 ? "Включить\n" : "Выключить\n"));
    // управляем реле 3, включаем подсветку в большой комнате в шкафу
    switch_relay(0x54ef44100018b523, cmd);
}

// Отдает состояние датчиков протечки для СМС-сообщения
std::string Coordinator::get_leak_state()
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
std::string Coordinator::get_motion_state()
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
void Coordinator::check_motion_activity()
{
    std::string what{"Motion"};
    std::string::size_type pos{};
    std::string state = get_motion_state(); // содержит Motion No motion через пробел, проверяем Motion

    if (state.npos != (pos = state.find(what.data(), 0, what.length())))
    {
        setLastMotionSensorActivity(std::time(nullptr));
    }
}

/// Идентификаторы возвращаются очень редко ( но возвращаются)
/// их не надо ждать сразу, ответ будет в OnMessage
/// реальная польза только в тестировании нового устройства, в штатном режиме можно отключить
void Coordinator::getIdentifier(zigbee::NetworkAddress address)
{
    //   zigbee::Message message;
    Cluster cl = Cluster::BASIC;
    uint16_t id, id2, id3, id4, id5, id6;
    zigbee::Endpoint endpoint{address, 1};
    id = zigbee::zcl::Attributes::Basic::MODEL_IDENTIFIER; //
    id2 = zigbee::zcl::Attributes::Basic::MANUFACTURER_NAME;
    id3 = zigbee::zcl::Attributes::Basic::SW_BUILD_ID;          // SW_BUILD_ID = 0x4000
    id4 = zigbee::zcl::Attributes::Basic::PRODUCT_LABEL;        //
    id5 = zigbee::zcl::Attributes::Basic::DEVICE_ENABLED;       // у датчиков движения Sonoff его нет
    id6 = zigbee::zcl::Attributes::Basic::PHYSICAL_ENVIRONMENT; //

    // ZCL Header
    Frame frame;
    frame.frame_control.type = zigbee::zcl::FrameType::GLOBAL;
    frame.frame_control.direction = zigbee::zcl::FrameDirection::FROM_CLIENT_TO_SERVER;
    frame.frame_control.disable_default_response = true;
    frame.frame_control.manufacturer_specific = false;
    frame.command = zigbee::zcl::GlobalCommands::READ_ATTRIBUTES; // 0x00
    frame.transaction_sequence_number = generateTransactionSequenceNumber();
    // end ZCL Header

    frame.payload.push_back(LOWBYTE(id));
    frame.payload.push_back(HIGHBYTE(id));
    frame.payload.push_back(LOWBYTE(id2));
    frame.payload.push_back(HIGHBYTE(id2));
    frame.payload.push_back(LOWBYTE(id3));
    frame.payload.push_back(HIGHBYTE(id3));
    frame.payload.push_back(LOWBYTE(id4));
    frame.payload.push_back(HIGHBYTE(id4));
    frame.payload.push_back(LOWBYTE(id5));
    frame.payload.push_back(HIGHBYTE(id5));
    frame.payload.push_back(LOWBYTE(id6));
    frame.payload.push_back(HIGHBYTE(id6));

    sendMessage(endpoint, cl, frame);
}

/*
2.5.7 ZCL specification
     * @param cluster - The cluster id of the requested report.
     * @param attributeId - The attribute id for requested report.
     * @param dataType - The two byte ZigBee type value for the requested report.
     * @param minReportTime - Minimum number of seconds between reports.
     * @param maxReportTime - Maximum number of seconds between reports.
     * @param reportableChange - OCTET_STRING - Amount of change to trigger a report in curly braces. Empty curly braces if no change needs to be configured.
*/
bool Coordinator::configureReporting(zigbee::NetworkAddress address)
{
    return configureReporting(address, Cluster::POWER_CONFIGURATION);
}
bool Coordinator::configureReporting(zigbee::NetworkAddress address,
                                     Cluster cluster,
                                     uint16_t attributeId,
                                     Attribute::DataType attribute_data_type,
                                     uint16_t reportable_change)
{
#ifdef TEST
    gsbutils::dprintf(1, "Configure Reporting \n");
#endif
    // default
    //   uint16_t attributeId = 0x0000;
    //   Attribute::DataType attribute_data_type = Attribute::DataType::UINT8;
    // uint16_t reportable_change = 0x0000;

    zigbee::Endpoint endpoint{address, 1};
    // ZCL Header
    Frame frame;
    frame.frame_control.type = zigbee::zcl::FrameType::GLOBAL;
    frame.frame_control.direction = zigbee::zcl::FrameDirection::FROM_CLIENT_TO_SERVER;
    frame.frame_control.disable_default_response = true;
    frame.frame_control.manufacturer_specific = false;
    frame.transaction_sequence_number = generateTransactionSequenceNumber();
    frame.command = zigbee::zcl::GlobalCommands::CONFIGURE_REPORTING; // 0x06
// end ZCL Header

// Интервал идет в секундах!
#ifdef TEST
    uint16_t min_interval = 180; // 3 min
    uint16_t max_interval = 600; // 10 min
#else
    uint16_t min_interval = 60;   // 1 minutes
    uint16_t max_interval = 3600; // 1 hours
#endif

    std::shared_ptr<zigbee::EndDevice> ed = getDeviceByShortAddr(address);
    if (ed)
    {
        switch (ed->get_device_type())
        {

        case 7:                      // IKEA button
            min_interval = 3600;     // 1 hour
            max_interval = 3600 * 8; // 8 hours
            break;
        }
    }

    frame.payload.push_back(LOWBYTE(attributeId));
    frame.payload.push_back(HIGHBYTE(attributeId));
    frame.payload.push_back((uint8_t)attribute_data_type);
    frame.payload.push_back(LOWBYTE(min_interval));
    frame.payload.push_back(HIGHBYTE(min_interval));
    frame.payload.push_back(LOWBYTE(max_interval));
    frame.payload.push_back(HIGHBYTE(max_interval));
    frame.payload.push_back(LOWBYTE(reportable_change));
    frame.payload.push_back(HIGHBYTE(reportable_change));

    sendMessage(endpoint, cluster, frame);
    return true;
    /*
       CONFIGURE_REPORTING = 0x06,
    cluster                                  0x000c
    direction 0x00 uint8_t
    uint16_t attribute_id                   0x0055
    Attribute::DataType attribute_data_type 0x39
    uint16_t min_interval                   0x3c
    uint16_t max_interval                   0x384
    vector<uint8_t> reportable_change       {0x3f800000}
*/
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
std::string Coordinator::show_device_statuses(bool as_html)
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

std::string Coordinator::show_one_type(std::shared_ptr<zigbee::EndDevice> ed, bool as_html)
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

    std::string bat{};
    std::string batRm{};
    bat = ed->showBatteryVoltage();
    if (bat.size() == 0)
        bat = ed->show_mains_voltage();
    batRm = ed->showBatteryRemain();
    std::string curr = ed->show_current();

    std::string power_src = bat.size() > 0 ? (bat + "V") : (batRm.size() > 0 ? batRm : ed->showPowerSource());
    if (as_html)
    {
        result = result + "<td>&nbsp;" + power_src;
        if (curr.size() > 0)
            result = result + " / " + curr + "A";
        result = result + "</td>";
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

// вызывается для набора аттрибутов в одном ответе от одного устройства
void Coordinator::on_attribute_report(zigbee::Endpoint endpoint, Cluster cluster, std::vector<Attribute> attributes)
{

#ifdef TEST
    int dbg = 1;
#else
    int dbg = 4;
#endif
    std::shared_ptr<zigbee::EndDevice> ed = getDeviceByShortAddr(endpoint.address);
    if (!ed)
        return;
    //    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report cluster 0x%04x device 0x%04x \n", (uint16_t)cluster, endpoint.address);
    uint64_t macAddress = (uint64_t)ed->getIEEEAddress();
    try
    {
        if (cluster == Cluster::BASIC) // 0x0000
        {
            for (auto attribute : attributes)
            {
                if (attribute.id != 0x0001)
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Cluster::BASIC device 0x%04x attribute Id 0x%04x  \n", endpoint.address, attribute.id);

                try
                {
                    switch (attribute.id)
                    {
                    case zigbee::zcl::Attributes::Basic::MANUFACTURER_NAME:
                    {
                        std::string identifier = any_cast<std::string>(attribute.value);
                        if (!identifier.empty())
                        {
                            ed->setManufacturer(identifier);
                            gsbutils::dprintf(dbg, "MANUFACTURER_NAME: 0x%02x %s \n\n", endpoint.address, identifier.c_str());
                        }
                        break;
                    }
                    case zigbee::zcl::Attributes::Basic::MODEL_IDENTIFIER:
                    {
                        std::string identifier = any_cast<std::string>(attribute.value);
                        if (!identifier.empty())
                        {
                            ed->setModelIdentifier(identifier);
                            gsbutils::dprintf(dbg, "MODEL_IDENTIFIER: 0x%02x %s \n\n", endpoint.address, identifier.c_str());
                        }
                        break;
                    }
                    case zigbee::zcl::Attributes::Basic::PRODUCT_CODE:
                    {
                        std::string identifier = any_cast<std::string>(attribute.value);
                        if (!identifier.empty())
                        {
                            ed->setProductCode(identifier);
                            gsbutils::dprintf(dbg, "PRODUCT_CODE: 0x%02x %s \n\n", endpoint.address, identifier.c_str());
                        }
                    }
                    break;
                    case zigbee::zcl::Attributes::Basic::PRODUCT_LABEL:
                    {
                        gsbutils::dprintf(dbg, "PRODUCT_LABEL: 0x%02x %s \n\n", endpoint.address, (any_cast<std::string>(attribute.value)).c_str());
                        break;
                    }
                    case zigbee::zcl::Attributes::Basic::SW_BUILD_ID:
                    {
                        gsbutils::dprintf(dbg, "SW_BUILD_ID: 0x%02x %s \n\n", endpoint.address, (any_cast<std::string>(attribute.value)).c_str());
                        break;
                    }
                    case zigbee::zcl::Attributes::Basic::POWER_SOURCE:
                    {
                        uint8_t val = any_cast<uint8_t>(attribute.value);
                        gsbutils::dprintf(dbg, "Device 0x%04x POWER_SOURCE: %d \n", endpoint.address, val);

                        if (ed && val > 0 && val < 0x8f)
                        {
                            ed->setPowerSource(val);
                        }

                        break;
                    }
                    case zigbee::zcl::Attributes::Basic::ZCL_VERSION:
                    {
                        gsbutils::dprintf(7, "Coordinator::on_attribute_report:ZCL version:\n endpoint.address: 0x%04x, \n attribute.value: %d \n\n", endpoint.address, (any_cast<uint8_t>(attribute.value)));

                        break;
                    }
                    case zigbee::zcl::Attributes::Basic::APPLICATION_VERSION:
                    {
                        // с умных розеток идет примерно каждые 300 секунд
                        if (ed->get_device_type() == 10) // умные розетки
                        {
                            zigbee::NetworkAddress shortAddr = ed->getNetworkAddress();
                            // запрос тока и напряжения, работает!
                            std::vector<uint16_t> idsAV{0x0505, 0x508};
                            read_attribute(shortAddr, zigbee::zcl::Cluster::ELECTRICAL_MEASUREMENTS, idsAV);

                            if (ed->get_current_state(1) != "On" && ed->get_current_state(1) != "Off")
                            {
                                std::vector<uint16_t> idsAV{0x0000};
                                read_attribute(shortAddr, zigbee::zcl::Cluster::ON_OFF, idsAV);
                            }
                        }

                        gsbutils::dprintf(7, "Coordinator::on_attribute_report: APPLICATION_VERSION:\n endpoint.address: 0x%04x, \n attribute.value: %d \n\n", endpoint.address, (any_cast<uint8_t>(attribute.value)));

                        break;
                    }

                    case zigbee::zcl::Attributes::Basic::GENERIC_DEVICE_TYPE:
                    {
                        gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: GENERIC_DEVICE_TYPE:\n endpoint.address: 0x%04x, \n attribute.value: 0x%02x \n\n", endpoint.address, any_cast<uint8_t>(attribute.value));

                        break;
                    }
                    case zigbee::zcl::Attributes::Basic::GENERIC_DEVICE_CLASS:
                    {
                        gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: GENERIC_DEVICE_CLASS:\n endpoint.address: 0x%04x, \n attribute.value: 0x%02x \n\n", endpoint.address, any_cast<uint8_t>(attribute.value));

                        break;
                    }
                    case zigbee::zcl::Attributes::Basic::PRODUCT_URL:
                    {
                        gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: GENERIC_DEVICE_CLASS:\n endpoint.address: 0x%04x, \n attribute.value: 0x%02x \n\n", endpoint.address, (any_cast<std::string>(attribute.value).c_str()));
                    }
                    break;
                    case zigbee::zcl::Attributes::Basic::FF01:
                    {
                        // датчик протечек Xiaomi. Двухканальное реле Aqara.
                        // Строка с набором аттрибутов. На входе std::string
#ifdef DEBUG
                        gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Device 0x%04x Cluster::BASIC Attribute Id FF01\n", endpoint.address);
#endif
                        std::vector<uint8_t> input{};
                        for (uint8_t b : any_cast<std::string>(attribute.value))
                        {
                            input.push_back((uint8_t)b);
                            gsbutils::dprintf_c(dbg, " %02x  ", b);
                        }
                        gsbutils::dprintf_c(dbg, " \n");
                        // датчик протечек
                        // 0x01 21 d1 0b // battery 3.025
                        // 0x03 28 1e // температура 29 град
                        // 0x04 21 a8 43  // no description
                        // 0x05 21 08 00  // RSSI 128 dB UINT16
                        // 0x06 24 01 00 00 00 00  // ?? UINT40
                        // 08 21 06 02 // no  description
                        // 09 21 00 04 // no  description
                        // 0a 21 00 00  // parent NWK - coordinator (0000)
                        // 64 10 00    // false - OFF
                        // двухканальное реле
                        // 0x03   0x28   0x1e //int8
                        // 0x05   0x21   0x05   0x00
                        // 0x08   0x21   0x2f   0x13
                        // 0x09   0x21   0x01   0x02
                        // 0x64   0x10   0x00 // bool
                        // 0x65   0x10   0x00
                        // 0x6e   0x20   0x00
                        // 0x6f   0x20   0x00
                        // 0x94   0x20   0x02  // uint8
                        // 0x95   0x39   0x0a   0xd7   0xa3   0x39   // float
                        // 0x96   0x39   0x58   0x48   0x0a   0x45
                        // 0x97   0x39   0x00   0x30   0x68   0x3b
                        // 0x98   0x39   0x00   0x00   0x00   0x00
                        // 0x9b   0x21   0x00   0x00
                        // 0x9c   0x20   0x01
                        // 0x0a   0x21   0x00   0x00 //uint16
                        // 0x0c   0x28   0x00   0x00
                        for (unsigned i = 0; i < input.size(); i++)
                        {
                            switch (input.at(i))
                            {
                            case 0x01: // напряжение батарейки
                            {
                                float bat = (float)_UINT16(input.at(i + 2), input.at(i + 3));
                                i = i + 3;
                                ed->setBatteryParams(0, bat / 1000);
                            }
                            break;
                            case 0x03: // температура
                            {
                                i = i + 2;
                                ed->set_temperature((double)input.at(i));
                            }
                            break;
                            case 0x04:
                                i = i + 3;
                                break;
                            case 0x05: // RSSI  val - 90
                            {
                                int16_t rssi = (int16_t)(_UINT16(input.at(i + 2), input.at(i + 3)) - 90);
                                gsbutils::dprintf(dbg, "device 0x%04x RSSI:  %d dBm \n", endpoint.address, rssi);
                                i = i + 3;
                            }
                            break;
                            case 0x06: // ?
                            {
                                uint64_t lqi = _UINT64(input.at(i + 2), input.at(i + 3), input.at(i + 4), input.at(i + 5), input.at(i + 6), 0, 0, 0);
                                gsbutils::dprintf(dbg, "0x06: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", input.at(i + 2), input.at(i + 3), input.at(i + 4), input.at(i + 5), input.at(i + 6));
                                i = i + 6;
                            }
                            break;
                            case 0x08:
                            case 0x09:
                                i = i + 3;
                                break;

                            case 0x0a: // nwk
                            {
                                i = i + 3;
                            }
                            break;
                            case 0x0c:
                                i = i + 2;
                                break;
                            case 0x64: // состояние устройства, канал 1
                            {
                                i = i + 2;
                                ed->set_current_state(input.at(i) == 1 ? "On" : "Off");
                            }
                            break;
                            case 0x65: // состояние устройства, канал 2
                            {
                                i = i + 2;
                                ed->set_current_state((input.at(i) == 1 ? "On" : "Off"), 2);
                            }
                            break;
                            case 0x6e: // uint8
                            case 0x6f:
                            case 0x94:
                            {
                                i = i + 2;
                            }
                            break;
                            case 0x95: // Потребленная мощность за период
                            case 0x98: // Мгновенная мощность
                            {
                                i = i + 5;
                            }
                            break;
                            case 0x96: // напряжение
                            {
                                union
                                {
                                    int num;
                                    float fnum;
                                } my_union;
                                my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);
                                ed->setPowerSource(0x01);
                                ed->set_mains_voltage(my_union.fnum / 10);
                                gsbutils::dprintf(dbg, "Voltage  %0.2f\n", my_union.fnum / 10);
                                i = i + 5;
                            }
                            break;
                            case 0x97: // ток
                            {
                                union
                                {
                                    int num;
                                    float fnum;
                                } my_union;
                                my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);
                                double val = my_union.fnum / 1000;
                                double val11 = my_union.fnum;
                                ed->set_current(val11);
                                gsbutils::dprintf(dbg, "Ток: %0.3f %0.3f\n", val, val11);
                                i = i + 5;
                            }
                            break;
                            case 0x9b:
                            {
                                i = i + 3;
                            }
                            break;
                            case 0x9c:
                            {
                                i = i + 2;
                            }
                            break;
                            } // switch
                            if (i >= input.size())
                                break;
                        } // for
                    }
                    break;

                    case zigbee::zcl::Attributes::Basic::FFE2:
                    case zigbee::zcl::Attributes::Basic::FFE4:
                    {
                        gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: Attribute 0x%04x endpoint.address: 0x%02x, attribute.value: %d \n\n", attribute.id, endpoint.address, (any_cast<uint8_t>(attribute.value)));
                    }
                    break;

                    case zigbee::zcl::Attributes::Basic::LOCATION_DESCRIPTION: // 0x0010
                    case zigbee::zcl::Attributes::Basic::DATA_CODE:            // 0x0006
                    {
                        std::string val = any_cast<std::string>(attribute.value);
                        gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: Attribute 0x%04x endpoint.address: 0x%02x, attribute.value: %s \n\n", attribute.id, endpoint.address, val.c_str());
                    }
                    break;
                    case zigbee::zcl::Attributes::Basic::STACK_VERSION: // 0x0002,
                    case zigbee::zcl::Attributes::Basic::HW_VERSION:    // 0x0003,
                    {
                        gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: Attribute 0x%04x endpoint.address: 0x%02x, attribute.value: %d \n\n", attribute.id, endpoint.address, (any_cast<uint8_t>(attribute.value)));
                    }
                    break;
                    case zigbee::zcl::Attributes::Basic::PHYSICAL_ENVIRONMENT: // 0x0011, enum8
                    {
#ifdef TEST
                        uint8_t val = any_cast<uint8_t>(attribute.value);
                        gsbutils::dprintf(1, "PHYSICAL_ENVIRONMENT: 0x%04x 0x%02x \n\n", endpoint.address, val);
#endif
                    }
                    break;
                    case zigbee::zcl::Attributes::Basic::DEVICE_ENABLED: // 0x0012, uint8
                    {
#ifdef TEST
                        uint8_t val = any_cast<uint8_t>(attribute.value);
                        gsbutils::dprintf(1, "DEVICE_ENABLED: 0x%04x %s \n\n", endpoint.address, val ? "Enabled" : "Disabled");
#endif
                    }
                    break;
                    case zigbee::zcl::Attributes::Basic::ALARM_MASK: //  0x0013, map8
                    {
                        gsbutils::dprintf(1, "ALARM_MASK 0x%04x , attribute.value: %d \n\n", endpoint.address, (any_cast<uint8_t>(attribute.value)));
                    }
                    break;
                    case zigbee::zcl::Attributes::Basic::DISABLE_LOCAL_CONFIG: // 0x0014, map8
                    {
                        gsbutils::dprintf(1, "DISABLE_LOCAL_CONFIG 0x%04x: 0x%02x \n", endpoint.address, (any_cast<uint8_t>(attribute.value)));
                    }
                    break;
                    case zigbee::zcl::Attributes::Basic::GLOBAL_CLUSTER_REVISION: // 0xFFFD
                    {
                        gsbutils::dprintf(1, "GLOBAL_CLUSTER_REVISION 0x%04x: 0x%02x \n", endpoint.address, (any_cast<uint8_t>(attribute.value)));
                    }
                    break;
                    default:
                    {
                        gsbutils::dprintf(1, " Cluster::%d: endpoint %d - unknown attribute 0x%04x \n", cluster, endpoint.number, attribute.id);
                    }
                    }
                }
                catch (std::exception &e)
                {
                    gsbutils::dprintf(1, " Cluster::%d: adddress 0x%04x endpoint %d - error attribute 0x%04x \n", cluster, endpoint.address, endpoint.number, attribute.id);
                }
            } // for attribute
        }
        /// POWER_CONFIGURATION 1
        else if (cluster == Cluster::POWER_CONFIGURATION) // 0x0001
        {
#ifdef TEST
            int dbg = 1;
#endif
            for (auto attribute : attributes)
            {

                switch (attribute.id)
                {
                case zigbee::zcl::Attributes::PowerConfiguration::MAINS_VOLTAGE:
                {

                    double val = static_cast<double>(any_cast<uint8_t>(attribute.value));
                    gsbutils::dprintf(dbg, "Device 0x%04x MAINS_VOLTAGE: %2.3f \n", endpoint.address, val / 10);

                    ed->set_mains_voltage(val);
                }
                break;
                case zigbee::zcl::Attributes::PowerConfiguration::BATTERY_VOLTAGE:
                {
                    double val = static_cast<double>(any_cast<uint8_t>(attribute.value));
                    gsbutils::dprintf(dbg, "Device 0x%04x BATTERY_VOLTAGE: %2.3f \n", endpoint.address, val / 10);

                    ed->setBatteryParams(0.0, val / 10);
                }
                break;
                case zigbee::zcl::Attributes::PowerConfiguration::BATTERY_REMAIN:
                {
                    uint8_t val = any_cast<uint8_t>(attribute.value); // 0x00-0x30 0x30-0x60 0x60-0x90 0x90-0xc8
                    gsbutils::dprintf(dbg, "Device 0x%04x BATTERY_REMAIN: 0x%02x \n", endpoint.address, val);

                    ed->setBatteryParams(val, 0.0);

                    break;
                }
                    // Кнопка ИКЕА не поддерживает этот аттрибут
                case zigbee::zcl::Attributes::PowerConfiguration::BATTERY_SIZE:
                {
#ifdef TEST
                    uint8_t val = static_cast<uint8_t>(any_cast<uint8_t>(attribute.value));
                    gsbutils::dprintf(dbg, "Device: 0x%04x BATTERY_SIZE:  0x%02x \n", endpoint.address, val);
#endif
                }
                break;
                default:
                    gsbutils::dprintf(1, " Cluster::POWER_CONFIGURATION: unknown attribute 0x%04x \n", attribute.id);
                }
            } // for attribute
        }
        else if (cluster == Cluster::ON_OFF)
        {
            std::set<uint16_t> usedAttributes{};
            for (auto attribute : attributes)
            {
                if (usedAttributes.contains(attribute.id))
                    continue;
                usedAttributes.insert(attribute.id);

                // TODO: уточнять по эндпойнтам
                switch (attribute.id)
                {
                case zigbee::zcl::Attributes::OnOffSwitch::ON_OFF: // 0x0000
                {
                    bool b_val = false;
                    uint8_t u_val = 0;
#ifdef TEST
                    dbg = 1;
#endif
                    try
                    {
                        b_val = (any_cast<bool>(attribute.value));
                        gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: +Device 0x%04x endpoint %d Level %s \n", endpoint.address, endpoint.number, b_val ? "High" : "Low");
                        uint64_t macAddress = (uint64_t)ed->getIEEEAddress();
                        if (macAddress == 0x00124b0014db2724)
                        {
                            // custom2 коридор
                            if (endpoint.number == 2) // датчик освещения
                            {
                                gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: Освещенность %s \n", b_val ? "высокая" : "низкая");
                                ed->set_luminocity(b_val ? 1 : 0);
                            }
                            if (endpoint.number == 6) // датчик движения (1 - нет движения, 0 - есть движение)
                            {
                                gsbutils::dprintf(dbg, "Прихожая: Движение %s \n", b_val ? "нет" : "есть");
                                handle_motion(ed, b_val ? 0 : 1);
                            }
                        }
                        else if (macAddress == 0x00124b0009451438)
                        {
                            // custom3 датчик присутствия1 - кухня
                            if (endpoint.number == 2) // датчик присутствия
                            {
                                gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: Присутствие %s \n", b_val ? "нет" : "есть");
                                handle_motion(ed, b_val ? 0 : 1);
                            }
                        }
                        else if (macAddress == 0x0c4314fffe17d8a8)
                        {
                            // датчик движения IKEA
                            gsbutils::dprintf(1, "Coordinator::on_attribute_report:датчик движения IKEA %s \n", b_val ? "есть" : "нет");
                            handle_motion(ed, b_val ? 1 : 0);
                        }
                        else if (macAddress == 0x00124b0007246963)
                        {
                            int dbg = 6;
                            // Устройство Custom3
                            if (endpoint.number == 2) // датчик освещения
                            {
                                gsbutils::dprintf(dbg, "Custom3: Освещенность %s \n", b_val ? "высокая" : "низкая");
                                ed->set_luminocity(b_val ? 1 : 0);
                            }
                            if (endpoint.number == 4) // датчик движения (с датчика приходит 1 - нет движения, 0 - есть движение)
                            {
                                gsbutils::dprintf(dbg, "Custom3: Движение %s \n", b_val ? "нет" : "есть");
                                handle_motion(ed, b_val ? 0 : 1);
                            }
                        }
                        else if (ed->get_device_type() == 10) // умные розетки
                        {
                            std::string currentState = ed->get_current_state();
                            std::string newState = b_val ? "On" : "Off";
                            if (newState != currentState)
                            {
                                std::time_t ts = std::time(0); // get time now
                                ed->set_last_action((uint64_t)ts);
                                ed->set_current_state(newState);
                            }
                        }
                        else if (ed->get_device_type() == 11) //  у двухканальных реле есть для EP1 и EP2
                        {
                            std::time_t ts = std::time(0); // get time now
                            ed->set_last_action((uint64_t)ts);
                            ed->set_current_state(b_val ? "On" : "Off", endpoint.number);
                            //                            gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: 1+Device 0x%04x endpoint %d Level %s \n", endpoint.address, endpoint.number, b_val ? "High" : "Low");
                        }
                        else
                        {
                            std::time_t ts = std::time(0); // get time now
                            ed->set_last_action((uint64_t)ts);
                            ed->set_current_state(b_val ? "On" : "Off");
                            //                            gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: 2+Device 0x%04x endpoint %d Level %s \n", endpoint.address, endpoint.number, b_val ? "High" : "Low");
                        }
                    }
                    catch (std::bad_any_cast &e)
                    {
                        gsbutils::dprintf(1, "Coordinator::on_attribute_report:-Device 0x%04x endpoint %d Bad any cast \n", endpoint.address, endpoint.number);
                    }
                }
                break;
                case zigbee::zcl::Attributes::OnOffSwitch::ON_TIME:
                {
                    uint16_t val = static_cast<uint16_t>(any_cast<uint16_t>(attribute.value));
                    gsbutils::dprintf(1, "Device 0x%04x endpoint %d ON_TIME =  %d s \n", endpoint.address, endpoint.number, val);
                }
                break;
                case zigbee::zcl::Attributes::OnOffSwitch::OFF_WAIT_TIME:
                {
                    uint16_t val = static_cast<uint16_t>(any_cast<uint16_t>(attribute.value));
                    gsbutils::dprintf(1, "Device 0x%04x endpoint %d OFF_WAIT_TIME =  %d s \n", endpoint.address, endpoint.number, val);
                }
                break;
                case zigbee::zcl::Attributes::OnOffSwitch::_00F5: // с реле aqara T1
                {
                    //  каждые 30 секунд, практически идет постоянно только с реле света в коридоре
                    // UINT32 0x070000nn или 0x03<short_addr>mm, nn крутится от 0 до ff
                    uint32_t val = any_cast<uint32_t>(attribute.value);
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: attribute Id 0x%04x in cluster ON_OFF Device 0x%04x val 0x%08x\n", attribute.id, endpoint.address, val);
                }
                break;
                case zigbee::zcl::Attributes::OnOffSwitch::F000: // двухканальное реле, очень походит на поведение реле в кластере _00F5
                case zigbee::zcl::Attributes::OnOffSwitch::F500: // с реле aqara T1
                case zigbee::zcl::Attributes::OnOffSwitch::F501: // с реле aqara T1
                {
                    uint32_t val = any_cast<uint32_t>(attribute.value);
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: attribute Id 0x%04x in cluster ON_OFF Device 0x%04x val 0x%08x\n", attribute.id, endpoint.address, val);
                }
                break;
                case zigbee::zcl::Attributes::OnOffSwitch::_00F7: // какие то наборы символов
                {
#ifdef TEST
                    for (uint8_t b : any_cast<std::string>(attribute.value))
                    {
                        gsbutils::dprintf_c(dbg, " 0x%02x", b);
                    }
                    gsbutils::dprintf_c(dbg, "\n");
#endif
                    std::string val = any_cast<std::string>(attribute.value);
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: attribute Id 0x%04x in cluster ON_OFF Device 0x%04x value: \n", attribute.id, endpoint.address, val.c_str());
                }
                break;
                case zigbee::zcl::Attributes::OnOffSwitch::_5000:
                case zigbee::zcl::Attributes::OnOffSwitch::_8000:
                case zigbee::zcl::Attributes::OnOffSwitch::_8001:
                case zigbee::zcl::Attributes::OnOffSwitch::_8002:
                {
                    // Краны
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: attribute Id 0x%04x in cluster ON_OFF device: 0x%04x\n", attribute.id, endpoint.address);
                }
                break;
                default:
                    gsbutils::dprintf(1, "Coordinator::on_attribute_report: unknown attribute Id 0x%04x in cluster ON_OFF device: 0x%04x\n", attribute.id, endpoint.address);
                }
            }
        }
        /// DEVICE_TEMPERATURE_CONFIGURATION
        else if (cluster == Cluster::DEVICE_TEMPERATURE_CONFIGURATION)
        {
            for (auto attribute : attributes)
            {

                int16_t val = static_cast<uint8_t>(any_cast<int16_t>(attribute.value));

                ed->set_temperature(static_cast<double>(val));
                if (val > 60)
                    height_temperature(endpoint.address);
                gsbutils::dprintf(dbg, "Device 0x%04x temperature =  %d grad Celsius\n", endpoint.address, val);
            }
        }
        /// ANALOG_INPUT
        // сюда относятся датчики температуры и влажности AM2302(DHT22) и  HDC1080
        // двухканальное реле Aqara
        else if (cluster == Cluster::ANALOG_INPUT)
        {
#ifdef TEST
            dbg = 1;
#endif
            double value = -100.0;
            std::string unit = "";
            for (auto attribute : attributes)
            {

                switch (attribute.id)
                {
                case 0x0055:
                {
                    //  на реле показывает суммарный ток в 0,1 А (потребляемый нагрузкой и самим реле)
                    // показывает сразу после изменения нагрузки в отличие от получаемого в репортинге
                    value = (double)(any_cast<float>(attribute.value));
                    gsbutils::dprintf(dbg, "Device 0x%04x endpoint %d Analog Input Value =  %f \n", endpoint.address, endpoint.number, value);
                }
                break;
                case 0x006f:
                {
                }
                break;
                case 0x001c:
                {
                    // единица измерения(для двухканального реле не приходит)
                    unit = (any_cast<std::string>(attribute.value));
                    unit = gsb_utils::remove_after(unit, ",");
#ifdef TEST
                    gsbutils::dprintf(1, "Device 0x%04x endpoint %d Analog Input Unit =  %s \n", endpoint.address, endpoint.number, unit.c_str());
#endif
                }
                break;
                default:
                    gsbutils::dprintf(1, "Coordinator::on_attribute_report: unknown attribute Id 0x%04x in cluster ANALOG_INPUT device: 0x%04x\n", attribute.id, endpoint.address);
                }
            }
            if (unit.size() && value > -100.0)
            {
                if (unit == "%")
                {
                    ed->set_humidity(value);
                    ed->set_current_state("On");
                }
                else if (unit == "C")
                    ed->set_temperature(value);
                else if (unit == "V")
                    ed->setBatteryParams(0, value);
                else if (unit == "Pa")
                    ed->set_pressure(value);
                else
                    gsbutils::dprintf(1, "Device 0x%04x endpoint %d Analog Input Unit =  %s \n", endpoint.address, endpoint.number, unit.c_str());
                value = -100.0;
                unit = "";
            }
            else if ((ed->get_device_type() == 11 || ed->get_device_type() == 9 || ed->get_device_type() == 10) && (value > -100.0))
            {
                ed->set_current(value / 100);
#ifdef TEST
                gsbutils::dprintf(1, "Device 0x%04x endpoint %d Full Current %0.3fA \n", endpoint.address, endpoint.number, value / 100);
#endif
            }
        }
        // MULTISTATE_INPUT 0x0012 c.226
        else if (cluster == Cluster::MULTISTATE_INPUT)
        {
            double value = -100.0;
            std::string unit = "";

            dbg = 1;
            for (auto attribute : attributes)
            {
                try
                {
                    switch (attribute.id)
                    {
                    case 0x000E: // state text - Array of character string
                        gsbutils::dprintf(dbg, "Device 0x%04x endpoint %d MULTISTATE_INPUT State_Text \n", endpoint.address, endpoint.number);

                        break;
                    case 0x001C: // description - string
                    {
                        unit = (any_cast<std::string>(attribute.value));
                        gsbutils::dprintf(dbg, "Device 0x%04x endpoint %d MULTISTATE_INPUT Unit =  %s \n", endpoint.address, endpoint.number, unit.c_str());
                    }
                    break;
                    case 0x004A: // NumberOfStates - uint16
                        break;
                    case 0x0051: // OutOfService - bool
                        break;
                    case 0x0055: // PresentValue - uint16
                    {
                        value = (double)(any_cast<uint16_t>(attribute.value));
                        gsbutils::dprintf(dbg, "Device 0x%04x endpoint %d MULTISTATE_INPUT Value =  %f \n", endpoint.address, endpoint.number, value);
                    }
                    break;
                    case 0x0067: // Reliability - enum8
                        break;
                    case 0x006F: // StatusFlags - map8
                        // Bit 0 = IN ALARM, Bit 1 = FAULT, Bit 2 = OVERRIDDEN, Bit 3 = OUT OF SERVICE
                        {
                            uint8_t val = any_cast<uint8_t>(attribute.value);
                            gsbutils::dprintf(dbg, "Device 0x%04x endpoint %d Multistate Input StatusFlags =  0x%02x \n", endpoint.address, endpoint.number, val);
                        }
                        break;
                    case 0x0100: // ApplicationType
                        break;
                    default:
                        gsbutils::dprintf(1, "Coordinator::on_attribute_report: unknown attribute Id 0x%04x in cluster MULTISTATE_INPUT device: 0x%04x\n", attribute.id, endpoint.address);
                    }
                }
                catch (std::bad_any_cast &bac)
                {
                    gsbutils::dprintf(1, "Coordinator::on_attribute_report: attribute Id 0x%04x in cluster MULTISTATE_INPUT device: 0x%04x bad any_cast\n", attribute.id, endpoint.address);
                }
            }
        }
        // XIAOMI_SWITCH
        else if (cluster == Cluster::XIAOMI_SWITCH)
        {
            // часть устройств Xiaomi отображает данные с устройств здесь : T1 - реле aquara с нейтралью
            // часть устройств Xiaomi в кластере Basic (0x0000): water leak sensor
            // идут каждые 5 минут
#ifdef DEBUG
            gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Device 0x%04x Cluster::XIAOMI_SWITCH \n", endpoint.address);
#endif
            for (auto attribute : attributes)
            {

                switch (attribute.id)
                {
                case 0x00f7: // Строка с набором данных. На входе std::string
                {

                    std::vector<uint8_t> input{};
                    for (uint8_t b : any_cast<std::string>(attribute.value))
                    {
                        input.push_back((uint8_t)b);
                        gsbutils::dprintf_c(dbg, "%02x ", b);
                    }
                    gsbutils::dprintf_c(dbg, "\n");
                    // 03 28 1e          int8                 Device_temperature температура устройства
                    // 05 21 08 00       uint16                PowerOutages Счетчик пропаданий напряжения
                    // 08 21 00 00       uint16
                    // 09 21 00 00       uint16
                    // 0b 28 00          uint8  ??Подключен или нет потребитель
                    // 0с 20 00          uint8
                    // 64 10 00          bool                    State    текущее состояние реле1
                    // 65 10 00          bool                    State    текущее состояние реле2(для двухканальных)
                    // 95 39 00 00 00 00 float Single precision  // Потребленная мощность за какой-то период
                    // 96 39 00 00 00 00 float Single precision  Voltage  напряжение на реле
                    // 97 39 00 00 00 00 float Single precision   ток
                    // 98 39 00 00 00 00 float Single precision  Power    мгновенная потребляемая мощность
                    // 9a 28 00          uint8
                    //
                    for (unsigned i = 0; i < input.size(); i++)
                    {

                        uint8_t att_id = (uint8_t)input.at(i);

                        switch (att_id)
                        {
                        case 0x03:
                        {
                            i = i + 2;
                            ed->set_temperature((double)input.at(i));
                            gsbutils::dprintf(dbg, "Temperature  %d\n", input.at(i));
                        }
                        break;
                        case 0x05: // Потеря питания 220В - количество раз
                            gsbutils::dprintf(dbg, "Power outages  %d\n", _UINT16(input.at(i + 2), input.at(i + 3)));
                            i = i + 3;
                            break;

                        case 0x08: // uint16
                        case 0x09: // uint16
                        {
                            gsbutils::dprintf(dbg, "tag 0x%02x  %d\n", att_id, _UINT16(input.at(i + 2), input.at(i + 3)));
                            i = i + 3;
                        }
                        break;

                        case 0x64: // status
                        {
                            i = i + 2;
                            ed->set_current_state(input.at(i) == 1 ? "On" : "Off");
                            gsbutils::dprintf(dbg, "State  %d\n", input.at(i));
                        }
                        break;

                        case 0x95: // Потребленная мощность
                        {
                            union
                            {
                                int num;
                                float fnum;
                            } my_union;
                            my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);

                            gsbutils::dprintf(dbg, "Потребленная мощность (0x95)  %0.6f\n", my_union.fnum);
                            i = i + 5;
                        }
                        break;
                        case 0x96: // напряжение
                        {
                            union
                            {
                                int num;
                                float fnum;
                            } my_union;
                            my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);
                            if (ed)
                            {
                                ed->setPowerSource(0x01);
                                ed->set_mains_voltage(my_union.fnum / 10);
                            }
                            gsbutils::dprintf(dbg, "Напряжение  %0.2f\n", my_union.fnum / 10);
                            i = i + 5;
                        }
                        break;
                        case 0x97: // ток
                        {
                            union
                            {
                                int num;
                                float fnum;
                            } my_union;
                            my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);
                            double val = my_union.fnum / 1000;
                            ed->set_current(val);
                            gsbutils::dprintf(dbg, "Ток: %0.3f\n", val);
                            i = i + 5;
                        }
                        break;

                        case 0x98: // Текущая потребляемая мощность
                        {
                            union
                            {
                                int num;
                                float fnum;
                            } my_union;
                            my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);

                            gsbutils::dprintf(dbg, "Текущая потребляемая мощность(0x98) %0.6f\n", my_union.fnum);

                            i = i + 5;
                        }
                        break;

                        case 0x9a: // тип 0x20 uint8
                        case 0x0b: // тип 0x20 uint8
                        {
                            gsbutils::dprintf(dbg, "0x%02x  %d\n", att_id, input.at(i + 2));
                            i = i + 2;
                        }
                        break;

                        default:
                        {
                            gsbutils::dprintf(1, "Необработанный тэг %d type %d \n ", att_id, input.at(i + 1));
                            i++;
                        }
                        } // switch
                        if (i >= input.size())
                            break;
                    } // for
                }
                break;
                case 0xFF01: // door sensor, xiaomi switch
                {
                    std::vector<uint8_t> input{};
                    for (uint8_t b : any_cast<std::string>(attribute.value))
                    {
                        input.push_back((uint8_t)b);
                        gsbutils::dprintf_c(dbg, "%02x ", b);
                    }
                    for (unsigned i = 0; i < input.size(); i++)
                    {
                        uint8_t att_id = (uint8_t)input.at(i);

                        switch (att_id)
                        {
                        case 0x03: // device temperature
                        {
                            i = i + 2;
                            //                           ed->set_temperature((double)input.at(i));
                            gsbutils::dprintf(1, "xiaomi temperature  %d\n", input.at(i));
                        }
                        break;
                        case 0x05: // RSSI
                        {
                            uint16_t rssi = _UINT16(input.at(i + 2), input.at(i + 3)) - 90;
                            gsbutils::dprintf(dbg, "xiaomi  RSSI: %d dBm \n", rssi);
                            i = i + 3;
                        }
                        break;
                        }
                    }
                }
                break;
                default:
                    gsbutils::dprintf(1, "Coordinator::on_attribute_report: Cluster::XIAOMI_SWITCH unknown attribute Id 0x%04x\n", attribute.id);
                }
            }
        }
        else if (cluster == Cluster::SIMPLE_METERING) // 0x0702 c.717
        {
#ifdef DEBUG
            // здесь отображаются данные с умной розетки
            for (auto attribute : attributes)
            {
                switch (attribute.id)
                {
                case 0x0000: // CurrentSummationDelivered
                {
                    // больше похоже на потребленную мощность за какой-то период
                    // 0x9 0x2 на лампочке 15Вт
                    // 0x1 на слабеньком заряднике
                    uint64_t val = any_cast<uint64_t>(attribute.value);
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Device 0x%04x Cluster::SIMPLE_METERING , attribute.id = 0x0000, value=0x%" PRIx64 "\n", endpoint.address, val);
                }
                break;
                default:
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Device 0x%04x Cluster::SIMPLE_METERING , attribute.id = 0x%04x\n", endpoint.address, attribute.id);
                }
            }
#endif
        }
        else if (cluster == Cluster::ELECTRICAL_MEASUREMENTS) // 0x0b04 c.339
        {
            // здесь отображаются данные с умной розетки
            for (auto attribute : attributes)
            {
                switch (attribute.id)
                {
                case 0x0505: // RMS Voltage V
                {
                    uint16_t val = any_cast<uint16_t>(attribute.value);
                    ed->set_mains_voltage((double)val);
#ifdef TEST
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Device 0x%04x Cluster::ELECTRICAL_MEASUREMENTS ,Voltage=%0.3f, \n", endpoint.address, (double)val);
#endif
                }
                break;
                case 0x0508: // RMS Current mA
                {
                    uint16_t val = any_cast<uint16_t>(attribute.value);
                    ed->set_current((double)val / 1000);
#ifdef TEST
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Device 0x%04x Cluster::ELECTRICAL_MEASUREMENTS ,Current=%0.3f, \n", endpoint.address, (double)val);
#endif
                }
                break;
                default:
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Device 0x%04x Cluster::ELECTRICAL_MEASUREMENTS , attribute.id = 0x%04x\n", endpoint.address, attribute.id);
                }
            }
        }
        else if (cluster == Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER) // 0xe001 умная розетка и кран
        {
            // Кран: attributes    : ["valve": "", "healthStatus": "unknown", "powerSource": "dc"],
            for (auto attribute : attributes)
            {
                gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Device 0x%04x Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER , attribute.id = 0x%04x\n", endpoint.address, attribute.id);
#ifdef TEST
                switch (attribute.id)
                {
                case 0xd010: // PowerOnBehavior enum8 defaultValue: "2", options:  ['0': 'closed', '1': 'open', '2': 'last state']]
                {
                    uint8_t val = any_cast<uint8_t>(attribute.value); // valve - 0,  smartplug - 2 кран при подаче питания закрывается, розетка остается в последнем состоянии
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER , attribute.id = 0xd010 value= 0x%02x\n", val);
                }
                break;
                case 0xd030: // switchType enum8 switchTypeOptions = [  '0': 'toggle',   '1': 'state',    '2': 'momentary'
                {
                    uint8_t val = any_cast<uint8_t>(attribute.value); // 0
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER , attribute.id = 0xd030 value= 0x%02x\n", val);
                }
                break;
                default:
                {
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER , attribute.id = 0x%04x\n", attribute.id);
                }
                }
#endif
            }
        }
        else if (cluster == Cluster::TUYA_SWITCH_MODE_0) // 0xE000
        {
            for (auto attribute : attributes)
            {
                gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Device 0x%04x Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", endpoint.address, attribute.id);
                switch (attribute.id)
                {
                case 0xd001: // array
                {
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", attribute.id);
                }
                break;
                case 0xd002: // array
                {
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", attribute.id);
                }
                break;
                case 0xd003: // CHARACTER_STRING AAAA
                {
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", attribute.id);
                }
                break;
                default:
                {
                    gsbutils::dprintf(dbg, "Coordinator::on_attribute_report Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", attribute.id);
                }
                }
            }
        }
        else if (cluster == Cluster::RSSI) // 0x000b
        {
            gsbutils::dprintf(dbg, "Coordinator::on_attribute_report:  Cluster::RSSI Device 0x%04x\n", endpoint.address);

            for (auto attribute : attributes)
            {
                gsbutils::dprintf(dbg, "Coordinator::on_attribute_report: attribute Id 0x%04x in Cluster::RSSI \n", attribute.id);
            }
        }
        else if (cluster == Cluster::TIME) // 0x000a
        {
        }
        else
        {
            gsbutils::dprintf(1, "Coordinator::on_attribute_report: unknownCluster 0x%04x \n", (uint16_t)cluster);

            for (auto attribute : attributes)
            {
                gsbutils::dprintf(1, "Coordinator::on_attribute_report: unknown attribute Id 0x%04x in Cluster 0x%04x \n", attribute.id, (uint16_t)cluster);
            }
        }
    }
    catch (std::exception &e)
    {
        std::stringstream sstream;
        sstream << "Cluster " << (uint16_t)cluster << " Invalid attributes  from 0x" << std::hex << endpoint.address << " " << e.what() << std::endl;
        gsbutils::dprintf(1, "%s", sstream.str().c_str());
    }
}

// Выключение реле по списку при долгом нажатии на кнопки Sonoff1 Sonoff2
inline void Coordinator::switch_off_with_list()
{
    for (auto &mac_addr : EndDevice::OFF_LIST)
    {
        switch_relay(mac_addr, 0, 1);
        if (mac_addr == 0x00158d0009414d7e) // реле на кухне имеет два канала
            switch_relay(mac_addr, 0, 2);
    }
}
