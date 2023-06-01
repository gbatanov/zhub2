
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <string>
#include <cstring>
#include <mutex>
#include <atomic>
#include <queue>
#include <utility>
#include <chrono>
#include <syslog.h>
#include <array>
#include <memory>
#include <set>
#include <inttypes.h>
#include <optional>
#include <any>
#include <sstream>
#include <termios.h>

#include "../version.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../../gsb_utils/gsbutils.h"
#include "../common.h"
#include "../main.h"
#include "zigbee.h"
#include "../modem.h"

#ifdef WITH_SIM800
extern GsmModem *gsmmodem;
#endif
#ifdef WITH_TELEGA
#include "../telebot32/src/tlg32.h"
extern std::unique_ptr<Tlg32> tlg32;
#endif

#ifdef __MACH__
// На маке зависит от гнезда, в которое воткнут координатор
#ifdef TEST
// тестовый координатор
#define ADAPTER_ADDRESS "/dev/cu.usbmodem148201"
#else
// рабочий координатор
#define ADAPTER_ADDRESS "/dev/cu.usbserial-53220280771"
#endif
#else
#ifdef __linux__
#define ADAPTER_ADDRESS "/dev/ttyACM0"
#endif
#endif

using namespace zigbee;

using zigbee::IEEEAddress;
using zigbee::NetworkAddress;
using zigbee::NetworkConfiguration;
using zigbee::zcl::Cluster;
using zigbee::zcl::Frame;

extern std::mutex trans_mutex;
extern std::atomic<uint8_t> transaction_sequence_number;

const NetworkConfiguration Controller::default_configuration_ = {0xFFFF,                                                                                           // Pan ID.
                                                                 0xDDDDDDDDDDDDDDDD,                                                                               // Extended pan ID.
                                                                 zigbee::LogicalType::COORDINATOR,                                                                 // Logical type.
                                                                 {11},                                                                                             // RF channel list.
                                                                 {0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0D}, // Precfg key.
                                                                 false};

const NetworkConfiguration Controller::test_configuration_ = {0x1234,                                                                                           // Pan ID.
                                                              0xDDDDDDDDDDDDDDDE,                                                                               // Extended pan ID.
                                                              zigbee::LogicalType::COORDINATOR,                                                                 // Logical type.
                                                              {12},                                                                                             // RF channel list.
                                                              {0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0D}, // Precfg key.
                                                              false};

const std::vector<zigbee::SimpleDescriptor> Controller::default_endpoints_ = {{1,      // Enpoint number.
                                                                               0x0104, // Profile ID.
                                                                               0x05,   // Device ID.
                                                                               0,      // Device version.
                                                                               {},     // Input clusters list.
                                                                               {}}};   // Output clusters list.

Controller::Controller()
{
}

Controller::~Controller()
{
}

