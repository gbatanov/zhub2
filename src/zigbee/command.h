#ifndef COMMAND_H
#define COMMAND_H

#include <cstdint>
#include <assert.h>

#define PAYLOAD_MAX_LENGTH 250

enum class Subsystem
{
    RPC_ERROR = 0,
    SYS = 1,
    MAC = 2,
    NWK = 3,
    AF = 4,
    ZDO = 5,
    SAPI = 6,
    UTIL = 7,
    DEBURG = 8,
    APP = 9,
    APP_CNF = 15,
    GREENPOWER = 21
};

enum class CommandId
{
    // SYS
    SYS_RESET_REQ = 0x4100,
    SYS_RESET_IND = 0x4180,
    SYS_PING = 0x2101,
    SYS_PING_SRSP = 0x6101,
    SYS_OSAL_NV_READ = 0x2108,
    SYS_OSAL_NV_READ_SRSP = 0x6108,
    SYS_OSAL_NV_WRITE = 0x2109,
    SYS_OSAL_NV_WRITE_SRSP = 0x6109,
    SYS_OSAL_NV_ITEM_INIT = 0x2107,
    SYS_OSAL_NV_ITEM_INIT_SRSP = 0x6107,
    SYS_OSAL_NV_LENGTH = 0x2113,
    SYS_OSAL_NV_LENGTH_SRSP = 0x6113,
    SYS_OSAL_NV_DELETE = 0x2112,
    SYS_OSAL_NV_DELETE_SRSP = 0x6112,
    SYS_SET_TX_POWER = 0x2114,
    SYS_SET_TX_POWER_SRSP = 0x6114,
    SYS_VERSION = 0x2102,
    SYS_VERSION_SRSP = 0x6102,
    // ZDO
    ZDO_STARTUP_FROM_APP = 0x2540,
    ZDO_STARTUP_FROM_APP_SRSP = 0x6540,
    ZDO_STATE_CHANGE_IND = 0x45c0,
    ZDO_BIND_REQ = 0x2521,
    ZDO_BIND_RSP = 0x45a1,
    ZDO_BIND_SRSP = 0x6521,
    ZDO_UNBIND_REQ = 0x2522,
    ZDO_UNBIND_RSP = 0x45a2,
    ZDO_MGMT_LQI_REQ = 0x2531,
    ZDO_MGMT_LQI_SRSP = 0x6531,
    ZDO_MGMT_LQI_RSP = 0x45b1,
    ZDO_SRC_RTG_IND = 0x45C4,
    ZDO_MGMT_PERMIT_JOIN_REQ = 0x2536,
    ZDO_MGMT_PERMIT_JOIN_SRSP = 0x6536,
    ZDO_MGMT_PERMIT_JOIN_RSP = 0x45b6,
    ZDO_PERMIT_JOIN_IND = 0x45cb,
    ZDO_TC_DEV_IND = 0x45ca,
    ZDO_LEAVE_IND = 0x45c9,
    ZDO_END_DEVICE_ANNCE_IND = 0x45c1,
    ZDO_ACTIVE_EP_REQ = 0x2505,
    ZDO_ACTIVE_EP_SRSP = 0x6505,
    ZDO_ACTIVE_EP_RSP = 0x4585,
    ZDO_SIMPLE_DESC_REQ = 0x2504,
    ZDO_SIMPLE_DESC_SRSP = 0x6504,
    ZDO_SIMPLE_DESC_RSP = 0x4584,
    ZDO_POWER_DESC_REQ = 0x2503,
    ZDO_POWER_DESC_SRSP = 0x6503,
    ZDO_POWER_DESC_RSP = 0x4583,
    ZDO_IEEE_ADDR_REQ = 0x2501,
    ZDO_IEEE_ADDR_REQ_SRSP = 0x6501,
    ZDO_IEEE_ADDR_RSP = 0x4581,
    // AF
    AF_REGISTER = 0x2400,
    AF_REGISTER_SRSP = 0x6400,
    AF_INCOMING_MSG = 0x4481,
    AF_DATA_REQUEST = 0x2401,
    AF_DATA_REQUEST_SRSP = 0x6401,
    AF_DATA_CONFIRM = 0x4480,
    // UTIL
    UTIL_GET_DEVICE_INFO = 0x2700,
    UTIL_GET_DEVICE_INFO_SRSP = 0x6700,
    // ZB
    ZB_GET_DEVICE_INFO = 0x2606,
    ZB_GET_DEVICE_INFO_SRSP = 0x6606,
    UNKNOWN_45B8 = 0x45b8,
    UNKNOWN_4F80 = 0x4f80
};

class Command
{
public:
    enum class Type
    {
        POLL = 0,
        SREQ = 1,
        AREQ = 2,
        SRSP = 3
    };

    Command() : Command((CommandId)0, 0) {}

    Command(CommandId id) : id_(id)
    {
        payload_ = std::make_shared<std::vector<uint8_t>>();
        payload_->reserve(PAYLOAD_MAX_LENGTH);
    }

    Command(CommandId id, size_t payload_length) : id_(id)
    {
        assert(payload_length < PAYLOAD_MAX_LENGTH);

        if (payload_length > PAYLOAD_MAX_LENGTH)
            payload_length = PAYLOAD_MAX_LENGTH;

        payload_ = std::make_shared<std::vector<uint8_t>>();
        payload_->resize(payload_length);
    }

    ~Command() {}

    // получение payload данных по индексу
    // если индекс больше размера payload, то payload увеличивается до нужного размера
    inline uint8_t &payload(const size_t index)
    {
        assert(index < PAYLOAD_MAX_LENGTH);

        if (index > PAYLOAD_MAX_LENGTH)
            return payload_->at(PAYLOAD_MAX_LENGTH);

        if (index >= payload_->size())
            payload_->resize(index + 1);

        return payload_->at(index);
    }

    // вычисление контрольной суммы команды
    // в вычислении участвуют длина payload, старший и младший байты команды и байты payload (если есть)
    uint8_t fcs()
    {
        uint8_t _fcs = static_cast<uint8_t>(payload_->size()) ^ HIGHBYTE(id_) ^ LOWBYTE(id_);

        for (uint8_t byte : *payload_)
            _fcs ^= byte;

        return _fcs;
    }

    inline bool operator==(Command const &command) { return (command.id_ == id_); }
    inline bool operator!=(Command const &command) { return (command.id_ != id_); }
    inline Type type() { return static_cast<Type>(HIGHBYTE(id_) >> 5); }
    inline Subsystem subsystem() { return static_cast<Subsystem>(HIGHBYTE(id_) & 0b00011111); }
    inline CommandId id() { return id_; }
    inline uint16_t uid() { return (uint16_t)id_; }
    inline size_t payload_size() { return payload_->size(); }
    inline uint8_t *data() { return payload_->data(); }

private:
    std::shared_ptr<std::vector<uint8_t>> payload_; // дополнительная информация в команде, может отсутствовать
    CommandId id_;                                  // идентификатор команды состоит из старшего и младшего байтов команды (CMD1 CMD0)
                                                    // 3 старших бита в cmd1 - это тип команды, 5 младших - подсистема команд
};

#endif
