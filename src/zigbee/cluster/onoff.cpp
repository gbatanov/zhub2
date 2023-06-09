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

using OnOff = zigbee::clusters::OnOff;

extern App app;

void OnOff::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef DEBUG
        int dbg = 1;
#else
        int dbg = 4;
#endif
    std::set<uint16_t> usedAttributes{};
    for (auto attribute : attributes)
    {
        if (usedAttributes.contains(attribute.id))
            continue;
        usedAttributes.insert(attribute.id);

        // уточнять по эндпойнтам
        switch (attribute.id)
        {
        case zigbee::zcl::Attributes::OnOffSwitch::ON_OFF: // 0x0000
        {
            bool b_val = false;
            uint8_t u_val = 0;

            try
            {
                b_val = (any_cast<bool>(attribute.value));
                gsbutils::dprintf(dbg, "Zhub::on_attribute_report: +Device 0x%04x endpoint %d Level %s \n", endpoint.address, endpoint.number, b_val ? "High" : "Low");
                uint64_t macAddress = (uint64_t)ed->getIEEEAddress();
                if (macAddress == 0x00124b0014db2724)
                {
                    // custom2 коридор
                    if (endpoint.number == 2) // датчик освещения
                    {
                        gsbutils::dprintf(dbg, "Zhub::on_attribute_report: Освещенность %s \n", b_val ? "высокая" : "низкая");
                        ed->set_luminocity(b_val ? 1 : 0);
                    }
                    if (endpoint.number == 6) // датчик движения (1 - нет движения, 0 - есть движение)
                    {
                        gsbutils::dprintf(dbg, "Прихожая: Движение %s \n", b_val ? "нет" : "есть");
                        app.zhub->handle_motion(ed, b_val ? 0 : 1);
                    }
                }
                else if (macAddress == 0x00124b0009451438)
                {
                    // custom3 датчик присутствия1 - кухня
                    if (endpoint.number == 2) // датчик присутствия
                    {
                        gsbutils::dprintf(dbg, "Zhub::on_attribute_report: Присутствие %s \n", b_val ? "нет" : "есть");
                        app.zhub->handle_motion(ed, b_val ? 0 : 1);
                    }
                }
                else if (macAddress == 0x0c4314fffe17d8a8)
                {
                    // датчик движения IKEA
                    gsbutils::dprintf(1, "Zhub::on_attribute_report:датчик движения IKEA %s \n", b_val ? "есть" : "нет");
                    app.zhub->handle_motion(ed, b_val ? 1 : 0);
                }
                else if (macAddress == 0x00124b0007246963)
                {
                    int dbg = 6;
                    // Устройство Custom3
                    if (endpoint.number == 2) // датчик освещения
                    {
                        gsbutils::dprintf(dbg, "Custom3: Освещенность %s \n", b_val ? "высокая" : "низкая");
                        ed->set_luminocity(b_val ? 1 : 0);
                    }
                    if (endpoint.number == 4) // датчик движения (с датчика приходит 1 - нет движения, 0 - есть движение)
                    {
                        gsbutils::dprintf(dbg, "Custom3: Движение %s \n", b_val ? "нет" : "есть");
                        app.zhub->handle_motion(ed, b_val ? 0 : 1);
                    }
                }
                else if (ed->get_device_type() == 10) // умные розетки
                {
                    std::string currentState = ed->get_current_state();
                    std::string newState = b_val ? "On" : "Off";
                    if (newState != currentState)
                    {
                        std::time_t ts = std::time(0); // get time now
                        ed->set_last_action((uint64_t)ts);
                        ed->set_current_state(newState);
                    }
                }
                else if (ed->get_device_type() == 11) //  у двухканальных реле есть для EP1 и EP2
                {
                    std::time_t ts = std::time(0); // get time now
                    ed->set_last_action((uint64_t)ts);
                    ed->set_current_state(b_val ? "On" : "Off", endpoint.number);
                    //                            gsbutils::dprintf(dbg, "Zhub::on_attribute_report: 1+Device 0x%04x endpoint %d Level %s \n", endpoint.address, endpoint.number, b_val ? "High" : "Low");
                }
                else
                {
                    std::time_t ts = std::time(0); // get time now
                    ed->set_last_action((uint64_t)ts);
                    ed->set_current_state(b_val ? "On" : "Off");
                    //                            gsbutils::dprintf(dbg, "Zhub::on_attribute_report: 2+Device 0x%04x endpoint %d Level %s \n", endpoint.address, endpoint.number, b_val ? "High" : "Low");
                }
            }
            catch (std::bad_any_cast &e)
            {
                gsbutils::dprintf(1, "Zhub::on_attribute_report:-Device 0x%04x endpoint %d Bad any cast \n", endpoint.address, endpoint.number);
            }
        }
        break;
        case zigbee::zcl::Attributes::OnOffSwitch::ON_TIME:
        {
            uint16_t val = static_cast<uint16_t>(any_cast<uint16_t>(attribute.value));
            gsbutils::dprintf(1, "Device 0x%04x endpoint %d ON_TIME =  %d s \n", endpoint.address, endpoint.number, val);
        }
        break;
        case zigbee::zcl::Attributes::OnOffSwitch::OFF_WAIT_TIME:
        {
            uint16_t val = static_cast<uint16_t>(any_cast<uint16_t>(attribute.value));
            gsbutils::dprintf(1, "Device 0x%04x endpoint %d OFF_WAIT_TIME =  %d s \n", endpoint.address, endpoint.number, val);
        }
        break;
        case zigbee::zcl::Attributes::OnOffSwitch::_00F5: // с реле aqara T1
        {
            //  каждые 30 секунд, практически идет постоянно только с реле света в коридоре
            // UINT32 0x070000nn или 0x03<short_addr>mm, nn крутится от 0 до ff
            uint32_t val = any_cast<uint32_t>(attribute.value);
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report: attribute Id 0x%04x in cluster ON_OFF Device 0x%04x val 0x%08x\n", attribute.id, endpoint.address, val);
        }
        break;
        case zigbee::zcl::Attributes::OnOffSwitch::F000: // двухканальное реле, очень походит на поведение реле в кластере _00F5
        case zigbee::zcl::Attributes::OnOffSwitch::F500: // с реле aqara T1
        case zigbee::zcl::Attributes::OnOffSwitch::F501: // с реле aqara T1
        {
            uint32_t val = any_cast<uint32_t>(attribute.value);
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report: attribute Id 0x%04x in cluster ON_OFF Device 0x%04x val 0x%08x\n", attribute.id, endpoint.address, val);
        }
        break;
        case zigbee::zcl::Attributes::OnOffSwitch::_00F7: // какие то наборы символов
        {
#ifdef DEBUG
            for (uint8_t b : any_cast<std::string>(attribute.value))
            {
                gsbutils::dprintf_c(dbg, " 0x%02x", b);
            }
            gsbutils::dprintf_c(dbg, "\n");
#endif
            std::string val = any_cast<std::string>(attribute.value);
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report: attribute Id 0x%04x in cluster ON_OFF Device 0x%04x value: \n", attribute.id, endpoint.address, val.c_str());
        }
        break;
        case zigbee::zcl::Attributes::OnOffSwitch::_5000:
        case zigbee::zcl::Attributes::OnOffSwitch::_8000:
        case zigbee::zcl::Attributes::OnOffSwitch::_8001:
        case zigbee::zcl::Attributes::OnOffSwitch::_8002:
        {
            // Краны
            gsbutils::dprintf(dbg, "Zhub::on_attribute_report: attribute Id 0x%04x in cluster ON_OFF device: 0x%04x\n", attribute.id, endpoint.address);
        }
        break;
        default:
            gsbutils::dprintf(1, "Zhub::on_attribute_report: unknown attribute Id 0x%04x in cluster ON_OFF device: 0x%04x\n", attribute.id, endpoint.address);
        }
    }
}