bool Controller::init_adapter()
{
    bool noAdapter = false;
    try
    {
        // в connect происходит запуск потока приема команд zigbee
        if (!connect(ADAPTER_ADDRESS, 115200))
        {
            gsbutils::dprintf(1, "Zigbee UART is not connected\n");
            // дальнейшее выполнение бессмысленно, но надо уведомить по СМС или телеграм
            noAdapter = true;
        }
    }
    catch (std::exception &e)
    {
        gsbutils::dprintf(1, "init adapter %s\n", e.what());
        noAdapter = true;
    }
    if (noAdapter)
    {
#ifdef WITH_TELEGA
        tlg32->send_message("Zigbee adapter не обнаружен.\n");
#endif
    }

    return noAdapter;
}
// Старт Zigbee-сети
bool Controller::startNetwork(NetworkConfiguration configuration)
{
    try
    {
        int res = reset(ResetType::HARD,false,false);
        if (0 == res)
            throw std::runtime_error("Adapter reseting error1");

        if (res == 1) // был аппаратный сброс
        {
            // Перезаписываем при необходимости конфигурацию сети
            gsbutils::dprintf(1, "Controller::startNetwork: Reading network configuration\n");
            NetworkConfiguration currentConfiguration = readNetworkConfiguration();

            if (currentConfiguration != configuration)
            {
                gsbutils::dprintf(1, "Controller::startNetwork: Writing correct network configuration\n"); // 0x00000800 for CH11;
                if (!writeNetworkConfiguration(configuration))
                    throw std::runtime_error("Network configuration write fail");

                // После смены конфигурации еще раз делаем программный сброс
                res = reset(ResetType::SOFT,false, false);
                if (0 == res)
                    throw std::runtime_error("Adapter reseting error2");
            }
        }
        gsbutils::dprintf(1, "Controller::startNetwork: Startup zigbee adapter\n");

        // startup нужен всегда
        if (!startup())
            throw std::runtime_error("Adapter startup error");
        gsbutils::dprintf(1, "Controller::startNetwork: Startup zigbee adapter SUCCESS\n");

        // Регистрация ендпойнтов самого координатора
        gsbutils::dprintf(1, "Coordinator::start Endpoints registration:\n");
        for (auto &endpoint_descriptor : default_endpoints_)
        {
            if (!registerEndpoint(endpoint_descriptor))
            {
                std::stringstream sstream;
                sstream << "Failure to register endpoint " << std::dec << endpoint_descriptor.endpoint_number << ", profile id: " << std::hex << endpoint_descriptor.profile_id;
                OnError(sstream.str());
            }
        }

        return true;
    }
    catch (std::exception &e)
    {
        gsbutils::dprintf(1, "%s \n", e.what());
        return false;
    }
}

