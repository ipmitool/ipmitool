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
# 
# You acknowledge that this software is not designed or intended for use
# in the design, construction, operation or maintenance of any nuclear
# facility.

EXTRA_DIST = ipmitool.spec.in AUTHORS COPYING ChangeLog \
	debian/changelog debian/control debian/copyright \
	debian/dirs debian/docs debian/rules

AUTOMAKE_OPTIONS = dist-bzip2

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 configure configure-stamp \
	config.guess config.sub depcomp install-sh ltmain.sh missing \
	mkinstalldirs config.h.in stamp-h.in \
	$(distdir).tar.gz $(distdir).tar.bz2

SUBDIRS = libltdl lib src include doc contrib control

dist-hook: ipmitool.spec
	cp ipmitool.spec $(distdir)

pkg: PKG := @IPMITOOL_BIN@-@VERSION@-@OS@-@ARCH@
pkg: control/pkginfo
	$(STRIP) src/@IPMITOOL_BIN@
# this needs done twice to workaround bug in gnu binutils
	$(STRIP) src/@IPMITOOL_BIN@
	(cd control ; pkgmk -o -d /tmp)
	pkgtrans -s /tmp $(PKG) @IPMITOOL_BIN@
	gzip -c /tmp/$(PKG) > $(PKG).gz
	-rm -rf /tmp/@IPMITOOL_BIN@ /tmp/$(PKG)
