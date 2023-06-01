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

using MultistateInput = zigbee::clusters::MultistateInput;

extern zigbee::Zhub *zhub;

void MultistateInput::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef TEST
    int dbg = 1;
#else
    int dbg = 4;
#endif

    double value = -100.0;
    std::string unit = "";

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
                gsbutils::dprintf(1, "Zhub::on_attribute_report: unknown attribute Id 0x%04x in cluster MULTISTATE_INPUT device: 0x%04x\n", attribute.id, endpoint.address);
            }
        }
        catch (std::bad_any_cast &bac)
        {
            gsbutils::dprintf(1, "Zhub::on_attribute_report: attribute Id 0x%04x in cluster MULTISTATE_INPUT device: 0x%04x bad any_cast\n", attribute.id, endpoint.address);
        }
    }
}
