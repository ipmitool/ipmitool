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

AUTOMAKE_OPTIONS		= subdir-objects
AM_CPPFLAGS			= -I$(top_srcdir)/include
MAINTAINERCLEANFILES		= Makefile.in

noinst_LTLIBRARIES		= libipmitool.la
libipmitool_la_SOURCES	= helper.c ipmi_sdr.c ipmi_sel.c ipmi_sol.c ipmi_pef.c \
				  ipmi_lanp.c ipmi_fru.c ipmi_chassis.c ipmi_mc.c log.c            \
				  dimm_spd.c ipmi_sensor.c ipmi_channel.c ipmi_event.c             \
				  ipmi_session.c ipmi_strings.c ipmi_user.c ipmi_raw.c             \
				  ipmi_oem.c ipmi_isol.c ipmi_sunoem.c ipmi_fwum.c ipmi_picmg.c    \
				  ipmi_main.c ipmi_tsol.c ipmi_firewall.c ipmi_kontronoem.c        \
				  ipmi_hpmfwupg.c ipmi_sdradd.c ipmi_ekanalyzer.c ipmi_gendev.c    \
				  ipmi_ime.c ipmi_delloem.c ipmi_dcmi.c hpm2.c ipmi_vita.c \
				  ipmi_lanp6.c ipmi_cfgp.c ipmi_quantaoem.c ipmi_time.c

libipmitool_la_LDFLAGS		= -export-dynamic
libipmitool_la_LIBADD		= -lm
libipmitool_la_DEPENDENCIES	= 