void Controller::on_message(zigbee::Command command)
{
    zigbee::Message message{};

    // AF часть сообщения
    message.cluster = static_cast<zigbee::zcl::Cluster>(_UINT16(command.payload(2), command.payload(3)));
    message.source.address = _UINT16(command.payload(4), command.payload(5));
    // Эти сообщения приходят от уже зарегистрированных работающих устройств, поэтому можно получить сам объект по сетевому адресу

    message.destination.address = network_address_; // приемник - координатор
    message.source.number = command.payload(6);
    message.destination.number = command.payload(7);
    message.linkquality = command.payload(9);
    size_t length = static_cast<size_t>(command.payload(16));
    // ZCL Frame часть сообщения
    message.zcl_frame = parseZclData(std::vector<uint8_t>(&command.payload(17), &command.payload(17 + length)));

    std::shared_ptr<zigbee::EndDevice> ed = getDeviceByShortAddr(message.source.address);
    if (!ed)
        return;

    uint64_t macAddress = (uint64_t)ed->getIEEEAddress();

#ifdef DEBUG
    int dbg = 3;

    gsbutils::dprintf(dbg, "Zdo::handle_command\n");
    uint32_t ts = (uint32_t)command.payload(11) + (uint32_t)(command.payload(12) << 8) + (uint32_t)(command.payload(13) << 16) + (uint32_t)(command.payload(14) << 24);
    gsbutils::dprintf_c(7, "GroupID: 0x%04x \n", _UINT16(command.payload(0), command.payload(1)));
    gsbutils::dprintf_c(dbg, "ClusterID: 0x%04x \n", _UINT16(command.payload(2), command.payload(3)));
    gsbutils::dprintf_c(dbg, "source shortAddr: 0x%04x \n", message.source.address);
    gsbutils::dprintf_c(7, "destination address: 0x%04x \n", message.destination.address);
    gsbutils::dprintf_c(dbg, "source endpoint: 0x%02x \n", message.source.number);
    gsbutils::dprintf_c(7, "destination endpoint: 0x%02x \n", message.destination.number);
    gsbutils::dprintf_c(7, "broadcast: 0x%02x \n", command.payload(8)); //
    gsbutils::dprintf_c(dbg, "linkQuality: %d \n", message.linkquality);
    gsbutils::dprintf_c(7, "SecurityUse: %d \n", command.payload(10));
    gsbutils::dprintf_c(7, "ts %u \n", (uint32_t)(ts / 1000));                      // todo: привязать к реальному времени
    gsbutils::dprintf_c(dbg, "TransferSequenceNumber: %d \n", command.payload(15)); //?? чаще всего, здесь 0, а реальное значение в payload
    gsbutils::dprintf_c(dbg, "length of ZCL data %lu \n", length);
    gsbutils::dprintf_c(dbg, "zcl_frame.frame_type: %02x \n", static_cast<unsigned>(message.zcl_frame.frame_control.type));
    if (message.zcl_frame.manufacturer_code != 0xffff) // признак, что проприетарного кода производителя нет в сообщении
        gsbutils::dprintf_c(7, " zcl_frame.manufacturer_code: %04x \n", message.zcl_frame.manufacturer_code);
    gsbutils::dprintf_c(dbg, " zcl_frame.transaction_sequence_number: %d \n", message.zcl_frame.transaction_sequence_number); // номер транзакции, может повторять переданный в устройство номер, может иметь свой внутренний номер
    gsbutils::dprintf_c(dbg, " message.zcl_frame.command: 0x%02x \n", message.zcl_frame.command);
    gsbutils::dprintf_c(dbg, "zdo: message.zcl_frame.payload: ");
    for (uint8_t &b : message.zcl_frame.payload)
    {
        gsbutils::dprintf_c(dbg, "0x%02x ", b);
    }
    gsbutils::dprintf_c(dbg, "\n\n");
#else
    int dbg = 7;
#endif

    // Фиксируем качество связи устройства и текущее время
    if (message.linkquality > 0)
        ed->set_linkquality(message.linkquality);
    std::time_t tls = std::time(0); // get time now
    ed->set_last_seen((uint64_t)tls);

    // Кнопка сброса на кастомах
    if (message.cluster == zigbee::zcl::Cluster::ON_OFF &&
        ed->deviceInfo.deviceType == 4 &&
        message.source.number == 1)
    {
        return;
    }

    // Команды, требующее парсинга аттрибутов, есть во всех кластерах.
    // Поэтому эти команды выносим отдельно
    if (
        ((static_cast<zigbee::zcl::GlobalCommands>(message.zcl_frame.command) == zigbee::zcl::GlobalCommands::READ_ATTRIBUTES_RESPONSE || // 0x01
          static_cast<zigbee::zcl::GlobalCommands>(message.zcl_frame.command) == zigbee::zcl::GlobalCommands::REPORT_ATTRIBUTES) &&       // 0x0a
         message.cluster != zigbee::zcl::Cluster::ON_OFF) ||
        (static_cast<zigbee::zcl::GlobalCommands>(message.zcl_frame.command) == zigbee::zcl::GlobalCommands::REPORT_ATTRIBUTES && // 0x0a
         (message.cluster == zigbee::zcl::Cluster::ON_OFF)) ||
        ((static_cast<zigbee::zcl::GlobalCommands>(message.zcl_frame.command) == zigbee::zcl::GlobalCommands::READ_ATTRIBUTES_RESPONSE &&
          message.cluster == zigbee::zcl::Cluster::ON_OFF &&
          message.zcl_frame.payload.size() > 0)))

    {

        try
        {
            bool with_status = (message.cluster != zigbee::zcl::Cluster::ANALOG_INPUT &&
                                message.cluster != zigbee::zcl::Cluster::XIAOMI_SWITCH) &&
                               static_cast<zigbee::zcl::GlobalCommands>(message.zcl_frame.command) != zigbee::zcl::GlobalCommands::REPORT_ATTRIBUTES;

            if (message.zcl_frame.payload.size() > 0)
            {
                std::vector<zigbee::zcl::Attribute> attributes = zigbee::zcl::Attribute::parseAttributesPayload(message.zcl_frame.payload, with_status);
                if (attributes.size() > 0)
                    on_attribute_report(message.source, message.cluster, attributes);
            }
        }
        catch (std::exception &error)
        {
            std::stringstream sstream{};
            sstream << "Controller::OnMessage: Cluster: " << static_cast<uint16_t>(message.cluster)
                    << " 1.Invalid received zcl payload (" << error.what() << ") from 0x" << std::hex << (uint16_t)message.source.address
                    << " endpoint 0x" << std::hex << (uint16_t)message.source.number << " || ";
            for (auto p : message.zcl_frame.payload)
            {
                sstream << std::hex << (uint16_t)p << " ";
            }
            sstream << std::endl;
            OnError(sstream.str());
        }
    }
    else
    {

        //-----------------------------------------------------------------------------------------
        // далее не аттрибуты, а кластеро-зависимые команды, на которые надо реагировать, вызывать какой-то Action
        // кастомы сюда не приходят, у них всегда AttributeReport, даже при срабатывании
        switch (message.cluster)
        {
        case zigbee::zcl::Cluster::ON_OFF:
        {

#ifdef TEST
            int dbg = 3;
            gsbutils::dprintf(dbg, "Controller::onMessage: Cluster::ON_OFF: message.zcl_frame.command: 0x%02x Device 0x%04x\n", message.zcl_frame.command, message.source.address);
            gsbutils::dprintf_c(dbg, "Controller::onMessage: Cluster::ON_OFF: message.zcl_frame.payload: ");
            for (uint8_t &b : message.zcl_frame.payload)
            {
                gsbutils::dprintf_c(dbg, "0x%02x ", b);
            }
            gsbutils::dprintf_c(dbg, "\n");
#endif
            // сюда же приходят команды с датчика движения IKEA 
            onoff_command(message); // отправляем команду на исполнение
        }
        break;
        case zigbee::zcl::Cluster::LEVEL_CONTROL:
        {
#ifdef TEST
            int dbg = 4;
            gsbutils::dprintf(dbg, "Controller::onMessage: Cluster::LEVEL_CONTROL: message.zcl_frame.command: 0x%02x\n", message.zcl_frame.command);
            gsbutils::dprintf_c(dbg, "Controller::onMessage: Cluster::LEVEL_CONTROL: message.zcl_frame.payload: ");
            for (uint8_t &b : message.zcl_frame.payload)
            {
                gsbutils::dprintf_c(dbg, "0x%02x ", b);
            }
            gsbutils::dprintf_c(dbg, "\n");
#endif
            // команды димирования.
            // код начала команды при нажатии кнопки приходит в ZCL Frame в payload
            // при отпускании payload отсутствует
            level_command(message);
        }
        break;
        case zigbee::zcl::Cluster::IAS_ZONE:
        {
#ifdef TEST
            int dbg = 4;
            gsbutils::dprintf(dbg, "Controller::onMessage: Cluster::IAS_ZONE: message.zcl_frame.command: 0x%02x Device 0x%04x, \n", message.zcl_frame.command, message.source.address);
            gsbutils::dprintf_c(dbg, "Controller::onMessage: Cluster::IAS_ZONE: message.zcl_frame.payload: ");
            for (uint8_t &b : message.zcl_frame.payload)
            {
                gsbutils::dprintf_c(dbg, "%02x ", b);
            }
            gsbutils::dprintf_c(dbg, "\n");
#endif

            // в этот кластер входят датчики движения от Sonoff и датчики открытия дверей от Sonoff
            // разделяю по типу устройств
            if (ed->get_device_type() == 2) // датчики движения Sonoff
            {
                handle_motion(ed, message.zcl_frame.payload[0]);
                getPower(message.source.address, zigbee::zcl::Cluster::POWER_CONFIGURATION);
            }
            else if (ed->get_device_type() == 3) // датчики открытия дверей Sonoff
            {
                gsbutils::dprintf(dbg, "Device 0x%02x Door Sensor: %s \n", message.source.address, message.zcl_frame.payload[0] ? "Opened" : "Closed");
                handle_sonoff_door(ed, message.zcl_frame.payload[0]);
                getPower(message.source.address, zigbee::zcl::Cluster::POWER_CONFIGURATION);
            }
            else if (ed->get_device_type() == 5) // датчики протечек Aqara
            {
                ed->set_current_state(message.zcl_frame.payload[0] ? "ALARM" : "NORMAL");

                if (message.zcl_frame.payload[0] == 0x01)
                {
                    ias_zone_command(0, (uint16_t)0); // закрываем краны, выключаем стиральную машину
                    std::time_t ts = std::time(0);    // get time now
                    ed->set_last_action((uint64_t)ts);
                    std::string alarm_msg = "Сработал датчик протечки: ";
                    alarm_msg = alarm_msg + ed->get_human_name();
#ifdef WITH_TELEGA
                    tlg32->send_message(alarm_msg);
#endif
#ifdef WITH_SIM800
                    gsbutils::dprintf(1, "Phone number call \n");
                    gsmmodem->master_call();
#endif
                }

                gsbutils::dprintf(2, "Device 0x%02x Water Leak: %s \n ", message.source.address, message.zcl_frame.payload[0] ? "ALARM" : "NORMAL");
            }
        }
        break;
        case zigbee::zcl::Cluster::TIME:
        {
        }
        break;
        default:
        {
            gsbutils::dprintf(7, "Controller::on_message:  other command 0x%02x in cluster 0x%04x \n\n", message.zcl_frame.command, static_cast<uint16_t>(message.cluster));
        }
        }
    }

    after_message_action();
}

