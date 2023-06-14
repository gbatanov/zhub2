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
#include "../../../telebot32/src/tlg32.h"
#include "../../common.h"
#include "../zigbee.h"
#include "cluster.h"
#include "../../modem.h"
#include "../../app.h"

using DeviceTemperatureConfiguration = zigbee::clusters::DeviceTemperatureConfiguration;

extern App app;

void DeviceTemperatureConfiguration::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef DEBUG
    int dbg = 1;
#else
    int dbg = 4;
#endif

    // Есть в реле Aqara
    for (auto attribute : attributes)
    {
        int16_t val = static_cast<uint8_t>(any_cast<int16_t>(attribute.value));
        ed->set_temperature(static_cast<double>(val));
        if (val > 60)
            app.zhub->height_temperature(endpoint.address);
        gsbutils::dprintf(dbg, "Device 0x%04x endpoint %d temperature =  %d grad Celsius\n", endpoint.address, endpoint.number, val);
    }
}
