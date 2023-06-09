#ifndef GSB_PI4_GPIO_H
#define GSB_PI4_GPIO_H


void initialize_gpio();
void close_gpio();
int read_pin(int pin);
int write_pin(int pin, int value);
void power_detect();


#endif
