include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-videoui

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT += opengl

DEPENDPATH  += ./platforms ./opengl ./direct3d ../libtorc-av ../libtorc-audio ../libtorc-video
INCLUDEPATH += ../libtorc-core ../libtorc-core/platforms ../libtorc-baseui
INCLUDEPATH += ../libtorc-baseui/opengl ../libtorc-baseui/direct3d
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../libtorc-core    -ltorc-core-$$LIBVERSION
LIBS += -L../libtorc-audio   -ltorc-audio-$$LIBVERSION
LIBS += -L../libtorc-video   -ltorc-video-$$LIBVERSION
LIBS += -L../libtorc-baseui  -ltorc-baseui-$$LIBVERSION
LIBS += -L../libtorc-av/libavformat  -ltorc-avformat
LIBS += -L../libtorc-av/libavcodec   -ltorc-avcodec
LIBS += -L../libtorc-av/libavutil    -ltorc-avutil
LIBS += -L../libtorc-av/libswscale   -ltorc-swscale

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += torcvideouiexport.h
HEADERS += videocolourspace.h
HEADERS += videouiplayer.h
HEADERS += videorenderer.h

SOURCES += videocolourspace.cpp
SOURCES += videouiplayer.cpp
SOURCES += videorenderer.cpp

win32 {
    HEADERS += direct3d/videorenderdirect3d9.h
    SOURCES += direct3d/videorenderdirect3d9.cpp
}
else {
    HEADERS += opengl/videorendereropengl.h
    SOURCES += opengl/videorendereropengl.cpp
}

contains(CONFIG_X11BASE, yes) {
    contains(CONFIG_VDPAU, yes) {
        DEPENDPATH += ./platforms
        HEADERS += platforms/videovdpau.h
        SOURCES += platforms/videovdpau.cpp
        LIBS    += -lvdpau
    }

    contains(CONFIG_VAAPI, yes) {
        DEPENDPATH += ./platforms
        HEADERS += platforms/videovaapi.h
        SOURCES += platforms/videovaapi.cpp
        LIBS    += -lva
    }
}

contains(CONFIG_DXVA2, yes) {
}

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files +=

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += TORC_VIDEOUI_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

