#ifndef GSB_APP_H
#define GSB_APP_H

#include "pi4-gpio.h"

class App
{
public:
    App(){};
    ~App(){};
    bool object_create();
    bool startApp();
    void stopApp();
    bool init_modem();
    int cmd_func();
    std::shared_ptr<gsbutils::CycleTimer> timer1Min;
    static void timer1min();
    std::shared_ptr<zigbee::Zhub> zhub;
    std::shared_ptr<GsmModem> gsmModem;
    std::shared_ptr<gsbutils::ThreadPool<std::vector<uint8_t>>> tpm;
    std::string show_sim800_battery();
    void exposer_handler();
    char *program_version = nullptr;
    std::atomic<bool> Flag{true};
    std::string startTime{};
    bool with_sim800 = false;
    bool noAdapter;
    std::thread cmdThread;     // поток приема команд с клавиатуры
    std::thread exposerThread; // поток ответа прометею
    std::thread httpThread;

    void get_main_temperature();
    float get_board_temperature();
    static void handle_power_off(int value);
    void handle_board_temperature(float temp);
    void fan(bool work);
    void ringer();
    std::shared_ptr<Pi4Gpio> gpio;

    std::shared_ptr<Tlg32> tlg32;
    std::shared_ptr<gsbutils::Channel<TlgMessage>> tlg_in, tlg_out;
    std::thread *tlgInThread;
    void handle();
    
    std::string show_statuses();
};
#endif
