/**
 * @file  sound.h
 * @brief Marmalade s3eSound output through a Vita audio port.
 */

#ifndef SOLOADER_SOUND_H
#define SOLOADER_SOUND_H

#ifdef __cplusplus
extern "C" {
#endif

// LoaderThread.soundInit(stereo, 0): returns the output rate, 0 on failure.
int sound_init(int stereo);

void sound_start(void);

void sound_stop(void);

// 0-100, as LoaderThread.soundSetVolume receives it.
void sound_set_volume(int percent);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_SOUND_H
