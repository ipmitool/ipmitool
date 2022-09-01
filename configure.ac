dnl
dnl autoconf for ipmitool
dnl
m4_define([git_suffix], m4_esyscmd_s(./csv-revision))
AC_INIT([ipmitool], [1.8.19git_suffix])
AC_CONFIG_SRCDIR([src/ipmitool.c])
AC_CONFIG_COMMANDS_PRE([export prefix=$prefix])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER(config.h)
AC_PREREQ(2.50)
AC_SUBST(ac_configure_args)

dnl check for requirements
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_CHECK_PROG([RPMBUILD], [rpmbuild], [rpmbuild], [rpm])
AC_CHECK_PROG([SED], [sed], [sed])
AC_CHECK_PROG([WGET], [wget], [wget])
AC_CHECK_PROG([CURL], [curl], [curl])

AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h sys/ioctl.h sys/stat.h unistd.h paths.h])
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h sys/socket.h])
AC_CHECK_HEADERS([sys/byteorder.h byteswap.h])

AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN

AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STRTOD
AC_CHECK_FUNCS([alarm gethostbyname getaddrinfo getifaddrs socket select])
AC_CHECK_FUNCS([memmove memset strchr strdup strerror])
AC_CHECK_FUNCS([getpassphrase])

CFLAGS="$CFLAGS -Wall -Wextra -std=gnu11 -pedantic -Wformat -Wformat-nonliteral"

AM_PROG_LIBTOOL
LIBTOOL="$LIBTOOL --silent"

AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([getaddrinfo], [nsl])
AC_SEARCH_LIBS([getifaddrs], [nsl])
AC_SEARCH_LIBS([socket], [socket], [],
	[AC_CHECK_LIB([nsl], [socket],
		[LIBS="$LIBS -lsocket -lnsl"], [], [-lsocket])])

if test "x$prefix" = "xNONE"; then
	prefix="$ac_default_prefix"
fi

if test "x$exec_prefix" = "xNONE"; then
	exec_prefix="$prefix"
fi

if test "x$WGET" = "x"; then
	if test "x$CURL" = "x"; then
		AC_MSG_WARN([** Neither wget nor curl could be found.])
		AC_MSG_WARN([** IANA PEN database will not be installed by `make install` !])
	else
		DOWNLOAD="$CURL --location --progress-bar"
		AM_CONDITIONAL([DOWNLOAD], [true])
	fi
else
	DOWNLOAD="$WGET -c -nd -O -"
	AM_CONDITIONAL([DOWNLOAD], [true])
fi

AC_MSG_WARN([** Download is:])
AC_MSG_WARN($DOWNLOAD)
AC_SUBST(DOWNLOAD, $DOWNLOAD)

dnl
dnl set default option values
dnl

xenable_all_options=yes
xenable_intf_bmc=no
xenable_intf_dbus=no
xenable_intf_dummy=no
xenable_intf_imb=yes
xenable_intf_lipmi=yes
xenable_intf_open=yes
#xenable_intf_serial=yes
xenable_intf_usb=no
xenable_ipmishell=yes

dnl set some things so we build with GNU tools on Solaris
case "$host_os" in
solaris*)
	MAKE=gmake
	STRIP=gstrip
	LD=gld
	AR=gar
	# openssl libs are in /usr/sfw/lib on solaris 10
	LIBS="$LIBS -R/usr/sfw/lib"
	# disable the linux-specific interfaces
	xenable_intf_bmc=yes
	xenable_intf_imb=no
	xenable_intf_open=no
	xenable_intf_lipmi=no
	xenable_ipmishell=no
	xenable_all_options=no
	;;
*cygwin*)
	# disable the linux and solaris-specific interfaces
	xenable_intf_imb=no
	xenable_intf_open=no
	xenable_intf_lipmi=no
	;;
*darwin*|aix*)
	# disable the linux and solaris-specific interfaces
	xenable_intf_imb=no
	xenable_intf_open=no
	xenable_intf_lipmi=no
	xenable_ipmishell=no
	;;
*freebsd*)
	xenable_intf_imb=no
	xenable_intf_lipmi=no
	CFLAGS="$CFLAGS -D__BSD_VISIBLE"
	;;
*netbsd*)
	xenable_intf_imb=no
	xenable_intf_lipmi=no
	xenable_intf_open=no
	;;
