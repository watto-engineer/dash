// Copyright (c) 2018-2022 The Dash Core developers
// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "staking-manager.h"

#include "init.h"
#include <governance/governance.h>
#include <llmq/blockprocessor.h>
#include <llmq/chainlocks.h>
#include <llmq/instantsend.h>
#include "masternode/sync.h"
#include "miner.h"
#include "net.h"
#include "policy/policy.h"
#include "pos/blocksignature.h"
#include "pos/kernel.h"
#include "pos/stakeinput.h"
#include "pow.h"
#include "script/sign.h"
#include "shutdown.h"
#include <spork.h>
#include "validation.h"
#include "wallet/wallet.h"

#include <boost/thread.hpp>

std::shared_ptr<CStakingManager> stakingManager;

CStakingManager::CStakingManager(std::shared_ptr<CWallet> pwalletIn) :
        nMintableLastCheck(0), fMintableCoins(false), fLastLoopOrphan(false), nExtraNonce(0), // Currently unused
        fEnableStaking(false), fEnableWAGERRStaking(false), nReserveBalance(0), pwallet(pwalletIn),
        nHashInterval(22), nLastCoinStakeSearchInterval(0), nLastCoinStakeSearchTime(GetAdjustedTime()) {}

bool CStakingManager::MintableCoins()
{
    if (pwallet == nullptr) return false;

    LOCK2(pwallet->cs_wallet, cs_main);

    int blockHeight = ::ChainActive().Height();

    std::vector<COutput> vCoins;
    CCoinControl coin_control;
    coin_control.nCoinType = CoinType::STAKABLE_COINS;
    int nMinDepth = blockHeight >= Params().GetConsensus().nBlockStakeModifierV2 ? Params().GetConsensus().nStakeMinDepth : 1;
    pwallet->AvailableCoins(vCoins, true, &coin_control, 1, MAX_MONEY, MAX_MONEY, 0, nMinDepth);
    CAmount nAmountSelected = 0;

    for (const COutput &out : vCoins) {
        if (out.tx->tx->vin[0].IsZerocoinSpend() && !out.tx->IsInMainChain())
            continue;

        CBlockIndex* utxoBlock = LookupBlockIndex(out.tx->m_confirm.hashBlock);
        if (!utxoBlock)
            return false;
        //check for maturity (min age/depth)
        if (HasStakeMinAgeOrDepth(blockHeight, GetAdjustedTime(), utxoBlock->nHeight, utxoBlock->GetBlockTime()))
            return true;
    }
    return false;
}

bool CStakingManager::SelectStakeCoins(std::list<std::unique_ptr<CStakeInput> >& listInputs, CAmount nTargetAmount, int blockHeight)
{
    if (pwallet == nullptr) return false;

    LOCK2(pwallet->cs_wallet, cs_main);
    //Add WAGERR
    std::vector<COutput> vCoins;
    CCoinControl coin_control;
    coin_control.nCoinType = CoinType::STAKABLE_COINS;
    int nMinDepth = blockHeight >= Params().GetConsensus().nBlockStakeModifierV2 ? Params().GetConsensus().nStakeMinDepth : 1;
    pwallet->AvailableCoins(vCoins, true, &coin_control, 1, MAX_MONEY, MAX_MONEY, 0, nMinDepth);
    CAmount nAmountSelected = 0;

    for (const COutput &out : vCoins) {
        //make sure not to outrun target amount
        if (nAmountSelected + out.tx->tx->vout[out.i].nValue > nTargetAmount)
            continue;

        if (out.tx->tx->vin[0].IsZerocoinSpend() && !out.tx->IsInMainChain())
            continue;

        CBlockIndex* utxoBlock = LookupBlockIndex(out.tx->m_confirm.hashBlock);
        if (!utxoBlock)
            continue;
        //check for maturity (min age/depth)
        if (!HasStakeMinAgeOrDepth(blockHeight, GetAdjustedTime(), utxoBlock->nHeight, utxoBlock->GetBlockTime()))
            continue;

        //add to our stake set
        nAmountSelected += out.tx->tx->vout[out.i].nValue;

        std::unique_ptr<CStake> input(new CStake());
        input->SetInput(out.tx->tx, out.i);
        listInputs.emplace_back(std::move(input));
    }
    return true;
}

