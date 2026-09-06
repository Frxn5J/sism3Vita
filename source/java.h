#ifndef SOLOADER_JAVA_H
#define SOLOADER_JAVA_H

#include <falso_jni/FalsoJNI.h>

#define JAVA_SURFACE_WIDTH 960
#define JAVA_SURFACE_HEIGHT 544

// Startup-only: register the returned array with setPixelsNative exactly once.
jintArray java_surface_init(jint width, jint height);

#endif
