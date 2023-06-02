#include "../version.h"
#include <algorithm>
#include <mutex>
#include <thread>
#include <string>
#include <string.h>
#include <inttypes.h>
#include <sstream>
#include <array>
#include <set>
#include <queue>
#include <optional>
#include <memory>
#include <any>
#include <termios.h>

#include "../version.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../../gsb_utils/gsbutils.h"
#include "../common.h"
#include "zigbee.h"
#include "../main.h"

using namespace zigbee;

extern std::unique_ptr<zigbee::Zhub> zhub;
extern std::atomic<bool> Flag;
std::atomic<uint8_t> transaction_sequence_number = 0;
std::mutex trans_mutex;

Zdo::Zdo()
{
}

Zdo::~Zdo()
{
}

// Сброс zigbee-адаптера, по умолчанию используем программный сброс без очистки конфига и сети
// Практика показала, что даже если был аппаратный сброс, надежнее после него использовать программный.
// В текущей прошивке координатора некорректно реализован возврат типа сброса,
// для программного и аппаратного приходит один и тот же код
int Zdo::reset(ResetType reset_type, bool clear_network_state, bool clear_config)
{
    int res = 1;
    std::optional<Command> reset_response = std::nullopt;
    // аппаратный сброс игнорируем
    event_command_.clear(zigbee::CommandId::SYS_RESET_IND); // очистка события с идентификатором id
    // Программный сброс делаю в любом случае (с 2.23.560)
    res = 2;
    uint8_t startup_options = static_cast<uint8_t>(clear_network_state << 1) + static_cast<uint8_t>(clear_config);

    // writeNv вызывает синхронный запрос
    writeNv(NvItems::STARTUP_OPTION, std::vector<uint8_t>{startup_options}); // STARTUP_OPTION = 0x0003

    Command reset_request(zigbee::CommandId::SYS_RESET_REQ);
    reset_request.payload(0) = static_cast<uint8_t>(reset_type);
    event_command_.clear(zigbee::CommandId::SYS_RESET_IND); // очистка события с идентификатором id

    if (uart_.sendCommandToDevice(reset_request))
        reset_response = event_command_.wait(zigbee::CommandId::SYS_RESET_IND, RESET_TIMEOUT); // ожидаем наступления события с заданным идентификатором

    if (reset_response)
    {
        version_[0] = reset_response->payload(3); // 0x02 // Major release number.
        version_[1] = reset_response->payload(4); // 0x06 // Minor release number.
        version_[2] = reset_response->payload(5); // 0x06 // Hardware revision number.

        return res;
    }
    else
        return 0;
}

bool Zdo::startup(std::chrono::duration<int, std::milli> delay)
{
    Command startup_request(zigbee::CommandId::ZDO_STARTUP_FROM_APP);
    startup_request.payload(0) = LOWBYTE(delay.count());
    startup_request.payload(1) = HIGHBYTE(delay.count());

    std::optional<Command> startup_response = syncRequest(startup_request);

    enum class StartupStatus
    {
        RESTORED_STATE = 0, //  Restored network state
        NEW_STATE = 1,      //  New network state
        NOT_STARTED = 2     // Leave and not Starte
    };

    if ((!startup_response) || (static_cast<StartupStatus>(startup_response->payload(0)) == StartupStatus::NOT_STARTED))
        return false;

    std::optional<Command> device_info_response = syncRequest(Command(CommandId::UTIL_GET_DEVICE_INFO));

    if ((device_info_response) && (static_cast<Status>(device_info_response->payload(0)) == Status::SUCCESS))
    {
        int dbg = 3;
        mac_address_ = *((zigbee::IEEEAddress *)(device_info_response->data() + 1));
        gsbutils::dprintf_c(dbg, "Configurator info: IEEE address: 0x%" PRIx64 " \n", mac_address_);     // 0x00124b001cdd4e4e  для CC2530
        network_address_ = _UINT16(device_info_response->payload(9), device_info_response->payload(10)); // shortAddr 0x0000 для координатора
        gsbutils::dprintf_c(7, "Configurator info: shortAddr: 0x%04x \n", network_address_);
        gsbutils::dprintf_c(7, "Configurator info: Device type: 0x%02x \n", device_info_response->payload(11));    // DeviceType: COORDINATOR, ROUTER, END_DEVICE (0x7) ??? не совсем понятно, как получена расшифровка, возможно битовая комбинация
        gsbutils::dprintf_c(dbg, "Configurator info: Device state: 0x%02x \n", device_info_response->payload(12)); // ZdoState COORDINATOR = 9  Started as ZigBee Zhub
        gsbutils::dprintf_c(dbg, "Configurator info: Associated device count: %d \n", device_info_response->payload(13));

        {
            char buffer[512]{0};
            if (device_info_response->payload(13) > 0)
            {
                size_t count = device_info_response->payload(13);
                uint8_t i = 14;
                while (count--)
                {
                    char buff2[64]{0};
                    size_t len = snprintf(buff2, 64, "0x%04x ", _UINT16(device_info_response->payload(i), device_info_response->payload(i + 1)));
                    buff2[len] = 0;
                    strcat(buffer, buff2);
                    strcat(buffer, ", ");
                    i += 2;
                }
                gsbutils::dprintf(1, "Configurator info: Associated device list: %s \n", buffer);
            }
        }
    }
    else
    {
        gsbutils::dprintf(1, "Configurator info bad  \n");
    }

    //
    return true;
}

// регистрация эндпойнта самого координатора
bool Zdo::registerEndpoint(zigbee::SimpleDescriptor endpoint_descriptor)
{
    if (endpoint_descriptor.endpoint_number > 240)
        return false;

    Command register_ep_request(zigbee::CommandId::AF_REGISTER);

    {
        size_t i = 0;

        register_ep_request.payload(i++) = static_cast<uint8_t>(endpoint_descriptor.endpoint_number);

        register_ep_request.payload(i++) = LOWBYTE(endpoint_descriptor.profile_id);
        register_ep_request.payload(i++) = HIGHBYTE(endpoint_descriptor.profile_id);

        register_ep_request.payload(i++) = LOWBYTE(endpoint_descriptor.device_id);
        register_ep_request.payload(i++) = HIGHBYTE(endpoint_descriptor.device_id);

        register_ep_request.payload(i++) = static_cast<uint8_t>(endpoint_descriptor.device_version);

        register_ep_request.payload(i++) = 0; // 0x00 - No latency*, 0x01 - fast beacons, 0x02 - slow beacons.

        register_ep_request.payload(i++) = static_cast<uint8_t>(endpoint_descriptor.input_clusters.size());
        for (auto cluster : endpoint_descriptor.input_clusters)
            register_ep_request.payload(i++) = static_cast<uint8_t>(cluster);

        register_ep_request.payload(i++) = static_cast<uint8_t>(endpoint_descriptor.output_clusters.size());
        for (auto cluster : endpoint_descriptor.output_clusters)
            register_ep_request.payload(i++) = static_cast<uint8_t>(cluster);
    }

    std::optional<Command> response = (syncRequest(register_ep_request));
    return ((response) && (static_cast<Status>(response->payload(0)) == Status::SUCCESS));
}

// Команда разрешения спаривания координатора с устройствами
bool Zdo::permitJoin(const std::chrono::duration<int, std::ratio<1>> duration)
{
    Command permit_join_request(zigbee::CommandId::ZDO_MGMT_PERMIT_JOIN_REQ);

    permit_join_request.payload(0) = 0x0F;                            // Destination address type : 0x02 - Address 16 bit, 0x0F - Broadcast.
    permit_join_request.payload(1) = 0xFC;                            // Specifies the network address of the destination device whose Permit Join information is to be modified.
    permit_join_request.payload(2) = 0xFF;                            // (address || 0xFFFC)
    permit_join_request.payload(3) = std::min(duration.count(), 254); // Maximum duration.
    permit_join_request.payload(4) = 0x00;                            // Trust Center Significance (0).

    return asyncRequest(permit_join_request);
}

