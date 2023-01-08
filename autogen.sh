#!/bin/sh
# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C
set -e
srcdir="$(dirname "$0")"
cd "$srcdir"
if [ -z "${LIBTOOLIZE}" ] && GLIBTOOLIZE="$(command -v glibtoolize)"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi
command -v autoreconf >/dev/null || \
  (echo "configuration failed, please install autoconf first" && exit 1)
autoreconf --install --force --warnings=all
# get recent config.sub and config.guess for arm64-apple
cp depends/config.sub build-aux
cp depends/config.guess build-aux
cp build-aux/config.sub src/univalue/build-aux
cp build-aux/config.guess src/univalue/build-aux
cp build-aux/config.sub src/secp256k1/build-aux
cp build-aux/config.guess src/secp256k1/build-aux
