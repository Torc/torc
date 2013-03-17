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
LIBS += -L../libtorc-av/libswscale -ltorc-swscale

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += torcvideoexport.h
HEADERS += videoplayer.h
HEADERS += videodecoder.h
HEADERS += videoframe.h
HEADERS += videobuffers.h
HEADERS += videocolourspace.h

SOURCES += videoplayer.cpp
SOURCES += videodecoder.cpp
SOURCES += videoframe.cpp
SOURCES += videobuffers.cpp
SOURCES += videocolourspace.cpp

contains(CONFIG_VDA, yes) {
    HEADERS += platforms/videovda.h
    SOURCES += platforms/videovda.cpp
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/CoreVideo.framework/Frameworks
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/CoreFoundation.framework/Frameworks
    LIBS           += -framework CoreVideo -framework CoreFoundation
}

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += videoplayer.h    videodecoder.h
inc.files += videoframe.h     videobuffers.h

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += TORC_VIDEO_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

