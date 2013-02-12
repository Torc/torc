include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-baseui

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT += xml script

DEPENDPATH  += ./opengl ./platforms ./peripherals ./peripherals/cec ./direct3d

INCLUDEPATH += ../libtorc-core ../libtorc-core/platforms
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../libtorc-core -ltorc-core-$$LIBVERSION

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += torcbaseuiexport.h
HEADERS += uiimagetracker.h
HEADERS += uiimage.h
HEADERS += uifont.h
HEADERS += uitextrenderer.h
HEADERS += uieffect.h
HEADERS += uiwindow.h
HEADERS += uiimageloader.h
HEADERS += uiwidget.h
HEADERS += uidisplay.h
HEADERS += uidisplaybase.h
HEADERS += uiperformance.h
HEADERS += uitheme.h
HEADERS += uithemeloader.h
HEADERS += uishapepath.h
HEADERS += uitimer.h
HEADERS += uianimation.h
HEADERS += uitext.h
HEADERS += uishape.h
HEADERS += uiimagewidget.h
HEADERS += uigroup.h
HEADERS += uibutton.h
HEADERS += uiactions.h
HEADERS += uishaperenderer.h
HEADERS += uimedia.h
HEADERS += uimessenger.h
HEADERS += uitexteditor.h
HEADERS += uiedid.h

SOURCES += uiimage.cpp
SOURCES += uiimagetracker.cpp
SOURCES += uifont.cpp
SOURCES += uitextrenderer.cpp
SOURCES += uieffect.cpp
SOURCES += uiwindow.cpp
SOURCES += uimedia.cpp
SOURCES += uiimageloader.cpp
SOURCES += uiwidget.cpp
SOURCES += uidisplaybase.cpp
SOURCES += uiperformance.cpp
SOURCES += uitheme.cpp
SOURCES += uithemeloader.cpp
SOURCES += uishapepath.cpp
SOURCES += uitimer.cpp
SOURCES += uianimation.cpp
SOURCES += uitext.cpp
SOURCES += uishape.cpp
SOURCES += uiimagewidget.cpp
SOURCES += uigroup.cpp
SOURCES += uibutton.cpp
SOURCES += uiactions.cpp
SOURCES += uishaperenderer.cpp
SOURCES += uimessenger.cpp
SOURCES += uitexteditor.cpp
SOURCES += uiedid.cpp

#libCEC
HEADERS += peripherals/cec/cec.h
HEADERS += peripherals/cec/cectypes.h
HEADERS += peripherals/torccecdevice.h
SOURCES += peripherals/torccecdevice.cpp

!win32 {
    QT += opengl
    HEADERS += opengl/uiopenglwindow.h
    HEADERS += opengl/uiopenglshaders.h
    HEADERS += opengl/uiopenglfence.h
    HEADERS += opengl/uiopenglmatrix.h
    HEADERS += opengl/uiopengltextures.h
    HEADERS += opengl/uiopenglview.h
    HEADERS += opengl/uiopenglbufferobjects.h
    HEADERS += opengl/uiopenglframebuffers.h

    SOURCES += opengl/uiopenglshaders.cpp
    SOURCES += opengl/uiopenglfence.cpp
    SOURCES += opengl/uiopenglwindow.cpp
    SOURCES += opengl/uiopenglmatrix.cpp
    SOURCES += opengl/uiopengltextures.cpp
    SOURCES += opengl/uiopenglview.cpp
    SOURCES += opengl/uiopenglbufferobjects.cpp
    SOURCES += opengl/uiopenglframebuffers.cpp
}

win32|unix {
    DEPENDPATH += ./platforms/adl
    HEADERS    += platforms/adl/adl_defines.h
    HEADERS    += platforms/adl/adl_sdk.h
    HEADERS    += platforms/adl/adl_structures.h
    HEADERS    += platforms/adl/uiadl.h
    SOURCES    += platforms/adl/uiadl.cpp
}

contains(CONFIG_X11BASE, yes) {
    DEPENDPATH += ./platforms/nvctrl
    HEADERS    += platforms/nvctrl/NVCtrl.h
    HEADERS    += platforms/nvctrl/NVCtrlLib.h
    HEADERS    += platforms/nvctrl/nv_control.h
    HEADERS    += platforms/nvctrl/uinvcontrol.h
    SOURCES    += platforms/nvctrl/NVCtrl.c
    SOURCES    += platforms/nvctrl/uinvcontrol.cpp
    SOURCES    += platforms/uidisplay-x11.cpp
    LIBS       += -lXxf86vm
}
else:macx {
    LIBS    += -framework Cocoa
    SOURCES += platforms/uidisplay-osx.cpp
}
else:win32 {
    SOURCES += platforms/uidisplay-win.cpp

    HEADERS += direct3d/_mingw_unicode.h
    HEADERS += direct3d/d3dx9.h
    HEADERS += direct3d/d3dx9core.h
    HEADERS += direct3d/d3dx9math.h
    HEADERS += direct3d/d3dx9math.inl
    HEADERS += direct3d/d3dx9shader.h
    HEADERS += direct3d/d3dx9tex.h
    HEADERS += direct3d/uidirect3d9window.h
    HEADERS += direct3d/uidirect3d9view.h
    HEADERS += direct3d/uidirect3d9textures.h
    HEADERS += direct3d/uidirect3d9shaders.h
    SOURCES += direct3d/uidirect3d9window.cpp
    SOURCES += direct3d/uidirect3d9view.cpp
    SOURCES += direct3d/uidirect3d9textures.cpp
    SOURCES += direct3d/uidirect3d9shaders.cpp

    LIBS += -lgdi32 -lsetupapi
}
else:greaterThan(QT_MAJOR_VERSION, 4) {
    SOURCES += platforms/uidisplay-qt5.cpp
}
else: {
    error(No valid display class. Aborting)
}

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += uiimage.h        uifont.h
inc.files += uieffect.h       uiwindow.h
inc.files += uiwidget.h       uitheme.h
inc.files += uitext.h         uimedia.h
inc.files += uishape.h        uiimagewidget.h
inc.files += uigroup.h        uibutton.h
inc.files += uidisplay.h      uidisplaybase.h
inc.files += uitexteditor.h
inc.files += uiedid.h

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

