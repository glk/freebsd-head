# $FreeBSD$
#
# Logic to build and install plain test programs.  A plain test programs it not
# supposed to use any specific testing framework: all it does is run some code
# and report the test's pass or fail status via a 0 or 1 exit code.

.include <bsd.init.mk>

# List of C, C++ and shell test programs to build.
#
# Programs listed here are built according to the semantics of bsd.prog.mk for
# PROGS, PROGS_CXX and SCRIPTS, respectively.
#
# Test programs registered in this manner are set to be installed into TESTSDIR
# (which should be overriden by the Makefile) and are not required to provide a
# manpage.
PLAIN_TESTS_C?=
PLAIN_TESTS_CXX?=
PLAIN_TESTS_SH?=

.if !empty(PLAIN_TESTS_C)
PROGS+= ${PLAIN_TESTS_C}
_TESTS+= ${PLAIN_TESTS_C}
.for _T in ${PLAIN_TESTS_C}
BINDIR.${_T}= ${TESTSDIR}
MAN.${_T}?= # empty
TEST_INTERFACE.${_T}= plain
.endfor
.endif

.if !empty(PLAIN_TESTS_CXX)
PROGS_CXX+= ${PLAIN_TESTS_CXX}
PROGS+= ${PLAIN_TESTS_CXX}
_TESTS+= ${PLAIN_TESTS_CXX}
.for _T in ${PLAIN_TESTS_CXX}
BINDIR.${_T}= ${TESTSDIR}
MAN.${_T}?= # empty
TEST_INTERFACE.${_T}= plain
.endfor
.endif

.if !empty(PLAIN_TESTS_SH)
SCRIPTS+= ${PLAIN_TESTS_SH}
_TESTS+= ${PLAIN_TESTS_SH}
.for _T in ${PLAIN_TESTS_SH}
SCRIPTSDIR_${_T}= ${TESTSDIR}
TEST_INTERFACE.${_T}= plain
.endfor
.endif

.include <bsd.test.mk>
