// Copyright (c) 2020 The ION Core developers
// Copyright (c) 2021 The Bytz Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pos/rewards.h"

#include "chainparams.h"
#include <evo/cbtx.h>
#include <evo/specialtx.h>
#include "tokens/tokengroupmanager.h"

CAmount GetBlockSubsidyBytz(const int nPrevHeight, const bool fPos, const Consensus::Params& consensusParams)
{
    CAmount nSubsidy = 0;
    int nHeight = nPrevHeight + 1;

    if (nHeight >= consensusParams.V16DeploymentHeight + 4 * 513000) return 260 * COIN;
    if (nHeight >= consensusParams.V16DeploymentHeight + 3 * 513000) return 520 * COIN;
    if (nHeight >= consensusParams.V16DeploymentHeight + 2 * 513000) return 770 * COIN;
    if (nHeight >= consensusParams.V16DeploymentHeight + 513000) return 1000 * COIN;
    if (nHeight >= consensusParams.V16DeploymentHeight) return 1210 * COIN;
    if (nHeight >= 107624) return 2710 * COIN;
    if (nHeight >= 67004) return 0 * COIN;
    if (nHeight >= 67001) return 2850000000 * COIN;
    if (nHeight >= 312) return 0 * COIN;
    if (nHeight >= 12) return 10000 * COIN;
    if (nHeight >= 2) return 0 * COIN;
    if (nHeight == 1) return 947000000 * COIN;
    return 0 * COIN;
}

CAmount GetMasternodePayment(int nHeight, CAmount blockValue, bool isZBYTZStake, const Consensus::Params& consensusParams) {
    if (nHeight >= consensusParams.V16DeploymentHeight + 4 * 513000) return 156 * COIN;
    if (nHeight >= consensusParams.V16DeploymentHeight + 3 * 513000) return 312 * COIN;
    if (nHeight >= consensusParams.V16DeploymentHeight + 2 * 513000) return 462 * COIN;
    if (nHeight >= consensusParams.V16DeploymentHeight + 513000) return 600 * COIN;
    if (nHeight >= consensusParams.V16DeploymentHeight) return 726 * COIN;
    if (nHeight >= consensusParams.nBlockZerocoinV2 && !isZBYTZStake) return blockValue > 2440 * COIN ? 2440 * COIN : 0;
    if (nHeight >= consensusParams.nBlockZerocoinV2 && isZBYTZStake) return blockValue > 2410 * COIN ? 2410 * COIN : 0;
    if (nHeight >= consensusParams.nPosStartHeight) return blockValue > 2440 * COIN ? 2440 * COIN : 0;
    if (Params().NetworkIDString() == CBaseChainParams::REGTEST) return 0.4 * blockValue;
    return 0 * COIN;
}

CReward::CReward(const RewardType typeIn, const CAmount amountIn, const CTokenGroupID group, const CAmount tokenAmount) : type(typeIn), amount(amountIn) {
    if (group != NoGroup && tokenAmount != 0) {
        tokenAmounts.insert(std::pair<CTokenGroupID, CAmount>(group, tokenAmount));
    }
};
CReward::CReward(const RewardType typeIn, const CTxOut& out) {
    type = typeIn;
    amount = 0;

    CTokenGroupInfo tokenGrp(out.scriptPubKey);
    if ((tokenGrp.associatedGroup != NoGroup)) {
        AddRewardAmounts(out.nValue, tokenGrp.associatedGroup, tokenGrp.getAmount());
    } else {
        AddRewardAmounts(out.nValue);
    }
}
CReward& CReward::operator+=(const CReward& b) {
    AddRewardAmounts(b.amount);
    std::map<CTokenGroupID, CAmount>::const_iterator it = b.tokenAmounts.begin();
    while (it != b.tokenAmounts.end()) {
        AddRewardAmounts(0, it->first, it->second);
        it++;
    }
    return *this;
}

int CReward::CompareTo(const CReward& rhs) const {
    if (amount == rhs.amount && tokenAmounts == rhs.tokenAmounts) {
        return 0;
    } else if ((amount > rhs.amount || tokenAmounts > rhs.tokenAmounts)) {
        return 1;
    }
    return -1;
}

void CReward::AddRewardAmounts(const CAmount amountIn, const CTokenGroupID group, const CAmount tokenAmount) {
    amount += amountIn;
    if (group != NoGroup && tokenAmount != 0) {
        tokenAmounts[group] += tokenAmount;
    }
}

