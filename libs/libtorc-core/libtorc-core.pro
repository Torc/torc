include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-core

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

QT += sql network
QT -= gui

DEPENDPATH  += ./platforms ./http
INCLUDEPATH += $$DEPENDPATH

HEADERS += torccoreexport.h   torclogging.h
HEADERS += torcloggingdefs.h  torcthread.h
HEADERS += torcloggingimp.h   torcplist.h
HEADERS += torccompat.h       torcexitcodes.h
HEADERS += torctimer.h        torclocalcontext.h
HEADERS += torcsqlitedb.h     torcdb.h
HEADERS += torcdirectories.h  torcreferencecounted.h
HEADERS += torccoreutils.h    torccommandlineparser.h#
HEADERS += torcevent.h        torcobservable.h
HEADERS += torclocaldefs.h    torcpower.h
HEADERS += torcadminthread.h  torcstoragedevice.h
HEADERS += torcstorage.h      torcusb.h
HEADERS += torcedid.h         torcbuffer.h
HEADERS += torcfilebuffer.h   torclanguage.h
HEADERS += torcplayer.h       torcdecoder.h
HEADERS += torcnetwork.h      torchttprequest.h
HEADERS += torchttpserver.h   torchtmlhandler.h
HEADERS += torchttphandler.h  torchttpconnection.h

SOURCES += torcloggingimp.cpp torcplist.cpp
SOURCES += torcthread.cpp     torclocalcontext.cpp
SOURCES += torctimer.cpp      torcsqlitedb.cpp
SOURCES += torcdb.cpp         torcdirectories.cpp
SOURCES += torccoreutils.cpp  torcreferencecounted.cpp
SOURCES += torccommandlineparser.cpp
SOURCES += torcevent.cpp      torcobservable.cpp
SOURCES += torcpower.cpp      torcadminthread.cpp
SOURCES += torcstoragedevice.cpp
SOURCES += torcstorage.cpp    torcusb.cpp
SOURCES += torcedid.cpp       torcbuffer.cpp
SOURCES += torcfilebuffer.cpp torclanguage.cpp
SOURCES += torcplayer.cpp     torcdecoder.cpp
SOURCES += torcnetwork.cpp    torchttprequest.cpp
SOURCES += torchttpserver.cpp torchtmlhandler.cpp
SOURCES += torchttphandler.cpp torchttpconnection.cpp

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files  = torclogging.h     torclocalcontext.h
inc.files += torcthread.h      torccompat.h
inc.files += torctimer.h       torcplist.h
inc.files += torcdirectories.h torcreferencecounted.h
inc.files += torccoreutils.h   torccommandlineparser.h
inc.files += torcevent.h       torcobservable.h
inc.files += torclocaldefs.h   torcpower.h
inc.files += torcusb.h         torcedid.h
inc.files += torcbuffer.h      torcplayer.h
inc.files += torcdecoder.h     torcnetwork.h
inc.files += torcwebserver.h   torchtmlhandler.h

unix:contains(CONFIG_LIBUDEV, yes) {
    HEADERS += torcusbprivunix.h
    SOURCES += torcusbprivunix.cpp
    LIBS    += -ludev
}

unix:contains(CONFIG_QTDBUS, yes) {
    QT += dbus
    HEADERS += torcpowerunixdbus.h   torcstorageunixdbus.h
    SOURCES += torcpowerunixdbus.cpp torcstorageunixdbus.cpp
}

macx {
    QMAKE_OBJECTIVE_CXXFLAGS += $$QMAKE_CXXFLAGS
    LIBS += -framework Cocoa -framework IOkit -framework DiskArbitration
    OBJECTIVE_HEADERS += torccocoa.h
    OBJECTIVE_SOURCES += torccocoa.mm
    HEADERS += torcosxutils.h   torcpowerosx.h   torcstorageosx.h
    HEADERS += torcrunlooposx.h torcusbprivosx.h
    SOURCES += torcosxutils.cpp torcpowerosx.cpp torcstorageosx.cpp
    SOURCES += torcrunlooposx.cpp torcusbprivosx.cpp
    inc.files += torccocoa.h torcosxutils.h
}

contains(CONFIG_LIBDNS_SD, yes) {
    HEADERS   += torcbonjour.h
    SOURCES   += torcbonjour.cpp
    inc.files += torcbonjour.h
    !macx: LIBS += -ldns_sd
}

# Allow both #include <blah.h> and #include <libxx/blah.h>
inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += PREFIX=\\\"$${PREFIX}\\\"
DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES += LIBDIRNAME=\\\"$${LIBDIRNAME}\\\"
DEFINES += TORC_CORE_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS
