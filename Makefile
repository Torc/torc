include config.mak


MAKE_SUBDIRS = libs/libtorc-av
QT_SUBDIRS = libs programs

SUBDIRS += $(MAKE_SUBDIRS) $(QT_SUBDIRS)

.PHONY: subdirs $(SUBDIRS)                            \
	clean     $(addsuffix _clean,    $(SUBDIRS))  \
	distclean $(addsuffix _distclean,$(SUBDIRS))  \
	install   $(addsuffix _install,  $(SUBDIRS))  \
	uninstall $(addsuffix _uninstall,$(SUBDIRS))  \
	libs/libtorc-core/version.h force

all: libs/libtorc-core/version.h subdirs
force:

config.mak:
	$(error run configure to create $@)

# Override PWD in case this is run from a makefile at a higher level with
# make -C torc
PWD := $(shell pwd)

libs/libtorc-core/version.h:	version.sh force
	sh version.sh "$(PWD)"

# explicit subdir dependencies
programs: libs

subdirs: $(SUBDIRS)

settings.pro: config.mak
%.pro: settings.pro

# explicit prerequisites for qmake generated makefiles
libs/Makefile: libs/libs.pro
html/Makefile: html/html.pro
programs/Makefile: programs/programs.pro
i18n/Makefile: i18n/i18n.pro
locales/Makefile: locales/locales.pro

$(addsuffix /Makefile,$(QT_SUBDIRS)): %/Makefile :
	cd $*; $(QMAKE) QMAKE=$(QMAKE) -o $(@F) $(<F)

$(SUBDIRS): $(addsuffix /Makefile,$(SUBDIRS)) libs/libtorc-core/version.h
	$(MAKE) -C $@

$(addsuffix _clean,$(SUBDIRS)): $(addsuffix /Makefile,$(SUBDIRS))
	$(MAKE) -C $(subst _clean,,$@) clean

$(addsuffix _distclean,$(SUBDIRS)): $(addsuffix /Makefile,$(SUBDIRS))
	$(MAKE) -C $(subst _distclean,,$@) distclean

$(addsuffix _install,$(SUBDIRS)): $(addsuffix /Makefile,$(SUBDIRS))
	$(MAKE) -C $(subst _install,,$@) install INSTALL_ROOT=${INSTALL_ROOT}

$(addsuffix _uninstall,$(SUBDIRS)): $(addsuffix /Makefile,$(SUBDIRS))
	$(MAKE) -C $(subst _uninstall,,$@) uninstall

clean: $(addsuffix _clean,$(SUBDIRS))

distclean: $(addsuffix _distclean,$(SUBDIRS))
	-rm -f libs/libtorc-core/torcconfig.mak config.mak
	-rm -f libs/libtorc-core/torcconfig.h   config.h
	-rm -f libs/libtorc-core/version.h
	-rm -f libs/libavutil/avconfig.h
	-rm -f libs/libtorc-av/libavutil/avconfig.h
	-rm -f $(addsuffix /Makefile,$(QT_SUBDIRS))

install: $(addsuffix _install,$(SUBDIRS))

uninstall: $(addsuffix _uninstall,$(SUBDIRS))
	-rmdir $(INSTALL_ROOT)/${PREFIX}/include/torc
	-rmdir $(INSTALL_ROOT)/${PREFIX}/lib/torc
	-rmdir $(INSTALL_ROOT)/${PREFIX}/share/torc