// Чтение из NVRAM координатора
std::optional<std::vector<uint8_t>> Zdo::readNv(NvItems item)
{
    std::vector<uint8_t> value;

    Command read_nv_request(zigbee::CommandId::SYS_OSAL_NV_READ);

    read_nv_request.payload(0) = LOWBYTE(item); // The Id of the NV item.
    read_nv_request.payload(1) = HIGHBYTE(item);

    read_nv_request.payload(2) = 0; // Number of bytes offset from the beginning or the NV value.

    std::optional<Command> read_nv_response = syncRequest(read_nv_request);

    if (read_nv_response)
    {
        if (read_nv_response->payload_size() > 0)
        {
        }
        else
        {
            gsbutils::dprintf(7, "Zdo::readNv: read_nv_response null length\n");
        }
    }
    else
    {
        gsbutils::dprintf(7, "Zdo::readNv: no  read_nv_response\n");
    }
    if ((read_nv_response) && (static_cast<Status>(read_nv_response->payload(0)) == Status::SUCCESS))
    {
        size_t length = static_cast<size_t>(read_nv_response->payload(1));
        std::copy_n(read_nv_response->data() + 2, length, back_inserter(value));

        return value;
    }

    return std::nullopt;
}

// Запись в NVRAM координатора
bool Zdo::writeNv(NvItems item, std::vector<uint8_t> item_data)
{
    assert(item_data.size() <= 240);

    Command write_nv_request(zigbee::CommandId::SYS_OSAL_NV_WRITE, item_data.size() + 4);

    {
        write_nv_request.payload(0) = LOWBYTE(item); // The Id of the NV item.
        write_nv_request.payload(1) = HIGHBYTE(item);
        write_nv_request.payload(2) = 0; // Number of bytes offset from the beginning or the NV value.
        write_nv_request.payload(3) = static_cast<uint8_t>(item_data.size());
        std::copy(item_data.begin(), item_data.end(), write_nv_request.data() + 4);
    }
    std::optional<Command> response = syncRequest(write_nv_request);
    return ((response) && (static_cast<Status>(response->payload(0)) == Status::SUCCESS));
}

// Инициализация элемента в NVRAM координатора
bool Zdo::initNv(NvItems item, uint16_t length, std::vector<uint8_t> item_data)
{
    assert(item_data.size() <= 245);

    Command init_nv_request(zigbee::CommandId::SYS_OSAL_NV_ITEM_INIT);

    init_nv_request.payload(0) = LOWBYTE(item); // The Id of the NV item.
    init_nv_request.payload(1) = HIGHBYTE(item);

    init_nv_request.payload(2) = LOWBYTE(length); // Number of bytes in the NV item.
    init_nv_request.payload(3) = HIGHBYTE(length);

    init_nv_request.payload(4) = static_cast<uint8_t>(item_data.size()); // Number of bytes in the initialization data.

    std::copy(item_data.begin(), item_data.end(), init_nv_request.data() + 5);

    std::optional<Command> init_nv_response = syncRequest(init_nv_request);

    return (init_nv_response && (init_nv_response->payload(0) != 0x0A)); // 0x00 = Item already exists, no action taken
    // 0x09 = Success, item created and initialized
    // 0x0A = Initialization failed, item not created
    // TODO: Error handling?
}

// Удаление элемента из NVRAM координатора
// SYS_OSAL_NV_DELETE This command is used by the application processor to delete an item from the CC2530 NV
//  memory. The ItemLen parameter must match the length of the NV item or the command will fail.
//  Use this command with caution – deleted items cannot be recovered.
//  SREQ:
//  Length = 0x04 Cmd0 = 0x21 Cmd1 = 0x12 Id ItemLen
//  Id – 2 bytes – The attribute id of the NV item.
//  ItemLen – 2 bytes - Number of bytes in the NV item.
//  SYS_OSAL_NV_LENGTH
// This command is used by the application processor to get the length of an item in the CC2530 NV
// memory. A length of zero is returned if the NV item does not exist.
//  SREQ:
// Length = 0x02 Cmd0 = 0x21 Cmd1 = 0x13 Id
// Id – 2 bytes – The attribute id of the NV item.
// SRSP:
// Length = 0x02 Cmd0 = 0x61 Cmd1 = 0x13 ItemLen
bool Zdo::deleteNvItem(NvItems item)
{
    Command len_nv_request(zigbee::CommandId::SYS_OSAL_NV_LENGTH);
    len_nv_request.payload(0) = LOWBYTE(item); // The Id of the NV item.
    len_nv_request.payload(1) = HIGHBYTE(item);
    std::optional<Command> len_nv_response = syncRequest(len_nv_request);
    if (len_nv_response)
    {
        uint16_t item_len = _UINT16(len_nv_response->payload(0), len_nv_response->payload(1));
        Command del_nv_request(zigbee::CommandId::SYS_OSAL_NV_DELETE);
        del_nv_request.payload(0) = LOWBYTE(item); // The Id of the NV item.
        del_nv_request.payload(1) = HIGHBYTE(item);
        del_nv_request.payload(2) = LOWBYTE(item_len); // Number of bytes in the NV item.
        del_nv_request.payload(3) = HIGHBYTE(item_len);
        std::optional<Command> del_nv_response = syncRequest(del_nv_request);

        return (del_nv_response && (del_nv_response->payload(0) == 0x00));
    }

    return false;
}

// TODO: Подключить к работе
// Возвращает актуальное значение значение
std::optional<int> Zdo::setTransmitPower(int power)
{
    // Returned actual TX power (dBm). Range: -22 ... 3 dBm.
    Command tx_power_request(zigbee::CommandId::SYS_SET_TX_POWER);

    tx_power_request.payload(0) = LOWBYTE(power);

    std::optional<Command> tx_power_response = syncRequest(tx_power_request);

    if (tx_power_response)
        return tx_power_response->payload(0);
    else
        return std::nullopt;
}

