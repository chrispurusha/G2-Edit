# Cross-compiling for Windows from a Mac, with llvm-mingw (https://github.com/mstorsjo/llvm-mingw).
# HAND-WRITTEN; used by tools/do-windows, which passes G2_WIN_ARCH and LLVM_MINGW. See
# Docs/windows-port-plan.md, "Cross-building on the Mac".

if(NOT G2_WIN_ARCH)
    set(G2_WIN_ARCH x86_64)            # or aarch64, for Windows on ARM (Parallels on Apple silicon)
endif()

if(NOT LLVM_MINGW)
    set(LLVM_MINGW $ENV{HOME}/Developer/llvm-mingw)
endif()

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ${G2_WIN_ARCH})

set(G2_WIN_TRIPLE ${G2_WIN_ARCH}-w64-mingw32)
set(CMAKE_C_COMPILER   ${LLVM_MINGW}/bin/${G2_WIN_TRIPLE}-clang)
set(CMAKE_CXX_COMPILER ${LLVM_MINGW}/bin/${G2_WIN_TRIPLE}-clang++)
set(CMAKE_RC_COMPILER  ${LLVM_MINGW}/bin/${G2_WIN_TRIPLE}-windres)
set(CMAKE_AR           ${LLVM_MINGW}/bin/${G2_WIN_TRIPLE}-ar)
set(CMAKE_RANLIB       ${LLVM_MINGW}/bin/${G2_WIN_TRIPLE}-ranlib)

# Find headers and libraries in the toolchain and the dependencies' prefix, programs on the Mac
set(CMAKE_FIND_ROOT_PATH ${LLVM_MINGW}/${G2_WIN_TRIPLE} ${G2_WIN_DEPS})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# One self-contained .exe: the C++ runtime, winpthreads and the libraries all linked in
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
