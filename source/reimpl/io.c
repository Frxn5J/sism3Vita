/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/io.h"

#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <psp2/kernel/threadmgr.h>

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#include "utils/logger.h"
#include "utils/utils.h"

// Includes the following inline utilities:
// int oflags_musl_to_newlib(int flags);
// dirent64_bionic * dirent_newlib_to_bionic(struct dirent* dirent_newlib);
// void stat_newlib_to_bionic(struct stat * src, stat64_bionic * dst);
#include "reimpl/bits/_struct_converters.c"

// Marmalade addresses game files by relative path ("images/splash_ea.jpg").
// On Android those resolve through the APK provider; on Vita there is no
// equivalent CWD, so relative opens silently fail and image/archive parsers
// then allocate from uninitialized dimensions (the deterministic ~3.8 GB
// s3eMalloc). Redirect missing relative paths into the extracted assets.
static const char *redirect_relative_path(const char *path) {
    // Rotating pool: the result is consumed synchronously by the caller, and
    // Marmalade opens files from several threads at once.
    static char candidates[8][1024];
    static int next_slot;
    if (!path || !*path || path[0] == '/' || strchr(path, ':'))
        return path;
    char *slot = candidates[next_slot++ & 7];
    snprintf(slot, sizeof(candidates[0]), DATA_PATH "assets/%s", path);
    if (file_exists(slot)) {
        l_info("redirect: \"%s\" -> \"%s\"", path, slot);
        return slot;
    }
    snprintf(slot, sizeof(candidates[0]), DATA_PATH "%s", path);
    if (file_exists(slot)) {
        l_info("redirect: \"%s\" -> \"%s\"", path, slot);
        return slot;
    }
    return path;
}

// Marmalade also joins file_root ("ux0:data/thesims3/") with asset paths
// itself ("ux0:data/thesims3/images/splash_ea.jpg"), but the extracted tree
// keeps them under assets/. Remap those misses the same way.
static const char *redirect_data_path(const char *path) {
    static char slots[8][1024];
    static int next_slot;
    size_t base_len = strlen(DATA_PATH);
    if (!path || strncmp(path, DATA_PATH, base_len) != 0)
        return path;
    if (file_exists(path))
        return path;
    const char *rest = path + base_len;
    if (strncmp(rest, "assets/", 7) == 0)
        return path;
    char *slot = slots[next_slot++ & 7];
    snprintf(slot, sizeof(slots[0]), DATA_PATH "assets/%s", rest);
    if (file_exists(slot)) {
        l_info("redirect: \"%s\" -> \"%s\"", path, slot);
        return slot;
    }
    return path;
}

FILE * fopen_soloader(const char * filename, const char * mode) {
    if (strcmp(filename, "/proc/cpuinfo") == 0) {
        return fopen_soloader("app0:/cpuinfo", mode);
    } else if (strcmp(filename, "/proc/meminfo") == 0) {
        return fopen_soloader("app0:/meminfo", mode);
    }

    filename = redirect_data_path(redirect_relative_path(filename));

#ifdef USE_SCELIBC_IO
    FILE* ret = sceLibcBridge_fopen(filename, mode);
#else
    FILE* ret = fopen(filename, mode);
#endif

    if (ret)
        l_info("fopen(%s, %s): %p", filename, mode, ret);
    else
        l_warn("fopen(%s, %s): %p", filename, mode, ret);

    return ret;
}

int open_soloader(const char * path, int oflag, ...) {
    if (strcmp(path, "/proc/cpuinfo") == 0) {
        return open_soloader("app0:/cpuinfo", oflag);
    } else if (strcmp(path, "/proc/meminfo") == 0) {
        return open_soloader("app0:/meminfo", oflag);
    } else if (strcmp(path, "/dev/urandom") == 0) {
        return open_soloader("app0:/urandom", oflag);
    }

    path = redirect_data_path(redirect_relative_path(path));

    mode_t mode = 0666;
    if (((oflag & BIONIC_O_CREAT) == BIONIC_O_CREAT) ||
        ((oflag & BIONIC_O_TMPFILE) == BIONIC_O_TMPFILE)) {
        va_list args;
        va_start(args, oflag);
        mode = (mode_t)(va_arg(args, int));
        va_end(args);
    }

    oflag = oflags_bionic_to_newlib(oflag);
    int ret = open(path, oflag, mode);
    if (ret >= 0)
        l_info("open(%s, %x): %i", path, oflag, ret);
    else
        l_warn("open(%s, %x): %i", path, oflag, ret);
    return ret;
}

