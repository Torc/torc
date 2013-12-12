include ( ../../settings.pro )

THIS_LIB = $$PROJECTNAME-video

TEMPLATE = lib
TARGET = $$THIS_LIB-$$LIBVERSION
CONFIG += thread dll
target.path = $${LIBDIR}
INSTALLS = target

QT += xml
QT -= gui

DEPENDPATH  += ../libtorc-core
DEPENDPATH  += ../libtorc-av
DEPENDPATH  += ../libtorc-audio
DEPENDPATH  += ./platforms
INCLUDEPATH += ../.. ../
INCLUDEPATH += $$DEPENDPATH

LIBS += -L../libtorc-core           -ltorc-core-$$LIBVERSION
LIBS += -L../libtorc-audio          -ltorc-audio-$$LIBVERSION
LIBS += -L../libtorc-av/libavformat -ltorc-avformat
LIBS += -L../libtorc-av/libavcodec  -ltorc-avcodec
LIBS += -L../libtorc-av/libavutil   -ltorc-avutil
LIBS += -L../libtorc-av/libswscale  -ltorc-swscale

QMAKE_CLEAN += $(TARGET) $(TARGETA) $(TARGETD) $(TARGET0) $(TARGET1) $(TARGET2)

HEADERS += torcvideoexport.h
HEADERS += videoplayer.h
HEADERS += videodecoder.h
HEADERS += videoframe.h
HEADERS += videobuffers.h
HEADERS += videocolourspace.h
HEADERS += torcvideooverlay.h
HEADERS += torcbluraybuffer.h
HEADERS += torcblurayhandler.h
HEADERS += torcbluraymetadata.h

SOURCES += videoplayer.cpp
SOURCES += videodecoder.cpp
SOURCES += videoframe.cpp
SOURCES += videobuffers.cpp
SOURCES += videocolourspace.cpp
SOURCES += torcvideooverlay.cpp
SOURCES += torcbluraybuffer.cpp
SOURCES += torcblurayhandler.cpp
SOURCES += torcbluraymetadata.cpp

#libbluray
DEFINES     += HAVE_CONFIG_H DLOPEN_CRYPTO_LIBS HAVE_PTHREAD_H HAVE_DIRENT_H HAVE_STRINGS_H
DEPENDPATH  += ./libbluray/src
INCLUDEPATH += ./libbluray/src

HEADERS += libbluray/src/file/dl.h
HEADERS += libbluray/src/file/file.h
HEADERS += libbluray/src/file/filesystem.h
HEADERS += libbluray/src/file/libaacs.h
HEADERS += libbluray/src/file/libbdplus.h
HEADERS += libbluray/src/libbluray/bluray_internal.h
HEADERS += libbluray/src/libbluray/bluray-version.h
HEADERS += libbluray/src/libbluray/bluray.h
HEADERS += libbluray/src/libbluray/keys.h
HEADERS += libbluray/src/libbluray/register.h
HEADERS += libbluray/src/libbluray/bdnav/bdid_parse.h
HEADERS += libbluray/src/libbluray/bdnav/bdparse.h
HEADERS += libbluray/src/libbluray/bdnav/clpi_data.h
HEADERS += libbluray/src/libbluray/bdnav/clpi_parse.h
HEADERS += libbluray/src/libbluray/bdnav/extdata_parse.h
HEADERS += libbluray/src/libbluray/bdnav/index_parse.h
HEADERS += libbluray/src/libbluray/bdnav/meta_data.h
HEADERS += libbluray/src/libbluray/bdnav/meta_parse.h
HEADERS += libbluray/src/libbluray/bdnav/mpls_parse.h
HEADERS += libbluray/src/libbluray/bdnav/navigation.h
HEADERS += libbluray/src/libbluray/bdnav/sound_parse.h
HEADERS += libbluray/src/libbluray/bdnav/uo_mask_table.h
HEADERS += libbluray/src/libbluray/decoders/graphics_controller.h
HEADERS += libbluray/src/libbluray/decoders/graphics_processor.h
HEADERS += libbluray/src/libbluray/decoders/ig_decode.h
HEADERS += libbluray/src/libbluray/decoders/ig.h
HEADERS += libbluray/src/libbluray/decoders/m2ts_demux.h
HEADERS += libbluray/src/libbluray/decoders/overlay.h
HEADERS += libbluray/src/libbluray/decoders/pes_buffer.h
HEADERS += libbluray/src/libbluray/decoders/pg_decode.h
HEADERS += libbluray/src/libbluray/decoders/pg.h
HEADERS += libbluray/src/libbluray/decoders/rle.h
HEADERS += libbluray/src/libbluray/decoders/textst_decode.h
HEADERS += libbluray/src/libbluray/decoders/textst_render.h
HEADERS += libbluray/src/libbluray/decoders/textst.h
HEADERS += libbluray/src/libbluray/hdmv/hdmv_insn.h
HEADERS += libbluray/src/libbluray/hdmv/hdmv_vm.h
HEADERS += libbluray/src/libbluray/hdmv/mobj_parse.h
HEADERS += libbluray/src/util/attributes.h
HEADERS += libbluray/src/util/bits.h
HEADERS += libbluray/src/util/logging.h
HEADERS += libbluray/src/util/log_control.h
HEADERS += libbluray/src/util/macro.h
HEADERS += libbluray/src/util/mutex.h
HEADERS += libbluray/src/util/refcnt.h
HEADERS += libbluray/src/util/strutl.h

