#include "vfs.h"
#include "../drivers/vga.h"
#include "../mm/heap.h"

static vfs_node_t root_dir;
static vfs_node_t system_dir;
static vfs_node_t etc_dir;

static vfs_node_t file_readme;
static vfs_node_t file_osinfo;
static vfs_node_t file_hostname;
static vfs_node_t file_devices;

static const char readme_content[] =
    "Eran OS v0.2.0 - Operating System Foundation\n"
    "Built from bare metal x86 assembly to freestanding C.\n"
    "Featuring 32-bit Protected Mode, IDT exception handling,\n"
    "8259 PIC remapping, PIT 100Hz timer, PS/2 keyboard driver,\n"
    "Bitmap Physical Memory Manager, Dynamic Kernel Heap, PCI enumeration,\n"
    "Virtual Filesystem, and unprivileged Offline AI Shell.\n";

static const char osinfo_content[] =
    "OS Name     : Eran OS\n"
    "Version     : 0.2.0 (Milestone Complete)\n"
    "Architecture: x86 32-bit (IA-32 Protected Mode)\n"
    "Boot Method : Legacy BIOS MBR -> Protected Mode Handover\n"
    "Kernel Base : 0x00010000 Physical\n";

static const char hostname_content[] = "eranos-workstation\n";

static const char devices_content[] =
    "Discovered Controllers:\n"
    "- 8254 Programmable Interval Timer (IRQ 0)\n"
    "- 8042 PS/2 Keyboard Controller (IRQ 1)\n"
    "- 16550 UART Serial Controller (COM1 0x3F8)\n"
    "- Standard VGA Text Buffer (0xB8000)\n"
    "- PCI Bus Bridge & Peripherals\n";

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
    etc_dir.next = &file_devices;

    // /etc/hostname
    mystrcpy(file_hostname.name, "hostname", sizeof(file_hostname.name));
    file_hostname.type = FS_FILE;
    file_hostname.size = mystrlen(hostname_content);
    file_hostname.data = hostname_content;
    file_hostname.children = 0;
    file_hostname.next = 0;

    // /devices.txt
    mystrcpy(file_devices.name, "devices.txt", sizeof(file_devices.name));
    file_devices.type = FS_FILE;
    file_devices.size = mystrlen(devices_content);
    file_devices.data = devices_content;
    file_devices.children = 0;
    file_devices.next = 0;
}

vfs_node_t *vfs_get_root(void) {
    return &root_dir;
}

vfs_node_t *vfs_find_node(const char *path) {
    if (!path || path[0] == '\0') return 0;

    if (path[0] == '/') path++;
    if (path[0] == '\0') return &root_dir;

    // Check direct files in root
    vfs_node_t *curr = root_dir.children;
    while (curr) {
        if (mystrcmp(curr->name, path) == 0) {
            return curr;
        }
        curr = curr->next;
    }

    // Check subdirectories
    if (path[0] == 's' && path[1] == 'y' && path[2] == 's' && path[3] == 't' && path[4] == 'e' && path[5] == 'm' && path[6] == '/') {
        const char *subpath = path + 7;
        curr = system_dir.children;
        while (curr) {
            if (mystrcmp(curr->name, subpath) == 0) return curr;
            curr = curr->next;
        }
    }

    if (path[0] == 'e' && path[1] == 't' && path[2] == 'c' && path[3] == '/') {
        const char *subpath = path + 4;
        curr = etc_dir.children;
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
