include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-tenfootui

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT += xml script

INCLUDEPATH += ../libtorc-core ../libtorc-core/platforms
INCLUDEPATH += ../libtorc-baseui
INCLUDEPATH += ../ ../..

LIBS += -L../libtorc-core -ltorc-core-$$LIBVERSION
LIBS += -L../libtorc-baseui -ltorc-baseui-$$LIBVERSION

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += tenfoottheme.h         tenfootthemeloader.h
HEADERS += tenfootclock.h

SOURCES += tenfoottheme.cpp       tenfootthemeloader.cpp
SOURCES += tenfootclock.cpp

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += tenfoottheme.h

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES += LIBDIRNAME=\\\"$${LIBDIRNAME}\\\"
DEFINES += TORC_TENFOOTUI_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS


