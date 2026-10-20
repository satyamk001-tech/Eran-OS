#include "vfs.h"
#include "../drivers/vga.h"
#include "../drivers/ata.h"
#include "../mm/heap.h"
#include "../user/hello_elf_data.h"

static vfs_node_t root_dir;
static vfs_node_t system_dir;
static vfs_node_t etc_dir;
static vfs_node_t bin_dir;

static vfs_node_t file_readme;
static vfs_node_t file_osinfo;
static vfs_node_t file_hostname;
static vfs_node_t file_devices;
static vfs_node_t file_hello_elf;

static const char readme_content[] =
    "Eran OS v0.4.0 - Operating System Foundation\n"
    "Built from bare metal x86 assembly to freestanding C.\n"
    "Featuring 32-bit Protected Mode, IDT exception handling,\n"
    "8259 PIC remapping, PIT 100Hz timer, PS/2 keyboard & mouse driver,\n"
    "Bitmap Physical Memory Manager, Dynamic Kernel Heap, Two-Level Paging,\n"
    "Intel e1000 Gigabit Network Stack, PC Speaker Audio Synthesizer,\n"
    "2D Graphical Desktop Compositor, ACPI Power Management, CPUID prober,\n"
    "ELF32 Executable Binary Loader, and unprivileged Offline AI Engine.\n";

static const char osinfo_content[] =
    "OS Name     : Eran OS\n"
    "Version     : 0.4.0 (Advanced Kernel Milestones Active)\n"
    "Architecture: x86 32-bit (IA-32 Protected Mode & Long Mode Probe)\n"
    "Boot Method : Legacy BIOS MBR -> Protected Mode Handover\n"
    "Kernel Base : 0x00010000 Physical\n";

static const char hostname_content[] = "eranos-workstation\n";

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

void vfs_init(void) {
    // Root directory /
    mystrcpy(root_dir.name, "/", sizeof(root_dir.name));
    root_dir.type = FS_DIRECTORY;
    root_dir.children = &file_readme;
    root_dir.next = 0;

    // /README.txt
    mystrcpy(file_readme.name, "README.txt", sizeof(file_readme.name));
    file_readme.type = FS_FILE;
    file_readme.size = mystrlen(readme_content);
    file_readme.data = readme_content;
    file_readme.children = 0;
    file_readme.next = &system_dir;

    // /system/
    mystrcpy(system_dir.name, "system", sizeof(system_dir.name));
    system_dir.type = FS_DIRECTORY;
    system_dir.size = 0;
    system_dir.children = &file_osinfo;
    system_dir.next = &etc_dir;

    // /system/os_info.txt
    mystrcpy(file_osinfo.name, "os_info.txt", sizeof(file_osinfo.name));
    file_osinfo.type = FS_FILE;
    file_osinfo.size = mystrlen(osinfo_content);
    file_osinfo.data = osinfo_content;
    file_osinfo.children = 0;
    file_osinfo.next = 0;

    // /etc/
    mystrcpy(etc_dir.name, "etc", sizeof(etc_dir.name));
    etc_dir.type = FS_DIRECTORY;
    etc_dir.size = 0;
    etc_dir.children = &file_hostname;
    etc_dir.next = &bin_dir;

    // /etc/hostname
    mystrcpy(file_hostname.name, "hostname", sizeof(file_hostname.name));
    file_hostname.type = FS_FILE;
    file_hostname.size = mystrlen(hostname_content);
    file_hostname.data = hostname_content;
    file_hostname.children = 0;
    file_hostname.next = &file_devices;

    // /etc/devices.txt
    mystrcpy(file_devices.name, "devices.txt", sizeof(file_devices.name));
    file_devices.type = FS_FILE;
    file_devices.size = mystrlen(devices_content);
    file_devices.data = devices_content;
    file_devices.children = 0;
    file_devices.next = 0;

    // /bin/ (User Executables Directory)
    mystrcpy(bin_dir.name, "bin", sizeof(bin_dir.name));
    bin_dir.type = FS_DIRECTORY;
    bin_dir.size = 0;
    bin_dir.children = &file_hello_elf;
    bin_dir.next = 0;

    // /bin/hello.elf (Compiled Standalone Ring 3 ELF32 Binary)
    mystrcpy(file_hello_elf.name, "hello.elf", sizeof(file_hello_elf.name));
    file_hello_elf.type = FS_FILE;
    file_hello_elf.size = sizeof(build_hello_elf);
    file_hello_elf.data = (const char *)build_hello_elf;
    file_hello_elf.children = 0;
    file_hello_elf.next = 0;
}

vfs_node_t *vfs_get_root(void) {
    return &root_dir;
}

vfs_node_t *vfs_find_node(const char *path) {
    if (!path || path[0] == '\0') return 0;

    if (path[0] == '/') path++;
    if (path[0] == '\0') return &root_dir;

    // Direct match in root
    vfs_node_t *curr = root_dir.children;
    while (curr) {
        if (mystrcmp(curr->name, path) == 0) return curr;
        curr = curr->next;
    }

    // Check /system/
    if (path[0] == 's' && path[1] == 'y' && path[2] == 's' && path[3] == 't' && path[4] == 'e' && path[5] == 'm' && path[6] == '/') {
        const char *subpath = path + 7;
        curr = system_dir.children;
        while (curr) {
            if (mystrcmp(curr->name, subpath) == 0) return curr;
            curr = curr->next;
        }
    }

    // Check /etc/
    if (path[0] == 'e' && path[1] == 't' && path[2] == 'c' && path[3] == '/') {
        const char *subpath = path + 4;
        curr = etc_dir.children;
        while (curr) {
            if (mystrcmp(curr->name, subpath) == 0) return curr;
            curr = curr->next;
        }
    }

    // Check /bin/
    if (path[0] == 'b' && path[1] == 'i' && path[2] == 'n' && path[3] == '/') {
        const char *subpath = path + 4;
        curr = bin_dir.children;
        while (curr) {
            if (mystrcmp(curr->name, subpath) == 0) return curr;
            curr = curr->next;
        }
    }

    return 0;
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
            vga_puts_color("  [FILE] ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            kprintf("%s (%d bytes)\n", child->name, child->size);
        }
        child = child->next;
    }
}

int vfs_read_file(const char *path, char *buf, size_t max_len) {
    vfs_node_t *node = vfs_find_node(path);
    if (!node || node->type != FS_FILE || !node->data) {
        return -1;
    }
    size_t to_copy = (node->size < max_len - 1) ? node->size : (max_len - 1);
    for (size_t i = 0; i < to_copy; i++) {
        buf[i] = node->data[i];
    }
    buf[to_copy] = '\0';
    return (int)to_copy;
}

int vfs_create_file(const char *path, const char *initial_data) {
    if (!path || path[0] == '\0') return -1;

    // Check if file already exists
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

    // If ATA storage drive is detected, flush block to disk sector 1000
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
