#include <memory>
#include <algorithm>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <vector>
#include <syslog.h>
#include <array>
#include <set>
#include <queue>
#include <optional>
#include <any>
#include <string.h>
#include <termios.h>

#include "../version.h"
#include "../comport/unix.h"
#include "../comport/serial.h"
#include "../../gsb_utils/gsbutils.h"
#include "../common.h"
#include "zigbee.h"
#include "../modem.h"


using std::cerr;
using std::cout;
using std::string;
using std::vector;


using namespace zigbee;

Uart::Uart(std::shared_ptr<gsbutils::Channel<Command>> chan_out_, std::shared_ptr<gsbutils::Channel<Command>> chan_in_)
{
    flag.store(true);
    chan_out = chan_out_;
    chan_in = chan_in_;
    rx_buff_.reserve(RX_BUFFER_SIZE);
    tx_buff_.reserve(TX_BUFFER_SIZE);
}

Uart::~Uart()
{
    flag.store(false);
    if (receiver_thread_.joinable())
        receiver_thread_.join();
    if (send_thread_.joinable())
        send_thread_.join();
    serial_->close();
}

bool Uart::connect(std::string port, unsigned int baud_rate)
{
    serial_->setPort(port);
    serial_->setBaudrate(baud_rate);
    //    serial::Timeout timeout(std::numeric_limits<uint32_t>::max(), 250, 250);
    //    serial_->setTimeout(timeout);

    try
    {
        serial_->open();
        return serial_->isOpen();
    }
    catch (std::exception &error)
    {
        gsbutils::dprintf(1, "Uart::connect: %s\n", error.what());
    }
    return false;
}

// старт потоков приема/отправки команд
bool Uart::start()
{
    // старт потока приема команд
    receiver_thread_ = std::thread(&Uart::loop, this);
    send_thread_ = std::thread(&Uart::snd_loop, this);
    return true;
}

//
void Uart::disconnect()
{
    try
    {
        if (serial_ && serial_->isOpen())
        {
            serial_->close();
        }
    }
    catch (std::system_error &error)
    {
        std::cout << "error while disconnect " << error.what() << std::endl;
    }
    catch (std::exception &error)
    {
        std::cout << "error while disconnect " << error.what() << std::endl;
    }
}

/// Отправляющий команды поток
// с версии 2.24.571 использую канал вывода команд
void Uart::snd_loop()
{
    while (flag.load())
    {
        Command cmd = chan_out->read(); // read next command from ouput channel
        if ((uint16_t)cmd.id() != 0)
            send_command_to_device(cmd);
    }
}

vector<Command> Uart::parseReceivedData(vector<uint8_t> &data)
{
    vector<Command> parsed_commands{};

    try
    {
        for (size_t i = 0; i < data.size();)
        {
            if (i >= data.size())
                break;
            if (data.at(i++) == SOF) // SOF == 0xFE
            {
                if (i >= data.size())
                    break;
                size_t payload_length = data.at(i++);
                if (i >= data.size())
                    break;

                uint8_t cmd0 = data.at(i++);
                if (i >= data.size())
                    break;
                uint8_t cmd1 = data.at(i++);
                if (i >= data.size())
                    break;

                if ((payload_length <= PAYLOAD_MAX_LENGTH) && ((i + payload_length) <= data.size()))
                {
                    Command command(CommandId(_UINT16(cmd1, cmd0)), payload_length);

                    std::copy_n(&data[i], payload_length, command.data()); // из data в command.data
                    i += payload_length;
                    if (i >= data.size())
                        break;
                    if (data.at(i++) == command.fcs())
                    {
                        parsed_commands.push_back(command);
                    }
                }
            }
        }
    }
    catch (std::out_of_range &ofr)
    {
        gsbutils::dprintf(1, "Bad received data (Uart) %s \n", ofr.what()); // TODO: OnError
    }
    catch (std::exception &e)
    {
        gsbutils::dprintf(1, "Bad received data (Uart) %s \n", e.what()); // TODO: OnError
    }

    return parsed_commands;
}

/// фактическая отправка команды в координатор
bool Uart::send_command_to_device(Command command)
{
    if (!serial_->isOpen())
        return false;

    bool res = false;
    try
    {
        std::lock_guard<std::mutex> transmit_lock(transmit_mutex_);

        tx_buff_.clear();
        tx_buff_.push_back(SOF);                             // SOF
        tx_buff_.push_back(LOWBYTE(command.payload_size())); // Length
        tx_buff_.push_back(HIGHBYTE(command.id()));          // CMD0
        tx_buff_.push_back(LOWBYTE(command.id()));           // CMD1

        std::copy_n(command.data(), command.payload_size(), std::back_inserter(tx_buff_)); // Payload

        tx_buff_.push_back(command.fcs()); // FCS
        size_t sent = serial_->write(tx_buff_);
        res = (sent == tx_buff_.size());
        return res;
    }
    catch (std::exception &e)
    {
        gsbutils::dprintf(1, "send_command_to_device exception: %s\n", e.what());
    }

    return false;
}

void Uart::loop()
{
    while (flag.load())
    {
        try
        {
            if (serial_->waitReadable()) // здесь тоже может быть исключение
            {
                rx_buff_.resize(rx_buff_.capacity());
                rx_buff_.clear();
                rx_buff_.resize(serial_->read(rx_buff_, rx_buff_.capacity()));

                for (Command command : parseReceivedData(rx_buff_))
                {
                    chan_in->write(command);
                }
            }
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        catch (std::exception &error)
        {
            gsbutils::dprintf(1, "loop exception: %s\n", error.what());
            throw;
        }
    }
}