bool CStakingManager::Stake(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx, uint256& hashProofOfStake)
{
    const int prevHeight = pindexPrev->nHeight;
    const int nHeight = pindexPrev->nHeight + 1;

    // get stake input pindex
    CBlockIndex* pindexFrom = stakeInput->GetIndexFrom();
    if (!pindexFrom || pindexFrom->nHeight < 1) return error("%s : no pindexfrom", __func__);

    const uint32_t nTimeBlockFrom = pindexFrom->nTime;
    const int nHeightBlockFrom = pindexFrom->nHeight;

    bool fSuccess = false;

    const Consensus::Params& params = Params().GetConsensus();
    if (params.IsTimeProtocolV2(nHeight)) {
        if (nHeight < nHeightBlockFrom + params.nStakeMinDepth)
            return error("%s : min depth violation, nHeight=%d, nHeightBlockFrom=%d", __func__, nHeight, nHeightBlockFrom);

        nTimeTx = GetTimeSlot(GetAdjustedTime());
        // double check that we are not on the same slot as prev block
        if (nTimeTx <= pindexPrev->nTime && Params().NetworkIDString() != CBaseChainParams::REGTEST)
            return false;

        // check stake kernel
        fSuccess = CheckStakeKernelHash(pindexPrev, nBits, stakeInput, nTimeTx, hashProofOfStake);
    } else {
        // iterate from maxTime down to pindexPrev->nTime (or min time due to maturity, 60 min after blockFrom)
        const unsigned int prevBlockTime = pindexPrev->nTime;
        const unsigned int maxTime = pindexPrev->MaxFutureBlockTime(GetAdjustedTime(), params);
        unsigned int minTime = std::max(prevBlockTime, nTimeBlockFrom + 3600);
        if (Params().NetworkIDString() == CBaseChainParams::REGTEST)
            minTime = prevBlockTime;
        unsigned int nTryTime = maxTime;

        if (maxTime <= minTime) {
            // too early to stake
            return false;
        }

        while (nTryTime > minTime)
        {
            //new block came in, move on
            if (::ChainActive().Height() != prevHeight)
                break;

            --nTryTime;

            // if stake hash does not meet the target then continue to next iteration
            if (!CheckStakeKernelHash(pindexPrev, nBits, stakeInput, nTryTime, hashProofOfStake))
                continue;

            // if we made it this far, then we have successfully found a valid kernel hash
            fSuccess = true;
            nTimeTx = nTryTime;
            break;
        }
    }

    mapHashedBlocks.clear();
    mapHashedBlocks[::ChainActive().Tip()->nHeight] = GetTime(); //store a time stamp of when we last hashed on this block
    return fSuccess;
}

