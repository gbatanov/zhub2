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

using Other = zigbee::clusters::Other;

extern zigbee::Zhub *zhub;

// Известные, но неподдерживаемые в этой программе кластеры
void Other::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef TEST
    int dbg = 1;
#else
    int dbg = 3;
#endif
    gsbutils::dprintf(dbg, "Cluster:: 0x%04x Device 0x%04x\n",cluster, endpoint.address);

    for (auto attribute : attributes)
    {
        gsbutils::dprintf(dbg, "Other: attribute Id 0x%04x in Cluster::0x%04x \n", attribute.id,cluster);
    }
}
