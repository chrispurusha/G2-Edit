# G2 Edit

Very much a work in-progress. Not yet tidied or rationalised.

If anyone's interested in helping, especially with the GUI side of things,
Please drop me a line.


This application has a dependency on 2 libraries: glfw and freetype2.

To add these to the host build system, here is an example of the commands I used:

// Next command only required if Homebrew not installed (or not up-to-date)

/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

brew install --build-from-source glfw

brew install --build-from-source freetype2


Todo: Many mechanisms yet to implement - including copying, file writing, morph group modulation etc. 200+ modules to implement. Graphical representation of filter curves / oscillator waves etc.