// Блок выполняемый после всех действий
void Controller::after_message_action()
{
    // Выполнять каждую минуту для устройств, которым нужен более частый контроль состояния
    std::vector<uint64_t> smartPlugDevices{0x70b3d52b6001b5d9};
    {
        std::time_t ts = std::time(0); // get time now
        if (ts - smartPlugTime > 60)
        {
            smartPlugTime = ts;

            for (uint64_t &di : smartPlugDevices)
            {
                std::shared_ptr<EndDevice> ed = get_device_by_mac((zigbee::IEEEAddress)di);
                if (ed)
                {
                    zigbee::NetworkAddress shortAddr = ed->getNetworkAddress();
                    // запрос тока и напряжения
                    std::vector<uint16_t> idsAV{0x0505, 0x508};
                    read_attribute(shortAddr, zigbee::zcl::Cluster::ELECTRICAL_MEASUREMENTS, idsAV);

                    if (ed->get_current_state(1) != "On" && ed->get_current_state(1) != "Off")
                    {
                        std::vector<uint16_t> idsAV{0x0000};
                        read_attribute(shortAddr, zigbee::zcl::Cluster::ON_OFF, idsAV);
                    }
                }
            }
        }
    }

    switch_off_with_timeout();
}

// Поставим проверку для некоторых реле - при отсутствии движения  больше получаса они должны быть принудительно выключены.
// TODO: Розетки здесь не будут выключены, двухканальное реле тоже
void Controller::switch_off_with_timeout()
{
    static bool off = false; // признак, чтобы не частить
    if (lastMotionSensorActivity > 0)
    {
        if (std::time(nullptr) - lastMotionSensorActivity > 1800)
        {
            if (!off)
            {
                off = true;
                gsbutils::dprintf(1, "Принудительное выключение реле  при отсутствии людей дома \n");

                tlg32->send_message("Принудительное выключение реле  при отсутствии людей дома.\n");

                for (const uint64_t &macAddress : EndDevice::OFF_LIST)
                {
                    std::shared_ptr<EndDevice> ed = get_device_by_mac((zigbee::IEEEAddress)macAddress);
                    if (ed)
                    {
                        if ("On" == ed->get_current_state(1))
                            switch_relay(macAddress, 0, 1);

                        if (macAddress == 0x00158d0009414d7e && "On" == ed->get_current_state(2))
                            switch_relay(macAddress, 0, 2);
                    }
                }
            }
        }
        else
        {
            off = false;
        }
    }
}

