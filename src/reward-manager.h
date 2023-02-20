// Copyright (c) 2020 The Ion Core developers
// Copyright (c) 2022 The Wagerr developers
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

    int64_t nBackoffUntilTime;

public:
    CRewardManager();

    void BindWallet(CWallet * const pwalletIn) {
        pwallet = pwalletIn;
    }

    bool fEnableRewardManager;
    uint32_t nAutoCombineNThreshold;

    bool IsReady(CConnman& connman);

    bool IsAutoCombineEnabled();
    CAmount GetAutoCombineThresholdAmount();

    std::map<CTxDestination, std::vector<COutput> > AvailableCoinsByAddress(bool fConfirmed, CAmount maxCoinValue);
    void AutocombineDust();

    void DoMaintenance(CConnman& connman);
};

#endif // REWARD_MANAGER_H
