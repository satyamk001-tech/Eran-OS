#ifndef ERAN_DRIVERS_TIMER_H
#define ERAN_DRIVERS_TIMER_H

#include <stdint.h>

void timer_init(uint32_t frequency);
uint32_t timer_get_ticks(void);
uint32_t timer_get_uptime_seconds(void);
void sleep_ticks(uint32_t ticks);
void sleep_ms(uint32_t ms);

#endif /* ERAN_DRIVERS_TIMER_H */
