include (../settings.pro)

QMAKE_STRIP = echo

TEMPLATE = app
CONFIG -= moc qt

install.path   = $${PREFIX}/share/torc/html/
install.files  = index.html
install.files += css fonts img js

INSTALLS += install

SOURCES += dummy.c
