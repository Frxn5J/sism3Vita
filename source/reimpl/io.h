/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  io.h
 * @brief Wrappers and implementations for some of the IO functions.
 */

#ifndef SOLOADER_IO_H
#define SOLOADER_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/dirent.h>
#include <sys/syslimits.h>
#include <sys/fcntl.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef DT_DIR
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14
#endif

typedef struct __attribute__((__packed__)) stat64_bionic {
    unsigned long long st_dev;
    unsigned char __pad0[4];
    unsigned long __st_ino;
    unsigned int st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    unsigned long long st_rdev;
    unsigned char __pad3[4];
    long long st_size;
    unsigned long st_blksize;
    unsigned long long st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    unsigned long long st_ino;
} stat64_bionic;

// Bionic (Android, ARM) `struct dirent` as seen by `readdir()`.
// The game imports 32-bit `readdir`, so this must match Bionic's layout:
// u64 ino, s64 off, u16 reclen, u8 type, name[256].
typedef struct __attribute__((__packed__)) dirent64_bionic {
    uint64_t d_ino; // 8 bytes // offset 0x0
    int64_t d_off; // 8 bytes // offset 0x8
    uint16_t d_reclen; // 2 bytes // offset 0x10
    unsigned char d_type; // 1 byte // offset 0x12
    char d_name[256]; // 256 bytes // offset 0x13
} dirent64_bionic;

int open_soloader(const char * path, int oflag, ...);

// Diagnostic wrappers: only failures/short I/O is logged, so healthy
// streaming stays silent while truncated reads stand out.
long read_soloader(int fd, void *buf, unsigned int count);
long lseek_soloader(int fd, long offset, int whence);
unsigned int fread_soloader(void *ptr, unsigned int size, unsigned int nmemb,
                            void *stream);
int fseek_soloader(FILE *stream, long offset, int whence);

// Path-redirecting wrappers: Marmalade's existence checks (s3eFileCheckExists
// and friends) use access()/mkdir()/unlink() directly, bypassing open/stat.
int access_soloader(const char *path, int mode);
int mkdir_soloader(const char *path, unsigned int mode);
int unlink_soloader(const char *path);
int rename_soloader(const char *oldpath, const char *newpath);
int remove_soloader(const char *path);

typedef long long off64_soloader_t;
off64_soloader_t lseek64_soloader(int fd, off64_soloader_t offset, int whence);

FILE * fopen_soloader(const char * filename, const char * mode);

DIR *opendir_soloader(char *name);

int stat_soloader(const char * path, stat64_bionic * buf);

int fstat_soloader(int fd, stat64_bionic * buf);

struct dirent64_bionic * readdir_soloader(DIR *dir);

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result);

int close_soloader(int fd);

int fclose_soloader(FILE *f);

int closedir_soloader(DIR *dir);

int fcntl_soloader(int fd, int cmd, ...);

int ioctl_soloader(int fd, int request, ... /* arg */);

int fsync_soloader(int fd);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_IO_H
