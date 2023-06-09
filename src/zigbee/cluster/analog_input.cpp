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
//#include "../../main.h"

using AnalogInput = zigbee::clusters::AnalogInput;

void AnalogInput::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef DEBUG
    int dbg = 1;
#else
    int dbg = 4;
#endif

    double value = -100.0;
    std::string unit = "";
    for (auto attribute : attributes)
    {

        switch (attribute.id)
        {
        case 0x0055:
        {
            //  на реле показывает потребляемую мощность в Вт
            // отдается в моменты включения/выключения
            value = (double)(any_cast<float>(attribute.value));
            gsbutils::dprintf(dbg, "ANALOG_INPUT Device 0x%04x endpoint %d  Value =  %f \n", endpoint.address, endpoint.number, value);
        }
        break;
        case 0x006f:
        {
        }
        break;
        case 0x001c:
        {
            // единица измерения(для двухканального реле не приходит)
            unit = (any_cast<std::string>(attribute.value));
            unit = gsbutils::SString::remove_after(unit, ",");
#ifdef DEBUG
            gsbutils::dprintf(1, "ANALOG_INPUT Device 0x%04x endpoint %d  Unit =  %s \n", endpoint.address, endpoint.number, unit.c_str());
#endif
        }
        break;
        default:
            gsbutils::dprintf(1, "ANALOG_INPUT Zhub::on_attribute_report: unknown attribute Id 0x%04x  device: 0x%04x\n", attribute.id, endpoint.address);
        }
    }
    if (unit.size() && value > -100.0)
    {
        if (unit == "%")
            ed->set_humidity(value);
        else if (unit == "C")
            ed->set_temperature(value);
        else if (unit == "V")
        {
            if (ed->deviceInfo.powerSource == zigbee::zcl::Attributes::PowerSource::BATTERY)
                ed->set_battery_params(0, value);
            else
                ed->set_mains_voltage(value);
        }
        else if (unit == "Pa")
            ed->set_pressure(value);
        else
            gsbutils::dprintf(1, "ANALOG_INPUT Device 0x%04x endpoint %d Unit =  %s \n", endpoint.address, endpoint.number, unit.c_str());
        value = -100.0;
        unit = "";
    }
    else if ((ed->get_device_type() == 11 || ed->get_device_type() == 9 || ed->get_device_type() == 10) && (value > -100.0))
    {
#ifdef DEBUG
        gsbutils::dprintf(1, "ANALOG_INPUT Device 0x%04x endpoint %d Power %0.3fW \n", endpoint.address, endpoint.number, value);
#endif
    }
}