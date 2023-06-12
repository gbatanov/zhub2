#ifndef GSB_PI4_GPIO_H
#define GSB_PI4_GPIO_H
#include <atomic>
#include <thread>

typedef void (*power_func)(int);

class Pi4Gpio
{
public:
    Pi4Gpio();
    ~Pi4Gpio();

    bool initialize_gpio(power_func power);
    void close_gpio();
    int read_pin(int pin);
    int write_pin(int pin, int value);
    void power_detect();

private:
    struct gpiod_chip *chip = nullptr;
    std::atomic<bool> flag;
    power_func power_;
    std::thread pwr_thread;
};

#endif
