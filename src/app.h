#ifndef GSB_APP_H
#define GSB_APP_H

#include <array>
#include <memory>
#include <sstream>
#include <string>
#include <fstream>
#include <fcntl.h>
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

#include "../telebot32/src/tlg32.h"
#include "../gsb_utils/gsbutils.h"
#include "pi4-gpio.h"
#include "http.h"
#include "httpserver.h"
#include "exposer.h"
#include "modem.h"
#include "zigbee/zigbee.h"

struct GlobalConfig
{
    // working mode
    std::string Mode;
    // telegram bot
    std::string BotName;
    int64_t MyId;
    std::string TokenPath;
    // map short address to mac address
    std::string MapPath;
    // channels
    std::vector<uint8_t> Channels;
    // serial port
    std::string Port;
    // operating system
    std::string Os;
    // modem
    bool Sim800;
    std::string PortModem;
    std::string PhoneNumber;
    // using Prometheus
    bool Prometheus;
    // using gpio
    bool Gpio;
};

class App
{
public:
    App(){};
    ~App(){};
    bool object_create();
    bool start_app();
    void stop_app();
    bool parse_config();
    float get_board_temperature();
    void ringer();
    std::string show_sim800_battery();

private:
    bool init_modem();
    int cmd_func();
    void exposer_handler();
    void get_main_temperature();
    static void handle_power_off(int value);
    void handle_board_temperature(float temp);
    void fan(bool work);
    void handle();
    std::string show_statuses();

public:
    std::string startTime{};
    GlobalConfig config;
    std::atomic<bool> Flag{true};
    std::shared_ptr<zigbee::Zhub> zhub;
    std::shared_ptr<GsmModem> gsmModem;
    bool withSim800 = false;
    std::shared_ptr<Tlg32> tlg32;
    std::shared_ptr<gsbutils::ThreadPool<std::vector<uint8_t>>> tpm;

private:
    std::shared_ptr<Pi4Gpio> gpio;
    std::shared_ptr<gsbutils::Channel<TlgMessage>> tlgIn, tlgOut;
    std::thread *tlgInThread;
    std::thread tempr_thread;
    bool noAdapter;
    std::thread cmdThread;     // поток приема команд с клавиатуры
    std::thread exposerThread; // поток ответа прометею
    std::thread httpThread;
};
#endif
