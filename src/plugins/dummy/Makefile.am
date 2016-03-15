MAINTAINERCLEANFILES	= Makefile.in

AM_CPPFLAGS		= -I$(top_srcdir)/include

EXTRA_LTLIBRARIES	= libintf_dummy.la
noinst_LTLIBRARIES	= @INTF_DUMMY_LIB@
libintf_dummy_la_LIBADD	= $(top_builddir)/lib/libipmitool.la
libintf_dummy_la_SOURCES	= dummy.c dummy.h