// управление реле по мак-адресу
// для двухканальных реле дополнительно указываем эндпойнт(по умолчанию 1)
void Controller::switch_relay(uint64_t mac_addr, uint8_t cmd, uint8_t ep)
{

    gsbutils::dprintf(6, "Relay 0x%" PRIx64 ", ep=%d, cmd=%d enter\n", mac_addr, ep, cmd);
    std::shared_ptr<EndDevice> ed = get_device_by_mac((zigbee::IEEEAddress)mac_addr);
    if (ed)
    {
        sendCommandToOnOffDevice(ed->getNetworkAddress(), cmd, ep);
        std::time_t ts = std::time(0); // get time now
        ed->set_last_action((uint64_t)ts);
    }
    else
    {
        gsbutils::dprintf(1, "Relay 0x%" PRIx64 " not found\n", mac_addr);
    }
}

// отправка команды Вкл/Выкл/Переключить в исполнительное устройство
// 0x01/0x00/0x02, остальные игнорируем в этой конфигурации
void Controller::sendCommandToOnOffDevice(zigbee::NetworkAddress address, uint8_t cmd, uint8_t ep)
{
    if (cmd > 2)
        return;
    zigbee::Endpoint endpoint{address, ep};
    Cluster cluster = zigbee::zcl::Cluster::ON_OFF;

    Frame frame;
    frame.frame_control.type = zigbee::zcl::FrameType::SPECIFIC;
    frame.frame_control.manufacturer_specific = false;
    frame.frame_control.direction = zigbee::zcl::FrameDirection::FROM_CLIENT_TO_SERVER;
    frame.frame_control.disable_default_response = false;
    frame.manufacturer_code = 0;
    frame.transaction_sequence_number = generateTransactionSequenceNumber();
    frame.command = cmd;

    sendMessage(endpoint, cluster, frame, 2s);
}