// Предварительный обработчик сообщений от координатора
// Основнаяя обработка входящих сообщений(AF_INCOMING_MSG) в OnMessage
void Zdo::handle_command(Command command)
{
#ifdef TEST
    gsbutils::dprintf(1, "Zdo::handle_command Command:%04x %s\n", command.id(), getCommandStr(command).c_str());
#endif
    switch (command.id())
    {
        // Это входящие сообщения от устройств
        // Игнорирую до инициализации конфигуратора
    case zigbee::CommandId::AF_INCOMING_MSG: // 0x4481
    {
        gsbutils::dprintf(3, "Zdo::handle_command AF_INCOMING_MSG:%04x\n", command.id());
        if (!zhub->get_ready())
            return;
        gsbutils::dprintf(7, "Zdo::handle_command AF_INCOMING_MSG:%04x\n", command.id());
        try
        {
            zhub->on_message(command);
        }
        catch (std::exception &err)
        {
            std::stringstream sstream;
            sstream << "ZDO::handle_command: Bad Incoming Message data:" << err.what() << std::endl;
            zhub->OnError(sstream.str());
        }
        return;
    }

    // Уведомление об изменении текущего состояния координатора
    //  COORDINATOR = 9,   Started as ZigBee Zhub
    case zigbee::CommandId::ZDO_STATE_CHANGE_IND:
    {
        gsbutils::dprintf(7, "Zdo::handle_command: ZDO_STATE_CHANGE_IND:%04x\n", command.id());
        current_state_ = static_cast<ZdoState>(command.payload(0));
        if (command.payload(0) == 9)
            zhub->set_ready();
        gsbutils::dprintf(1, "Zdo::handle_command: current state %d \n", command.payload(0));
        break;
    }

    case zigbee::CommandId::ZDO_END_DEVICE_ANNCE_IND: //  0x45c1
    {
        // Indicates reception of a ZDO Device Announce.
        // This command contains the device's SrcAddr, new 16-bit NWK address, 64-bit IEEE address and  the capabilities of the ZigBee device.
        // Capabilities 1 Биты, определяющие свойства узла
        // bit 0: 1 - Alternate PAN Zhub
        // bit 1: Device type
        // 0 – End Device
        // 1 – Router
        // bit 2: Power Source
        //   0 – Battery powered
        // 1 – Main powered
        //   bit 3: Receiver on when Idle
        //   0 – Off
        //   1 – On
        // bit 4: – Reserved
        //   bit 5: – Reserved
        //   bit 6: – Security capability
        //   bit 7: – Reserved
        // кнопка - 0x80 1000 0000 - end device, battery, no reseived when idle,
        // Анонс нового устройства, инициируется самим устройством
        //       event_command_.emit(command.id(), command);
        uint8_t dbg = 1;

#ifdef DEBUG
        gsbutils::dprintf(dbg, "Zdo::handle_command  ZDO_END_DEVICE_ANNCE_IND::%04x\n", command.id());
        if (command.payload_size() > 0)
        {

            size_t len = command.payload_size();
            uint8_t i = 0;
            gsbutils::dprintf_c(dbg, "zcl_frame.payload ");
            while (len--)
            {
                gsbutils::dprintf_c(dbg, " %02x ", command.payload(i++));
            }
        }
        gsbutils::dprintf_c(dbg, "\n");
#endif
        zigbee::NetworkAddress networkAddress = _UINT16(command.payload(2), command.payload(3));
        zigbee::IEEEAddress macAddress = *(reinterpret_cast<zigbee::IEEEAddress *>(&command.payload(4)));
        gsbutils::dprintf(dbg, "Zdo::ZDO_END_DEVICE_ANNCE_IND: new shortAddress: 0x%04x\n", networkAddress);
        gsbutils::dprintf(dbg, "Zdo::ZDO_END_DEVICE_ANNCE_IND: IEEEaddress: 0x%" PRIx64 " \n", macAddress);
        zhub->on_join(networkAddress, macAddress);
    }
    break;
    //
    case zigbee::CommandId::ZDO_TC_DEV_IND: // 0x45ca This response indication message is sent whenever the Trust Center allows a device to join the network.
                                            //  This message will only occur on a zhub/trust center device.
                                            // формат payload не совпадает с ZDO_END_DEVICE_ANNCE_IND
                                            // может приходить несколько раз для одного события
    {
//        event_command_.emit(command.id(), command);
#ifdef TEST
        uint8_t dbg = 5;
        gsbutils::dprintf(dbg, "Zdo::handle_command: ZDO_TC_DEV_IND:%04x\n", command.id());
        zigbee::NetworkAddress networkAddress = _UINT16(command.payload(0), command.payload(1));
        zigbee::IEEEAddress macAddress = *(reinterpret_cast<zigbee::IEEEAddress *>(&command.payload(2)));
        gsbutils::dprintf(dbg, "Zdo::ZDO_END_DEVICE_ANNCE_IND: new shortAddress: 0x%04x\n", networkAddress);
        gsbutils::dprintf(dbg, "Zdo::ZDO_END_DEVICE_ANNCE_IND: IEEEaddress: 0x%" PRIx64 " \n", macAddress);

        if (command.payload_size() > 0)
        {
            size_t len = command.payload_size();
            uint8_t i = 0;
            gsbutils::dprintf_c(dbg, "zcl_frame.payload ");
            while (len--)
            {
                gsbutils::dprintf_c(dbg, " %02x ", command.payload(i++));
            }
        }
        gsbutils::dprintf_c(dbg, "\n");
#endif
    }
    break;

    case zigbee::CommandId::ZDO_LEAVE_IND:
    {
        zigbee::NetworkAddress network_address = _UINT16(command.payload(0), command.payload(1));
        zigbee::IEEEAddress mac_address = *(reinterpret_cast<zigbee::IEEEAddress *>(&command.payload(2)));
        zhub->on_leave(network_address, mac_address);
    }
    break;
    //
    case zigbee::CommandId::ZDO_SRC_RTG_IND: // (0x45C4) This message is an indication to inform host device of receipt of a source route to a given device.
        // Length = 0x04-0x44 Cmd0 = 0x45 Cmd1 = 0xC4 dstAddr Relay Count (N) Relay List
        // Если есть устройства, работающией через роутер, то первым будет адрес устройства,
        // потом количество промежуточных роутеров, потом список роутеров
        // с версии 2.22.520 больше не использую, неинформативно для меня
        break;
    case zigbee::CommandId::ZDO_PERMIT_JOIN_IND: // 0x45cb
    {
        // В payload отдает оставшееся время в секундах
        int dbg = 3;
        if (command.payload_size() > 0)
        {
            gsbutils::dprintf(dbg, "Zdo::handle_command:  ZDO_PERMIT_JOIN_IND  Time is %d second\n", command.payload(0));
        }
    }
    break;
    case zigbee::CommandId::ZDO_BIND_RSP: // 0x45a1
    {

#ifdef TEST
        size_t len = command.payload_size();
        if (len > 0)
        {
            uint8_t i = 0;
            gsbutils::dprintf(1, "ZDO_BIND_RSP: zcl_frame.payload ");
            while (len--)
            {
                gsbutils::dprintf_c(1, " %02x ", command.payload(i++));
            }
            gsbutils::dprintf(1, "\n ");
        }
#endif
    }
    break;

    case zigbee::CommandId::ZDO_MGMT_PERMIT_JOIN_RSP: // = 0x45b6,
    {
        // Ответ на команду включения режима спаривания
    }
    break;
    case zigbee::CommandId::UNKNOWN_45B8: // 0x45b8
    case zigbee::CommandId::UNKNOWN_4F80: // 0x4f80:
    {
    }
    break;
    case zigbee::CommandId::ZDO_ACTIVE_EP_RSP: // 0x4585
    {
//        event_command_.emit(command.id(), command);
#ifdef TEST

        if ((static_cast<Status>(command.payload(2)) == Status::SUCCESS))
        {
            uint16_t shortAddr = _UINT16(command.payload(0), command.payload(1));

            std::vector<uint8_t> endpoints{};
            uint8_t ep_count = command.payload(5);
            for (int i = 0; i < ep_count; i++) // Number of active endpoint in the list
                endpoints.push_back(static_cast<uint8_t>(command.payload(6 + i)));

            gsbutils::dprintf(3, "Zhub::ZDO_ACTIVE_EP_RSP: Device 0x%04x Endpoints count: %d list: ", shortAddr, ep_count);
            for (uint8_t ep : endpoints)
            {
                gsbutils::dprintf_c(3, "%u ", ep);
            }
            gsbutils::dprintf_c(3, "\n");
        }
#endif
    }
    break;
    case zigbee::CommandId::ZDO_SIMPLE_DESC_RSP: // 0x4584
    {
        //       event_command_.emit(command.id(), command);
#ifdef TEST
        size_t len = command.payload_size();
        if (len > 0)
        {
            uint8_t i = 0;
            gsbutils::dprintf(3, "ZDO_SIMPLE_DESC_RSP: zcl_frame.payload ");
            while (len--)
            {
                gsbutils::dprintf(3, " %02x ", command.payload(i++));
            }

            if ((static_cast<Status>(command.payload(2)) == Status::SUCCESS))
            {
                uint16_t shortAddr = _UINT16(command.payload(0), command.payload(1));

                uint8_t descriptorLen = command.payload(5); // длина дескриптора, начинается с номера эндпойнта

                zigbee::SimpleDescriptor descriptor;
                descriptor.endpoint_number = static_cast<unsigned int>(command.payload(6)); // номер эндпойнта, для которого пришел дескриптор
                descriptor.profile_id = _UINT16(command.payload(7), command.payload(8));    // профиль эндпойнта
                descriptor.device_id = _UINT16(command.payload(9), command.payload(10));    // ID устройства
                descriptor.device_version = static_cast<unsigned int>(command.payload(11)); // Версия устройства

                gsbutils::dprintf(3, "ZDO_SIMPLE_DESC_RSP:: Device 0x%04x Descriptor length %d \n", shortAddr, descriptorLen);
                gsbutils::dprintf(3, "ZDO_SIMPLE_DESC_RSP:: Device 0x%04x Endpoint %d ProfileId 0x%04x DeviceId 0x%04x \n", shortAddr, descriptor.endpoint_number, descriptor.profile_id, descriptor.device_id);
                size_t i = 12; // Index of number of input clusters/

                int input_clusters_number = static_cast<int>(command.payload(i++));
                gsbutils::dprintf(3, "ZDO_SIMPLE_DESC_RSP: Input Cluster count %d \n ", input_clusters_number);
                while (input_clusters_number--)
                {
                    uint8_t p1 = command.payload(i++);
                    uint8_t p2 = command.payload(i++);
                    gsbutils::dprintf(3, "ZDO_SIMPLE_DESC_RSP: Input Cluster 0x%04x \n ", _UINT16(p1, p2));

                    descriptor.input_clusters.push_back(static_cast<unsigned int>(_UINT16(p1, p2))); // List of input cluster Id's supported.
                }
                int output_clusters_number = static_cast<int>(command.payload(i++));
                gsbutils::dprintf(3, "ZDO_SIMPLE_DESC_RSP: Output Cluster count %d \n ", output_clusters_number);
                while (output_clusters_number--)
                {
                    uint8_t p1 = command.payload(i++);
                    uint8_t p2 = command.payload(i++);
                    gsbutils::dprintf(3, "ZDO_SIMPLE_DESC_RSP: Output Cluster 0x%04x \n ", _UINT16(p1, p2));

                    descriptor.output_clusters.push_back(static_cast<unsigned int>(_UINT16(p1, p2))); // List of output cluster Id's supported.
                }
            }
        }
#endif
    }
    break;

        // Для этой группы команд (ответы на синхронные запросы) нужно фиксировать событие,
        // иначе синхронная команда отвалится по тайм-ауту
    case zigbee::CommandId::SYS_RESET_IND: // 0x4180
    {
        // Команда информирующая об аппаратном или программном сбросе
        // This command is generated by zhub automatically immediately after a reset.
        /*
        Reason – 1 byte – One of the following values indicating the reason for the reset.
                Power-up 0x00
                External 0x01
                Watch-dog 0x02
        TransportRev – 1 byte – Transport protocol revision. This is set to value of 2.
        Product – 1 byte – Product ID. This is set to value of 1.
        MajorRel – 1 byte – Major release number.
        MinorRel – 1 byte – Minor release number.
        HwRev – 1 byte – Hardware revision number
*/
#ifdef TEST
        // программный сброс: 00 02 00 02 06 03
        gsbutils::dprintf(1, "Zdo::handle_command SYS_RESET_IND\n");
        size_t len = command.payload_size();
        if (len > 0)
        {
            uint8_t i = 0;
            gsbutils::dprintf(1, "zcl_frame.payload: \n");
            while (len--)
            {
                gsbutils::dprintf_c(1, " %02x ", command.payload(i++));
            }
        }
        gsbutils::dprintf_c(1, "\n");
#endif
        event_command_.emit(command.id(), command);
    }
    break;

    case zigbee::CommandId::AF_DATA_CONFIRM: // 0x4480 Подтверждение, что команда принята устройством
                                             // Синхронный ответ на запрос AF_DATA_REQUEST

    // ответы на синхронные запросы
    case zigbee::CommandId::SYS_PING_SRSP:             // 0x6101
    case zigbee::CommandId::ZDO_STARTUP_FROM_APP_SRSP: // 0x6540
    case zigbee::CommandId::UTIL_GET_DEVICE_INFO_SRSP: // 0x6700
    case zigbee::CommandId::ZDO_MGMT_PERMIT_JOIN_SRSP: //  0x6536,
    case zigbee::CommandId::SYS_OSAL_NV_WRITE_SRSP:
    case zigbee::CommandId::SYS_OSAL_NV_READ_SRSP: // 0x6108
    case zigbee::CommandId::SYS_OSAL_NV_DELETE_SRSP:
    case zigbee::CommandId::SYS_OSAL_NV_ITEM_INIT_SRSP:
    case zigbee::CommandId::AF_DATA_REQUEST_SRSP: // 0x6401
    case zigbee::CommandId::AF_REGISTER_SRSP:     // 0x6400
    case zigbee::CommandId::ZDO_SIMPLE_DESC_SRSP: // 0x6504
    case zigbee::CommandId::ZDO_ACTIVE_EP_SRSP:   // 0x6505
    case zigbee::CommandId::ZDO_BIND_SRSP:        // 0x6521
    {
        event_command_.emit(command.id(), command);
    }
    break;

    default:
    {
#ifdef DEBUG
        gsbutils::dprintf(1, "Zdo::handle_command unknown:%04x\n", command.id());
        size_t len = command.payload_size();
        if (len > 0)
        {
            uint8_t i = 0;
            gsbutils::dprintf(7, "zcl_frame.payload ");
            while (len--)
            {
                gsbutils::dprintf(7, " %02x ", command.payload(i++));
            }
        }
        gsbutils::dprintf(7, "\n");
#endif
    }

    } // switch
}

