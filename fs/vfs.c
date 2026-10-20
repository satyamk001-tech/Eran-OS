#include "vfs.h"
#include "../drivers/vga.h"
#include "../drivers/ata.h"
#include "../mm/heap.h"
#include "../user/hello_elf_data.h"

// ---------------------------------------------------------------------------
// Directory Nodes
// ---------------------------------------------------------------------------
static vfs_node_t root_dir;
static vfs_node_t bin_dir;
static vfs_node_t etc_dir;
static vfs_node_t dev_dir;
static vfs_node_t home_dir;
static vfs_node_t user_dir;
static vfs_node_t root_user_dir;
static vfs_node_t var_dir;
static vfs_node_t www_dir;
static vfs_node_t tmp_dir;
static vfs_node_t system_dir;

// ---------------------------------------------------------------------------
// File Nodes
// ---------------------------------------------------------------------------
static vfs_node_t file_readme;
static vfs_node_t file_osinfo;

// /bin files
static vfs_node_t file_hello_elf;
static vfs_node_t file_bin_sh;
static vfs_node_t file_bin_ls;
static vfs_node_t file_bin_cat;
static vfs_node_t file_bin_echo;
static vfs_node_t file_bin_pwd;
static vfs_node_t file_bin_uname;
static vfs_node_t file_bin_whoami;
static vfs_node_t file_bin_date;
static vfs_node_t file_bin_clear;
static vfs_node_t file_bin_browser;
static vfs_node_t file_bin_curl;
static vfs_node_t file_bin_snake;
static vfs_node_t file_bin_snakes;
static vfs_node_t file_bin_spaceimpact;

// /etc files
static vfs_node_t file_hostname;
static vfs_node_t file_issue;
static vfs_node_t file_osrelease;
static vfs_node_t file_passwd;
static vfs_node_t file_motd;
static vfs_node_t file_devices;

// /dev files
static vfs_node_t file_dev_tty0;
static vfs_node_t file_dev_null;
static vfs_node_t file_dev_zero;
static vfs_node_t file_dev_eth0;
static vfs_node_t file_dev_sda;

// /home/user files
static vfs_node_t file_home_welcome;

// /root files
static vfs_node_t file_root_bashrc;

// /var/www files
static vfs_node_t file_www_index;
static vfs_node_t file_www_about;
static vfs_node_t file_www_help;
static vfs_node_t file_www_search;

// ---------------------------------------------------------------------------
// File Content Constants
// ---------------------------------------------------------------------------
static const char readme_content[] =
    "Eran OS v0.4.0 - Mini Linux Operating System\n"
    "Built from bare metal x86 assembly to freestanding C.\n"
    "Featuring 32-bit Protected Mode, IDT exception handling,\n"
    "8259 PIC remapping, PIT 100Hz timer, PS/2 keyboard & mouse driver,\n"
    "Bitmap Physical Memory Manager, Dynamic Kernel Heap, Two-Level Paging,\n"
    "Intel e1000 Gigabit Network Stack, PC Speaker Audio Synthesizer,\n"
    "2D Graphical Desktop Compositor, ACPI Power Management, CPUID prober,\n"
    "ELF32 Executable Binary Loader, Mini Linux /bin utilities, and Web Browser.\n";

static const char osinfo_content[] =
    "OS Name     : Eran OS\n"
    "Version     : 0.4.0 (Mini Linux Edition Active)\n"
    "Architecture: x86 32-bit (IA-32 Protected Mode & Long Mode Probe)\n"
    "Boot Method : Legacy BIOS MBR -> Protected Mode Handover\n"
    "Kernel Base : 0x00010000 Physical\n";

static const char hostname_content[] = "eranos\n";

static const char issue_content[] = "Eran OS Mini Linux 0.4.0 \\n \\l\n\n";

static const char osrelease_content[] =
    "NAME=\"Eran OS\"\n"
    "VERSION=\"0.4.0\"\n"
    "ID=eranos\n"
    "ID_LIKE=linux\n"
    "PRETTY_NAME=\"Eran OS Mini Linux 0.4.0 (x86_32)\"\n"
    "HOME_URL=\"https://github.com/satyamk001-tech/Eran-OS\"\n";

static const char passwd_content[] =
    "root:x:0:0:root:/root:/bin/sh\n"
    "user:x:1000:1000:user:/home/user:/bin/sh\n";

static const char motd_content[] =
    "Welcome to Eran OS Mini Linux workstation.\n"
    "Type 'help' for commands, 'browser' for web, 'desktop' for GUI.\n";

