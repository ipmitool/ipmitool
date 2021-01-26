# Copyright (c) 2003 Sun Microsystems, Inc.  All Rights Reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
# Redistribution of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
# Redistribution in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# 
# Neither the name of Sun Microsystems, Inc. or the names of
# contributors may be used to endorse or promote products derived
# from this software without specific prior written permission.
# 
# This software is provided "AS IS," without a warranty of any kind.
# ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
# INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
# PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
# SUN MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE
# FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
# OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
# SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
# OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
# PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
# LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
# EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

DOCDIR = $(datadir)/doc/$(PACKAGE)
DOCLIST = $(top_srcdir)/README $(top_srcdir)/COPYING $(top_srcdir)/AUTHORS $(top_srcdir)/ChangeLog

EXTRA_DIST = $(DOCLIST) bootstrap

AUTOMAKE_OPTIONS = dist-bzip2

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure configure-stamp \
	config.guess config.sub depcomp install-sh ltmain.sh missing \
	mkinstalldirs config.h.in stamp-h.in \
	$(distdir).tar.gz $(distdir).tar.bz2

SUBDIRS = lib src include doc contrib control
IANA_PEN = http://www.iana.org/assignments/enterprise-numbers

dist-hook:
	cp control/ipmitool.spec $(distdir)

.PHONY: install-pen-database uninstall-pen-database
.INTERMEDIATE: %.o %.la enterprise-numbers

if DOWNLOAD

enterprise-numbers:
	@echo Downloading IANA PEN database...
	@$(DOWNLOAD) "$(IANA_PEN)" > tmpfile.$$PPID || {\
		echo "FAILED to download the IANA PEN database"; \
		rm tmpfile.$$PPID; \
		false; \
	}
	@mv tmpfile.$$PPID $@

install-pen-database: enterprise-numbers
	mkdir -m 755 -p $(DESTDIR)$(IANADIR)
	$(INSTALL_DATA) $< $(DESTDIR)$(IANADIR)/

uninstall-pen-database:
	-rm -rf $(DESTDIR)$(IANADIR)/enterprise-numbers

else

install-pen-database:
	@echo "*** NOT installing the IANA PEN database."
	@echo "*** Don't know how to download it."

uninstall-pen-database:
	@echo "*** NOT uninstalling the IANA PEN database."
	@echo "*** It was installed manually (if ever)."

endif

install-data-local: install-pen-database
	mkdir -p $(DESTDIR)$(DOCDIR)
	$(INSTALL_DATA) $(DOCLIST) $(DESTDIR)$(DOCDIR)

uninstall-local: uninstall-pen-database
	-rm -rf $(DESTDIR)$(DOCDIR)

.PHONY: pkg
pkg: PKG := ipmitool-@VERSION@-@OS@-@ARCH@
pkg: control/pkginfo
	$(STRIP) src/ipmitool
	$(STRIP) src/ipmitool
	(cd control ; pkgmk -o -d /tmp)
	pkgtrans -s /tmp $(PKG) ipmitool
	gzip -c /tmp/$(PKG) > $(PKG).gz
	-rm -rf /tmp/ipmitool /tmp/$(PKG)

BUILDDIR = ${shell cd $(top_builddir) ; pwd | sed -e 's,^[^:\\/]:[\\/],/,'}
SRCDIR   = ${shell cd $(top_srcdir) ; pwd | sed -e 's,^[^:\\/]:[\\/],/,'}
RPMDIR   = $(BUILDDIR)/rpmbuild

.PHONY: rpm
rpm: dist
	@RPMBUILD@ -ta --define "_topdir $(RPMDIR)" $(distdir).tar.gz

.PHONY: clean-rpm
clean-rpm:
	-rm -rf $(RPMDIR)