// Парсит ответ в ZclFrame
zigbee::zcl::Frame Zdo::parseZclData(std::vector<uint8_t> data)
{
    // If there is an error in the data, an exception std::out_of_range may be thrown.
    zigbee::zcl::Frame zcl_frame;

    zcl_frame.frame_control.type = static_cast<zigbee::zcl::FrameType>(data.at(0) & 0b00000011);
    zcl_frame.frame_control.manufacturer_specific = static_cast<bool>(data.at(0) & 0b00000100);
    zcl_frame.frame_control.direction = static_cast<zigbee::zcl::FrameDirection>(data.at(0) & 0b00001000);
    zcl_frame.frame_control.disable_default_response = static_cast<bool>(data.at(0) & 0b00010000);

    if (zcl_frame.frame_control.manufacturer_specific)
        zcl_frame.manufacturer_code = _UINT16(data.at(1), data.at(2)); // поле со значением, определяемым производителем   // кнопка ИКЕА 0x7f66
    else
        zcl_frame.manufacturer_code = 0xffff; // чтобы игнорировать

    size_t i = (zcl_frame.frame_control.manufacturer_specific) ? 3 : 1;

    zcl_frame.transaction_sequence_number = data.at(i++);
    zcl_frame.command = data.at(i++); // если zcl_frame.frame_control.type == 0x00, код команды из таблицы Table 2-3
    // если == 0x01 то команда зависит от кластера
    std::copy_n(data.data() + i, data.size() - i, std::back_inserter(zcl_frame.payload));

    return zcl_frame;
}

