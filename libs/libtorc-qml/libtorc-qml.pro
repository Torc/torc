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
HEADERS += torcqmlopengldefs.h
HEADERS += torcedid.h
SOURCES += torcqmlutils.cpp
SOURCES += torcqmleventproxy.cpp
SOURCES += torcqmlframerate.cpp
SOURCES += torcqmlmediaelement.cpp
SOURCES += torcsgvideoprovider.cpp
SOURCES += torcsgvideoplayer.cpp
SOURCES += torcedid.cpp

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

