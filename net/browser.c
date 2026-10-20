#include "browser.h"
#include "tcp.h"
#include "../fs/vfs.h"
#include "../drivers/vga.h"

static browser_page_t current_page;

#define BROWSER_HISTORY_MAX 8
static char history[BROWSER_HISTORY_MAX][BROWSER_MAX_URL];
static int  history_count = 0;

// ---------------------------------------------------------------------------
// String Utility Helpers
// ---------------------------------------------------------------------------
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

static int b_strcmp(const char *s1, const char *s2) {
    if (!s1 || !s2) return -1;
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static int b_strncmp(const char *s1, const char *s2, size_t n) {
    if (!s1 || !s2) return -1;
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static const char *b_strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle) return haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && (*h == *n || (*h >= 'A' && *h <= 'Z' && *h + 32 == *n) || (*n >= 'A' && *n <= 'Z' && *n + 32 == *h))) {
            h++;
            n++;
        }
        if (!*n) return haystack;
    }
    return 0;
}

static void b_add_line(browser_page_t *page, const char *text) {
    if (page->line_count < BROWSER_MAX_LINES) {
        b_strncpy(page->lines[page->line_count], text, BROWSER_LINE_LEN);
        page->line_count++;
    }
}

// ---------------------------------------------------------------------------
// HTML Parser
// ---------------------------------------------------------------------------
static void browser_parse_html(const char *html, browser_page_t *page) {
    page->line_count = 0;
    page->link_count = 0;
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
    // Skip HTTP headers if present
    const char *hdr_end = b_strstr(html, "\r\n\r\n");
    if (hdr_end) {
        p = hdr_end + 4;
    } else {
        const char *hdr_end_nl = b_strstr(html, "\n\n");
        if (hdr_end_nl) {
            p = hdr_end_nl + 2;
        }
    }

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
                // Extract href="..." attribute
                char href[64] = "";
                const char *h = p;
                while (*h && *h != '>') {
                    if (b_strncmp(h, "href=\"", 6) == 0 || b_strncmp(h, "HREF=\"", 6) == 0) {
                        h += 6;
                        int hi = 0;
                        while (*h && *h != '\"' && hi < 63) {
                            href[hi++] = *h++;
                        }
                        href[hi] = '\0';
                        break;
                    }
                    h++;
                }
                while (*p && *p != '>') p++;
                if (*p == '>') p++;

                // Register hyperlink for line
                if (href[0] && page->link_count < BROWSER_MAX_LINKS) {
                    page->links[page->link_count].line = page->line_count;
                    b_strncpy(page->links[page->link_count].href, href, sizeof(page->links[page->link_count].href));
                    page->link_count++;
                }

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

// ---------------------------------------------------------------------------
// Offline EranSearch Query Engine
// ---------------------------------------------------------------------------
static void browser_search(const char *query, browser_page_t *page) {
    page->status_code = 200;
    b_strncpy(page->url, "search", BROWSER_MAX_URL);
    b_strcpy(page->title, "EranSearch: Offline Index");

    // Match query topic
    int is_linux = b_strstr(query, "linux") || b_strstr(query, "bin") || b_strstr(query, "sh") || b_strstr(query, "command");
    int is_net   = b_strstr(query, "net") || b_strstr(query, "tcp") || b_strstr(query, "web") || b_strstr(query, "browser") || b_strstr(query, "ip");
    int is_ai    = b_strstr(query, "ai") || b_strstr(query, "neural") || b_strstr(query, "model") || b_strstr(query, "chat");
    int is_gui   = b_strstr(query, "gui") || b_strstr(query, "desktop") || b_strstr(query, "mouse") || b_strstr(query, "ui");
    int is_ext   = b_strstr(query, ".com") || b_strstr(query, "google") || b_strstr(query, "youtube") || b_strstr(query, "http");

    b_strcpy(page->raw_html,
        "<html><head><title>EranSearch Results</title></head><body>"
        "<h1>EranSearch: Offline Knowledge Index</h1>"
        "<p>Instant local search results across Eran OS subsystems:</p><hr>");

    if (is_ext) {
        b_strcpy(page->raw_html + b_strlen(page->raw_html),
            "<h2>External Web Notice: Domain Resolution</h2>"
            "<p>Public websites require TLS/HTTPS (port 443) and DNS resolution.</p>"
            "<p>Eran OS provides local VFS web nodes, EranSearch, and raw TCP HTTP.</p>"
            "<p><a href=\"/var/www/index.html\">Return to Eran OS Home Portal</a></p><hr>");
    }

    if (is_linux || (!is_net && !is_ai && !is_gui)) {
        b_strcpy(page->raw_html + b_strlen(page->raw_html),
            "<h2>Mini Linux Shell & Core Utilities</h2>"
            "<p>Posix-style /bin tools: sh, ls, cat, echo, pwd, uname, whoami, browser.</p>"
            "<p><a href=\"/var/www/help.html\">Open Mini Linux Command Guide</a></p>");
    }

    if (is_net || (!is_linux && !is_ai && !is_gui)) {
        b_strcpy(page->raw_html + b_strlen(page->raw_html),
            "<h2>TCP Networking & Web Server Stack</h2>"
            "<p>RFC 793 TCP engine, BSD Sockets, HTTP client, and e1000 Gigabit NIC.</p>"
            "<p><a href=\"/var/www/about.html\">Read Networking Architecture</a></p>");
    }

    if (is_ai) {
        b_strcpy(page->raw_html + b_strlen(page->raw_html),
            "<h2>Embedded Micro-Neural Assistant Core</h2>"
            "<p>Offline quantized matrix inference running in user space Ring 3.</p>"
            "<p><a href=\"/var/www/about.html\">Learn About Offline Micro-AI</a></p>");
    }

    if (is_gui) {
        b_strcpy(page->raw_html + b_strlen(page->raw_html),
            "<h2>2D Graphical Desktop & PS/2 Mouse</h2>"
            "<p>Window compositor with double buffering, taskbar, and audio synthesis.</p>"
            "<p><a href=\"/var/www/help.html\">View Desktop Shortcuts</a></p>");
    }

    b_strcpy(page->raw_html + b_strlen(page->raw_html),
        "<p><a href=\"/var/www/index.html\">Back to Home Portal</a> | <a href=\"search\">New Search</a></p>"
        "</body></html>");

    browser_parse_html(page->raw_html, page);
}

// ---------------------------------------------------------------------------
// External Gateway Notice Renderer
// ---------------------------------------------------------------------------
static void browser_external_notice(const char *domain, browser_page_t *page) {
    page->status_code = 200;
    b_strncpy(page->url, domain, BROWSER_MAX_URL);
    b_strcpy(page->title, "External Gateway Notice");

    b_strcpy(page->raw_html,
        "<html><head><title>External Web Gateway</title></head><body>"
        "<h1>External Web Notice: DNS & HTTPS</h1>"
        "<p>You requested an external public domain on the Internet:</p>"
        "<h2>Target Domain: ");
    b_strncpy(page->raw_html + b_strlen(page->raw_html), domain, 32);
    b_strcpy(page->raw_html + b_strlen(page->raw_html),
        "</h2><hr><p>Modern public websites use TLS 1.3 encryption on HTTPS port 443.</p>"
        "<p>Eran OS is a bare-metal kernel operating with offline VFS and local TCP port 80.</p>"
        "<h2>Available Fast Actions:</h2><ul>"
        "<li><a href=\"search\">Search Offline Eran OS Knowledge Index</a></li>"
        "<li><a href=\"/var/www/index.html\">Go to Eran OS Web Home Portal</a></li>"
        "<li><a href=\"/var/www/help.html\">Open Command & Utilities Manual</a></li>"
        "</ul></body></html>");

    browser_parse_html(page->raw_html, page);
}

// ---------------------------------------------------------------------------
// History Navigation
// ---------------------------------------------------------------------------
int browser_can_go_back(void) {
    return (history_count > 0);
}

int browser_go_back(void) {
    if (history_count <= 0) return -1;
    history_count--;
    char prev_url[BROWSER_MAX_URL];
    b_strncpy(prev_url, history[history_count], BROWSER_MAX_URL);

    // Call internal navigation without re-pushing current URL to history
    return browser_navigate(prev_url);
}

// ---------------------------------------------------------------------------
// Link Click Dispatcher
// ---------------------------------------------------------------------------
int browser_click_line(int line) {
    for (int i = 0; i < current_page.link_count; i++) {
        if (current_page.links[i].line == line) {
            browser_navigate(current_page.links[i].href);
            return 1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Core Browser Engine API
// ---------------------------------------------------------------------------
void browser_init(void) {
    history_count = 0;
    b_strcpy(current_page.url, "http://eranos.local/");
    b_strcpy(current_page.title, "Eran OS Web Portal");
    current_page.status_code = 200;
    current_page.line_count = 0;
    current_page.link_count = 0;
    browser_navigate("http://eranos.local/");
}

int browser_navigate(const char *url) {
    if (!url || url[0] == '\0') {
        url = "http://eranos.local/";
    }

    // Push previous URL to history if valid and different
    if (current_page.url[0] && b_strcmp(current_page.url, url) != 0) {
        if (history_count < BROWSER_HISTORY_MAX) {
            b_strncpy(history[history_count++], current_page.url, BROWSER_MAX_URL);
        }
    }

    b_strncpy(current_page.url, url, BROWSER_MAX_URL);
    current_page.status_code = 200;

    // 1. Search Query Dispatcher
    if (b_strncmp(url, "search ", 7) == 0 || b_strncmp(url, "find ", 5) == 0 || b_strncmp(url, "?", 1) == 0) {
        const char *q = url;
        if (b_strncmp(url, "search ", 7) == 0) q += 7;
        else if (b_strncmp(url, "find ", 5) == 0) q += 5;
        else if (b_strncmp(url, "?", 1) == 0) q += 1;
        while (*q == ' ') q++;
        browser_search(q, &current_page);
        return 0;
    }
    if (b_strcmp(url, "search") == 0 || b_strcmp(url, "eransearch") == 0 || b_strcmp(url, "search.html") == 0) {
        browser_search("all", &current_page);
        return 0;
    }

    // 2. External Domain Resolution Notice (google.com, youtube.com, https://...)
    if (b_strncmp(url, "https://", 8) == 0 ||
        b_strstr(url, ".com") != 0 ||
        b_strstr(url, ".org") != 0 ||
        b_strstr(url, ".net") != 0 ||
        b_strstr(url, "www.") != 0) {
        browser_external_notice(url, &current_page);
        return 0;
    }

    // 3. Resolve Local VFS Target
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
    } else if (b_strcmp(url, "home") == 0 || b_strcmp(url, "index") == 0 || b_strcmp(url, "index.html") == 0) {
        b_strcpy(local_vfs_path, "/var/www/index.html");
    } else if (b_strcmp(url, "about") == 0 || b_strcmp(url, "about.html") == 0) {
        b_strcpy(local_vfs_path, "/var/www/about.html");
    } else if (b_strcmp(url, "help") == 0 || b_strcmp(url, "help.html") == 0) {
        b_strcpy(local_vfs_path, "/var/www/help.html");
    } else if (b_strncmp(url, "http://", 7) == 0) {
        // Real remote TCP HTTP request for numeric IP (e.g. 10.0.2.2 or 192.168.1.1)
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
        // If TCP failed, show clear error page rather than silent fallback
        current_page.status_code = 502;
        b_strcpy(current_page.title, "502 Bad Gateway / Unreachable");
        b_strcpy(current_page.lines[0], "== 502 TCP Host Unreachable ==");
        b_strcpy(current_page.lines[1], "--------------------------------------------------------");
        b_strcpy(current_page.lines[2], "Could not establish TCP handshake on port 80.");
        b_strcpy(current_page.lines[3], "Target IP address did not respond to SYN packet.");
        b_strcpy(current_page.lines[4], "Links: [ http://eranos.local/ ] | [ search ]");
        current_page.line_count = 5;
        current_page.link_count = 2;
        current_page.links[0].line = 4;
        b_strcpy(current_page.links[0].href, "http://eranos.local/");
        current_page.links[1].line = 4;
        b_strcpy(current_page.links[1].href, "search");
        return -1;
    } else {
        // Relative file name fallback: check in /var/www/
        b_strcpy(local_vfs_path, "/var/www/");
        b_strncpy(local_vfs_path + 9, url, sizeof(local_vfs_path) - 10);
    }

    // Read from VFS
    int b = vfs_read_file(local_vfs_path, current_page.raw_html, sizeof(current_page.raw_html) - 1);
    if (b > 0) {
        current_page.raw_html[b] = '\0';
        current_page.status_code = 200;
        browser_parse_html(current_page.raw_html, &current_page);
        return 0;
    } else {
        // Fallback: If not found in /var/www/, try treating as search query
        browser_search(url, &current_page);
        return 0;
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
            current_page.status_code, (current_page.status_code == 200) ? "OK" : "Error");
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
