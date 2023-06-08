#ifndef GSB_PI4_GPIO_H
#define GSB_PI4_GPIO_H

void get_main_temperature();
void power_detect();
    float get_board_temperature();
int initialize_gpio();
int close_gpio();
int read_pin(int pin);
int write_pin(int pin, int value);

#endif
