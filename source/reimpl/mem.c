/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/mem.h"
#include "utils/logger.h"

#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include <psp2/kernel/clib.h>

void *sceClibMemclr(void *dst, size_t len) {
    return sceClibMemset(dst, 0, len);
}

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

// Diagnostic guard: Marmalade aborts on a deterministic ~3.8 GB s3eMallocOS
// request while streaming res.dz. Log huge requests with the caller's PC so
// the next boot.log pinpoints the exact .so function computing the size.
#define MALLOC_TRACE_THRESHOLD (64u * 1024u * 1024u)

void *malloc_soloader(size_t size) {
    if (size >= MALLOC_TRACE_THRESHOLD) {
        l_error("malloc(%u): huge request from %p", (unsigned int)size,
                __builtin_return_address(0));
    }
    void *ret = malloc(size);
    // Diagnostic: NULL returns are silent killers (loader reports generic
    // "Can't open s3e file" when an intermediate buffer fails).
    if (!ret && size) {
        l_error("malloc(%u): FAILED (out of memory) from %p",
                (unsigned int)size, __builtin_return_address(0));
    }
    return ret;
}

void *calloc_soloader(size_t nmemb, size_t size) {
    if (size != 0 && nmemb > SIZE_MAX / size) {
        l_error("calloc(%u, %u): size overflow from %p", (unsigned int)nmemb,
                (unsigned int)size, __builtin_return_address(0));
        return NULL;
    }
    if (nmemb * size >= MALLOC_TRACE_THRESHOLD) {
        l_error("calloc(%u, %u): huge request from %p", (unsigned int)nmemb,
                (unsigned int)size, __builtin_return_address(0));
    }
    void *ret = calloc(nmemb, size);
    if (!ret && nmemb && size) {
        l_error("calloc(%u, %u): FAILED (out of memory) from %p",
                (unsigned int)nmemb, (unsigned int)size,
                __builtin_return_address(0));
    }
    return ret;
}

void *realloc_soloader(void *ptr, size_t size) {
    if (size >= MALLOC_TRACE_THRESHOLD) {
        l_error("realloc(%p, %u): huge request from %p", ptr,
                (unsigned int)size, __builtin_return_address(0));
    }
    void *ret = realloc(ptr, size);
    if (!ret && size) {
        l_error("realloc(%p, %u): FAILED (out of memory) from %p", ptr,
                (unsigned int)size, __builtin_return_address(0));
    }
    return ret;
}

void *memalign_soloader(size_t alignment, size_t size) {
    if (size >= MALLOC_TRACE_THRESHOLD) {
        l_error("memalign(%u, %u): huge request from %p", (unsigned int)alignment,
                (unsigned int)size, __builtin_return_address(0));
    }
    void *ret = memalign(alignment, size);
    if (!ret && size) {
        l_error("memalign(%u, %u): FAILED (out of memory) from %p",
                (unsigned int)alignment, (unsigned int)size,
                __builtin_return_address(0));
    }
    return ret;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offs) {
    l_warn("mmap(%p, %i, %i, %i, %i, %li)", addr, length, prot, flags, fd, offs);

    if (length <= 0) {
        return MAP_FAILED;
    }
    if (!(flags & MAP_ANONYMOUS)) {
        // A file-backed mapping must reflect the file's contents. Returning
        // zeroed memory here silently corrupts every parser that mmaps an
        // archive (e.g. Marmalade's DTRZ reader), so fail loudly and let the
        // caller fall back to fread/fseek instead.
        l_error("mmap: file-backed mapping (fd=%i) is not supported", fd);
        return MAP_FAILED;
    }
    void* ret= malloc(length);
    memset(ret, 0, length);
    return ret;
}

int munmap(void *addr, size_t length) {
    if (addr) free(addr);
    return 0;
}
