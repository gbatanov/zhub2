#ifndef MAIN_H
#define MAIN_H


std::string show_statuses();
void timer1min();
void ikeaMotionTimerCallback();

#ifdef IS_PI
void get_main_temperature();
void power_detect();
#endif

#endif
