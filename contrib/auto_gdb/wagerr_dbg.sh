#!/usr/bin/env bash
# use testnet settings,  if you need mainnet,  use ~/.wagerrcoin/wagerrd.pid file instead
export LC_ALL=C

wagerr_pid=$(<~/.wagerrcoin/testnet/wagerrd.pid)
sudo gdb -batch -ex "source debug.gdb" wagerrd ${wagerr_pid}