SOURCES += libbluray/src/file/dl_posix.c
SOURCES += libbluray/src/file/file_posix.c
SOURCES += libbluray/src/file/dir_posix.c
SOURCES += libbluray/src/file/filesystem.c
SOURCES += libbluray/src/file/libaacs.c
SOURCES += libbluray/src/file/libbdplus.c
SOURCES += libbluray/src/libbluray/bluray.c
SOURCES += libbluray/src/libbluray/register.c
SOURCES += libbluray/src/libbluray/bdnav/bdid_parse.c
SOURCES += libbluray/src/libbluray/bdnav/clpi_parse.c
SOURCES += libbluray/src/libbluray/bdnav/extdata_parse.c
SOURCES += libbluray/src/libbluray/bdnav/index_parse.c
SOURCES += libbluray/src/libbluray/bdnav/meta_parse.c
SOURCES += libbluray/src/libbluray/bdnav/mpls_parse.c
SOURCES += libbluray/src/libbluray/bdnav/navigation.c
SOURCES += libbluray/src/libbluray/bdnav/sound_parse.c
SOURCES += libbluray/src/libbluray/decoders/graphics_controller.c
SOURCES += libbluray/src/libbluray/decoders/graphics_processor.c
SOURCES += libbluray/src/libbluray/decoders/ig_decode.c
SOURCES += libbluray/src/libbluray/decoders/m2ts_demux.c
SOURCES += libbluray/src/libbluray/decoders/pes_buffer.c
SOURCES += libbluray/src/libbluray/decoders/pg_decode.c
SOURCES += libbluray/src/libbluray/decoders/textst_decode.c
SOURCES += libbluray/src/libbluray/decoders/textst_render.c
SOURCES += libbluray/src/libbluray/hdmv/hdmv_vm.c
SOURCES += libbluray/src/libbluray/hdmv/mobj_parse.c
SOURCES += libbluray/src/libbluray/hdmv/mobj_print.c
SOURCES += libbluray/src/util/bits.c
SOURCES += libbluray/src/util/logging.c
SOURCES += libbluray/src/util/refcnt.c
SOURCES += libbluray/src/util/strutl.c

#uncomment for comparative testing of libbluray v internal metadata parsing
#INCLUDEPATH += /usr/include/libxml2
#DEFINES     += HAVE_LIBXML2
#LIBS        += -lxml2

