#ifndef ZCL_H
#define ZCL_H

namespace zcl
{
    enum class Cluster : uint16_t
    {
        BASIC = 0x0000,
        POWER_CONFIGURATION = 0x0001,
        DEVICE_TEMPERATURE_CONFIGURATION = 0x0002,
        IDENTIFY = 0x0003,
        GROUPS = 0x0004,
        SCENES = 0x0005,
        ON_OFF = 0x0006,
        ON_OFF_SWITCH_CONFIGURATION = 0x0007,
        LEVEL_CONTROL = 0x0008,
        ALARMS = 0x0009,
        TIME = 0x000a,          // Attributes and commands that provide an interface to a real-time clock. (С реле aquara идет каждую минуту с одним и тем же значением)
        RSSI = 0x000b, // Attributes and commands for exchanging location information and channel parameters among devices, and (optionally) reporting data to a centralized device that collects data from devices in the network and calculates their positions from the set of collected data.
        ANALOG_INPUT = 0x000c,  // у реле от Aquara похоже на передачу потребляемой мощности, значения идут только при включенной нагрузке, чередуются значение и нуль. При выключенной нагрузке ничего не передается.
                                // передается на endpoint=15 По значению похоже на показатель потребляемой мощности.
        ANALOG_OUTPUT = 0x000d,
        ANALOG_VALUE = 0x000e,
        BINARY_INPUT = 0x000f,
        BINARY_OUTPUT = 0x0010,
        BINARY_VALUE = 0x0011,
        MULTISTATE_INPUT = 0x0012,
        MULTISTATE_OUTPUT = 0x0013,
        MULTISTATE_VALUE = 0x0014,
        OTA = 0x0019,
        POWER_PROFILE = 0x001a,          // Attributes and commands that provide an interface to the power profile of a device
        PULSE_WIDTH_MODULATION = 0x001c, //
        POLL_CONTROL = 0x0020,           // Attributes and commands that provide an interface to control the polling of sleeping end device
        XIAOMI_SWITCH_OUTPUT = 0x0021,   // выходной кластер на реле aqara
        KEEP_ALIVE = 0x0025,
        WINDOW_COVERING = 0x0102,
        TEMPERATURE_MEASUREMENT = 0x0402,
        HUMIDITY_MEASUREMENT = 0x0405,
        ILLUMINANCE_MEASUREMENT = 0x0400,
        IAS_ZONE = 0x0500,
        SIMPLE_METERING = 0x0702,         // умная розетка
        METER_IDENTIFICATION = 0x0b01,    // Attributes and commands that provide an interface to meter identification
        ELECTRICAL_MEASUREMENTS = 0x0b04, //
        DIAGNOSTICS = 0x0b05,             // Attributes and commands that provide an interface to diagnostics of the ZigBee stack
        TOUCHLINK_COMISSIONING = 0x1000,  // Для устройств со светом, в другом варианте LIGHT_LINK
        TUYA_SWITCH_MODE_0 = 0xe000, // кран
        TUYA_ELECTRICIAN_PRIVATE_CLUSTER = 0xe001,        // имеется у умной розетки и крана Voltage - ??
        IKEA_BUTTON_ATTR_UNKNOWN2 = 0xfc7c, // Имеется у кнопки IKEA
        XIAOMI_SWITCH = 0xfcc0,             // проприетарный кластер (Lumi) на реле Aquara, присутствует код производителя и длинная строка payload
        SMOKE_SENSOR = 0xfe00,              // Датчик дыма, TUYA-совместимый
        UNKNOWN_CLUSTER = 0xffff
    };

