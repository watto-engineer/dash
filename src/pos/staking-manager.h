// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STAKING_CLIENT_H
#define STAKING_CLIENT_H

#include "amount.h"
#include "script/script.h"
#include "sync.h"

#include <univalue.h>

class CBlockIndex;
class CConnman;
class CMutableTransaction;
class CStakeInput;
class CStakingManager;
class CWallet;

extern std::shared_ptr<CStakingManager> stakingManager;

class CStakingManager
{
public:
    CCriticalSection cs;

private:
    const CBlockIndex* tipIndex{nullptr};
    CWallet* pwallet = nullptr;

    int64_t nMintableLastCheck;
    bool fMintableCoins;
    unsigned int nExtraNonce;

public:
    CStakingManager(CWallet * const pwalletIn = nullptr);

    bool fEnableStaking;
    bool fEnableBYTZStaking;
    CAmount nReserveBalance;

    bool MintableCoins();
    bool SelectStakeCoins(std::list<std::unique_ptr<CStakeInput> >& listInputs, CAmount nTargetAmount, int blockHeight);
    bool CreateCoinStake(const CBlockIndex* pindexPrev, std::shared_ptr<CMutableTransaction>& coinstakeTx, std::shared_ptr<CStakeInput>& coinstakeInput, unsigned int& nTxNewTime);

    void UpdatedBlockTip(const CBlockIndex* pindex);

    void DoMaintenance(CConnman& connman);
};

#endif // STAKING_CLIENT_H
