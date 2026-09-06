/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/sys.h"

#include <sys/errno.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <string.h>
#include <stdint.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/rtc.h>
#include <stdlib.h>
#include <kubridge.h>
#include <psp2/kernel/sysmem.h>

#include "utils/utils.h"
#include "utils/logger.h"
#include "utils/dialog.h"

#define BIONIC_CLOCK_REALTIME           0
#define BIONIC_CLOCK_MONOTONIC          1
#define BIONIC_CLOCK_PROCESS_CPUTIME_ID 2
#define BIONIC_CLOCK_THREAD_CPUTIME_ID  3
#define BIONIC_CLOCK_MONOTONIC_RAW      4
#define BIONIC_CLOCK_REALTIME_COARSE    5
#define BIONIC_CLOCK_MONOTONIC_COARSE   6
#define BIONIC_CLOCK_BOOTTIME           7
#define BIONIC_CLOCK_REALTIME_ALARM     8
#define BIONIC_CLOCK_BOOTTIME_ALARM     9
#define BIONIC_CLOCK_SGI_CYCLE         10
#define BIONIC_CLOCK_TAI               11

// 1969 years in microseconds, used to adjust SCE tick to UNIX timestamp
#define __epoch 62135587294000000

int clock_gettime_soloader(clockid_t clock_id, struct timespec * tp) {
    switch (clock_id) {
        case BIONIC_CLOCK_MONOTONIC:
        case BIONIC_CLOCK_MONOTONIC_RAW:
        case BIONIC_CLOCK_MONOTONIC_COARSE:
        case BIONIC_CLOCK_BOOTTIME:
        case BIONIC_CLOCK_BOOTTIME_ALARM:
        case BIONIC_CLOCK_SGI_CYCLE:
        case BIONIC_CLOCK_PROCESS_CPUTIME_ID:
        case BIONIC_CLOCK_THREAD_CPUTIME_ID: {
            uint64_t proctime = sceKernelGetProcessTimeWide();

            tp->tv_sec = (proctime / 1000000);
            tp->tv_nsec = ((proctime - (tp->tv_sec * 1000000)) * 1000);
            break;
        }
        case BIONIC_CLOCK_REALTIME:
        case BIONIC_CLOCK_REALTIME_COARSE:
        case BIONIC_CLOCK_REALTIME_ALARM:
        case BIONIC_CLOCK_TAI: {
            SceRtcTick tick;
            sceRtcGetCurrentTick(&tick);
            tick.tick -= __epoch;

            tp->tv_sec = (tick.tick / 1000000);
            tp->tv_nsec = ((tick.tick - (tp->tv_sec * 1000000)) * 1000);
            break;
        }
        default:
            l_error("clock_gettime / unexpected clock id %i", clock_id);
    }

    return 0;
}

int clock_getres_soloader(clockid_t clock_id, struct timespec * res) {
    res->tv_sec = 0;
    res->tv_nsec = 1000;
    return 0;
}

clock_t clock_soloader(void) {
    return sceKernelGetProcessTimeLow();
}

int sigaction(int signum, const struct sigaction * act, struct sigaction * oldact) {
    l_warn("sigaction(%i, ...): not implemented", signum);
    return 0;
}

int __system_property_get_soloader(const char *name, char *value) {
    l_warn("__system_property_get(%s, %p): not implemented", name, value);
    strncpy(value, "psvita", 7);
    return 7;
}

void assert2(const char* f, int l, const char* func, const char* msg) {
    l_fatal("[%s:%i][%s] Assertion failed: %s", f, l, func, msg);
}

void syscall(int c) {
    l_warn("syscall(%i): not implemented", c);
}

void __stack_chk_fail_soloader() {
    l_fatal("Stack collapsed at address %p", __builtin_return_address(0));
}

void abort_soloader() {
    l_fatal("Abort called from address %p", __builtin_return_address(0));
    abort();
}

void exit_soloader(int status) {
    l_fatal("Exit(%i) called from %p", status, __builtin_return_address(0));
    exit(status);
}

int __atomic_dec(volatile int *ptr) {
    return __sync_fetch_and_sub(ptr, 1);
}

int __atomic_inc(volatile int *ptr) {
    return __sync_fetch_and_add(ptr, 1);
}

int __atomic_swap(int new_value, volatile int *ptr) {
    int old_value;
    do {
        old_value = *ptr;
    } while (__sync_val_compare_and_swap(ptr, old_value, new_value) != old_value);
    return old_value;
}

int __atomic_cmpxchg(int old_value, int new_value, volatile int* ptr) {
    /* We must return 0 on success */
    return __sync_val_compare_and_swap(ptr, old_value, new_value) != old_value;
}

char * getenv_soloader(const char * var) {
    l_warn("getenv(\"%s\"): not implemented.", var);
    return NULL;
}

int setenv_soloader(const char * name, const char * value, int overwrite) {
    l_warn("setenv(\"%s\", \"%s\"): not implemented.", name, value);
    return 0;
}

int getpagesize(void) {
    return PAGE_SIZE;
}

