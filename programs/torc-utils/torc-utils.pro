include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
TARGET = torc-utils
target.path = $${PREFIX}/bin
INSTALLS = target

QT -= gui

DEPENDPATH  += ../../libs/libtorc-core
DEPENDPATH  += ../../libs/libtorc-audio
DEPENDPATH  += ../../libs/libtorc-av
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../../libs/libtorc-core             -ltorc-core-$$LIBVERSION
LIBS += -L../../libs/libtorc-audio            -ltorc-audio-$$LIBVERSION
LIBS += -L../../libs/libtorc-av/libavformat   -ltorc-avformat
LIBS += -L../../libs/libtorc-av/libavcodec    -ltorc-avcodec
LIBS += -L../../libs/libtorc-av/libavutil     -ltorc-avutil
LIBS += -L../../libs/libtorc-av/libavdevice   -ltorc-avdevice
LIBS += -L../../libs/libtorc-av/libavresample -ltorc-avresample
LIBS += -L../../libs/libtorc-av/libswresample -ltorc-swresample
LIBS += -L../../libs/libtorc-av/libavfilter   -ltorc-avfilter
LIBS += -L../../libs/libtorc-av/libswscale    -ltorc-swscale
LIBS += -L../../libs/libtorc-av/libpostproc   -ltorc-postproc

setting.path = $${PREFIX}/share/$${PROJECTNAME}/
setting.extra = -ldconfig

INSTALLS += setting

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += torcutils.h
SOURCES += main.cpp
SOURCES += torcutils.cpp

macx {
    # OS X has no ldconfig
    setting.extra -= -ldconfig
}

# OpenBSD ldconfig expects different arguments than the Linux one
openbsd {
    setting.extra -= -ldconfig
    setting.extra += -ldconfig -R
}