gnu*)
	# disable the linux and solaris-specific interfaces on Hurd
	xenable_intf_imb=no
	xenable_intf_open=no
	xenable_intf_usb=yes
	;;
esac

AC_SUBST(ARCH, $host_cpu)
AC_SUBST(OS, $host_os)
AC_SUBST(PSTAMP, $host)
AC_SUBST(BASEDIR, $prefix)

dnl allow solaris builds to include all options
AC_ARG_ENABLE([solaris-opt],
	[AC_HELP_STRING([--enable-solaris-opt],
			[enable all options for Solaris [default=no]])],
	[xenable_solaris_opt=$enableval],
	[xenable_solaris_opt=no])
if test "x$xenable_all_options" = "xyes" || test "x$xenable_solaris_opt" = "xyes"; then
	AC_DEFINE(ENABLE_ALL_OPTIONS, [1], [Define to 1 to enable all command line options.])
fi

dnl Determine anonymous union/structure support in GCC
AC_TRY_COMPILE([
#include <stdio.h>
	], [
	struct test {
		union {
			int a;
			unsigned int b;
		};
	} test;

	printf("a is %d", test.a);
        ], ac_need_fms_extension=no, ac_need_fms_extension=yes)
if test "x$ac_need_fms_extension" = "xyes"; then
       CFLAGS="$CFLAGS -fms-extensions"
       AC_SUBST(CFLAGS)
fi

dnl check for OpenSSL functionality
AC_ARG_ENABLE([internal-md5],
	[AC_HELP_STRING([--enable-internal-md5],
			[enable internal MD5 library [default=no]])],
	[xenable_internal_md5=$enableval],
	[xenable_internal_md5=no])

AC_CHECK_LIB([crypto], [EVP_aes_128_cbc],
	[if test "x$xenable_internal_md5" != "xyes"; then
	    have_crypto=yes; LIBS="$LIBS -lcrypto"
	fi],
	[have_crypto=no], [-lcrypto])

AC_CHECK_LIB([crypto], [EVP_sha256],
	[if test "x$xenable_internal_sha256" != "xyes"; then
		if test "x$have_crypto" != "xyes"; then
			LIBS="$LIBS -lcrypto"
		have_sha256=yes
		fi
		AC_DEFINE(HAVE_CRYPTO_SHA256, [1], [Define to 1 if libcrypto supports SHA256.])
	fi],
	[], [-lcrypto])

AC_CHECK_LIB([crypto], [MD5_Init],
	[if test "x$xenable_internal_md5" != "xyes"; then
	    if test "x$have_crypto" != "xyes"; then
	        LIBS="$LIBS -lcrypto"
	        have_md5=yes
	    fi
	    AC_DEFINE(HAVE_CRYPTO_MD5, [1], [Define to 1 if libcrypto supports MD5.])
	fi],
	[], [-lcrypto])

AC_CHECK_LIB([crypto], [MD2_Init],
	[if test "x$xenable_internal_md5" != "xyes"; then
	    if test "x$have_crypto" != "xyes" && test "x$have_md5" != "xyes"; then
	        LIBS="$LIBS -lcrypto"
	        have_md2=yes
	    fi
	    AC_DEFINE(HAVE_CRYPTO_MD2, [1], [Define to 1 if libcrypto supports MD2.])
	fi],
	[], [-lcrypto])

dnl check for libsystemd in case dbus-intf is requested
AC_CHECK_LIB([systemd], [sd_bus_default],
	[
		LIBS="$LIBS -lsystemd"
		have_systemd=yes
	],
	[ have_systemd=no],[])

dnl enable IPMIv1.5 LAN interface
AC_ARG_ENABLE([intf-lan],
	[AC_HELP_STRING([--enable-intf-lan],
			[enable IPMIv1.5 LAN interface [default=yes]])],
	[xenable_intf_lan=$enableval],
	[xenable_intf_lan=yes])
if test "x$xenable_intf_lan" = "xstatic" || test "x$xenable_intf_lan" = "xplugin"; then
	xenable_intf_lan=yes
fi
if test "x$xenable_intf_lan" = "xyes"; then
	AC_DEFINE(IPMI_INTF_LAN, [1], [Define to 1 to enable LAN IPMIv1.5 interface.])
	AC_SUBST(INTF_LAN, [lan])
	AC_SUBST(INTF_LAN_LIB, [libintf_lan.la])
	IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB lan/libintf_lan.la"
