#!/bin/sh
#-
# Copyright (c) 2011 Nathan Whitehorn
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $MidnightBSD$
#

# generate-release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
#
#  Usage: generate-release.sh svn-branch[@revision] scratch-dir
#
# Environment variables:
#  SVNROOTBASE: SVN base URL to MidnightBSD repository (svn://svn.midnightbsd.org)
#  SVNROOTSRC:  URL to MidnightBSD src tree (${SVNROOTBASE}/base)
#  SVNROOTDOC:  URL to MidnightBSD doc tree (${SVNROOTBASE}/doc)
#  SVNROOTPORTS:URL to MidnightBSD mports tree (${SVNROOTBASE}/mports)
#  BRANCHSRC:   branch name of src (svn-branch[@revision])
#  BRANCHDOC:   branch name of doc (trunk)
#  BRANCHPORTS: branch name of mports (trunk)
#  WORLD_FLAGS: optional flags to pass to buildworld (e.g. -j)
#  KERNEL_FLAGS: optional flags to pass to buildkernel (e.g. -j)
#

usage()
{
	echo "Usage: $0 svn-branch[@revision] scratch-dir" 2>&1
	exit 1
}

if [ $# -lt 2 ]; then
	usage
fi

: ${SVNROOTBASE:=https://github.com/midnightbsd/}
: ${SVNROOTSRC:=${SVNROOTBASE}/src.git}
: ${SVNROOTDOC:=${SVNROOTBASE}/doc.git}
: ${SVNROOTPORTS:=${SVNROOTBASE}/mports.git}
: ${SVNROOT:=${SVNROOTSRC}} # for backward compatibility
: ${SVN_CMD:=/usr/local/bin/svn}
BRANCHSRC=$1
: ${BRANCHDOC:=trunk}
: ${BRANCHPORTS:=trunk}
: ${WORLD_FLAGS:=${MAKE_FLAGS}}
: ${KERNEL_FLAGS:=${MAKE_FLAGS}}
: ${CHROOTDIR:=$2}
 
if [ ! -r "${CHROOTDIR}" ]; then
	echo "${CHROOTDIR}: scratch dir not found."
	exit 1
fi

CHROOT_CMD="/usr/sbin/chroot ${CHROOTDIR}"
case ${TARGET} in
"")	;;
*)	SETENV_TARGET="TARGET=$TARGET" ;;
esac
case ${TARGET_ARCH} in
"")	;;
*)	SETENV_TARGET_ARCH="TARGET_ARCH=$TARGET_ARCH" ;;
esac
SETENV="env -i PATH=/bin:/usr/bin:/sbin:/usr/sbin"
CROSSENV="${SETENV_TARGET} ${SETENV_TARGET_ARCH}"
WMAKE="make -C /usr/src ${WORLD_FLAGS}"
NWMAKE="${WMAKE} __MAKE_CONF=/dev/null SRCCONF=/dev/null"
KMAKE="make -C /usr/src ${KERNEL_FLAGS}"
RMAKE="make -C /usr/src/release"

if [ $(id -u) -ne 0 ]; then
	echo "Needs to be run as root."
	exit 1
fi

set -e # Everything must succeed

mkdir -p ${CHROOTDIR}/usr/src
${SVN_CMD} co ${SVNROOT}/${BRANCHSRC} ${CHROOTDIR}/usr/src
${SVN_CMD} co ${SVNROOTPORTS}/${BRANCHPORTS} ${CHROOTDIR}/usr/mports

${SETENV} ${NWMAKE} -C ${CHROOTDIR}/usr/src ${WORLD_FLAGS} buildworld
${SETENV} ${NWMAKE} -C ${CHROOTDIR}/usr/src installworld distribution DESTDIR=${CHROOTDIR}
mount -t devfs devfs ${CHROOTDIR}/dev
trap "umount ${CHROOTDIR}/dev" EXIT # Clean up devfs mount on exit

${CHROOT_CMD} ${SETENV} ${CROSSENV} ${WMAKE} buildworld
${CHROOT_CMD} ${SETENV} ${CROSSENV} ${KMAKE} buildkernel
${CHROOT_CMD} ${SETENV} ${CROSSENV} ${RMAKE} release
${CHROOT_CMD} ${SETENV} ${CROSSENV} ${RMAKE} install DESTDIR=/R