// Сообщения через OnLog являются информационными и не зависят от DEBUG
void Controller::OnLog(std::string logMsg)
{
    syslog(LOG_INFO, "%s", logMsg.c_str());
}

// Сообщения через OnError являются информационными и не зависят от DEBUG
// TODO: На компьютере писать в лог-файл
void Controller::OnError(std::string logMsg)
{
    syslog(LOG_ERR, "%s", logMsg.c_str());
}

void Controller::read_attribute(zigbee::NetworkAddress address, zigbee::zcl::Cluster cl, std::vector<uint16_t> ids)
{

    zigbee::Endpoint endpoint{address, 1};

    // ZCL Header
    Frame frame;
    frame.frame_control.type = zigbee::zcl::FrameType::GLOBAL;
    frame.frame_control.direction = zigbee::zcl::FrameDirection::FROM_CLIENT_TO_SERVER;
    frame.frame_control.disable_default_response = true;
    frame.frame_control.manufacturer_specific = false;
    frame.command = zigbee::zcl::GlobalCommands::READ_ATTRIBUTES; // 0x00
    frame.transaction_sequence_number = generateTransactionSequenceNumber();
    frame.manufacturer_code = 0;
    // end ZCL Header

    for (uint16_t id : ids)
    {
        frame.payload.push_back(LOWBYTE(id));
        frame.payload.push_back(HIGHBYTE(id));
    }

    sendMessage(endpoint, cl, frame);
}

/// все что дальше, унести в координатор
// обработчик пропажи 220В
void Controller::handle_power_off(int value)
{
    std::string alarm_msg = "";
    if (value == 1)
        alarm_msg = "Восстановлено 220В\n";
    else if (value == 0)
        alarm_msg = "Отсутствует 220В\n";
    else
        return;
    gsbutils::dprintf(1, alarm_msg);
#ifdef WITH_TELEGA
    tlg32->send_message(alarm_msg);
#endif
}

// Обработчик показаний температуры корпуса
// Включение/выключение вентилятора
void Controller::handle_board_temperature(float temp)
{
    char buff[128]{0};

#ifdef IS_PI
    if (temp > 70.0)
        write_pin(16, 1);
    else
        write_pin(16, 0);
#endif

    size_t len = snprintf(buff, 128, "Температура платы управления: %0.1f \n", temp);
    buff[len] = 0;
    std::string temp_msg = std::string(buff);
    gsbutils::dprintf(1, temp_msg);
#ifdef WITH_TELEGA
    tlg32->send_message(temp_msg);
#endif
}

