include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
TARGET = torc-client
target.path = $${PREFIX}/bin
INSTALLS = target

QT += script

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

INCLUDEPATH += ../../libs/libtorc-baseui
INCLUDEPATH += ../../libs/libtorc-tenfootui
INCLUDEPATH += ../../libs/libtorc-videoui
LIBS += -L../../libs/libtorc-baseui -ltorc-baseui-$$LIBVERSION
LIBS += -L../../libs/libtorc-tenfootui -ltorc-tenfootui-$$LIBVERSION
LIBS += -L../../libs/libtorc-video -ltorc-videoui-$$LIBVERSION

setting.path = $${PREFIX}/share/$${PROJECTNAME}/
setting.extra = -ldconfig

INSTALLS += setting

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += clientcommandlineparser.h

SOURCES += main.cpp
SOURCES += clientcommandlineparser.cpp

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
