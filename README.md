Torc
====

Library dependencies
------------------

### Required
Qt >= 5.0
libfreetype

### Optional
libcdio - Audio CD playback.
libass - SSA/ASS subtitle rendering.
libdnssd - Bonjour service announcement and discovery.
libcec2 - CEC support.
libasound2 - Alsa sound library.
libudev - USB device detection (linux).

### Hardware video decoding
VDPAU
VAAPI
OpenMax

Building Torc on the Raspberry Pi
------------------------------------------

The following guide is subject to change and is based on a fresh installation of Raspbian.

* Run `raspi-config` and set the video memory to at least 128mb. You will probably want to enable SSH as well.
* Add the Qt5 Raspbian repository provided by Sébastien Noel. Details can be found at http://twolife.be/raspbian
* Run `sudo apt-get update` followed by `sudo apt-get upgrade` to ensure your Pi is fully up to date.
* Run `sudo apt-get install qtdeclarative5-dev qt5-qmake qtdeclarative5-qtquick2-plugin build-essential git-core libudev-dev libavahi-compat-libdnssd-dev yasm libasound2-dev ccache libass-dev libcec-dev libfreetype6-dev`.
* Checkout the Torc source with `git clone https://github.com/Torc/torc.git`.
* Enter the torc directory and run `./configure`.
* If the configure script fails due to a lack of qmake for Qt5, you will need to specify the path to qmake with something like `./configure --qmake=/usr/lib/arm-linux-gnueabihf/qt5/bin/qmake`.
* Run `make` and go and do the shopping...
* Run `sudo make install`.
* `torc-server` and `torc-utils` can be run as normal from the command line.
* `torc-desktop` and `torc-tv` do not need an X server running but they need the EGL fullscreen plugin specified at run time e.g. `torc-tv -platform eglfs`.


