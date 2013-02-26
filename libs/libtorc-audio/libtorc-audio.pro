include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-audio

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT -= gui

DEPENDPATH  += ./platforms ./samplerate ./soundtouch ./freesurround ../libtorc-av
INCLUDEPATH += ../libtorc-core ../libtorc-core/platforms
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../libtorc-core -ltorc-core-$$LIBVERSION
LIBS += -L../libtorc-av/libavformat -ltorc-avformat
LIBS += -L../libtorc-av/libavcodec -ltorc-avcodec
LIBS += -L../libtorc-av/libavutil -ltorc-avutil
LIBS += -L../libtorc-av/libavdevice -ltorc-avdevice
LIBS += -L../libtorc-av/libavresample -ltorc-avresample

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += audiosettings.h
HEADERS += audiooutputsettings.h
HEADERS += audiooutput.h
HEADERS += audiooutputnull.h
HEADERS += audiooutpututil.h
HEADERS += audiooutputlisteners.h
HEADERS += audiooutputdownmix.h
HEADERS += audiovolume.h
HEADERS += audioeld.h
HEADERS += audiospdifencoder.h
HEADERS += audiooutputdigitalencoder.h
HEADERS += audiowrapper.h

SOURCES += audiosettings.cpp
SOURCES += audiooutputsettings.cpp
SOURCES += audiooutput.cpp
SOURCES += audiooutputnull.cpp
SOURCES += audiooutpututil.cpp
SOURCES += audiooutputlisteners.cpp
SOURCES += audiooutputdownmix.cpp
SOURCES += audiovolume.cpp
SOURCES += audioeld.cpp
SOURCES += audiospdifencoder.cpp
SOURCES += audiooutputdigitalencoder.cpp
SOURCES += audiowrapper.cpp

HEADERS += audioplayer.h
HEADERS += audiodecoder.h
HEADERS += audiointerface.h
HEADERS += torcavutils.h
SOURCES += audioplayer.cpp
SOURCES += audiodecoder.cpp
SOURCES += audiointerface.cpp
SOURCES += torcavutils.cpp

contains(CONFIG_LIBCDIO_INDEV, yes) {
    DEPENDPATH += ./cdaudio
    HEADERS    += cdaudio/torccdbuffer.h
    SOURCES    += cdaudio/torccdbuffer.cpp
}

contains(CONFIG_LIBPULSE, yes) {
    HEADERS += audiopulsehandler.h
    SOURCES += audiopulsehandler.cpp
    LIBS    += -lpulse
}

macx {
    HEADERS += platforms/audiooutputosx.h
    SOURCES += platforms/audiooutputosx.cpp

    FWKS = ApplicationServices AudioUnit AudioToolbox CoreAudio
    FC = $$join(FWKS,",","{","}")
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/$${FC}.framework/Frameworks
    LIBS           += -framework $$join(FWKS," -framework ")
}

win32 {
    HEADERS += platforms/_mingw_unicode.h
    HEADERS += platforms/dsound.h
    HEADERS += platforms/audiooutputdx.h
    HEADERS += platforms/audiooutputwin.h
    SOURCES += platforms/audiooutputdx.cpp
    SOURCES += platforms/audiooutputwin.cpp
    LIBS    += -lwinmm
}

contains(CONFIG_ALSA_OUTDEV, yes) {
    HEADERS += platforms/audiooutputalsa.h
    SOURCES += platforms/audiooutputalsa.cpp
    LIBS    += -lasound
}

contains(CONFIG_OSS_OUTDEV, yes) {
    HEADERS += platforms/audiooutputoss.h
    SOURCES += platforms/audiooutputoss.cpp
}

contains(CONFIG_LIBCRYPTO, yes) {
    contains(CONFIG_LIBDNS_SD, yes) {
        DEPENDPATH += ./raop
        HEADERS += raop/torcraopdevice.h
        HEADERS += raop/torcraopbuffer.h
        HEADERS += raop/torcraopconnection.h
        SOURCES += raop/torcraopdevice.cpp
        SOURCES += raop/torcraopbuffer.cpp
        SOURCES += raop/torcraopconnection.cpp
        LIBS    += -lcrypto
        QT      += network
    }
}

#soundtouch
HEADERS += soundtouch/AAFilter.h
HEADERS += soundtouch/cpu_detect.h
HEADERS += soundtouch/FIRFilter.h
HEADERS += soundtouch/RateTransposer.h
HEADERS += soundtouch/TDStretch.h
HEADERS += soundtouch/STTypes.h

SOURCES += soundtouch/AAFilter.cpp
SOURCES += soundtouch/FIRFilter.cpp
SOURCES += soundtouch/FIFOSampleBuffer.cpp
SOURCES += soundtouch/RateTransposer.cpp
SOURCES += soundtouch/SoundTouch.cpp
SOURCES += soundtouch/TDStretch.cpp
SOURCES += soundtouch/cpu_detect_x86.cpp

contains(HAVE_SSE3, yes) {
    SOURCES += soundtouch/sse_optimized.cpp
    unix|mingw:QMAKE_CXXFLAGS += -msse3
}

# samplerate
HEADERS += samplerate/samplerate.h
SOURCES += samplerate/samplerate.c
SOURCES += samplerate/src_linear.c
SOURCES += samplerate/src_sinc.c
SOURCES += samplerate/src_zoh.c

#freesurround
HEADERS += freesurround/el_processor.h
HEADERS += freesurround/freesurround.h
SOURCES += freesurround/el_processor.cpp
SOURCES += freesurround/freesurround.cpp

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += audiosettings.h audiooutput.h audiooutputsettings.h
inc.files += audiowrapper.h  audioplayer.h audiointerface.h
inc.files += torcavutils.h   audiodecoder.h

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += TORC_AUDIO_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

