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

V_MAJ	= 1
V_MIN	= 4
V_REV	= 1.1
VERSION	= $(V_MAJ).$(V_MIN).$(V_REV)

PROG	= ipmitool
SPEC	= $(PROG).spec
MAN	= $(PROG).1
OBJS	= main.o ipmi_sdr.o ipmi_sel.o ipmi_fru.o ipmi_chassis.o ipmi_lanp.o

LIB	= libipmitool.a
LIB_OBJS= lib/helper.o lib/ipmi_dev.o lib/ipmi_lan.o

CC	= gcc
RM	= rm -f
AR	= ar rc
RANLIB	= ranlib
STRIP	= strip

INCLUDE	= -I include
CFLAGS	= -g -Wall -Werror -D_GNU_SOURCE -DVERSION=\"$(VERSION)\"
LDFLAGS	= -L . -lm -lipmitool

SBINDIR	= $(DESTDIR)/usr/sbin
MANDIR	= $(DESTDIR)/usr/share/man/man1

.PHONY:	all
all: $(LIB) $(PROG) man

$(PROG): $(OBJS)
	$(CC) $(INCLUDE) -o $@ $(OBJS) $(LDFLAGS)

$(LIB): $(LIB_OBJS)
	$(AR) $(LIB) $?
	$(RANLIB) $(LIB)

install: $(PROG)
	strip $(PROG)
	install -m755 -o root -g root $(PROG) $(SBINDIR)/$(PROG)

install-man: man
	gzip -c $(PROG).1 > $(MANDIR)/$(PROG).1.gz

clean:
	-$(RM) $(PROG) $(SPEC) $(MAN) $(LIB)
	-$(RM) $(OBJS)
	-$(RM) $(LIB_OBJS)

dist: clean spec
	tar -C .. -czf ../$(PROG)-$(VERSION).tar.gz $(PROG)

man:
	sed -e "s/@@VERSION@@/$(VERSION)/" $(MAN).in > $(MAN)

spec:
	sed -e "s/@@VERSION@@/$(VERSION)/" $(SPEC).in > $(SPEC)

rpm: spec
	tar -C .. -czf $(PROG).tar.gz $(PROG); \
	RPM=`which rpmbuild`; \
	if [ -z "$$RPM" ]; then RPM=rpm; fi; \
	$$RPM -ta $(PROG).tar.gz; \
	rm $(PROG).tar.gz

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -c $(@D)/$(<F) -o $(@D)/$(@F)