#bd-j support needs libfreetype
contains(CONFIG_LIBFREETYPE, yes):contains(CONFIG_BLURAYJAVA, yes) {
    DEFINES += HAVE_FT2
    HEADERS += libbluray/src/libbluray/bdj/bdj.h
    HEADERS += libbluray/src/libbluray/bdj/bdjo_parser.h
    HEADERS += libbluray/src/libbluray/bdj/bdj_private.h
    HEADERS += libbluray/src/libbluray/bdj/bdj_util.h
    HEADERS += libbluray/src/libbluray/bdj/common.h
    HEADERS += libbluray/src/libbluray/bdj/native/java_awt_BDFontMetrics.h
    HEADERS += libbluray/src/libbluray/bdj/native/java_awt_BDGraphics.h
    HEADERS += libbluray/src/libbluray/bdj/native/org_videolan_Libbluray.h
    HEADERS += libbluray/src/libbluray/bdj/native/org_videolan_Logger.h
    HEADERS += libbluray/src/libbluray/bdj/native/register_native.h

    SOURCES += libbluray/src/libbluray/bdj/bdj.c
    SOURCES += libbluray/src/libbluray/bdj/bdjo_parser.c
    SOURCES += libbluray/src/libbluray/bdj/bdj_util.c
    SOURCES += libbluray/src/libbluray/bdj/native/java_awt_BDFontMetrics.c
    SOURCES += libbluray/src/libbluray/bdj/native/java_awt_BDGraphics.c
    SOURCES += libbluray/src/libbluray/bdj/native/org_videolan_Libbluray.c
    SOURCES += libbluray/src/libbluray/bdj/native/org_videolan_Logger.c
    SOURCES += libbluray/src/libbluray/bdj/native/register_native.c

    DEFINES += JAVA_ARCH=\\\"$$QT_ARCH\\\"
    DEFINES += USING_BDJAVA=1
    DEFINES += BDJ_BOOTCLASSPATH=\\\"\\\"

    macx {
        #TODO these paths need to be set properly
        ANTBIN          = /usr/bin/ant
        DEFINES        += JDK_HOME=\\\"System/Library/Java/JavaVirtualMachines/1.6.0.jdk/Contents/Home\\\"
        INCLUDEPATH    += /System/Library/Frameworks/JavaVM.framework/Headers
        QMAKE_CXXFLAGS += -F/System/Library/Frameworks/JavaVM.framework/Frameworks
        LIBS           += -framework JavaVM
        SOURCES        += libbluray/src/file/dirs_darwin.c
    }

    unix {
        ANTBIN          = /usr/bin/ant
        DEFINES        += JDK_HOME=\\\"/usr/lib/jvm/default-java\\\"
        INCLUDEPATH    += /usr/lib/jvm/java-7-openjdk-i386/include
        SOURCES        += libbluray/src/file/dirs_xdg.c
    }

    QMAKE_POST_LINK  = $${ANTBIN} -v -f libbluray/src/libbluray/bdj/build.xml -Dsrc_awt=:java-j2se; $${ANTBIN} -f libbluray/src/libbluray/bdj/build.xml clean
    installjar.path  = $${PREFIX}/share/$${PROJECTNAME}/jars
    installjar.files = libbluray/src/.libs/libbluray.jar
    DEFINES         += TORC_LIBBLURAY_JAR_LOCATION=\\\"$${PREFIX}/share/$${PROJECTNAME}/jars/libbluray.jar\\\"
    INSTALLS        += installjar
    QMAKE_CLEAN     += libbluray.jar
}

contains(CONFIG_VDA, yes) {
    HEADERS += platforms/videovda.h
    SOURCES += platforms/videovda.cpp
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/CoreVideo.framework/Frameworks
    QMAKE_CXXFLAGS += -F/System/Library/Frameworks/CoreFoundation.framework/Frameworks
    LIBS           += -framework CoreVideo -framework CoreFoundation
}

win32-msvc* {
    # your guess is as good as mine...
    CONFIG += staticlib
}

inc.path   = $${PREFIX}/include/$${PROJECTNAME}/
inc.files += videoplayer.h    videodecoder.h
inc.files += videoframe.h     videobuffers.h

inc2.path  = $${PREFIX}/include/$${PROJECTNAME}/lib$${THIS_LIB}
inc2.files = $${inc.files}

DEFINES += TORC_VIDEO_API

use_hidesyms {
    QMAKE_CXXFLAGS += -fvisibility=hidden
}

INSTALLS += inc inc2

include ( ../libs-targetfix.pro )

LIBS += $$EXTRA_LIBS $$LATE_LIBS

