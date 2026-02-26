#include <allegro.h>

#include "platform.h"

#ifdef WIZBALL_USE_SDL2
#include <SDL.h>
#endif

#ifdef ALLEGRO_WINDOWS
#include <windows.h>
#endif

#if !defined(ALLEGRO_WINDOWS) && !defined(WIZBALL_USE_SDL2)
#include <sys/time.h>
#endif

unsigned int PLATFORM_get_wall_time_ms(void)
{
#ifdef WIZBALL_USE_SDL2
	return (unsigned int)SDL_GetTicks();
#elif defined(ALLEGRO_WINDOWS)
	return (unsigned int)GetTickCount();
#else
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (unsigned int)((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
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
