#ifndef SOLOADER_JAVA_H
#define SOLOADER_JAVA_H

#include <falso_jni/FalsoJNI.h>
#include <stdint.h>

// Fake LoaderThread object handed to Marmalade's natives.
#define JAVA_LOADER_THREAD ((jobject)0x42424242)
// Fake LoaderView object.
#define JAVA_LOADER_VIEW ((jobject)0x69696969)

#define JAVA_SURFACE_WIDTH 960
#define JAVA_SURFACE_HEIGHT 544

// Startup-only: register the returned array with setPixelsNative exactly once.
jintArray java_surface_init(jint width, jint height);

// FalsoJNI ignores RegisterNatives. Call this after jni_init() and before
// JNI_OnLoad to record (and log) every native method the game registers.
void java_natives_hook(void);

// Function registered under `name`, or 0 if it is unknown or ambiguous.
uintptr_t java_native_lookup(const char *name);

// Non-zero while getInputString shows the IME; game input is paused.
extern volatile int java_text_input_active;

#endif
