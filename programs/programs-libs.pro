INCLUDEPATH += ../.. ../../libs/
INCLUDEPATH += ../../libs/libtorc-core

LIBS += -L../../libs/libtorc-core
LIBS += -ltorc-core-$$LIBVERSION

mingw {
    CONFIG += console
}

LIBS += $$EXTRA_LIBS $$LATE_LIBS
