##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

dnl AC_PREREQ([2.65])
AC_INIT(xupnp, version-1.0, balaji_punnuru@cable.comcast.com,xupnp)
AC_CONFIG_HEADERS([cfg/config.h])
AC_CONFIG_MACRO_DIR([cfg])
AC_CONFIG_AUX_DIR([cfg])
AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-bzip2 1.9 subdir-objects])
LT_INIT

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_CXX

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_HEADER_STDBOOL

IARM_CFLAGS=" "

# Checks for library functions.
#Add the subdirectories to be considered for building.
#SUBDIRS="src include"
#to setup m4 macros
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
[AM_DEFAULT_VERBOSITY=1
AC_SUBST(AM_DEFAULT_VERBOSITY)])
AC_ARG_ENABLE([gupnp_latest],
              AS_HELP_STRING([--enable-gupnp_latest],[enable gupnp to point to latest version (default is no)]),
              [
      		case "${enableval}" in
        	 yes) GUPNP_VERSION=GUPNP_0_19
                      PKG_CHECK_MODULES([GUPNP], [gupnp-1.0 >= 0.19.4]);;
        	 no)  GUPNP_VERSION=GUPNP_0_14
                      PKG_CHECK_MODULES([GUPNP], [gupnp-1.0 >= 0.14.1]);;
        	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-gupnp_latest]) ;;
       		esac
    	      ],
    	      [echo "latest gupnp is disabled"])
AC_ARG_ENABLE([iarmbus],
              AS_HELP_STRING([--enable-iarmbus],[enable iarmbus to to be used (default is no)]),
              [
      		case "${enableval}" in
        	 yes) IARMBUSLIB="-lIARMBus"
		      IARM_CFLAGS=" -DUSE_XUPNP_IARM_BUS"
                      AC_SUBST(IARMBUSLIB);;
        	 no)  IARMLIB="-lUIIARM"
		      IARM_CFLAGS=" -DUSE_XUPNP_IARM"
                      AC_SUBST(IARMLIB);;
        	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-iarmbus]) ;;
       		esac
    	      ],
    	      [echo "IARMBUS is disabled"])
AC_ARG_ENABLE([client-xcal-server],
              AS_HELP_STRING([--enable-client-xcal-server],[enable client-xcal-server (default is no)]),
              [
                case "${enableval}" in
                 yes) CFLAGS+=" -DCLIENT_XCAL_SERVER";;
                 no)   echo "CLIENT_XCAL_SERVER is disabled";;
                 *) AC_MSG_ERROR([bad value ${enableval} for --enable-client-xcal-server]) ;;
                esac
              ],
              [echo "client-xcal-server is disabled"])
AC_ARG_ENABLE([dbus],
              AS_HELP_STRING([--enable-dbus],[enable dbus to to be used (default is no)]),
              [
      		case "${enableval}" in
        	 yes) PKG_CHECK_MODULES([DBUS], [dbus-1 >= 0.22])
                      DBUS_CFLAGS+=" -DUSE_DBUS";;
        	 no)  echo "DBUS is disabled";;
        	 *) AC_MSG_ERROR([bad value ${enableval} for --enable-dbus]) ;;
       		esac
    	      ],
    	      [echo "DBUS is disabled"])
AC_ARG_ENABLE([fog],
              AS_HELP_STRING([--enable-fog],[enable fog (default is no)]),
              [
                case "${enableval}" in
                 yes) CFLAGS+=" -DENABLE_FOG";;
                 no)   echo "FOG is disabled";;
                 *) AC_MSG_ERROR([bad value ${enableval} for --enable-fog]) ;;
                esac
              ],
              [echo "fog is disabled"])
AC_ARG_ENABLE([rfc],
              AS_HELP_STRING([--enable-rfc],[enable rfc (default is no)]),
              [
                case "${enableval}" in
                 yes) CFLAGS+=" -DENABLE_RFC";;
                 no)   echo "RFC is disabled";;
                 *) AC_MSG_ERROR([bad value ${enableval} for --enable-rfc]) ;;
                esac
              ],
              [echo "rfc is disabled"])
LIBRARY_SUPPORT=" "
AC_ARG_ENABLE([library-support],
        AS_HELP_STRING([--enable-library-support],[This will enable/disable library support.]),
        [
          case "${enableval}" in
           yes) LIBRARY_SUPPORT=true ;;
           no) LIBRARY_SUPPORT=false ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-library-support ]) ;;
           esac
           ],
         [echo "library-support is enable"])
AM_CONDITIONAL([LIBRARY_SUPPORT], [test x$LIBRARY_SUPPORT = xtrue])

VIDEO_SUPPORT=" "
AC_ARG_ENABLE([video-support],
        AS_HELP_STRING([--enable-video-support],[This will enable/disable video library support.]),
        [
          case "${enableval}" in
           yes) VIDEO_SUPPORT=true ;;
           no) VIDEO_SUPPORT=false ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-video-support ]) ;;
           esac
           ],
         [echo "video-support is enable"])
AM_CONDITIONAL([VIDEO_SUPPORT], [test x$VIDEO_SUPPORT = xtrue])
BROADBAND_SUPPORT=" "
AC_ARG_ENABLE([broadband-support],
        AS_HELP_STRING([--enable-broadband-support],[This will enable/disable broadband library support.]),
        [
          case "${enableval}" in
           yes) BROADBAND_SUPPORT=true
                CFLAGS+=" -DCLIENT_XCAL_SERVER"
		CFLAGS+=" -DBROADBAND";;
           no) BROADBAND_SUPPORT=false ;;
          *) AC_MSG_ERROR([bad value ${enableval} for --enable-broadband-support ]) ;;
           esac
           ],
         [echo "broadband-support is enable"])
AM_CONDITIONAL([BROADBAND_SUPPORT], [test x$BROADBAND_SUPPORT = xtrue])
PKG_CHECK_MODULES([LIBXML], [libxml-2.0 >= 2.7.8])
PKG_CHECK_MODULES([LIBGSSDP], [gssdp-1.0] >= 0.10.0)
PKG_CHECK_MODULES([LIBSOUP], [libsoup-2.4 >= 0.22])
PKG_CHECK_MODULES([GLIB],[glib-2.0 >= 2.22.5])
PKG_CHECK_MODULES([GNUTLS],[gnutls >= 2.10.0])
PKG_CHECK_MODULES([GTHREAD],[gthread-2.0 >= 2.22.5])
PKG_CHECK_EXISTS([glib-2.0 >= 2.32], [PKG_CHECK_MODULES(LIBFFI, [libffi >= 3.0.0])
                                      PKG_CHECK_MODULES(GMODULE,[gmodule-2.0 >= 2.32])
                                      PKG_CHECK_MODULES(ZLIB, zlib)], [])
PKG_CHECK_EXISTS([libsoup-2.4 >= 2.45], [PKG_CHECK_MODULES(SQLITE, sqlite3)], [])

AC_CONFIG_FILES([Makefile
include/Makefile
conf/Makefile
cfg/Makefile
src/Makefile
video/Makefile
video/lib/Makefile
video/lib/src/Makefile
broadband/Makefile
broadband/lib/Makefile])
AC_SUBST(IARM_CFLAGS)
AC_SUBST(GUPNP_VERSION)
AC_SUBST(VIDEO_SUPPORT)
AC_SUBST(BROADBAND_SUPPORT)
AC_OUTPUT
