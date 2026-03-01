#include <allegro.h>

#include "platform.h"

#include <SDL.h>

#ifdef ALLEGRO_WINDOWS
#include <windows.h>
#endif

unsigned int PLATFORM_get_wall_time_ms(void)
{
	return (unsigned int)SDL_GetTicks();
}

int PLATFORM_install_timer_system(void)
{
	return install_timer();
}

void PLATFORM_install_timer_callback_ms(void (*callback)(void), int ms)
{
	install_int(callback, ms);
}

void PLATFORM_install_timer_callback_bps(void (*callback)(void), int bps)
{
	install_int_ex(callback, BPS_TO_TIMER(bps));
}

void PLATFORM_remove_timer_callback(void (*callback)(void))
{
	remove_int(callback);
}

void PLATFORM_sleep_ms(int ms)
{
	rest(ms);
}
