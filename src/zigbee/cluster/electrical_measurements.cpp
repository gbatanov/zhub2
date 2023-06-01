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

using ElectricalMeasurements = zigbee::clusters::ElectricalMeasurements;

extern zigbee::Zhub *zhub;

void ElectricalMeasurements::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef TEST
    int dbg = 1;
#else
    int dbg = 4;
#endif

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
            gsbutils::dprintf(dbg, "ElectricalMeasuremenrs Device 0x%04x,Voltage=%0.3fV, \n", endpoint.address, (double)val);
#endif
        }
        break;
        case 0x0508: // RMS Current mA
        {
            uint16_t val = any_cast<uint16_t>(attribute.value);
            ed->set_current((double)val / 1000);
#ifdef TEST
            gsbutils::dprintf(dbg, "ElectricalMeasuremenrs Device 0x%04x,Current=%0.3fA, \n", endpoint.address, (double)val / 1000);
#endif
        }
        break;
        default:
            gsbutils::dprintf(dbg, "ElectricalMeasuremenrs Device 0x%04x, attribute.id = 0x%04x\n", endpoint.address, attribute.id);
        }
    }
}
