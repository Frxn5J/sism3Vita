#include "utils/init.h"
#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "java.h"

#include <psp2/kernel/processmgr.h>
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
#define MARMALADE_MOTION_EVENT_OFFSET    0x30d89 // LoaderThread.onMotionEvent
#define MARMALADE_KEY_EVENT_OFFSET       0x309a5 // onKeyEventNative
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
typedef void (*marmalade_motion_event_fn)(JNIEnv *env, jobject loader_thread,
                                          jint pointer_id, jint action,
                                          jint x, jint y);
typedef jboolean (*marmalade_key_event_fn)(JNIEnv *env, jobject loader_view,
                                           jint keycode, jint unicode_char,
                                           jint pressed);

// onMotionEvent actions sent by LoaderView on multitouch devices. They also
// raise Marmalade's single-pointer events. 1-3 are the single-touch variants.
#define MARMALADE_TOUCH_DOWN 4
#define MARMALADE_TOUCH_UP   5
#define MARMALADE_TOUCH_MOVE 6

// Set by main() before the controls thread starts.
static marmalade_motion_event_fn volatile motion_event;
static marmalade_key_event_fn volatile key_event;

#ifndef NDK_PORT
// Marmalade's loop runs inside runNative, so poll input from our own thread,
// much like Android's UI thread delivering events.
static int controls_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    while (1) {
        if (!java_text_input_active)
            controls_poll();
        sceKernelDelayThread(1000000 / 60);
    }
    return 0;
}
#endif

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
    marmalade_check_native("onKeyEventNative",
                           marmalade_entry(MARMALADE_KEY_EVENT_OFFSET));

    // Marmalade drops input until its device layer is up, so input can flow
    // before runNative. onMotionEvent is also registered by s3eTouchpad, which
    // makes a by-name lookup ambiguous.
    motion_event = (void *)marmalade_entry(MARMALADE_MOTION_EVENT_OFFSET);
    key_event = (void *)marmalade_entry(MARMALADE_KEY_EVENT_OFFSET);

    SceUID input_thread = sceKernelCreateThread("MarmaladeInput", controls_thread,
                                                0x10000100, 64 * 1024, 0, 0,
                                                NULL);
    if (input_thread < 0 || sceKernelStartThread(input_thread, 0, NULL) < 0)
        fatal_error("Could not start the input thread: 0x%x", input_thread);

    jobject loader_thread = JAVA_LOADER_THREAD;
    jobject loader_view = JAVA_LOADER_VIEW;

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
    if (marmalade_quit_requested) {
        // The game quit on purpose. Ending only this thread would leave the
        // input, sound and Marmalade threads running behind a frozen screen.
        sceKernelExitProcess(0);
    }
    l_warn("runNative returned without a quit request; other threads keep running.");
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
    marmalade_key_event_fn fn = key_event;
    if (!fn)
        return;
    // Arguments are (keycode, unicode char, pressed); a zero char means a
    // plain key event with no text input.
    fn(&jni, JAVA_LOADER_VIEW, keycode, 0,
       action == CONTROLS_ACTION_DOWN ? 1 : 0);
}

void controls_handler_touch(int32_t id, float x, float y, ControlsAction action) {
    marmalade_motion_event_fn fn = motion_event;
    if (!fn)
        return;
    jint marmalade_action = MARMALADE_TOUCH_MOVE;
    if (action == CONTROLS_ACTION_DOWN)
        marmalade_action = MARMALADE_TOUCH_DOWN;
    else if (action == CONTROLS_ACTION_UP)
        marmalade_action = MARMALADE_TOUCH_UP;
    // x/y are already in the 960x544 surface space the game was given.
    fn(&jni, JAVA_LOADER_THREAD, id, marmalade_action, (jint)x, (jint)y);
}

void controls_handler_analog(ControlsStickId which, float x, float y, ControlsAction action) {
    // This Marmalade loader has no analog stick native; the d-pad covers it.
}
#endif