static const char devices_content[] =
    "Discovered Hardware Subsystems:\n"
    "- 8254 Programmable Interval Timer (IRQ 0)\n"
    "- 8042 PS/2 Keyboard Controller (IRQ 1)\n"
    "- 8042 PS/2 Auxiliary Mouse (IRQ 12)\n"
    "- 16550 UART Serial Controller (COM1 0x3F8)\n"
    "- Standard VGA Text & 2D Graphics Canvas (0xB8000)\n"
    "- Intel 82540EM (e1000) Gigabit NIC (eth0)\n"
    "- 8254 PIT Ch 2 PC Speaker Audio Synthesizer\n"
    "- Primary Parallel ATA / IDE Controller (0x1F0)\n"
    "- USB Host Controller & Root Hub Discovery\n"
    "- ACPI Power Management & Battery Telemetry\n"
    "- CPUID Extended Feature & Long Mode Detector\n";

static const char script_sh[]      = "#!/bin/sh\n# Eran OS Command Interpreter\n";
static const char script_ls[]      = "#!/bin/sh\n# List directory contents\n";
static const char script_cat[]     = "#!/bin/sh\n# Print file contents\n";
static const char script_echo[]    = "#!/bin/sh\n# Print arguments\n";
static const char script_pwd[]     = "#!/bin/sh\n# Print working directory\n";
static const char script_uname[]   = "#!/bin/sh\n# Print system architecture\n";
static const char script_whoami[]  = "#!/bin/sh\n# Print current user\n";
static const char script_date[]    = "#!/bin/sh\n# Print date and uptime\n";
static const char script_clear[]   = "#!/bin/sh\n# Clear console screen\n";
static const char script_browser[] = "#!/bin/sh\n# Eran OS Mini Linux Web Browser\n";
static const char script_curl[]    = "#!/bin/sh\n# HTTP network fetcher\n";
static const char script_snake[]   = "#!/bin/sh\n# Eran OS Retro Snake Arcade Game\nsnake\n";
static const char script_snakes[]  = "#!/bin/sh\n# Eran OS Snakes and Ladders Board Game\nsnakes\n";
static const char script_spaceimpact[] = "#!/bin/sh\n# Eran OS Space Impact Nokia 3310 Arcade Shooter\nspaceimpact\n";

static const char dev_tty0_content[] = "Character Device: Standard VGA 80x25 Console\n";
static const char dev_null_content[] = "Character Device: Null sink\n";
static const char dev_zero_content[] = "Character Device: Zero stream\n";
static const char dev_eth0_content[] = "Network Device: Intel 82540EM (e1000) Gigabit NIC (192.168.1.50)\n";
static const char dev_sda_content[]  = "Block Device: Primary Parallel ATA IDE Drive (0x1F0 LBA28)\n";

static const char home_welcome_content[] =
    "Welcome to your Eran OS user home directory!\n"
    "Type 'browser' to surf the web or 'desktop' for the GUI workstation.\n";

static const char root_bashrc_content[] =
    "# Eran OS environment configuration\n"
    "export PATH=/bin:/usr/bin\n"
    "export USER=root\n"
    "export HOME=/root\n";

static const char www_index_content[] =
    "<html><head><title>Eran OS Home Portal</title></head><body>"
    "<h1>Welcome to Eran OS Web Portal</h1>"
    "<p>Eran OS is a lightweight bare-metal operating system engineered from scratch.</p>"
    "<hr><h2>System Highlights</h2><ul>"
    "<li>Fast x86 32-bit Protected Mode kernel with two-level paging</li>"
    "<li>Intel e1000 Gigabit network stack with TCP streams</li>"
    "<li>Embedded Micro-Neural inference model running offline</li>"
    "<li>Interactive 2D graphical workstation desktop with mouse</li>"
    "<li>Mini Linux core utilities and HTML web browser</li>"
    "</ul><hr><h2>Interactive Navigation Links</h2>"
    "<p><a href=\"/var/www/about.html\">1. About Eran OS Workstation</a></p>"
    "<p><a href=\"/var/www/help.html\">2. Mini Linux User Manual</a></p>"
    "<p><a href=\"search\">3. EranSearch Offline Index</a></p>"
    "</body></html>";

