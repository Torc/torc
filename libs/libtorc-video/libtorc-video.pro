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

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += torcvideoexport.h
HEADERS += videoplayer.h    videodecoder.h

SOURCES += videoplayer.cpp  videodecoder.cpp

contains(CONFIG_VDPAU, yes) {
}

contains(CONFIG_VAAPI, yes) {
}

contains(CONFIG_DXVA2, yes) {
}

contains(CONFIG_VDA, yes) {
}

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += videointerface.h

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += TORC_VIDEO_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