// Включение звонка
void Controller::ringer()
{
#ifdef IS_PI
    write_pin(26, 1);
    sleep(1);
    write_pin(26, 0);
#endif
}

// Получить значение температуры управляющей платы
float Controller::get_board_temperature()
{
    char *fname = (char *)"/sys/class/thermal/thermal_zone0/temp";
    uint32_t temp_int = 0; // uint16_t не пролезает !!!!
    float temp_f = 0.0;

    int fd = open(fname, O_RDONLY);
    if (!fd)
        return -200.0;

    char buff[32]{0};
    size_t len = read(fd, buff, 32);
    close(fd);
    if (len < 0)
    {
        return -100.0;
    }
    buff[len - 1] = 0;
    if (sscanf(buff, "%d", &temp_int))
    {
        temp_f = (float)temp_int / 1000;
    }
    return temp_f;
}
// Управление вентилятором обдува платы управления
void Controller::fan(bool work)
{
#ifdef IS_PI
    write_pin(16, work ? 1 : 0);
#endif
}

// Параметры питания модема
std::string Controller::show_sim800_battery()
{
#ifdef WITH_SIM800
    static uint8_t counter = 0;
    char answer[256]{};
    std::array<int, 3> battery = gsmmodem->get_battery_level(false);
    //    std::string charge = battery[0] == 1 ? "Заряжается" : "Не заряжается";
    std::string charge = (battery[1] == 100 && battery[2] > 4400) ? "от сети" : "от батареи";
    std::string level = battery[1] == -1 ? "" : std::to_string(battery[1]) + "%";
    std::string volt = battery[2] == -1 ? "" : std::to_string((float)(battery[2] / 1000)) + "V";
    int res = std::snprintf(answer, 256, "SIM800l питание: %s, %s, %0.2f V\n", charge.c_str(), level.c_str(), battery[2] == -1 ? 0.0 : (float)battery[2] / 1000);
    if (res > 0 && res < 256)
        answer[res] = 0;
    else
        strcat(answer, (char *)"Ошибка получения инфо о батарее");

    counter++;
    if (counter > 5)
    {
        counter = 0;
        gsmmodem->get_battery_level(true);
    }
    return std::string(answer);

#else
    return "";
#endif
}

// получить устройство по сетевому адресу
std::shared_ptr<zigbee::EndDevice> Controller::getDeviceByShortAddr(zigbee::NetworkAddress network_address)
{
    std::string result = "";

    auto mac_address = end_devices_address_map_.find(network_address); // find ищет по ключу, а не по значению !!!

    if (mac_address != end_devices_address_map_.end())
    {
        return (std::shared_ptr<zigbee::EndDevice>)get_device_by_mac(mac_address->second);
    }

    return nullptr;
}

// Безопасное получение класса устройства по мак-адресу
std::shared_ptr<zigbee::EndDevice> Controller::get_device_by_mac(zigbee::IEEEAddress di)
{
    try
    {
        return devices_.at(di);
    }
    catch (std::out_of_range &ofr)
    {
        gsbutils::dprintf(6, "Device with mac_address 0x%" PRIx64 " absent in devices list\n", (uint64_t)di);
        return nullptr;
    }
}

// получить shortAddr устройства по мак-адресу
zigbee::NetworkAddress Controller::getShortAddrByMacAddr(zigbee::IEEEAddress mac_address)
{
    std::shared_ptr<zigbee::EndDevice> ed = nullptr;
    try
    {
        ed = devices_.at(mac_address);
        if (ed)
            return ed->getNetworkAddress();
    }
    catch (std::out_of_range &e)
    {
        return (zigbee::NetworkAddress)-1;
    }
    return (zigbee::NetworkAddress)-1;
}