// Отправка сообщения конечному устройству
// Команда AF_DATA_REQUEST
void Zdo::sendMessage(zigbee::Endpoint endpoint, zigbee::zcl::Cluster cluster, zigbee::zcl::Frame frame, std::chrono::duration<int, std::milli> timeout)
{
    zigbee::Message message;

    message.cluster = cluster;
    message.source = {0x0000, 1};
    message.destination = endpoint;
    message.zcl_frame = frame;

    uint8_t transaction_number = generateTransactionNumber();

    if ((message.zcl_frame.payload.size() + (message.zcl_frame.frame_control.manufacturer_specific * sizeof(message.zcl_frame.manufacturer_code)) + 3) > 128)
        return;

    Command af_data_request(zigbee::CommandId::AF_DATA_REQUEST);

    {
        af_data_request.payload(0) = LOWBYTE(message.destination.address);
        af_data_request.payload(1) = HIGHBYTE(message.destination.address);
        af_data_request.payload(2) = message.destination.number;
        af_data_request.payload(3) = message.source.number;
        af_data_request.payload(4) = LOWBYTE(message.cluster);
        af_data_request.payload(5) = HIGHBYTE(message.cluster);
        af_data_request.payload(6) = transaction_number;
        af_data_request.payload(7) = 0; // Options. с.49 ZNP спецификации на 2530 процессор
        af_data_request.payload(8) = DEFAULT_RADIUS;
        af_data_request.payload(10) = (static_cast<uint8_t>(message.zcl_frame.frame_control.type) & 0b00000011) +
                                      (static_cast<uint8_t>(message.zcl_frame.frame_control.manufacturer_specific) << 2) +
                                      (static_cast<uint8_t>(message.zcl_frame.frame_control.direction) << 3) +
                                      (static_cast<uint8_t>(message.zcl_frame.frame_control.disable_default_response) << 4);

        uint8_t i = 11;

        if (message.zcl_frame.frame_control.manufacturer_specific)
        {
            af_data_request.payload(i++) = LOWBYTE(message.zcl_frame.manufacturer_code);
            af_data_request.payload(i++) = HIGHBYTE(message.zcl_frame.manufacturer_code);
        }

        af_data_request.payload(i++) = message.zcl_frame.transaction_sequence_number;
        af_data_request.payload(i++) = message.zcl_frame.command;

        for (auto byte : message.zcl_frame.payload)
            af_data_request.payload(i++) = byte;

        af_data_request.payload(9) = i; // data length.
    }

    asyncRequest(af_data_request);
}

std::vector<uint8_t> Zdo::read_rf_channels()
{
    int dbg = 4;
    gsbutils::dprintf(dbg, "Zdo: Reading RF channels\n");
    std::optional<std::vector<uint8_t>> item_data;
    std::vector<uint8_t> rf{};

    item_data = readNv(NvItems::CHANNEL_LIST); // CHANNEL_LIST = 0x0084
    {
        // channel bit mask. Little endian. Default is 0x00 00 08 00 for CH11; 00 00 30 00 CH12,CH13
        uint32_t channelBitMask = *(reinterpret_cast<uint32_t *>(item_data->data())); //
        gsbutils::dprintf(dbg, "read channelBitMask: 0x%08x\n", channelBitMask);      //  0x00 00 08 00
        gsbutils::dprintf(dbg, "configuration.channels: ");
        for (unsigned int i = 0; i < 32; i++)
        {
            if (channelBitMask & (1 << i))
            {
                rf.push_back(i);
                gsbutils::dprintf_c(dbg, " %d ", i); // TEST - 11
            }
        }
        gsbutils::dprintf_c(dbg, "\n");
    }
    return rf;
}
/*
// Читает конфигурацию сети из координатора
zigbee::NetworkConfiguration Zdo::readNetworkConfiguration()
{
    int dbg = 4;
    gsbutils::dprintf(dbg, "Zdo: Reading network configuration\n");

    std::optional<std::vector<uint8_t>> item_data;
    zigbee::NetworkConfiguration configuration;

    item_data = readNv(NvItems::PAN_ID); // PAN_ID = 0x0083, идентификатор сети
    if (item_data && item_data->size() == 2)
    {
        configuration.pan_id = _UINT16((*item_data)[0], (*item_data)[1]);
        gsbutils::dprintf(dbg, "Configuration.pan_id 0x%04x\n", configuration.pan_id); // TEST -  0x1a62
    }
    item_data = readNv(NvItems::EXTENDED_PAN_ID); // EXTENDED_PAN_ID = 0x002D MAC address
    if (item_data && item_data->size() == 8)
    {
        configuration.extended_pan_id = *(reinterpret_cast<uint64_t *>(item_data->data()));
        gsbutils::dprintf(dbg, "Configuration.extended_pan_id 0x%" PRIx64 "\n", configuration.extended_pan_id); // TEST - 0x124b001cdd4e4e
    }
    item_data = readNv(NvItems::LOGICAL_TYPE); // LOGICAL_TYPE = 0x0087
    if (item_data && item_data->size() == 1)
    {
        configuration.logical_type = static_cast<zigbee::LogicalType>((*item_data)[0]);            //  Logical type
        gsbutils::dprintf(dbg, "Configuration.logical_type 0x%02x\n", configuration.logical_type); // TEST -0 (COORDINATOR)
    }
    item_data = readNv(NvItems::PRECFG_KEYS_ENABLE); // PRECFG_KEYS_ENABLE = 0x0063
    if (item_data && item_data->size() == 1)
    {
        configuration.precfg_key_enable = (*item_data)[0];
        gsbutils::dprintf(dbg, "Configuration.precfg_key_enable %d \n", configuration.precfg_key_enable); // TEST false
    }

    if (configuration.precfg_key_enable)
    {
        item_data = readNv(NvItems::PRECFG_KEY); // PRECFG_KEY = 0x0062
        if (item_data && item_data->size() == 16)
        {
            gsbutils::dprintf(dbg, "Configuration.precfg_key: ");
            for (uint8_t i = 0; i < 16; i++)
            {
                configuration.precfg_key[i] = item_data.value()[i];
                gsbutils::dprintf_c(dbg, "Configuration.precfg_key %02x \n", configuration.precfg_key[i]); // TEST absent
            }
            gsbutils::dprintf_c(dbg, "\n");
        }
    }

    item_data = readNv(NvItems::CHANNEL_LIST); // CHANNEL_LIST = 0x0084
    {
        // channel bit mask. Little endian. Default is 0x00 00 08 00 for CH11; 00 00 30 00 CH12,CH13
        uint32_t channelBitMask = *(reinterpret_cast<uint32_t *>(item_data->data())); //
        gsbutils::dprintf(dbg, "read channelBitMask: 0x%08x\n", channelBitMask);      //  0x00 00 08 00
        gsbutils::dprintf(dbg, "configuration.channels: ");
        for (unsigned int i = 0; i < 32; i++)
        {
            if (channelBitMask & (1 << i))
            {
                configuration.channels.push_back(i);
                gsbutils::dprintf_c(dbg, " %d ", i); // TEST - 11
            }
        }
        gsbutils::dprintf_c(dbg, "\n");
    }

    return configuration;
}
*/
bool Zdo::write_rf_channels(std::vector<uint8_t> rf)
{
    int dbg = 4;
    gsbutils::dprintf(dbg, "Write RF channels\n");
    uint32_t channelBitMask = 0;
    for (uint8_t channel : rf)
        channelBitMask |= (1 << channel);

    std::vector<uint8_t> chan{};
    for (int i = 0; i < 4; i++)
    {
        uint8_t ch = channelBitMask >> i * 8;
        chan.push_back(ch);
    }

    if (!writeNv(NvItems::CHANNEL_LIST, chan)) // старший байт последний
        return false;

    return true;
}
bool Zdo::finish_configuration()
{
    if (!writeNv(NvItems::ZDO_DIRECT_CB, std::vector<uint8_t>{1}))
        return false;

    if (!initNv(NvItems::ZNP_HAS_CONFIGURED, 1, std::vector<uint8_t>{0}))
        return false;

    if (!writeNv(NvItems::ZNP_HAS_CONFIGURED, std::vector<uint8_t>{0x55}))
        return false;

    // После изменения в  конфигурации еще раз делаем программный сброс
   int  res = reset(ResetType::SOFT, false, false);
    if (0 == res)
        return false;

    return true;
}
/*
// Записывает новую конфигурацию сети в координатор
bool Zdo::writeNetworkConfiguration(zigbee::NetworkConfiguration newConfiguration, zigbee::NetworkConfiguration current_configuration, bool &wasChange)
{
    int dbg = 4;
    wasChange = false;
    gsbutils::dprintf(dbg, "Write network configuration\n");
    if (current_configuration.logical_type != zigbee::LogicalType::COORDINATOR)
    {
        if (!writeNv(NvItems::LOGICAL_TYPE, std::vector<uint8_t>{static_cast<uint8_t>(zigbee::LogicalType::COORDINATOR)}))
            return false; //

        wasChange = true;
    }

    if (newConfiguration.precfg_key_enable)
    {
        if (!writeNv(NvItems::PRECFG_KEYS_ENABLE, std::vector<uint8_t>{static_cast<uint8_t>(newConfiguration.precfg_key_enable)}))
            return false;
        std::vector<uint8_t> temp_v{};
        for (uint8_t i = 0; i < 16; i++)
        {
            temp_v[i] = newConfiguration.precfg_key[i];
        }
        if (!writeNv(NvItems::PRECFG_KEY, temp_v))
            return false;

        wasChange = true;
    }
    if (newConfiguration.channels != current_configuration.channels)
    {
        uint32_t channelBitMask = 0;
        for (auto channel : newConfiguration.channels)
            channelBitMask |= (1 << channel);

        std::vector<uint8_t> chan{};
        for (int i = 0; i < 4; i++)
        {
            uint8_t ch = channelBitMask >> i * 8;
            chan.push_back(ch);
        }

        if (!writeNv(NvItems::CHANNEL_LIST, chan)) // старший байт последний
            return false;

        wasChange = true;
    }
    if (wasChange)
    {
        if (!writeNv(NvItems::ZDO_DIRECT_CB, std::vector<uint8_t>{1}))
            return false;

        if (!initNv(NvItems::ZNP_HAS_CONFIGURED, 1, std::vector<uint8_t>{0}))
            return false;

        if (!writeNv(NvItems::ZNP_HAS_CONFIGURED, std::vector<uint8_t>{0x55}))
            return false;
    }
    return true;
}
*/
// результат выполнения синхронной команды ожидается с определенным идентификатором, который формируется из исходного
std::optional<Command> Zdo::syncRequest(Command request, std::chrono::duration<int, std::milli> timeout)
{
    assert(request.type() == Command::Type::SREQ); // убеждаемся, что команда синхронная

    CommandId id = (CommandId)((uint16_t)request.id() | 0b0100000000000000); // идентификатор синхронного ответа
    event_command_.clear(id);                                                // очистка события с идентификатором id

    if (uart_.sendCommandToDevice(request))
    {
        // TODO: а нужна ли здесь команда???
        return event_command_.wait(id, timeout); // ожидаем наступления события с заданным идентификатором
    }
    else
        return std::nullopt;
}

