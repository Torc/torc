include (../settings.pro)

!exists ("../libs/libtorc-core/version.h") {
    warning("No version.h file. Be sure to run version.bat from the command line.")
}

TEMPLATE = subdirs

SUBDIRS += ../libs ../programs
