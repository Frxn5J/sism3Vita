#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>

#include "utils/glutil.h"

enum {
	METHOD_GL_INIT = 1,
	METHOD_GL_REINIT,
	METHOD_GL_SWAP_BUFFERS,
	METHOD_GL_TERM,
	METHOD_GET_CARD_ROOT,
	METHOD_GET_NETWORK_SUBTYPE,
	METHOD_GET_NETWORK_TYPE,
	METHOD_GET_ORIENTATION,
	METHOD_DEVICE_UNYIELD,
	METHOD_DO_RESUME,
	METHOD_DO_PAUSE,
	METHOD_FIX_ORIENTATION,
	METHOD_TOUCH_SET_WAIT,
	METHOD_RUN_ON_OS_SIGNAL,
	METHOD_RUN_RUNNABLE,
	METHOD_GET_SILENT_MODE,
	METHOD_HAS_MULTITOUCH,
	METHOD_GET_BATTERY_LEVEL,
	METHOD_CHARGER_IS_CONNECTED,
	METHOD_GET_DEVICE_ID,
	METHOD_GET_DEVICE_MODEL,
	METHOD_GET_DEVICE_IMSI,
	METHOD_GET_DEVICE_NUMBER,
};

static int gl_initialized;

static void ensure_gl_initialized(void) {
	if (!gl_initialized) {
		gl_init();
		gl_initialized = 1;
	}
}

static void method_gl_init(jmethodID id, va_list args) {
	(void)id;
	(void)va_arg(args, jint);
	ensure_gl_initialized();
}

static void method_gl_reinit(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	ensure_gl_initialized();
}

static void method_gl_swap_buffers(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	ensure_gl_initialized();
	gl_swap();
}

static void method_void_stub(jmethodID id, va_list args) {
	(void)id;
	(void)args;
}

static jobject method_get_card_root(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return jni->NewStringUTF(&jni, DATA_PATH);
}

static jobject method_get_device_string(jmethodID id, va_list args) {
	(void)args;
	const char *value = id == (jmethodID)METHOD_GET_DEVICE_MODEL
		? "PlayStation Vita"
		: "";
	return jni->NewStringUTF(&jni, value);
}

static jint method_get_network_state(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return -1;
}

static jint method_get_orientation(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return 1;
}

static jint method_get_battery_level(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return 100;
}

static jboolean method_false(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return JNI_FALSE;
}

static jboolean method_true(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return JNI_TRUE;
}

/*
 * JNI Methods
*/

NameToMethodID nameToMethodId[] = {
		{ METHOD_GL_INIT, "glInit", METHOD_TYPE_VOID },
		{ METHOD_GL_REINIT, "glReInit", METHOD_TYPE_VOID },
		{ METHOD_GL_SWAP_BUFFERS, "glSwapBuffers", METHOD_TYPE_VOID },
		{ METHOD_GL_TERM, "glTerm", METHOD_TYPE_VOID },
		{ METHOD_GET_CARD_ROOT, "getCardRoot", METHOD_TYPE_OBJECT },
		{ METHOD_GET_NETWORK_SUBTYPE, "getNetworkSubType", METHOD_TYPE_INT },
		{ METHOD_GET_NETWORK_TYPE, "getNetworkType", METHOD_TYPE_INT },
		{ METHOD_GET_ORIENTATION, "getOrientation", METHOD_TYPE_INT },
		{ METHOD_DEVICE_UNYIELD, "deviceUnYield", METHOD_TYPE_VOID },
		{ METHOD_DO_RESUME, "doResume", METHOD_TYPE_VOID },
		{ METHOD_DO_PAUSE, "doPause", METHOD_TYPE_VOID },
		{ METHOD_FIX_ORIENTATION, "fixOrientation", METHOD_TYPE_VOID },
		{ METHOD_TOUCH_SET_WAIT, "touchSetWait", METHOD_TYPE_VOID },
		{ METHOD_RUN_ON_OS_SIGNAL, "runOnOSSignal", METHOD_TYPE_VOID },
		{ METHOD_RUN_RUNNABLE, "runRunnable", METHOD_TYPE_VOID },
		{ METHOD_GET_SILENT_MODE, "getSilentMode", METHOD_TYPE_BOOLEAN },
		{ METHOD_HAS_MULTITOUCH, "hasMultitouch", METHOD_TYPE_BOOLEAN },
		{ METHOD_GET_BATTERY_LEVEL, "getBatteryLevel", METHOD_TYPE_INT },
		{ METHOD_CHARGER_IS_CONNECTED, "chargerIsConnected", METHOD_TYPE_BOOLEAN },
		{ METHOD_GET_DEVICE_ID, "getDeviceId", METHOD_TYPE_OBJECT },
		{ METHOD_GET_DEVICE_MODEL, "getDeviceModel", METHOD_TYPE_OBJECT },
		{ METHOD_GET_DEVICE_IMSI, "getDeviceIMSI", METHOD_TYPE_OBJECT },
		{ METHOD_GET_DEVICE_NUMBER, "getDeviceNumber", METHOD_TYPE_OBJECT },
};

