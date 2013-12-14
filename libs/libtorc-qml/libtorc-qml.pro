include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-qml

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT += qml quick

DEPENDPATH  += ../libtorc-core
DEPENDPATH  += ../libtorc-core/platforms
DEPENDPATH  += ../libtorc-av
DEPENDPATH  += ../libtorc-audio
DEPENDPATH  += ../libtorc-video
DEPENDPATH  += ./peripherals
DEPENDPATH  += ./peripherals/cec
DEPENDPATH  += ./platforms
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../libtorc-core  -ltorc-core-$$LIBVERSION
LIBS += -L../libtorc-audio -ltorc-audio-$$LIBVERSION
LIBS += -L../libtorc-video -ltorc-video-$$LIBVERSION
LIBS += -L../libtorc-av/libavformat -ltorc-avformat
LIBS += -L../libtorc-av/libavcodec  -ltorc-avcodec
LIBS += -L../libtorc-av/libavutil   -ltorc-avutil
LIBS += -L../libtorc-av/libswscale  -ltorc-swscale

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += torcqmlexport.h
HEADERS += torcqmleventproxy.h
HEADERS += torcqmlutils.h
HEADERS += torcqmlframerate.h
HEADERS += torcqmlmediaelement.h
HEADERS += torcsgvideoprovider.h
HEADERS += torcsgvideoplayer.h
HEADERS += torcoverlaydecoder.h
HEADERS += torcblurayuibuffer.h
HEADERS += torcblurayuihandler.h
HEADERS += torcqmlopengldefs.h
HEADERS += torcedid.h
HEADERS += torcqmldisplay.h

SOURCES += torcqmlutils.cpp
SOURCES += torcqmleventproxy.cpp
SOURCES += torcqmlframerate.cpp
SOURCES += torcqmlmediaelement.cpp
SOURCES += torcsgvideoprovider.cpp
SOURCES += torcsgvideoplayer.cpp
SOURCES += torcoverlaydecoder.cpp
SOURCES += torcblurayuibuffer.cpp
SOURCES += torcblurayuihandler.cpp
SOURCES += torcedid.cpp
SOURCES += torcqmldisplay.cpp

#libbluray
DEFINES     += HAVE_CONFIG_H DLOPEN_CRYPTO_LIBS HAVE_PTHREAD_H HAVE_DIRENT_H HAVE_STRINGS_H
DEPENDPATH  += ../libtorc-video/libbluray/src
INCLUDEPATH += ../libtorc-video/libbluray/src

#libCEC
HEADERS += peripherals/cec/cec.h
HEADERS += peripherals/cec/cectypes.h
HEADERS += peripherals/torccecdevice.h
SOURCES += peripherals/torccecdevice.cpp

#make sure libass is linked
contains(CONFIG_LIBASS, yes) {
    LIBS += -lass
}

macx {
    LIBS    += -framework Cocoa
    SOURCES += platforms/torcqmldisplayosx.h
    SOURCES += platforms/torcqmldisplayosx.cpp
}

linux-rasp-pi-g++ {
    HEADERS    += platforms/torcraspberrypi.h
    HEADERS    += platforms/videodecoderomx.h
    HEADERS    += platforms/torcomxvideoplayer.h
    SOURCES    += platforms/torcraspberrypi.cpp
    SOURCES    += platforms/videodecoderomx.cpp
    SOURCES    += platforms/torcomxvideoplayer.cpp
    DEPENDPATH += ./openmax
    HEADERS    += openmax/torcomxcore.h
    HEADERS    += openmax/torcomxport.h
    HEADERS    += openmax/torcomxtunnel.h
    HEADERS    += openmax/torcomxcomponent.h
    SOURCES    += openmax/torcomxcore.cpp
    SOURCES    += openmax/torcomxport.cpp
    SOURCES    += openmax/torcomxtunnel.cpp
    SOURCES    += openmax/torcomxcomponent.cpp
    inc.files  += openmax/torcomxcore.h
    inc.files  += openmax/torcomxport.h
    inc.files  += openmax/torcomxtunnel.h
    inc.files  += openmax/torcomxcomponent.h
}

win32|contains(CONFIG_X11BASE, yes) {
    DEPENDPATH += ./platforms/adl
    HEADERS    += platforms/adl/include/adl_defines.h
    HEADERS    += platforms/adl/include/adl_sdk.h
    HEADERS    += platforms/adl/include/adl_structures.h
    HEADERS    += platforms/adl/torcadl.h
    SOURCES    += platforms/adl/torcadl.cpp
}

contains(CONFIG_X11BASE, yes) {
    contains(CONFIG_VDPAU, yes) {
        DEPENDPATH += ./platforms
        HEADERS += platforms/videovdpau.h
        HEADERS += platforms/nvidiavdpau.h
        SOURCES += platforms/videovdpau.cpp
        SOURCES += platforms/nvidiavdpau.cpp
        LIBS    += -lvdpau
    }

    contains(CONFIG_VAAPI, yes) {
        DEPENDPATH += ./platforms
        HEADERS += platforms/videovaapi.h
        SOURCES += platforms/videovaapi.cpp
        LIBS    += -lva
    }

    DEPENDPATH += ./platforms/nvctrl
    HEADERS    += platforms/nvctrl/include/NVCtrl.h
    HEADERS    += platforms/nvctrl/include/NVCtrlLib.h
    HEADERS    += platforms/nvctrl/include/nv_control.h
    HEADERS    += platforms/nvctrl/torcnvcontrol.h
    SOURCES    += platforms/nvctrl/include/NVCtrl.c
    SOURCES    += platforms/nvctrl/torcnvcontrol.cpp
    HEADERS    += platforms/torcqmldisplayx11.h
    SOURCES    += platforms/torcqmldisplayx11.cpp
    LIBS       += -lXxf86vm
}

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += torcqmlutils.h
inc.files += torcqmleventproxy.h
inc.files += torcqmlframerate.h

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES += LIBDIRNAME=\\\"$${LIBDIRNAME}\\\"
DEFINES += TORC_QML_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

