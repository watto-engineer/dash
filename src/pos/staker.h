// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Copyright (c) 2019-2020 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef POS_STAKER_H
#define POS_STAKER_H

#include "script/script.h"

#include <univalue.h>

class ChainstateManager;
class CWallet;
class CBlockIndex;
class CMutableTransaction;
class ReserveDestination;
class CTxMemPool;

/** Generate mixed POS/POW blocks (mine or stake) */
UniValue generateHybridBlocks(ChainstateManager& chainman, const CTxMemPool& mempool, std::shared_ptr<CReserveScript> coinbase_script, int nGenerate, uint64_t nMaxTries, bool keepScript, CWallet * const pwallet = nullptr);

#endif // POS_STAKER_H
