/**
 * @file  sound.c
 * @brief Marmalade s3eSound output through a Vita audio port.
 *
 * On Android, LoaderThread.soundInit creates an AudioTrack and a SoundPlayer
 * thread that keeps calling the generateAudio native for PCM. This does the
 * same with a BGM audio port and a Vita thread.
 */

#include "reimpl/sound.h"

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include "java.h"
#include "utils/logger.h"

extern so_module so_mod;

// SoundPlayer.generateAudio([SI)V in libthesims3.so (1.5.21), used when
// RegisterNatives did not record it by name.
#define MARMALADE_GENERATE_AUDIO_OFFSET 0x30f19

// Rate reported to Marmalade. It mixes on the CPU (in a soft-float build), so
// stay at 22050 Hz, which the BGM port accepts.
#define SOUND_FREQUENCY 22050
// Frames per generateAudio call; audio port lengths are multiples of 64.
#define SOUND_FRAMES 1024

typedef void (*generate_audio_fn)(JNIEnv *env, jobject thiz,
                                  jshortArray buffer, jint frames);

static volatile int sound_stereo;
static volatile int sound_playing;
static volatile int sound_volume = 100;
static SceUID sound_thread = -1;

static int sound_thread_main(SceSize args, void *argp) {
    (void)args;
    (void)argp;

    uintptr_t fn = java_native_lookup("generateAudio");
    if (!fn)
        fn = so_mod.load_addr + MARMALADE_GENERATE_AUDIO_OFFSET;
    generate_audio_fn generate_audio = (generate_audio_fn)fn;

    int stereo = sound_stereo;
    int channels = stereo ? 2 : 1;
    int port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, SOUND_FRAMES,
                                   SOUND_FREQUENCY,
                                   stereo ? SCE_AUDIO_OUT_MODE_STEREO
                                          : SCE_AUDIO_OUT_MODE_MONO);
    if (port < 0) {
        l_error("sound: could not open the audio port: 0x%x", port);
        return 0;
    }

    // generateAudio copies the mix in with SetShortArrayRegion (interleaved
    // when stereo). FalsoJNI arrays expose their storage directly, so the
    // element pointer stays valid for the whole thread.
    jshortArray array = jni->NewShortArray(&jni, SOUND_FRAMES * channels);
    jshort *samples = array ? jni->GetShortArrayElements(&jni, array, NULL)
                            : NULL;
    if (!samples) {
        l_error("sound: could not allocate the PCM buffer");
        sceAudioOutReleasePort(port);
        return 0;
    }
    l_info("sound: %d Hz, %d channel(s), %d frames per buffer",
           SOUND_FREQUENCY, channels, SOUND_FRAMES);

    int applied_volume = -1;
    while (1) {
        if (!sound_playing) {
            sceKernelDelayThread(10000);
            continue;
        }
        int volume = sound_volume;
        if (volume != applied_volume) {
            int level = volume * SCE_AUDIO_VOLUME_0DB / 100;
            int levels[2] = { level, level };
            sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH |
                                       SCE_AUDIO_VOLUME_FLAG_R_CH, levels);
            applied_volume = volume;
        }
        // The native ignores its SoundPlayer object.
        generate_audio(&jni, NULL, array, SOUND_FRAMES);
        sceAudioOutOutput(port, samples); // Blocks until the port takes it.
    }
    return 0;
}

int sound_init(int stereo) {
    sound_stereo = stereo ? 1 : 0;
    l_info("soundInit(stereo=%d): %d Hz", sound_stereo, SOUND_FREQUENCY);
    return SOUND_FREQUENCY;
}

void sound_start(void) {
    if (sound_thread < 0) {
        // Default user priority; the mixer runs Marmalade code, so give it a
        // roomy stack.
        sound_thread = sceKernelCreateThread("MarmaladeSound", sound_thread_main,
                                             0x10000100, 256 * 1024, 0, 0, NULL);
        if (sound_thread < 0) {
            l_error("sound: could not create the audio thread: 0x%x",
                    sound_thread);
            return;
        }
        sceKernelStartThread(sound_thread, 0, NULL);
    }
    sound_playing = 1;
}

void sound_stop(void) {
    sound_playing = 0;
}

void sound_set_volume(int percent) {
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    sound_volume = percent;
}
