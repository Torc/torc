include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-media

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

INCLUDEPATH += ../libtorc-core ../libtorc-core/platforms
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../libtorc-core -ltorc-core-$$LIBVERSION

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

QT += sql
QT -= gui

HEADERS += torcmedia.h
HEADERS += torcmetadata.h
HEADERS += torcmediasource.h
HEADERS += torcmediamaster.h
HEADERS += torcmediasourcedirectory.h

SOURCES += torcmedia.cpp
SOURCES += torcmetadata.cpp
SOURCES += torcmediasource.cpp
SOURCES += torcmediamaster.cpp
SOURCES += torcmediasourcedirectory.cpp

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files  = torcmedia.h
inc.files += torcmetadata.h
inc.files += torcmediamaster.h
inc.files += torcmediasource.cpp

# Allow both #include <blah.h> and #include <libxx/blah.h>
inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES += LIBDIRNAME=\\\"$${LIBDIRNAME}\\\"
DEFINES += TORC_MEDIA_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
