# G2-Edit

Very much a work in-progress. Not yet tidied or rationalised.

If anyone's interested in helping, especially with the GUI side of things,
Please drop me a line.


This application has a dependency on 2 libraries: glfw and freetype2.

To add these to the host build system, here is an example of the commands I used:

// Next command only required if Homebrew not installed (or not up-to-date)

/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

brew install --build-from-source glfw

brew install --build-from-source freetype2

If you have those installed already and want to update, here's the sequence I ran to update:

brew update
brew doctor
brew reinstall pkgconf
brew upgrade

Todo: Many mechanisms yet to implement - including copying, file writing, etc. 200+ modules to implement and test. Graphical representation of filter curves / oscillator waves etc.

See [THIRD_PARTY.md](./THIRD_PARTY.md) for open-source acknowledgments.
