#include "utils/init.h"
#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "java.h"

#include <psp2/kernel/threadmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#ifndef NDK_PORT
#include "reimpl/controls.h"
#else
#include <falso_ndk/FalsoNDK.h>
#endif

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;

#define MARMALADE_INIT_NATIVE_OFFSET     0x2e37d
#define MARMALADE_SET_VIEW_NATIVE_OFFSET 0x2dc81
#define MARMALADE_SET_PIXELS_NATIVE_OFFSET 0x2dae9
#define MARMALADE_RUN_NATIVE_OFFSET      0x2ddd1
#define PACKAGE_RESOURCE_PATH            DATA_PATH "The-Sims-3_1.5.21.apk"
#define MARMALADE_FILE_ROOT              DATA_PATH "assets/"

typedef void (*marmalade_init_native_fn)(JNIEnv *env, jobject loader_thread);
typedef void (*marmalade_set_view_native_fn)(JNIEnv *env, jobject loader_thread,
                                              jobject loader_view);
typedef void (*marmalade_set_pixels_native_fn)(JNIEnv *env, jobject loader_view,
                                              jint width, jint height,
                                              jintArray pixels);
typedef void (*marmalade_run_native_fn)(JNIEnv *env, jobject loader_thread,
                                        jstring file_root, jstring package_path);

static uintptr_t marmalade_entry(uintptr_t offset) {
    uintptr_t text_offset = so_mod.text_base - so_mod.load_addr;
    if (offset < text_offset || offset >= text_offset + so_mod.text_size) {
        fatal_error("Invalid Marmalade entry offset: 0x%x", offset);
    }

    // The offsets come from the ELF virtual address space, like st_value in
    // so_symbol(), so they must be based at load_addr rather than text_base.
    return so_mod.load_addr + offset;
}

// The offsets above were taken from this exact libthesims3.so. If JNI_OnLoad
// registered the same method by name, make sure both agree.
static void marmalade_check_native(const char *name, uintptr_t entry) {
    uintptr_t registered = java_native_lookup(name);
    if (!registered) {
        l_info("%s was not registered by name; using offset entry %p", name,
               (void *)entry);
    } else if (registered != entry) {
        l_warn("%s is registered at %p but the offset entry is %p", name,
               (void *)registered, (void *)entry);
    }
}


int main() {
    soloader_init_all();

    uintptr_t jni_on_load_address = so_symbol(&so_mod, "JNI_OnLoad");
    if (!jni_on_load_address) {
        l_fatal("JNI_OnLoad export not found in %s.", SO_PATH);
        fatal_error("The game library does not export JNI_OnLoad.\n"
                    "This port needs the Marmalade startup entry point.");
    }

    jint (*jni_on_load)(JavaVM *vm, void *reserved) =
        (void *)jni_on_load_address;
    java_natives_hook();
    jint jni_version = jni_on_load(&jvm, NULL);
    if (jni_version < 0) {
        l_fatal("JNI_OnLoad failed: version=0x%x.", jni_version);
        fatal_error("JNI_OnLoad failed.\nReturn value: 0x%x", jni_version);
    }
    l_success("JNI_OnLoad completed: version=0x%x.", jni_version);

#ifndef NDK_PORT
    marmalade_init_native_fn init_native =
        (void *)marmalade_entry(MARMALADE_INIT_NATIVE_OFFSET);
    marmalade_set_view_native_fn set_view_native =
        (void *)marmalade_entry(MARMALADE_SET_VIEW_NATIVE_OFFSET);
    marmalade_set_pixels_native_fn set_pixels_native =
        (void *)marmalade_entry(MARMALADE_SET_PIXELS_NATIVE_OFFSET);
    marmalade_run_native_fn run_native =
        (void *)marmalade_entry(MARMALADE_RUN_NATIVE_OFFSET);

    l_info("Marmalade entry points: init=0x%x view=0x%x run=0x%x",
           (unsigned int)init_native, (unsigned int)set_view_native,
           (unsigned int)run_native);
    marmalade_check_native("initNative", (uintptr_t)init_native);
    marmalade_check_native("setViewNative", (uintptr_t)set_view_native);
    marmalade_check_native("setPixelsNative", (uintptr_t)set_pixels_native);
    marmalade_check_native("runNative", (uintptr_t)run_native);

    jobject loader_thread = (jobject)0x42424242;
    jobject loader_view = (jobject)0x69696969;

    if (!file_exists(PACKAGE_RESOURCE_PATH)) {
        fatal_error("The original APK is required at:\n%s", PACKAGE_RESOURCE_PATH);
    }

    l_info("Initializing Marmalade JNI bindings.");
    init_native(&jni, loader_thread);
    l_info("Marmalade JNI bindings initialized.");
    jintArray pixels = java_surface_init(JAVA_SURFACE_WIDTH, JAVA_SURFACE_HEIGHT);
    set_pixels_native(&jni, loader_view, JAVA_SURFACE_WIDTH, JAVA_SURFACE_HEIGHT,
                      pixels);
    l_info("Marmalade software pixels registered through setPixelsNative.");
    set_view_native(&jni, loader_thread, loader_view);
    l_info("Marmalade view configured.");

    // Marmalade's internal s3e provider bypasses the POSIX redirect wrappers.
    // Point its file root directly at the extracted asset tree.
    jstring file_root = jni->NewStringUTF(&jni, MARMALADE_FILE_ROOT);
    jstring package_path = jni->NewStringUTF(&jni, PACKAGE_RESOURCE_PATH);
    if (!file_root || !package_path) {
        fatal_error("Could not allocate Marmalade startup paths.");
    }

    l_info("Starting Marmalade: root=%s package=%s", MARMALADE_FILE_ROOT,
           PACKAGE_RESOURCE_PATH);
    uint64_t run_start_ms = current_timestamp_ms();
    run_native(&jni, loader_thread, file_root, package_path);
    l_info("Marmalade runtime exited after %llu ms.", current_timestamp_ms() - run_start_ms);
#else
    // Build a fake ANativeActivity that the game's onCreate will receive
    ANativeActivity *activity = malloc(sizeof(ANativeActivity));
    activity->callbacks = malloc(sizeof(ANativeActivityCallbacks));
    activity->env = &jni; // from FalsoJNI
    activity->vm = &jvm;  // from FalsoJNI
    activity->clazz = (jclass)0x42424242;
    activity->internalDataPath = DATA_PATH "assets/";
    activity->externalDataPath = DATA_PATH "assets/";
    activity->sdkVersion = 14;
    activity->instance = NULL;

    // Drive the activity lifecycle
    int (*ANativeActivity_onCreate)(ANativeActivity *, void *, size_t) =
        (void *)so_symbol(&so_mod, "ANativeActivity_onCreate");
    ANativeActivity_onCreate(activity, NULL, 0);

    activity->callbacks->onStart(activity);
    activity->callbacks->onResume(activity);

    // Wire up input and the native window
    AInputQueue *aInputQueue = AInputQueue_create();
    activity->callbacks->onInputQueueCreated(activity, aInputQueue);

    ANativeWindow *aNativeWindow = ANativeWindow_create();
    activity->callbacks->onNativeWindowCreated(activity, aNativeWindow);

    activity->callbacks->onWindowFocusChanged(activity, 1);
#endif

    sceKernelExitDeleteThread(0);
}

#ifndef NDK_PORT
void controls_handler_key(int32_t keycode, ControlsAction action) {
    // Call into the .so here
}

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    // Call into the .so here
}

void controls_handler_analog(ControlsStickId which, float x, float y, ControlsAction action) {
    // Call into the .so here
}
#endif
