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

plugindir			= $(pluginpath)

INCLUDES			= -I$(top_srcdir)/include $(INCLTDL)
MAINTAINERCLEANFILES		= Makefile.in ipmi_intf_static.c
EXTRA_DIST			= ipmi_intf_static.c.in

SUBDIRS				= @INTF_LAN@ @INTF_OPEN@
DIST_SUBDIRS			= lan open

noinst_LTLIBRARIES		= libintf.la
libintf_la_SOURCES		= ipmi_intf.c ipmi_intf_static.c
libintf_la_LDFLAGS		= -export-dynamic
libintf_la_LIBADD		= $(LIBLTDL) @STATIC_INTF_LIB@
libintf_la_DEPENDENCIES		= $(LIBLTDL) @STATIC_INTF_LIB@

