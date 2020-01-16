// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef POS_STAKER_H
#define POS_STAKER_H

#include "script/script.h"

#include <univalue.h>

class CWallet;
class CBlockIndex;
class CMutableTransaction;
class CReserveKey;

/** Generate mixed POS/POW blocks (mine or stake) */
UniValue generateHybridBlocks(std::shared_ptr<CReserveKey> coinbaseKey, int nGenerate, uint64_t nMaxTries, bool keepScript, CWallet * const pwallet = nullptr);

#endif // POS_STAKER_H