fi

dnl enable IPMI USB interface
AC_ARG_ENABLE([intf-usb],
	[AC_HELP_STRING([--enable-intf-usb],
			[enable IPMI USB interface [default=auto]])],
		[xenable_intf_usb=$enableval],
		[xenable_intf_usb=$xenable_intf_usb])
if test "x$xenable_intf_usb" = "xstatic" || test "x$xenable_intf_usb" = "xplugin"; then
   xenable_intf_usb=yes
fi
if test "x$xenable_intf_usb" = "xyes"; then
	AC_DEFINE(IPMI_INTF_USB, [1], [Define to 1 to enable USB interface.])
	AC_SUBST(INTF_USB, [usb])
	AC_SUBST(INTF_USB_LIB, [libintf_usb.la])
	IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB usb/libintf_usb.la"
fi

dnl enable IPMIv2.0 RMCP+ LAN interface
AC_ARG_ENABLE([intf-lanplus],
	[AC_HELP_STRING([--enable-intf-lanplus],
			[enable IPMIv2.0 RMCP+ LAN interface [default=auto]])],
	[xenable_intf_lanplus=$enableval],
	[xenable_intf_lanplus=$have_crypto])
if test "x$xenable_intf_lanplus" = "xstatic" || test "x$xenable_intf_lanplus" = "xplugin"; then
	xenable_intf_lanplus=yes
fi
if test "x$xenable_intf_lanplus" != "xno" && test "x$have_crypto" != "xyes"; then
	echo "** The lanplus interface requires an SSL library with EVP_aes_128_cbc defined."
	xenable_intf_lanplus=no
fi
if test "x$xenable_intf_lanplus" = "xyes"; then
	AC_DEFINE(IPMI_INTF_LANPLUS, [1], [Define to 1 to enable LAN+ IPMIv2 interface.])
	AC_SUBST(INTF_LANPLUS, [lanplus])
	AC_SUBST(INTF_LANPLUS_LIB, [libintf_lanplus.la])
	IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB lanplus/libintf_lanplus.la"
else
	xenable_intf_lanplus=no
fi

ORIG_CPPFLAGS=$CPPFLAGS

dnl enable serial interface
AC_ARG_ENABLE([intf-serial],
	[AC_HELP_STRING([--enable-intf-serial],
			[enable direct Serial Basic/Terminal mode interface [default=yes]])],
	[xenable_intf_serial=$enableval], [xenable_intf_serial=yes])
if test "x$enable_intf_serial" = "xstatic" || test "x$enable_intf_serial" = "xplugin"; then
   xenable_intf_serial=yes
fi
if test "x$xenable_intf_serial" = "xyes"; then
    AC_DEFINE(IPMI_INTF_SERIAL, [1], [Define to 1 to enable serial interface.])
    AC_SUBST(INTF_SERIAL, [serial])
    AC_SUBST(INTF_SERIAL_LIB, [libintf_serial.la])
    IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB serial/libintf_serial.la"
else
	xenable_intf_serial=no
fi

dnl look for OpenIPMI header files
AC_ARG_WITH([kerneldir],
	[AC_HELP_STRING([--with-kerneldir=DIR],
			[set kernel include path to DIR])],
	[if test "x$with_kerneldir" = "xyes"; then
	    with_kerneldir="/lib/modules/`uname -r`/build"
	fi
	CFLAGS="$CFLAGS -I ${with_kerneldir}/include"
	AC_SUBST(CFLAGS)
	CPPFLAGS="$CPPFLAGS -I ${with_kerneldir}/include"
	AC_SUBST(CPPFLAGS)
	if test -s ${with_kerneldir}/include/linux/version.h ; then
	    kernelver=`grep UTS_RELEASE ${with_kerneldir}/include/linux/version.h | \
	        sed 's/^\#define UTS_RELEASE \"\(2\.[0-9]\)\..*/\1/'`
	    if test "x$kernelver" = "x2.6"; then
	        CPPFLAGS="$CPPFLAGS -D__user="
	        AC_SUBST(CPPFLAGS)
	    fi
	fi])

AH_TEMPLATE([HAVE_LINUX_COMPILER_H], [])
AC_MSG_CHECKING([for linux/compiler.h])

