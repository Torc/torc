############################################################
# Optional qmake instructions which automatically generate #
# an extra source file containing the Subversion revision. #
#       If the directory has no .svn directories,          #
#        "exported" is reported as the revision.           #
############################################################

HEADERS += $$PWD/libs/libtorc-core/version.h
DEPENDPATH += $$PWD/libs/libtorc-core
