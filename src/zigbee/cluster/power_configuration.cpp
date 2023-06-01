#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <syslog.h>
#include <array>
#include <memory>
#include <set>
#include <inttypes.h>
#include <optional>
#include <any>
#include <sstream>
#include <termios.h>

#include "../../version.h"
#include "../../comport/unix.h"
#include "../../comport/serial.h"
#include "../../../gsb_utils/gsbutils.h"
#include "../../common.h"
#include "../../main.h"
#include "../zigbee.h"
#include "cluster.h"

using PowerConfiguration = zigbee::clusters::PowerConfiguration;

extern zigbee::Zhub *zhub;

void PowerConfiguration::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef TEST
    int dbg = 1;
#else
    int dbg = 3;
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