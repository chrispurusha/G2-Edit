# G2-Edit

A macOS GUI editor for the Nord G2 modular synthesizer. Work in progress.

Binary beta releases: https://github.com/chrispurusha/G2-Edit/releases

If anyone is interested in helping, please drop me a line.

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

### 2. Update SynthLib (contributors / returning developers)

SynthLib is a shared library submodule pinned to a specific commit. If SynthLib has been updated since you last pulled, advance the pin before building:

```
git submodule update --remote SynthLib
git add SynthLib && git commit -m "Update SynthLib"
```

Do not manually copy files into the `SynthLib/` directory — this will cause conflicts on the next update.

### 3. Build third-party libraries

All commands run from the root of the cloned repository.

**glfw:**

```
cmake -S SynthLib/ThirdParty/glfw -B SynthLib/ThirdParty/glfw/build \
  -DBUILD_SHARED_LIBS=OFF \
  -DGLFW_BUILD_DOCS=OFF \
  -DGLFW_BUILD_EXAMPLES=OFF \
  -DGLFW_BUILD_TESTS=OFF \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.5
cmake --build SynthLib/ThirdParty/glfw/build
```

**freetype:**

```
cmake -S SynthLib/ThirdParty/freetype -B SynthLib/ThirdParty/freetype/build \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.5 \
  -DFT_DISABLE_BROTLI=ON \
  -DFT_DISABLE_BZIP2=ON \
  -DFT_DISABLE_HVF=ON \
  -DFT_DISABLE_PNG=ON \
  -DFT_DISABLE_ZLIB=ON
cmake --build SynthLib/ThirdParty/freetype/build
```

**libusb:**

```
cd SynthLib/ThirdParty/libusb
export CFLAGS="-arch arm64 -arch x86_64"
export CXXFLAGS="-arch arm64 -arch x86_64"
export LDFLAGS="-arch arm64 -arch x86_64"
autoreconf -fi
./configure --disable-shared --enable-static
make
cd ../../..
```

### 4. Build with Xcode

Open `G2 Editor.xcodeproj` and build normally.

### 5. Code formatting (optional)

After editing source files, run from the repository root:

```
./do-uncrustify
```

Note this covers `src/` and `SynthLib/src/` only — **not** `vst3/`.

## Building the VST3 plug-in (experimental)

The sound engine can also be built as a VST3 plug-in. It plays a `.pch2` file rather than talking
to a G2, and it renders through the same `soundEngine.c` the application uses.

### Prerequisites

Download the [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) — MIT licensed, so it is
compatible with this project's GPLv3 — and place it at `~/Documents/vst3sdk`, or point
`VST3_SDK` at wherever you put it. Only its `pluginterfaces/` directory is compiled, plus one file
from `public.sdk` that does nothing but instantiate interface IDs. No CMake is involved, and the
SDK does **not** need building first.

### Build

```
./do-vst3
```

Writes `build/G2 Edit.vst3` — universal (arm64 + x86_64), ad-hoc signed. To install it:

```
cp -R "build/G2 Edit.vst3" ~/Library/Audio/Plug-Ins/VST3/
```

As with the application, there is no paid Apple Developer membership behind this, so a host may
refuse to load the bundle until its quarantine flag is cleared.

### Choosing the patch

There is no plug-in editor, so the patch is chosen by path, in this order:

1. the path saved into the host project (the plug-in stores a path, not the patch bytes, so
   editing that patch in G2-Edit is picked up rather than frozen into the project)
2. `$G2_VST3_PATCH`
3. `~/Documents/G2-Edit/plugin.pch2`

### Controls

There is no custom editor, so the host draws its own generic panel. Nine parameters are exposed and
all are automatable: **Morph 1-8** (the G2's own performance controls) and **Output Level**.

Pitch bend and mod wheel are not yet routed.

### What to expect

The plug-in inherits exactly what the sound engine can do, which is a **subset** of the G2: around
17 module types. A patch built from anything else will load without complaint and render silence.
`PatchTestFiles/SimpleLead.pch2` is known to work.

See [THIRD_PARTY.md](./THIRD_PARTY.md) for open-source acknowledgments.
