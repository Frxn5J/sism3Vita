/*
 * Copyright (C) 2026 The Sims 3 Vita port
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  egl_ext.c
 * @brief EGL entry points Marmalade resolves via dlsym() that vitaGL does
 *        not provide. Kept in a separate TU because source/reimpl/egl.c
 *        intentionally shadows vitaGL's egl.o and is NOT compiled
 *        (see CMakeLists SOURCES).
 */

#include "reimpl/egl.h"

#include <stdlib.h>
#include <string.h>

EGLDisplay eglGetCurrentDisplay(void) {
    // Single-display port: the display handle is opaque to the game.
    return (EGLDisplay)0x42424547;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
                                   const EGLint *attrib_list) {
    (void)dpy;
    (void)config;
    (void)attrib_list;
    // No offscreen pbuffers on Vita: hand out an opaque handle.
    return strdup("pbuffer");
}

EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
                                  void *pixmap, const EGLint *attrib_list) {
    (void)dpy;
    (void)config;
    (void)pixmap;
    (void)attrib_list;
    return strdup("pixmap");
}

EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype,
                                            void *buffer, EGLConfig config,
                                            const EGLint *attrib_list) {
    (void)dpy;
    (void)buftype;
    (void)buffer;
    (void)config;
    (void)attrib_list;
    return strdup("pbuffer-client");
}

EGLBoolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface,
                            EGLint attribute, EGLint value) {
    (void)dpy;
    (void)surface;
    (void)attribute;
    (void)value;
    return EGL_TRUE;
}

EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
    (void)dpy;
    (void)surface;
    (void)buffer;
    return EGL_TRUE;
}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface,
                              EGLint buffer) {
    (void)dpy;
    (void)surface;
    (void)buffer;
    return EGL_TRUE;
}

EGLBoolean eglWaitClient(void) {
    return EGL_TRUE;
}

EGLBoolean eglWaitGL(void) {
    return EGL_TRUE;
}

EGLBoolean eglWaitNative(EGLint engine) {
    (void)engine;
    return EGL_TRUE;
}

EGLBoolean eglReleaseThread(void) {
    return EGL_TRUE;
}

EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface,
                          void *native_pixmap) {
    (void)dpy;
    (void)surface;
    (void)native_pixmap;
    return EGL_TRUE;
}