m4_version_prereq([2.68],
	[AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <linux/compiler.h>]])],
		[AC_DEFINE(HAVE_LINUX_COMPILER_H, [1],
			[Define to 1 if you have the <linux/compiler.h> header file.])
		AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])])],
	[AC_PREPROC_IFELSE([#include <linux/compiler.h>],
		[AC_DEFINE(HAVE_LINUX_COMPILER_H, [1],
		[Define to 1 if you have the <linux/compiler.h> header file.])
		AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])])])

CPPFLAGS=$ORIG_CPPFLAGS
AC_SUBST(CPPFLAGS)

AC_CHECK_HEADER([sys/ioccom.h],
	[AC_DEFINE(HAVE_SYS_IOCCOM_H, [1],
	[Define to 1 if you have the <sys/ioccom.h> header file.])])

AC_CHECK_HEADER([linux/ipmi.h],
	[AC_DEFINE(HAVE_OPENIPMI_H, [1],
	[Define to 1 if you have the <linux/ipmi.h> header file.])],
	[AC_CHECK_HEADER([sys/ipmi.h],
		[AC_DEFINE(HAVE_FREEBSD_IPMI_H, [1],
		[Define to 1 if you have the <sys/ipmi.h> header file.])],
		[echo "** Unable to find OpenIPMI header files.  Using internal version."])])

dnl look for FreeIPMI files
AC_CHECK_LIB(freeipmi, ipmi_open_inband, [have_free=yes], [have_free=no])
if test "x$have_free" != "xyes"; then
	AC_CHECK_LIB(freeipmi, ipmi_ctx_open_inband, [have_free=yes], [have_free=no])
fi
AC_ARG_ENABLE([intf-free],
	[AC_HELP_STRING([--enable-intf-free],
		[enable FreeIPMI IPMI interface [default=auto]])],
	[xenable_intf_free=$enableval],
	[xenable_intf_free=$have_free])
if test "x$xenable_intf_free" = "xstatic" || test "x$xenable_intf_free" = "xplugin"; then
	xenable_intf_free=yes
fi
if test "x$xenable_intf_free" != "xno" && test "x$have_free" != "xyes"; then
	echo "** Unable to build FreeIPMI interface support!"
	xenable_intf_free=no
