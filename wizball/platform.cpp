#include "platform.h"

#include <SDL.h>

unsigned int PLATFORM_get_wall_time_ms(void)
{
	return (unsigned int)SDL_GetTicks();
}

int PLATFORM_install_timer_system(void)
{
	if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0)
	{
		return -1;
	}
	return 0;
}

int PLATFORM_install_timer_callback_ms(void (*callback)(void), int ms)
{
    return SDL_AddTimer(ms, [](Uint32 interval, void *param) -> Uint32 {
        // Correctly cast the parameter to the callback function type and call it
        reinterpret_cast<void (*)(void)>(param)();
        return interval; // Continue the timer
    }, reinterpret_cast<void *>(callback));
}

int PLATFORM_install_timer_callback_bps(void (*callback)(void), int bps)
{
	int ms = 1000 / bps;
  return PLATFORM_install_timer_callback_ms(callback, ms);
	// install_int_ex(callback, BPS_TO_TIMER(bps));
}

void PLATFORM_remove_timer_callback(int time_id)
{
	SDL_RemoveTimer(time_id);
}

void PLATFORM_sleep_ms(int ms)
{
	SDL_Delay(ms);
}
