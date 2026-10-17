#pragma once
#include <stdbool.h>
#include <stdlib.h>

bool fs_init(void);                 /* mounts the first card that has /games */
const char *fs_root(void);          /* "gcldr", "sdb", ... ("" if none) */
const char *fs_label(void);         /* human name of the device */
char *fs_path(char *buf, int n, const char *rel);   /* "/cubemenu/x" -> "gcldr:/cubemenu/x" */
void fs_shutdown(void);            /* unmount (flushes FAT) before handing over */
bool fs_writable(void);              /* false on the read-only emulator disc */
bool fs_exists(const char *path);
void *fs_read_all(const char *path, int *len);       /* NUL-terminated, free() it */
bool fs_write_all(const char *path, const void *data, int len);
