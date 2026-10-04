# platform/windows/audioOutputWin.c notes

The Windows `src/audioOutput.c`: the same API (`audioOutput.h`) and the same pref keys, through WASAPI by way of
miniaudio (`platform/windows/miniaudio.h`, its implementation alone in `miniaudioImpl.c`, built with `-w`).
Written on the Mac 2026-10-04 and cross-built; FIRST HEARD on Windows: not yet.

## 1. `enumerate()`

A WASAPI endpoint ID is a UTF-16 string; converted to UTF-8 it is the uid kept in the prefs
(`audioOutputDeviceUID`), as the Mac keeps CoreAudio's. The device's channel count is the largest of its
native formats. No device chosen, or the chosen one gone: the default output.

## 2. `data_callback()`

The engine renders the stereo pair, in chunks of up to MAX_CALLBACK_FRAMES; each frame's two samples go to
the chosen left and right outputs of the device's own frame, the rest silent - what the Mac's output channel
map does. The device runs at its own rate and channel count (shared mode); the engine is told the rate before
the first callback, as on the Mac. WASAPI's thread is registered with MMCSS as "Pro Audio"
(`ma_wasapi_usage_pro_audio`).

## 3. Render Ahead

Kept and saved so the menu and the prefs behave as on the Mac, but not used: WASAPI's own buffering stands in.

## 4. Overloads

WASAPI reports none, so the count is 0; the engine's own "late" figure (sound-engine-notes §207) is the one
to read.
