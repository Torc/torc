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

INCLUDEPATH += ../../libs/libtorc-core ../../libs/libtorc-media
LIBS += -L../../libs/libtorc-core -ltorc-core-$$LIBVERSION
LIBS += -L../../libs/libtorc-media -ltorc-media-$$LIBVERSION

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
