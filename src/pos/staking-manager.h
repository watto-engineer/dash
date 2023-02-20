// Copyright (c) 2018-2022 The Dash Core developers
// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef STAKING_CLIENT_H
#define STAKING_CLIENT_H

#include "amount.h"
#include "script/script.h"
#include "sync.h"

#include <univalue.h>

class CBlockIndex;
class ChainstateManager;
class CConnman;
class CMutableTransaction;
class CStakeInput;
class CStakingManager;
class CWallet;
class uint256;

extern std::shared_ptr<CStakingManager> stakingManager;

class CStakingManager
{
public:
    CCriticalSection cs;

private:
    const CBlockIndex* tipIndex{nullptr};
    std::shared_ptr<CWallet> pwallet = nullptr;

    int64_t nBackoffUntilTime;

    std::map<unsigned int, unsigned int> mapHashedBlocks;

    int64_t nMintableLastCheck;
    bool fMintableCoins;
    bool fLastLoopOrphan;
    int64_t nLastCoinStakeSearchInterval;
    int64_t nLastCoinStakeSearchTime;
    unsigned int nExtraNonce;
    const int64_t nHashInterval;

public:
    CStakingManager(std::shared_ptr<CWallet> pwalletIn = nullptr);

    bool fEnableStaking;
    bool fEnableWAGERRStaking;
    CAmount nReserveBalance;

    bool MintableCoins();
    bool SelectStakeCoins(std::list<std::unique_ptr<CStakeInput> >& listInputs, CAmount nTargetAmount, int blockHeight);
    bool CreateCoinStake(const CBlockIndex* pindexPrev, std::shared_ptr<CMutableTransaction>& coinstakeTx, std::shared_ptr<CStakeInput>& coinstakeInput, int64_t& nTxNewTime);
    bool Stake(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx, uint256& hashProofOfStake);
    bool IsStaking();

    void UpdatedBlockTip(const CBlockIndex* pindex);

    void DoMaintenance(CConnman& connman, ChainstateManager& chainman);
};

#endif // STAKING_CLIENT_H
