#ifndef ERAN_NET_BROWSER_H
#define ERAN_NET_BROWSER_H

#include <stdint.h>
#include <stddef.h>

#define BROWSER_MAX_URL   96
#define BROWSER_MAX_TITLE 64
#define BROWSER_MAX_LINES 18
#define BROWSER_LINE_LEN  72

typedef struct {
    char url[BROWSER_MAX_URL];
    char title[BROWSER_MAX_TITLE];
    int status_code;
    char lines[BROWSER_MAX_LINES][BROWSER_LINE_LEN];
    int line_count;
    char raw_html[1024];
} browser_page_t;

void browser_init(void);
int  browser_navigate(const char *url);
const browser_page_t *browser_get_current_page(void);
void browser_cmd_open(const char *url);

#endif /* ERAN_NET_BROWSER_H */