typedef struct CodeBlock {
	void *base;
	size_t size;
	SceUID uid;
	struct CodeBlock *next;
} CodeBlock;

static CodeBlock *code_blocks;

void *marmalade_code_alloc(size_t len) {
	if (!len || len > SIZE_MAX - (PAGE_SIZE - 1))
		fatal_error("Invalid Marmalade code size: 0x%x", len);
	size_t size = (len + PAGE_SIZE - 1) & ~(size_t)(PAGE_SIZE - 1);
	CodeBlock *block = calloc(1, sizeof(*block));
	if (!block) fatal_error("Could not allocate code block metadata.");
	SceUID uid = sceKernelAllocMemBlock("MarmaladeCode", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, size, NULL);
	if (uid < 0) {
		free(block);
		fatal_error("Could not allocate Marmalade code (0x%x bytes): 0x%x", size, uid);
	}
	int ret = sceKernelGetMemBlockBase(uid, &block->base);
	if (ret >= 0) {
		memset(block->base, 0, size);
		ret = kuKernelMemProtect(block->base, size,
			KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE | KU_KERNEL_PROT_EXEC);
	}
	if (ret < 0) {
		sceKernelFreeMemBlock(uid);
		free(block);
		fatal_error("Could not enable Marmalade code execution: 0x%x", ret);
	}
	block->uid = uid;
	block->size = size;
	block->next = code_blocks;
	code_blocks = block;
	l_info("Marmalade code arena: %p + 0x%x, uid=0x%x, RWX", block->base, size, uid);
	return block->base;
}

void marmalade_code_free(void *addr) {
	if (!addr) return;
	for (CodeBlock **link = &code_blocks; *link; link = &(*link)->next) {
		CodeBlock *block = *link;
		if (block->base != addr) continue;
		int ret = sceKernelFreeMemBlock(block->uid);
		if (ret < 0) fatal_error("Could not free Marmalade code %p: 0x%x", addr, ret);
		*link = block->next;
		free(block);
		return;
	}
	fatal_error("Attempt to free an unknown Marmalade code arena: %p", addr);
}

int mprotect_soloader(void *addr, size_t len, int prot) {
	uintptr_t start = (uintptr_t)addr;
	if ((start & (PAGE_SIZE - 1)) || (prot & ~7) ||
	    len > SIZE_MAX - (PAGE_SIZE - 1)) {
		errno = EINVAL;
		return -1;
	}
	if (!len) return 0;
	size_t size = (len + PAGE_SIZE - 1) & ~(size_t)(PAGE_SIZE - 1);
	if (size > UINTPTR_MAX - start) {
		errno = EINVAL;
		return -1;
	}
	for (CodeBlock *block = code_blocks; block; block = block->next) {
		uintptr_t base = (uintptr_t)block->base;
		if (start >= base + block->size || start + size <= base) continue;
		if (start < base || start + size > base + block->size || (prot != 5 && prot != 7))
			fatal_error("Unexpected code protection: %p + 0x%x, prot=0x%x", addr, len, prot);
		// kubridge splits remap untouched pieces with the block's original RW policy.
		// Keep only these dedicated code arenas uniformly RWX, never split them.
		l_info("mprotect(%p, %zu, 0x%x): retaining dedicated code arena RWX", addr, len, prot);
		return 0;
	}
	int vita_prot = KU_KERNEL_PROT_NONE;
	if (prot & 1) vita_prot |= KU_KERNEL_PROT_READ;
	if (prot & 2) vita_prot |= KU_KERNEL_PROT_WRITE;
	if (prot & 4) vita_prot |= KU_KERNEL_PROT_EXEC;

	int ret = kuKernelMemProtect(addr, len, vita_prot);
	if (ret < 0) {
		l_warn("mprotect(%p, %zu, 0x%x): failed 0x%x", addr, len, prot, ret);
		if (prot & 4) fatal_error("Executable mprotect failed: %p + 0x%x, error=0x%x", addr, len, ret);
		errno = EACCES;
		return -1;
	}

	l_info("mprotect(%p, %zu, 0x%x): applied", addr, len, prot);
	return 0;
}

int statfs_soloader(const char *path, void *buf) {
    l_warn("statfs(%s): not implemented -> -1", path);
    if (buf) memset(buf, 0, 64);
    return -1;
}

int uname_soloader(void *buf) {
    l_warn("uname: stub -> filling linux-like utsname");
    if (!buf) return -1;
    struct { char sysname[65]; char nodename[65]; char release[65]; char version[65]; char machine[65]; char domainname[65]; } *u = buf;
    memset(u, 0, sizeof(*u));
    strncpy(u->sysname, "Linux", sizeof(u->sysname)-1);
    strncpy(u->nodename, "psvita", sizeof(u->nodename)-1);
    strncpy(u->release, "3.10.0", sizeof(u->release)-1);
    strncpy(u->version, "#1", sizeof(u->version)-1);
    strncpy(u->machine, "armv7l", sizeof(u->machine)-1);
    return 0;
}