long read_soloader(int fd, void *buf, unsigned int count) {
    long ret = read(fd, buf, count);
    if (ret < 0 || (unsigned long)ret != count) {
        // Offset query is read-only (SEEK_CUR) and only runs on the rare
        // short path: it tells EOF (offset+ret == size) apart from a
        // mid-file truncation.
        long off = lseek(fd, 0, SEEK_CUR);
        l_warn("read(fd=%d, %u): short/failed ret=%li offset=%li from %p",
               fd, count, ret, off, __builtin_return_address(0));
    }
    return ret;
}

long lseek_soloader(int fd, long offset, int whence) {
    long ret = lseek(fd, offset, whence);
    // Diagnostic: full seek trace (the .s3e loader seeks near-EOF footers).
    l_info("lseek(fd=%d, %li, %d): ret=%li", fd, offset, whence, ret);
    return ret;
}

unsigned int fread_soloader(void *ptr, unsigned int size, unsigned int nmemb,
                            void *stream) {
#ifdef USE_SCELIBC_IO
    unsigned int ret = sceLibcBridge_fread(ptr, size, nmemb, (FILE *)stream);
#else
    unsigned int ret = fread(ptr, size, nmemb, (FILE *)stream);
#endif
    if (ret != nmemb) {
        l_warn("fread(%p, %u, %u, %p): short ret=%u from %p", ptr, size,
               nmemb, stream, ret, __builtin_return_address(0));
    }
    return ret;
}

int fseek_soloader(FILE *stream, long offset, int whence) {
#ifdef USE_SCELIBC_IO
    int ret = sceLibcBridge_fseek(stream, offset, whence);
#else
    int ret = fseek(stream, offset, whence);
#endif
    if (ret != 0) {
        l_warn("fseek(%p, %li, %d): failed ret=%d from %p", stream, offset,
               whence, ret, __builtin_return_address(0));
    }
    return ret;
}

off64_soloader_t lseek64_soloader(int fd, off64_soloader_t offset, int whence) {
    if (offset > (off64_soloader_t)LONG_MAX || offset < (off64_soloader_t)LONG_MIN) {
        l_error("lseek64(%i, %lli, %i): offset out of 32-bit range", fd,
                (long long)offset, whence);
        return -1;
    }
    long ret = lseek(fd, (long)offset, whence);
    if (ret < 0)
        l_warn("lseek64(%i, %lli, %i): %li", fd, (long long)offset, whence, ret);
    else
        l_debug("lseek64(%i, %lli, %i): %li", fd, (long long)offset, whence, ret);
    return (off64_soloader_t)ret;
}

int fstat_soloader(int fd, stat64_bionic * buf) {
    struct stat st;
    int res = fstat(fd, &st);

    if (res == 0) {
        stat_newlib_to_bionic(&st, buf);
        l_info("fstat(fd=%d): ok, size=%lli", fd, (long long)st.st_size);
    } else {
        l_warn("fstat(fd=%d): %i", fd, res);
    }
    return res;
}

int stat_soloader(const char * path, stat64_bionic * buf) {
    if (strcmp(path, "/system/lib/libOpenSLES.so") == 0) {
        l_debug("stat(%s): returning 0 in case this is a check for OpenSLES support", path);
        return 0;
    }

    path = redirect_data_path(redirect_relative_path(path));

    struct stat st;
    int res = stat(path, &st);

    if (res == 0) {
        stat_newlib_to_bionic(&st, buf);
        l_info("stat(%s): ok, size=%lli", path, (long long)st.st_size);
    } else {
        l_warn("stat(%s): %i", path, res);
    }
    return res;
}

int fclose_soloader(FILE * f) {
#ifdef USE_SCELIBC_IO
    int ret = sceLibcBridge_fclose(f);
#else
    int ret = fclose(f);
#endif

    l_debug("fclose(%p): %i", f, ret);
    return ret;
}

