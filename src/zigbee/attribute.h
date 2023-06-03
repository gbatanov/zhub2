#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

namespace zcl
{

    template <typename T>
    std::vector<uint8_t> to_uint8_vector(T value)
    {
        return std::vector<uint8_t>(reinterpret_cast<uint8_t *>(&value), reinterpret_cast<uint8_t *>(&value) + sizeof(T));
    }
    template <typename T>
    std::vector<uint8_t> convertAttributeValue(std::any value)
    {
        return to_uint8_vector<T>(std::any_cast<T>(value));
    }
    template <typename T>
    T convertAttributeData(uint8_t *data)
    {
        return *reinterpret_cast<T *>(data);
    }

    struct Attribute
    {
        uint16_t id = 0;
        std::any value;

        // Table 2-11 ZCL Specification
        enum class DataType
        {
            NODATA = 0x00,
            DATA8 = 0x08,
            DATA16 = 0x09,
            DATA24 = 0x0a,
            DATA32 = 0x0b,
            DATA40 = 0x0c,
            DATA48 = 0x0d,
            DATA56 = 0x0e,
            DATA64 = 0x0f,
            BOOLEAN = 0x10,
            BITMAP8 = 0x18,
            BITMAP16 = 0x19,
            BITMAP24 = 0x1a,
            BITMAP32 = 0x1b,
            BITMAP40 = 0x1c,
            BITMAP48 = 0x1d,
            BITMAP56 = 0x1e,
            BITMAP64 = 0x1f,
            UINT8 = 0x20,
            UINT16 = 0x21,
            UINT24 = 0x22,
            UINT32 = 0x23,
            UINT40 = 0x24,
            UINT48 = 0x25,
            UINT56 = 0x26,
            UINT64 = 0x27,
            INT8 = 0x28,
            INT16 = 0x29,
            INT24 = 0x2a,
            INT32 = 0x2b,
            INT40 = 0x2c,
            INT48 = 0x2d,
            INT56 = 0x2e,
            INT64 = 0x2f,
            ENUM8 = 0x30,
            ENUM16 = 0x31,
            SEMI_FLOAT = 0x38,
            FLOAT = 0x39,
            DOUBLE = 0x3a,
            OCT_STRING = 0x41,
            CHARACTER_STRING = 0x42,
            LONG_OCT_STRING = 0x43,
            LONG_CHARACTER_STRING = 0x44,
            ARRAY = 0x48,
            STRUCTURE = 0x4c,
            SET = 0x50,
            BAG = 0x51,
            TIME_OF_DAY = 0xe0,
            DATE = 0xe1,
            UTC_TIME = 0xe2,
            CLUSTER_ID = 0xe8,
            ATTRIBUTE_ID = 0xe9,
            BACNET_OID = 0xea,
            IEEE_ADDRESS = 0xf0,
            SECURITY_KEY_128 = 0xf1,
            UNK = 0xff
        } data_type;

        //   std::vector<uint8_t> data();

        static std::vector<Attribute> parseAttributesPayload(std::vector<uint8_t> &payload, bool status_field);
    };

    namespace Attributes
    {
        // Table 3-7 ZCL Specification
        enum Basic : uint16_t
        {
            ZCL_VERSION = 0x0000,                  // Type: uint8, Range: 0x00 – 0xff, Access : Read Only, Default: 0x02.
            APPLICATION_VERSION = 0x0001,          // Type: uint8, Range: 0x00 – 0xff, Access : Read Only, Default: 0x00.
            STACK_VERSION = 0x0002,                // Type: uint8, Range: 0x00 – 0xff, Access : Read Only, Default: 0x00.
            HW_VERSION = 0x0003,                   // Type: uint8, Range: 0x00 – 0xff, Access : Read Only, Default: 0x00.
            MANUFACTURER_NAME = 0x0004,            // Type: string, Range: 0 – 32 bytes, Access : Read Only, Default: Empty string.
            MODEL_IDENTIFIER = 0x0005,             // Type: string, Range: 0 – 32 bytes, Access : Read Only, Default: Empty string.
            DATA_CODE = 0x0006,                    // Type: string, Range: 0 – 16 bytes, Access : Read Only, Default: Empty string.
            POWER_SOURCE = 0x0007,                 // Type: enum8, Range: 0x00 – 0xff, Access : Read Only, Default: 0x00.
            GENERIC_DEVICE_CLASS = 0x0008,         // Type: enum8, 0x00 - 0xff, RO, 0xff
            GENERIC_DEVICE_TYPE = 0x0009,          // enum8, 0x00 - 0xff, RO, 0xff
            PRODUCT_CODE = 0x000a,                 // octstr, RO, empty string
            PRODUCT_URL = 0x000b,                  // string, RO, empty string
            MANUFACTURER_VERSION_DETAILS = 0x000c, // string, RO, empty string
            SERIAL_NUMBER = 0x000d,                // string, RO, empty string
            PRODUCT_LABEL = 0x000e,                // string, RO, empty string
            LOCATION_DESCRIPTION = 0x0010,         // Type: string, Range: 0 – 16 bytes, Access : Read Write, Default: Empty string.
            PHYSICAL_ENVIRONMENT = 0x0011,         // Type: enum8, Range: 0x00 – 0xff, Access : Read Write, Default: 0x00.
            DEVICE_ENABLED = 0x0012,               // Type: uint8, Range: 0x00 – 0x01, Access : Read Write, Default: 0x01.
            ALARM_MASK = 0x0013,                   // Type: map8, Range: 000000xx, Access : Read Write, Default: 0x00.
            DISABLE_LOCAL_CONFIG = 0x0014,         // Type: map8, Range: 000000xx, Access : Read Write, Default: 0x00.
            SW_BUILD_ID = 0x4000,                  // Type: string, Range: 0 – 16 bytes, Access : Read Only, Default: Empty string.
            FF01 = 0xff01,                         // Type: string, Датчик протечек Xiaomi. Двухканальное реле Aqara. Строка с набором аттрибутов.
            FFE2 = 0xffe2,
            FFE4 = 0xffe4,
            GLOBAL_CLUSTER_REVISION = 0xfffd //
        };

