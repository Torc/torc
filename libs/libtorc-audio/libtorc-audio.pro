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

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += audiosettings.h          audiooutputsettings.h
HEADERS += audiooutput.h            audiooutputbase.h
HEADERS += audiooutputnull.h        audiooutpututil.h
HEADERS += audiooutputlisteners.h   audiooutputdownmix.h
HEADERS += audiovolume.h            audioeld.h
HEADERS += audiospdifencoder.h      audiooutputdigitalencoder.h

SOURCES += audiosettings.cpp        audiooutputsettings.cpp
SOURCES += audiooutput.cpp          audiooutputbase.cpp
SOURCES += audiooutputnull.cpp      audiooutpututil.cpp
SOURCES += audiooutputlisteners.cpp audiooutputdownmix.cpp
SOURCES += audiovolume.cpp          audioeld.cpp
SOURCES += audiospdifencoder.cpp    audiooutputdigitalencoder.cpp

HEADERS += torcavutils.h            torcdecoder.h
SOURCES += torcavutils.cpp          torcdecoder.cpp

contains(CONFIG_LIBPULSE, yes) {
    HEADERS += audiopulsehandler.h
    SOURCES += audiopulsehandler.cpp
    LIBS    += -lpulse
}

macx {
    HEADERS += audiooutputosx.h
    SOURCES += audiooutputosx.cpp

    FWKS = ApplicationServices AudioUnit AudioToolbox CoreAudio
    FC = $$join(FWKS,",","{","}")
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/$${FC}.framework/Frameworks
    LIBS           += -framework $$join(FWKS," -framework ")
}

contains(CONFIG_ALSA_OUTDEV, yes) {
    HEADERS += audiooutputalsa.h
    SOURCES += audiooutputalsa.cpp
    LIBS    += -lasound
}

#soundtouch
HEADERS += AAFilter.h       cpu_detect.h FIRFilter.h
HEADERS += RateTransposer.h TDStretch.h  STTypes.h

SOURCES += AAFilter.cpp FIRFilter.cpp FIFOSampleBuffer.cpp
SOURCES += RateTransposer.cpp SoundTouch.cpp TDStretch.cpp
SOURCES += cpu_detect_x86.cpp

contains(ARCH_X86, yes) {
    DEFINES += ALLOW_SSE2 ALLOW_SSE3
    SOURCES += sse_optimized.cpp
    unix:QMAKE_CXXFLAGS += -msse3
}

# samplerate
HEADERS += samplerate.h
SOURCES += samplerate.c src_linear.c src_sinc.c src_zoh.c

#freesurround
HEADERS += el_processor.h           freesurround.h
SOURCES += el_processor.cpp         freesurround.cpp

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += audiosettings.h audiooutput.h audiooutputsettings.h
inc.files += torcavutils.h   torcdecoder.h

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += TORC_AUDIO_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

