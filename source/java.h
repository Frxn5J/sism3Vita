#ifndef SOLOADER_JAVA_H
#define SOLOADER_JAVA_H

#include <falso_jni/FalsoJNI.h>
#include <stdint.h>

#define JAVA_SURFACE_WIDTH 960
#define JAVA_SURFACE_HEIGHT 544

// Startup-only: register the returned array with setPixelsNative exactly once.
jintArray java_surface_init(jint width, jint height);

// FalsoJNI ignores RegisterNatives. Call this after jni_init() and before
// JNI_OnLoad to record (and log) every native method the game registers.
void java_natives_hook(void);

// Function registered under `name`, or 0 if it is unknown or ambiguous.
uintptr_t java_native_lookup(const char *name);

#endif
