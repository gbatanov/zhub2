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
#include "../zigbee.h"
#include "cluster.h"
#include "../../modem.h"
#include "../../main.h"

using SimpleMetering = zigbee::clusters::SimpleMetering;


void SimpleMetering::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef DEBUG
    int dbg = 1;
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
            gsbutils::dprintf(dbg, "SimpleMetering Device 0x%04x, attribute.id = 0x0000, value=0x%" PRIx64 "\n", endpoint.address, val);
        }
        break;
        default:
            gsbutils::dprintf(dbg, "SimpleMetering Device 0x%04x, attribute.id = 0x%04x\n", endpoint.address, attribute.id);
        }
    }
#endif
}
