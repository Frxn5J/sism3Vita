# libthesims3.so notes

Static analysis of the library this port targets. Offsets are ELF virtual
addresses (the port adds `so_mod.load_addr`); odd values are Thumb entry
points.

- File: `libthesims3.so` from `The-Sims-3_1.5.21.apk`, 531164 bytes,
  SHA-256 `e212aa7e913bbad3439f545da5a1af5a522c7d4287fddbac70b5f6cfa74d9d19`
  (the one `init.c` refers to), SHA-1 `fecbae359390cc2c16d6100185def1af465c5eb3`.
- It is the Marmalade loader. The game code lives in the `.s3e` inside the
  APK and is loaded at runtime into the code arena that `patch.c` hooks.
- 159 imports, all covered by `source/dynlib.c`. There are no GL imports:
  Marmalade `dlopen`s `libGLESv2.so`, `libGLESv1_CM.so` and `libEGL.so` and
  looks up about 260 GL/EGL names with `dlsym`, which `dlsym_soloader` answers from
  the same `dynlib.c` table.
- No `__kuser_*` helper addresses appear in the code (neither as literals nor
  built with `mvn`), so `kuser_patch()` has nothing to rewrite in this file.
- The signature words checked by `so_patch()` all match this file.

## Registered natives

Tables of `JNINativeMethod` in `.rodata`:

| Offset | Method |
|---|---|
| 0x2dbf9 | `shutdownNative()V` |
| 0x2e37d | `initNative()V` |
| 0x2dbb9 | `suspendAppThreads()V` |
| 0x2dba5 | `resumeAppThreads()V` |
| 0x2d96d | `onAccelNative(FFF)V` |
| 0x2f2b9 | `onCompassNative(IFFF)V` |
| 0x30d89 | `onMotionEvent(IIII)V` (LoaderThread) |
| 0x2dc81 | `setViewNative(Lcom/ideaworks3d/marmalade/LoaderView;)V` |
| 0x2ddd1 | `runNative(Ljava/lang/String;Ljava/lang/String;)V` |
| 0x2db8d | `audioStoppedNotify(I)V` |
| 0x2dabd | `chargerStateChanged(Z)V` |
| 0x2da79 | `networkCheckChanged(Z)V` |
| 0x2dc65 | `runOnOSThreadNative(Ljava/lang/Runnable;)V` |
| 0x2dbf1 | `runOnOSTickNative()V` |
| 0x2dbe5 | `signalSuspend()V` |
| 0x2dbd9 | `signalResume()V` |
| 0x2d9fd | `setSuspended()V` |
| 0x2da05 | `setResumed()V` |
| 0x2dae9 | `setPixelsNative(II[I)V` |
| 0x2db79 | `videoStoppedNotify()V` |
| 0x2dc15 | `setInputText(Ljava/lang/String;)V` |
| 0x309a5 | `onKeyEventNative(III)Z` |
| 0x307bd | `setCharInputEnabledNative(Z)V` |
| 0x30f19 | `generateAudio([SI)V` (SoundPlayer) |
| 0x2da0d | `recordAudio([SII)V` |
| 0x2a911 | `onMotionEvent(IIII)V` (s3eTouchpad) |

The four entry offsets in `source/main.c` match this table.

### Input

`onMotionEvent(pointerId, action, x, y)`: actions 1/2/3 are single-touch
down/up/move, 4/5/6 are multitouch down/up/move (0 is ignored). The
multitouch handlers dispatch the single-pointer events (device 6, events 0
and 1) next to the touch events (2 and 3), so the port sends 4/5/6 like
Android does.

`onKeyEventNative(keycode, unicodeChar, pressed)`: `pressed` is 1/0. A
non-zero char (or keycode 67, DEL) on press also feeds text input. Android
keycodes go through a table at 0x74ef4. `BUTTON_A` and `BUTTON_B` map to
nothing; the useful ones are the Xperia Play set: `DPAD_*`, `DPAD_CENTER`
(OK), `BACK`, `MENU`, `SEARCH`, `BUTTON_X/Y/L1/R1/START/SELECT`, letters,
digits, `ENTER`, `DEL` and `SPACE`.

