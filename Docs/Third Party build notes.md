**The app now targets macOS 12.0, these libraries are still built at 11.5 (2026-09-21).**
That is deliberate and safe - a library built to a LOWER minimum links into a higher-target
app without complaint; it is the other way round that produces the "built for newer macOS"
warning. Leave the numbers below alone unless the libraries are being rebuilt anyway.

# These are the kinds of commands I used to pull in and build the 3rd party libraries.
# Needs some rationalising and potentially adding to one of the existing readme files.

# These brew components required
brew install autoconf automake libtool cmake

mkdir ThirdParty

git submodule add --force https://github.com/glfw/glfw.git ThirdParty/glfw

git submodule add --force https://github.com/libusb/libusb.git ThirdParty/libusb

git submodule add --force https://gitlab.freedesktop.org/freetype/freetype.git ThirdParty/freetype

git submodule update --init --recursive

cd ThirdParty/freetype

cmake -B build \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.5 \
      -DFT_DISABLE_PNG=ON \
      -DFT_DISABLE_ZLIB=ON \
      -DFT_DISABLE_BZIP2=ON \
      -DFT_DISABLE_BROTLI=ON \
      -DFT_DISABLE_HVF=ON \
      -DFT_DISABLE_HARFBUZZ=ON

cmake --build build --config Release

cd ThirdParty/glfw

cmake -B build \
    -DBUILD_SHARED_LIBS=OFF \
    -DGLFW_BUILD_EXAMPLES=OFF \
    -DGLFW_BUILD_TESTS=OFF \
    -DGLFW_BUILD_DOCS=OFF \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.5

cmake --build build --config Release

cd ThirdParty/libusb

export CFLAGS="-arch arm64 -arch x86_64"
export CXXFLAGS="-arch arm64 -arch x86_64"
export LDFLAGS="-arch arm64 -arch x86_64"

autoreconf -fi
./configure --disable-shared --enable-static
make