static const char www_search_content[] =
    "<html><head><title>EranSearch Portal</title></head><body>"
    "<h1>EranSearch Offline Portal</h1>"
    "<p>Instant local search across all Eran OS subsystems.</p>"
    "<h2>Quick Search Categories:</h2>"
    "<p><a href=\"search linux\">1. Search Linux Shell & Core Utilities</a></p>"
    "<p><a href=\"search tcp\">2. Search Networking & TCP Stack</a></p>"
    "<p><a href=\"search ai\">3. Search Embedded Micro-Neural AI Core</a></p>"
    "<p><a href=\"/var/www/index.html\">Back to Home Portal</a></p>"
    "</body></html>";

static const char www_about_content[] =
    "<html><head><title>About Eran OS</title></head><body>"
    "<h1>About Eran OS Workstation</h1>"
    "<p>Architected from x86 assembly to freestanding C.</p>"
    "<h2>Subsystem Overview</h2><ul>"
    "<li>Kernel: Protected Mode and Long Mode prober</li>"
    "<li>Filesystem: Virtual Filesystem with ATA IDE persistence</li>"
    "<li>Networking: TCP RFC 793, BSD Sockets, HTTP client</li>"
    "<li>Browser: Lightweight HTML parser and document renderer</li>"
    "</ul><p><a href=\"/var/www/index.html\">Back to Home Portal</a></p>"
    "</body></html>";

