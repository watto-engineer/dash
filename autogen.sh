#!/bin/sh
# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C
set -e
srcdir="$(dirname $0)"
cd "$srcdir"
if [ -z ${LIBTOOLIZE} ] && GLIBTOOLIZE="`which glibtoolize 2>/dev/null`"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi
which autoreconf >/dev/null || \
  (echo "configuration failed, please install autoconf first" && exit 1)
autoreconf --install --force --warnings=all
# get recent config.sub and config.guess for arm64-apple
cd build-aux
/bin/rm config.sub
/bin/rm config.guess
curl 'https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=4550d2f15b3a7ce2451c1f29500b9339430c877f' -o config.sub
curl 'https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=4550d2f15b3a7ce2451c1f29500b9339430c877f' -o config.guess
cd -
cp build-aux/config.sub src/univalue/build-aux
cp build-aux/config.guess src/univalue/build-aux
cp build-aux/config.sub src/secp256k1/build-aux
cp build-aux/config.guess src/secp256k1/build-aux
