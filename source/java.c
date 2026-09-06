#include "java.h"
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_Logger.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"

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
	METHOD_GET_LOCALE,
	METHOD_DO_DRAW,
	METHOD_VIDEO_STOP,
	METHOD_NETWORK_CHECK_START,
	METHOD_NETWORK_CHECK_STOP,
};

static int backend_initialized;
static int game_gl_started;
static jintArray software_pixels;
static uint8_t *software_rgba;
static GLuint software_texture;
static int software_frame_logged;

jintArray java_surface_init(jint width, jint height) {
	// gl_init() uses this fixed display size; this is not the splash image size.
	if (width != JAVA_SURFACE_WIDTH || height != JAVA_SURFACE_HEIGHT) {
		fatal_error("Unsupported software surface size: %dx%d", width, height);
	}
	if (software_pixels) {
		fatal_error("Software surface must only be initialized once.");
	}

	size_t count = (size_t)width * height;
	jintArray pixels = jni->NewIntArray(&jni, (jsize)count);
	if (!pixels) {
		fatal_error("Could not allocate the Marmalade software pixel array.");
	}
	jint *elements = jni->GetIntArrayElements(&jni, pixels, NULL);
	if (!elements) {
		fatal_error("Could not access the Marmalade software pixel array.");
	}
	memset(elements, 0, count * sizeof(*elements));
	jni->ReleaseIntArrayElements(&jni, pixels, elements, 0);

	software_rgba = malloc(count * 4);
	if (!software_rgba) {
		fatal_error("Could not allocate the software presentation buffer.");
	}
	software_pixels = pixels;
	l_info("Marmalade software surface allocated: %dx%d array=%p",
	       width, height, (void *)pixels);
	return pixels;
}

static void ensure_gl_initialized(void) {
	if (!backend_initialized) {
		gl_init();
		backend_initialized = 1;
	}
}

static void method_do_draw(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	if (game_gl_started) {
		return;
	}
	if (!software_pixels || !software_rgba) {
		fatal_error("doDraw called before the software surface was initialized.");
	}

	jint *pixels = jni->GetIntArrayElements(&jni, software_pixels, NULL);
	if (!pixels) {
		fatal_error("doDraw could not access the software pixel array.");
	}
	size_t count = (size_t)JAVA_SURFACE_WIDTH * JAVA_SURFACE_HEIGHT;
	unsigned int nonblack_pixels = 0;
	for (size_t i = 0; i < count; ++i) {
		// Bitmap.setPixels takes ARGB ints; the original RGB565 bitmap is opaque.
		uint32_t argb = (uint32_t)pixels[i];
		software_rgba[4 * i] = (uint8_t)(argb >> 16);
		software_rgba[4 * i + 1] = (uint8_t)(argb >> 8);
		software_rgba[4 * i + 2] = (uint8_t)argb;
		software_rgba[4 * i + 3] = 255;
		nonblack_pixels += (argb & 0x00ffffff) != 0;
	}
	jni->ReleaseIntArrayElements(&jni, software_pixels, pixels, JNI_ABORT);

	ensure_gl_initialized();
	glActiveTexture(GL_TEXTURE0);
	glClientActiveTexture(GL_TEXTURE0);
	if (!software_texture) {
		glGenTextures(1, &software_texture);
		if (!software_texture) {
			fatal_error("Could not allocate the software presentation texture.");
		}
		glBindTexture(GL_TEXTURE_2D, software_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, JAVA_SURFACE_WIDTH,
		             JAVA_SURFACE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, software_rgba);
	} else {
		glBindTexture(GL_TEXTURE_2D, software_texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, JAVA_SURFACE_WIDTH,
		                JAVA_SURFACE_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, software_rgba);
	}
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		fatal_error("Software texture upload failed: GL error 0x%x", error);
	}

	// Only the presenter owns GL before game_gl_started. Restore startup defaults
	// below, rather than relying on vitaGL's incomplete attribute stack.
	// Java rows are top-down, so t=0 maps to the top of the clip-space quad.
	static const GLfloat vertices[] = {
		-1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f
	};
	static const GLfloat texcoords[] = {
		0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f
	};
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, JAVA_SURFACE_WIDTH, JAVA_SURFACE_HEIGHT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_FOG);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(4, GL_FLOAT, 0, NULL);
	glTexCoordPointer(4, GL_FLOAT, 0, NULL);
	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	error = glGetError();
	if (error != GL_NO_ERROR) {
		fatal_error("Software presentation failed: GL error 0x%x", error);
	}
	gl_swap();
	if (!software_frame_logged) {
		l_info("First software frame presented: %dx%d, %u nonblack native pixels",
		       JAVA_SURFACE_WIDTH, JAVA_SURFACE_HEIGHT, nonblack_pixels);
		software_frame_logged = 1;
	}
}

static void method_gl_init(jmethodID id, va_list args) {
	(void)id;
	jint version = va_arg(args, jint);
	ensure_gl_initialized();
	if (!game_gl_started) {
		game_gl_started = 1;
		l_info("Marmalade switching to game GL: version=%d; software presenter disabled",
		       version);
		if (software_texture) {
			glDeleteTextures(1, &software_texture);
			software_texture = 0;
		}
		free(software_rgba);
		software_rgba = NULL;
		// Native code retains software_pixels; do not delete its global reference.
	}
}

static void method_gl_reinit(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	if (game_gl_started) {
		ensure_gl_initialized();
	}
}

static void method_gl_swap_buffers(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	if (game_gl_started) {
		gl_swap();
	}
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

static jobject method_get_locale(jmethodID id, va_list args) {
	(void)id;
	(void)args;
	return jni->NewStringUTF(&jni, "en_US");
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
		{ METHOD_GET_LOCALE, "getLocale", METHOD_TYPE_OBJECT },
		{ METHOD_DO_DRAW, "doDraw", METHOD_TYPE_VOID },
		{ METHOD_VIDEO_STOP, "videoStop", METHOD_TYPE_VOID },
		{ METHOD_NETWORK_CHECK_START, "networkCheckStart", METHOD_TYPE_BOOLEAN },
		{ METHOD_NETWORK_CHECK_STOP, "networkCheckStop", METHOD_TYPE_BOOLEAN },
};

MethodsBoolean methodsBoolean[] = {
		{ METHOD_GET_SILENT_MODE, method_false },
		{ METHOD_HAS_MULTITOUCH, method_true },
		{ METHOD_CHARGER_IS_CONNECTED, method_false },
		{ METHOD_NETWORK_CHECK_START, method_true },
		{ METHOD_NETWORK_CHECK_STOP, method_true },
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
		{ METHOD_GET_LOCALE, method_get_locale },
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
		{ METHOD_DO_DRAW, method_do_draw },
		{ METHOD_VIDEO_STOP, method_void_stub },
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
