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

using Tuya = zigbee::clusters::Tuya;

extern zigbee::Zhub *zhub;

void Tuya::attribute_handler_private(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef TEST
    int dbg = 1;
#else
    int dbg = 3;
#endif
    // Кран: attributes    : ["valve": "", "healthStatus": "unknown", "powerSource": "dc"],
    for (auto attribute : attributes)
    {
        gsbutils::dprintf(dbg, "Zhub::on_attribute_report Device 0x%04x Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER , attribute.id = 0x%04x\n", endpoint.address, attribute.id);
#ifdef TEST
        switch (attribute.id)
        {
        case 0xd010: // PowerOnBehavior enum8 defaultValue: "2", options:  ['0': 'closed', '1': 'open', '2': 'last state']]
        {
            uint8_t val = any_cast<uint8_t>(attribute.value); // valve - 0,  smartplug - 2 кран при подаче питания закрывается, розетка остается в последнем состоянии
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER , attribute.id = 0xd010 value= 0x%02x\n", val);
        }
        break;
        case 0xd030: // switchType enum8 switchTypeOptions = [  '0': 'toggle',   '1': 'state',    '2': 'momentary'
        {
            uint8_t val = any_cast<uint8_t>(attribute.value); // 0
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER , attribute.id = 0xd030 value= 0x%02x\n", val);
        }
        break;
        default:
        {
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER , attribute.id = 0x%04x\n", attribute.id);
        }
        }
#endif
    }
}

void Tuya::attribute_handler_switch_mode(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef TEST
    int dbg = 1;
#else
    int dbg = 3;
#endif
    for (auto attribute : attributes)
    {
        gsbutils::dprintf(dbg, "Zhub::on_attribute_report Device 0x%04x Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", endpoint.address, attribute.id);
        switch (attribute.id)
        {
        case 0xd001: // array
        {
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", attribute.id);
        }
        break;
        case 0xd002: // array
        {
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", attribute.id);
        }
        break;
        case 0xd003: // CHARACTER_STRING AAAA
        {
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", attribute.id);
        }
        break;
        default:
        {
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report Cluster::TUYA_SWITCH_MODE_0 , attribute.id = 0x%04x\n", attribute.id);
        }
        }
    }
}
