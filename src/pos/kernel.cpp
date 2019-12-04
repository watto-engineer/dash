// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018-2019 The Ion developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>
#include <algorithm>

#include "wallet/db.h"
#include "kernel.h"
#include "policy/policy.h"
#include "script/interpreter.h"
#include "timedata.h"
#include "util.h"
#include "stakeinput.h"

// v1 modifier interval.
static const int64_t OLD_MODIFIER_INTERVAL = 2087;

// Hard checkpoints of stake modifiers to ensure they are deterministic
static std::map<int, unsigned int> mapStakeModifierCheckpoints =
    boost::assign::map_list_of(0, 234907403);

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("%s : null pindex", __func__);
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier())
        return error("%s : no generation at genesis block", __func__);
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    assert(nSection >= 0 && nSection < 64);
    int64_t a = MODIFIER_INTERVAL  * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1)));
    return a;
}

// Get stake modifier selection interval (in seconds)
static int64_t GetStakeModifierSelectionIntervalPreDGW()
{
    int64_t nSelectionInterval = 0;
    for (int nSection = 0; nSection < 64; nSection++) {
        nSelectionInterval += 60 * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1)));
    }
    return nSelectionInterval;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(
    std::vector<std::pair<int64_t, uint256> >& vSortedByTimestamp,
    std::map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop,
    uint64_t nStakeModifierPrev,
    const CBlockIndex** pindexSelected)
{
    bool fSelected = false;
    uint256 hashBest;
    *pindexSelected = (const CBlockIndex*)0;
    for (const std::pair<int64_t, uint256> & item : vSortedByTimestamp) {
        if (!mapBlockIndex.count(item.second))
            return error("%s : failed to find block index for candidate block %s", __func__, item.second.GetHex());

        const CBlockIndex* pindex = mapBlockIndex[item.second];
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;

        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;

        // compute the selection hash by hashing an input that is unique to that block
        uint256 hashProof = pindex->GetBlockHash();

        CDataStream ss(SER_GETHASH, 0);
        ss << hashProof << nStakeModifierPrev;
        uint256 hashSelection = Hash(ss.begin(), ss.end());

        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (pindex->IsProofOfStake())
            hashSelection = ArithToUint256(UintToArith256(hashSelection) >> 32);

        if (fSelected && UintToArith256(hashSelection) < UintToArith256(hashBest)) {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        } else if (!fSelected) {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*)pindex;
        }
    }
    return fSelected;
}

/* NEW MODIFIER */

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
bool ComputeStakeModifierV2(CBlockIndex* pindex, const uint256& kernel)
{
    if (!pindex->pprev)
        return true; // genesis block's modifier is 0

    const CBlockIndex* pindexPrev = pindex->pprev;

    CHashWriter ss(SER_GETHASH, 0);
    ss << kernel;

    // switch with old modifier on upgrade block
    if (pindexPrev->nHeight + 1 < Params().GetConsensus().nBlockStakeModifierV2) {
        ss << pindexPrev->nStakeModifier;
    } else {
        ss << pindexPrev->nStakeModifierV2;
    }
    pindex->nStakeModifierV2 = ss.GetHash();

    return true;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier)
{
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;
    if (!pindexPrev) {
        fGeneratedStakeModifier = true;
        return true; // genesis block's modifier is 0
    }
    if (pindexPrev->nHeight == 0) {
        //Give a stake modifier to the first block
        fGeneratedStakeModifier = true;
        nStakeModifier = 93825007363294; //uint64_t("stakemodifier");
        return true;
    }

    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("%s : unable to get last modifier", __func__);

    if (nModifierTime / MODIFIER_INTERVAL >= pindexPrev->GetBlockTime() / MODIFIER_INTERVAL)
        return true;

    // Sort candidate blocks by timestamp
    std::vector<std::pair<int64_t, uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * MODIFIER_INTERVAL  / Params().GetConsensus().nPosTargetSpacing);
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / MODIFIER_INTERVAL ) * MODIFIER_INTERVAL  - OLD_MODIFIER_INTERVAL;
    const CBlockIndex* pindex = pindexPrev;

    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart) {
        vSortedByTimestamp.push_back(std::make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }

    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    std::reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    std::sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end(), [ ]( const std::pair<int64_t, uint256>& lhs, const std::pair<int64_t, uint256>& rhs )
    {
        if (lhs.first == rhs.first) return UintToArith256(lhs.second) < UintToArith256(rhs.second);
        return lhs.first < rhs.first;
    });

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    std::map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound = 0; nRound < std::min(64, (int)vSortedByTimestamp.size()); nRound++) {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);

        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);

        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);

        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(std::make_pair(pindex->GetBlockHash(), pindex));
    }

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
bool GetKernelStakeModifier(uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    nStakeModifier = 0;
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("%s : block not indexed", __func__);
    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    nStakeModifierHeight = pindexFrom->nHeight;
    nStakeModifierTime = pindexFrom->GetBlockTime();
    // Fixed stake modifier only for regtest
    if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
        nStakeModifier = pindexFrom->nStakeModifier;
        return true;
    }
    const CBlockIndex* pindex = pindexFrom;
    CBlockIndex* pindexNext = chainActive[pindex->nHeight + 1];

    // loop to find the stake modifier later by a selection interval
    do {
        if (!pindexNext) {
            // Should never happen
            if (chainActive.Height() >= 1126 && chainActive.Height() <= Params().GetConsensus().DGWStartHeight) {
                return true;
            } else {
                return error("%s : Null pindexNext, current block %s ", __func__, pindex->phashBlock->GetHex());
            }
        }
        pindex = pindexNext;
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
        pindexNext = chainActive[pindex->nHeight + 1];
    } while (nStakeModifierTime < pindexFrom->GetBlockTime() + OLD_MODIFIER_INTERVAL);

    nStakeModifier = pindex->nStakeModifier;
    return true;
}