static const char www_help_content[] =
    "<html><head><title>Eran OS Help & Manual</title></head><body>"
    "<h1>Mini Linux Command Reference</h1>"
    "<p>Use the following commands in the terminal:</p><ul>"
    "<li><b>ls [dir]</b> - List directory files</li>"
    "<li><b>cat [file]</b> - View file contents</li>"
    "<li><b>browser [url]</b> - Open web browser</li>"
    "<li><b>curl [ip]</b> - Fetch HTTP endpoint</li>"
    "<li><b>uname -a</b> - Print Linux architecture</li>"
    "<li><b>whoami</b> - Display current user identity</li>"
    "<li><b>desktop</b> - Launch 2D graphical desktop</li>"
    "</ul><p><a href=\"/var/www/index.html\">Back to Home Portal</a></p>"
    "</body></html>";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void mystrcpy(char *dest, const char *src, size_t max) {
    size_t i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int mystrcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static size_t mystrlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

// Helper to initialize a file node
static void init_file(vfs_node_t *node, const char *name, const char *data, size_t size, vfs_node_t *next) {
    mystrcpy(node->name, name, sizeof(node->name));
    node->type = FS_FILE;
    node->data = data;
    node->size = size;
    node->children = 0;
    node->next = next;
}

// Helper to initialize a directory node
static void init_dir(vfs_node_t *node, const char *name, vfs_node_t *children, vfs_node_t *next) {
    mystrcpy(node->name, name, sizeof(node->name));
    node->type = FS_DIRECTORY;
    node->data = 0;
    node->size = 0;
    node->children = children;
    node->next = next;
}

// ---------------------------------------------------------------------------
// VFS Initialization
// ---------------------------------------------------------------------------
void vfs_init(void) {
    // 1. Initialize /bin files
    init_file(&file_bin_spaceimpact, "spaceimpact", script_spaceimpact, mystrlen(script_spaceimpact), 0);
    init_file(&file_bin_snakes,      "snakes",      script_snakes,      mystrlen(script_snakes),      &file_bin_spaceimpact);
    init_file(&file_bin_snake,       "snake",       script_snake,       mystrlen(script_snake),       &file_bin_snakes);
    init_file(&file_bin_curl,        "curl",        script_curl,        mystrlen(script_curl),        &file_bin_snake);
    init_file(&file_bin_browser, "browser",   script_browser, mystrlen(script_browser), &file_bin_curl);
    init_file(&file_bin_clear,   "clear",     script_clear,   mystrlen(script_clear),   &file_bin_browser);
    init_file(&file_bin_date,    "date",      script_date,    mystrlen(script_date),    &file_bin_clear);
    init_file(&file_bin_whoami,  "whoami",    script_whoami,  mystrlen(script_whoami),  &file_bin_date);
    init_file(&file_bin_uname,   "uname",     script_uname,   mystrlen(script_uname),   &file_bin_whoami);
    init_file(&file_bin_pwd,     "pwd",       script_pwd,     mystrlen(script_pwd),     &file_bin_uname);
    init_file(&file_bin_echo,    "echo",      script_echo,    mystrlen(script_echo),    &file_bin_pwd);
    init_file(&file_bin_cat,     "cat",       script_cat,     mystrlen(script_cat),     &file_bin_echo);
    init_file(&file_bin_ls,      "ls",        script_ls,      mystrlen(script_ls),      &file_bin_cat);
    init_file(&file_bin_sh,      "sh",        script_sh,      mystrlen(script_sh),      &file_bin_ls);
    init_file(&file_hello_elf,   "hello.elf", (const char *)build_hello_elf, sizeof(build_hello_elf), &file_bin_sh);
    init_dir(&bin_dir, "bin", &file_hello_elf, &etc_dir);

    // 2. Initialize /etc files
    init_file(&file_devices,   "devices.txt", devices_content,   mystrlen(devices_content),   0);
    init_file(&file_motd,      "motd",        motd_content,      mystrlen(motd_content),      &file_devices);
    init_file(&file_passwd,    "passwd",      passwd_content,    mystrlen(passwd_content),    &file_motd);
    init_file(&file_osrelease, "os-release",  osrelease_content, mystrlen(osrelease_content), &file_passwd);
    init_file(&file_issue,     "issue",       issue_content,     mystrlen(issue_content),     &file_osrelease);
    init_file(&file_hostname,  "hostname",    hostname_content,  mystrlen(hostname_content),  &file_issue);
    init_dir(&etc_dir, "etc", &file_hostname, &dev_dir);

    // 3. Initialize /dev files
    init_file(&file_dev_sda,  "sda",  dev_sda_content,  mystrlen(dev_sda_content),  0);
    init_file(&file_dev_eth0, "eth0", dev_eth0_content, mystrlen(dev_eth0_content), &file_dev_sda);
    init_file(&file_dev_zero, "zero", dev_zero_content, mystrlen(dev_zero_content), &file_dev_eth0);
    init_file(&file_dev_null, "null", dev_null_content, mystrlen(dev_null_content), &file_dev_zero);
    init_file(&file_dev_tty0, "tty0", dev_tty0_content, mystrlen(dev_tty0_content), &file_dev_null);
    init_dir(&dev_dir, "dev", &file_dev_tty0, &home_dir);

    // 4. Initialize /home and /home/user
    init_file(&file_home_welcome, "welcome.txt", home_welcome_content, mystrlen(home_welcome_content), 0);
    init_dir(&user_dir, "user", &file_home_welcome, 0);
    init_dir(&home_dir, "home", &user_dir, &root_user_dir);

    // 5. Initialize /root
    init_file(&file_root_bashrc, ".bashrc", root_bashrc_content, mystrlen(root_bashrc_content), 0);
    init_dir(&root_user_dir, "root", &file_root_bashrc, &var_dir);

    // 6. Initialize /var and /var/www
    init_file(&file_www_search, "search.html", www_search_content, mystrlen(www_search_content), 0);
    init_file(&file_www_help,  "help.html",  www_help_content,  mystrlen(www_help_content),  &file_www_search);
    init_file(&file_www_about, "about.html", www_about_content, mystrlen(www_about_content), &file_www_help);
    init_file(&file_www_index, "index.html", www_index_content, mystrlen(www_index_content), &file_www_about);
    init_dir(&www_dir, "www", &file_www_index, 0);
    init_dir(&var_dir, "var", &www_dir, &tmp_dir);

    // 7. Initialize /tmp
    init_dir(&tmp_dir, "tmp", 0, &system_dir);

    // 8. Initialize /system
    init_file(&file_osinfo, "os_info.txt", osinfo_content, mystrlen(osinfo_content), 0);
    init_dir(&system_dir, "system", &file_osinfo, 0);

    // 9. Root /
    init_file(&file_readme, "README.txt", readme_content, mystrlen(readme_content), &bin_dir);
    init_dir(&root_dir, "/", &file_readme, 0);
}

vfs_node_t *vfs_get_root(void) {
    return &root_dir;
}

// ---------------------------------------------------------------------------
// Multi-Level Hierarchical Path Resolution
// ---------------------------------------------------------------------------
vfs_node_t *vfs_find_node(const char *path) {
    if (!path || path[0] == '\0') return 0;
    while (*path == ' ') path++;
    if (path[0] == '\0') return 0;

    vfs_node_t *curr = &root_dir;
    if (path[0] == '/') path++;
    if (path[0] == '\0') return curr;

    char segment[32];
    while (*path != '\0') {
        int idx = 0;
        while (*path != '\0' && *path != '/' && idx < 31) {
            segment[idx++] = *path++;
        }
        segment[idx] = '\0';
        while (*path == '/') path++;

        vfs_node_t *child = curr->children;
        vfs_node_t *found = 0;
        while (child) {
            if (mystrcmp(child->name, segment) == 0) {
                found = child;
                break;
            }
            child = child->next;
        }
        if (!found) return 0;
        curr = found;
    }

    return curr;
}

void vfs_list_dir(const char *path) {
    vfs_node_t *dir = vfs_find_node(path);
    if (!dir) {
        kprintf("vfs: Directory not found: %s\n", path);
        return;
    }
    if (dir->type != FS_DIRECTORY) {
        kprintf("vfs: Not a directory: %s\n", path);
        return;
    }

    kprintf("Contents of %s:\n", path);
    vfs_node_t *child = dir->children;
    while (child) {
        if (child->type == FS_DIRECTORY) {
            vga_puts_color("  [DIR]  ", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            kprintf("%s/\n", child->name);
        } else {
            vga_puts_color("  [FILE] ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            kprintf("%s (%u bytes)\n", child->name, child->size);
        }
        child = child->next;
    }
}

int vfs_read_file(const char *path, char *buf, size_t max_len) {
    vfs_node_t *node = vfs_find_node(path);
    if (!node || node->type != FS_FILE || !node->data) {
        return -1;
    }

    size_t copy_len = (node->size < max_len - 1) ? node->size : max_len - 1;
    for (size_t i = 0; i < copy_len; i++) {
        buf[i] = node->data[i];
    }
    buf[copy_len] = '\0';
    return (int)copy_len;
}

int vfs_create_file(const char *path, const char *initial_data) {
    if (!path || path[0] == '\0') return -1;
    if (vfs_find_node(path) != 0) return 0; // Already exists

    const char *fname = path;
    if (fname[0] == '/') fname++;

    vfs_node_t *new_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!new_node) return -1;

    mystrcpy(new_node->name, fname, sizeof(new_node->name));
    new_node->type = FS_FILE;
    new_node->children = 0;

    size_t len = initial_data ? mystrlen(initial_data) : 0;
    new_node->size = len;
    if (len > 0) {
        char *data_buf = (char *)kmalloc(len + 1);
        if (data_buf) {
            for (size_t i = 0; i < len; i++) data_buf[i] = initial_data[i];
            data_buf[len] = '\0';
            new_node->data = data_buf;
        } else {
            new_node->data = "";
            new_node->size = 0;
        }
    } else {
        new_node->data = "";
    }

    // Prepend to root directory children
    new_node->next = root_dir.children;
    root_dir.children = new_node;

    return 0;
}

int vfs_write_file(const char *path, const char *data, size_t len) {
    vfs_node_t *node = vfs_find_node(path);
    if (!node) {
        if (vfs_create_file(path, data) != 0) return -1;
        node = vfs_find_node(path);
        if (!node) return -1;
    }

    if (node->type != FS_FILE) return -1;

    char *data_buf = (char *)kmalloc(len + 1);
    if (!data_buf) return -1;

    for (size_t i = 0; i < len; i++) {
        data_buf[i] = data[i];
    }
    data_buf[len] = '\0';

    node->data = data_buf;
    node->size = len;

    // Sync to ATA storage drive
    if (ata_is_detected()) {
        uint8_t sector_buf[512];
        for (int i = 0; i < 512; i++) sector_buf[i] = 0;
        size_t to_disk = (len < 512) ? len : 512;
        for (size_t i = 0; i < to_disk; i++) sector_buf[i] = (uint8_t)data[i];
        ata_write_sector(1000, sector_buf);
    }

    return (int)len;
}

int vfs_create_dir(const char *path) {
    if (!path || path[0] == '\0') return -1;
    if (vfs_find_node(path) != 0) return 0;

    const char *dname = path;
    if (dname[0] == '/') dname++;

    vfs_node_t *new_dir = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!new_dir) return -1;

    mystrcpy(new_dir->name, dname, sizeof(new_dir->name));
    new_dir->type = FS_DIRECTORY;
    new_dir->size = 0;
    new_dir->data = 0;
    new_dir->children = 0;

    new_dir->next = root_dir.children;
    root_dir.children = new_dir;

    return 0;
}

int vfs_delete_file(const char *path) {
    if (!path || path[0] == '\0') return -1;
    const char *fname = path;
    if (fname[0] == '/') fname++;

    vfs_node_t *prev = 0;
    vfs_node_t *curr = root_dir.children;

    while (curr) {
        if (mystrcmp(curr->name, fname) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                root_dir.children = curr->next;
            }
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1;
}
