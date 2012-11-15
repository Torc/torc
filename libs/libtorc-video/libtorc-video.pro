include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-video

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

DEPENDPATH  += ./platforms ../libtorc-av ../libtorc-audio
INCLUDEPATH += ../libtorc-core ../libtorc-core/platforms
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../libtorc-core -ltorc-core-$$LIBVERSION
LIBS += -L../libtorc-audio -ltorc-audio-$$LIBVERSION
LIBS += -L../libtorc-av/libavformat -ltorc-avformat
LIBS += -L../libtorc-av/libavcodec -ltorc-avcodec
LIBS += -L../libtorc-av/libavutil -ltorc-avutil

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += torcvideoexport.h
HEADERS += videoplayer.h    videodecoder.h
HEADERS += videoframe.h     videobuffers.h
HEADERS += videocolourspace.h

SOURCES += videoplayer.cpp  videodecoder.cpp
SOURCES += videoframe.cpp   videobuffers.cpp
SOURCES += videocolourspace.cpp

contains(CONFIG_VDPAU, yes) {
}

contains(CONFIG_VAAPI, yes) {
}

contains(CONFIG_DXVA2, yes) {
}

contains(CONFIG_VDA, yes) {
}

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += videointerface.h   videobuffer.h

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += TORC_VIDEO_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

