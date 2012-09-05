include ( ../settings.pro )

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

QMAKE_COPY_DIR = sh ./cpsvndir
win32:QMAKE_COPY_DIR = sh ./cpsimple

tenfootthemes.path = $${PREFIX}/share/torc/tenfootthemes/
tenfootthemes.files = default

INSTALLS += tenfootthemes

# Input
SOURCES += dummy.c
