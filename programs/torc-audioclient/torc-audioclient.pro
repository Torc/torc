include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
TARGET = torc-audioclient
target.path = $${PREFIX}/bin
INSTALLS = target

QT -= gui

INCLUDEPATH += ../../libs/libtorc-core
INCLUDEPATH += ../../libs/libtorc-audio
INCLUDEPATH += ../../libs/libtorc-media
LIBS += -L../../libs/libtorc-core -ltorc-core-$$LIBVERSION
LIBS += -L../../libs/libtorc-audio -ltorc-audio-$$LIBVERSION
LIBS += -L../../libs/libtorc-media -ltorc-media-$$LIBVERSION

setting.path = $${PREFIX}/share/$${PROJECTNAME}/
setting.extra = -ldconfig

INSTALLS += setting

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += audioclientcommandlineparser.h
SOURCES += main.cpp
SOURCES += audioclientcommandlineparser.cpp

macx {
    # OS X has no ldconfig
    setting.extra -= -ldconfig
}

# OpenBSD ldconfig expects different arguments than the Linux one
openbsd {
    setting.extra -= -ldconfig
    setting.extra += -ldconfig -R
}
