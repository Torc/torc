include ( ../settings.pro )

TEMPLATE = subdirs

# Directories
SUBDIRS += torc-client torc-audioclient torc-utils

# torc-desktop requires QtQuickControls 1.0 which is available with Qt 5.1
greaterThan(QT_MAJOR_VERSION, 4) {
   greaterThan(QT_MINOR_VERSION, 0) {
      SUBDIRS += torc-desktop
   }
}
