## Process this file with automake to produce Makefile.in
##
## Original author:
##     Mohammad Akhlaghi <akhlaghi@gnu.org>
## Contributing author(s):
## Copyright (C) 2015, Free Software Foundation, Inc.
##
## Gnuastro is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## Gnuastro is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with Gnuastro. If not, see <http://www.gnu.org/licenses/>.





## Necessary flags.
##
##   $(top_srcdir)/bootstrapped/lib: only necessary for the libraries since
##       the Gnulib functions will be statically linked to the Gnuastro
##       library so linking to Gnuastro is enough to access them also.
##
##   SYSCONFIG_DIR: only necessary in `options.c' to get the system
##       installation directory.
AM_CPPFLAGS = -I\$(top_srcdir)/bootstrapped/lib            \
              -DSYSCONFIG_DIR=\"$(sysconfdir)\"





# Define the main compiled library file, its Libtool version and also link
# with Gnulib's compiled library for this system. We are doing this so the
# Gnuastro library functions can also benefit from Gnulib's many great
# features. This also avoids the need for the programs to link separately
# with Gnulib, they only need to link with the Gnuastro library.
lib_LTLIBRARIES = libgnuastro.la
libgnuastro_la_LDFLAGS = -version-info $(GAL_LT_VERSION)
libgnuastro_la_LIBADD = $(top_builddir)/bootstrapped/lib/libgnu.la




# Specify the library .c files
libgnuastro_la_SOURCES = arithmetic.c arithmetic-binary.c                  \
  arithmetic-onlyint.c binary.c blank.c box.c checkset.c convolve.c data.c \
  fits.c git.c interpolate.c list.c options.c permutation.c polygon.c      \
  qsort.c dimension.c statistics.c table.c tableintern.c threads.c tile.c  \
  timing.c txt.c type.c wcs.c







# Installed headers, note that we are not blindly including all `.h' files
# in the $(headersdir) directory. Some of the header files don't need to be
# installed.
headersdir=$(top_srcdir)/lib/gnuastro
pkginclude_HEADERS = gnuastro/config.h $(headersdir)/arithmetic.h          \
  $(headersdir)/binary.h $(headersdir)/blank.h $(headersdir)/box.h         \
  $(headersdir)/convolve.h $(headersdir)/data.h $(headersdir)/dimension.h  \
  $(headersdir)/fits.h $(headersdir)/git.h $(headersdir)/interpolate.h     \
  $(headersdir)/list.h $(headersdir)/permutation.h $(headersdir)/polygon.h \
  $(headersdir)/qsort.h $(headersdir)/statistics.h $(headersdir)/table.h   \
  $(headersdir)/threads.h $(headersdir)/tile.h $(headersdir)/txt.h         \
  $(headersdir)/type.h $(headersdir)/wcs.h





# Files to distribute in the tarball. These are internal headers and don't
# need to be installed. Headers are only mentioned within the source files,
# and if they are not explicitly mentioned somewhere in the Makefile, they
# will not distributed, so we need to explicitly tell Automake to
# distribute them here.
internaldir=$(top_srcdir)/lib/gnuastro-internal
EXTRA_DIST = gnuastro.pc.in $(headersdir)/README $(internaldir)/README    \
  $(internaldir)/arithmetic-binary.h $(internaldir)/arithmetic-internal.h \
  $(internaldir)/arithmetic-onlyint.h $(internaldir)/checkset.h           \
  $(internaldir)/commonopts.h $(internaldir)/config.h.in                  \
  $(internaldir)/fixedstringmacros.h $(internaldir)/options.h             \
  $(internaldir)/tableintern.h $(internaldir)/timing.h





# Definitions for Gnuastro's the pkg-config file (inspired from GSL's
# Makefile.am)
pkgconfig_DATA = gnuastro.pc
pkgconfigdir = $(libdir)/pkgconfig
CLEANFILES = gnuastro.pc gnuastro/config.h





# Build `gnuastro/config.h' based on the information in the Makefile after
# the Makefile has been built.
gnuastro/config.h: Makefile $(internaldir)/config.h.in
	rm -f $@ $@.tmp
	$(MKDIR_P) gnuastro
	$(SED) -e 's|@VERSION[@]|$(VERSION)|g'                            \
	       -e 's|@HAVE_LIBGIT2[@]|$(HAVE_LIBGIT2)|g'                  \
	       -e 's|@HAVE_WCSLIB_VERSION[@]|$(HAVE_WCSLIB_VERSION)|g'    \
	       -e 's|@HAVE_PTHREAD_BARRIER[@]|$(HAVE_PTHREAD_BARRIER)|g'  \
	       -e 's|@HAVE_BIN_OP_UINT8[@]|$(HAVE_BIN_OP_UINT8)|g'        \
	       -e 's|@HAVE_BIN_OP_INT8[@]|$(HAVE_BIN_OP_INT8)|g'          \
	       -e 's|@HAVE_BIN_OP_UINT16[@]|$(HAVE_BIN_OP_UINT16)|g'      \
	       -e 's|@HAVE_BIN_OP_INT16[@]|$(HAVE_BIN_OP_INT16)|g'        \
	       -e 's|@HAVE_BIN_OP_UINT32[@]|$(HAVE_BIN_OP_UINT32)|g'      \
	       -e 's|@HAVE_BIN_OP_INT32[@]|$(HAVE_BIN_OP_INT32)|g'        \
	       -e 's|@HAVE_BIN_OP_UINT64[@]|$(HAVE_BIN_OP_UINT64)|g'      \
	       -e 's|@HAVE_BIN_OP_INT64[@]|$(HAVE_BIN_OP_INT64)|g'        \
	       -e 's|@HAVE_BIN_OP_FLOAT32[@]|$(HAVE_BIN_OP_FLOAT32)|g'    \
	       -e 's|@HAVE_BIN_OP_FLOAT64[@]|$(HAVE_BIN_OP_FLOAT64)|g'    \
	       -e 's|@SIZEOF_SIZE_T[@]|$(SIZEOF_SIZE_T)|g'                \
	       -e 's|@RESTRICT_REPLACEMENT[@]|$(RESTRICT_REPLACEMENT)|g'  \
               $(internaldir)/config.h.in >> $@.tmp
	chmod a-w $@.tmp
	mv $@.tmp $@





# Build Gnuastro's pkg-config file similar to `gnuastro/config.h'.
gnuastro.pc: Makefile $(srcdir)/gnuastro.pc.in
	rm -f $@ $@.tmp
	$(SED)                                      \
	-e 's|@prefix[@]|$(prefix)|g'               \
	-e 's|@exec_prefix[@]|$(exec_prefix)|g'     \
	-e 's|@libdir[@]|$(libdir)|g'               \
	-e 's|@includedir[@]|$(includedir)|g'       \
	-e 's|@LIBS[@]|$(LIBS)|g'                   \
	-e 's|@VERSION[@]|$(VERSION)|g'             \
	'$(srcdir)/$@.in' >> $@.tmp
	chmod a-w $@.tmp
	mv $@.tmp $@
