# G2-Edit

Binary beta release can be found here:

https://github.com/chrispurusha/G2-Edit/releases/tag/V0.0.1-beta.1

Very much a work in-progress. Not yet tidied or rationalised.

If anyone's interested in helping, especially with the GUI side of things,
Please drop me a line.

Since I'm now incurring costs (I recently started using LLMS) which would be good to at least cover, I now have a Buy-me-a-coffee page:

buymeacoffee.com/chrispurusha

Thanks for any donations!

This application has a dependency on 3 libraries: glfw, freetype2 and libusb.

To add these to the host build system, here is an example of the commands I used:

// Next command only required if Homebrew not installed (or not up-to-date)

/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

brew install glfw

brew install freetype2

brew install libusb

// For uncrustify, which is a 'C' beautifier

brew install uncrustify

(Note - seems I no longer have to use ...install --build-from-source <library>)

If you have those installed already and want to update, here's the sequence I ran to update:

brew update

brew doctor

brew reinstall pkgconf

brew upgrade

Todo: Many mechanisms yet to implement - including copying, etc. 200+ modules to test. Graphical representation of filter curves / oscillator waves etc.

See [THIRD_PARTY.md](./THIRD_PARTY.md) for open-source acknowledgments.
