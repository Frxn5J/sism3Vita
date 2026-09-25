/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>

#ifdef __cplusplus
extern "C"
{
#endif
	extern so_module so_mod;
#ifdef __cplusplus
};
#endif

#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RX (0x0C20D050)
#define MARMALADE_CACHEFLUSH_OFFSET       0x2d844
#define MARMALADE_CODE_ALLOC_OFFSET       0x4f9c8
#define MARMALADE_CODE_FREE_OFFSET        0x4f234
#define MARMALADE_MALLOC_BASE_OFFSET      0x5022c
#define MARMALADE_DEVICE_EXIT_OFFSET      0x41ce0
#define MARMALADE_DEVICE_REQ_QUIT_OFFSET  0x42c7c
#define MARMALADE_S3E_FILE_OPEN_CORE_OFFSET 0x493e8
#define MALLOC_TRACE_THRESHOLD            (64u * 1024u * 1024u)

#include "utils/logger.h"
#include "utils/dialog.h"
#include "reimpl/sys.h"
#include <stdbool.h>

void __kuser_memory_barrier(void) {
	__sync_synchronize();
}

void kuser_patch(void) {
	SceKernelAllocMemBlockKernelOpt opt;
	memset(&opt, 0, sizeof(SceKernelAllocMemBlockKernelOpt));
	opt.size = sizeof(SceKernelAllocMemBlockKernelOpt);
	opt.attr = 0x1;
	opt.field_C = (SceUInt32)0x9A000000;
	if (kuKernelAllocMemBlock("atomic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RX, 0x1000, &opt) < 0)
		fatal_error("Error could not allocate atomic block.");
	int ret = kuKernelMemProtect((void *)0x9A000000, (SceSize)0x1000,
		KU_KERNEL_PROT_EXEC | KU_KERNEL_PROT_READ | KU_KERNEL_PROT_WRITE);
	if (ret < 0)
		fatal_error("Could not protect atomic helpers: 0x%x", ret);

	hook_addr(0x9A000FA0, (uintptr_t)__kuser_memory_barrier);
	hook_addr(0x9A000FC0, (uintptr_t)__atomic_cmpxchg);
	kuKernelFlushCaches((void *)0x9A000000, 0x1000);

	uint32_t patched_addr;
	for (uint32_t addr = so_mod.text_base; addr < so_mod.text_base + so_mod.text_size; addr += 4) {
		uint32_t *a = (uint32_t *)addr;
		if (*a == 0xFFFF0FC0) {
			l_debug("Patching 0x%x -> __kuser_cmpxchg", a);
			patched_addr = 0x9A000FC0;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
		}
		else if (*a == 0xFFFF0FA0) {
			l_debug("Patching 0x%x -> __kuser_memory_barrier", a);
			patched_addr = 0x9A000FA0;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
		}
	}
}

static void marmalade_cacheflush(void *start, size_t length) {
	// This Marmalade wrapper takes a byte count, not GCC's end pointer.
	if (!length) return;
	if (!start || (uintptr_t)start > UINTPTR_MAX - 31 ||
	    length > UINTPTR_MAX - (uintptr_t)start - 31)
		fatal_error("Invalid Marmalade cache range: %p + 0x%x", start, length);
	kuKernelFlushCaches(start, length);
	l_info("Marmalade code caches synchronized: %p + 0x%x from %p", start,
	       length, __builtin_return_address(0));
}

static so_hook s3e_malloc_hook;
static so_hook s3e_device_exit_hook;
static so_hook s3e_device_req_quit_hook;
static so_hook s3e_file_check_exists_hook;
static so_hook s3e_decomp_init_hook;
static so_hook s3e_file_open_core_hook;

static void *s3e_malloc_traced(size_t size) {
	// Diagnostic: capture the direct caller of the deterministic ~3.8 GB
	// s3eMalloc request seen after the splash shaders compile.
	if (size >= MALLOC_TRACE_THRESHOLD) {
		// Marmalade switches stacks before invoking callbacks. r4 retains the
		// original stack pointer, whose saved LR identifies the real game code
		// caller instead of the generic stack-switch trampoline.
		uintptr_t original_sp;
		__asm__ volatile ("mov %0, r4" : "=r"(original_sp));
		uintptr_t original_lr = 0;
		if (original_sp >= 0x80000000u && original_sp < 0x9a000000u)
			original_lr = *(uintptr_t *)(original_sp + sizeof(uintptr_t));
		l_error("s3eMallocBase(%u): huge request, trampoline=%p game_lr=%p sp=%p",
		        (unsigned int)size, __builtin_return_address(0),
		        (void *)original_lr, (void *)original_sp);
		if (original_sp >= 0x80000000u &&
		    original_sp <= 0x9a000000u - 32 * sizeof(uintptr_t)) {
			const uint32_t *stack = (const uint32_t *)original_sp;
			l_error("s3eMallocBase stack[0]: %08x %08x %08x %08x %08x %08x %08x %08x",
			        stack[0], stack[1], stack[2], stack[3], stack[4], stack[5],
			        stack[6], stack[7]);
			l_error("s3eMallocBase stack[8]: %08x %08x %08x %08x %08x %08x %08x %08x",
			        stack[8], stack[9], stack[10], stack[11], stack[12], stack[13],
			        stack[14], stack[15]);
			l_error("s3eMallocBase stack[16]: %08x %08x %08x %08x %08x %08x %08x %08x",
			        stack[16], stack[17], stack[18], stack[19], stack[20], stack[21],
			        stack[22], stack[23]);
			l_error("s3eMallocBase stack[24]: %08x %08x %08x %08x %08x %08x %08x %08x",
			        stack[24], stack[25], stack[26], stack[27], stack[28], stack[29],
			        stack[30], stack[31]);
		}
	}
	return SO_CONTINUE(void *, s3e_malloc_hook, size);
}

volatile int marmalade_quit_requested;

static void s3e_device_exit_traced(void) {
	// Diagnostic: who asks the device to exit, and from where.
	l_error("s3eDeviceExit: requested from %p", __builtin_return_address(0));
	marmalade_quit_requested = 1;
	SO_CONTINUE(int, s3e_device_exit_hook);
}

static void s3e_device_req_quit_traced(void) {
	// Diagnostic: the game loop quits when this flag is set.
	l_error("s3eDeviceRequestQuit: requested from %p", __builtin_return_address(0));
	marmalade_quit_requested = 1;
	SO_CONTINUE(int, s3e_device_req_quit_hook);
}

static int s3e_file_check_exists_traced(void *out, const char *name, int flag) {
	// Diagnostic: this gate prints "Can't open s3e file" on zero return.
	l_info("s3eFileCheckExists(out=%p, name=\"%s\", flag=%d): from %p", out,
	       name ? name : "(null)", flag, __builtin_return_address(0));
	int ret = SO_CONTINUE(int, s3e_file_check_exists_hook, out, name, flag);
	l_info("s3eFileCheckExists(\"%s\"): -> %d", name ? name : "(null)", ret);
	return ret;
}

static int s3e_decomp_init_traced(int a, void *b, void *c) {
	// Diagnostic: zero return here prints "Error reading s3e file".
	l_info("s3eCompressionDecompInit(%d, %p, %p): from %p", a, b, c,
	       __builtin_return_address(0));
	int ret = SO_CONTINUE(int, s3e_decomp_init_hook, a, b, c);
	l_info("s3eCompressionDecompInit: -> %d", ret);
	return ret;
}

static void *s3e_file_open_core_traced(const char *path, const char *mode,
                                       int provider) {
	bool is_s3e = path && strstr(path, ".s3e");
	if (is_s3e) {
		l_info("s3eFileOpenCore(path=\"%s\", mode=\"%s\", provider=%d)",
		       path, mode ? mode : "(null)", provider);
	}
	void *ret = SO_CONTINUE(void *, s3e_file_open_core_hook, path, mode, provider);
	if (!ret && is_s3e && provider == 1) {
		// The Android asset provider rejects Vita's absolute path after it has
		// already enumerated the external asset directory. Provider 0 accepts
		// this same file by basename (as seen during the first loader pass).
		const char *basename = strrchr(path, '/');
		if (basename && basename[1]) {
			basename++;
			l_warn("s3eFileOpenCore: retrying provider 1 failure as provider 0: \"%s\"",
			       basename);
			ret = SO_CONTINUE(void *, s3e_file_open_core_hook, basename, mode, 0);
		}
	}
	if (is_s3e)
		l_info("s3eFileOpenCore(\"%s\"): -> %p", path, ret);
	return ret;
}

void so_patch(void) {
	if (*(uint16_t *)(so_mod.load_addr + MARMALADE_CACHEFLUSH_OFFSET) != 0xb590 ||
	    *(uint16_t *)(so_mod.load_addr + MARMALADE_CODE_ALLOC_OFFSET) != 0xb538 ||
	    *(uint16_t *)(so_mod.load_addr + MARMALADE_CODE_FREE_OFFSET) != 0xb508 ||
	    *(uint16_t *)(so_mod.load_addr + MARMALADE_MALLOC_BASE_OFFSET) != 0xb5f0 ||
	    *(uint16_t *)(so_mod.load_addr + MARMALADE_DEVICE_EXIT_OFFSET) != 0xb508 ||
	    *(uint16_t *)(so_mod.load_addr + MARMALADE_DEVICE_REQ_QUIT_OFFSET) != 0xb510)
		fatal_error("Unsupported Marmalade library: code hook signatures differ.");
	kuser_patch();
	// The Android library contains __clear_cache(), which invokes the
	// Android-only cacheflush syscall (SVC 0, r7 = 0xf0002).
	hook_addr(so_mod.load_addr + MARMALADE_CACHEFLUSH_OFFSET + 1,
	          (uintptr_t)marmalade_cacheflush);
	// Pair the image allocator and release, leaving guarded stack/heap allocations alone.
	hook_addr(so_mod.load_addr + MARMALADE_CODE_ALLOC_OFFSET + 1,
	          (uintptr_t)marmalade_code_alloc);
	hook_addr(so_mod.load_addr + MARMALADE_CODE_FREE_OFFSET + 1,
	          (uintptr_t)marmalade_code_free);
	// Trace the origin of huge heap requests (diagnostic, see s3e_malloc_traced).
	s3e_malloc_hook = hook_addr(so_mod.load_addr + MARMALADE_MALLOC_BASE_OFFSET + 1,
	                            (uintptr_t)s3e_malloc_traced);
	// Trace who asks the device to quit/exit (diagnostic).
	s3e_device_exit_hook = hook_addr(so_mod.load_addr + MARMALADE_DEVICE_EXIT_OFFSET + 1,
	                                 (uintptr_t)s3e_device_exit_traced);
	s3e_device_req_quit_hook = hook_addr(so_mod.load_addr + MARMALADE_DEVICE_REQ_QUIT_OFFSET + 1,
	                                     (uintptr_t)s3e_device_req_quit_traced);
	// NOTE: s3eFileCheckExists/s3eCompressionDecompInit hooks removed: the
	// guessed 3-arg signature smashed the stack and crashed early init.
	// Re-add only with pointer-only logging after verifying the ABI.
	(void)s3e_file_check_exists_hook;
	(void)s3e_decomp_init_hook;
	if (*(uint16_t *)(so_mod.load_addr + MARMALADE_S3E_FILE_OPEN_CORE_OFFSET) != 0xb5f0)
		fatal_error("Unsupported Marmalade s3e file-open helper.");
	s3e_file_open_core_hook = hook_addr(
		so_mod.load_addr + MARMALADE_S3E_FILE_OPEN_CORE_OFFSET + 1,
		(uintptr_t)s3e_file_open_core_traced);
	// Sample hook with symbol name
	// hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN6glitch2os7Printer5printEPKcz"), (uintptr_t)&hookedFunction);
	// Or with offset
	// hook_addr((uintptr_t)so_mod.text_base + 0xdeadbabe, (uintptr_t)&hookedFunction);
	// If you use SO_CONTINUE, define a so_hook before the function and assign to it
	// function_hook = hook_addr(...);
}
