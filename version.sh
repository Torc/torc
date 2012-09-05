#!/bin/sh
#
# small shell script to generate version.cpp
# it expects one parameter
# first parameter is the root of the source directory

if test $# -ne 1; then
    echo "Usage: version.sh GIT_TREE_DIR"
    exit 1
fi

TESTFN=`mktemp $1/.test-write-XXXXXX` 2> /dev/null
if test x$TESTFN != x"" ; then
    rm -f $TESTFN
else
    echo "$0: Can not write to destination, skipping.."
    exit 0
fi

GITTREEDIR=$1
GITREPOPATH="exported"

cd ${GITTREEDIR}

SOURCE_VERSION=$(git describe --dirty || git describe || echo Unknown)

case "${SOURCE_VERSION}" in
    exported|Unknown)
        if test -e $GITTREEDIR/VERSION ; then
            . $GITTREEDIR/VERSION
        fi
    ;;
    *)
    BRANCH=$(git branch --no-color | sed -e '/^[^\*]/d' -e 's/^\* //' -e 's/(no branch)/exported/')
    ;;
esac

cat > .vers.new <<EOF
#ifndef TORC_SOURCE_VERSION
#define TORC_SOURCE_VERSION "${SOURCE_VERSION}"
#define TORC_SOURCE_PATH    "${BRANCH}"
#endif
EOF

# check if the version strings are changed and update version.pro if necessary
if ! cmp -s .vers.new libs/libtorc-core/version.h; then
   mv -f .vers.new libs/libtorc-core/version.h
fi
rm -f .vers.new
