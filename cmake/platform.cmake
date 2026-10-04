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
    pkg_check_modules(G2_DEPS REQUIRED IMPORTED_TARGET glfw3 freetype2 libusb-1.0)
    list(APPEND G2_PLATFORM_LIBS PkgConfig::G2_DEPS opengl32 winmm)

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

    list(APPEND G2_PLATFORM_DEFINES _USE_MATH_DEFINES)
endif()
