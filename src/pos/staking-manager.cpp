// Copyright (c) 2014-2019 The ION Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "staking-manager.h"

#include "init.h"
#include "miner.h"
#include "net.h"
#include "policy/policy.h"
#include "pos/blocksignature.h"
#include "pos/kernel.h"
#include "pos/stakeinput.h"
#include "script/sign.h"
#include "validation.h"
#include "wallet/wallet.h"

std::shared_ptr<CStakingManager> stakingManager;

CStakingManager::CStakingManager(std::shared_ptr<CWallet> pwalletIn) :
        nMintableLastCheck(0), fMintableCoins(false), nExtraNonce(0), // Currently unused
        fEnableStaking(false), fEnableBYTZStaking(false), nReserveBalance(0), pwallet(pwalletIn) {}

bool CStakingManager::MintableCoins()
{
    if (pwallet == nullptr) return false;

    LOCK2(cs_main, pwallet->cs_wallet);

    int blockHeight = chainActive.Height();

    std::vector<COutput> vCoins;
    CCoinControl coin_control;
    coin_control.nCoinType = CoinType::STAKABLE_COINS;
    int nMinDepth = blockHeight >= Params().GetConsensus().nBlockStakeModifierV2 ? Params().GetConsensus().nStakeMinDepth : 1;
    pwallet->AvailableCoins(vCoins, true, &coin_control, 1, MAX_MONEY, MAX_MONEY, 0, nMinDepth);
    CAmount nAmountSelected = 0;

    for (const COutput &out : vCoins) {
        if (out.tx->tx->vin[0].IsZerocoinSpend() && !out.tx->IsInMainChain())
            continue;

        CBlockIndex* utxoBlock = mapBlockIndex.at(out.tx->hashBlock);
        //check for maturity (min age/depth)
        if (HasStakeMinAgeOrDepth(blockHeight, GetAdjustedTime(), utxoBlock->nHeight, utxoBlock->GetBlockTime()))
            return true;
    }
    return false;
}

bool CStakingManager::SelectStakeCoins(std::list<std::unique_ptr<CStakeInput> >& listInputs, CAmount nTargetAmount, int blockHeight)
{
    if (pwallet == nullptr) return false;

    LOCK2(cs_main, pwallet->cs_wallet);
    //Add BYTZ
    std::vector<COutput> vCoins;
    CCoinControl coin_control;
    coin_control.nCoinType = CoinType::STAKABLE_COINS;
    int nMinDepth = blockHeight >= Params().GetConsensus().nBlockStakeModifierV2 ? Params().GetConsensus().nStakeMinDepth : 1;
    pwallet->AvailableCoins(vCoins, true, &coin_control, 1, MAX_MONEY, MAX_MONEY, 0, nMinDepth);
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
    if (!SelectStakeCoins(listInputs, nBalance - nReserveBalance, pindexPrev->nHeight + 1)) {
        LogPrint(BCLog::STAKING, "CreateCoinStake(): selectStakeCoins failed\n");
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
            LogPrint(BCLog::STAKING, "CreateCoinStake : kernel found\n");

            // Stake output value is set to stake input value.
            // Adding stake rewards and potentially splitting outputs is performed in BlockAssembler::CreateNewBlock()
            if (!stakeInput->CreateTxOuts(pwallet, coinstakeTx->vout, stakeInput->GetValue())) {
                LogPrint(BCLog::STAKING, "%s : failed to get scriptPubKey\n", __func__);
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
                    LogPrint(BCLog::STAKING, "%s : failed to create TxIn\n", __func__);
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
    if (pwallet->IsLocked(true)) return;

    const Consensus::Params& params = Params().GetConsensus();

    CBlockIndex* pindexPrev = chainActive.Tip();
    int nStakeHeight = pindexPrev->nHeight;
    if (!pindexPrev)
        return;

    // Check block height
    const bool fPosPhase = (nStakeHeight >= params.nPosStartHeight);
    if (!fPosPhase) {
        // no POS for at least 1 block
        return;
    }

    //control the amount of times the client will check for mintable coins
    if (!MintableCoins()) {
        // No mintable coins
        return;
    }

    // Create new block
    std::shared_ptr<CMutableTransaction> coinstakeTxPtr = std::shared_ptr<CMutableTransaction>(new CMutableTransaction);
    std::shared_ptr<CStakeInput> coinstakeInputPtr = nullptr;
    std::unique_ptr<CBlockTemplate> pblocktemplate = nullptr;
    unsigned int nCoinStakeTime;
    if (CreateCoinStake(chainActive.Tip(), coinstakeTxPtr, coinstakeInputPtr, nCoinStakeTime)) {
        // Coinstake found. Extract signing key from coinstake
        try {
            pblocktemplate = BlockAssembler(Params()).CreateNewBlock(CScript(), coinstakeTxPtr, coinstakeInputPtr, nCoinStakeTime);
        } catch (const std::exception& e) {
            LogPrint(BCLog::STAKING, "%s: error creating block - %s", __func__, e.what());
            return;
        }
    } else {
        return;
    }

    if (!pblocktemplate.get())
        return;
    CBlock *pblock = &pblocktemplate->block;

    // Sign block
    CKeyID keyID;
    if (!GetKeyIDFromUTXO(pblock->vtx[1]->vout[1], keyID)) {
        LogPrint(BCLog::STAKING, "%s: failed to find key for PoS", __func__);
        return;
    }
    CKey key;
    if (!pwallet->GetKey(keyID, key)) {
        LogPrint(BCLog::STAKING, "%s: failed to get key from keystore", __func__);
        return;
    }
    if (!key.Sign(pblock->GetHash(), pblock->vchBlockSig)) {
        LogPrint(BCLog::STAKING, "%s: failed to sign block hash with key", __func__);
        return;
    }

    /// Process block
    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
    if (!ProcessNewBlock(Params(), shared_pblock, true, nullptr)) {
        LogPrint(BCLog::STAKING, "%s: ProcessNewBlock, block not accepted", __func__);
    }
}