CBlockReward::CBlockReward(const CBlock& block, const bool fLegacy, const CAmount coinstakeValueIn) {
    rewards.clear();
    fBurnUnpaidMasternodeReward = false;
    fSplitCoinstake = false;

    if (block.IsProofOfStake()) {
        fPos = true;
        const int coinbaseVoutSize = block.vtx[0]->vout.size();
        const int coinstakeVoutSize = block.vtx[1]->vout.size();

        for (auto& out : block.vtx[0]->vout) {
            AddReward(CReward(CReward::REWARD_COINBASE, out));
        }
        if (fLegacy) {
            if (coinstakeVoutSize > 1) {
                AddReward(CReward(CReward::REWARD_COINSTAKE, block.vtx[1]->vout[1]));
                for (int i = 2; i < coinstakeVoutSize; i++) {
                    AddReward(CReward(CReward::REWARD_MASTERNODE, block.vtx[1]->vout[i]));
                }
            }
        } else {
            CCbTx cbTx;
            int nTx = 0;
            GetTxPayload(*block.vtx[0], cbTx);

            bool fCarbonOffset;
            bool fMasternodeTx;
            bool fOperatorTx;
            GetCbTxCoinstakeFlags(cbTx.coinstakeFlags, fPos, fSplitCoinstake, fCarbonOffset, fMasternodeTx, fOperatorTx);
            assert(CheckCoinstakeOutputs(block, fPos, fSplitCoinstake, fCarbonOffset, fMasternodeTx, fOperatorTx));

            nTx++;
            int nOutput = 1;
            AddReward(CReward(CReward::REWARD_COINSTAKE, block.vtx[nTx]->vout[nOutput]));
            if (fSplitCoinstake) {
                AddReward(CReward(CReward::REWARD_COINSTAKE, block.vtx[nTx]->vout[++nOutput]));
                fSplitCoinstake = true;
            }
            if (fCarbonOffset) AddReward(CReward(CReward::REWARD_CARBON, block.vtx[nTx]->vout[++nOutput]));
            if (fMasternodeTx) AddReward(CReward(CReward::REWARD_MASTERNODE, block.vtx[nTx]->vout[++nOutput]));
            if (fOperatorTx) AddReward(CReward(CReward::REWARD_OPERATOR, block.vtx[nTx]->vout[++nOutput]));
        }
    } else {
        if (fLegacy) {
            fPos = false;
            const int coinbaseVoutSize = block.vtx[0]->vout.size();
            AddReward(CReward(CReward::REWARD_COINBASE, block.vtx[0]->vout[0]));

            if (coinbaseVoutSize == 2) {
                AddReward(CReward(CReward::REWARD_MASTERNODE, block.vtx[0]->vout[coinbaseVoutSize-1]));
            } else if (coinbaseVoutSize == 3) {
                AddReward(CReward(CReward::REWARD_CARBON, block.vtx[0]->vout[coinbaseVoutSize-2]));
                AddReward(CReward(CReward::REWARD_MASTERNODE, block.vtx[0]->vout[coinbaseVoutSize-1]));
            } else if (coinbaseVoutSize > 3) {
                AddReward(CReward(CReward::REWARD_CARBON, block.vtx[0]->vout[1]));
                AddReward(CReward(CReward::REWARD_MASTERNODE, block.vtx[0]->vout[2]));
                for (int i = 3; i < coinbaseVoutSize; i++) {
                    AddReward(CReward(CReward::REWARD_MASTERNODE, block.vtx[0]->vout[i]));
                }
            }
        } else {
            CCbTx cbTx;
            int nTx = 0;
            GetTxPayload(*block.vtx[nTx], cbTx);

            bool fCarbonOffset;
            bool fMasternodeTx;
            bool fOperatorTx;
            GetCbTxCoinstakeFlags(cbTx.coinstakeFlags, fPos, fSplitCoinstake, fCarbonOffset, fMasternodeTx, fOperatorTx);
            assert(CheckCoinstakeOutputs(block, fPos, fSplitCoinstake, fCarbonOffset, fMasternodeTx, fOperatorTx));

            int nOutput = 0;
            AddReward(CReward(CReward::REWARD_COINBASE, block.vtx[nTx]->vout[nOutput]));
            if (fCarbonOffset) AddReward(CReward(CReward::REWARD_CARBON, block.vtx[nTx]->vout[++nOutput]));
            if (fMasternodeTx) AddReward(CReward(CReward::REWARD_MASTERNODE, block.vtx[nTx]->vout[++nOutput]));
            if (fOperatorTx) AddReward(CReward(CReward::REWARD_OPERATOR, block.vtx[nTx]->vout[++nOutput]));
        }
    }
}

