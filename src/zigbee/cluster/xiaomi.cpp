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

using Xiaomi = zigbee::clusters::Xiaomi;

void Xiaomi::attribute_handler(std::vector<zigbee::zcl::Attribute> attributes, zigbee::Endpoint endpoint)
{
#ifdef DEBUG
    int dbg = 1;
#else
    int dbg = 3;
#endif
    // часть устройств Xiaomi отображает данные с устройств здесь :  реле aquara 
    // часть устройств Xiaomi в кластере Basic (0x0000): water leak sensor
#ifdef DEBUG
    gsbutils::dprintf(dbg, "Xiaomi Device 0x%04x Cluster::XIAOMI_SWITCH \n", endpoint.address);
#endif
    for (auto attribute : attributes)
    {

        switch (attribute.id)
        {
        case 0x00f7: // Строка с набором данных. На входе std::string
        {

            std::vector<uint8_t> input{};
            for (uint8_t b : any_cast<std::string>(attribute.value))
            {
                input.push_back((uint8_t)b);
                gsbutils::dprintf_c(dbg, "%02x ", b);
            }
            gsbutils::dprintf_c(dbg, "\n");
            // 03 28 1e          int8                 Device_temperature температура устройства
            // 05 21 08 00       uint16                PowerOutages Счетчик пропаданий напряжения
            // 08 21 00 00       uint16
            // 09 21 00 00       uint16
            // 0b 28 00          uint8  ??Подключен или нет потребитель
            // 0с 20 00          uint8
            // 64 10 00          bool                    State    текущее состояние реле1
            // 65 10 00          bool                    State    текущее состояние реле2(для двухканальных)
            // 95 39 00 00 00 00 float Single precision  // Потребленная мощность за какой-то период
            // 96 39 00 00 00 00 float Single precision  Voltage  напряжение на реле
            // 97 39 00 00 00 00 float Single precision   ток
            // 98 39 00 00 00 00 float Single precision  Power    мгновенная потребляемая мощность
            // 9a 28 00          uint8
            //
            for (unsigned i = 0; i < input.size(); i++)
            {

                uint8_t att_id = (uint8_t)input.at(i);

                switch (att_id)
                {
                case 0x03:
                {
                    i = i + 2;
                    ed->set_temperature((double)input.at(i));
                    gsbutils::dprintf(dbg, "Temperature  %d\n", input.at(i));
                }
                break;
                case 0x05: // Потеря питания 220В - количество раз
                    gsbutils::dprintf(dbg, "Power outages  %d\n", _UINT16(input.at(i + 2), input.at(i + 3)));
                    i = i + 3;
                    break;

                case 0x08: // uint16
                case 0x09: // uint16
                {
                    gsbutils::dprintf(dbg, "tag 0x%02x  %d\n", att_id, _UINT16(input.at(i + 2), input.at(i + 3)));
                    i = i + 3;
                }
                break;

                case 0x64: // status
                {
                    i = i + 2;
                    ed->set_current_state(input.at(i) == 1 ? "On" : "Off");
                    gsbutils::dprintf(dbg, "State  %d\n", input.at(i));
                }
                break;

                case 0x95: // Потребленная мощность
                {
                    union
                    {
                        int num;
                        float fnum;
                    } my_union;
                    my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);

                    gsbutils::dprintf(dbg, "Потребленная мощность (0x95)  %0.6f\n", my_union.fnum);
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
                    if (ed)
                    {
                        ed->setPowerSource(0x01);
                        ed->set_mains_voltage(my_union.fnum / 10);
                    }
                    gsbutils::dprintf(dbg, "Напряжение  %0.2f\n", my_union.fnum / 10);
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
                    ed->set_current(val);
                    gsbutils::dprintf(dbg, "Ток: %0.3f\n", val);
                    i = i + 5;
                }
                break;

                case 0x98: // Текущая потребляемая мощность
                {
                    union
                    {
                        int num;
                        float fnum;
                    } my_union;
                    my_union.num = input.at(i + 2) + (input.at(i + 3) << 8) + (input.at(i + 4) << 16) + (input.at(i + 5) << 24);

                    gsbutils::dprintf(dbg, "Текущая потребляемая мощность(0x98) %0.6f\n", my_union.fnum);

                    i = i + 5;
                }
                break;

                case 0x9a: // тип 0x20 uint8
                case 0x0b: // тип 0x20 uint8
                {
                    gsbutils::dprintf(dbg, "0x%02x  %d\n", att_id, input.at(i + 2));
                    i = i + 2;
                }
                break;

                default:
                {
                    gsbutils::dprintf(1, "Необработанный тэг %d type %d \n ", att_id, input.at(i + 1));
                    i++;
                }
                } // switch
                if (i >= input.size())
                    break;
            } // for
        }
        break;
        case 0xFF01: // door sensor, xiaomi switch
        {
            std::vector<uint8_t> input{};
            for (uint8_t b : any_cast<std::string>(attribute.value))
            {
                input.push_back((uint8_t)b);
                gsbutils::dprintf_c(dbg, "%02x ", b);
            }
            for (unsigned i = 0; i < input.size(); i++)
            {
                uint8_t att_id = (uint8_t)input.at(i);

                switch (att_id)
                {
                case 0x03: // device temperature
                {
                    i = i + 2;
                    //                           ed->set_temperature((double)input.at(i));
                    gsbutils::dprintf(1, "xiaomi temperature  %d\n", input.at(i));
                }
                break;
                case 0x05: // RSSI
                {
                    uint16_t rssi = _UINT16(input.at(i + 2), input.at(i + 3)) - 90;
                    gsbutils::dprintf(dbg, "xiaomi  RSSI: %d dBm \n", rssi);
                    i = i + 3;
                }
                break;
                }
            }
        }
        break;
        default:
            gsbutils::dprintf(1, "Xiaomi:: unknown attribute Id 0x%04x\n", attribute.id);
        }
    }
}
