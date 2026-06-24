# G2-Edit

A macOS GUI editor for the Nord G2 modular synthesizer. Work in progress.

Binary beta releases: https://github.com/chrispurusha/G2-Edit/tags

If anyone is interested in helping, especially with the GUI side of things, please drop me a line.

Since I'm now incurring costs (I recently started using LLMs) which would be good to at least cover, I now have a Buy Me a Coffee page:

https://buymeacoffee.com/chrispurusha

Thanks for any donations!

## Building

### Prerequisites

Install the following via Homebrew (https://brew.sh):

```
brew install cmake autoconf automake libtool uncrustify
```

- **cmake** — builds glfw and freetype
- **autoconf / automake / libtool** — builds libusb
- **uncrustify** — code formatter (optional, only needed when editing source)

Xcode and its command line tools are also required:

```
xcode-select --install
```

### 1. Clone with submodules

The third-party libraries (glfw, freetype, libusb) are nested submodules inside SynthLib. The `--recurse-submodules` flag is required — without it the build will fail.

```
git clone --recurse-submodules https://github.com/chrispurusha/G2-Edit.git
```

If you already cloned without `--recurse-submodules`:

```
git submodule update --init --recursive
```

### 2. Build the third-party libraries

All commands run from the root of the cloned repository.

**glfw:**

```
cmake -S SynthLib/ThirdParty/glfw -B SynthLib/ThirdParty/glfw/build \
  -DGLFW_BUILD_DOCS=OFF \
  -DGLFW_BUILD_EXAMPLES=OFF \
  -DGLFW_BUILD_TESTS=OFF
cmake --build SynthLib/ThirdParty/glfw/build
```

**freetype:**

```
cmake -S SynthLib/ThirdParty/freetype -B SynthLib/ThirdParty/freetype/build \
  -DFT_DISABLE_BROTLI=ON \
  -DFT_DISABLE_BZIP2=ON \
  -DFT_DISABLE_PNG=ON \
  -DFT_DISABLE_ZLIB=ON
cmake --build SynthLib/ThirdParty/freetype/build
```

**libusb:**

```
cd SynthLib/ThirdParty/libusb
./bootstrap.sh
./configure
make
cd ../../..
```

### 3. Build with Xcode

Open `G2 Editor.xcodeproj` and build normally.

### 4. Code formatting (optional)

After editing source files, run from the repository root:

```
./do-uncrustify
```

## Todo

Many mechanisms yet to implement — including copying, etc. 200+ modules to test. Graphical representation of filter curves / oscillator waves etc.

See [THIRD_PARTY.md](./THIRD_PARTY.md) for open-source acknowledgments.
