#include "fs.h"
#include "logging.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>

static __declspec(thread) FsError g_last_error = FS_OK;

static void SetError(FsError err)
{
    g_last_error = err;
}

FsError Fs_LastError(void)
{
    return g_last_error;
}

char *Fs_ReadFile(const char *path, size_t *out_size)
{
    SetError(FS_OK);

    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            SetError(FS_ERR_NOT_FOUND);
        } else {
            Warn("Fs_ReadFile: failed to open '%s' (err=%lu)\n", path, (unsigned long)err);
            SetError(FS_ERR_OPEN);
        }
        return NULL;
    }

    LARGE_INTEGER size_li;
    if (!GetFileSizeEx(h, &size_li)) {
        Warn("Fs_ReadFile: GetFileSizeEx failed for '%s'\n", path);
        CloseHandle(h);
        SetError(FS_ERR_READ);
        return NULL;
    }

    if (size_li.QuadPart < 0 || (unsigned long long)size_li.QuadPart > (unsigned long long)(SIZE_MAX - 1)) {
        Warn("Fs_ReadFile: '%s' is too large to read into memory\n", path);
        CloseHandle(h);
        SetError(FS_ERR_TOO_LARGE);
        return NULL;
    }

    size_t size = (size_t)size_li.QuadPart;
    char *buf = malloc(size + 1);
    if (buf == NULL) {
        CloseHandle(h);
        SetError(FS_ERR_OOM);
        return NULL;
    }

    size_t total_read = 0;
    while (total_read < size) {
        DWORD to_read = (size - total_read) > MAXDWORD ? MAXDWORD : (DWORD)(size - total_read);
        DWORD bytes_read = 0;
        if (!ReadFile(h, buf + total_read, to_read, &bytes_read, NULL)) {
            Warn("Fs_ReadFile: ReadFile failed for '%s' (err=%lu)\n", path, (unsigned long)GetLastError());
            free(buf);
            CloseHandle(h);
            SetError(FS_ERR_READ);
            return NULL;
        }
        if (bytes_read == 0) {
            break;
        }
        total_read += bytes_read;
    }

    buf[total_read] = '\0';
    CloseHandle(h);

    if (out_size != NULL) {
        *out_size = total_read;
    }

    return buf;
}

static int WriteOrAppend(const char *path, const char *data, size_t size, int append)
{
    SetError(FS_OK);

    DWORD disposition = append ? OPEN_ALWAYS : CREATE_ALWAYS;
    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        Warn("Fs_%s: failed to open '%s' (err=%lu)\n",
             append ? "AppendFile" : "WriteFile", path, (unsigned long)GetLastError());
        SetError(FS_ERR_OPEN);
        return 0;
    }

    if (append) {
        LARGE_INTEGER zero = {0};
        if (!SetFilePointerEx(h, zero, NULL, FILE_END)) {
            Warn("Fs_AppendFile: seek to end failed for '%s'\n", path);
            CloseHandle(h);
            SetError(FS_ERR_WRITE);
            return 0;
        }
    }

    size_t total_written = 0;
    while (total_written < size) {
        DWORD to_write = (size - total_written) > MAXDWORD ? MAXDWORD : (DWORD)(size - total_written);
        DWORD bytes_written = 0;
        if (!WriteFile(h, data + total_written, to_write, &bytes_written, NULL)) {
            Warn("Fs_%s: WriteFile failed for '%s' (err=%lu)\n",
                 append ? "AppendFile" : "WriteFile", path, (unsigned long)GetLastError());
            CloseHandle(h);
            SetError(FS_ERR_WRITE);
            return 0;
        }
        if (bytes_written == 0) {
            Warn("Fs_%s: zero bytes written, aborting ('%s')\n",
                 append ? "AppendFile" : "WriteFile", path);
            CloseHandle(h);
            SetError(FS_ERR_WRITE);
            return 0;
        }
        total_written += bytes_written;
    }

    CloseHandle(h);
    return 1;
}

int Fs_WriteFile(const char *path, const char *data, size_t size)
{
    return WriteOrAppend(path, data, size, 0);
}

int Fs_AppendFile(const char *path, const char *data, size_t size)
{
    return WriteOrAppend(path, data, size, 1);
}
