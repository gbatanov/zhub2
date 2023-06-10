#include <stdio.h>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <set>
#include <ctime>
#include <sys/time.h>
#include <array>
#include <queue>
#include <memory>
#include <optional>
#include <any>
#include <string.h>
#include <termios.h>

#include "../version.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../../gsb_utils/gsbutils.h"
#include "../common.h"
#include "zigbee.h"

using gsb_utils = gsbutils::SString;
using namespace zigbee;

// TODO: добавить команду получения текущего статуса устройства для кластера ON_OFF
// MAC Address,Type, Vendor,Model, EngName, Human name, Main cluster,Power source,available,test
const std::map<const uint64_t, const DeviceInfo> EndDevice::KNOWN_DEVICES = {
    {0x00158d0006e469a4, {5, "Aqara", "SJCGQ11LM", "Протечка1", "Датчик протечки 1 (туалет)", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    {0x00158d0006f8fc61, {5, "Aqara", "SJCGQ11LM", "Протечка2", "Датчик протечки 2 (кухня)", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    {0x00158d0006b86b79, {5, "Aqara", "SJCGQ11LM", "Протечка3", "Датчик протечки 3 (ванна)", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    {0x00158d0006ea99db, {5, "Aqara", "SJCGQ11LM", "Протечка4", "Датчик протечки 4 (кухня)", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    // реле
    {0x54ef44100019335b, {9, "Aqara", "SSM-U01", "Реле1", "Реле 1", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0x54ef441000193352, {9, "Aqara", "SSM-U01", "Стиралка", "Реле 2(Стиральная машина)", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0x54ef4410001933d3, {9, "Aqara", "SSM-U01", "КоридорСвет", "Реле 4(Свет в коридоре)", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0x54ef44100018b523, {9, "Aqara", "SSM-U01", "ШкафСвет", "Реле 3(Шкаф, подсветка)", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0x54ef4410005b2639, {9, "Aqara", "SSM-U01", "ТулетЗанят", "Реле 5(Туалет занят)", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0x54ef441000609dcc, {9, "Aqara", "SSM-U01", "Реле6", "Реле 6", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 1}},
    {0x00158d0009414d7e, {11, "Aqara", "Double", "КухняСвет/КухняВент", "Реле 7(Свет/Вентилятор кухня)", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    // Умные розетки
    {0x70b3d52b6001b4a4, {10, "Girier", "TS011F", "Розетка1", "Розетка 1", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 1}},
    {0x70b3d52b6001b5d9, {10, "Girier", "TS011F", "Розетка2", "Розетка 2(Зарядники)", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0x70b3d52b60022ac9, {10, "Girier", "TS011F", "Розетка3", "Розетка 3(Лампы в детской)", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0x70b3d52b60022cfd, {10, "Girier", "TS011F", "Розетка3", "Розетка 4(Паяльник)", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    // краны
    {0xa4c138d9758e1dcd, {6, "TUYA", "Valve", "КранГВ", "Кран 1 ГВ", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0xa4c138373e89d731, {6, "TUYA", "Valve", "КранХВ", "Кран 2 ХВ", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    // датчики движения и/или освещения
    {0x00124b0025137475, {2, "Sonoff", "SNZB-03", "КоридорДвижение", "Датчик движения 1 (коридор)", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    {0x00124b0024455048, {2, "Sonoff", "SNZB-03", "КомнатаДвижение", "Датчик движения 2 (комната)", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    {0x00124b002444d159, {2, "Sonoff", "SNZB-03", "Движение3", "Датчик движения 3 ", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    {0x00124b0009451438, {4, "Custom", "CC2530", "КухняДвижение", "Датчик присутствия1 (кухня)", zigbee::zcl::Cluster::BASIC, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0x00124b0014db2724, {4, "Custom", "CC2530", "ПрихожаяДвижение", "Датчик движение + освещение (прихожая)", zigbee::zcl::Cluster::BASIC, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    {0x0c4314fffe17d8a8, {8, "IKEA", "E1745", "ИкеаДвижение", "Датчик движения IKEA", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::BATTERY, 0, 1}},
    {0x00124b0007246963, {4, "Custom", "CC2530", "ДетскаяДвижение", "Датчик движение + освещение (детская)", zigbee::zcl::Cluster::BASIC, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}},
    // Датчики открытия дверей
    {0x00124b0025485ee6, {3, "Sonoff", "SNZB-04", "ТуалетДатчик", "Датчик открытия 1 (туалет)", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    {0x00124b002512a60b, {3, "Sonoff", "SNZB-04", "ШкафДатчик", "Датчик открытия 2 (шкаф, подсветка)", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    {0x00124b00250bba63, {3, "Sonoff", "SNZB-04", "ЯщикДатчик", "Датчик открытия 3 (ящик)", zigbee::zcl::Cluster::IAS_ZONE, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    // Кнопки
    {0x8cf681fffe0656ef, {7, "IKEA", "E1743", "КнопкаИкеа", "Кнопка ИКЕА", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::BATTERY, 0, 1}},
    {0x00124b0028928e8a, {1, "Sonoff", "SNZB-01", "Кнопка1", "Кнопка Sonoff 1", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    {0x00124b00253ba75f, {1, "Sonoff", "SNZB-01", "Кнопка2", "Кнопка Sonoff 2", zigbee::zcl::Cluster::ON_OFF, zigbee::zcl::Attributes::PowerSource::BATTERY, 1, 0}},
    // Датчики климата
    {0x00124b000b1bb401, {4, "GSB", "CC2530", "КлиматБалкон", "Датчик климата (балкон)", zigbee::zcl::Cluster::ANALOG_INPUT, zigbee::zcl::Attributes::PowerSource::SINGLE_PHASE, 1, 0}}};

// Типы устройств в моей классификации
// 0x0003 - Identify, 0x0004 - GROUPS, 0x0005 - SCENES, 0x0006 - ON_OFF, 0x0007 - ON_OFF_SWITCH_CONFIGURATION, 0x0008 - LEVEL_CONTROL,0x0009 - ALARMS,
// 0x000a - Time, 0x000c - ANALOG_INPUT, 0x0012 - MULTISTATE_INPUT, 0x0019 - OTA, 0x0020 - POLL_CONTROL,
// 0x0102 - WINDOW_COVERING, 0x0500 - IAS_ZONE, 0x0702 - METER_SMART_ENERGY, 0x0b04 - ELECTRICAL_MEASUREMENTS
// 0x1000 - TouchlinkCommissioning, 0xfc7c - Unknown, 0xffff - Unknown, 0xfcc0 - Unknown
const std::map<const uint8_t, const std::string> EndDevice::DEVICE_TYPES = {
    {1, "SonoffButton"},       // 1 endpoint (3 input cluster - 0x0000, 0x0001, 0x0003,        2 output cluster - 0x0003, 0x0006 )
    {2, "SonoffMotionSensor"}, // 1 endpoint (4 input cluster - 0x0000, 0x0001, 0x0003,0x0500, 1 output cluster - 0x0003,        )
    {3, "SonoffDoorSensor"},   // 1 endpoint (4 input cluster - 0x0000, 0x0001, 0x0003,0x0500, 1 output cluster - 0x0003,       )
    {4, "Custom"},
    {5, "WaterSensor"}, // 1 endpoint (3 input cluster - 0x0000, 0x0001, 0x0003,        1 output cluster - 0x0019        )
    {6, "WaterValve"},
    {7, "IkeaButton"},         // 1 endpoint (7 input cluster - 0x0000, 0x0001, 0x0003,0x0009,0x0020,0x1000,0xfc7c 7 output cluster - 0x0003,0x0004,0x0006,0x0008,0x0019,0x0102, 0x1000)
    {8, "IkeaMotionSensor"},   // 1 endpoint (7 input cluster - 0x0000, 0x0001, 0x0003,0x0009,0x0020,0x1000,0xfc7c 6 output cluster - 0x0003,0x0004,0x0006,0x0008,0x0019,0x1000 )
    {9, "RelayAqara"},         // 5 endpoint 1 - (8 input cluster - 0x0000, 0x0002, 0x0003,0x0004,0x0005,0x0009,0x000a,0xfcc0 3 output cluster - 0x000a,0x0019,0xffff)
                               //   endpoint 21, 31 - (1 input cluster - 0x0012 )
                               //   endpoint 41 - (1 input cluster - 0x000c )
                               //   endpoint 242 - (1 output cluster 0x0021)
                               //
    {10, "SmartPlug"},         // 1 endpoint (8 input cluster - 0x0000, 0x0003, 0x0004, 0x0005, 0x0006, 0x0702,0x0b04, 0xe001,  )
    {11, "RelayAqaraDouble"}}; // 2 endpoint {11 input cluster - 0x0000, 0x0001,0x0002, 0x0003,0x0004,0x0005,0x0006,0x000a,0x0010,0x0b04,0x000c 2 output cluster - 0x000a,0x0019}

// Список устройств, отключаемых при долгом нажатии на кнопку Sonoff1
// Этот же список используем для принудительного выключения в режиме Никого нет дома (или Все спят)
const std::vector<uint64_t> EndDevice::OFF_LIST = {
    0x54ef4410001933d3, // свет в коридоре
    0x00158d0009414d7e, // свет и вентилятор в кухне
    0x54ef44100018b523, // шкаф(подсветка)
    0x54ef4410005b2639, // туалет занят
    0x70b3d52b6001b4a4, // Умная розетка 1
    0x70b3d52b60022ac9, // Умная розетка 3
    0x70b3d52b60022cfd, // Умная розетка 4
    0x54ef44100019335b, // Реле1
    0x54ef441000609dcc  // Реле6
};

// Список устройств для отображения в Графане
const std::vector<uint64_t> EndDevice::PROM_MOTION_LIST = {
    0x00124b0025137475, // коридор
    0x00124b0014db2724, // прихожая
    0x00124b0009451438, // кухня
    0x00124b0024455048, // комната
    0x00124b002444d159, // детская
    0x00124b0007246963  // балкон
};
const std::vector<uint64_t> EndDevice::PROM_DOOR_LIST = {
    0x00124b0025485ee6 // туалет
};
const std::vector<uint64_t> EndDevice::PROM_RELAY_LIST = {
    0x00158d0009414d7e,  // свет/вентилятор кухня
    0x54ef4410001933d3,  // свет в коридоре
    0x54ef4410005b2639}; // туалет занят

extern std::atomic<bool> Flag;

EndDevice::EndDevice(zigbee::NetworkAddress shortAddr, zigbee::IEEEAddress IEEEAddress) : shortAddr_(shortAddr), IEEEAddress_(IEEEAddress) {}

EndDevice::~EndDevice(){};

void EndDevice::init()
{
    try
    {
        DeviceInfo di = KNOWN_DEVICES.at(static_cast<const uint64_t>(IEEEAddress_));
        deviceInfo = di;
        modelIdentifier_ = DEVICE_TYPES.at(di.deviceType);
        linkquality_ = 0;
    }
    catch (std::exception &e)
    {
        gsbutils::dprintf(1, "Err0r init \n");
    }
}

void EndDevice::set_battery_params(uint8_t battery_remain_percent, double battery_voltage)
{
    if (battery_remain_percent > 0)
        battery_remain_percent_ = battery_remain_percent;
    if (battery_voltage > 0)
        battery_voltage_ = battery_voltage;
};

void EndDevice::set_power_source(uint8_t power_source)
{
    if (power_source > 0)
        deviceInfo.powerSource = static_cast<zigbee::zcl::Attributes::PowerSource>(power_source);
};

std::string EndDevice::show_power_source()
{
    std::string result = "";
    uint8_t ps = static_cast<uint8_t>(deviceInfo.powerSource);

    result = "";
    switch (ps)
    {
    case 0x01:
    case 0x81:
        result = result + "Single_Phase";
        break;
    case 0x02:
        result = result + "Three_Phase";
        break;
    case 0x03:
        result = result + "Battery";
        break;
    case 0x04:
        result = result + "DC";
        break;
    case 0x05: //"Emergency_constantly"
        result = result + "Emergency";
        break;
    case 0x06: //"EMERGENCY_MAINS_AND_TRANSFER_SWITCH"
        result = result + "Other";
        break;
    default:
        return "";
        break;
    }

    return result;
}
std::string EndDevice::show_battery_remain()
{
    std::string result = "";
    uint8_t val = battery_remain_percent_ / 2;
    if (val > 0)
    {
        char buf[16]{0};
        int len = snprintf(buf, 16, "%0d%%", val);
        if (len > 0)
        {
            buf[len] = 0;
            result = std::string(buf);
        }
    }
    return result;
}
std::string EndDevice::show_battery_voltage()
{
    if (battery_voltage_ > 0)
    {
        char buff[16]{0};
        size_t len = snprintf(buff, 16, "%0.2fV", battery_voltage_);
        buff[len] = 0;
        return std::string(buff);
    }
    return "";
}

std::string EndDevice::show_mains_voltage()
{
    if (mains_voltage_ > 0)
    {
        char buff[16]{0};
        size_t len = snprintf(buff, 16, "%0.2fV", mains_voltage_);
        buff[len] = 0;
        return std::string(buff);
    }
    return "";
}

std::string EndDevice::show_current()
{
    if (current_ < 0)
        return "";
    char buff[16]{0};
    size_t len = snprintf(buff, 16, "%0.3fA", current_);
    buff[len] = 0;
    return std::string(buff);
}

void EndDevice::set_current_state(std::string state, uint8_t channel)
{
    if (channel == 1)
        state_ = state;
    else if (channel == 2)
        state2_ = state;
}

std::string EndDevice::get_current_state(uint8_t channel)
{
    std::string state = "";
    if (get_motion_state() != -1)
    {
        state = state + (motion_ ? "Motion " : "No motion ");
    }
    if (get_luminocity() != -1)
    {
        state = state + (luminocity_ ? "Light" : "Dark");
    }
    if (state.size() > 0)
        return state;
    if (channel == 2)
        state = state2_;
    else
        state = state_;
    if (IEEEAddress_ == 0x54ef441000193352)
    {
        // инвертируем On -> Off
        if (state.find("On") != std::string::npos)
            gsb_utils::replace_first(state, "On", "Off");
        else if (state.find("Off") != std::string::npos)
            gsb_utils::replace_first(state, "Off", "On");
    }

    return state;
}

// Установка времени (таймстамп) последнего ответа устройства
// В ответе функция фозвращает предыдущее значение
uint64_t EndDevice::set_last_seen(uint64_t timestamp)
{
    uint64_t ret = last_seen_;
    if (timestamp > 0)
        last_seen_ = timestamp;
    return ret;
}
// Установка времени (таймстамп) последней активности устройства
// В ответе функция фозвращает предыдущее значение
uint64_t EndDevice::set_last_action(uint64_t timestamp)
{
    uint64_t ret = last_action_;
    if (timestamp > 0)
        last_action_ = timestamp;
    if (last_action_ > last_seen_)
        last_seen_ = last_action_;
    return ret;
}

// what = 0 - last seen(default), 1 - last action
std::string EndDevice::show_last(int what)
{
    if ((what == 0 && last_seen_ < 1) || (what == 1 && last_action_ < 1))
        return "";

    char mbstr[100];
    std::time_t t1 = what == 0 ? last_seen_ : last_action_;
    std::tm *tm = std::localtime(&t1);

    if (std::strftime(mbstr, sizeof(mbstr), "%d.%m.%Y %H:%M:%S", tm))
    {
        std::string dt_string(mbstr);
        return dt_string;
    }

    return "";
}

/// @brief Возвращает строку для Prometheus
/// @return
std::string EndDevice::get_prom_motion_string()
{
    double state = (double)get_motion_state();
    if (state < 0)
        return "";
    return "zhub2_metrics{device=\"" + deviceInfo.engName + "\",type=\"motion\"} " + std::to_string(state) + "\n";
}
/// @brief Возвращает строку для Prometheus
/// @return
std::string EndDevice::get_prom_door_string()
{
    std::string state = get_current_state();
    std::string strState = "";
    if (state == "Opened")
        strState = "0.95";
    else if (state == "Closed")
        strState = "0.05";
    else
        return "";
    return "zhub2_metrics{device=\"" + deviceInfo.engName + "\",type=\"door\"} " + strState + "\n";
}
/// @brief Возвращает строку для Prometheus
/// Чтобы линия реле не сливалась с линией датчика,
/// искуственно отступаю на 0,1
/// @return
std::string EndDevice::get_prom_relay_string()
{
    std::string state2 = "";
    std::string state = get_current_state(1);
    if (get_device_type() == 11)
        state2 = get_current_state(2);
    std::string strState = "";
    std::string strState2 = "";
    if (state == "On")
        strState = "0.9";
    else if (state == "Off")
        strState = "0.1";
    if (state2 == "On")
        strState2 = "0.95";
    else if (state2 == "Off")
        strState2 = "0.15";
    std::string name1 = gsbutils::SString::remove_after(deviceInfo.engName, "/");
    std::string name2 = gsbutils::SString::remove_before(deviceInfo.engName, "/");
    strState = "zhub2_metrics{device=\"" + name1 + "\",type=\"relay\"} " + strState + "\n";
    if (strState2.size())
        strState2 = "zhub2_metrics{device=\"" + name2 + "\",type=\"relay\"} " + strState2 + "\n";
    return strState + strState2;
}
/// @brief Возвращает строку для Prometheus с давлением
/// @return
std::string EndDevice::get_prom_pressure()
{
    double pressure = get_pressure();
    if (pressure > 0)
        return "zhub2_metrics{device=\"" + deviceInfo.engName + "\",type=\"pressure\"} " + std::to_string(pressure) + "\n";
    else
        return "";
}

void EndDevice::set_linkquality(uint8_t lq)
{
    linkquality_ = lq;
    // датчик климата не отдает никакой статус, принудительно ставлю, что он живой
    if ((uint64_t)IEEEAddress_ == 0x124b000b1bb401)
        state_ = "On";
}

// Запрашиваем питание каждый час для устройств, не отдающих этот параметр в репорте
bool EndDevice::check_last_power_query()
{
    std::time_t ts = std::time(0); // get time now
    if (lastPowerQuery == 0)
    {
        lastPowerQuery = ts;
        return true;
    }

    if (ts - 3600 > lastPowerQuery)
    {
        lastPowerQuery = ts;
        return true;
    }
    return false;
}