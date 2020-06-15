// Copyright (c) 2020 The Ion Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef REWARD_MANAGER_H
#define REWARD_MANAGER_H

#include "amount.h"
//#include "base58.h"
#include <script/standard.h>
#include "sync.h"

class CConnman;
class CRewardManager;
class COutput;
class CWallet;

extern std::shared_ptr<CRewardManager> rewardManager;

class CRewardManager
{
public:
    CCriticalSection cs;

private:
    CWallet* pwallet = nullptr;

public:
    CRewardManager();

    void BindWallet(CWallet * const pwalletIn) {
        pwallet = pwalletIn;
    }

    bool fEnableRewardManager;

    bool fEnableAutoCombineRewards;
    CAmount nAutoCombineAmountThreshold;
    uint32_t nAutoCombineNThreshold;

    bool IsReady();
    bool IsCombining();

    bool IsAutoCombineEnabled() { return fEnableAutoCombineRewards; };
    CAmount GetAutoCombineThresholdAmount() { return nAutoCombineAmountThreshold; };
    void AutoCombineSettings(bool fEnable, CAmount nAutoCombineAmountThresholdIn = 0);

    std::map<CTxDestination, std::vector<COutput> > AvailableCoinsByAddress(bool fConfirmed, CAmount maxCoinValue);
    void AutoCombineRewards();

    void DoMaintenance(CConnman& connman);
};

#endif // REWARD_MANAGER_H