fi
if test "x$xenable_intf_free" = "xyes"; then
dnl Determine if you got the right FreeIPMI version  
    AC_MSG_CHECKING([for libfreeipmi version 0.3.0])
    AC_TRY_COMPILE([
#include <sys/types.h>	/* For size_t */
#include <stdio.h>	/* For NULL */
#include <freeipmi/freeipmi.h>
#include <freeipmi/udm/ipmi-udm.h>
    ], [
    ipmi_device_t dev;
    dev = ipmi_open_inband(IPMI_DEVICE_KCS,
                           0,
                           0,
                           0,
                           NULL,
                           0);
    ], ac_free_version_0_3_0=yes, ac_free_version_0_3_0=no)
    AC_MSG_RESULT($ac_free_version_0_3_0)
    AC_MSG_CHECKING([for libfreeipmi version 0.4.0])
    AC_TRY_COMPILE([
#include <sys/types.h>	/* For size_t */
#include <stdio.h>	/* For NULL */
#include <freeipmi/freeipmi.h>
#include <freeipmi/udm/ipmi-udm.h>
    ], [
    ipmi_device_t dev = NULL;
    int rv;
    dev = ipmi_device_create();
    rv = ipmi_open_inband(dev,
                          IPMI_DEVICE_KCS,
                          0,
                          0,
                          0,
                          NULL,
                          0);
    ], ac_free_version_0_4_0=yes, ac_free_version_0_4_0=no)
    AC_MSG_RESULT($ac_free_version_0_4_0)
    AC_MSG_CHECKING([for libfreeipmi version 0.5.0])
    AC_TRY_COMPILE([
#include <sys/types.h>	/* For size_t */
#include <stdio.h>	/* For NULL */
#include <freeipmi/freeipmi.h>
#include <freeipmi/udm/ipmi-udm.h>
    ], [
    ipmi_device_t dev = NULL;
    int rv;
    dev = ipmi_device_create();
    rv = ipmi_open_inband(dev,
                          IPMI_DEVICE_KCS,
                          0,
                          0,
                          0,
                          NULL,
                          0,
                          0);
    ], ac_free_version_0_5_0=yes, ac_free_version_0_5_0=no)
    AC_MSG_RESULT($ac_free_version_0_5_0)

    AC_MSG_CHECKING([for libfreeipmi version 0.6.0])
    AC_TRY_COMPILE([
#include <stdio.h>	/* For NULL */
#include <freeipmi/freeipmi.h>
    ], [
    ipmi_ctx_t ctx = NULL;
    int rv;
    ctx = ipmi_ctx_create();
    rv = ipmi_ctx_open_inband(ctx,
                              IPMI_DEVICE_KCS,
                              0,
                              0,
                              0,
                              NULL,
                              0,
                              0);
    ], ac_free_version_0_6_0=yes, ac_free_version_0_6_0=no)
    AC_MSG_RESULT($ac_free_version_0_6_0)

    if test "x$ac_free_version_0_3_0" = "xyes" \
       || test "x$ac_free_version_0_4_0" = "xyes" \
       || test "x$ac_free_version_0_5_0" = "xyes" \
       || test "x$ac_free_version_0_6_0" = "xyes"; then
       AC_DEFINE(IPMI_INTF_FREE, [1], [Define to 1 to enable FreeIPMI interface.])
       AC_SUBST(INTF_FREE, [free])
       AC_SUBST(INTF_FREE_LIB, [libintf_free.la])
       IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB free/libintf_free.la"
       if test "x$ac_free_version_0_3_0" = "xyes"; then
          AC_DEFINE(IPMI_INTF_FREE_0_3_0, [1], [Define to 1 for FreeIPMI 0.3.0.])
       fi
       if test "x$ac_free_version_0_4_0" = "xyes"; then
          AC_DEFINE(IPMI_INTF_FREE_0_4_0, [1], [Define to 1 for FreeIPMI 0.4.0.])
       fi
       if test "x$ac_free_version_0_5_0" = "xyes"; then
          AC_DEFINE(IPMI_INTF_FREE_0_5_0, [1], [Define to 1 for FreeIPMI 0.5.0.])
       fi
       if test "x$ac_free_version_0_6_0" = "xyes"; then
          AC_DEFINE(IPMI_INTF_FREE_0_6_0, [1], [Define to 1 for FreeIPMI 0.6.0.])
       fi
    else
       xenable_intf_free=no
    fi

    AC_CHECK_LIB(freeipmi,
      ipmi_cmd_raw_ipmb, 
      AC_DEFINE(IPMI_INTF_FREE_BRIDGING, [1],
        [Define to 1 to enable FreeIPMI Bridging Support.]))
fi

dnl look for termios header file
AC_CHECK_HEADER([termios.h],
	[AC_DEFINE(HAVE_TERMIOS_H, [1], [Define to 1 if you have <termios.h>.])],
	[AC_CHECK_HEADER([sys/termios.h],
		[AC_DEFINE(HAVE_SYS_TERMIOS_H, [1], [Define to 1 if you have <sys/termios.h>.])],
		[echo "** Unable to find termios header file."])])

dnl set RPM distro tag for use in RPM name
AC_ARG_WITH([rpm-distro],
	[AC_HELP_STRING([--with-rpm-distro=DISTRO],
			[set Linux distribution tag for use in RPM version string])],
	[AC_SUBST(DISTRO, $with_rpm_distro)])

dnl set RPM release tag
AC_ARG_WITH([rpm-release],
	[AC_HELP_STRING([--with-rpm-release=RELEASE],
			[set release number for RPM release field])],
	[], [with_rpm_release=1])
AC_SUBST(RPM_RELEASE, $with_rpm_release)

dnl enable Linux OpenIPMI interface
AC_ARG_ENABLE([intf-open],
	[AC_HELP_STRING([--enable-intf-open],
			[enable Linux OpenIPMI interface [default=auto]])],
	[xenable_intf_open=$enableval],
	[])
if test "x$xenable_intf_open" = "xstatic" || test "x$xenable_intf_open" = "xplugin"; then
	xenable_intf_open=yes