    enum class FrameType : uint8_t
    {
        GLOBAL = 0,
        SPECIFIC = 1

    };
    enum class FrameDirection : uint8_t
    {
        FROM_CLIENT_TO_SERVER = 0,
        FROM_SERVER_TO_CLIENT = 8

    };
    // Table 2-12 ZCL Specification
    enum Status : uint8_t
    {
        SUCCESS = 0x00,
        FAILURE = 0x01,
        NOT_AUTHORIZED = 0x7e,
        RESERVED_FIELD_NOT_ZERO = 0x7f,
        MALFORMED_COMMAND = 0x80,
        UNSUP_COMMAND = 0x81,
        UNSUPPORTED_GENERAL_COMMAND = 0x82,       // deprecated
        UNSUPPORTED_MANUF_CLUSTER_COMMAND = 0x83, // deprecated
        UNSUPPORTED_MANUF_GENERAL_COMMAND = 0x84, // deprecated
        INVALID_FIELD = 0x85,
        UNSUPPORTED_ATTRIBUTE = 0x86,
        INVALID_VALUE = 0x87,
        READ_ONLY = 0x88,
        INSUFFICIENT_SPACE = 0x89,
        DUPLICATE_EXISTS = 0x8a, // deprecated
        NOT_FOUND = 0x8b,
        UNREPORTABLE_ATTRIBUTE = 0x8c,
        INVALID_DATA_TYPE = 0x8d,
        INVALID_SELECTOR = 0x8e,
        WRITE_ONLY = 0x8f,                 // deprecated
        INCONSISTENT_STARTUP_STATE = 0x90, // deprecated
        DEFINED_OUT_OF_BAND = 0x91,        // deprecated
        RESERVED = 0x92,
        ACTION_DENIED = 0x93, // deprecated
        TIMEOUT = 0x94,
        ABORT = 0x95,
        INVALID_IMAGE = 0x96,
        WAIT_FOR_DATA = 0x97,
        NO_IMAGE_AVAILABLE = 0x98,
        REQUIRE_MORE_IMAGE = 0x99,
        NOTIFICATION_PENDING = 0x9a,
        RESERVED_2 = 0xc2,
        UNSUPPORTED_CLUSTER = 0xc3
    };

    struct Frame
    {
        struct
        {
            FrameType type;
            bool manufacturer_specific;
            FrameDirection direction;
            bool disable_default_response;
        } frame_control;

        uint16_t manufacturer_code;
        uint8_t transaction_sequence_number;
        uint8_t command;

        std::vector<uint8_t> payload;
    };

    // Table 2-3 ZCL Specification
    enum GlobalCommands : uint8_t
    {
        READ_ATTRIBUTES = 0x00,
        READ_ATTRIBUTES_RESPONSE = 0x01,
        WRITE_ATTRIBUTES = 0x02,
        WRITE_ATTRIBUTES_UNDIVIDED = 0x03,
        WRITE_ATTRIBUTES_RESPONSE = 0x04,
        WRITE_ATTRIBUTES_NO_RESPONSE = 0x05,
        CONFIGURE_REPORTING = 0x06,
        CONFIGURE_REPORTING_RESPONSE = 0x07,
        READ_REPORTING_CONFIGURATION = 0x08,
        READ_REPORTING_CONFIGURATION_RESPONSE = 0x0,
        REPORT_ATTRIBUTES = 0x0a,
        DEFAULT_RESPONSE = 0x0b,
        DISCOVER_ATTRIBUTES = 0x0c,
        DISCOVER_ATTRIBUTES_RESPONSE = 0x0d,
        READ_ATTRIBUTES_STRUCTURED = 0x0e,
        WRITE_ATTRIBUTES_STRUCTURED = 0x0f,
        WRITE_ATTRIBUTES_STRUCTURED_RESPONSE = 0x10,
        DISCOVER_COMMANDS_RECEIVED = 0x11,
        DISCOVER_COMMANDS_RECEIVED_RESPONSE = 0x12,
        DISCOVER_COMMANDS_GENERATED = 0x13,
        DISCOVER_COMMANDS_GENERATED_RESPONSE = 0x14,
        DISCOVER_ATTRIBUTES_EXTENDED = 0x15,
        DISCOVER_ATTRIBUTES_EXTENDED_RESPONSE = 0x16
    };
    // Table 3-32. Received Command IDs for the Identify Cluster
    enum IdentifiCommands : uint8_t
    {
        IDENTIFY = 0x00,       // Identify M
        IDENTIFY_QUERY = 0x01, // Identify Query M
        TRIGGER_EFFECT = 0x40  // Trigger effect O
    };
}

#endif
