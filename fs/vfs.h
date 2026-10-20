#ifndef ERAN_FS_VFS_H
#define ERAN_FS_VFS_H

#include <stdint.h>
#include <stddef.h>

#define FS_FILE      0x01
#define FS_DIRECTORY 0x02

typedef struct vfs_node {
    char name[32];
    uint8_t type;
    size_t size;
    const char *data;
    struct vfs_node *children;
    struct vfs_node *next;
} vfs_node_t;

void vfs_init(void);
vfs_node_t *vfs_get_root(void);
vfs_node_t *vfs_find_node(const char *path);
void vfs_list_dir(const char *path);
int vfs_read_file(const char *path, char *buf, size_t max_len);

#endif /* ERAN_FS_VFS_H */