fi
if test "x$xenable_intf_open" = "xyes"; then
	AC_DEFINE(IPMI_INTF_OPEN, [1], [Define to 1 to enable Linux OpenIPMI interface.])
	AC_SUBST(INTF_OPEN, [open])
	AC_SUBST(INTF_OPEN_LIB, [libintf_open.la])
	IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB open/libintf_open.la"

	dnl Check for dual bridge support in OpenIPMI
	AC_MSG_CHECKING([for OpenIPMI dual bridge support])
	have_openipmi_dual_bridge=no
	ORIG_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS -Isrc/plugins/open -DENABLE_INTF_OPEN_DUAL_BRIDGE"
	AC_TRY_COMPILE([
			#if defined(HAVE_OPENIPMI_H)
			# if defined(HAVE_LINUX_COMPILER_H)
			#  include <linux/compiler.h>
			# endif
			# include <linux/ipmi.h>
			#elif defined(HAVE_FREEBSD_IPMI_H)
			# include <sys/ipmi.h>
			#else
			# include "open.h"
			#endif
		], [
			struct ipmi_ipmb_addr a;
			a.transit_slave_addr = 0;
		], [have_openipmi_dual_bridge=yes])
	if test x"$have_openipmi_dual_bridge" = x"yes"; then
		AC_DEFINE(ENABLE_INTF_OPEN_DUAL_BRIDGE, [1],
			[Define to 1 to enable OpenIPMI interface dual bridge support])
	fi
	CPPFLAGS="$ORIG_CPPFLAGS"
	AC_MSG_RESULT([$have_openipmi_dual_bridge])
fi

dnl enable Intel IMB interface
AC_ARG_ENABLE([intf-imb],
	[AC_HELP_STRING([--enable-intf-imb],
			[enable Intel IMB driver interface [default=auto]])],
	[xenable_intf_imb=$enableval],
	[])
if test "x$xenable_intf_imb" = "xstatic" || test "x$xenable_intf_imb" = "xplugin"; then
	xenable_intf_imb=yes
fi
if test "x$xenable_intf_imb" = "xyes"; then
	AC_DEFINE(IPMI_INTF_IMB, [1], [Define to 1 to enable Intel IMB interface.])
	AC_SUBST(INTF_IMB, [imb])
	AC_SUBST(INTF_IMB_LIB, [libintf_imb.la])
	IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB imb/libintf_imb.la"
fi

dnl enable Solaris LIPMI interface
AC_CHECK_HEADER([sys/lipmi/lipmi_intf.h], [have_lipmi=yes], [have_lipmi=no])
AC_ARG_ENABLE([intf-lipmi],
	[AC_HELP_STRING([--enable-intf-lipmi],
			[enable Solaris 9 x86 IPMI interface [default=no]])],
	[xenable_intf_lipmi=$enableval],
	[])
if test "x$xenable_intf_lipmi" = "xstatic" || test "x$xenable_intf_lipmi" = "xplugin"; then
	xenable_intf_lipmi=yes
fi
if test "x$xenable_intf_lipmi" != "xno" && test "x$have_lipmi" != "xyes"; then
	echo "** Unable to build Solaris 9 x86 IPMI interface support!"
	xenable_intf_lipmi=no
fi
if test "x$xenable_intf_lipmi" = "xyes"; then
	AC_DEFINE(IPMI_INTF_LIPMI, [1], [Define to 1 to enable Solaris 9 LIPMI interface.])
	AC_SUBST(INTF_LIPMI, [lipmi])
	AC_SUBST(INTF_LIPMI_LIB, [libintf_lipmi.la])
	IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB lipmi/libintf_lipmi.la"
fi

dnl enable Solaris BMC interface
AC_ARG_ENABLE([intf-bmc],
	[AC_HELP_STRING([--enable-intf-bmc],
			[enable Solaris 10 x86 IPMI interface [default=auto]])],
	[xenable_intf_bmc=$enableval],
	[xenable_intf_bmc=no])
if test "x$xenable_intf_bmc" = "xstatic" || test "x$xenable_intf_bmc" = "xplugin"; then
	xenable_intf_bmc=yes
fi
if test "x$xenable_intf_bmc" = "xyes"; then
	AC_DEFINE(IPMI_INTF_BMC, [1], [Define to 1 to enable Solaris 10 BMC interface.])
	AC_SUBST(INTF_BMC, [bmc])
	AC_SUBST(INTF_BMC_LIB, [libintf_bmc.la])
	IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB bmc/libintf_bmc.la"
fi

