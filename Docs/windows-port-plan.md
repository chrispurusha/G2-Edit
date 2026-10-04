# Windows port - plan

Living design note, started 2026-10-04 (CT and Claude). Read this first in a Windows session: it was written
on the Mac, for a session that has none of the Mac session's memory.

## The decision: the Xcode project stays the master

The Mac build is Xcode's and stays so. A CMake build is GENERATED from it:

```
tools/gen-cmake            # writes CMakeLists.txt from "G2 Editor.xcodeproj"
tools/gen-cmake --check    # exits 1 if CMakeLists.txt is stale
```

- `CMakeLists.txt` - generated, never edited. It mirrors the project: the synchronized folders (`src`,
  `SynthLib/src`) and every source file in them, the project's exceptions, the C17/C++20 standards, the Debug
  (-O1, `DEBUG=1`, `ENABLE_LOG_DEBUG=1`) and Release (-Os) settings, the warning switches the project turns on,
  -Werror, the header paths, and on a Mac the frameworks and the bundled static libraries.
- `cmake/platform.cmake` - hand-written. Everything a port does DIFFERENTLY: which files are swapped, which
  libraries are linked, extra defines. The generated file includes it before making the target.
- Re-run `tools/gen-cmake` (on either machine - it is Python 3 and reads only the project file and the folders)
  whenever the Xcode project changes or a source file is added or removed. Commit both.

CHECKED on the Mac 2026-10-04: the generated CMake builds the application with Ninja under -Werror (80
files), and the result runs - online with the G2, drawing exactly as the Xcode build.

## What is Apple-only

Small and well contained. Everything else (about 60,000 lines) is portable C on GLFW, FreeType and libusb.

| File | Lines | On Windows |
|---|---|---|
| `SynthLib/src/renderBackendMetal.m` | 802 | not built: `renderBackendSelect.h` picks the OpenGL backend (`renderBackendGL.c`, OpenGL 1.1, no platform code) off Apple - it was kept for exactly this |
| `src/misc.mm` | 89 | five functions: `setup_main_menu`, `platform_begin_audio_activity`, `platform_end_audio_activity`, `register_sleep_wake_notifications`, `platform_any_mouse_button_down` - stubs, then real versions |
| `src/audioOutput.c` | 1,053 | CoreAudio output, render-ahead thread, device listing: a Windows version with the same `audioOutput.h` API (30 functions) |
| `src/midiInput.c` | 505 | CoreMIDI input: same `midiInput.h` API (16 functions), on winmm |
| `SynthLib/src/synthlibMidi.c`, `synthlibMidiPorts.c` | ~200 | CoreMIDI out and port names (`synthlibMidiPorts.h` uses `MIDIEndpointRef` in one declaration) |
| `src/sysIncludes.h` | 37 | its Apple includes need guarding |
| `src/soundEngine.c` | - | already portable: its thread pieces have a non-Apple path (POSIX semaphores, no real-time policy, no workgroup) |

`cmake/platform.cmake` already swaps the four C files for `platform/windows/*Win.c` and leaves each out
with a warning until its Windows version exists.

## Toolchain: MSYS2, CLANG64

Microsoft's compiler lacks pthreads and the GNU C extensions the code uses (`gnu17`, `_Thread_local`,
`__attribute__`). MSYS2 gives clang, CMake, Ninja and the three libraries as packages, and CLANG64 keeps the
same compiler - and so the same warnings - as Xcode:

```
# in the "MSYS2 CLANG64" shell
pacman -S --needed mingw-w64-clang-x86_64-{clang,cmake,ninja,pkgconf,glfw,freetype,libusb} git python
git clone --recurse-submodules https://github.com/chrispurusha/G2-Edit.git    # INSIDE Windows, not a Parallels shared folder
cd G2-Edit
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DG2_WERROR=OFF
cmake --build build
```

Clone inside the Windows file system: a Parallels shared folder is slow to build from and its case handling is
not to be trusted. `-DG2_WERROR=OFF` to begin with, so the first build shows every warning at once; turn it back
on once it is clean.

## The G2 over USB - read before plugging in