// для асинхронной команды происходит ее отправка,
// ожидаем только синхронный ответ на саму команду (подтверждение, что команда получена устройством и распознана)
// асинхронный ответ (результат исполнения команды) получаем в обработчике команд
bool Zdo::asyncRequest(Command &request, std::chrono::duration<int, std::milli> timeout)
{
    std::optional<Command> response = syncRequest(request, timeout);
    if ((response) && (static_cast<Status>(response->payload(0)) == Status::SUCCESS)) // проверка, что выполнилась синхронная часть команды
        return true;
    else
        return false;
}

// Коннект к компорту адаптера
bool Zdo::connect(std::string port, unsigned int baud_rate)
{
    if (uart_.connect(port, baud_rate))
        return uart_.start();
    else
        return false;
}

// отправка компорту адаптера на отключение
void Zdo::disconnect()
{
    uart_.disconnect();
}

void Zdo::on_disconnect()
{
}

// Получить ендпойнты с устройства
void Zdo::activeEndpoints(zigbee::NetworkAddress address)
{

    Command active_endpoints_request(zigbee::CommandId::ZDO_ACTIVE_EP_REQ);

    active_endpoints_request.payload(0) = LOWBYTE(address); // Specifies NWK address of the device generating the inquiry.
    active_endpoints_request.payload(1) = HIGHBYTE(address);

    active_endpoints_request.payload(2) = LOWBYTE(address); // Specifies NWK address of the destination device being queried.
    active_endpoints_request.payload(3) = HIGHBYTE(address);

    asyncRequest(active_endpoints_request);
}

// Получить дескриптор ендпойнта с устройства
void Zdo::simpleDescriptor(zigbee::NetworkAddress address, unsigned int endpoint_number)
{
    assert(endpoint_number <= 242);

    Command simple_descriptor_request(zigbee::CommandId::ZDO_SIMPLE_DESC_REQ);

    simple_descriptor_request.payload(0) = LOWBYTE(address); // Specifies NWK address of the device generating the inquiry.
    simple_descriptor_request.payload(1) = HIGHBYTE(address);

    simple_descriptor_request.payload(2) = LOWBYTE(address); // Specifies NWK address of the destination device being queried.
    simple_descriptor_request.payload(3) = HIGHBYTE(address);

    simple_descriptor_request.payload(4) = static_cast<uint8_t>(endpoint_number);

    asyncRequest(simple_descriptor_request);
}

bool Zdo::ping()
{
    return (syncRequest(Command(CommandId::SYS_PING)).has_value());
}

// TODO: MAC-адреса надо передавать в обратном порядке
// Длина payload вычисляется и подставляется при передаче команды,
// специально тут не надо делать, только собьет систему с толку
// Все после кода команды пишется в payload
// В текущей версии все устройства связываем только с координатором, поэтому приемные адреса и ендпойнты забиваем жестко
bool Zdo::bind(zigbee::NetworkAddress dst_network_address, zigbee::IEEEAddress src_mac_address, uint8_t src_endpoint, zigbee::zcl::Cluster cluster)
{
    uint64_t dst_mac_address = (uint64_t)mac_address_;
    uint8_t dst_endpoint = 0x01;

    Command bind_request(zigbee::CommandId::ZDO_BIND_REQ);
    {
        size_t i = 0;

        // это адрес связываемого с координатором устройства
        bind_request.payload(i++) = LOWBYTE(dst_network_address);
        bind_request.payload(i++) = HIGHBYTE(dst_network_address);

        uint8_t byte = (uint8_t)src_mac_address;
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(src_mac_address >> 8);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(src_mac_address >> 16);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(src_mac_address >> 24);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(src_mac_address >> 32);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(src_mac_address >> 40);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(src_mac_address >> 48);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(src_mac_address >> 56);
        bind_request.payload(i++) = byte;

        bind_request.payload(i++) = src_endpoint;

        bind_request.payload(i++) = LOWBYTE(cluster);
        bind_request.payload(i++) = HIGHBYTE(cluster);

        bind_request.payload(i++) = 0x03; // ADDRESS_64_BIT BindAddressMode

        byte = (uint8_t)dst_mac_address;
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(dst_mac_address >> 8);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(dst_mac_address >> 16);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(dst_mac_address >> 24);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(dst_mac_address >> 32);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(dst_mac_address >> 40);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(dst_mac_address >> 48);
        bind_request.payload(i++) = byte;
        byte = (uint8_t)(dst_mac_address >> 56);
        bind_request.payload(i++) = byte;

        bind_request.payload(i++) = dst_endpoint;
    }

    return asyncRequest(bind_request);
}

void Zdo::on_command()
{
    while (Flag.load())
    {
        Command cmd = get_command();
        if ((uint16_t)cmd.id() != 0 && Flag.load())
            handle_command(cmd);
    }
}

// то, что передается в ZCL Frame
uint8_t Zdo::generateTransactionSequenceNumber()
{
    std::lock_guard<std::mutex> lk(trans_mutex);
    transaction_sequence_number.store(transaction_sequence_number.load() + 1);
    return transaction_sequence_number.load();
}

