MAINTAINERCLEANFILES           = Makefile.in

INCLUDES                       = -I$(top_srcdir)/include

EXTRA_LTLIBRARIES              = libintf_free.la
noinst_LTLIBRARIES             = @INTF_FREE_LIB@
libintf_free_la_LIBADD         = $(top_builddir)/lib/libipmitool.la
libintf_free_la_SOURCES        = free.c
libintf_free_la_LDFLAGS        = -lfreeipmi
