#ifndef FS_H
#define FS_H

#include <stddef.h>

char *Fs_ReadFile(const char *path, size_t *out_size);
int Fs_WriteFile(const char *path, const char *data, size_t size);
int Fs_AppendFile(const char *path, const char *data, size_t size);

typedef enum {
    FS_OK = 0,
    FS_ERR_NOT_FOUND,
    FS_ERR_OPEN,
    FS_ERR_READ,
    FS_ERR_WRITE,
    FS_ERR_OOM,
    FS_ERR_TOO_LARGE
} FsError;

FsError Fs_LastError(void);

#endif