// fSplitCoinstake is not set after calling this constructor
CBlockReward::CBlockReward(const int nHeight, const CAmount nFees, const bool fPosIn, const Consensus::Params& consensusParams) {
    rewards.clear();
    fBurnUnpaidMasternodeReward = false;
    fPos = fPosIn;
    fSplitCoinstake = false;
    CAmount nBlockValue = GetBlockSubsidyBytz(nHeight - 1, fPos, consensusParams);
    CAmount mnRewardAmount = GetMasternodePayment(nHeight, nBlockValue, false, consensusParams);
    SetRewards(nBlockValue, mnRewardAmount, 0, nFees, nHeight < consensusParams.DIP0003Height, fPos);
}

int CBlockReward::CompareTo(const CBlockReward& rhs) const {
    CReward lhsTotalReward(CReward::RewardType::REWARD_TOTAL);
    std::map<CReward::RewardType, CReward>::const_iterator it = rewards.begin();
    while (it != rewards.end()) {
        lhsTotalReward += it->second;
        it++;
    }

    CReward rhsTotalReward(CReward::RewardType::REWARD_TOTAL);
    it = rhs.rewards.begin();
    while (it != rhs.rewards.end()) {
        rhsTotalReward += it->second;
        it++;
    }

    if (lhsTotalReward < rhsTotalReward) {
        return -1;
    } else if (lhsTotalReward > rhsTotalReward) {
        return 1;
    }
    return 0;
}

CReward CBlockReward::GetTotalRewards() {
    CReward totalRewards(CReward::REWARD_TOTAL);
    totalRewards += GetCoinbaseReward();
    totalRewards += GetCoinstakeReward();
    totalRewards += GetCarbonReward();
    totalRewards += GetMasternodeReward();
    totalRewards += GetOperatorReward();
    return totalRewards;
}

bool CBlockReward::SetReward(CReward &reward, const CAmount amount, const CTokenGroupID tokenID, const CAmount tokenAmount) {
    if (tokenID != NoGroup && tokenAmount != 0) {
        reward = CReward(reward.type, amount, tokenID, tokenAmount);
        return true;
    } else {
        reward = CReward(reward.type, amount);
        return false;
    }
}

void CBlockReward::AddReward(const CReward::RewardType rewardType, const CAmount amount, const CTokenGroupID tokenID, const CAmount tokenAmount) {
    CReward additionalReward(rewardType, amount, tokenID, tokenAmount);
    CReward reward = GetReward(rewardType);
    reward += additionalReward;
    rewards[rewardType] = reward;
}

void CBlockReward::AddReward(const CReward rewardIn) {
    auto r_it = rewards.find(rewardIn.type);
    if (r_it != rewards.end()) {
        r_it->second += rewardIn;
    } else {
        rewards.insert(std::pair<CReward::RewardType, CReward>(rewardIn.type, rewardIn));
    }
}

CReward CBlockReward::GetReward(CReward::RewardType rewardType) {
    auto r_it = rewards.find(rewardType);
    if (r_it != rewards.end()) {
        return r_it->second;
    } else {
        rewards[rewardType] = CReward(rewardType);
        return rewards[rewardType];
    }
}
CReward CBlockReward::GetCoinbaseReward() {
    return GetReward(CReward::REWARD_COINBASE);
}
CReward CBlockReward::GetCoinstakeReward() {
    return GetReward(CReward::REWARD_COINSTAKE);
}
CReward CBlockReward::GetCarbonReward() {
    return GetReward(CReward::REWARD_CARBON);
}
CReward CBlockReward::GetMasternodeReward() {
    return GetReward(CReward::REWARD_MASTERNODE);
}
CReward CBlockReward::GetOperatorReward() {
    return GetReward(CReward::REWARD_OPERATOR);
}

