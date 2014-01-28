include (../settings.pro)

TEMPLATE = app
CONFIG -= moc qt

translations.path   = $${PREFIX}/share/torc/i18n/
translations.files  = torc_en_GB.qm
translations.files += torc_fr.qm

SOURCES += dummy.c

INSTALLS += translations 
