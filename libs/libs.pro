include (../settings.pro)

TEMPLATE = subdirs

SUBDIRS += libtorc-core libtorc-audio libtorc-media libtorc-baseui libtorc-tenfootui

libtorc-audio.depends      = libtorc-core
libtorc-baseui.depends     = libtorc-core
libtorc-media.depends      = libtorc-core
libtorc-tenfootui.depends  = libtorc-baseui
