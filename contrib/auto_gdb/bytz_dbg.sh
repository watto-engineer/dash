#!/usr/bin/env bash
# use testnet settings,  if you need mainnet,  use ~/.bytzcoin/bytzd.pid file instead
export LC_ALL=C

bytz_pid=$(<~/.bytzcoin/testnet/bytzd.pid)
sudo gdb -batch -ex "source debug.gdb" bytzd ${bytz_pid}