bool CheckStakeKernelHash(const CBlockIndex* pindexPrev, const unsigned int nBits, CStakeInput* stake, const unsigned int nTimeTx, uint256& hashProofOfStake, const bool fVerify)
{
    // Calculate the proof of stake hash
    if (!GetHashProofOfStake(pindexPrev, stake, nTimeTx, fVerify, hashProofOfStake)) {
        return error("%s : Failed to calculate the proof of stake hash", __func__);
    }

    // Base target
    arith_uint256 bnTarget;
    bnTarget.SetCompact(nBits);

    // Weighted target
    bnTarget *= (arith_uint256(stake->GetValue()) / 100);

    // Check if proof-of-stake hash meets target protocol
    const arith_uint256 proof = UintToArith256(hashProofOfStake);
    const bool res = (UintToArith256(hashProofOfStake) < bnTarget);

    bool fPreDGW = (pindexPrev->nHeight + 1) < Params().GetConsensus().DGWStartHeight || nTimeTx < (unsigned int)Params().GetConsensus().DGWStartTime;
    return res || fPreDGW;
}

bool GetHashProofOfStakePreDGW(const CBlockIndex* pindexPrev, CStakeInput* stake, const unsigned int nTimeTx, const bool fVerify, uint256& hashProofOfStakeRet) {
    // Grab the stake data
    CBlockIndex* pindexfrom = stake->GetIndexFrom();
    const unsigned int nTimeBlockFrom = pindexfrom->nTime;

    CDataStream ss(SER_GETHASH, 0);
    ss << nTimeBlockFrom << hashProofOfStakeRet << stake->GetValue() << nTimeTx;
    hashProofOfStakeRet = Hash(ss.begin(), ss.end());

    return true;
}

bool GetHashProofOfStake(const CBlockIndex* pindexPrev, CStakeInput* stake, const unsigned int nTimeTx, const bool fVerify, uint256& hashProofOfStakeRet) {
    if (pindexPrev->nHeight < Params().GetConsensus().DGWStartHeight)
        return GetHashProofOfStakePreDGW(pindexPrev, stake, nTimeTx, fVerify, hashProofOfStakeRet);

    // Grab the stake data
    CBlockIndex* pindexfrom = stake->GetIndexFrom();
    if (!pindexfrom) return error("%s : Failed to find the block index for stake origin", __func__);
    const CDataStream& ssUniqueID = stake->GetUniqueness();
    const unsigned int nTimeBlockFrom = pindexfrom->nTime;
    CDataStream modifier_ss(SER_GETHASH, 0);

    // Hash the modifier
    if ((pindexPrev->nHeight + 1) < Params().GetConsensus().nBlockStakeModifierV2) {
        // Modifier v1
        uint64_t nStakeModifier = 0;
        if (!stake->GetModifier(nStakeModifier))
            return error("%s : Failed to get kernel stake modifier", __func__);
        modifier_ss << nStakeModifier;
    } else {
        // Modifier v2
        const arith_uint256 prevModifier = UintToArith256(pindexPrev->nStakeModifierV2);
        modifier_ss << pindexPrev->nStakeModifierV2;
    }

    CDataStream ss(modifier_ss);
    // Calculate hash
    ss << nTimeBlockFrom << ssUniqueID << nTimeTx;
    hashProofOfStakeRet = Hash(ss.begin(), ss.end());

    return true;
}

