#include "browser.h"
#include "tcp.h"
#include "../fs/vfs.h"
#include "../drivers/vga.h"

static browser_page_t current_page;

static size_t b_strlen(const char *s) {
    size_t len = 0;
    if (!s) return 0;
    while (*s++) len++;
    return len;
}

static void b_strcpy(char *dest, const char *src) {
    if (!dest || !src) return;
    while ((*dest++ = *src++));
}

static void b_strncpy(char *dest, const char *src, size_t n) {
    if (!dest || !src || n == 0) return;
    size_t i = 0;
    while (i < n - 1 && src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int b_strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static void b_add_line(browser_page_t *page, const char *text) {
    if (page->line_count < BROWSER_MAX_LINES) {
        b_strncpy(page->lines[page->line_count], text, BROWSER_LINE_LEN);
        page->line_count++;
    }
}

static void browser_parse_html(const char *html, browser_page_t *page) {
    page->line_count = 0;
    for (int i = 0; i < BROWSER_MAX_LINES; i++) {
        page->lines[i][0] = '\0';
    }

    // 1. Extract <title>...</title>
    b_strcpy(page->title, "Eran OS Web Document");
    const char *t = html;
    while (*t) {
        if (b_strncmp(t, "<title>", 7) == 0 || b_strncmp(t, "<TITLE>", 7) == 0) {
            t += 7;
            int ti = 0;
            while (*t && b_strncmp(t, "</title>", 8) != 0 && b_strncmp(t, "</TITLE>", 8) != 0 && ti < BROWSER_MAX_TITLE - 1) {
                page->title[ti++] = *t++;
            }
            page->title[ti] = '\0';
            break;
        }
        t++;
    }

    // 2. Parse HTML Body Elements into lines
    char cur_line[BROWSER_LINE_LEN];
    int cur_col = 0;
    cur_line[0] = '\0';

    const char *p = html;
    // Skip everything before <body> if <body> exists
    const char *body = p;
    while (*body) {
        if (b_strncmp(body, "<body>", 6) == 0 || b_strncmp(body, "<BODY>", 6) == 0) {
            p = body + 6;
            break;
        }
        body++;
    }

    while (*p && page->line_count < BROWSER_MAX_LINES) {
        if (*p == '<') {
            // Check Tag Type
            if (b_strncmp(p, "<h1>", 4) == 0 || b_strncmp(p, "<H1>", 4) == 0) {
                if (cur_col > 0) {
                    cur_line[cur_col] = '\0';
                    b_add_line(page, cur_line);
                    cur_col = 0;
                }
                b_strcpy(cur_line, "== ");
                cur_col = 3;
                p += 4;
            } else if (b_strncmp(p, "</h1>", 5) == 0 || b_strncmp(p, "</H1>", 5) == 0) {
                if (cur_col < BROWSER_LINE_LEN - 4) {
                    cur_line[cur_col++] = ' ';
                    cur_line[cur_col++] = '=';
                    cur_line[cur_col++] = '=';
                }
                cur_line[cur_col] = '\0';
                b_add_line(page, cur_line);
                b_add_line(page, "--------------------------------------------------------");
                cur_col = 0;
                cur_line[0] = '\0';
                p += 5;
            } else if (b_strncmp(p, "<h2>", 4) == 0 || b_strncmp(p, "<H2>", 4) == 0) {
                if (cur_col > 0) {
                    cur_line[cur_col] = '\0';
                    b_add_line(page, cur_line);
                    cur_col = 0;
                }
                b_strcpy(cur_line, "* ");
                cur_col = 2;
                p += 4;
            } else if (b_strncmp(p, "</h2>", 5) == 0 || b_strncmp(p, "</H2>", 5) == 0) {
                cur_line[cur_col] = '\0';
                b_add_line(page, cur_line);
                cur_col = 0;
                cur_line[0] = '\0';
                p += 5;
            } else if (b_strncmp(p, "<li>", 4) == 0 || b_strncmp(p, "<LI>", 4) == 0) {
                if (cur_col > 0) {
                    cur_line[cur_col] = '\0';
                    b_add_line(page, cur_line);
                    cur_col = 0;
                }
                b_strcpy(cur_line, "  * ");
                cur_col = 4;
                p += 4;
            } else if (b_strncmp(p, "</li>", 5) == 0 || b_strncmp(p, "</LI>", 5) == 0) {
                cur_line[cur_col] = '\0';
                b_add_line(page, cur_line);
                cur_col = 0;
                cur_line[0] = '\0';
                p += 5;
            } else if (b_strncmp(p, "<hr>", 4) == 0 || b_strncmp(p, "<HR>", 4) == 0 || b_strncmp(p, "<hr/>", 5) == 0) {
                if (cur_col > 0) {
                    cur_line[cur_col] = '\0';
                    b_add_line(page, cur_line);
                    cur_col = 0;
                }
                b_add_line(page, "--------------------------------------------------------");
                cur_col = 0;
                cur_line[0] = '\0';
                while (*p && *p != '>') p++;
                if (*p == '>') p++;
            } else if (b_strncmp(p, "<p>", 3) == 0 || b_strncmp(p, "<P>", 3) == 0) {
                if (cur_col > 0) {
                    cur_line[cur_col] = '\0';
                    b_add_line(page, cur_line);
                    cur_col = 0;
                    cur_line[0] = '\0';
                }
                p += 3;
            } else if (b_strncmp(p, "</p>", 4) == 0 || b_strncmp(p, "</P>", 4) == 0) {
                if (cur_col > 0) {
                    cur_line[cur_col] = '\0';
                    b_add_line(page, cur_line);
                    cur_col = 0;
                    cur_line[0] = '\0';
                }
                p += 4;
            } else if (b_strncmp(p, "<br>", 4) == 0 || b_strncmp(p, "<BR>", 4) == 0 || b_strncmp(p, "<br/>", 5) == 0) {
                cur_line[cur_col] = '\0';
                b_add_line(page, cur_line);
                cur_col = 0;
                cur_line[0] = '\0';
                while (*p && *p != '>') p++;
                if (*p == '>') p++;
            } else if (b_strncmp(p, "<a ", 3) == 0 || b_strncmp(p, "<A ", 3) == 0) {
                // Link opening tag: skip attributes, insert [
                while (*p && *p != '>') p++;
                if (*p == '>') p++;
                if (cur_col < BROWSER_LINE_LEN - 3) {
                    cur_line[cur_col++] = '[';
                    cur_line[cur_col++] = ' ';
                }
            } else if (b_strncmp(p, "</a>", 4) == 0 || b_strncmp(p, "</A>", 4) == 0) {
                if (cur_col < BROWSER_LINE_LEN - 3) {
                    cur_line[cur_col++] = ' ';
                    cur_line[cur_col++] = ']';
                }
                p += 4;
            } else {
                // Skip unknown tag
                while (*p && *p != '>') p++;
                if (*p == '>') p++;
            }
        } else if (*p == '\n' || *p == '\r') {
            // Newline in HTML is whitespace
            if (cur_col > 0 && cur_line[cur_col - 1] != ' ') {
                if (cur_col < BROWSER_LINE_LEN - 1) cur_line[cur_col++] = ' ';
            }
            p++;
        } else if (*p == '\t') {
            if (cur_col < BROWSER_LINE_LEN - 1) cur_line[cur_col++] = ' ';
            p++;
        } else {
            // Normal character
            if (cur_col < BROWSER_LINE_LEN - 1) {
                cur_line[cur_col++] = *p;
            } else {
                // Word wrap
                cur_line[cur_col] = '\0';
                b_add_line(page, cur_line);
                cur_col = 0;
                cur_line[cur_col++] = *p;
            }
            p++;
        }
    }

    if (cur_col > 0 && page->line_count < BROWSER_MAX_LINES) {
        cur_line[cur_col] = '\0';
        b_add_line(page, cur_line);
    }
}

void browser_init(void) {
    b_strcpy(current_page.url, "http://eranos.local/");
    b_strcpy(current_page.title, "Eran OS Web Portal");
    current_page.status_code = 200;
    current_page.line_count = 0;
    browser_navigate("http://eranos.local/");
}

int browser_navigate(const char *url) {
    if (!url || url[0] == '\0') {
        url = "http://eranos.local/";
    }

    b_strncpy(current_page.url, url, BROWSER_MAX_URL);
    current_page.status_code = 200;

    // Check if URL is local VFS or special eranos.local hostname
    char local_vfs_path[64] = "/var/www/index.html";

    if (b_strncmp(url, "file://", 7) == 0) {
        b_strncpy(local_vfs_path, url + 7, sizeof(local_vfs_path));
    } else if (url[0] == '/') {
        b_strncpy(local_vfs_path, url, sizeof(local_vfs_path));
    } else if (b_strncmp(url, "http://eranos.local", 19) == 0 || b_strncmp(url, "http://localhost", 16) == 0) {
        const char *sub = url + 19;
        if (b_strncmp(url, "http://localhost", 16) == 0) sub = url + 16;
        if (*sub == '/') sub++;
        if (*sub == '\0') {
            b_strcpy(local_vfs_path, "/var/www/index.html");
        } else {
            b_strcpy(local_vfs_path, "/var/www/");
            b_strncpy(local_vfs_path + 9, sub, sizeof(local_vfs_path) - 10);
        }
    } else if (b_strncmp(url, "http://", 7) == 0) {
        // Real remote TCP HTTP request
        const char *host_start = url + 7;
        char host[32];
        int hi = 0;
        while (*host_start && *host_start != '/' && *host_start != ':' && hi < 31) {
            host[hi++] = *host_start++;
        }
        host[hi] = '\0';

        // Connect via TCP stack
        int sock = tcp_socket();
        if (sock >= 0) {
            if (tcp_connect(sock, host, 80) == 0) {
                const char *req = "GET / HTTP/1.1\r\nHost: target\r\nUser-Agent: EranBrowser/1.0\r\nConnection: close\r\n\r\n";
                tcp_send(sock, req, b_strlen(req));

                int bytes = tcp_recv(sock, current_page.raw_html, sizeof(current_page.raw_html) - 1);
                if (bytes > 0) {
                    current_page.raw_html[bytes] = '\0';
                    current_page.status_code = 200;
                    tcp_close(sock);
                    browser_parse_html(current_page.raw_html, &current_page);
                    return 0;
                }
                tcp_close(sock);
            }
        }
        // Fallback to local default if remote server not reachable
        b_strcpy(local_vfs_path, "/var/www/index.html");
    }

    // Read from VFS
    int b = vfs_read_file(local_vfs_path, current_page.raw_html, sizeof(current_page.raw_html) - 1);
    if (b > 0) {
        current_page.raw_html[b] = '\0';
        current_page.status_code = 200;
        browser_parse_html(current_page.raw_html, &current_page);
        return 0;
    } else {
        current_page.status_code = 404;
        b_strcpy(current_page.title, "404 Not Found");
        b_strcpy(current_page.lines[0], "== 404 HTTP Not Found ==");
        b_strcpy(current_page.lines[1], "--------------------------------------------------------");
        b_strcpy(current_page.lines[2], "The requested URL was not found on this Eran OS node.");
        b_strcpy(current_page.lines[3], "Navigation: [ http://eranos.local/ ]");
        current_page.line_count = 4;
        return -1;
    }
}

const browser_page_t *browser_get_current_page(void) {
    return &current_page;
}

void browser_cmd_open(const char *url) {
    kprintf("Navigating to: %s ...\n", (url && url[0]) ? url : "http://eranos.local/");
    browser_navigate(url);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("===================================================================\n");
    kprintf("                  ERAN OS - MINI LINUX WEB BROWSER                 \n");
    kprintf("===================================================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf(" URL    : %s\n", current_page.url);
    kprintf(" Title  : %s\n", current_page.title);
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf(" Status : HTTP/1.1 %d %s | Content-Type: text/html\n",
            current_page.status_code, (current_page.status_code == 200) ? "OK" : "Not Found");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("===================================================================\n\n");

    for (int i = 0; i < current_page.line_count; i++) {
        if (current_page.lines[i][0] == '=' && current_page.lines[i][1] == '=') {
            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        } else if (current_page.lines[i][0] == '*') {
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        } else if (current_page.lines[i][0] == '-' && current_page.lines[i][1] == '-') {
            vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        } else {
            vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        }
        kprintf("%s\n", current_page.lines[i]);
    }

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("\n===================================================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}
