include (../settings.pro)

TEMPLATE = subdirs

SUBDIRS += libtorc-core libtorc-audio libtorc-video
SUBDIRS += libtorc-media libtorc-baseui libtorc-tenfootui
SUBDIRS += libtorc-videoui

libtorc-audio.depends      += libtorc-core
libtorc-video.depends      += libtorc-audio
libtorc-videoui.depends    += libtorc-video
libtorc-videoui.depends    += libtorc-baseui
libtorc-baseui.depends     += libtorc-core
libtorc-media.depends      += libtorc-core
libtorc-tenfootui.depends  += libtorc-baseui