bool HasStakeMinAgeOrDepth(const int contextHeight, const uint32_t contextTime,
        const int utxoFromBlockHeight, const uint32_t utxoFromBlockTime)
{
    // before stake modifier V2, the age required was 60 * 60 (1 hour) / not required on regtest
    if (contextHeight < Params().GetConsensus().nBlockStakeModifierV2)
        return (Params().NetworkIDString() == CBaseChainParams::REGTEST || (utxoFromBlockTime + 3600 <= contextTime));

    // after stake modifier V2, we require the utxo to be nStakeMinDepth deep in the chain
    return (contextHeight - utxoFromBlockHeight >= Params().GetConsensus().nStakeMinDepth);
}

bool Stake(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, unsigned int& nTimeTx, uint256& hashProofOfStake)
{
    int prevHeight = pindexPrev->nHeight;

    // get stake input pindex
    CBlockIndex* pindexFrom = stakeInput->GetIndexFrom();
    if (!pindexFrom || pindexFrom->nHeight < 1) return error("%s : no pindexfrom", __func__);

    const uint32_t nTimeBlockFrom = pindexFrom->nTime;
    const int nHeightBlockFrom = pindexFrom->nHeight;

    // check for maturity (min age/depth) requirements
    if (!HasStakeMinAgeOrDepth(prevHeight + 1, nTimeTx, nHeightBlockFrom, nTimeBlockFrom))
        return error("%s : min age violation - height=%d - nTimeTx=%d, nTimeBlockFrom=%d, nHeightBlockFrom=%d",
                         __func__, prevHeight + 1, nTimeTx, nTimeBlockFrom, nHeightBlockFrom);

    // iterate the hashing
    bool fSuccess = false;
    const unsigned int nHashDrift = 60;
    const unsigned int nFutureTimeDriftPoS = 180;
    unsigned int nTryTime = nTimeTx - 1;
    // iterate from nTimeTx up to nTimeTx + nHashDrift
    // but not after the max allowed future blocktime drift (3 minutes for PoS)
    const unsigned int maxTime = std::min(nTimeTx + nHashDrift, (uint32_t)GetAdjustedTime() + nFutureTimeDriftPoS);

    while (nTryTime < maxTime)
    {
        //new block came in, move on
        if (chainActive.Height() != prevHeight)
            break;

        ++nTryTime;

        // if stake hash does not meet the target then continue to next iteration
        if (!CheckStakeKernelHash(pindexPrev, nBits, stakeInput, nTryTime, hashProofOfStake))
            continue;

        // if we made it this far, then we have successfully found a valid kernel hash
        fSuccess = true;
        nTimeTx = nTryTime;
        break;
    }

    return fSuccess;
}

// Check kernel hash target and coinstake signature
bool initStakeInput(const CBlock block, std::unique_ptr<CStakeInput>& stake, int nPreviousBlockHeight) {
    const CTransaction tx = *block.vtx[1];
    if (!tx.IsCoinStake())
        return error("%s : called on non-coinstake %s", __func__, tx.GetHash().GetHex());

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTxIn& txin = tx.vin[0];

    {
        // First try finding the previous transaction in database
        uint256 hashBlock;
        CTransactionRef txPrev;
        if (!GetTransaction(txin.prevout.hash, txPrev, Params().GetConsensus(), hashBlock, true))
            return error("%s : INFO: read txPrev failed, tx id prev: %s, block id %s",
                         __func__, txin.prevout.hash.GetHex(), block.GetHash().GetHex());

        //verify signature and script
        if (!VerifyScript(txin.scriptSig, txPrev->vout[txin.prevout.n].scriptPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&tx, 0, txPrev->vout[txin.prevout.n].nValue)))
            return error("%s : VerifySignature failed on coinstake %s", __func__, tx.GetHash().GetHex());

        CStake* bytzInput = new CStake();
        bytzInput->SetInput(txPrev, txin.prevout.n);
        stake = std::unique_ptr<CStakeInput>(bytzInput);
    }
    return true;
}