int close_soloader(int fd) {
    int ret = close(fd);
    l_debug("close(%i): %i", fd, ret);
    return ret;
}

DIR* opendir_soloader(char* _pathname) {
    _pathname = (char *)redirect_data_path(redirect_relative_path(_pathname));
    DIR* ret = opendir(_pathname);
    if (ret)
        l_info("opendir(\"%s\"): %p", _pathname, ret);
    else
        l_warn("opendir(\"%s\"): %p", _pathname, ret);
    return ret;
}

// POSIX allows readdir() to reuse one buffer per DIR stream (glibc does
// exactly that), but it must NOT share a single buffer across all streams:
// Marmalade holds entries from one listing while reading another, so a
// global buffer makes every held entry alias the last name read.
#define READDIR_SLOTS 16
static struct {
    int used;
    DIR *dir;
    dirent64_bionic ent;
} readdir_slots[READDIR_SLOTS];
static int readdir_slot_next;

struct dirent64_bionic * readdir_soloader(DIR * dir) {
    struct dirent* ret = readdir(dir);

    if (ret) {
        // Diagnostic: show exactly what Marmalade sees in directory listings.
        l_info("readdir(%p): \"%s\"", dir, ret->d_name);
        int slot = -1;
        for (int i = 0; i < READDIR_SLOTS; i++) {
            if (readdir_slots[i].used && readdir_slots[i].dir == dir) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            slot = readdir_slot_next++ % READDIR_SLOTS;
            readdir_slots[slot].used = 1;
            readdir_slots[slot].dir = dir;
        }
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(ret);
        memcpy(&readdir_slots[slot].ent, entry_tmp,
               sizeof(dirent64_bionic));
        free(entry_tmp);
        return &readdir_slots[slot].ent;
    }

    l_info("readdir(%p): end of listing", dir);
    return NULL;
}

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result) {
    struct dirent dirent_tmp;
    struct dirent * pdirent_tmp;

    int ret = readdir_r(dirp, &dirent_tmp, &pdirent_tmp);

    if (ret == 0) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(&dirent_tmp);
        memcpy(entry, entry_tmp, sizeof(dirent64_bionic));
        *result = (pdirent_tmp != NULL) ? entry : NULL;
        free(entry_tmp);
    }

    l_debug("readdir_r(%p, %p, %p): %i", dirp, entry, result, ret);
    return ret;
}

int closedir_soloader(DIR * dir) {
    int ret = closedir(dir);
    l_debug("closedir(%p): %i", dir, ret);
    return ret;
}

int fcntl_soloader(int fd, int cmd, ...) {
    l_warn("fcntl(%i, %i, ...): not implemented", fd, cmd);
    return 0;
}

int ioctl_soloader(int fd, int request, ...) {
    l_warn("ioctl(%i, %i, ...): not implemented", fd, request);
    return 0;
}

int fsync_soloader(int fd) {
    int ret = fsync(fd);
    l_debug("fsync(%i): %i", fd, ret);
    return ret;
}

int access_soloader(const char *path, int mode) {
    const char *orig = path;
    path = redirect_data_path(redirect_relative_path(path));
    int ret = access(path, mode);
    if (ret == 0 && path != orig)
        l_info("access(\"%s\"): ok (redirected)", orig);
    else if (ret < 0)
        l_warn("access(\"%s\"): -1", orig);
    return ret;
}

int mkdir_soloader(const char *path, unsigned int mode) {
    path = redirect_data_path(redirect_relative_path(path));
    int ret = mkdir(path, (mode_t)mode);
    if (ret < 0)
        l_warn("mkdir(\"%s\"): failed", path);
    else
        l_info("mkdir(\"%s\"): ok", path);
    return ret;
}

int unlink_soloader(const char *path) {
    path = redirect_data_path(redirect_relative_path(path));
    return unlink(path);
}

int rename_soloader(const char *oldpath, const char *newpath) {
    oldpath = redirect_data_path(redirect_relative_path(oldpath));
    newpath = redirect_data_path(redirect_relative_path(newpath));
    return rename(oldpath, newpath);
}

int remove_soloader(const char *path) {
    path = redirect_data_path(redirect_relative_path(path));
    return remove(path);
}
