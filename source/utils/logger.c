/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/logger.h"

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <stdarg.h>
#include <string.h>

#include <stdbool.h>
#include <stdatomic.h>

#define COLOR_RED    "\x1B[38;5;196m"
#define COLOR_PINK   "\x1B[38;5;212m"
#define COLOR_ORANGE "\x1B[38;5;202m"
#define COLOR_BLUE   "\x1B[38;5;32m"
#define COLOR_GREEN  "\x1B[32m"
#define COLOR_CYAN   "\x1B[36m"

#define COLOR_END    "\033[0m"

static SceKernelLwMutexWork _log_mutex;
static atomic_bool _log_mutex_ready = ATOMIC_VAR_INIT(false);

// Buffer A is used to adjust the format string.
static char buffer_a[2048];
// Buffer B is used to compile the final log using the updated format string.
static char buffer_b[2048];
static SceUID boot_log = -1;
static char overlay_lines[LOGGER_OVERLAY_LINES][LOGGER_OVERLAY_LINE_SIZE];
static size_t overlay_next;
static size_t overlay_count;

void _log_print(int t, const char* fmt, ...) {
    if (!atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        int ret = sceKernelCreateLwMutex(&_log_mutex, "log_lock", 0, 0, NULL);
        if (ret < 0) {
            sceClibPrintf("Error: failed to create log mutex: 0x%x\n", ret);
            return;
        }
        atomic_store_explicit(&_log_mutex_ready, true, memory_order_relaxed);
    }
    sceKernelLockLwMutex(&_log_mutex, 1, NULL);

    switch (t) {
        case LT_DEBUG:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s• debug%s    %s\n",
                            COLOR_PINK, COLOR_END, fmt); break;
        case LT_INFO:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %sℹ info%s     %s\n",
                            COLOR_BLUE, COLOR_END, fmt); break;
        case LT_WARN:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⚠ warning%s  %s\n",
                            COLOR_ORANGE, COLOR_END, fmt); break;
        case LT_ERROR:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s⨯ error%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_FATAL:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! fatal%s    %s\n",
                            COLOR_RED, COLOR_END, fmt); break;
        case LT_SUCCESS:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s! success%s  %s\n",
                            COLOR_GREEN, COLOR_END, fmt); break;
        case LT_WAIT:
            sceClibSnprintf(buffer_a, sizeof(buffer_a), " %s… waiting%s  %s\n",
                            COLOR_CYAN, COLOR_END, fmt); break;
        default:
            if (atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
                sceKernelUnlockLwMutex(&_log_mutex, 1);
            }
            return;
    }

    va_list list;
    va_start(list, fmt);
    va_list overlay_list;
    va_copy(overlay_list, list);
    sceClibVsnprintf(buffer_b, sizeof(buffer_b), buffer_a, list);
    if (t != LT_DEBUG) {
        sceClibVsnprintf(overlay_lines[overlay_next], LOGGER_OVERLAY_LINE_SIZE,
                         fmt, overlay_list);
        overlay_lines[overlay_next][LOGGER_OVERLAY_LINE_SIZE - 1] = '\0';
        overlay_next = (overlay_next + 1) % LOGGER_OVERLAY_LINES;
        if (overlay_count < LOGGER_OVERLAY_LINES)
            overlay_count++;
    }
    va_end(overlay_list);
    va_end(list);
    sceClibPrintf("%s", buffer_b);
    // Keep startup milestones after the small TTY ring has wrapped.
    if (t != LT_DEBUG) {
        if (boot_log < 0)
            boot_log = sceIoOpen(DATA_PATH "boot.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
        if (boot_log >= 0)
            sceIoWrite(boot_log, buffer_b, strlen(buffer_b));
    }

    if (atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        sceKernelUnlockLwMutex(&_log_mutex, 1);
    }
}

size_t logger_overlay_snapshot(char lines[LOGGER_OVERLAY_LINES][LOGGER_OVERLAY_LINE_SIZE],
                               size_t max_lines) {
    if (!lines || max_lines == 0 ||
        !atomic_load_explicit(&_log_mutex_ready, memory_order_relaxed)) {
        return 0;
    }

    sceKernelLockLwMutex(&_log_mutex, 1, NULL);
    size_t count = overlay_count < max_lines ? overlay_count : max_lines;
    size_t first = (overlay_next + LOGGER_OVERLAY_LINES - count) % LOGGER_OVERLAY_LINES;
    for (size_t i = 0; i < count; ++i) {
        size_t source = (first + i) % LOGGER_OVERLAY_LINES;
        strncpy(lines[i], overlay_lines[source], LOGGER_OVERLAY_LINE_SIZE);
        lines[i][LOGGER_OVERLAY_LINE_SIZE - 1] = '\0';
    }
    sceKernelUnlockLwMutex(&_log_mutex, 1);
    return count;
}
