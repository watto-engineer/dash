// Copyright (c) 2020 The ION Core developers
// Copyright (c) 2018-2022 The Dash Core developers
// Copyright (c) 2014-2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef POS_REWARDS_H
#define POS_REWARDS_H

#include "amount.h"
#include "consensus/params.h"
#include "primitives/block.h"
#include "tokens/groups.h"

class CReward {
public:
    typedef enum RewardType_t {
        REWARD_COINBASE     = 0x01,
        REWARD_COINSTAKE    = 0x02,
        REWARD_MASTERNODE   = 0x03,
        REWARD_OPERATOR     = 0x04,
        REWARD_BETTING      = 0x05,

        REWARD_BURN         = 0x06,
        REWARD_TOTAL        = 0x07,
        REWARD_UNDEFINED    = 0x00
    } RewardType;

    RewardType type;
    CAmount amount;
    std::map<CTokenGroupID, CAmount> tokenAmounts;

    CReward() : type(RewardType::REWARD_UNDEFINED), amount(0) { };

    CReward(const RewardType typeIn) : type(typeIn), amount(0) { };
    CReward(const RewardType typeIn, const CAmount amountIn, const CTokenGroupID group = NoGroup, const CAmount tokenAmount = 0);
    CReward(const RewardType typeIn, const CTxOut& out);

    CReward& operator+=(const CReward& b);

    bool operator<(const CReward& rhs) const { return CompareTo(rhs) < 0; }
    bool operator>(const CReward& rhs) const { return CompareTo(rhs) > 0; }
    bool operator<=(const CReward& rhs) const { return CompareTo(rhs) <= 0; }
    bool operator>=(const CReward& rhs) const { return CompareTo(rhs) >= 0; }
    bool operator==(const CReward& rhs) const { return CompareTo(rhs) == 0; }
    bool operator!=(const CReward& rhs) const { return CompareTo(rhs) != 0; }

    int CompareTo(const CReward& rhs) const;

    void AddRewardAmounts(const CAmount amountIn, const CTokenGroupID group = NoGroup, const CAmount tokenAmount = 0);
};

class CBlockReward {
private:
    std::map<CReward::RewardType, CReward> rewards;

    bool SetReward(CReward &reward, const CAmount amount, const CTokenGroupID tokenID = NoGroup, const CAmount tokenAmount = 0);

public:
    bool fBurnUnpaidMasternodeReward;
    bool fPos; // PoS, not PoW
    bool fSplitCoinstake; // True if two staker rewards, false if one staker reward.

    CBlockReward() {
        fBurnUnpaidMasternodeReward = false;
        fSplitCoinstake = false;
    };

    CBlockReward(const CBlock& block, const bool fLegacy, const CAmount coinstakeValueIn);
    CBlockReward(const int nHeight, const CAmount nFees, const bool fPos, const Consensus::Params& consensusParams);

    bool operator<(const CBlockReward& rhs) const { return CompareTo(rhs) < 0; }
    bool operator>(const CBlockReward& rhs) const { return CompareTo(rhs) > 0; }
    bool operator<=(const CBlockReward& rhs) const { return CompareTo(rhs) <= 0; }
    bool operator>=(const CBlockReward& rhs) const { return CompareTo(rhs) >= 0; }
    bool operator==(const CBlockReward& rhs) const { return CompareTo(rhs) == 0; }
    bool operator!=(const CBlockReward& rhs) const { return CompareTo(rhs) != 0; }

    int CompareTo(const CBlockReward& rhs) const;

    void AddReward(const CReward::RewardType rewardType, const CAmount amount, const CTokenGroupID tokenID = NoGroup, const CAmount tokenAmount = 0);
    void AddReward(const CReward reward);

    CReward GetReward(CReward::RewardType rewardType);
    CReward GetCoinbaseReward();
    CReward GetCoinstakeReward();
    CReward GetMasternodeReward();
    CReward GetOperatorReward();

    void SetCoinbaseReward(const CAmount amount, const CTokenGroupID tokenID = NoGroup, const CAmount tokenAmount = 0);
    void SetCoinstakeReward(const CAmount amount, const CTokenGroupID tokenID = NoGroup, const CAmount tokenAmount = 0);
    void SetMasternodeReward(const CAmount amount, const CTokenGroupID tokenID = NoGroup, const CAmount tokenAmount = 0);
    void SetOperatorReward(const CAmount amount, const CTokenGroupID tokenID = NoGroup, const CAmount tokenAmount = 0);

    void MoveMasternodeRewardToCoinbase();
    void MoveMasternodeRewardToCoinstake();
    void RemoveMasternodeReward();

    void AddFees(const CAmount nFees);

    void SetRewards(const CAmount blockSubsidy, const CAmount mnRewardAmount, const CAmount opRewardAmount, const CAmount nFees, const bool fLegacy, const bool fPOS);

    CReward GetTotalRewards();
};

CAmount GetBlockSubsidyWagerr(const int nPrevHeight, const bool fPos, const Consensus::Params& consensusParams);

#endif //POS_REWARDS_H
