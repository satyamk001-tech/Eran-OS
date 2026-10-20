#include "audio.h"
#include "../hal/io.h"
#include "../drivers/vga.h"
#include "../drivers/timer.h"

#define PIT_CH2_DATA   0x42
#define PIT_COMMAND    0x43
#define SPEAKER_PORT   0x61
#define PIT_BASE_FREQ  1193180

static int audio_enabled = 0;
static uint32_t last_played_freq = 0;

void audio_init(void) {
    audio_enabled = 1;
    audio_stop();
}

static void audio_start_hardware_tone(uint32_t freq_hz) {
    if (freq_hz == 0) {
        audio_stop();
        return;
    }

    uint32_t div = PIT_BASE_FREQ / freq_hz;
    if (div > 0xFFFF) div = 0xFFFF;

    // Set PIT Channel 2 to mode 3 (square wave generator)
    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CH2_DATA, (uint8_t)(div & 0xFF));
    outb(PIT_CH2_DATA, (uint8_t)((div >> 8) & 0xFF));

    // Enable speaker gate and data output (bits 0 and 1 of port 0x61)
    uint8_t tmp = inb(SPEAKER_PORT);
    if ((tmp & 3) != 3) {
        outb(SPEAKER_PORT, tmp | 3);
    }

    last_played_freq = freq_hz;
}

void audio_stop(void) {
    uint8_t tmp = inb(SPEAKER_PORT) & 0xFC; // Clear bits 0 and 1
    outb(SPEAKER_PORT, tmp);
    last_played_freq = 0;
}

void audio_play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    if (!audio_enabled) return;

    audio_start_hardware_tone(freq_hz);
    sleep_ms(duration_ms);
    audio_stop();
}

void audio_beep(void) {
    audio_play_tone(880, 100); // 880 Hz standard system alert beep
}

void audio_chime(void) {
    // 4-note ascending melodic chime (C5, E5, G5, C6)
    audio_play_tone(523, 80);
    sleep_ms(20);
    audio_play_tone(659, 80);
    sleep_ms(20);
    audio_play_tone(784, 80);
    sleep_ms(20);
    audio_play_tone(1046, 140);
}

void audio_print_status(void) {
    kprintf("===================================================================\n");
    kprintf("                    AUDIO SUBSYSTEM & SOUND GENERATOR              \n");
    kprintf("===================================================================\n");
    kprintf(" Hardware Driver       : 8254 PIT Ch 2 Square Wave + Port 0x61 Gate\n");
    kprintf(" Audio Output Status   : %s\n", audio_enabled ? "ACTIVE & READY" : "DISABLED");
    kprintf(" Last Frequency Played : %u Hz\n", last_played_freq);
    kprintf(" Sound Capabilities    : PCM Square-Wave Melodic Synthesis (20-20000Hz)\n");
    kprintf(" Supported Presets     : 'beep', 'chime', 'audio <freq> <duration_ms>'\n");
    kprintf("===================================================================\n");
}
