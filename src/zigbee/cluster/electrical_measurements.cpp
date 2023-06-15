
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
#include "../../app.h"
extern std::shared_ptr<App> app;

using ElectricalMeasurements = zigbee::clusters::ElectricalMeasurements;

void ElectricalMeasurements::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef DEBUG
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
#ifdef DEBUG
            gsbutils::dprintf(dbg, "ElectricalMeasuremenrs Device 0x%04x,Voltage=%0.3fV, \n", endpoint.address, (double)val);
#endif
        }
        break;
        case 0x0508: // RMS Current mA
        {
            uint16_t val = any_cast<uint16_t>(attribute.value);
            ed->set_current((double)val / 1000);
            if (ed->get_ieee_address() == 0x70b3d52b6001b5d9)
                check_charger(val);
#ifdef DEBUG
            gsbutils::dprintf(dbg, "ElectricalMeasuremenrs Device 0x%04x,Current=%0.3fA, \n", endpoint.address, (double)val / 1000);
#endif
        }
        break;
        default:
            gsbutils::dprintf(dbg, "ElectricalMeasuremenrs Device 0x%04x, attribute.id = 0x%04x\n", endpoint.address, attribute.id);
        }
    }
}
/// val - milliampers
void ElectricalMeasurements::check_charger(uint16_t val)
{
    // если chargerOn == false и ток больше 20 мА, значит зарядник включен. Ставим признак chargerOn = true
    // если chargerOn == true и ток больше 20 мА, приодолжаем зарядку
    // если chargerOn == true и ток меньше 20 мА, выключаем розетку
    if (!chargerOn && val > 20)
        chargerOn = true;
    else if (chargerOn && val < 20)
        app->zhub->switch_relay(ed->get_ieee_address(), 0);
}