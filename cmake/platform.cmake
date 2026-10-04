# What a port does differently from the Xcode project. HAND-WRITTEN, unlike ../CMakeLists.txt, which
# tools/gen-cmake writes from "G2 Editor.xcodeproj" and includes this. See Docs/windows-port-plan.md.
#
# It may change G2_SOURCES and set G2_PLATFORM_SOURCES, G2_PLATFORM_LIBS, G2_PLATFORM_DEFINES and
# G2_PLATFORM_INCLUDES; the target is made after it runs.

if(APPLE)
    # the Mac builds exactly what the Xcode project builds
    return()
endif()

if(WIN32)
    # Third-party libraries from MSYS2 (CLANG64 or UCRT64), found through pkg-config:
    #   pacman -S mingw-w64-clang-x86_64-{clang,cmake,ninja,pkgconf,glfw,freetype,libusb}
    find_package(PkgConfig REQUIRED)

    # tools/do-windows, cross-building one self-contained .exe, links the libraries statically and
    # needs what each one in turn links (GLFW's gdi32, for one) - pkg-config's --static answers
    option(G2_WIN_STATIC "link glfw, freetype and libusb statically" OFF)

    if(G2_WIN_STATIC)
        set(PKG_CONFIG_ARGN --static)
    endif()
    pkg_check_modules(G2_DEPS REQUIRED IMPORTED_TARGET glfw3 freetype2 libusb-1.0)
    find_package(Threads REQUIRED)    # winpthreads: the code is pthreads throughout
    list(APPEND G2_PLATFORM_LIBS PkgConfig::G2_DEPS Threads::Threads opengl32 winmm)

    # The files that talk to CoreAudio and CoreMIDI, swapped for Windows ones as they are written
    # (platform/windows/, plan step 3). Until then the stubs keep the editor building and running silent.
    set(G2_WINDOWS_SWAPS
        src/audioOutput.c               platform/windows/audioOutputWin.c
        src/midiInput.c                 platform/windows/midiInputWin.c
        SynthLib/src/synthlibMidi.c     platform/windows/synthlibMidiWin.c
        SynthLib/src/synthlibMidiPorts.c platform/windows/synthlibMidiPortsWin.c
    )
    list(LENGTH G2_WINDOWS_SWAPS swapCount)
    math(EXPR lastPair "${swapCount} - 1")

    foreach(i RANGE 0 ${lastPair} 2)
        math(EXPR j "${i} + 1")
        list(GET G2_WINDOWS_SWAPS ${i} mac)
        list(GET G2_WINDOWS_SWAPS ${j} win)
        list(REMOVE_ITEM G2_SOURCES ${mac})

        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${win})
            list(APPEND G2_PLATFORM_SOURCES ${win})
        else()
            message(WARNING "${mac} needs a Windows version (${win}) - left out for now")
        endif()
    endforeach()

    # misc.mm's five functions (setup_main_menu, platform_begin/end_audio_activity,
    # register_sleep_wake_notifications, platform_any_mouse_button_down)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/miscWin.c)
        list(APPEND G2_PLATFORM_SOURCES platform/windows/miscWin.c)
    endif()

    # platform/windows/ is outside src/, so it needs src/'s headers by path
    list(APPEND G2_PLATFORM_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src)

    # The POSIX and OpenGL names MinGW lacks, given to every file (platform/windows/winCompat.h)
    add_compile_options(-include ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/winCompat.h)

    # _POSIX_THREAD_SAFE_FUNCTIONS: MinGW's time.h declares localtime_r() only with it
    list(APPEND G2_PLATFORM_DEFINES _USE_MATH_DEFINES _POSIX_THREAD_SAFE_FUNCTIONS=200112L)
endif()
