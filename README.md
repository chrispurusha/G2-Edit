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

Writes `build/G2 Alike.vst3` — universal (arm64 + x86_64), ad-hoc signed. To install it:

```
cp -R "build/G2 Alike.vst3" ~/Library/Audio/Plug-Ins/VST3/
```

As with the application, there is no paid Apple Developer membership behind this, so a host may
refuse to load the bundle until its quarantine flag is cleared.

### Which patch it plays

**Temporarily, one patch is compiled into the plug-in** — `PatchTestFiles/SimpleLead.pch2` by
default, overridable at build time with `$G2_BUILTIN_PATCH`. `do-vst3` generates
`vst3/g2BuiltInPatch.h` from it on every build, so the file is the source of truth and the header
is build output.

That is not just convenience. A host may sandbox a plug-in and deny it access to `~/Documents`, in
which case a perfectly correct file path still produces silence — and silence looks the same
whatever caused it. Embedding removes that whole class of failure while the rest is being proven.

The file-path machinery is still in the source behind it, unused for now, and will choose the patch
in this order when it comes back: the path saved into the host project, then `$G2_VST3_PATCH`, then
`~/Documents/G2-Edit/plugin.pch2`. Note that a host launched from the Dock does not inherit shell
environment variables, so that middle option only ever applies to a scripted run.

### Controls

The plug-in is called **G2 Alike**, since it plays a patch rather than editing one.

It has its own editor window: a Cocoa panel with sliders for **Morph 1-8** — the G2's own
performance controls — and an **Output Level** trim, plus a readout of which patch is loaded. All
nine are exposed as automatable VST3 parameters as well, so a host can record them.

That window is *not* a view of the patch. The application draws through GLFW, which owns its own
window and cannot adopt the one a host provides, so the plug-in has a second and much smaller
interface rather than a port of the editor's canvas.

Pitch bend and mod wheel are not yet routed.

### What to expect

The plug-in inherits exactly what the sound engine can do, which is a **subset** of the G2: around
17 module types. A patch built from anything else will load without complaint and render silence.
`PatchTestFiles/SimpleLead.pch2` is known to work.

See [THIRD_PARTY.md](./THIRD_PARTY.md) for open-source acknowledgments.
