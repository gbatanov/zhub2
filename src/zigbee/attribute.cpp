#include <array>
#include <set>
#include <queue>
#include <memory>
#include <utility>
#include <optional>
#include <any>
#include <limits>
#include <sstream>
#include <string.h>
#include <termios.h>

#include "../version.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../../gsb_utils/gsbutils.h"
#include "../common.h"
#include "zigbee.h"

using namespace zigbee;

std::vector<zcl::Attribute> zcl::Attribute::parseAttributesPayload(std::vector<uint8_t> &payload, bool status_field)
{
    std::vector<Attribute> attributes;

    try
    {
        size_t i = 0;
        bool valid = true;
        size_t maxI = payload.size();

        // Минимальная длина - 4 байта: 2 - attribute id, 1 -тип, 1 значение
        if (maxI < 4)
            return attributes;

        while (i < maxI)
        {
            zcl::Attribute attribute;

            uint8_t lo = payload.at(i++);
            if (i >= maxI)
                break;

            uint8_t hi = payload.at(i++);
            if (i >= maxI)
                break;
            attribute.id = _UINT16(lo, hi);

            if (status_field)
            {
                Status attribute_status = static_cast<Status>(payload.at(i++));
                if (i >= maxI)
                    break;

                if (attribute_status != Status::SUCCESS)
                {
                    continue;
                }
            }
            attribute.data_type = static_cast<DataType>(payload.at(i++));
            if (i >= maxI)
                return attributes;

            if (static_cast<uint8_t>(attribute.data_type) == 0)
            {
                std::string error = std::string("throw error with attribute data type: ") +
                                    std::to_string((unsigned)attribute.data_type) + std::string(" attribute_id: ") + std::to_string(attribute.id);
                throw std::runtime_error(error.c_str());
            }

            size_t size = 0;
            valid = true;

            switch (attribute.data_type)
            {

            case DataType::ARRAY: // реализация условная, теоретически могут быть вложенные объекты
            case DataType::STRUCTURE:
            case DataType::SET:
            case DataType::BAG:
            {
                uint8_t lo = payload.at(i++);
                uint8_t hi = payload.at(i++);
                if (i >= maxI)
                    return attributes;

                size = _UINT16(lo, hi);
                attribute.value = std::vector<uint8_t>(reinterpret_cast<uint8_t *>(payload.data() + i),
                                                       reinterpret_cast<uint8_t *>(payload.data() + i + size));
            }
            break;

            case DataType::OCT_STRING:
            case DataType::CHARACTER_STRING:
            {
                size = payload.at(i++);
                if (i >= maxI)
                    return attributes;

                attribute.value = std::string(reinterpret_cast<char *>(payload.data() + i), size);
            }
            break;

            case DataType::LONG_OCT_STRING:
            case DataType::LONG_CHARACTER_STRING:
            {
                uint8_t lo = payload.at(i++);
                if (i >= maxI)
                    return attributes;

                uint8_t hi = payload.at(i++);
                if (i >= maxI)
                    return attributes;

                size = _UINT16(lo, hi);
                if (i + size >= maxI)
                    return attributes;
                attribute.value = std::string(reinterpret_cast<char *>(payload.data() + i), size);
            }
            break;

            case DataType::BOOLEAN:
            {
                attribute.value = convertAttributeData<bool>(payload.data() + i);
                size = sizeof(bool);
            }
            break;

            case DataType::ENUM8:
            case DataType::BITMAP8:
            case DataType::DATA8:
            case DataType::UINT8:
            {
                attribute.value = convertAttributeData<uint8_t>(payload.data() + i);
                size = sizeof(uint8_t);
            }
            break;

            case DataType::ENUM16:
            case DataType::BITMAP16:
            case DataType::DATA16:
            case DataType::UINT16:
            {
                attribute.value = convertAttributeData<uint16_t>(payload.data() + i);
                size = sizeof(uint16_t);
            }
            break;

            case DataType::BITMAP32:
            case DataType::DATA32:
            case DataType::UINT32:
            {
                attribute.value = convertAttributeData<uint32_t>(payload.data() + i);
                size = 4;
            }
            break;

            case DataType::BITMAP40:
            case DataType::DATA40:
            case DataType::UINT40:
            {
                size = 5;
                attribute.value = convertAttributeData<uint64_t>(payload.data() + i);
            }
            break;

            case DataType::BITMAP48:
            case DataType::DATA48:
            case DataType::UINT48:
            {
                size = 6;
                attribute.value = convertAttributeData<uint64_t>(payload.data() + i);
            }
            break;

            case DataType::BITMAP56:
            case DataType::DATA56:
            case DataType::UINT56:
            {
                size = 7;
                attribute.value = convertAttributeData<uint64_t>(payload.data() + i);
            }
            break;

            case DataType::BITMAP64:
            case DataType::DATA64:
            case DataType::UINT64:
            {
                attribute.value = convertAttributeData<uint64_t>(payload.data() + i);
                size = sizeof(uint64_t);
            }
            break;

            case DataType::INT8:
            {
                attribute.value = convertAttributeData<int8_t>(payload.data() + i);
                size = 1;
            }
            break;

            case DataType::INT16:
            {
                attribute.value = convertAttributeData<int16_t>(payload.data() + i);
                size = 2;
            }
            break;

            case DataType::INT24:
            {
                attribute.value = convertAttributeData<int32_t>(payload.data() + i);
                size = 3;
            }
            break;

            case DataType::INT32:
            {
                attribute.value = convertAttributeData<int32_t>(payload.data() + i);
                size = 4;
            }
            break;

            case DataType::INT40:
            {
                attribute.value = convertAttributeData<int64_t>(payload.data() + i);
                size = 5;
            }
            break;

            case DataType::INT48:
            {
                attribute.value = convertAttributeData<int64_t>(payload.data() + i);
                size = 6;
            }
            break;

            case DataType::INT56:
            {
                attribute.value = convertAttributeData<int64_t>(payload.data() + i);
                size = 7;
            }
            break;

            case DataType::INT64:
            {
                attribute.value = convertAttributeData<int64_t>(payload.data() + i);
                size = 8;
            }
            break;
                // TODO: считать по формуле из спецификации, с.101
            case DataType::SEMI_FLOAT: // half precision
            {
                attribute.value = convertAttributeData<float>(payload.data() + i);
                size = 2;
            }
            break;
            case DataType::FLOAT: // single precision
            {
                attribute.value = convertAttributeData<float>(payload.data() + i);
                size = 4;
            }
            break;
            case DataType::DOUBLE: // double precision
            {
                attribute.value = convertAttributeData<double>(payload.data() + i);
                size = 8;
            }
            break;

            case DataType::TIME_OF_DAY: // hours minutes seconds hundredths
            case DataType::DATE:        // year-1900 month day_of_month day_of_week
            case DataType::UTC_TIME:    // the number of seconds since 0 hours, 0 minutes, 0 seconds, on the 1st of January, 2000 UTC.
            {
                attribute.value = convertAttributeData<uint32_t>(payload.data() + i);
                size = 4;
            }
            break;

            case DataType::CLUSTER_ID:
            case DataType::ATTRIBUTE_ID:
            {
                attribute.value = convertAttributeData<uint16_t>(payload.data() + i);
                size = 2;
            }
            break;

            case DataType::BACNET_OID:
            {
                attribute.value = convertAttributeData<uint32_t>(payload.data() + i);
                size = 4;
            }
            break;

            case DataType::IEEE_ADDRESS:
            {
                attribute.value = convertAttributeData<uint64_t>(payload.data() + i);
                size = 8;
            }
            break;
            case DataType::SECURITY_KEY_128:
            {
                size = 16;
                attribute.value = std::vector<uint8_t>(reinterpret_cast<uint8_t *>(payload.data() + i),
                                                       reinterpret_cast<uint8_t *>(payload.data() + i + size));
            }
            break;

            default:
                gsbutils::dprintf(1, "Unknown attribute data type: %d for attribute 0x%04x\n", (uint8_t)attribute.data_type, attribute.id);
                valid = false;
                break;
            } // switch

            if (valid)
                attributes.push_back(attribute);
            else
                return attributes;
            i += size;
            if (i >= maxI)
                return attributes;

        } // while
    }
    catch (std::out_of_range &error)
    {
        ERRLOG("%s\n", error.what());
    }

    return attributes;
}
