include (../settings.pro)

TEMPLATE = subdirs

SUBDIRS += libtorc-core libtorc-audio libtorc-video
SUBDIRS += libtorc-media libtorc-baseui
SUBDIRS += libtorc-videoui
SUBDIRS += libtorc-qml

libtorc-audio.depends      += libtorc-core
libtorc-video.depends      += libtorc-audio
libtorc-videoui.depends    += libtorc-video
libtorc-videoui.depends    += libtorc-baseui
libtorc-baseui.depends     += libtorc-core
libtorc-media.depends      += libtorc-core
libtorc-qml.depends        += libtorc-core
