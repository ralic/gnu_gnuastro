# Crop from a catalog using x and y coordinates in WCS mode.
#
# See the Tests subsection of the manual for a complete explanation
# (in the Installing gnuastro section).
#
# Original author:
#     Mohammad Akhlaghi <akhlaghi@gnu.org>
# Contributing author(s):
#
# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.  This file is offered as-is,
# without any warranty.





# Preliminaries
# =============
#
# Set the variabels (The executable is in the build tree). Do the
# basic checks to see if the executable is made or if the defaults
# file exists (basicchecks.sh is in the source tree).
prog=crop
img=mkprofcat*.fits
execname=../bin/$prog/ast$prog




# Skip?
# =====
#
# If the dependencies of the test don't exist, then skip it. There are two
# types of dependencies:
#
#   - The executable was not made (for example due to a configure option),
#
#   - The input data was not made (for example the test that created the
#     data file failed).
if [ ! -f $execname ]; then exit 77; fi
for fn in $img; do if [ ! -f $fn ]; then exit 77; fi; done





# Actual test script
# ==================
#
# The number of threads is one so if CFITSIO does is not configured to
# enable multithreaded access to files, the tests pass. It is the
# users choice to enable this feature.
cat=$topsrc/tests/$prog/cat.txt
$execname $img --catalog=$cat --mode=wcs --suffix=_wcscat.fits        \
          --zeroisnotblank --racol=4 --deccol=DEC_CENTER --numthreads=1
