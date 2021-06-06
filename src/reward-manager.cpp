// Copyright (c) 2014-2020 The Ion Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "reward-manager.h"

#include "init.h"
#include "masternode/masternode-sync.h"
#include "policy/policy.h"
#include "validation.h"
#include "wallet/wallet.h"

// fix windows build
#include <boost/thread.hpp>

std::shared_ptr<CRewardManager> rewardManager;

CRewardManager::CRewardManager() :
        fEnableRewardManager(false), nAutoCombineNThreshold(10) {
}

bool CRewardManager::IsReady() {
    if (!fEnableRewardManager) return false;

    if (pwallet == nullptr || pwallet->IsLocked()) {
        return false;
    }
    bool fHaveConnections = !g_connman ? false : g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) > 0;
    if (!fHaveConnections || !masternodeSync.IsSynced()) {
        return false;
    }
    const CBlockIndex* tip = chainActive.Tip();
    if (tip == nullptr || tip->nTime < (GetAdjustedTime() - 300)) {
        return false;
    }
    return true;
}

bool CRewardManager::IsAutoCombineEnabled()
{
    bool fEnable;
    CAmount nAutoCombineAmountThreshold;
    pwallet->GetAutoCombineSettings(fEnable, nAutoCombineAmountThreshold);
    return fEnable;
}

CAmount CRewardManager::GetAutoCombineThresholdAmount()
{
    bool fEnable;
    CAmount nAutoCombineAmountThreshold;
    pwallet->GetAutoCombineSettings(fEnable, nAutoCombineAmountThreshold);
    return nAutoCombineAmountThreshold;
}

bool CRewardManager::IsCombining()
{
    return IsReady() && IsAutoCombineEnabled();
}

// TODO: replace with pwallet->FilterCoins()
std::map<CTxDestination, std::vector<COutput> > CRewardManager::AvailableCoinsByAddress(bool fConfirmed, CAmount maxCoinValue) {
    std::vector<COutput> vCoins;
    pwallet->AvailableCoins(vCoins, fConfirmed);

    std::map<CTxDestination, std::vector<COutput> > mapCoins;
    for (COutput out : vCoins) {
        if (maxCoinValue > 0 && out.tx->tx->vout[out.i].nValue > maxCoinValue)
            continue;

        CTxDestination address;
        if (!ExtractDestination(out.tx->tx->vout[out.i].scriptPubKey, address))
            continue;

        mapCoins[address].push_back(out);
    }

    return mapCoins;
}

void CRewardManager::AutocombineDust() {
    bool fEnable;
    CAmount nAutoCombineAmountThreshold;
    pwallet->GetAutoCombineSettings(fEnable, nAutoCombineAmountThreshold);
    if (!fEnable) return;

    std::map<CTxDestination, std::vector<COutput> > mapCoinsByAddress = AvailableCoinsByAddress(true, nAutoCombineAmountThreshold * COIN);

    //coins are sectioned by address. This combination code only wants to combine inputs that belong to the same address
    for (std::map<CTxDestination, std::vector<COutput> >::iterator it = mapCoinsByAddress.begin(); it != mapCoinsByAddress.end(); it++) {
        bool maxSize = false;
        std::vector<COutput> vRewardCoins;
        std::vector<COutput> vCoins(it->second);

        std::sort(vCoins.begin(), vCoins.end(), [](const COutput& a, const COutput& b) {
            return a.GetValue() < b.GetValue();
        });

        // We don't want the tx to be refused for being too large
        // we use 50 bytes as a base tx size (2 output: 2*34 + overhead: 10 -> 90 to be certain)
        unsigned int txSizeEstimate = 90;

        //find masternode rewards that need to be combined
        CCoinControl coinControl;
        CAmount nTotalRewardsValue = 0;
        for (const COutput& out : vCoins) {
            if (!out.fSpendable)
                continue;

            COutPoint outpt(out.tx->GetHash(), out.i);
            coinControl.Select(outpt);
            vRewardCoins.push_back(out);
            nTotalRewardsValue += out.GetValue();

            // Combine to the threshold and not way above
            if (nTotalRewardsValue > nAutoCombineAmountThreshold * COIN)
                break;

            // Around 180 bytes per input. We use 190 to be certain
            txSizeEstimate += 190;
            if (txSizeEstimate >= MAX_STANDARD_TX_SIZE - 200) {
                maxSize = true;
                break;
            }
        }

        //if no inputs found then return
        if (!coinControl.HasSelected())
            continue;

        //we cannot combine one coin with itself
        if (vRewardCoins.size() <= 1)
            continue;

        //we want at least N inputs to combine
        if (vRewardCoins.size() <= nAutoCombineNThreshold)
            continue;

        std::vector<CRecipient> vecSend;
        int nChangePosRet = -1;
        CScript scriptPubKey = GetScriptForDestination(it->first);
        // Subtract fee from amount
        CRecipient recipient = {scriptPubKey, nTotalRewardsValue, true};
        vecSend.push_back(recipient);

        //Send change to same address
        CTxDestination destMyAddress;
        if (!ExtractDestination(scriptPubKey, destMyAddress)) {
            LogPrintf("AutoCombineDust: failed to extract destination\n");
            continue;
        }
        coinControl.destChange = destMyAddress;

        // Create the transaction and commit it to the network
        CTransactionRef tx;
        CReserveKey keyChange(pwallet); // this change address does not end up being used, because change is returned with coin control switch
        std::string strErr;
        CAmount nFeeRet = 0;

        if (!pwallet->CreateTransaction(vecSend, tx, keyChange, nFeeRet, nChangePosRet, strErr, coinControl)) {
            LogPrintf("AutoCombineDust createtransaction failed, reason: %s\n", strErr);
            continue;
        }

        //we don't combine below the threshold unless the fees are 0 to avoid paying fees over fees over fees
        if (!maxSize && nTotalRewardsValue < nAutoCombineAmountThreshold * COIN && nFeeRet > 0)
            continue;

        CValidationState state;
        if (!pwallet->CommitTransaction(tx, {}, {}, {}, keyChange, g_connman.get(), state)) {
            LogPrintf("AutoCombineDust transaction commit failed\n");
            continue;
        }

        LogPrintf("AutoCombineDust sent transaction\n");
        // Max one transaction per cycle
        break;
    }
}

void CRewardManager::DoMaintenance(CConnman& connman) {
    if (!IsReady()) {
        MilliSleep(5 * 60 * 1000); // Wait 5 minutes
        return;
    }

    if (IsAutoCombineEnabled()) {
        AutocombineDust();
        int randsleep = GetRandInt(5 * 60 * 1000);
        MilliSleep(randsleep); // Sleep between 3 and 8 minutes
    }
}