void CBlockReward::SetCoinbaseReward(const CAmount amount, const CTokenGroupID tokenID, const CAmount tokenAmount) {
    CReward coinbaseReward(CReward::REWARD_COINBASE);
    SetReward(coinbaseReward, amount, tokenID, tokenAmount);
    rewards[CReward::REWARD_COINBASE] = coinbaseReward;
}
void CBlockReward::SetCoinstakeReward(const CAmount amount, const CTokenGroupID tokenID, const CAmount tokenAmount) {
    CReward coinstakeReward(CReward::REWARD_COINSTAKE);
    SetReward(coinstakeReward, amount, tokenID, tokenAmount);
    rewards[CReward::REWARD_COINSTAKE] = coinstakeReward;
}
void CBlockReward::SetCarbonReward(const CAmount amount, const CTokenGroupID tokenID, const CAmount tokenAmount) {
    CReward carbonReward(CReward::REWARD_COINSTAKE);
    SetReward(carbonReward, amount, tokenID, tokenAmount);
    rewards[CReward::REWARD_CARBON] = carbonReward;
}
void CBlockReward::SetMasternodeReward(const CAmount amount, const CTokenGroupID tokenID, const CAmount tokenAmount) {
    CReward masternodeReward(CReward::REWARD_MASTERNODE);
    SetReward(masternodeReward, amount, tokenID, tokenAmount);
    rewards[CReward::REWARD_MASTERNODE] = masternodeReward;
}
void CBlockReward::SetOperatorReward(const CAmount amount, const CTokenGroupID tokenID, const CAmount tokenAmount) {
    CReward operatorReward(CReward::REWARD_OPERATOR);
    SetReward(operatorReward, amount, tokenID, tokenAmount);
    rewards[CReward::REWARD_OPERATOR] = operatorReward;
}

void CBlockReward::MoveMasternodeRewardToCoinbase() {
    auto r_it = rewards.find(CReward::RewardType::REWARD_MASTERNODE);
    if (r_it != rewards.end()) {
        CReward masternodeReward(r_it->second);
        masternodeReward.type = CReward::RewardType::REWARD_COINBASE;
        AddReward(masternodeReward);
        rewards.erase(r_it);
    }
    r_it = rewards.find(CReward::RewardType::REWARD_OPERATOR);
    if (r_it != rewards.end()) {
        CReward operatorReward(r_it->second);
        operatorReward.type = CReward::RewardType::REWARD_COINBASE;
        AddReward(operatorReward);
        rewards.erase(r_it);
    }
}

void CBlockReward::MoveMasternodeRewardToCoinstake() {
    auto r_it = rewards.find(CReward::RewardType::REWARD_MASTERNODE);
    if (r_it != rewards.end()) {
        CReward masternodeReward(r_it->second);
        masternodeReward.type = CReward::RewardType::REWARD_COINSTAKE;
        AddReward(masternodeReward);
        rewards.erase(r_it);
    }
    r_it = rewards.find(CReward::RewardType::REWARD_OPERATOR);
    if (r_it != rewards.end()) {
        CReward operatorReward(r_it->second);
        operatorReward.type = CReward::RewardType::REWARD_COINSTAKE;
        AddReward(operatorReward);
        rewards.erase(r_it);
    }
}

void CBlockReward::RemoveMasternodeReward() {
    auto r_it = rewards.find(CReward::RewardType::REWARD_MASTERNODE);
    if (r_it != rewards.end()) {
        rewards.erase(r_it);
    }
}

void CBlockReward::AddFees(const CAmount nFees) {
    CAmount nCarbonFees = nFees * 0.5;
    AddReward(CReward::RewardType::REWARD_CARBON, nCarbonFees);
    AddReward(CReward::RewardType::REWARD_BURN, nFees - nCarbonFees);
}

void CBlockReward::SetRewards(const CAmount blockSubsidy, const CAmount mnRewardAmount, const CAmount opRewardAmount, const CAmount nFees, const bool fLegacy, const bool fPOS) {
    if (!fLegacy) {
        SetMasternodeReward(mnRewardAmount);
        SetOperatorReward(opRewardAmount);
        if (fPOS) {
            SetCoinstakeReward(blockSubsidy - mnRewardAmount - opRewardAmount);
        } else {
            SetCoinbaseReward(blockSubsidy - mnRewardAmount - opRewardAmount);
        }
        AddFees(nFees);
    } else {
        SetMasternodeReward(mnRewardAmount);
        if (fPOS) {
            SetCoinstakeReward(blockSubsidy - GetMasternodeReward().amount);
            AddReward(CReward::RewardType::REWARD_MASTERNODE, nFees);
        } else {
            SetCoinbaseReward(blockSubsidy - GetMasternodeReward().amount);
            AddReward(CReward::RewardType::REWARD_MASTERNODE, nFees);
        }
    }
}