## Sound

`soundInit(stereo, 0)` is called by s3eSound's init (config key
`SoundStereo` picks stereo, with a mono retry) and must return the output
rate; 0 means failure. It also stores the mixer callback at 0x92548.
`generateAudio(short[] buffer, int frames)` mixes `frames` frames and copies
them with `SetShortArrayRegion`: `frames` shorts in mono, `2 * frames`
interleaved in stereo. `soundSetVolume` receives 0-100.
`source/reimpl/sound.c` answers 22050 Hz and pumps `generateAudio` into a
BGM audio port from its own thread between `soundStart` and `soundStop`.

## Text input

`s3eOSReadString` (around 0x299a8) calls `getInputString(title, default,
flags)` and then polls the string that `setInputText` stores (global
+0x194), yielding 20 ms per try until it is set or the app quits.
`setInputText` copies the string with `GetStringUTFChars`, so it must never
receive NULL. `java.c` shows the Vita IME from the calling (GL) thread and
answers through `setInputText`, falling back to the default text on cancel.

## OS-thread calls

`s3eEdkThreadRunOnOS` (around 0x5bc90) takes the lock at +0x52c, stores the
call at +0x53c, calls the Java method `runOnOSSignal()` through 0x2e2b0 and,
for synchronous calls, waits on the semaphore at +0x530 with no timeout.
`runOnOSTickNative` (0x5b670) runs the stored call and posts that semaphore.
`java.c` therefore calls `runOnOSTickNative` from `runOnOSSignal`.

## Java methods used by initNative

`initNative` looks up these `LoaderThread` methods; the ones marked with ✓
are implemented in `source/java.c`, the rest return 0/NULL through FalsoJNI.

- Video: `videoPlay` ✓ (returns -1), `videoStop` ✓, `videoPause`,
  `videoResume`, `videoGetStatus`, `videoGetPosition`, `videoSetVolume`.
  `videoPlay` and `audioPlay` results of -1 and -2 are errors and 0 is
  success, so an unimplemented method looked like playback that never ends.
- Audio: `audioPlay` ✓ (returns -1), `audioStop`, `audioPause`,
  `audioResume`, `audioGetPosition`, `audioSetPosition`, `audioGetStatus`,
  `audioGetDuration`, `audioSetVolume`, `audioIsPlaying`,
  `audioGetNumChannels`, `soundInit(ZI)I`, `soundStart`, `soundStop`,
  `soundSetVolume`. `soundInit` ✓, `soundStart` ✓, `soundStop` ✓ and
  `soundSetVolume` ✓ drive `source/reimpl/sound.c`.
- GL and loop: `glInit` ✓, `glReInit` ✓, `glTerm` ✓, `glSwapBuffers` ✓,
  `doDraw` ✓, `runRunnable` ✓, `runOnOSSignal` ✓, `runOnOSThread`,
  `deviceUnYield` ✓, `doResume` ✓, `doPause` ✓.
- Device: `hasMultitouch` ✓, `fixOrientation` ✓, `getOrientation` ✓,
  `touchSetWait` ✓, `getSilentMode` ✓, `getDeviceId/Model/IMSI/Number` ✓,
  `getNetworkType/SubType` ✓, `getCardRoot` ✓, `getBatteryLevel` ✓,
  `chargerIsConnected` ✓, `networkCheckStart/Stop` ✓, `getLocale` ✓,
  `showError` ✓ (logged), `backlightOn`, `vibrateStart/Stop/Available`.
- Text input: `getInputString` ✓ (Vita IME), `setShowOnScreenKeyboard`,
  `getKeyboardInfo`.
- Not relevant on Vita: `launchBrowser`, `sendEmail`, `contacts*`,
  `location*`, `record*`, `accel*`, `compass*`, `sms*`, `clipboardGet/Set`,
  `acquire/releaseMulticastLock`.