///
/// с датчика движения ИКЕА ничего не мог получить
void Zdo::get_attribute_RSSI_Power(zigbee::NetworkAddress address)
{
    zigbee::Cluster cl = zigbee::zcl::Cluster::RSSI;
    zigbee::Endpoint endpoint{address, 1};
    //   uint16_t id = zigbee::zcl::Attributes::RSSI::Power; //
    uint16_t id = zigbee::zcl::Attributes::RSSI::QualityMeasure; //

    // ZCL Header
    Frame frame;
    frame.frame_control.type = zigbee::zcl::FrameType::GLOBAL;
    frame.frame_control.direction = zigbee::zcl::FrameDirection::FROM_CLIENT_TO_SERVER;
    frame.frame_control.disable_default_response = false;
    frame.frame_control.manufacturer_specific = false;
    frame.command = zigbee::zcl::GlobalCommands::READ_ATTRIBUTES; // 0x00
                                                                  //    frame.command = 0x05; // RSSI Request Command
    frame.transaction_sequence_number = generateTransactionSequenceNumber();
    // end ZCL Header

    frame.payload.push_back(LOWBYTE(id));
    frame.payload.push_back(HIGHBYTE(id));
    sendMessage(endpoint, cl, frame);
}

/// @brief Отображение кода команды в строковом виде
/// @param command
/// @return
std::string Zdo::getCommandStr(Command &command)
{
    std::string cmd_str;

    switch (command.id())
    {
    case zigbee::CommandId::SYS_RESET_REQ:
        cmd_str = "SYS_RESET_REQ";
        break; //       0x4100,
    case zigbee::CommandId::SYS_RESET_IND:
        cmd_str = "SYS_RESET_IND";
        break; //        0x4180,
    case zigbee::CommandId::ZDO_STARTUP_FROM_APP:
        cmd_str = "ZDO_STARTUP_FROM_APP";
        break; //    0x2540,
    case zigbee::CommandId::ZDO_STARTUP_FROM_APP_SRSP:
        cmd_str = "ZDO_STARTUP_FROM_APP_SRSP";
        break; //  0x6540,
    case zigbee::CommandId::ZDO_STATE_CHANGE_IND:
        cmd_str = "ZDO_STATE_CHANGE_IND";
        break; //     0x45c0,
    case zigbee::CommandId::AF_REGISTER:
        cmd_str = "AF_REGISTER";
        break; //       0x2400,
    case zigbee::CommandId::AF_REGISTER_SRSP:
        cmd_str = "AF_REGISTER_SRSP";
        break; //      0x6400,
    case zigbee::CommandId::AF_INCOMING_MSG:
        cmd_str = "AF_INCOMING_MSG";
        break; //        0x4481,
    case zigbee::CommandId::ZDO_MGMT_PERMIT_JOIN_REQ:
        cmd_str = "ZDO_MGMT_PERMIT_JOIN_REQ";
        break; //  0x2536,
    case zigbee::CommandId::ZDO_MGMT_PERMIT_JOIN_SRSP:
        cmd_str = "ZDO_MGMT_PERMIT_JOIN_SRSP";
        break; //  0x6536,
    case zigbee::CommandId::ZDO_MGMT_PERMIT_JOIN_RSP:
        cmd_str = "ZDO_MGMT_PERMIT_JOIN_RSP";
        break; //  0x45b6,
    case zigbee::CommandId::ZDO_PERMIT_JOIN_IND:
        cmd_str = "ZDO_PERMIT_JOIN_IND";
        break; //     0x45cb,
    case zigbee::CommandId::ZDO_TC_DEV_IND:
        cmd_str = "ZDO_TC_DEV_IND";
        break; //      0x45ca,
    case zigbee::CommandId::ZDO_LEAVE_IND:
        cmd_str = "ZDO_LEAVE_IND";
        break; //      0x45c9,
    case zigbee::CommandId::ZDO_END_DEVICE_ANNCE_IND:
        cmd_str = "ZDO_END_DEVICE_ANNCE_IND";
        break; //  0x45c1,
    case zigbee::CommandId::ZDO_ACTIVE_EP_REQ:
        cmd_str = "ZDO_ACTIVE_EP_REQ";
        break; //     0x2505,
    case zigbee::CommandId::ZDO_ACTIVE_EP_SRSP:
        cmd_str = "ZDO_ACTIVE_EP_SRSP";
        break; //      0x6505,
    case zigbee::CommandId::ZDO_ACTIVE_EP_RSP:
        cmd_str = "ZDO_ACTIVE_EP_RSP";
        break; //      0x4585,
    case zigbee::CommandId::ZDO_SIMPLE_DESC_REQ:
        cmd_str = "ZDO_SIMPLE_DESC_REQ";
        break; //      0x2504,
    case zigbee::CommandId::ZDO_SIMPLE_DESC_SRSP:
        cmd_str = "ZDO_SIMPLE_DESC_SRSP";
        break; //    0x6504,
    case zigbee::CommandId::ZDO_SIMPLE_DESC_RSP:
        cmd_str = "ZDO_SIMPLE_DESC_RSP";
        break; //     0x4584,
    case zigbee::CommandId::ZDO_POWER_DESC_REQ:
        cmd_str = "ZDO_POWER_DESC_REQ";
        break; //      0x2503,
    case zigbee::CommandId::ZDO_POWER_DESC_SRSP:
        cmd_str = "ZDO_POWER_DESC_SRSP";
        break; //    0x6503,
    case zigbee::CommandId::ZDO_POWER_DESC_RSP:
        cmd_str = "ZDO_POWER_DESC_RSP";
        break;                               //     0x4583,
    case zigbee::CommandId::ZDO_SRC_RTG_IND: // 0x45c4
        cmd_str = "ZDO_SRC_RTG_IND";
        break;
    case zigbee::CommandId::SYS_PING:
        cmd_str = "SYS_PING";
        break; //      0x2101,
    case zigbee::CommandId::SYS_PING_SRSP:
        cmd_str = "SYS_PING_SRSP";
        break; //      0x6101,
    case zigbee::CommandId::SYS_OSAL_NV_READ:
        cmd_str = "SYS_OSAL_NV_READ";
        break; // 0x2108,
    case zigbee::CommandId::SYS_OSAL_NV_READ_SRSP:
        cmd_str = "SYS_OSAL_NV_READ_SRSP";
        break; //    0x6108,
    case zigbee::CommandId::SYS_OSAL_NV_WRITE:
        cmd_str = "SYS_OSAL_NV_WRITE";
        break; //    0x2109,
    case zigbee::CommandId::SYS_OSAL_NV_WRITE_SRSP:
        cmd_str = "SYS_OSAL_NV_WRITE_SRSP";
        break; //   0x6109,
    case zigbee::CommandId::SYS_OSAL_NV_ITEM_INIT:
        cmd_str = "SYS_OSAL_NV_ITEM_INIT";
        break; //    0x2107,
    case zigbee::CommandId::SYS_OSAL_NV_ITEM_INIT_SRSP:
        cmd_str = "SYS_OSAL_NV_ITEM_INIT_SRSP";
        break;                                  //  0x6107,
    case zigbee::CommandId::SYS_OSAL_NV_LENGTH: //   0x2113:
        cmd_str = "SYS_OSAL_NV_LENGTH";
        break; //
    case zigbee::CommandId::SYS_OSAL_NV_LENGTH_SRSP:
        cmd_str = "SYS_OSAL_NV_LENGTH_SRSP";
        break;                                  //  0x6113,
    case zigbee::CommandId::SYS_OSAL_NV_DELETE: //   0x2112:
        cmd_str = "SYS_OSAL_NV_DELETE";
        break; //
    case zigbee::CommandId::SYS_OSAL_NV_DELETE_SRSP:
        cmd_str = "SYS_OSAL_NV_DELETE_SRSP";
        break; //  0x6112,
    case zigbee::CommandId::UTIL_GET_DEVICE_INFO:
        cmd_str = "UTIL_GET_DEVICE_INFO";
        break; //    0x2700,
    case zigbee::CommandId::UTIL_GET_DEVICE_INFO_SRSP:
        cmd_str = "UTIL_GET_DEVICE_INFO_SRSP";
        break; //  0x6700,
    case zigbee::CommandId::SYS_SET_TX_POWER:
        cmd_str = "SYS_SET_TX_POWER";
        break; //      0x2114,
    case zigbee::CommandId::SYS_SET_TX_POWER_SRSP:
        cmd_str = "SYS_SET_TX_POWER_SRSP";
        break; //     0x6114,
    case zigbee::CommandId::SYS_VERSION:
        cmd_str = "SYS_VERSION";
        break; //           0x2102,
    case zigbee::CommandId::SYS_VERSION_SRSP:
        cmd_str = "SYS_VERSION_SRSP";
        break; //       0x6102,
    case zigbee::CommandId::AF_DATA_REQUEST:
        cmd_str = "AF_DATA_REQUEST";
        break; //      0x2401,
    case zigbee::CommandId::AF_DATA_REQUEST_SRSP:
        cmd_str = "AF_DATA_REQUEST_SRSP";
        break; //     0x6401,
    case zigbee::CommandId::AF_DATA_CONFIRM:
        cmd_str = "AF_DATA_CONFIRM";
        break; //       0x4480,
    case zigbee::CommandId::ZDO_BIND_REQ:
        cmd_str = "ZDO_BIND_REQ";
        break; //      0x2521,
    case zigbee::CommandId::ZDO_UNBIND_REQ:
        cmd_str = "ZDO_UNBIND_REQ";
        break; // 0x2522,
    case zigbee::CommandId::ZDO_BIND_RSP:
        cmd_str = "ZDO_BIND_RSP";
        break; //  0x45a1,
    case zigbee::CommandId::ZDO_BIND_SRSP:
        cmd_str = "ZDO_BIND_SRSP";
        break; //  0x65a1,
    case zigbee::CommandId::ZDO_UNBIND_RSP:
        cmd_str = "ZDO_UNBIND_RSP";
        break; //         0x45a2
    case zigbee::CommandId::ZDO_IEEE_ADDR_REQ:
        cmd_str = "ZDO_IEEE_ADDR_REQ";
        break; // 0x2501,
    case zigbee::CommandId::ZDO_IEEE_ADDR_REQ_SRSP:
        cmd_str = "ZDO_IEEE_ADDR_REQ_SRSP";
        break; // 0x6501,

    default:
        cmd_str = "Unknown command " + std::to_string((uint16_t)command.id());
    }
    return cmd_str;
}