dnl enable IPMI dbus interface
AC_ARG_ENABLE([intf-dbus],
		[AC_HELP_STRING([--enable-intf-dbus],
			[enable IPMI dbus interface [default=no]])],
		[xenable_intf_dbus=$enableval],
		[xenable_intf_dbus=no])
if test "x$xenable_intf_dbus" != "xno"; then
	if test "x$have_systemd" != "xyes"; then
		AC_MSG_ERROR([** Unable to find libsystemd required by dbus-intf.])
		xenable_intf_dbus=no
	fi
fi
if test "x$xenable_intf_dbus" = "xyes"; then
	AC_DEFINE(IPMI_INTF_DBUS, [1], [Define to 1 to enable dbus interface.])
	AC_SUBST(INTF_DBUS, [dbus])
	AC_SUBST(INTF_DBUS_LIB, [libintf_dbus.la])
	IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB dbus/libintf_dbus.la"
fi

dnl enable Dummy interface for testing
AC_ARG_ENABLE([intf-dummy],
	[AC_HELP_STRING([--enable-intf-dummy],
			[enable Dummy(test) interface [default=no]])],
	[xenable_intf_dummy=$enableval], [xenable_intf_dummy=no])
if test "x$xenable_intf_dummy" = "xyes"; then
	AC_DEFINE(IPMI_INTF_DUMMY, [1], [Define to 1 to enable Dummy interface.])
	AC_SUBST(INTF_DUMMY, [dummy])
	AC_SUBST(INTF_DUMMY_LIB, [libintf_dummy.la])
	IPMITOOL_INTF_LIB="$IPMITOOL_INTF_LIB dummy/libintf_dummy.la"
fi

AC_SUBST(IPMITOOL_INTF_LIB)

AC_ARG_ENABLE([ipmishell],
	[AC_HELP_STRING([--enable-ipmishell],
			[enable IPMI shell interface [default=auto]])],
	[xenable_ipmishell=$enableval],
	[])

dnl check for readline library to enable ipmi shell
if test "x$xenable_ipmishell" = "xyes"; then
	AC_SEARCH_LIBS([readline], [readline edit], [have_readline=yes])
	if test "x$have_readline" != "xyes"; then
		AC_MSG_ERROR([** Unable to find readline required by ipmishell.])
		xenable_ipmishell=no
	fi
	AC_DEFINE(HAVE_READLINE, [1], [Define to 1 if readline present.])
fi

dnl Enable -Wall -Werror
AC_ARG_ENABLE([buildcheck],
	[AC_HELP_STRING([--enable-buildcheck],
			[enable -Wall -Werror for build testing [default=no]])],
	[xenable_buildcheck=$enableval],
	[xenable_buildcheck=no])
if test "x$xenable_buildcheck" != "xno"; then
	CFLAGS="$CFLAGS -Wall -Werror -Wpointer-arith -Wstrict-prototypes"
fi
AC_SUBST(CFLAGS)

dnl Enable extra file security paranoia
AC_ARG_ENABLE([file-security],
	[AC_HELP_STRING([--enable-file-security],
			[enable extra security checks on files opened for read [default=no]])],
	[xenable_file_security=$enableval],
	[xenable_file_security=no])
if test "x$xenable_file_security" != "xno"; then
	AC_DEFINE(ENABLE_FILE_SECURITY, [1], [Define to 1 for extra file security.])
fi