libusb on Windows talks to a device only through a generic driver (WinUSB). Binding the G2 to WinUSB - with
Zadig (https://zadig.akeo.ie), choosing the "Nord Modular G2" device and "WinUSB" - REPLACES Clavia's own
driver for it. While it is bound, Clavia's editor cannot see the G2. It is reversible (Device Manager >
the G2 > Update driver > Clavia's), but do it knowingly, and not on a machine whose G2 setup must keep working.

## Order of work

1. **Editor builds and draws, offline.** The four swapped files and `misc.mm` stubbed (`platform/windows/`),
   `sysIncludes.h` guarded. Load `.pch2` files from `PatchTestFiles/` and compare the canvas with the Mac's.
   Expect: path separators and the prefs folder (SynthLib's `prefs.cpp`), `_Thread_local` and `pthread_*`
   (MSYS2 has winpthreads), the `/tmp` backdoor channel (`backdoor.c`, `G2_EDIT_BACKDOOR=1`) - use the
   temporary folder - and fonts (the bundled FreeType font path).
2. **The G2 over USB** (WinUSB, above). `usbComms.c` is libusb throughout.
3. **Sound.** `audioOutput.c` on WASAPI - or on `miniaudio` (one MIT-licensed C file, compatible with the
   GPLv3, and it hides WASAPI's ceremony) - and `midiInput.c` on winmm. The engine itself is portable.
4. **The plug-ins, later.** The VST3 wrapper (`SynthLib/plugin/synthlibPluginVst3.cpp`) is portable C++, but
   its editor view is Cocoa (`synthlibPluginVst3View.mm`) and would need a Win32 window with an OpenGL
   context; `do-plugin` is a macOS script. The Audio Unit has no Windows meaning.

Keep `platform/windows/` beside `src/` rather than inside it: `src/` is a synchronized folder, so anything put
there is compiled into the Mac application too.

## Progress

**2026-10-04, Windows (step 1, first pass).** MSYS2 CLANG64 installed. The application builds (`build-win/`,
-Werror off) and runs: the window opens and the menu bar and top bar draw. Not yet compared against the Mac
with a loaded patch.

- `platform/windows/`: `audioOutputWin.c`, `midiInputWin.c`, `synthlibMidiWin.c`, `synthlibMidiPortsWin.c` -
  silent stubs that read and write the Mac's pref keys; `miscWin.c` - `setup_main_menu` does the Mac's prefs
  and settings loading, mouse state via `GetAsyncKeyState`, sleep/wake still a stub; `winCompat.h` -
  force-included (`SIGBUS`, two-argument `mkdir`, `GL_MULTISAMPLE`, `GL_CLAMP_TO_EDGE`).
- `cmake/platform.cmake`: `src/` on the include path, the forced include, `_POSIX_THREAD_SAFE_FUNCTIONS`
  (MinGW's `localtime_r`), `Threads::Threads`.
- Shared files, each Mac-identical by `#if defined (__APPLE__)`: `src/sysIncludes.h` (dispatch),
  `src/graphics.c` (Arial path), `SynthLib/src/synthlibMidi.h` and `synthlibMidiPorts.h` (CoreMIDI; typedefs
  off Apple - a SynthLib commit). `src/usbComms.c`: `usbThread = 0`, not `NULL` (`pthread_t` is an integer in
  winpthreads). NOT YET BUILT ON THE MAC - build there before pushing.

Still open in step 1: five warnings stand between it and -Werror (`long` is 32 bits on Windows:
`renderParams.c:1068`, `usbComms.c:309,328`; unreachable code at `usbComms.c:2549-2550`); the exe is a
console-subsystem program (opens a console window); `deviceSync.c` builds its Recovery folder from `$HOME` and
`Library/Application Support`; load `PatchTestFiles/` and compare with the Mac.

## Rules that still hold on Windows

- `CLAUDE.md` at the root of the GitHub folder on the Mac carries the project's rules; the essentials: comments
  live in `Docs/code-notes/<file>.md` with `// notes §k` pointers; the owner commits and pushes, Claude never

- Run `./do-uncrustify` after edits (MSYS2: `pacman -S mingw-w64-clang-x86_64-uncrustify`).
- Re-run `tools/gen-cmake` after any change to the set of source files, on whichever machine made it.
