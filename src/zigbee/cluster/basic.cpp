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
#include "../../../gsb_utils/gsbutils.h"
#include "../../../telebot32/src/tlg32.h"
#include "../../version.h"
#include "../../comport/unix.h"
#include "../../comport/serial.h"
#include "../../common.h"

#include "../zigbee.h"
#include "cluster.h"
#include "../../modem.h"
#include "../../app.h"
using Basic =  zigbee::clusters::Basic;

extern App app;

void Basic::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
    int dbg = 4;
    for (auto attribute : attributes)
    {
        if (attribute.id != 0x0001)
            gsbutils::dprintf(dbg, "Cluster::BASIC device 0x%04x attribute Id 0x%04x  \n", endpoint.address, attribute.id);

        try
        {
            switch (attribute.id)
            {
            case zigbee::zcl::Attributes::Basic::MANUFACTURER_NAME:
            {
                std::string identifier = any_cast<std::string>(attribute.value);
                if (!identifier.empty())
                {
                    ed->setManufacturer(identifier);
                    gsbutils::dprintf(dbg, "MANUFACTURER_NAME: 0x%02x %s \n\n", endpoint.address, identifier.c_str());
                }
                break;
            }
            case zigbee::zcl::Attributes::Basic::MODEL_IDENTIFIER:
            {
                std::string identifier = any_cast<std::string>(attribute.value);
                if (!identifier.empty())
                {
                    ed->setModelIdentifier(identifier);
                    gsbutils::dprintf(dbg, "MODEL_IDENTIFIER: 0x%02x %s \n\n", endpoint.address, identifier.c_str());
                }
                break;
            }
            case zigbee::zcl::Attributes::Basic::PRODUCT_CODE:
            {
                std::string identifier = any_cast<std::string>(attribute.value);
                if (!identifier.empty())
                {
                    ed->setProductCode(identifier);
                    gsbutils::dprintf(dbg, "PRODUCT_CODE: 0x%02x %s \n\n", endpoint.address, identifier.c_str());
                }
            }
            break;
            case zigbee::zcl::Attributes::Basic::PRODUCT_LABEL:
            {
                gsbutils::dprintf(dbg, "PRODUCT_LABEL: 0x%02x %s \n\n", endpoint.address, (any_cast<std::string>(attribute.value)).c_str());
                break;
            }
            case zigbee::zcl::Attributes::Basic::SW_BUILD_ID:
            {
                gsbutils::dprintf(dbg, "SW_BUILD_ID: 0x%02x %s \n\n", endpoint.address, (any_cast<std::string>(attribute.value)).c_str());
                break;
            }
            case zigbee::zcl::Attributes::Basic::POWER_SOURCE:
            {
                uint8_t val = any_cast<uint8_t>(attribute.value);
                gsbutils::dprintf(dbg, "Device 0x%04x POWER_SOURCE: %d \n", endpoint.address, val);

                if (ed && val > 0 && val < 0x8f)
                {
                    ed->setPowerSource(val);
                }

                break;
            }
            case zigbee::zcl::Attributes::Basic::ZCL_VERSION:
            {
                gsbutils::dprintf(7, "Cluster BASIC::ZCL version:\n endpoint.address: 0x%04x, \n attribute.value: %d \n\n", endpoint.address, (any_cast<uint8_t>(attribute.value)));

                break;
            }
            case zigbee::zcl::Attributes::Basic::APPLICATION_VERSION:
            {
                // с умных розеток идет примерно каждые 300 секунд
                if (ed->get_device_type() == 10) // умные розетки
                {
                    zigbee::NetworkAddress shortAddr = ed->getNetworkAddress();
                    // запрос тока и напряжения, работает!
                    std::vector<uint16_t> idsAV{0x0505, 0x508};
                    app.zhub->read_attribute(shortAddr, zigbee::zcl::Cluster::ELECTRICAL_MEASUREMENTS, idsAV);

                    if (ed->get_current_state(1) != "On" && ed->get_current_state(1) != "Off")
                    {
                        std::vector<uint16_t> idsAV{0x0000};
                        app.zhub->read_attribute(shortAddr, zigbee::zcl::Cluster::ON_OFF, idsAV);
                    }
                }

                gsbutils::dprintf(7, "Cluster BASIC:: APPLICATION_VERSION:\n endpoint.address: 0x%04x, \n attribute.value: %d \n\n", endpoint.address, (any_cast<uint8_t>(attribute.value)));

                break;
            }

            case zigbee::zcl::Attributes::Basic::GENERIC_DEVICE_TYPE:
            {
                gsbutils::dprintf(dbg, "Cluster BASIC:: GENERIC_DEVICE_TYPE:\n endpoint.address: 0x%04x, \n attribute.value: 0x%02x \n\n", endpoint.address, any_cast<uint8_t>(attribute.value));

                break;
            }
            case zigbee::zcl::Attributes::Basic::GENERIC_DEVICE_CLASS:
            {
                gsbutils::dprintf(dbg, "Cluster BASIC:: GENERIC_DEVICE_CLASS:\n endpoint.address: 0x%04x, \n attribute.value: 0x%02x \n\n", endpoint.address, any_cast<uint8_t>(attribute.value));

                break;
            }
            case zigbee::zcl::Attributes::Basic::PRODUCT_URL:
            {
                gsbutils::dprintf(dbg, "Cluster BASIC:: GENERIC_DEVICE_CLASS:\n endpoint.address: 0x%04x, \n attribute.value: 0x%02x \n\n", endpoint.address, (any_cast<std::string>(attribute.value).c_str()));
            }
            break;
            case zigbee::zcl::Attributes::Basic::FF01:
            {
                // датчик протечек Xiaomi. Двухканальное реле Aqara.
                // Строка с набором аттрибутов. На входе std::string
#ifdef DEBUG
                gsbutils::dprintf(dbg, "Cluster BASIC:: Device 0x%04x Cluster::BASIC Attribute Id FF01\n", endpoint.address);
#endif
                std::vector<uint8_t> input{};
                for (uint8_t b : any_cast<std::string>(attribute.value))
                {
                    input.push_back((uint8_t)b);
                    gsbutils::dprintf_c(dbg, " %02x  ", b);
                }
                gsbutils::dprintf_c(dbg, " \n");
                // датчик протечек
                // 0x01 21 d1 0b // battery 3.025
                // 0x03 28 1e // температура 29 град
                // 0x04 21 a8 43  // no description
                // 0x05 21 08 00  // RSSI 128 dB UINT16
                // 0x06 24 01 00 00 00 00  // ?? UINT40
                // 08 21 06 02 // no  description
                // 09 21 00 04 // no  description
                // 0a 21 00 00  // parent NWK - zhub (0000)
                // 64 10 00    // false - OFF
                // двухканальное реле
                // 0x03   0x28   0x1e //int8
                // 0x05   0x21   0x05   0x00
                // 0x08   0x21   0x2f   0x13
                // 0x09   0x21   0x01   0x02
                // 0x64   0x10   0x00 // bool
                // 0x65   0x10   0x00
                // 0x6e   0x20   0x00
                // 0x6f   0x20   0x00
                // 0x94   0x20   0x02  // uint8
                // 0x95   0x39   0x0a   0xd7   0xa3   0x39   // float
                // 0x96   0x39   0x58   0x48   0x0a   0x45
                // 0x97   0x39   0x00   0x30   0x68   0x3b
                // 0x98   0x39   0x00   0x00   0x00   0x00
                // 0x9b   0x21   0x00   0x00
                // 0x9c   0x20   0x01
                // 0x0a   0x21   0x00   0x00 //uint16
                // 0x0c   0x28   0x00   0x00
                for (unsigned i = 0; i < input.size(); i++)
                {
                    switch (input.at(i))
                    {
                    case 0x01: // напряжение батарейки
                    {
                        float bat = (float)_UINT16(input.at(i + 2), input.at(i + 3));
                        i = i + 3;
                        ed->set_battery_params(0, bat / 1000);
                    }
                    break;
                    case 0x03: // температура
                    {
                        i = i + 2;
                        ed->set_temperature((double)input.at(i));
                    }
                    break;
                    case 0x04:
                        i = i + 3;
                        break;
                    case 0x05: // RSSI  val - 90
                    {
                        int16_t rssi = (int16_t)(_UINT16(input.at(i + 2), input.at(i + 3)) - 90);
                        gsbutils::dprintf(dbg, "device 0x%04x RSSI:  %d dBm \n", endpoint.address, rssi);
                        i = i + 3;
                    }
                    break;
                    case 0x06: // ?
                    {
                        uint64_t lqi = _UINT64(input.at(i + 2), input.at(i + 3), input.at(i + 4), input.at(i + 5), input.at(i + 6), 0, 0, 0);
                        gsbutils::dprintf(dbg, "0x06: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", input.at(i + 2), input.at(i + 3), input.at(i + 4), input.at(i + 5), input.at(i + 6));
                        i = i + 6;
                    }
                    break;
                    case 0x08:
                    case 0x09:
                        i = i + 3;
                        break;

                    case 0x0a: // nwk
                    {
                        i = i + 3;
                    }
                    break;
                    case 0x0c:
                        i = i + 2;
                        break;
                    case 0x64: // состояние устройства, канал 1
                    {
                        i = i + 2;
                        ed->set_current_state(input.at(i) == 1 ? "On" : "Off");
                    }
                    break;
                    case 0x65: // состояние устройства, канал 2
                    {
                        i = i + 2;
                        ed->set_current_state((input.at(i) == 1 ? "On" : "Off"), 2);
                    }
                    break;
                    case 0x6e: // uint8
                    case 0x6f:
                    case 0x94:
                    {
                        i = i + 2;
                    }
                    break;
                    case 0x95: // Потребленная мощность за период
                    case 0x98: // Мгновенная мощность
                    {
                        i = i + 5;
                    }
                    break;
                    case 0x96: // напряжение
                    {
                        union
                        {
                            int num;
                            float fnum;
                        } my_union;
                        my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);
                        ed->setPowerSource(0x01);
                        ed->set_mains_voltage(my_union.fnum / 10);
                        gsbutils::dprintf(dbg, "Voltage  %0.2f\n", my_union.fnum / 10);
                        i = i + 5;
                    }
                    break;
                    case 0x97: // ток
                    {
                        union
                        {
                            int num;
                            float fnum;
                        } my_union;
                        my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);
                        double val = my_union.fnum / 1000;
                        double val11 = my_union.fnum;
                        ed->set_current(val11);
                        gsbutils::dprintf(dbg, "Ток: %0.3f %0.3f\n", val, val11);
                        i = i + 5;
                    }
                    break;
                    case 0x9b:
                    {
                        i = i + 3;
                    }
                    break;
                    case 0x9c:
                    {
                        i = i + 2;
                    }
                    break;
                    } // switch
                    if (i >= input.size())
                        break;
                } // for
            }
            break;

            case zigbee::zcl::Attributes::Basic::FFE2:
            case zigbee::zcl::Attributes::Basic::FFE4:
            {
                gsbutils::dprintf(dbg, "Cluster BASIC:: Attribute 0x%04x endpoint.address: 0x%02x, attribute.value: %d \n\n", attribute.id, endpoint.address, (any_cast<uint8_t>(attribute.value)));
            }
            break;

            case zigbee::zcl::Attributes::Basic::LOCATION_DESCRIPTION: // 0x0010
            case zigbee::zcl::Attributes::Basic::DATA_CODE:            // 0x0006
            {
                std::string val = any_cast<std::string>(attribute.value);
                gsbutils::dprintf(dbg, "Cluster BASIC:: Attribute 0x%04x endpoint.address: 0x%02x, attribute.value: %s \n\n", attribute.id, endpoint.address, val.c_str());
            }
            break;
            case zigbee::zcl::Attributes::Basic::STACK_VERSION: // 0x0002,
            case zigbee::zcl::Attributes::Basic::HW_VERSION:    // 0x0003,
            {
                gsbutils::dprintf(dbg, "Cluster BASIC::: Attribute 0x%04x endpoint.address: 0x%02x, attribute.value: %d \n\n", attribute.id, endpoint.address, (any_cast<uint8_t>(attribute.value)));
            }
            break;
            case zigbee::zcl::Attributes::Basic::PHYSICAL_ENVIRONMENT: // 0x0011, enum8
            {
#ifdef DEBUG
                uint8_t val = any_cast<uint8_t>(attribute.value);
                gsbutils::dprintf(1, "PHYSICAL_ENVIRONMENT: 0x%04x 0x%02x \n\n", endpoint.address, val);
#endif
            }
            break;
            case zigbee::zcl::Attributes::Basic::DEVICE_ENABLED: // 0x0012, uint8
            {
#ifdef DEBUG
                uint8_t val = any_cast<uint8_t>(attribute.value);
                gsbutils::dprintf(1, "DEVICE_ENABLED: 0x%04x %s \n\n", endpoint.address, val ? "Enabled" : "Disabled");
#endif
            }
            break;
            case zigbee::zcl::Attributes::Basic::ALARM_MASK: //  0x0013, map8
            {
                gsbutils::dprintf(1, "ALARM_MASK 0x%04x , attribute.value: %d \n\n", endpoint.address, (any_cast<uint8_t>(attribute.value)));
            }
            break;
            case zigbee::zcl::Attributes::Basic::DISABLE_LOCAL_CONFIG: // 0x0014, map8
            {
                gsbutils::dprintf(1, "DISABLE_LOCAL_CONFIG 0x%04x: 0x%02x \n", endpoint.address, (any_cast<uint8_t>(attribute.value)));
            }
            break;
            case zigbee::zcl::Attributes::Basic::GLOBAL_CLUSTER_REVISION: // 0xFFFD
            {
                gsbutils::dprintf(1, "GLOBAL_CLUSTER_REVISION 0x%04x: 0x%02x \n", endpoint.address, (any_cast<uint8_t>(attribute.value)));
            }
            break;
            default:
            {
                gsbutils::dprintf(1, " Cluster::BASIC: endpoint %d - unknown attribute 0x%04x \n",  endpoint.number, attribute.id);
            }
            }
        }
        catch (std::exception &e)
        {
            gsbutils::dprintf(1, " Cluster::BASIC: adddress 0x%04x endpoint %d - error attribute 0x%04x \n", endpoint.address, endpoint.number, attribute.id);
        }
    } // for attribute
}