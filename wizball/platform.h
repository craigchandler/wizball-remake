#ifndef _PLATFORM_H_
#define _PLATFORM_H_

unsigned int PLATFORM_get_wall_time_ms(void);
int PLATFORM_install_timer_system(void);
int PLATFORM_install_timer_callback_ms(void (*callback)(void), int ms);
int PLATFORM_install_timer_callback_bps(void (*callback)(void), int bps);
void PLATFORM_remove_timer_callback(int time_id);
void PLATFORM_sleep_ms(int ms);

#endif
