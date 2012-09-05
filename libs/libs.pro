include (../settings.pro)

TEMPLATE = subdirs

SUBDIRS += libtorc-core libtorc-media libtorc-baseui libtorc-tenfootui

libtorc-baseui.depends = libtorc-core
libtorc-media.depends = libtorc-core
libtorc-tenfootui.depends = libtorc-baseui
