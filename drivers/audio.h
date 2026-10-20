#ifndef ERAN_DRIVERS_AUDIO_H
#define ERAN_DRIVERS_AUDIO_H

#include <stdint.h>

void audio_init(void);
void audio_play_tone(uint32_t freq_hz, uint32_t duration_ms);
void audio_beep(void);
void audio_chime(void);
void audio_stop(void);
void audio_print_status(void);

#endif /* ERAN_DRIVERS_AUDIO_H */