AC_TRY_COMPILE([],[
	#include <stdio.h>
	
	struct packstruct {
		unsigned char t0       :5;
		unsigned char t1       :5;
		unsigned char t2       :5;
		unsigned char t3       :5;
		unsigned char t4       :4;
	} __attribute__ ((packed));
	
	int
	main(int argc, char ** argv)
	{
	if(sizeof(struct packstruct) != 3)
	return(1);
	else
	return(0);
	}
	],
	[],
	[AC_DEFINE(HAVE_PRAGMA_PACK,[1],
		[Define to 1 if you need to use #pragma pack instead of __attribute__ ((packed))])]
)

dnl if no environment variable is set, set the default value for the default intf
if test "${xenable_intf_open}" = "yes"; then
   DEFAULT_INTF_NO_ENV=open
else dnl macOS does not build open interface, it defaults to lan
   DEFAULT_INTF_NO_ENV=lan
fi
dnl allow for a default interface to be set on configure
AC_ARG_VAR(DEFAULT_INTF, [Set the default interface to use (default='open' if available, 'lan' otherwise)])

dnl set the default value for the default interface environment variable
if test "x${DEFAULT_INTF}" = "x"; then
	echo "DEFAULT_INTF not found in environment; setting to ${DEFAULT_INTF_NO_ENV}"
	DEFAULT_INTF=${DEFAULT_INTF_NO_ENV}
fi

if test "x"`eval "echo \\\${xenable_intf_${DEFAULT_INTF}}"` != "xyes"; then
	AC_MSG_ERROR([** Cannot set ${DEFAULT_INTF} as default; intf-${DEFAULT_INTF} is not enabled.])
fi

AC_ARG_VAR(IANADIR, [Configure the path to IANA PEN dictionary (default=DATAROOTDIR/misc)])
AC_ARG_VAR(IANAUSERDIR, [Configure the path to IANA PEN dictionary wihtin the user's HOME directory (default=.local/usr/share/misc)])

if test "x${IANADIR}" = "x"; then
	IANADIR=`eval echo "${datarootdir}/misc"`
	echo Set IANA PEN dictionary search path to ${IANADIR}
fi

if test "x${IANAUSERDIR}" = "x"; then
	IANAUSERDIR=".local/usr/share/misc"
	echo Set user\'s IANA PEN dictionary search path to ${IANAUSERDIR}
fi

AH_TEMPLATE([IANADIR],[The path to system IANA PEN dictionary])
AC_DEFINE_UNQUOTED(IANADIR, "`eval "echo ${IANADIR}"`", [])

AH_TEMPLATE([IANAUSERDIR],[The subpath to user IANA PEN dictionary within the user's HOME])
AC_DEFINE_UNQUOTED(IANAUSERDIR, "`eval "echo ${IANAUSERDIR}"`", [])

AH_TEMPLATE([PATH_SEPARATOR], [The path separator string])
#if defined _WIN32 || defined __CYGWIN__
AC_DEFINE(PATH_SEPARATOR, "\\")
#else
AC_DEFINE(PATH_SEPARATOR, "/")
#endif

dnl Generate files for build
AC_CONFIG_FILES([Makefile
		doc/Makefile
		contrib/Makefile
		control/Makefile
		control/pkginfo
		control/prototype
		control/ipmitool.spec
		lib/Makefile
		include/Makefile
		include/ipmitool/Makefile
		src/Makefile
		src/plugins/Makefile
		src/plugins/lan/Makefile
		src/plugins/lanplus/Makefile
		src/plugins/open/Makefile
		src/plugins/free/Makefile
		src/plugins/imb/Makefile
		src/plugins/bmc/Makefile
		src/plugins/dbus/Makefile
		src/plugins/usb/Makefile
		src/plugins/lipmi/Makefile
		src/plugins/serial/Makefile
		src/plugins/dummy/Makefile
		doc/ipmitool.1
		doc/ipmievd.8])

AC_OUTPUT

AC_MSG_RESULT([])
AC_MSG_RESULT([ipmitool $VERSION])
AC_MSG_RESULT([])
AC_MSG_RESULT([Interfaces (default=$DEFAULT_INTF)])
AC_MSG_RESULT([  lan     : $xenable_intf_lan])
AC_MSG_RESULT([  lanplus : $xenable_intf_lanplus])
AC_MSG_RESULT([  open    : $xenable_intf_open])
AC_MSG_RESULT([  free    : $xenable_intf_free])
AC_MSG_RESULT([  imb     : $xenable_intf_imb])
AC_MSG_RESULT([  bmc     : $xenable_intf_bmc])
AC_MSG_RESULT([  dbus    : $xenable_intf_dbus])
AC_MSG_RESULT([  usb     : $xenable_intf_usb])
AC_MSG_RESULT([  lipmi   : $xenable_intf_lipmi])
AC_MSG_RESULT([  serial  : $xenable_intf_serial])
AC_MSG_RESULT([  dummy   : $xenable_intf_dummy])
AC_MSG_RESULT([])
AC_MSG_RESULT([Extra tools])
AC_MSG_RESULT([  ipmievd   : yes])
AC_MSG_RESULT([  ipmishell : $xenable_ipmishell])
AC_MSG_RESULT([])

