#ifndef ERAN_UI_DESKTOP_H
#define ERAN_UI_DESKTOP_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int id;
    const char *title;
    int x;
    int y;
    int width;
    int height;
    int is_active;
    int is_minimized;
} desktop_window_t;

void desktop_init(void);
void desktop_render(void);
void desktop_interactive(void);

#endif /* ERAN_UI_DESKTOP_H */