// Check kernel hash target and coinstake signature
bool CheckProofOfStake(const CBlock block, uint256& hashProofOfStake, std::unique_ptr<CStakeInput>& stake, const CBlockIndex* pindex)
{
    if (pindex == nullptr || pindex->pprev == nullptr)
        return error("%s : null pindexPrev for block %s", __func__, block.GetHash().GetHex());
    const int nPreviousBlockHeight = pindex->pprev->nHeight;

    // Initialize the stake object
    if(!initStakeInput(block, stake, nPreviousBlockHeight))
        return error("%s : stake input object initialization failed", __func__);

    const CTransactionRef tx = block.vtx[1];
    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTxIn& txin = tx->vin[0];
    CBlockIndex* pindexfrom = stake->GetIndexFrom();
    if (!pindexfrom)
        return error("%s: Failed to find the block index for stake origin", __func__);

    unsigned int nBlockFromTime = pindexfrom->nTime;
    unsigned int nTxTime = block.nTime;
    const int nBlockFromHeight = pindexfrom->nHeight;

    if (!CheckStakeKernelHash(pindex->pprev, block.nBits, stake.get(), nTxTime, hashProofOfStake, true)) {
        return error("%s : INFO: check kernel failed on coinstake %s, hashProof=%s", __func__,
                     tx->GetHash().GetHex(), hashProofOfStake.GetHex());
    }

    return true;
}

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx)
{
    // v0.3 protocol
    return (nTimeBlock == nTimeTx);
}

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex)
{
    assert(pindex->pprev || pindex->GetBlockHash() == Params().GetConsensus().hashGenesisBlock);
    // Hash previous checksum with flags, hashProofOfStake and nStakeModifier
    CDataStream ss(SER_GETHASH, 0);
    if (pindex->pprev)
        ss << pindex->pprev->nStakeModifierChecksum;
    uint256 hashProofOfStake = mapProofOfStake[pindex->GetBlockHash()];
    ss << pindex->nFlags << hashProofOfStake << pindex->nStakeModifier;
    uint256 hashChecksum = Hash(ss.begin(), ss.end());
    arith_uint256 arithHashChecksum = UintToArith256(hashChecksum);
    arithHashChecksum >>= (256 - 32);
    return arithHashChecksum.GetLow64();
}

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum)
{
    if (Params().NetworkIDString() != CBaseChainParams::MAIN) return true; // Testnet has no checkpoints
    if (mapStakeModifierCheckpoints.count(nHeight)) {
        return nStakeModifierChecksum == mapStakeModifierCheckpoints[nHeight];
    }
    return true;
}

// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
bool GetKernelStakeModifierPreDGW(uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    nStakeModifier = 0;
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("GetKernelStakeModifier() : block not indexed");
    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    nStakeModifierHeight = pindexFrom->nHeight;
    nStakeModifierTime = pindexFrom->GetBlockTime();
    int64_t nStakeModifierSelectionInterval = GetStakeModifierSelectionIntervalPreDGW();
    const CBlockIndex* pindex = pindexFrom;
    CBlockIndex* pindexNext = chainActive[pindexFrom->nHeight + 1];

    // loop to find the stake modifier later by a selection interval
    int nDGWStartHeight = Params().GetConsensus().DGWStartHeight;
    while (nStakeModifierTime < pindexFrom->GetBlockTime() + nStakeModifierSelectionInterval) {
        if (!pindexNext) {
            if (chainActive.Height() >= 1126 && chainActive.Height() <= nDGWStartHeight) {
                return true;
            } else {
                LogPrint(BCLog::STAKING, "Null pindexNext\n");
            }
            return true;
        }

        pindex = pindexNext;
        pindexNext = chainActive[pindexNext->nHeight + 1];
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
    }
    nStakeModifier = pindex->nStakeModifier;
    return true;
}

bool AcceptPOSParameters(const CBlock& block, CValidationState& state, CBlockIndex* pindexNew) {
    AssertLockHeld(cs_main);

    if (!pindexNew->SetStakeEntropyBit(pindexNew->GetStakeEntropyBit()))
        return state.Invalid(error("%s : SetStakeEntropyBit() failed", __func__));

    if (pindexNew->nHeight < Params().GetConsensus().nBlockStakeModifierV2) {
        uint64_t nStakeModifier = 0;
        bool fGeneratedStakeModifier = false;
        if (!ComputeNextStakeModifier(pindexNew->pprev, nStakeModifier, fGeneratedStakeModifier))
            return state.Invalid(error("%s : ComputeNextStakeModifier() failed", __func__));
        pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
        pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);
        if (!CheckStakeModifierCheckpoints(pindexNew->nHeight, pindexNew->nStakeModifierChecksum))
            return state.DoS(20, error("%s : Rejected by stake modifier checkpoint height=%d, modifier=%sn", pindexNew->nHeight, std::to_string(nStakeModifier), __func__));
    } else {
        // compute v2 stake modifier
        ComputeStakeModifierV2(pindexNew, block.vtx[1]->vin[0].prevout.hash);
    }
    return true;
}
