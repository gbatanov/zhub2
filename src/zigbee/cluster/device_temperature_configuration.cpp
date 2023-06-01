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

using DeviceTemperatureConfiguration = zigbee::clusters::DeviceTemperatureConfiguration;

extern zigbee::Zhub *zhub;

void DeviceTemperatureConfiguration::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef TEST
    int dbg = 1;
#else
    int dbg = 3;
#endif

    // Есть в реле Aqara
    for (auto attribute : attributes)
    {
        int16_t val = static_cast<uint8_t>(any_cast<int16_t>(attribute.value));
        ed->set_temperature(static_cast<double>(val));
        if (val > 60)
            zhub->height_temperature(endpoint.address);
        gsbutils::dprintf(dbg, "Device 0x%04x temperature =  %d grad Celsius\n", endpoint.address, val);
    }
}
