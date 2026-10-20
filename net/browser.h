#ifndef ERAN_NET_BROWSER_H
#define ERAN_NET_BROWSER_H

#include <stdint.h>
#include <stddef.h>

#define BROWSER_MAX_URL   96
#define BROWSER_MAX_TITLE 64
#define BROWSER_MAX_LINES 18
#define BROWSER_LINE_LEN  72
#define BROWSER_MAX_LINKS 16

typedef struct {
    int line;
    char href[64];
    char label[32];
} browser_link_t;

typedef struct {
    char url[BROWSER_MAX_URL];
    char title[BROWSER_MAX_TITLE];
    int status_code;
    char lines[BROWSER_MAX_LINES][BROWSER_LINE_LEN];
    int line_count;
    browser_link_t links[BROWSER_MAX_LINKS];
    int link_count;
    char raw_html[1536];
} browser_page_t;

void browser_init(void);
int  browser_navigate(const char *url);
int  browser_click_line(int line);
int  browser_can_go_back(void);
int  browser_go_back(void);
const browser_page_t *browser_get_current_page(void);
void browser_cmd_open(const char *url);

#endif /* ERAN_NET_BROWSER_H */