bool CStakingManager::CreateCoinStake(const CBlockIndex* pindexPrev, std::shared_ptr<CMutableTransaction>& coinstakeTx, std::shared_ptr<CStakeInput>& coinstakeInput, int64_t& nTxNewTime) {
    if (pwallet == nullptr || pindexPrev == nullptr)
        return false;

    coinstakeTx->vin.clear();
    coinstakeTx->vout.clear();

    // Mark coin stake transaction
    CScript scriptEmpty;
    scriptEmpty.clear();
    coinstakeTx->vout.push_back(CTxOut(0, scriptEmpty));

    // Choose coins to use
    CAmount nBalance = pwallet->GetBalance().m_mine_trusted;

    if (nBalance > 0 && nBalance <= nReserveBalance)
        return false;

    // Get the list of stakable inputs
    std::list<std::unique_ptr<CStakeInput> > listInputs;
    if (!SelectStakeCoins(listInputs, nBalance - nReserveBalance, pindexPrev->nHeight + 1)) {
        LogPrint(BCLog::STAKING, "CreateCoinStake(): selectStakeCoins failed\n");
        return false;
    }

    if (GetAdjustedTime() - pindexPrev->GetBlockTime() < 60) {
        if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
            UninterruptibleSleep(std::chrono::milliseconds{100});
        }
    }

    CScript scriptPubKeyKernel;
    bool fKernelFound = false;
    int nAttempts = 0;

    for (std::unique_ptr<CStakeInput>& stakeInput : listInputs) {
        // Make sure the wallet is unlocked and shutdown hasn't been requested
        if (pwallet->IsLocked(true) || ShutdownRequested())
            return false;

        boost::this_thread::interruption_point();

        CBlockHeader dummyBlockHeader;
        unsigned int stakeNBits = GetNextWorkRequired(pindexPrev, &dummyBlockHeader, Params().GetConsensus());
        uint256 hashProofOfStake = uint256();
        nAttempts++;
        //iterates each utxo inside of CheckStakeKernelHash()
        if (Stake(pindexPrev, stakeInput.get(), stakeNBits, nTxNewTime, hashProofOfStake)) {
            // Found a kernel
            LogPrint(BCLog::STAKING, "CreateCoinStake : kernel found\n");

            // Stake output value is set to stake input value.
            // Adding stake rewards and potentially splitting outputs is performed in BlockAssembler::CreateNewBlock()
            if (!stakeInput->CreateTxOuts(pwallet, coinstakeTx->vout, stakeInput->GetValue())) {
                LogPrint(BCLog::STAKING, "%s : failed to get scriptPubKey\n", __func__);
                return false;
            }

            // Limit size
            unsigned int nBytes = ::GetSerializeSize(*coinstakeTx, CTransaction::CURRENT_VERSION);
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

bool CStakingManager::IsStaking() {
    if (mapHashedBlocks.count(::ChainActive().Tip()->nHeight)) {
        return true;
    } else if (mapHashedBlocks.count(::ChainActive().Tip()->nHeight - 1) && nLastCoinStakeSearchInterval) {
        return true;
    }
    return false;
}

void CStakingManager::UpdatedBlockTip(const CBlockIndex* pindex)
{
    LOCK(cs);

    tipIndex = pindex;

    LogPrint(BCLog::STAKING, "CStakingManager::UpdatedBlockTip -- height: %d\n", pindex->nHeight);
}

void CStakingManager::DoMaintenance(CConnman& connman, ChainstateManager& chainman)
{
    if (!fEnableStaking) return; // Should never happen

    CBlockIndex* pindexPrev = ::ChainActive().Tip();
    bool fHaveConnections = connman.GetNodeCount(CConnman::CONNECTIONS_ALL) > 0;
    if (pwallet->IsLocked(true) || !pindexPrev || !masternodeSync->IsSynced() || !fHaveConnections || nReserveBalance >= pwallet->GetBalance().m_mine_trusted) {
        nLastCoinStakeSearchInterval = 0;
        UninterruptibleSleep(std::chrono::milliseconds{1 * 60 * 1000}); // Wait 1 minute
        return;
    }

    const int nStakeHeight = pindexPrev->nHeight + 1;
    const Consensus::Params& params = Params().GetConsensus();
    const bool fPosPhase = (nStakeHeight >= params.nPosStartHeight);// || (nStakeHeight >= params.PosPowStartHeight);

    if (!fPosPhase) {
        // no POS for at least 1 block
        nLastCoinStakeSearchInterval = 0;
        UninterruptibleSleep(std::chrono::milliseconds{1 * 60 * 1000}); // Wait 1 minute
        return;
    }

    const bool fTimeV2 = Params().GetConsensus().IsTimeProtocolV2(::ChainActive().Height()+1);
    //search our map of hashed blocks, see if bestblock has been hashed yet
    const int chainHeight = ::ChainActive().Height();
    if (mapHashedBlocks.count(chainHeight) && !fLastLoopOrphan)
    {
        int64_t nTime = GetAdjustedTime();
        int64_t tipHashTime = mapHashedBlocks[chainHeight];
        if (    (!fTimeV2 && nTime < tipHashTime + 22) ||
                (fTimeV2 && GetTimeSlot(nTime) <= tipHashTime) )
        {
            UninterruptibleSleep(std::chrono::milliseconds{std::min(nHashInterval - (nTime - tipHashTime), (int64_t)5) * 1000});
            return;
        }
    }
    fLastLoopOrphan = false;

   //control the amount of times the client will check for mintable coins
    if (!MintableCoins()) {
        // No mintable coins
        nLastCoinStakeSearchInterval = 0;
        LogPrint(BCLog::STAKING, "%s: No mintable coins, waiting..\n", __func__);
        UninterruptibleSleep(std::chrono::milliseconds{5 * 60 * 1000}); // Wait 5 minutes
        return;
    }

    int64_t nSearchTime = GetAdjustedTime();
    if (nSearchTime < nLastCoinStakeSearchTime) {
        UninterruptibleSleep(std::chrono::milliseconds{(nLastCoinStakeSearchTime - nSearchTime) * 1000}); // Wait
        return;
    } else {
        nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
        nLastCoinStakeSearchTime = nSearchTime;
    }

    pwallet->BlockUntilSyncedToCurrentChain();

    // Create new block
    std::shared_ptr<CMutableTransaction> coinstakeTxPtr = std::shared_ptr<CMutableTransaction>(new CMutableTransaction);
    std::shared_ptr<CStakeInput> coinstakeInputPtr = nullptr;
    std::unique_ptr<CBlockTemplate> pblocktemplate = nullptr;
    int64_t nCoinStakeTime;
    if (CreateCoinStake(::ChainActive().Tip(), coinstakeTxPtr, coinstakeInputPtr, nCoinStakeTime)) {
        // Coinstake found. Extract signing key from coinstake
        try {
            pblocktemplate = BlockAssembler(*sporkManager, *governance, *llmq::quorumBlockProcessor, *llmq::chainLocksHandler, *llmq::quorumInstantSendManager, mempool, Params()).CreateNewBlock(CScript(), coinstakeTxPtr, coinstakeInputPtr, nCoinStakeTime, pwallet.get());
        } catch (const std::exception& e) {
            LogPrint(BCLog::STAKING, "%s: error creating block, waiting.. - %s", __func__, e.what());
            UninterruptibleSleep(std::chrono::milliseconds{1 * 60 * 1000}); // Wait 1 minute
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
    auto spk_man = pwallet->GetLegacyScriptPubKeyMan();
    if (!spk_man) {
        return;
    }
    CKey key;
    if (!spk_man->GetKey(keyID, key)) {
        LogPrint(BCLog::STAKING, "%s: failed to get key from keystore", __func__);
        return;
    }
    if (!key.Sign(pblock->GetHash(), pblock->vchBlockSig)) {
        LogPrint(BCLog::STAKING, "%s: failed to sign block hash with key", __func__);
        return;
    }

    /// Process block
    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
    if (!chainman.ProcessNewBlock(Params(), shared_pblock, true, nullptr)) {
        fLastLoopOrphan = true;
        LogPrint(BCLog::STAKING, "%s: ProcessNewBlock, block not accepted", __func__);
        UninterruptibleSleep(std::chrono::milliseconds{10 * 1000}); // Wait 10 seconds
    }
}