        // Table 3-65 ZCL Specification
        enum Alarms : uint16_t
        {
            ALARM_COUNT = 0x0000 // Type: uint16, Range: 0x0000 - 0xffff, Access: ReadOnly default:0
        };
        enum PowerConfiguration : uint16_t
        {
            MAINS_VOLTAGE = 0x0000, // Type: uint16, Range: 0x0000 - 0xffff, Access: ReadOnly
            MAINS_SETTINGS = 0x0010,
            BATTERY_VOLTAGE = 0x0020, // uint8 0x00-0xff 0,1V step
            BATTERY_REMAIN = 0x0021,  // 0x00 - 0%  0x32 - 25% 0x64 -50%  0x96 - 75% 0xc8 -100%
            BATTERY_SIZE = 0x0031
        };

        enum OnOffSwitch : uint16_t
        {
            ON_OFF = 0x0000, // Type: bool, Range: 0x00 – 0x01, Access : Read Only, Default: 0x00.
            _00F5 = 0x00f5,
            _00F7 = 0x00f7,
            GLOBAL_SCENE_CONTROL = 0x4000, // Type: bool, Range: 0x00 – 0x01, Access : Read Only, Default: 0x01.
            ON_TIME = 0x4001,              // Type: uint16, Range: 0x0000 – 0xffff, Access : Read Write, Default: 0x0000.
            OFF_WAIT_TIME = 0x4002,        // Type: uint16, Range: 0x0000 – 0xffff, Access : Read Write, Default: 0x0000.
            _5000 = 0x5000,
            _8000 = 0x8000,
            _8001 = 0x8001,
            _8002 = 0x8002,
            F000 = 0xf000,
            F500 = 0xf500,
            F501 = 0xf501
        };

        enum IlluminanceMeasurement : uint16_t
        {
            ILLUM_MEASURED_VALUE = 0x0000,     // Type: uint16, Range: 0x0000 – 0xffff, Access : Read Only.
            ILLUM_MIN_MEASURED_VALUE = 0x0001, // Type: uint16, Range: 0x0000 – 0xffff, Access : Read Only.
            ILLUM_MAX_MEASURED_VALUE = 0x0002, // Type: uint16, Range: 0x0000 – 0xffff, Access : Read Only.
            ILLUM_TOLERANCE = 0x0003,          // Type: uint16, Range: 0x0000 – 0xffff, Access : Read Only.
            LIGHT_SENSOR_TYPE = 0x0004         // Type: enum8, Range: 0x00 – 0xff, Access : Read Only.
        };

        enum MultistateInput : uint16_t
        {
            PRESENT_VALUE = 0x0055, // Type: uint16, Range: 0x0000 – 0xffff, Access : Read Write.
        };

        enum TemperatureMeasurement : uint16_t
        {
            TEMP_MEASURED_VALUE = 0x0000,     // Type: int16, Range: 0x0000 – 0xffff, Access : Read Only.
            TEMP_MIN_MEASURED_VALUE = 0x0001, // Type: int16, Range: 0x954d – 0x7ffe, Access : Read Only.
            TEMP_MAX_MEASURED_VALUE = 0x0002, // Type: int16, Range: 0x954e – 0x7fff, Access : Read Only.
            TEMP_TOLERANCE = 0x0003,          // Type: uint16, Range: 0x0000 – 0x0800, Access : Read Only.
        };

