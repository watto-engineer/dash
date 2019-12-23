// Copyright (c) 2014-2019 The ION Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "staking-manager.h"

#include "init.h"
#include "net.h"
#include "policy/policy.h"
#include "pos/kernel.h"
#include "pos/stakeinput.h"
#include "script/sign.h"
#include "validation.h"
#include "wallet/wallet.h"

std::shared_ptr<CStakingManager> stakingManager;

CStakingManager::CStakingManager(CWallet * const pwalletIn) {
    this->pwallet = pwalletIn;
}

bool CStakingManager::SelectStakeCoins(CWallet * const pwallet, std::list<std::unique_ptr<CStakeInput> >& listInputs, CAmount nTargetAmount, int blockHeight)
{
    LOCK2(cs_main, pwallet->cs_wallet);
    //Add BYTZ
    std::vector<COutput> vCoins;
    CCoinControl coin_control;
    coin_control.nCoinType = CoinType::STAKABLE_COINS;
    int nMinDepth = blockHeight >= Params().GetConsensus().nBlockStakeModifierV2 ? Params().GetConsensus().nStakeMinDepth : 1;
    pwallet->AvailableCoins(vCoins, true, &coin_control, 1, MAX_MONEY, 0, nMinDepth);
    CAmount nAmountSelected = 0;

    if (fEnableBYTZStaking) {
        for (const COutput &out : vCoins) {
            //make sure not to outrun target amount
            if (nAmountSelected + out.tx->tx->vout[out.i].nValue > nTargetAmount)
                continue;

            if (out.tx->tx->vin[0].IsZerocoinSpend() && !out.tx->IsInMainChain())
                continue;

            CBlockIndex* utxoBlock = mapBlockIndex.at(out.tx->hashBlock);
            //check for maturity (min age/depth)
            if (!HasStakeMinAgeOrDepth(blockHeight, GetAdjustedTime(), utxoBlock->nHeight, utxoBlock->GetBlockTime()))
                continue;

            //add to our stake set
            nAmountSelected += out.tx->tx->vout[out.i].nValue;

            std::unique_ptr<CStake> input(new CStake());
            input->SetInput(out.tx->tx, out.i);
            listInputs.emplace_back(std::move(input));
        }
    }
    return true;
}

bool CStakingManager::CreateCoinStake(const CBlockIndex* pindexPrev, std::shared_ptr<CMutableTransaction>& coinstakeTx, std::shared_ptr<CStakeInput>& coinstakeInput, unsigned int& nTxNewTime) {
    // Needs wallet
    if (pwallet == nullptr || pindexPrev == nullptr)
        return false;

    coinstakeTx->vin.clear();
    coinstakeTx->vout.clear();

    // Mark coin stake transaction
    CScript scriptEmpty;
    scriptEmpty.clear();
    coinstakeTx->vout.push_back(CTxOut(0, scriptEmpty));

    // Choose coins to use
    CAmount nBalance = pwallet->GetBalance();

    if (nBalance > 0 && nBalance <= nReserveBalance)
        return false;

    // Get the list of stakable inputs
    std::list<std::unique_ptr<CStakeInput> > listInputs;
    if (!SelectStakeCoins(pwallet, listInputs, nBalance - nReserveBalance, pindexPrev->nHeight + 1)) {
        LogPrintf("CreateCoinStake(): selectStakeCoins failed\n");
        return false;
    }

    if (listInputs.empty()) {
        LogPrint(BCLog::STAKING, "CreateCoinStake(): listInputs empty\n");
//        MilliSleep(50000);
        return false;
    }

    if (GetAdjustedTime() - chainActive.Tip()->GetBlockTime() < 60) {
        if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
//            MilliSleep(1000);
        }
    }

    CScript scriptPubKeyKernel;
    bool fKernelFound = false;
    int nAttempts = 0;

    // Block time.
    nTxNewTime = GetAdjustedTime();
    // If the block time is in the future, then starts there.
    if (pindexPrev->nTime > nTxNewTime) {
        nTxNewTime = pindexPrev->nTime;
    }

    for (std::unique_ptr<CStakeInput>& stakeInput : listInputs) {
        // Make sure the wallet is unlocked and shutdown hasn't been requested
        if (pwallet->IsLocked(true) || ShutdownRequested())
            return false;

        uint256 hashProofOfStake = uint256();
        nAttempts++;
        //iterates each utxo inside of CheckStakeKernelHash()
        if (Stake(pindexPrev, stakeInput.get(), pindexPrev->nBits, nTxNewTime, hashProofOfStake)) {
            // Found a kernel
            LogPrintf("CreateCoinStake : kernel found\n");

            // add a zero value coinstake output
            // value will be updated later
            if (!stakeInput->CreateTxOuts(pwallet, coinstakeTx->vout, 0)) {
                LogPrintf("%s : failed to get scriptPubKey\n", __func__);
                return false;
            }

            // Limit size
            unsigned int nBytes = ::GetSerializeSize(*coinstakeTx, SER_NETWORK, CTransaction::CURRENT_VERSION);
            if (nBytes >= MAX_STANDARD_TX_SIZE)
                return error("CreateCoinStake : exceeded coinstake size limit");

            {
                uint256 hashTxOut = coinstakeTx->GetHash();
                CTxIn in;
                if (!stakeInput->CreateTxIn(pwallet, in, hashTxOut)) {
                    LogPrintf("%s : failed to create TxIn\n", __func__);
                    coinstakeTx->vin.clear();
                    coinstakeTx->vout.clear();
                    continue;
                }
                coinstakeTx->vin.emplace_back(in);
            }
            coinstakeInput = std::move(stakeInput);
            fKernelFound = true;
            break;
        }
    }
    LogPrint(BCLog::STAKING, "%s: attempted staking %d times\n", __func__, nAttempts);

    if (!fKernelFound)
        return false;

    // Successfully generated coinstake
    return true;
}

void CStakingManager::UpdatedBlockTip(const CBlockIndex* pindex)
{
    LOCK(cs);

    tipIndex = pindex;

    LogPrint(BCLog::STAKING, "CStakingManager::UpdatedBlockTip -- height: %d\n", pindex->nHeight);
}

void CStakingManager::DoMaintenance(CConnman& connman)
{
    if (!fEnableStaking) return;

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev)
        return;

    // Take structure from BitcoinMiner()
}
