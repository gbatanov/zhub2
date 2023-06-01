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

    Command() : Command(0, 0) {}

    Command(uint16_t id) : id_(id)
    {
        payload_ = std::make_shared<std::vector<uint8_t>>();
        payload_->reserve(PAYLOAD_MAX_LENGTH);
    }

    Command(uint16_t id, size_t payload_length) : id_(id)
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
    inline uint16_t id() { return id_; }
    inline size_t payload_size() { return payload_->size(); }
    inline uint8_t *data() { return payload_->data(); }

private:
    std::shared_ptr<std::vector<uint8_t>> payload_; // дополнительная информация в команде, может отсутствовать
    uint16_t id_;                                   // идентификатор команды состоит из старшего и младшего байтов команды (CMD1 CMD0)
                                                    // 3 старших бита в cmd1 - это тип команды, 5 младших - подсистема команд
};

#endif
