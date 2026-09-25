/*
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  init.h
 * @brief so-loader initialization routines.
 */

#ifndef SOLOADER_INIT_H
#define SOLOADER_INIT_H

#include <so_util/so_util.h>

#ifdef __cplusplus
extern "C" {
#endif

void resolve_imports(so_module *mod);

void so_patch();

// Set by the s3eDeviceExit/s3eDeviceRequestQuit hooks in patch.c.
extern volatile int marmalade_quit_requested;

void soloader_init_all();

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_INIT_H
