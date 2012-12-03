include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-baseui

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT += opengl xml script

DEPENDPATH  += ./opengl ./platforms ./peripherals

INCLUDEPATH += ../libtorc-core ../libtorc-core/platforms
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../libtorc-core -ltorc-core-$$LIBVERSION

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += torcbaseuiexport.h uiimagetracker.h
HEADERS += uiimage.h          uifont.h
HEADERS += uitextrenderer.h   uiopenglwindow.h
HEADERS += uieffect.h         uiwindow.h
HEADERS += uiopenglshaders.h
HEADERS += uiopenglfence.h    uiopenglmatrix.h
HEADERS += uiopengltextures.h uiopenglview.h
HEADERS += uiopenglbufferobjects.h
HEADERS += uiopenglframebuffers.h
HEADERS += uiimageloader.h    uiwidget.h
HEADERS += uidisplay.h        uidisplaybase.h
HEADERS += uiperformance.h    uitheme.h
HEADERS += uithemeloader.h    uishapepath.h
HEADERS += uitimer.h          uianimation.h
HEADERS += uitext.h           uishape.h
HEADERS += uiimagewidget.h    uigroup.h
HEADERS += uibutton.h         uiactions.h
HEADERS += uishaperenderer.h  uimedia.h
HEADERS += uimessenger.h

SOURCES += uiimage.cpp        uiimagetracker.cpp
SOURCES += uifont.cpp         uitextrenderer.cpp
SOURCES += uiopenglwindow.cpp uieffect.cpp
SOURCES += uiwindow.cpp       uiopenglshaders.cpp
SOURCES += uiopenglfence.cpp  uimedia.cpp
SOURCES += uiopenglmatrix.cpp uiopengltextures.cpp
SOURCES += uiopenglview.cpp   uiopenglbufferobjects.cpp
SOURCES += uiopenglframebuffers.cpp
SOURCES += uiimageloader.cpp  uiwidget.cpp
SOURCES += uidisplaybase.cpp  uiperformance.cpp
SOURCES += uitheme.cpp        uithemeloader.cpp
SOURCES += uishapepath.cpp    uitimer.cpp
SOURCES += uianimation.cpp    uitext.cpp
SOURCES += uishape.cpp        uiimagewidget.cpp
SOURCES += uigroup.cpp        uibutton.cpp
SOURCES += uiactions.cpp      uishaperenderer.cpp
SOURCES += uimessenger.cpp

greaterThan(QT_MAJOR_VERSION, 4) {
    SOURCES += uidisplay-qt5.cpp
}
else:contains(CONFIG_X11BASE, yes) {
    DEPENDPATH += ./platforms/nvctrl
    HEADERS    += NVCtrl.h NVCtrlLib.h nv_control.h uinvcontrol.h
    SOURCES    += NVCtrl.c uinvcontrol.cpp
    SOURCES    += uidisplay-x11.cpp
    LIBS       += -lXxf86vm
}
else:macx {
    LIBS    += -framework Cocoa
    SOURCES += uidisplay-osx.cpp
}
else:win32 {
    SOURCES += uidisplay-win.cpp
}
else: {
    error(No valid display class. Aborting)
}

contains(CONFIG_LIBCEC, yes) {
    HEADERS += torccecdevice.h
    SOURCES += torccecdevice.cpp
    LIBS += -lcec
}

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += uiimage.h        uifont.h
inc.files += uieffect.h       uiwindow.h
inc.files += uiwidget.h       uitheme.h
inc.files += uitext.h         uimedia.h
inc.files += uishape.h        uiimagewidget.h
inc.files += uigroup.h        uibutton.h
inc.files += uidisplay.h      uidisplaybase.h

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += RUNPREFIX=\\\"$${RUNPREFIX}\\\"
DEFINES += LIBDIRNAME=\\\"$${LIBDIRNAME}\\\"
DEFINES += TORC_BASEUI_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

