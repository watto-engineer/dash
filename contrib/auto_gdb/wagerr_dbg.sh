#!/usr/bin/env bash
# use testnet settings,  if you need mainnet,  use ~/.wagerrcore/wagerrd.pid file instead
export LC_ALL=C

wagerr_pid=$(<~/.wagerrcore/testnet3/wagerrd.pid)
sudo gdb -batch -ex "source debug.gdb" wagerrd ${wagerr_pid}