        enum RelativeHumidityMeasurement : uint16_t
        {
            HUM_MEASURED_VALUE = 0x0000,     // Type: uint16, Range: 0x0000 – 0xffff, Access : Read Only.
            HUM_MIN_MEASURED_VALUE = 0x0001, // Type: uint16, Range: 0x954d – 0x7ffe, Access : Read Only.
            HUM_MAX_MEASURED_VALUE = 0x0002, // Type: uint16, Range: 0x954e – 0x7fff, Access : Read Only.
            HUM_TOLERANCE = 0x0003,          // Type: uint16, Range: 0x0000 – 0x0800, Access : Read Only.
        };

        enum AnalogInput : uint16_t
        {
            ANALOG_PRESENT_VALUE = 0x0055, // Type: float, Range: -, Access : Read Only.
        };

        // Table 3-8 ZCL Specification
        // Вариант с битом в 7 разряде означает наличие резервного питания в виде батареи
        enum PowerSource : uint8_t
        {
            UNKNOWN = 0x00,
            SINGLE_PHASE = 0x01,
            THREE_PHASE = 0x02,
            BATTERY = 0x03,
            DC = 0x04,
            EMERGENCY_CONSTANTLY = 0x05,
            EMERGENCY_MAINS_AND_TRANSFER_SWITCH = 0x06,
            UNKNOWN_PLUS = 0x80,
            SINGLE_PHASE_PLUS = 0x81,
            THREE_PHASE_PLUS = 0x82,
            BATTERY_PLUS = 0x83,
            DC_PLUS = 0x84,
            EMERGENCY_CONSTANTLY_PLUS = 0x85,
            EMERGENCY_MAINS_AND_TRANSFER_SWITCH_PLUS = 0x86

        };
        // Table 8-3. Attributes of the Zone Information Attribute Set
        enum ZoneInformation : uint16_t
        {
            ZONE_STATE = 0x0000, // ZoneState enum8 All R 0x00 M
            ZONE_TYPE = 0x0001,  // ZoneType enum16 All R - M
            ZONE_STATUS = 0x0002 // ZoneStatus  map16 All R 0x00 M
        };
        // Table 8-5. Values of the ZoneType Attribute
        enum ZoneTypeAttributes : uint16_t
        {
            STANDARD_CIE = 0x0000,       // system alarm |-
            MOTION_SENSOR = 0x000d,      // Intrusion indication | Presence indication
            CONTACT_SWITCH = 0x0015,     // 1st portal Open-Close | 2nd portal Open-Close
            DOOR_WINDOW_HANDLE = 0x0016, // Table 8-7
            FIRE_SENSOR = 0x0028,        // Fire indication | -
            WATER_SENSOR = 0x002a,       // Water overflow indication | -
            CO_SENSOR = 0x002b,          // CO indication | Cooking indication
            VIBRATION_SENSOR = 0x002d,   // Movement indication | Vibration
            REMOTE_CONTROL = 0x010f      // Panic | Emergency
            // неполный
        };
        // Table 8-6. Values of the ZoneStatus Attribute
        enum ZoneStatusAttributes : uint16_t
        { // TODO: bitmap
            ALARM1 = 0b0000000000000001,
            ALARM2 = 0b0000000000000010,
            TAMPER = 0b0000000000000100,
            BATTERY_LOW = 0b0000000000001000,
            SUPERVISION_NOTIFY = 0b0000000000010000, // посылается для уведомления о работоспособности
            RESTORE_NOTIFY = 0b0000000000100000,     // признак что событие было, но в данный момент отсутствует
            TROUBLE = 0b0000000001000000,
            AC_MAINS = 0b0000000010000000,
            IN_TEST_MODE = 0b0000000100000000,
            BATTERY_DEFECT = 0b0000001000000000,
        };

        enum XiaomiAttributes : uint16_t
        {
            POWER_OUTAGES = 0x0002,           // Unsigned 16-bit integer rw
            SWITCH_MODE = 0x0009,             // Unsigned 8-bit integer rw
            REPORTING_INTERVAL = 0x00f6,      // Reporting Interval rw
            SERIAL_NUMBER_X = 0x00fe,         // Character String rw
            LOCK_FOR_BUTTON = 0x0200,         // Unsigned 8-bit integer rw
            RESTORE_POWER_ON_OUTAGE = 0x0201, // Boolean rw
            AUTO_OFF = 0x0202,                // Boolean rw
            DEVICE_LED_OFF = 0x0203,          // Boolean rw
            MIN_POWER_CHANGE_REPORT = 0x0204, // single precision rw
            CONSUMER_CONNECTED = 0x0207,      // Boolean r
            MAX_LOAD_EXCEEDED = 0x020b,       // single precision rw

        };
        enum RSSI : uint16_t
        {
            QualityMeasure = 0x0003,
            Power = 0x0013
        };
    } // namespace Attributes

} // namespace zcl

#endif
