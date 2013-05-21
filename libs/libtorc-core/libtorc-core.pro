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

HEADERS += torccoreexport.h
HEADERS += torclogging.h
HEADERS += torcloggingdefs.h
HEADERS += torcthread.h
HEADERS += torcloggingimp.h
HEADERS += torcplist.h
HEADERS += torccompat.h
HEADERS += torcexitcodes.h
HEADERS += torctimer.h
HEADERS += torclocalcontext.h
HEADERS += torcsqlitedb.h
HEADERS += torcdb.h
HEADERS += torcdirectories.h
HEADERS += torcreferencecounted.h
HEADERS += torccoreutils.h
HEADERS += torccommandlineparser.h
HEADERS += torcevent.h
HEADERS += torcobservable.h
HEADERS += torclocaldefs.h
HEADERS += torcpower.h
HEADERS += torcadminthread.h
HEADERS += torcstoragedevice.h
HEADERS += torcstorage.h
HEADERS += torcusb.h
HEADERS += torcbuffer.h
HEADERS += torcfilebuffer.h
HEADERS += torclanguage.h
HEADERS += torcplayer.h
HEADERS += torcdecoder.h
HEADERS += torcnetwork.h
HEADERS += torcnetworkbuffer.h
HEADERS += torcnetworkrequest.h
HEADERS += torcsetting.h
HEADERS += http/torchttprequest.h
HEADERS += http/torchttpservice.h
HEADERS += http/torchttpserver.h
HEADERS += http/torchtmlhandler.h
HEADERS += http/torchttphandler.h
HEADERS += http/torchttpconnection.h
HEADERS += http/torcserialiser.h
HEADERS += http/torcxmlserialiser.h
HEADERS += http/torcjsonserialiser.h
HEADERS += http/torcplistserialiser.h
HEADERS += http/torcbinaryplistserialiser.h

SOURCES += torcloggingimp.cpp
SOURCES += torcplist.cpp
SOURCES += torcthread.cpp
SOURCES += torclocalcontext.cpp
SOURCES += torctimer.cpp
SOURCES += torcsqlitedb.cpp
SOURCES += torcdb.cpp
SOURCES += torcdirectories.cpp
SOURCES += torccoreutils.cpp
SOURCES += torcreferencecounted.cpp
SOURCES += torccommandlineparser.cpp
SOURCES += torcevent.cpp
SOURCES += torcobservable.cpp
SOURCES += torcpower.cpp
SOURCES += torcadminthread.cpp
SOURCES += torcstoragedevice.cpp
SOURCES += torcstorage.cpp
SOURCES += torcusb.cpp
SOURCES += torcbuffer.cpp
SOURCES += torcfilebuffer.cpp
SOURCES += torclanguage.cpp
SOURCES += torcplayer.cpp
SOURCES += torcdecoder.cpp
SOURCES += torcnetwork.cpp
SOURCES += torcnetworkbuffer.cpp
SOURCES += torcnetworkrequest.cpp
SOURCES += torcsetting.cpp
SOURCES += http/torchttprequest.cpp
SOURCES += http/torchttpserver.cpp
SOURCES += http/torchtmlhandler.cpp
SOURCES += http/torchttphandler.cpp
SOURCES += http/torchttpconnection.cpp
SOURCES += http/torchttpservice.cpp
SOURCES += http/torcserialiser.cpp
SOURCES += http/torcxmlserialiser.cpp
SOURCES += http/torcjsonserialiser.cpp
SOURCES += http/torcplistserialiser.cpp
SOURCES += http/torcbinaryplistserialiser.cpp

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files  = torclogging.h
inc.files += torclocalcontext.h
inc.files += torcthread.h
inc.files += torccompat.h
inc.files += torctimer.h
inc.files += torcplist.h
inc.files += torcdirectories.h
inc.files += torcreferencecounted.h
inc.files += torccoreutils.h
inc.files += torccommandlineparser.h
inc.files += torcevent.h
inc.files += torcobservable.h
inc.files += torclocaldefs.h
inc.files += torcpower.h
inc.files += torcusb.h
inc.files += torcbuffer.h
inc.files += torcplayer.h
inc.files += torcdecoder.h
inc.files += torcnetwork.h
inc.files += torcnetworkrequest.h
inc.files += torcsetting.h
inc.files += torchttpserver.h
inc.files += torchtmlhandler.h
inc.files += torchttpservice.h

unix:contains(CONFIG_LIBUDEV, yes) {
    HEADERS += platforms/torcusbprivunix.h
    SOURCES += platforms/torcusbprivunix.cpp
    LIBS    += -ludev
}

unix:contains(CONFIG_QTDBUS, yes) {
    QT += dbus
    HEADERS += platforms/torcpowerunixdbus.h
    HEADERS += platforms/torcstorageunixdbus.h
    SOURCES += platforms/torcpowerunixdbus.cpp
    SOURCES += platforms/torcstorageunixdbus.cpp
}

win32 {
    HEADERS += platforms/torcpowerwin.h
    HEADERS += platforms/torcusbprivwin.h
    SOURCES += platforms/torcpowerwin.cpp
    SOURCES += platforms/torcusbprivwin.cpp
    LIBS += -lPowrProf -lsetupapi
}

macx {
    QMAKE_OBJECTIVE_CXXFLAGS += $$QMAKE_CXXFLAGS
    LIBS += -framework Cocoa -framework IOkit -framework DiskArbitration
    OBJECTIVE_HEADERS += platforms/torccocoa.h
    OBJECTIVE_SOURCES += platforms/torccocoa.mm
    HEADERS += platforms/torcosxutils.h
    HEADERS += platforms/torcpowerosx.h
    HEADERS += platforms/torcstorageosx.h
    HEADERS += platforms/torcrunlooposx.h
    HEADERS += platforms/torcusbprivosx.h
    SOURCES += platforms/torcosxutils.cpp
    SOURCES += platforms/torcpowerosx.cpp
    SOURCES += platforms/torcstorageosx.cpp
    SOURCES += platforms/torcrunlooposx.cpp
    SOURCES += platforms/torcusbprivosx.cpp
    inc.files += torccocoa.h
    inc.files += torcosxutils.h
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
