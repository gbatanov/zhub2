
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
#include "../telebot32/src/tlg32.h"
#include "../pi4-gpio.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../../gsb_utils/gsbutils.h"
#include "../common.h"
#include "zigbee.h"
#include "../modem.h"
#include "../app.h"

using namespace zigbee;

using zigbee::IEEEAddress;
using zigbee::NetworkAddress;
using zigbee::NetworkConfiguration;
using zigbee::zcl::Cluster;
using zigbee::zcl::Frame;

extern App app;
extern std::mutex trans_mutex;
extern std::atomic<uint8_t> transaction_sequence_number;

const std::vector<zigbee::SimpleDescriptor> Controller::DEFAULT_ENDPOINTS_ = {{1,      // Enpoint number.
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
    gsbutils::dprintf(1, "Controller init_adapter\n");
    bool noAdapter = false;
    try
    {
        // в connect происходит запуск потока приема команд zigbee
        if (!connect(app.config.Port, 115200))
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
        send_tlg_message("Zigbee adapter не обнаружен.\n");
    gsbutils::dprintf(3, "Controller init_adapter success\n");

    return noAdapter;
}
// Старт Zigbee-сети
bool Controller::start_network(std::vector<uint8_t> rfChannels)
{
    try
    {
        int res = reset(ResetType::SOFT, false, false);
        if (0 == res)
            throw std::runtime_error("Adapter reseting error1");

        // Перезаписываем при необходимости RF-каналы
        std::vector<uint8_t> rf = read_rf_channels();
        if (rf != rfChannels)
            write_rf_channels(rfChannels);
        finish_configuration();

        gsbutils::dprintf(1, "Controller::start_network: Startup zigbee adapter\n");
        // startup нужен всегда
        if (!startup())
            throw std::runtime_error("Adapter startup error");

        // Регистрация ендпойнтов самого координатора
        gsbutils::dprintf(1, "Zhub::start Endpoints registration:\n");
        for (auto &endpoint_descriptor : DEFAULT_ENDPOINTS_)
        {
            if (!registerEndpoint(endpoint_descriptor))
            {
                std::stringstream sstream;
                sstream << "Failure to register endpoint " << std::dec << endpoint_descriptor.endpoint_number << ", profile id: " << std::hex << endpoint_descriptor.profile_id;
                OnError(sstream.str());
            }
        }
        // получаем из файла связь сетевого и мак адресов, создаем объекты
        getDevicesMapFromFile(true);
        // разрешить прием входящих сообщений
        isReady_ = true;
        permitJoin(60s); // разрешаем джойн  при запуске на 1 минуту
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
    // Cообщения приходят от уже зарегистрированных работающих устройств,
    // поэтому можно получить сам объект и работать с ним

    zigbee::Message message{};

    // AF часть сообщения
    message.cluster = static_cast<zigbee::zcl::Cluster>(_UINT16(command.payload(2), command.payload(3)));
    message.source.address = _UINT16(command.payload(4), command.payload(5));
    message.destination.address = network_address_; // приемник - координатор
    message.source.number = command.payload(6);
    message.destination.number = command.payload(7);
    message.linkquality = command.payload(9);
    size_t length = static_cast<size_t>(command.payload(16));
    // ZCL Frame часть сообщения
    message.zcl_frame = parseZclData(std::vector<uint8_t>(&command.payload(17), &command.payload(17 + length)));

    std::shared_ptr<zigbee::EndDevice> ed = get_device_by_short_addr(message.source.address);
    if (!ed)
    {
        gsbutils::dprintf(1, "Сообщение от устройства, незарегистрированного в моей рабочей системе\n");
        return;
    }
    uint64_t macAddress = (uint64_t)ed->get_ieee_address();

#ifdef DEBUG
    int dbg = 3;

    gsbutils::dprintf_c(dbg, "Cluster %s (0x%04X) \n", get_cluster_string(message.cluster).c_str(), (uint16_t)message.cluster);
    if (message.cluster != zcl::Cluster::TIME)
    {
        uint32_t ts = (uint32_t)command.payload(11) + (uint32_t)(command.payload(12) << 8) + (uint32_t)(command.payload(13) << 16) + (uint32_t)(command.payload(14) << 24);
        gsbutils::dprintf_c(7, "GroupID: 0x%04x \n", _UINT16(command.payload(0), command.payload(1)));
        gsbutils::dprintf_c(dbg, "source endpoint:: shortAddr: 0x%04x ", message.source.address);
        gsbutils::dprintf_c(dbg, "number: 0x%02x ", message.source.number);
        gsbutils::dprintf_c(dbg, "%s \n", ed->get_human_name().c_str());
        gsbutils::dprintf_c(7, "destination address: 0x%04x \n", message.destination.address);
        gsbutils::dprintf_c(7, "destination endpoint: 0x%02x \n", message.destination.number);
        gsbutils::dprintf_c(7, "broadcast: 0x%02x \n", command.payload(8)); //
        gsbutils::dprintf_c(dbg, "linkQuality: %d \n", message.linkquality);
        gsbutils::dprintf_c(7, "SecurityUse: %d \n", command.payload(10));
        gsbutils::dprintf_c(7, "ts %u \n", (uint32_t)(ts / 1000));                                                               // todo: привязать к реальному времени
        gsbutils::dprintf_c(dbg, "TransferSequenceNumber: %d \n", command.payload(15));                                          //?? чаще всего, здесь 0, а реальное значение в payload
        gsbutils::dprintf_c(dbg, "zcl_frame.transaction_sequence_number: %d \n", message.zcl_frame.transaction_sequence_number); // номер транзакции, может повторять переданный в устройство номер, может иметь свой внутренний номер
        if (message.zcl_frame.manufacturer_code != 0xffff)                                                                       // признак, что проприетарного кода производителя нет в сообщении
            gsbutils::dprintf_c(7, "zcl_frame.manufacturer_code: %04x \n", message.zcl_frame.manufacturer_code);
        gsbutils::dprintf_c(dbg, "length of ZCL data %lu \n", length);
        gsbutils::dprintf_c(dbg, "zcl_frame::frame_type: %02x  ", static_cast<unsigned>(message.zcl_frame.frame_control.type));
        gsbutils::dprintf_c(dbg, "command: 0x%02x \n", message.zcl_frame.command);
        gsbutils::dprintf_c(dbg, "zdo: message.zcl_frame.payload: ");
        for (uint8_t &b : message.zcl_frame.payload)
        {
            gsbutils::dprintf_c(dbg, "0x%02x ", b);
        }
        gsbutils::dprintf_c(dbg, "\n\n");
    }
#else
    int dbg = 7;
#endif

    // Фиксируем качество связи устройства и текущее время
    if (message.linkquality > 0)
        ed->set_linkquality(message.linkquality);
    std::time_t tls = std::time(0); // get time now
    ed->set_last_seen((uint64_t)tls);

    if (message.zcl_frame.frame_control.type == zigbee::zcl::FrameType::GLOBAL)
    {
        // Глобальные команды
        if ((static_cast<zigbee::zcl::GlobalCommands>(message.zcl_frame.command) == zigbee::zcl::GlobalCommands::READ_ATTRIBUTES_RESPONSE || // 0x01
             static_cast<zigbee::zcl::GlobalCommands>(message.zcl_frame.command) == zigbee::zcl::GlobalCommands::REPORT_ATTRIBUTES)          // 0x0a
        )
        {
            // Команды, требующие парсинга аттрибутов
            try
            {
                bool with_status = (message.cluster != zigbee::zcl::Cluster::ANALOG_INPUT && message.cluster != zigbee::zcl::Cluster::XIAOMI_SWITCH) &&
                                   static_cast<zigbee::zcl::GlobalCommands>(message.zcl_frame.command) != zigbee::zcl::GlobalCommands::REPORT_ATTRIBUTES;

                std::vector<zigbee::zcl::Attribute> attributes = zigbee::zcl::Attribute::parseAttributesPayload(message.zcl_frame.payload, with_status);
                if (attributes.size() > 0)
                    on_attribute_report(message.source, message.cluster, attributes);
            }
            catch (std::exception &error)
            {
                gsbutils::dprintf(1, "%s\n", error.what());
            }
        }
    }
    else
    {
        // Команды, специфичные для кластеров
        switch (message.cluster)
        {
        case zigbee::zcl::Cluster::ON_OFF:
        {
            // сюда же приходят команды с датчика движения IKEA 
            onoff_command(message); // отправляем команду на исполнение
        }
        break;
        case zigbee::zcl::Cluster::LEVEL_CONTROL:
        {
            // команды димирования.
            // код начала команды при нажатии кнопки приходит в ZCL Frame в payload
            // при отпускании payload отсутствует
            level_command(message);
        }
        break;
        case zigbee::zcl::Cluster::IAS_ZONE:
        {
            // в этот кластер входят датчики движения от Sonoff и датчики открытия дверей от Sonoff
            // разделяю по типу устройств
            if (ed->get_device_type() == 2) // датчики движения Sonoff
            {
                handle_motion(ed, message.zcl_frame.payload[0]);
                if (ed->check_last_power_query())
                    get_power(message.source.address, zigbee::zcl::Cluster::POWER_CONFIGURATION);
            }
            else if (ed->get_device_type() == 3) // датчики открытия дверей Sonoff
            {
                handle_sonoff_door(ed, message.zcl_frame.payload[0]);
                if (ed->check_last_power_query())
                    get_power(message.source.address, zigbee::zcl::Cluster::POWER_CONFIGURATION);
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

                    send_tlg_message(alarm_msg);

                    if (app.withSim800)
                    {
                        gsbutils::dprintf(1, "Phone number call \n");
                        app.gsmModem->master_call();
                    }
                }
                gsbutils::dprintf(1, "Device 0x%02x Water Leak: %s \n ", message.source.address, message.zcl_frame.payload[0] ? "ALARM" : "NORMAL");
            }
        }
        break;
        case zigbee::zcl::Cluster::TIME: // 0x000A
        {
            // необслуживаемый кластер
        }
        break;
        default:
        {
            // неизвестный кластер
            gsbutils::dprintf(1, "Controller::on_message:  other command 0x%02x in cluster 0x%04x \n\n", message.zcl_frame.command, static_cast<uint16_t>(message.cluster));
        }
        }
    }
    after_message_action();
}

// Блок выполняемый после всех действий
// Играет функцию таймера, поскольку точность в данной системе не важна,
// а выделенные таймеры отъедают потоки, что для Raspberry плохо
void Controller::after_message_action()
{
    // Выполнять каждую минуту для устройств, которым нужен более частый контроль состояния
    // 0x70b3d52b6001b5d9 - зарядники
    std::vector<uint64_t> smartPlugDevices{0x70b3d52b6001b5d9};

    std::time_t ts = std::time(0); // get time now
    if (ts - smartPlugTime > 60)
    {
        smartPlugTime = ts;

        app.zhub->check_motion_activity();

        for (uint64_t &di : smartPlugDevices)
        {
            std::shared_ptr<EndDevice> ed = get_device_by_mac((zigbee::IEEEAddress)di);
            if (ed)
            {
                zigbee::NetworkAddress shortAddr = ed->get_network_address();
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
        // краны
        {
            // Получим состояние кранов, если не было получено при старте
            std::shared_ptr<EndDevice> ed1 = get_device_by_mac((zigbee::IEEEAddress)0xa4c138d9758e1dcd);
            if (ed1 && ed1->get_current_state(1) != "On" && ed1->get_current_state(1) != "Off")
            {
                uint16_t shortAddr = getShortAddrByMacAddr((zigbee::IEEEAddress)0xa4c138d9758e1dcd);
                std::vector<uint16_t> idsAV{0x0000};
                read_attribute(shortAddr, zigbee::zcl::Cluster::ON_OFF, idsAV);
            }
            std::shared_ptr<EndDevice> ed2 = get_device_by_mac((zigbee::IEEEAddress)0xa4c138373e89d731);
            if (ed2 && ed2->get_current_state(1) != "On" && ed2->get_current_state(1) != "Off")
            {
                std::vector<uint16_t> idsAV{0x0000};
                uint16_t shortAddr = getShortAddrByMacAddr((zigbee::IEEEAddress)0xa4c138373e89d731);
                read_attribute(shortAddr, zigbee::zcl::Cluster::ON_OFF, idsAV);
            }
        }
    }

    switch_off_with_timeout();
}

// При отсутствии движения  больше 20 минут устройства из списка EndDevice::OFF_LIST должны быть принудительно выключены.
void Controller::switch_off_with_timeout()
{
    static bool off = false; // признак, чтобы не частить
    if (lastMotionSensorActivity > 0)
    {
        if (std::time(nullptr) - lastMotionSensorActivity > 1200)
        {
            if (!off)
            {
                off = true;
                gsbutils::dprintf(1, "Принудительное выключение реле  при отсутствии людей дома \n");

                send_tlg_message("Никого нет дома.\n");

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
        sendCommandToOnOffDevice(ed->get_network_address(), cmd, ep);
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

// получить устройство по сетевому адресу
std::shared_ptr<zigbee::EndDevice> Controller::get_device_by_short_addr(zigbee::NetworkAddress network_address)
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
            return ed->get_network_address();
    }
    catch (std::out_of_range &e)
    {
        return (zigbee::NetworkAddress)-1;
    }
    return (zigbee::NetworkAddress)-1;
}

// вызывается для набора аттрибутов в одном ответе от одного устройства
void Controller::on_attribute_report(zigbee::Endpoint endpoint, Cluster cluster, std::vector<zcl::Attribute> attributes)
{
    std::shared_ptr<zigbee::EndDevice> ed = get_device_by_short_addr(endpoint.address);
    if (!ed)
        return;
    uint64_t macAddress = (uint64_t)ed->get_ieee_address();

    switch (cluster)
    {
    case Cluster::BASIC: // 0x0000
    {
        zigbee::clusters::Basic clusterHandler(ed);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    case Cluster::POWER_CONFIGURATION: // 0x0001
    {
        zigbee::clusters::PowerConfiguration clusterHandler(ed);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    case Cluster::ON_OFF: // 0x0006
    {
        zigbee::clusters::OnOff clusterHandler(ed);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    case Cluster::DEVICE_TEMPERATURE_CONFIGURATION: // 0x0002
    {
        zigbee::clusters::DeviceTemperatureConfiguration clusterHandler(ed);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    case Cluster::ANALOG_INPUT: // 0x000C
    {
        // сюда относятся кастомы с датчиками температуры и влажности AM2302(DHT22) и  HDC1080
        // двухканальное реле Aqara
        zigbee::clusters::AnalogInput clusterHandler(ed);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    case Cluster::MULTISTATE_INPUT: // 0x0012
    {
        zigbee::clusters::MultistateInput clusterHandler(ed);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    case Cluster::XIAOMI_SWITCH: // 0xFCC0
    {
        zigbee::clusters::Xiaomi clusterHandler(ed);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    case Cluster::SIMPLE_METERING: // 0x0702
    {
        zigbee::clusters::SimpleMetering clusterHandler(ed);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    case Cluster::ELECTRICAL_MEASUREMENTS: // 0x0B04
    {
        zigbee::clusters::ElectricalMeasurements clusterHandler(ed);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    case Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER: // 0xe001
    {                                               // умная розетка и кран
        zigbee::clusters::Tuya clusterHandler(ed);
        clusterHandler.attribute_handler_private(attributes, endpoint);
    }
    break;
    case Cluster::TUYA_SWITCH_MODE_0: // 0xE000
    {
        zigbee::clusters::Tuya clusterHandler(ed);
        clusterHandler.attribute_handler_switch_mode(attributes, endpoint);
    }
    break;
    case Cluster::RSSI: // 0x000b
    case Cluster::TIME: // 0x000a
    {
        zigbee::clusters::Other clusterHandler(ed, cluster);
        clusterHandler.attribute_handler(attributes, endpoint);
    }
    break;
    default:
        gsbutils::dprintf(1, "Zhub::on_attribute_report: unknownCluster 0x%04x \n", (uint16_t)cluster);
        for (auto attribute : attributes)
        {
            gsbutils::dprintf(1, "Zhub::on_attribute_report: unknown attribute Id 0x%04x in Cluster 0x%04x \n", attribute.id, (uint16_t)cluster);
        }
    } // switch
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
bool Controller::configureReporting(zigbee::NetworkAddress address)
{
    return configureReporting(address, Cluster::POWER_CONFIGURATION);
}
bool Controller::configureReporting(zigbee::NetworkAddress address,
                                    Cluster cluster,
                                    uint16_t attributeId,
                                    zcl::Attribute::DataType attribute_data_type,
                                    uint16_t reportable_change)
{
#ifdef DEBUG
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
    uint16_t min_interval = 60;   // 1 minutes
    uint16_t max_interval = 3600; // 1 hours

    std::shared_ptr<zigbee::EndDevice> ed = get_device_by_short_addr(address);

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

// Вызываем при Join and Leave
bool Controller::setDevicesMapToFile()
{
    std::string prefix = "/usr/local";

    int dbg = 3;

    std::string filename = app.config.MapPath;

    std::FILE *fd = std::fopen(filename.c_str(), "w");
    if (!fd)
    {
        gsbutils::dprintf(1, "Zhub::setDevicesMapToFile: error opening file\n");
        return false;
    }

    for (auto m : end_devices_address_map_)
    {
        fprintf(fd, "%04x %" PRIx64 " \n", m.first, m.second); // лидирующие нули у PRIx64 не записываются, надо учитывать при чтении
        gsbutils::dprintf(dbg, "%04x %" PRIx64 " \n", m.first, m.second);
    }

    fclose(fd);
    gsbutils::dprintf(dbg, "Zhub::setDevicesMapToFile: write map to file\n");

    return true;
}

// Вызываем сразу после старта конфигуратора
// заполняем std::map<zigbee::NetworkAddress, zigbee::IEEEAddress> end_devices_address_map_
// Для варианта на компе и малине делаю запись в файл и чтение из файла, ибо с памятью CC2531 есть проблемы
bool Controller::getDevicesMapFromFile(bool with_reg)
{

    int dbg = 3;
    gsbutils::dprintf(3, "Zhub::getDevicesMap: START\n");

    std::map<uint16_t, uint64_t> item_data = readMapFromFile();

    for (auto b : item_data)
    {
        gsbutils::dprintf_c(dbg, "0x%04x 0x%" PRIx64 "\n", b.first, b.second);
        join_device(b.first, b.second); // тут запись в файл не требуется
    }
    gsbutils::dprintf(3, "Zhub::getDevicesMap: FINISH\n");

    return true;
}

std::map<uint16_t, uint64_t> Controller::readMapFromFile()
{

    std::map<uint16_t, uint64_t> item_data{};

    std::string filename = app.config.MapPath;
#ifdef DEBUG
    gsbutils::dprintf(1, "MapFile  opening file %s\n", filename.c_str());
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

/// Создаем новое устройство
/// Сохраняем ссылку на него в мапе
/// Перед добавлением старое устройство (если есть в списках) удаляем
/// @param norec писать или нет в файл, по умолчанию - нет
void Controller::join_device(zigbee::NetworkAddress network_address, zigbee::IEEEAddress mac_address, bool norec)
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
            zigbee::NetworkAddress shortAddr = ed->get_network_address();
            if (shortAddr != network_address)
            {
                ed->set_network_address(network_address);
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
        gsbutils::dprintf(1, "Zhub::join_device error %s \n", e.what());
    }
}

// Функция вызывается периодически  для устройств, не передающих эти параметры самостоятельно.
// Ответ разбираем  в OnMessage
// Надо запускать в потоке
void Controller::get_power(zigbee::NetworkAddress address, Cluster cluster)
{
    gsbutils::dprintf(7, "Controller::get_power\n");

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
void Controller::on_join(zigbee::NetworkAddress networkAddress, zigbee::IEEEAddress macAddress)
{
    int dbg = 1;

    gsbutils::dprintf(dbg, "Controllerb::on_join mac_address 0x%" PRIx64 "\n", macAddress);
    join_device(networkAddress, macAddress, true);

    std::shared_ptr<zigbee::EndDevice> ed = get_device_by_mac(macAddress);
    if (!ed)
    {
        gsbutils::dprintf(1, "Device 0x%" PRIx64 " not found\n", macAddress);
        return;
    }

    // bind срабатывает только при спаривании/переспаривании устройства, повторный биндинг дает ошибку
    // ON_OFF попробуем конфигурить всегда
    // для кастомных устройств репортинг не надо конфигурить!!!
    bind(networkAddress, macAddress, 1, zigbee::zcl::Cluster::ON_OFF);
    if (ed->get_device_type() != 4)
        configureReporting(networkAddress, zigbee::zcl::Cluster::ON_OFF); //

    /*
        // smart plug  - репортинга нет, данные получаем только по запросу аттрибутов
        if (ed->get_device_type() == 10)
        {
            bind(network_address, mac_address, 1, zigbee::zcl::Cluster::ELECTRICAL_MEASUREMENTS);
            configureReporting(network_address, zigbee::zcl::Cluster::ELECTRICAL_MEASUREMENTS, 0x0505, zcl::Attribute::DataType::UINT16, 0x00);
            configureReporting(network_address, zigbee::zcl::Cluster::ELECTRICAL_MEASUREMENTS, 0x0508, zcl::Attribute::DataType::UINT16, 0x00);
        }

        // краны репортинг отключил, после настройки кран перестал репортить
        if ((uint64_t)mac_address == 0xa4c138373e89d731 || (uint64_t)mac_address == 0xa4c138d9758e1dcd)
        {
            bind(network_address, mac_address, 1, zigbee::zcl::Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER);
            configureReporting(network_address, zigbee::zcl::Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER);
            bind(network_address, mac_address, 1, zigbee::zcl::Cluster::TUYA_SWITCH_MODE_0);
            configureReporting(network_address, zigbee::zcl::Cluster::TUYA_SWITCH_MODE_0);
        }
        */

    // Датчики движения и открытия дверей Sonoff
    if (ed->get_device_type() == 2 || ed->get_device_type() == 3)
    {
        bind(networkAddress, macAddress, 1, zigbee::zcl::Cluster::IAS_ZONE);
        configureReporting(networkAddress, zigbee::zcl::Cluster::IAS_ZONE); //
    }
    // датчики движения IKEA
    if (ed->get_device_type() == 8)
    {
        bind(networkAddress, macAddress, 1, zigbee::zcl::Cluster::IAS_ZONE);
        configureReporting(networkAddress, zigbee::zcl::Cluster::IAS_ZONE); //
    }

    // для икеевских устройств необходимо запрашивать питание, когда от них приходит сообщение
    // у них есть POLL INTERVAL во время которого они могут принять комманду на исполнение
    if (ed->get_device_type() == 7 || ed->get_device_type() == 8)
    {
        if (ed->check_last_power_query())
            get_power(networkAddress, zigbee::zcl::Cluster::POWER_CONFIGURATION);
    }
    // у остальных устройств надо сделать привязку, чтобы получить параметры питания (оставшийся заряд батареи)
    bind(networkAddress, macAddress, 1, zigbee::zcl::Cluster::POWER_CONFIGURATION);
    if (ed->get_device_type() != 4)
        configureReporting(networkAddress, zigbee::zcl::Cluster::POWER_CONFIGURATION);

    // пока спящий датчик не проявится сам, инфу об эндпойнтах не получить, только при спаривании
    activeEndpoints(networkAddress);
    simpleDescriptor(networkAddress, 1); // TODO: получить со всех эндпойнотов, полученных на предыдущем этапе

    get_identifier(networkAddress); // Для многих устройств этот запрос обязателен!!!! Без него не работатет устройство, только регистрация в сети
    gsbutils::dprintf(1, showDeviceInfo(networkAddress));
}

// Ничего не делаем, список поддерживается вручную
// из координатора запись удаляется самим координатором
void Controller::on_leave(NetworkAddress network_address, IEEEAddress mac_address)
{
    int dbg = 4;
    gsbutils::dprintf(dbg, "Controller::on_leave 0x%04x 0x%" PRIx64 " \n", (uint16_t)network_address, (uint64_t)mac_address);
}

/// Идентификаторы возвращаются очень редко ( но возвращаются)
/// их не надо ждать сразу, ответ будет в OnMessage
/// реальная польза только в тестировании нового устройства, в штатном режиме можно отключить
void Controller::get_identifier(zigbee::NetworkAddress address)
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

std::string Controller::showDeviceInfo(zigbee::NetworkAddress network_address)
{
    std::string result = "";

    auto mac_address = end_devices_address_map_.find(network_address); // find ищет по ключу, а не по значению !!!

    if (mac_address != end_devices_address_map_.end())
    {
        std::shared_ptr<zigbee::EndDevice> ed = get_device_by_mac(mac_address->second);
        if (ed && !ed->get_model_identifier().empty())
        {
            result = result + ed->get_model_identifier();
            if (!ed->get_manufacturer().empty())
            {
                result = result + " | " + ed->get_manufacturer();
            }
            if (!ed->get_product_code().empty())
            {
                result = result + " | " + ed->get_product_code();
            }
            result = result + "\n";
        }
    }
    else
    {
        // такое бывает, после перезапуска программы - shortAddr уже есть у координатора, но объект не создан - TODO: уже не должно быть
        gsbutils::dprintf(1, "Zhub::showDeviceInfo: shortAddr 0x%04x не найден в мапе адресов\n", network_address);
    }

    return result;
}