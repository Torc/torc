include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
TARGET = torc-desktop
target.path = $${PREFIX}/bin
INSTALLS = target

QT += qml quick

qtHaveModule(widgets) {
    QT += widgets
}

DEPENDPATH  += ../../libs/libtorc-core
DEPENDPATH  += ../../libs/libtorc-media
DEPENDPATH  += ../../libs/libtorc-qml
DEPENDPATH  += ../../libs/libtorc-av
DEPENDPATH  += ../../libs/libtorc-audio
DEPENDPATH  += ../../libs/libtorc-video
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../../libs/libtorc-core             -ltorc-core-$$LIBVERSION
LIBS += -L../../libs/libtorc-audio            -ltorc-audio-$$LIBVERSION
LIBS += -L../../libs/libtorc-video            -ltorc-video-$$LIBVERSION
LIBS += -L../../libs/libtorc-media            -ltorc-media-$$LIBVERSION
LIBS += -L../../libs/libtorc-qml              -ltorc-qml-$$LIBVERSION
LIBS += -L../../libs/libtorc-av/libavformat   -ltorc-avformat
LIBS += -L../../libs/libtorc-av/libavcodec    -ltorc-avcodec
LIBS += -L../../libs/libtorc-av/libavutil     -ltorc-avutil
LIBS += -L../../libs/libtorc-av/libavdevice   -ltorc-avdevice
LIBS += -L../../libs/libtorc-av/libavresample -ltorc-avresample
LIBS += -L../../libs/libtorc-av/libswscale    -ltorc-swscale

SOURCES += main.cpp

qmlfiles.path  = $${PREFIX}/share/$${PROJECTNAME}/torc-desktop/
qmlfiles.files = qml
INSTALLS += qmlfiles

macx {
    # OS X has no ldconfig
    setting.extra -= -ldconfig
}

# OpenBSD ldconfig expects different arguments than the Linux one
openbsd {
    setting.extra -= -ldconfig
    setting.extra += -ldconfig -R
}

win32 : !debug {
    # To hide the window that contains logging output:
    CONFIG -= console
    DEFINES += WINDOWS_CLOSE_CONSOLE
}
