#ifndef GSB_PI4_GPIO_H
#define GSB_PI4_GPIO_H

class Pi4Gpio
{
public:
    Pi4Gpio();
    ~Pi4Gpio();

    int read_pin(int pin);
    int write_pin(int pin, int value);
    void power_detect();

private:
    void initialize_gpio();
    void close_gpio();
    struct gpiod_chip *chip = nullptr;
};

#endif