std::string Zdo::get_cluster_string(zcl::Cluster cl)
{
    std::string result;

    switch (cl)
    {
    case zcl::Cluster::BASIC: // 0x0000
        return "BASIC";
        break;
    case zcl::Cluster::POWER_CONFIGURATION: // 0x0001
        return "POWER_CONFIGURATION";
        break;
    case zcl::Cluster::DEVICE_TEMPERATURE_CONFIGURATION: // 0x0002
        return "DEVICE_TEMPERATURE_CONFIGURATION";
        break;
    case zcl::Cluster::IDENTIFY: // 0x0003
        return "IDENTIFY";
        break;
    case zcl::Cluster::GROUPS: // 0x0004
        return "GROUPS";
    case zcl::Cluster::SCENES: // 0x0005
        return "SCENES";
        break;
    case zcl::Cluster::ON_OFF: // 0x0006
        return "ON_OFF";
        break;
    case zcl::Cluster::ON_OFF_SWITCH_CONFIGURATION: // 0x0007
        return "ON_OFF_SWITCH_CONFIGURATION";
        break;
    case zcl::Cluster::LEVEL_CONTROL: // 0x0008
        return "LEVEL_CONTROL";
        break;
    case zcl::Cluster::ALARMS: // 0x0009
        return "ALARMS";
        break;
    case zcl::Cluster::TIME: // 0x000a
        return "TIME";
        break;
    case zcl::Cluster::RSSI: // 0x000b
        return "RSSI";
        break;
    case zcl::Cluster::ANALOG_INPUT: // 0x000c
        return "ANALOG_INPUT";
        break;
    case zcl::Cluster::ANALOG_OUTPUT: // 0x000d
        return "ANALOG_OUTPUT";
        break;
    case zcl::Cluster::ANALOG_VALUE: // 0x000e
        return "ANALOG_VALUE";
        break;
    case zcl::Cluster::BINARY_INPUT: // 0x000f
        return "BINARY_INPUT";
        break;
    case zcl::Cluster::BINARY_OUTPUT: // 0x0010
        return "BINARY_OUTPUT";
        break;
    case zcl::Cluster::BINARY_VALUE: // 0x0011
        return "BINARY_VALUE";
        break;
    case zcl::Cluster::MULTISTATE_INPUT: // 0x0012
        return "MULTISTATE_INPUT";
        break;
    case zcl::Cluster::MULTISTATE_OUTPUT: // 0x0013
        return "MULTISTATE_OUTPUT";
        break;
    case zcl::Cluster::MULTISTATE_VALUE: // 0x0014
        return "MULTISTATE_VALUE";
        break;
    case zcl::Cluster::OTA: // 0x0019
        return "OTA";
        break;
    case zcl::Cluster::POWER_PROFILE: // 0x001a //
        return "POWER_PROFILE";
        break;
    case zcl::Cluster::PULSE_WIDTH_MODULATION: // 0x001c
        return "PULSE_WIDTH_MODULATION";
        break;
    case zcl::Cluster::POLL_CONTROL: // 0x0020 //
        return "POLL_CONTROL";
        break;
    case zcl::Cluster::XIAOMI_SWITCH_OUTPUT: // 0x0021
        return "XIAOMI_SWITCH_OUTPUT";
        break;
    case zcl::Cluster::KEEP_ALIVE: // 0x0025
        return "KEEP_ALIVE";
        break;
    case zcl::Cluster::WINDOW_COVERING: // 0x0102
        return "WINDOW_COVERING";
        break;
    case zcl::Cluster::TEMPERATURE_MEASUREMENT: // 0x0402
        return "TEMPERATURE_MEASUREMENT";
        break;
    case zcl::Cluster::HUMIDITY_MEASUREMENT: // 0x0405
        return "HUMIDITY_MEASUREMENT";
        break;
    case zcl::Cluster::ILLUMINANCE_MEASUREMENT: // 0x0400
        return "ILLUMINANCE_MEASUREMENT";
        break;
    case zcl::Cluster::IAS_ZONE: // 0x0500
        return "IAS_ZONE";
        break;
    case zcl::Cluster::SIMPLE_METERING: // 0x0702
        return "SIMPLE_METERING";
        break;
    case zcl::Cluster::METER_IDENTIFICATION: // 0x0b01
        return "METER_IDENTIFICATION";
        break;
    case zcl::Cluster::ELECTRICAL_MEASUREMENTS: // 0x0b04
        return "ELECTRICAL_MEASUREMENTS";
        break;
    case zcl::Cluster::DIAGNOSTICS: // 0x0b05
        return "DIAGNOSTICS";
        break;
    case zcl::Cluster::TOUCHLINK_COMISSIONING: // 0x1000
        return "TOUCHLINK_COMISSIONING";
        break;
    case zcl::Cluster::TUYA_SWITCH_MODE_0: // 0xe000
        return "TUYA_SWITCH_MODE_0";
        break;
    case zcl::Cluster::TUYA_ELECTRICIAN_PRIVATE_CLUSTER: // 0xe001
        return "TUYA_ELECTRICIAN_PRIVATE_CLUSTER";
        break;
    case zcl::Cluster::IKEA_BUTTON_ATTR_UNKNOWN2: // 0xfc7c
        return "IKEA_BUTTON_ATTR_UNKNOWN2";
        break;
    case zcl::Cluster::XIAOMI_SWITCH: // 0xfcc0
        return "XIAOMI_SWITCH";
        break;
    case zcl::Cluster::SMOKE_SENSOR: // 0xfe00
        return "SMOKE_SENSOR";
        break;
    default:
        return "UNKNOWN_CLUSTER";
        break;
    }
}