MethodsBoolean methodsBoolean[] = {
		{ METHOD_GET_SILENT_MODE, method_false },
		{ METHOD_HAS_MULTITOUCH, method_true },
		{ METHOD_CHARGER_IS_CONNECTED, method_false },
};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {
		{ METHOD_GET_NETWORK_SUBTYPE, method_get_network_state },
		{ METHOD_GET_NETWORK_TYPE, method_get_network_state },
		{ METHOD_GET_ORIENTATION, method_get_orientation },
		{ METHOD_GET_BATTERY_LEVEL, method_get_battery_level },
};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {
		{ METHOD_GET_CARD_ROOT, method_get_card_root },
		{ METHOD_GET_DEVICE_ID, method_get_device_string },
		{ METHOD_GET_DEVICE_MODEL, method_get_device_string },
		{ METHOD_GET_DEVICE_IMSI, method_get_device_string },
		{ METHOD_GET_DEVICE_NUMBER, method_get_device_string },
};
MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {
		{ METHOD_GL_INIT, method_gl_init },
		{ METHOD_GL_REINIT, method_gl_reinit },
		{ METHOD_GL_SWAP_BUFFERS, method_gl_swap_buffers },
		{ METHOD_GL_TERM, method_void_stub },
		{ METHOD_DEVICE_UNYIELD, method_void_stub },
		{ METHOD_DO_RESUME, method_void_stub },
		{ METHOD_DO_PAUSE, method_void_stub },
		{ METHOD_FIX_ORIENTATION, method_void_stub },
		{ METHOD_TOUCH_SET_WAIT, method_void_stub },
		{ METHOD_RUN_ON_OS_SIGNAL, method_void_stub },
		{ METHOD_RUN_RUNNABLE, method_void_stub },
};

/*
 * JNI Fields
*/

// System-wide constant that applications sometimes request
// https://developer.android.com/reference/android/content/Context.html#WINDOW_SERVICE
char WINDOW_SERVICE[] = "window";

// System-wide constant that's often used to determine Android version
// https://developer.android.com/reference/android/os/Build.VERSION.html#SDK_INT
// Possible values: https://developer.android.com/reference/android/os/Build.VERSION_CODES
const int SDK_INT = 19; // Android 4.4 / KitKat

NameToFieldID nameToFieldId[] = {
		{ 0, "WINDOW_SERVICE", FIELD_TYPE_OBJECT }, 
		{ 1, "SDK_INT", FIELD_TYPE_INT },
		{ 2, "m_GL", FIELD_TYPE_OBJECT },
		{ 3, "m_MediaPlayerManager", FIELD_TYPE_OBJECT },
		{ 4, "m_LoaderKeyboard", FIELD_TYPE_OBJECT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
		{ 1, SDK_INT },
};
FieldsObject fieldsObject[] = {
		{ 0, WINDOW_SERVICE },
		{ 2, (jobject)0x42424242 },
		{ 3, (jobject)0x42424242 },
		{ 4, (jobject)0x42424242 },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
