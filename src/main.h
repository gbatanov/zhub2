#ifndef MAIN_H
#define MAIN_H

std::string show_statuses();
void timer1min();
void ikeaMotionTimerCallback();

#ifdef IS_PI
void get_main_temperature();
void power_detect();

int initialize_gpio();
int close_gpio();
int read_pin(int pin);
int write_pin(int pin, int value);
#endif

#endif
