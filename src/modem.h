// pi4.h
#ifndef PI4_GPIO_H
#define PI4_GPIO_H

#include <thread>
#include <string>

#include "main.h"
#include "comport/serial.h"
#include "zigbee/zigbee.h"

int initialize_gpio();
int close_gpio();
int read_pin(int pin);
int write_pin(int pin, int value);

void execute_tone_command(std::string command);

#ifdef WITH_SIM800

#define PI_RX_BUFFER_SIZE 1024
#define PI_TX_BUFFER_SIZE 256

class GsmModem
{
public:
    GsmModem();
    ~GsmModem();
    bool connect(std::string port, unsigned int baud_rate = 9600);
    void disconnect();
    std::atomic<bool> execute_flag_;
    std::string send_command(std::string command, std::string id);
    bool init();
    std::array<int, 3> get_battery_level(bool need_query = true);
    bool get_balance();
    std::string show_balance() { return balance_; };
    bool master_call();
    bool send_sms(std::string sms);

private:
    std::unique_ptr<serial::Serial> serial_ = std::make_unique<serial::Serial>();

    std::string parseReceivedData(std::vector<uint8_t> &data);
    void loop();
    void OnDisconnect();
    void on_tone_command(std::string answer);
    void on_sms_command(std::string answer);
    void on_sms(std::string answer);
    bool send_command_(std::string command);
    bool set_echo();
    bool set_echo(bool);
    bool set_aon();
    std::string convert_sms(std::string msg);
    bool is_call_; // есть принятый входящий звонок
    std::string tone_cmd;
    bool tone_cmd_started;
    std::thread receiver_thread_;
    std::vector<uint8_t> tx_buff_;
    std::vector<uint8_t> rx_buff_;
    std::mutex transmit_mutex_;
    zigbee::Sim800Event sim800_event_emitter_;
    std::string balance_{"Не  определен"};
    int charge_ = -1;
    int level_ = -1;
    int voltage_ = -1;
};
#endif

#endif