// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2021 The Dash Core developers
// Copyright (c) 2021 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/quickgames/dice.h>
#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>

#include <arith_uint256.h>

#include <assert.h>

#include <chainparamsseeds.h>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

static CBlock CreateDevNetGenesisBlock(const uint256 &prevBlockHash, const std::string& devNetName, uint32_t nTime, uint32_t nNonce, uint32_t nBits, const CAmount& genesisReward)
{
    assert(!devNetName.empty());

    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    // put height (BIP34) and devnet name into coinbase
    txNew.vin[0].scriptSig = CScript() << 1 << std::vector<unsigned char>(devNetName.begin(), devNetName.end());
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = CScript() << OP_RETURN;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = 4;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock = prevBlockHash;
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
 *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
 *   vMerkleTree: e0028e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "RT 15/Feb/2018 12.03 GMT - Soros brands bitcoin nest egg for dictators, but still invests in it";
    const CScript genesisOutputScript = CScript() << ParseHex("046013426db3d877adca7cea18ebeca33e88fafc53ab4040e0fe1bd0429712178c10571dfed6b3f1f19bcff0805cdf1c798e7a84ef0f5e0f4459aabd7e94ced9e6") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}


void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int64_t nWindowSize, int64_t nThresholdStart, int64_t nThresholdMin, int64_t nFalloffCoeff)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
    if (nWindowSize != -1) {
            consensus.vDeployments[d].nWindowSize = nWindowSize;
    }
    if (nThresholdStart != -1) {
        consensus.vDeployments[d].nThresholdStart = nThresholdStart;
    }
    if (nThresholdMin != -1) {
        consensus.vDeployments[d].nThresholdMin = nThresholdMin;
    }
    if (nFalloffCoeff != -1) {
        consensus.vDeployments[d].nFalloffCoeff = nFalloffCoeff;
    }
}

void CChainParams::UpdateDIP3Parameters(int nActivationHeight, int nEnforcementHeight)
{
    consensus.DIP0003Height = nActivationHeight;
//    consensus.DIP0003EnforcementHeight = nEnforcementHeight;
}

void CChainParams::UpdateDIP8Parameters(int nActivationHeight)
{
    consensus.DIP0008Height = nActivationHeight;
}

void CChainParams::UpdateBudgetParameters(int nMasternodePaymentsStartBlock, int nBudgetPaymentsStartBlock, int nSuperblockStartBlock)
{
    consensus.nMasternodePaymentsStartBlock = nMasternodePaymentsStartBlock;
    consensus.nBudgetPaymentsStartBlock = nBudgetPaymentsStartBlock;
    consensus.nSuperblockStartBlock = nSuperblockStartBlock;
}

void CChainParams::UpdateSubsidyAndDiffParams(int nMinimumDifficultyBlocks, int nHighSubsidyBlocks, int nHighSubsidyFactor)
{
    consensus.nMinimumDifficultyBlocks = nMinimumDifficultyBlocks;
    consensus.nHighSubsidyBlocks = nHighSubsidyBlocks;
    consensus.nHighSubsidyFactor = nHighSubsidyFactor;
}

void CChainParams::UpdateLLMQChainLocks(Consensus::LLMQType llmqType) {
    consensus.llmqTypeChainLocks = llmqType;
}

void CChainParams::UpdateLLMQInstantSend(Consensus::LLMQType llmqType) {
    consensus.llmqTypeInstantSend = llmqType;
}

void CChainParams::UpdateLLMQTestParams(int size, int threshold) {
    auto& params = consensus.llmqs.at(Consensus::LLMQ_TEST);
    params.size = size;
    params.minSize = threshold;
    params.threshold = threshold;
    params.dkgBadVotesThreshold = threshold;
}

void CChainParams::UpdateLLMQDevnetParams(int size, int threshold)
{
    auto& params = consensus.llmqs.at(Consensus::LLMQ_DEVNET);
    params.size = size;
    params.minSize = threshold;
    params.threshold = threshold;
    params.dkgBadVotesThreshold = threshold;
}

static CBlock FindDevNetGenesisBlock(const CBlock &prevBlock, const CAmount& reward)
{
    std::string devNetName = gArgs.GetDevNetName();
    assert(!devNetName.empty());

    CBlock block = CreateDevNetGenesisBlock(prevBlock.GetHash(), devNetName.c_str(), prevBlock.nTime + 1, 0, prevBlock.nBits, reward);

    arith_uint256 bnTarget;
    bnTarget.SetCompact(block.nBits);

    for (uint32_t nNonce = 0; nNonce < UINT32_MAX; nNonce++) {
        block.nNonce = nNonce;

        uint256 hash = block.GetHash();
        if (UintToArith256(hash) <= bnTarget)
            return block;
    }

    // This is very unlikely to happen as we start the devnet with a very low difficulty. In many cases even the first
    // iteration of the above loop will give a result already
    error("FindDevNetGenesisBlock: could not find devnet genesis block for %s", devNetName);
    assert(false);
}

// this one is for testing only
static Consensus::LLMQParams llmq_test = {
        .type = Consensus::LLMQ_TEST,
        .name = "llmq_test",
        .size = 3,
        .minSize = 2,
        .threshold = 2,

        .dkgInterval = 30, // one DKG every 30 minutes
        .dkgPhaseBlocks = 3,
        .dkgMiningWindowStart = 15, // dkgPhaseBlocks * 5 = after finalization
        .dkgMiningWindowEnd = 27,
        .dkgBadVotesThreshold = 2,

        .signingActiveQuorumCount = 2, // just a few ones to allow easier testing

        .keepOldConnections = 3,
        .recoveryMembers = 3,
};

// this one is for testing only
static Consensus::LLMQParams llmq_test_v17 = {
        .type = Consensus::LLMQ_TEST_V17,
        .name = "llmq_test_v17",
        .size = 3,
        .minSize = 2,
        .threshold = 2,

        .dkgInterval = 30, // one DKG per 30 minutes
        .dkgPhaseBlocks = 3,
        .dkgMiningWindowStart = 15, // dkgPhaseBlocks * 5 = after finalization
        .dkgMiningWindowEnd = 27,
        .dkgBadVotesThreshold = 2,

        .signingActiveQuorumCount = 2, // just a few ones to allow easier testing

        .keepOldConnections = 3,
        .recoveryMembers = 3,
};

// this one is for devnets only
static Consensus::LLMQParams llmq_devnet = {
        .type = Consensus::LLMQ_DEVNET,
        .name = "llmq_devnet",
        .size = 10,
        .minSize = 7,
        .threshold = 6,

        .dkgInterval = 30, // one DKG per thirty minutes
        .dkgPhaseBlocks = 3,
        .dkgMiningWindowStart = 15, // dkgPhaseBlocks * 5 = after finalization
        .dkgMiningWindowEnd = 27,
        .dkgBadVotesThreshold = 7,

        .signingActiveQuorumCount = 3, // just a few ones to allow easier testing

        .keepOldConnections = 4,
        .recoveryMembers = 6,
};

static Consensus::LLMQParams llmq20_60 = {
        .type = Consensus::LLMQ_20_60,
        .name = "llmq_20_60",
        .size = 20,
        .minSize = 16,
        .threshold = 12,

        .dkgInterval = 60, // one DKG per hour
        .dkgPhaseBlocks = 4,
        .dkgMiningWindowStart = 20, // dkgPhaseBlocks * 5 = after finalization
        .dkgMiningWindowEnd = 32,
        .dkgBadVotesThreshold = 14,

        .signingActiveQuorumCount = 24, // a full day worth of LLMQs

        .keepOldConnections = 25,
        .recoveryMembers = 12,
};

static Consensus::LLMQParams llmq40_60 = {
        .type = Consensus::LLMQ_40_60,
        .name = "llmq_40_60",
        .size = 40,
        .minSize = 30,
        .threshold = 24,

        .dkgInterval = 60 * 12, // one DKG every 12 hours
        .dkgPhaseBlocks = 6,
        .dkgMiningWindowStart = 30, // dkgPhaseBlocks * 5 = after finalization
        .dkgMiningWindowEnd = 42,
        .dkgBadVotesThreshold = 30,

        .signingActiveQuorumCount = 4, // two days worth of LLMQs

        .keepOldConnections = 5,
        .recoveryMembers = 20,
};

// Used for deployment and min-proto-version signalling, so it needs a higher threshold
static Consensus::LLMQParams llmq40_85 = {
        .type = Consensus::LLMQ_40_85,
        .name = "llmq_40_85",
        .size = 40,
        .minSize = 35,
        .threshold = 34,

        .dkgInterval = 60 * 24, // one DKG every 24 hours
        .dkgPhaseBlocks = 6,
        .dkgMiningWindowStart = 30, // dkgPhaseBlocks * 5 = after finalization
        .dkgMiningWindowEnd = 60, // give it a larger mining window to make sure it is mined
        .dkgBadVotesThreshold = 30,

        .signingActiveQuorumCount = 4, // four days worth of LLMQs

        .keepOldConnections = 5,
        .recoveryMembers = 20,
};

// Used for Platform
static Consensus::LLMQParams llmq20_70 = {
        .type = Consensus::LLMQ_20_70,
        .name = "llmq_20_70",
        .size = 20,
        .minSize = 16,
        .threshold = 14,

        .dkgInterval = 60, // one DKG per hour
        .dkgPhaseBlocks = 4,
        .dkgMiningWindowStart = 20, // dkgPhaseBlocks * 5 = after finalization
        .dkgMiningWindowEnd = 32,
        .dkgBadVotesThreshold = 14,

        .signingActiveQuorumCount = 24, // a full day worth of LLMQs

        .keepOldConnections = 25,
        .recoveryMembers = 50,
};

libzerocoin::ZerocoinParams* CChainParams::Zerocoin_Params(bool useModulusV1) const
{
    assert(this);
    static CBigNum bnHexModulus = 0;
    if (!bnHexModulus)
        bnHexModulus.SetHex(consensus.zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsHex = libzerocoin::ZerocoinParams(bnHexModulus);
    static CBigNum bnDecModulus = 0;
    if (!bnDecModulus)
        bnDecModulus.SetDec(consensus.zerocoinModulus);
    static libzerocoin::ZerocoinParams ZCParamsDec = libzerocoin::ZerocoinParams(bnDecModulus);

    if (useModulusV1)
        return &ZCParamsHex;

    return &ZCParamsDec;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */


class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 210240; // Note: actual number of blocks per calendar year with DGW v3 is ~200700 (for example 449750 - 249050)
        consensus.nMasternodePaymentsStartBlock = 100000; // not true, but it's ok as long as it's less then nMasternodePaymentsIncreaseBlock
        consensus.nMasternodePaymentsIncreaseBlock = 158000; // actual historical value
        consensus.nMasternodePaymentsIncreasePeriod = 576*30; // 17280 - actual historical value
        consensus.nInstantSendConfirmationsRequired = 6;
        consensus.nInstantSendKeepLock = 24;
        consensus.nBudgetPaymentsStartBlock = std::numeric_limits<int>::max();
        consensus.nBudgetPaymentsCycleBlocks = 43200; // (60*24*30)
        consensus.nBudgetPaymentsWindowBlocks = 2880;
        consensus.nSuperblockStartBlock = std::numeric_limits<int>::max();
        consensus.nSuperblockStartHash = uint256(); // do not check this
        consensus.nSuperblockCycle = 43200; // (60*24*30)
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nMasternodeMinimumConfirmations = 15;
        consensus.V17DeploymentHeight = std::numeric_limits<int>::max();
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("000001364c4ed20f1b240810b5aa91fee23ae9b64b6e746b594b611cf6d8c87b");
        consensus.BIP65Height = consensus.V17DeploymentHeight;
        consensus.BIP66Height = 1; // 000002f68dbbf1fcfacb8f0b4e64083efdd2f07a906728ee068d573ffa5bcb4e
        consensus.CSVHeight = consensus.V17DeploymentHeight;
        consensus.BIP147Height = consensus.V17DeploymentHeight;
        consensus.DIP0001Height = consensus.V17DeploymentHeight;
        consensus.DIP0003Height = consensus.V17DeploymentHeight;
//        consensus.DIP0003EnforcementHeight = std::numeric_limits<int>::max();
        consensus.DIP0003EnforcementHash = uint256();
        consensus.DIP0008Height = consensus.V17DeploymentHeight;

        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 25;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Proof of work parameters
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPowTargetTimespan = 24 * 60 * 60; // Wagerr: 1 day
        consensus.nPowTargetSpacing = 1 * 60; // Wagerr: 1 minute
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nMaturityV1 = 100;
        consensus.nMaturityV2 = 60;
        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00000000000000000000000000000000000000000000009db835052f74f73219"); // 1623262
        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0");

        // Wagerr specific deployment heights
        consensus.nWagerrProtocolV1StartHeight = 298386;                            // Betting protocol v1 activation block
        consensus.nWagerrProtocolV2StartHeight = 763350;                            // Betting protocol v2 activation block
        consensus.nWagerrProtocolV3StartHeight = 1501000;                           // Betting protocol v3 activation block
        consensus.nWagerrProtocolV4StartHeight = std::numeric_limits<int>::max();   // Betting protocol v4 activation block
        consensus.nQuickGamesEndHeight = consensus.nWagerrProtocolV3StartHeight;    // Quick games: retired functionality
        consensus.nMaturityV2StartHeight = consensus.nWagerrProtocolV3StartHeight;  // Reduced block maturity required for spending coinstakes and betting payouts
        consensus.nKeysRotateHeight = consensus.nWagerrProtocolV3StartHeight;       // Rotate spork key, oracle keys and fee payout keys
        consensus.nPosStartHeight = 1002;
        consensus.nBlockStakeModifierV1A = 1000;
        consensus.nBlockStakeModifierV2 = 891276;
        consensus.nBlockTimeProtocolV2 = consensus.nWagerrProtocolV3StartHeight;
        consensus.ATPStartHeight = consensus.V17DeploymentHeight;

        // Proof of Stake parameters
        consensus.posLimit = uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 24
        consensus.posLimit_V2 = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPosTargetSpacing = 1 * 60; // 1 minute
        consensus.nPosTargetTimespan = 40 * 60; // 40 minutes
        consensus.nTimeSlotLength = 15;
        consensus.nPosTargetTimespan_V2 = 2 * consensus.nTimeSlotLength * 60; // 30 minutes
        consensus.nStakeMinDepth = 600;
        consensus.nStakeMinAge = 60 * 60; // 1 hour

        // ATP parameters
        consensus.WagerrAddrPrefix = "wagerr";
        consensus.strTokenManagementKey = "WdFESJpjnXBjq4xahEsbHYeD8yoHfSHLCh"; // 04d449cc1ac45d327c34d8b116797ad9ed287980a9199ea48dc4c8beab90ae2ded738e826ba0b27b5571d63884d985e2a50afbe8eef2925fc280af51a2a2d5e0e0
        consensus.nOpGroupNewRequiredConfirmations = 1;

        // Zerocoin
        consensus.nZerocoinRequiredStakeDepth = 200;
        consensus.nZerocoinStartHeight = 700;
        consensus.nZerocoinStartTime = 1518696182; // GMT: Thursday, 15. February 2018 12:03:02
        consensus.nBlockZerocoinV2 = 298386;
        consensus.nPublicZCSpends = 752800;
        consensus.nFakeSerialBlockheightEnd = 556623;
        consensus.nSupplyBeforeFakeSerial = 3703597*COIN;   // zerocoin supply at block nFakeSerialBlockheightEnd
        consensus.nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        consensus.nRequiredAccumulation = 1;
        consensus.zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
            "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
            "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
            "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
            "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
            "31438167899885040445364023527381951378636564391212010397122822120720357";

        // Betting
        consensus.nBetBlocksIndexTimespanV2 = 23040;                                // Checking back 2 weeks for events and bets for each result.  (With approx. 2 days buffer).
        consensus.nBetBlocksIndexTimespanV3 = 90050;                                // Checking back 2 months for events and bets for each result.  (With approx. 2 days buffer).
        consensus.nOMNORewardPermille = 24;                                         // profitAcc / (100-6) * 100 * 0.024 (nMNBetReward = Total Profit * 0.024).
        consensus.nDevRewardPermille = 6;                                           // profitAcc / (100-6) * 100 * 0.006 (nDevReward = Total Profit * 0.006).
        consensus.nBetBlockPayoutAmount = 1440;                                     // Set the number of blocks we want to look back for results already paid out.
        consensus.nMinBetPayoutRange = 25;                                          // Spam filter to prevent malicious actors congesting the chain (Only payout bets that are between 25 - 10000 WRG inclusive).
        consensus.nMaxBetPayoutRange = 10000;                                       // Minimizes maximum payout size to avoid unnecessary large numbers (Only payout bets that are between 25 - 10000 WRG inclusive).
        consensus.nMaxParlayBetPayoutRange = 4000;                                  // Minimizes maximum parlay payout size to avoid unnecessary large numbers (Only payout parlay bets that are between 25 - 4000 WRG inclusive).
        consensus.nBetPlaceTimeoutBlocks = 120;                                     // Discard bets placed less than 120 seconds (approx. 2 mins) before event start time
        consensus.nMaxParlayLegs = 5;                                               // Minimizes maximum legs in parlay bet

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x84;
        pchMessageStart[1] = 0x2d;
        pchMessageStart[2] = 0x61;
        pchMessageStart[3] = 0xfd;
        nDefaultPort = 55002;
        nPruneAfterHeight = 100000;
        nMaxBettingUndoDepth = 101;

        genesis = CreateGenesisBlock(1518696181, 96620932, 0x1e0ffff0, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x000007b9191bc7a17bfb6cedf96a8dacebb5730b498361bf26d44a9f9dcc1079"));
        assert(genesis.hashMerkleRoot == uint256S("0xc4d06cf72583752c23b819fa8d8cededd1dad5733d413ea1f123f98a7db6af13"));

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.clear();
        vSeeds.emplace_back("main.seederv1.wgr.host");       // Wagerr's official seed 1
        vSeeds.emplace_back("main.seederv2.wgr.host");       // Wagerr's official seed 2
        vSeeds.emplace_back("main.devseeder1.wgr.host");     // Wagerr's dev1 testseed
        vSeeds.emplace_back("main.devseeder2.wgr.host");     // Wagerr's dev1 testseed
        // Wagerr addresses start with 'W
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1, 73);
        // Wagerr script addresses start with '7'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1, 63);
        // Wagerr private keys start with '7' or 'W'
        base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1, 199);
        // Wagerr BIP32 pubkeys start with 'xpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x02, 0x2D, 0x25, 0x33};
        // Wagerr BIP32 prvkeys start with 'xprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = {0x02, 0x21, 0x31, 0x2B};

        // Wagerr BIP44 coin type is '0x776772'
        nExtCoinType = 7825266;

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_20_60] = llmq20_60;
        consensus.llmqs[Consensus::LLMQ_40_60] = llmq40_60;
        consensus.llmqs[Consensus::LLMQ_40_85] = llmq40_85;
        consensus.llmqs[Consensus::LLMQ_20_70] = llmq20_70;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_40_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_20_60;
        consensus.llmqTypePlatform = Consensus::LLMQ_20_70;

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = false;
        nLLMQConnectionRetryTimeout = 60;

        nPoolMinParticipants = 3;
        nPoolMaxParticipants = 20;
        nFulfilledRequestExpireTime = 60*60; // fulfilled requests expire in 1 hour

        vSporkAddresses = {"Wj33PEETpJmDSHa2qosPcg8XzKe5bGLLZV"}; // 043432137728fb0f6ea29315e3e65d76f976b5d88710a8921437e1aabf1adc98ddb55035c17ffa581243db4bc7b6b3e5d0bdd968a28be906098c0b6cb8c6936b80
        nMinSporkKeys = 1;
        fBIP9CheckMasternodesUpgraded = true;

        /** Betting related parameters **/
        std::string strDevPayoutAddrOld = "Wm5om9hBJTyKqv5FkMSfZ2FDMeGp12fkTe";     // Development fund payout address (old).
        std::string strDevPayoutAddrNew = "Shqrs3mz3i65BiTEKPgnxoqJqMw5b726m5";     // Development fund payout address (new).
        std::string strOMNOPayoutAddrOld = "WRBs8QD22urVNeGGYeAMP765ncxtUA1Rv2";    // OMNO fund payout address (old).
        std::string strOMNOPayoutAddrNew = "SNCNYcDyXPCLHpG9AyyhnPcLNpxCpGZ2X6";    // OMNO fund payout address (new).
        vOracles = {
            { "WcsijutAF46tSLTcojk9mR9zV9wqwUUYpC", strDevPayoutAddrOld, strOMNOPayoutAddrOld, consensus.nWagerrProtocolV2StartHeight, consensus.nKeysRotateHeight },
            { "Weqz3PFBq3SniYF5HS8kuj72q9FABKzDrP", strDevPayoutAddrOld, strOMNOPayoutAddrOld, consensus.nWagerrProtocolV2StartHeight, consensus.nKeysRotateHeight },
            { "WdAo2Xk8r1MVx7ZmxARpJJkgzaFeumDcCS", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, std::numeric_limits<int>::max() },
            { "WhW3dmThz2hWEfpagfbdBQ7hMfqf6MkfHR", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, std::numeric_limits<int>::max() },
        };

        quickGamesArr.clear();
        quickGamesArr.emplace_back(
            std::string("Dice"), // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            std::string("Wm5om9hBJTyKqv5FkMSfZ2FDMeGp12fkTe"), // Dev address
            400, // OMNO reward permille (40%)
            100); // Dev reward permille (10%)

        // Workarounds
        nSkipBetValidationStart = 5577;
        nSkipBetValidationEnd = 35619;

        checkpointData = {
            {
                {1, uint256S("000001364c4ed20f1b240810b5aa91fee23ae9b64b6e746b594b611cf6d8c87b")},     // First PoW premine block
                {101, uint256S("0000005e89a1fab52bf996e7eb7d653962a0eb064c16c09887504797deb7feaf")},     // Last premine block
                {1001, uint256S("0000002a314058a8f61293e18ddbef5664a2097ac0178005f593444549dd5b8c")},     // Last PoW block
                {5530, uint256S("b3a8e6eb90085394c1af916d5690fd5b83d53c43cf60c7b6dd1e904e0ede8e88")},     // Block on which switch off happened, 5531, 5532 differed
                {14374, uint256S("61dc2dbb225de3146bc59ab96dedf48047ece84d004acaf8f386ae7a7d074983")},
                {70450, uint256S("ea83266a9dfd7cf92a96aa07f86bdf60d45850bd47c175745e71a1aaf60b4091")},
                {257142, uint256S("eca635870323e7c0785fec1e663f4cb8645b7e84b5df4511ba4c189e580bfafd")},
                {290000, uint256S("5a70e614a2e6035be0fa1dd1a67bd6caa0a78e396e889aac42bbbc08e11cdabd")},
                {294400, uint256S("01be3c3c84fd6063ba27080996d346318242d5335efec936408c1e1ae3fdb4a1")},
                {320000, uint256S("9060f8d44058c539653f37eaac4c53de7397e457dda264c5ee1be94293e9f6bb")},
                {695857, uint256S("680a170b5363f308cc0698a53ab6a83209dab06c138c98f91110f9e11e273778")},
                {720000, uint256S("63fc356380b3b8791e83a9d63d059ccc8d0e65dab703575ef4ca070e26e02fc7")},
                {732900, uint256S("5d832b3de9b207e03366fb8d4da6265d52015f5d1bd8951a656b5d4508a1da8e")},
                {891270, uint256S("eedb1794ca9267fb0ef88aff27afdd376ac93a54491a7b812cbad4b6c2e28d25")},
                {1427000, uint256S("2ee16722a21094f4ae8e371021c28d19268d6058de42e37ea0d4c90273c6a42e")},    // 3693972 1605485238
            }
        };

        chainTxData = ChainTxData{
            1605485238, // * UNIX timestamp of last known number of transactions (Block 1344000)
            3693972,     // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            0.0008        // * estimated number of transactions per second after that timestamp
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 210240;
        consensus.nMasternodePaymentsStartBlock = 4010; // not true, but it's ok as long as it's less then nMasternodePaymentsIncreaseBlock
        consensus.nMasternodePaymentsIncreaseBlock = 4030;
        consensus.nMasternodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = 4200;
        consensus.nBudgetPaymentsCycleBlocks = 144;
        consensus.nBudgetPaymentsWindowBlocks = 64;
        consensus.nSuperblockStartBlock = std::numeric_limits<int>::max(); // NOTE: Should satisfy nSuperblockStartBlock > nBudgetPeymentsStartBlock
        consensus.nSuperblockStartHash = uint256(); // do not check this on testnet
        consensus.nSuperblockCycle = 24; // Superblocks can be issued hourly on testnet
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.V17DeploymentHeight = 826130;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("0000065432f43b3efb23bd0f63fe33d00d02a5f36233fe1b982c08274d58ef12");
        consensus.BIP65Height = consensus.V17DeploymentHeight;
        consensus.BIP66Height = 1; // 0000065432f43b3efb23bd0f63fe33d00d02a5f36233fe1b982c08274d58ef12
        consensus.CSVHeight = consensus.V17DeploymentHeight;
        consensus.BIP147Height = consensus.V17DeploymentHeight;
        consensus.DIP0001Height = consensus.V17DeploymentHeight;
        consensus.DIP0003Height = consensus.V17DeploymentHeight;
//        consensus.DIP0003EnforcementHeight = std::numeric_limits<int>::max();
        consensus.DIP0003EnforcementHash = uint256();
        consensus.DIP0008Height = consensus.V17DeploymentHeight;

        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 25;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Proof of work parameters
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPowTargetTimespan = 24 * 60 * 60; // Wagerr: 1 day
        consensus.nPowTargetSpacing = 1 * 60; // Wagerr: 1 minute
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nMaturityV1 = 15;
        consensus.nMaturityV2 = 10;
        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000"); // 0
        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0000009303aeadf8cf3812f5c869691dbd4cb118ad20e9bf553be434bafe6a52"); // 470000

        // Wagerr specific deployment heights
        consensus.nWagerrProtocolV1StartHeight = 1100;      // Betting protocol v1 activation block
        consensus.nWagerrProtocolV2StartHeight = 1100;      // Betting protocol v2 activation block
        consensus.nWagerrProtocolV3StartHeight = 2000;      // Betting protocol v3 activation block
        consensus.nWagerrProtocolV4StartHeight = 405000;    // Betting protocol v4 activation block
        consensus.nQuickGamesEndHeight = 101650;
        consensus.nMaturityV2StartHeight = 38000;           // Reduced block maturity required for spending coinstakes and betting payouts
        consensus.nKeysRotateHeight = 102000;               // Rotate spork key, oracle keys and fee payout keys
        consensus.nPosStartHeight = 301;
        consensus.nBlockStakeModifierV1A = 51197;
        consensus.nBlockStakeModifierV2 = 92500;
        consensus.nBlockTimeProtocolV2 = 139550;
        consensus.ATPStartHeight = consensus.V17DeploymentHeight;

        // Proof of stake parameters
        consensus.posLimit = uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 24
        consensus.posLimit_V2 = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPosTargetSpacing = 1 * 60; // 1 minute
        consensus.nPosTargetTimespan = 40 * 60; // 40 minutes
        consensus.nTimeSlotLength = 15;
        consensus.nPosTargetTimespan_V2 = 2 * consensus.nTimeSlotLength * 60; // 30 minutes
        consensus.nStakeMinDepth = 100;
        consensus.nStakeMinAge = 60 * 60; // 1 hour

        // ATP parameters
        consensus.WagerrAddrPrefix = "wagerrtest";
        consensus.strTokenManagementKey = "TNPPuVRwCbBtNtWG9dBtv1fYDC8PFEeQ6y";
        consensus.nOpGroupNewRequiredConfirmations = 1;

        // Zerocoin
        consensus.nZerocoinRequiredStakeDepth = 200;
        consensus.nZerocoinStartHeight = 25;
        consensus.nZerocoinStartTime = 1524496462;
        consensus.nBlockZerocoinV2 = 60;
        consensus.nPublicZCSpends = std::numeric_limits<int>::max();
        consensus.nFakeSerialBlockheightEnd = -1;
        consensus.nSupplyBeforeFakeSerial = 0;
        consensus.nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        consensus.nRequiredAccumulation = 1;
        consensus.zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
            "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
            "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
            "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
            "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
            "31438167899885040445364023527381951378636564391212010397122822120720357";

        // Betting
        consensus.nBetBlocksIndexTimespanV2 = 23040;        // Checking back 2 weeks for events and bets for each result.  (With approx. 2 days buffer).
        consensus.nBetBlocksIndexTimespanV3 = 90050;        // Checking back 2 months for events and bets for each result.  (With approx. 2 days buffer).
        consensus.nOMNORewardPermille = 24;                 // profitAcc / (100-6) * 100 * 0.024 (nMNBetReward = Total Profit * 0.024).
        consensus.nDevRewardPermille = 6;                   // profitAcc / (100-6) * 100 * 0.006 (nDevReward = Total Profit * 0.006).
        consensus.nBetBlockPayoutAmount = 1440;             // Set the number of blocks we want to look back for results already paid out.
        consensus.nMinBetPayoutRange = 25;                  // Spam filter to prevent malicious actors congesting the chain (Only payout bets that are between 25 - 10000 WRG inclusive).
        consensus.nMaxBetPayoutRange = 10000;               // Minimizes maximum payout size to avoid unnecessary large numbers (Only payout bets that are between 25 - 10000 WRG inclusive).
        consensus.nMaxParlayBetPayoutRange = 4000;          // Minimizes maximum parlay payout size to avoid unnecessary large numbers (Only payout parlay bets that are between 25 - 4000 WRG inclusive).
        consensus.nBetPlaceTimeoutBlocks = 120;             // Discard bets placed less than 120 seconds (approx. 2 mins) before event start time,
        consensus.nMaxParlayLegs = 5;                       // Minimizes maximum legs in parlay bet

        // Chain parameters
        pchMessageStart[0] = 0x87;
        pchMessageStart[1] = 0x9e;
        pchMessageStart[2] = 0xd1;
        pchMessageStart[3] = 0x99;
        nDefaultPort = 55004;
        nPruneAfterHeight = 1000;
        nMaxBettingUndoDepth = 101;

        genesis = CreateGenesisBlock(1518696182, 75183976, 0x1e0ffff0, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00000fdc268f54ff1368703792dc046b1356e60914c2b5b6348032144bcb2de5"));
        //assert(genesis.hashMerkleRoot == uint256S("0xc4d06cf72583752c23b819fa8d8cededd1dad5733d413ea1f123f98a7db6af13"));

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        // nodes with support for servicebits filtering should be at the top
        vSeeds.clear();
        vSeeds.emplace_back("testnet-seeder-01.wgr.host");
        vSeeds.emplace_back("testnet-seedr-02.wgr.host");
        vSeeds.emplace_back("testnet.testnet-seeder-01.wgr.host");
        vSeeds.emplace_back("testnet.testnet-seeder-02.wgr.host");

        // Testnet Wagerr addresses start with 'y'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);
        // Testnet Wagerr script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,125);
        // Testnet private keys start with '9' or 'c'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,177);
        // Testnet Wagerr BIP32 pubkeys start with 'DRKV' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x3A, 0x80, 0x61, 0xA0};
        // Testnet Wagerr BIP32 prvkeys start with 'DRKP' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = {0x3A, 0x80, 0x58, 0x37};

        // Testnet Wagerr BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_TEST_V17] = llmq_test_v17;
        consensus.llmqs[Consensus::LLMQ_20_60] = llmq20_60;
        consensus.llmqs[Consensus::LLMQ_40_60] = llmq40_60;
        consensus.llmqs[Consensus::LLMQ_40_85] = llmq40_85;
        consensus.llmqs[Consensus::LLMQ_20_70] = llmq20_70;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_20_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_20_60;
        consensus.llmqTypePlatform = Consensus::LLMQ_20_70;

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = true;
        nLLMQConnectionRetryTimeout = 60;

        nPoolMinParticipants = 2;
        nPoolMaxParticipants = 20;
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes

        vSporkAddresses = {"TFykoxcv77bbLq3gG3hFLZqZ6QKERU9Syi"}; // 04d23d4179050244bfeff9f03ab4117e79a8835a9c0aba21b6df8d9e31042cc3b76bcb323a6e3a0e87b801ba2beef2c1db3a2a93d62bdb2e10192d8807f27e6f33
        nMinSporkKeys = 1;
        fBIP9CheckMasternodesUpgraded = true;

        /** Betting related parameters **/
        std::string strDevPayoutAddrOld = "TLceyDrdPLBu8DK6UZjKu4vCDUQBGPybcY";     // Development fund payout address (Testnet).
        std::string strDevPayoutAddrNew = "sUihJctn8P4wDVRU3SgSYbJkG8ajV68kmx";     // Development fund payout address (Testnet).
        std::string strOMNOPayoutAddrOld = "TDunmyDASGDjYwhTF3SeDLsnDweyEBpfnP";    // OMNO fund payout address (Testnet).
        std::string strOMNOPayoutAddrNew = "sMF9ejP1QMcoQnzURrSenRrFMznCfQfWgd";    // OMNO fund payout address (Testnet).
        vOracles = {
            { "TGFKr64W3tTMLZrKBhMAou9wnQmdNMrSG2", strDevPayoutAddrOld, strOMNOPayoutAddrOld, consensus.nWagerrProtocolV2StartHeight, consensus.nKeysRotateHeight },
            { "TWM5BQzfjDkBLGbcDtydfuNcuPfzPVSEhc", strDevPayoutAddrOld, strOMNOPayoutAddrOld, consensus.nWagerrProtocolV2StartHeight, consensus.nKeysRotateHeight },
            { "TRNjH67Qfpfuhn3TFonqm2DNqDwwUsJ24T", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, std::numeric_limits<int>::max() },
            { "TYijVoyFnJ8dt1SGHtMtn2wa34CEs8EVZq", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, std::numeric_limits<int>::max() },
        };

        quickGamesArr.clear();
        quickGamesArr.emplace_back(
            std::string("Dice"), // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            std::string("TLceyDrdPLBu8DK6UZjKu4vCDUQBGPybcY"), // Dev address
            400, // OMNO reward permille (40%)
            100); // Dev reward permille (10%)

        checkpointData = {
            {
                {0, uint256S("00000fdc268f54ff1368703792dc046b1356e60914c2b5b6348032144bcb2de5")},
                {1, uint256S("0000098cc93ece2804776d2e9eda2d01e2ff830d80bab22500821361259f8aa3")},
                {450, uint256S("3cec3911fdf321a22b8109ca95ca28913e6b51f0d80cc6d2b2e30e1f2a6115c0")},
                {469, uint256S("d69d843cd63d333cfa3ff4dc0675fa320d6ef8cab7ab1a73bf8a1482210f93ce")},
                {1100, uint256S("fa462709a1f3cf81d699ffbd45440204aa4d38de84c2da1fc8b3ff15c3c7a95f")},  // 1588780440
                {2000, uint256S("a5aab45e4e2345715adf79774d661a5bb9b2a2efd001c339df5678418fb51409")}, // 1588834261
            }
        };

        chainTxData = ChainTxData{
            1518696183, // * UNIX timestamp of last known number of transactions (Block 387900)
            0,       // * total number of transactions between genesis and that timestamp
                     //   (the tx=... number in the SetBestChain debug.log lines)
            0.000019 // * estimated number of transactions per second after that timestamp
        };

    }
};

/**
 * Devnet
 */
class CDevNetParams : public CChainParams {
public:
    CDevNetParams(bool fHelpOnly = false) {
        strNetworkID = "devnet";
        consensus.nSubsidyHalvingInterval = 210240;
        consensus.nMasternodePaymentsStartBlock = 4010;
        consensus.nMasternodePaymentsIncreaseBlock = 4030;
        consensus.nMasternodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = 4100;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nSuperblockStartBlock = 4200;
        consensus.nSuperblockStartHash = uint256(); // do not check this on regtest
        consensus.nSuperblockCycle = 64;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.V17DeploymentHeight = 300;
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = consensus.V17DeploymentHeight; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.CSVHeight = consensus.V17DeploymentHeight;
        consensus.BIP147Height = consensus.V17DeploymentHeight;
        consensus.DIP0001Height = 2;
        consensus.DIP0003Height = 2;
//        consensus.DIP0003EnforcementHeight = 500;
        consensus.DIP0003EnforcementHash = uint256();
        consensus.DIP0008Height = 2;

        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 25;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;

        // Proof of work parameters
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.nPowTargetTimespan = 24 * 60 * 60; // Wagerr: 1 day
        consensus.nPowTargetSpacing = 2.5 * 60; // Wagerr: 2.5 minutes
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nMaturityV1 = 100;
        consensus.nMaturityV2 = 60;
        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");
        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        // Wagerr specific deployment heights
        consensus.nWagerrProtocolV1StartHeight = 251;                             // Betting protocol v1 activation block
        consensus.nWagerrProtocolV2StartHeight = 251;                             // Betting protocol v2 activation block
        consensus.nWagerrProtocolV3StartHeight = 300;                             // Betting protocol v3 activation block
        consensus.nWagerrProtocolV4StartHeight = 300;                             // Betting protocol v4 activation block
        consensus.nQuickGamesEndHeight = consensus.nWagerrProtocolV3StartHeight;
        consensus.nMaturityV2StartHeight = consensus.nWagerrProtocolV3StartHeight;          // Reduced block maturity required for spending coinstakes and betting payouts
        consensus.nKeysRotateHeight = 270;                                        // Rotate spork key, oracle keys and fee payout keys
        consensus.nPosStartHeight = 301;
        consensus.nBlockStakeModifierV1A = consensus.nPosStartHeight;
        consensus.nBlockStakeModifierV2 = 400;
        consensus.nBlockTimeProtocolV2 = 500;
        consensus.ATPStartHeight = consensus.V17DeploymentHeight;

        // Proof of Stake parameters
        consensus.posLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.posLimit_V2 = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.nPosTargetSpacing = 1 * 60; // 1 minute
        consensus.nPosTargetTimespan = 40 * 60; // 40 minutes
        consensus.nTimeSlotLength = 15;
        consensus.nPosTargetTimespan_V2 = 2 * consensus.nTimeSlotLength * 60; // 30 minutes
        consensus.nStakeMinDepth = 1;
        consensus.nStakeMinAge = 0;

        // ATP parameters
        consensus.WagerrAddrPrefix = "wagerrdev";
        consensus.strTokenManagementKey = "TGRnrYZg52LwL3U2LLAUGiFE6xhqontQa9";
        consensus.nOpGroupNewRequiredConfirmations = 1;

        // Zerocoin
        consensus.nZerocoinRequiredStakeDepth = 200;
        consensus.nZerocoinStartHeight = std::numeric_limits<int>::max();
        consensus.nZerocoinStartTime = std::numeric_limits<int>::max();
        consensus.nBlockZerocoinV2 = std::numeric_limits<int>::max();
        consensus.nPublicZCSpends = std::numeric_limits<int>::max();
        consensus.nFakeSerialBlockheightEnd = -1;
        consensus.nSupplyBeforeFakeSerial = 0;
        consensus.nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        consensus.nRequiredAccumulation = 1;
        consensus.zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
            "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
            "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
            "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
            "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
            "31438167899885040445364023527381951378636564391212010397122822120720357";

        //Betting
        consensus.nBetBlocksIndexTimespanV2 = 2880;                               // Checking back 2 days for events and bets for each result.
        consensus.nBetBlocksIndexTimespanV3 = 23040;                              // Checking back 2 weeks for events and bets for each result.  (With approx. 2 days buffer).
        consensus.nOMNORewardPermille = 24;                                       // profitAcc / (100-6) * 100 * 0.024 (nMNBetReward = Total Profit * 0.024).
        consensus.nDevRewardPermille = 6;                                         // profitAcc / (100-6) * 100 * 0.006 (nDevReward = Total Profit * 0.006).
        consensus.nBetBlockPayoutAmount = 1440;                                   // Set the number of blocks we want to look back for results already paid out.
        consensus.nMinBetPayoutRange = 25;                                        // Spam filter to prevent malicious actors congesting the chain (Only payout bets that are between 25 - 10000 WRG inclusive).
        consensus.nMaxBetPayoutRange = 10000;                                     // Minimizes maximum payout size to avoid unnecessary large numbers (Only payout bets that are between 25 - 10000 WRG inclusive).
        consensus.nMaxParlayBetPayoutRange = 4000;                                // Minimizes maximum parlay payout size to avoid unnecessary large numbers (Only payout parlay bets that are between 25 - 4000 WRG inclusive).
        consensus.nBetPlaceTimeoutBlocks = 120;                                   // Discard bets placed less than 120 seconds (approx. 2 mins) before event start time,
        consensus.nMaxParlayLegs = 5;                                             // Minimizes maximum legs in parlay bet

        pchMessageStart[0] = 0xc5;
        pchMessageStart[1] = 0x2a;
        pchMessageStart[2] = 0x93;
        pchMessageStart[3] = 0xeb;
        nDefaultPort = 55008;
        nPruneAfterHeight = 1000;
        nMaxBettingUndoDepth = 101;

        genesis = CreateGenesisBlock(1518696184, 4638953, 0x207fffff, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x174db003bb4ce38c3462e7cbd9598ae891011f0043bdaaddeb67d2b42247e530"));
//        assert(genesis.hashMerkleRoot == uint256S("0xc4d06cf72583752c23b819fa8d8cededd1dad5733d413ea1f123f98a7db6af13"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        // Devnet Wagerr addresses start with 'y'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);
        // Devnet Wagerr script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,125);
        // Devnet private keys start with '9' or 'c'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,177);
        // Devnet Wagerr BIP32 pubkeys start with 'DRKV' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x3A, 0x80, 0x61, 0xA0};
        // Devnet Wagerr BIP32 prvkeys start with 'DRKP' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = {0x3A, 0x80, 0x58, 0x37};

        // Devnet Wagerr BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_DEVNET] = llmq_devnet;
        consensus.llmqs[Consensus::LLMQ_20_60] = llmq20_60;
        consensus.llmqs[Consensus::LLMQ_40_60] = llmq40_60;
        consensus.llmqs[Consensus::LLMQ_40_85] = llmq40_85;
        consensus.llmqs[Consensus::LLMQ_20_70] = llmq20_70;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_20_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_20_60;
        consensus.llmqTypePlatform = Consensus::LLMQ_20_70;

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fRequireRoutableExternalIP = false;
        fMineBlocksOnDemand = true;
        fAllowMultipleAddressesFromGroup = true;
        fAllowMultiplePorts = true;
        nLLMQConnectionRetryTimeout = 1; // must be lower then the LLMQ signing session timeout so that tests have control over failing behavior

        nPoolMinParticipants = 2;
        nPoolMaxParticipants = 20;
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes

        /* Spork Key for RegTest:
        WIF private key: 6xLZdACFRA53uyxz8gKDLcgVrm5kUUEu2B3BUzWUxHqa2W7irbH
        private key hex: a792662ff7b4cca1603fb9b67a4bce9e8ffb9718887977a5a0b2a522e3eab97e
        */
        vSporkAddresses = {"TNZgamuYWzNeupr9qD1To2rEBoEcbPA2x4"}; // 04b33722601343992c8a651fafa0f424c6ac90f797d3f58d90eebf96e817e9d7ca76a40e3c53b3d47f6f6a60b0d36dbb94ee630a5ad622f08d92782999fe7b043a
        nMinSporkKeys = 1;
        // regtest usually has no masternodes in most tests, so don't check for upgraged MNs
        fBIP9CheckMasternodesUpgraded = false;

        /** Betting related parameters **/
        std::string strDevPayoutAddrOld = "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs";     // Development fund payout address (Regtest).
        std::string strDevPayoutAddrNew = "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs";     // Development fund payout address (Regtest).
        std::string strOMNOPayoutAddrOld = "THofaueWReDjeZQZEECiySqV9GP4byP3qr";    // OMNO fund payout address (Regtest).
        std::string strOMNOPayoutAddrNew = "THofaueWReDjeZQZEECiySqV9GP4byP3qr";    // OMNO fund payout address (Regtest).
        vOracles = {
            { "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", strDevPayoutAddrOld, strOMNOPayoutAddrOld, consensus.nWagerrProtocolV2StartHeight, consensus.nKeysRotateHeight },
            { "TFvZVYGdrxxNunQLzSnRSC58BSRA7si6zu", strDevPayoutAddrOld, strOMNOPayoutAddrOld, consensus.nWagerrProtocolV2StartHeight, consensus.nKeysRotateHeight },
            { "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, std::numeric_limits<int>::max() },
            { "TFvZVYGdrxxNunQLzSnRSC58BSRA7si6zu", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, std::numeric_limits<int>::max() },
        };

        quickGamesArr.clear();
        quickGamesArr.emplace_back(
            std::string("Dice"), // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            std::string("TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs"), // Dev address
            400, // OMNO reward permille (40%)
            100); // Dev reward permille (10%)

        checkpointData = {
            {
                {0,uint256S("174db003bb4ce38c3462e7cbd9598ae891011f0043bdaaddeb67d2b42247e530")},
            }
        };

        chainTxData = ChainTxData{
            1518696184,
            1,
            0.01
        };

    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nMasternodePaymentsStartBlock = 240;
        consensus.nMasternodePaymentsIncreaseBlock = 350;
        consensus.nMasternodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = 1000;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 10;
        consensus.nSuperblockStartBlock = 1500;
        consensus.nSuperblockStartHash = uint256(); // do not check this on regtest
        consensus.nSuperblockCycle = 10;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 100;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.V17DeploymentHeight = 300;
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = consensus.V17DeploymentHeight; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.CSVHeight = consensus.V17DeploymentHeight;
        consensus.BIP147Height = consensus.V17DeploymentHeight;
        consensus.DIP0001Height = 2000;
        consensus.DIP0003Height = 210;
//        consensus.DIP0003EnforcementHeight = 500;
        consensus.DIP0003EnforcementHash = uint256();
        consensus.DIP0008Height = 432;

        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 25;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;

        // Proof of work parameters
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.nPowTargetTimespan = 24 * 60 * 60; // Wagerr: 1 day
        consensus.nPowTargetSpacing = 2.5 * 60; // Wagerr: 2.5 minutes
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nMaturityV1 = 100;
        consensus.nMaturityV2 = 60;
        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");
        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        // Wagerr specific deployment heights
        consensus.nWagerrProtocolV1StartHeight = 251;                             // Betting protocol v1 activation block
        consensus.nWagerrProtocolV2StartHeight = 251;                             // Betting protocol v2 activation block
        consensus.nWagerrProtocolV3StartHeight = 300;                             // Betting protocol v3 activation block
        consensus.nWagerrProtocolV4StartHeight = 300;                             // Betting protocol v4 activation block
        consensus.nQuickGamesEndHeight = consensus.nWagerrProtocolV3StartHeight;
        consensus.nMaturityV2StartHeight = consensus.nWagerrProtocolV3StartHeight;          // Reduced block maturity required for spending coinstakes and betting payouts
        consensus.nKeysRotateHeight = 270;                                        // Rotate spork key, oracle keys and fee payout keys
        consensus.nPosStartHeight = 251;
        consensus.nBlockStakeModifierV1A = consensus.nPosStartHeight;
        consensus.nBlockStakeModifierV2 = 400;
        consensus.nBlockTimeProtocolV2 = 500;
        consensus.ATPStartHeight = consensus.V17DeploymentHeight;

        // Proof of stake parameters
        consensus.posLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.posLimit_V2 = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.nPosTargetSpacing = 1 * 60; // 1 minute
        consensus.nPosTargetTimespan = 40 * 60; // 40 minutes
        consensus.nTimeSlotLength = 15;
        consensus.nPosTargetTimespan_V2 = 2 * consensus.nTimeSlotLength * 60; // 30 minutes
        consensus.nStakeMinDepth = 1;
        consensus.nStakeMinAge = 0;

        // ATP parameters
        consensus.WagerrAddrPrefix = "wagerrreg";
        consensus.strTokenManagementKey = "TJA37d7KPVmd5Lqa2EcQsptcfLYsQ1Qcfk";
        consensus.nOpGroupNewRequiredConfirmations = 1;

        // Zerocoin
        consensus.nZerocoinRequiredStakeDepth = 200;
        consensus.nZerocoinStartHeight = std::numeric_limits<int>::max();
        consensus.nZerocoinStartTime = std::numeric_limits<int>::max();
        consensus.nBlockZerocoinV2 = std::numeric_limits<int>::max();
        consensus.nPublicZCSpends = std::numeric_limits<int>::max();
        consensus.nFakeSerialBlockheightEnd = -1;
        consensus.nSupplyBeforeFakeSerial = 0;
        consensus.nMintRequiredConfirmations = 20; //the maximum amount of confirmations until accumulated in 19
        consensus.nRequiredAccumulation = 1;
        consensus.zerocoinModulus = "25195908475657893494027183240048398571429282126204032027777137836043662020707595556264018525880784"
            "4069182906412495150821892985591491761845028084891200728449926873928072877767359714183472702618963750149718246911"
            "6507761337985909570009733045974880842840179742910064245869181719511874612151517265463228221686998754918242243363"
            "7259085141865462043576798423387184774447920739934236584823824281198163815010674810451660377306056201619676256133"
            "8441436038339044149526344321901146575444541784240209246165157233507787077498171257724679629263863563732899121548"
            "31438167899885040445364023527381951378636564391212010397122822120720357";

        consensus.nBetBlocksIndexTimespanV2 = 2880;                               // Checking back 2 days for events and bets for each result.
        consensus.nBetBlocksIndexTimespanV3 = 23040;                              // Checking back 2 weeks for events and bets for each result.  (With approx. 2 days buffer).
        consensus.nOMNORewardPermille = 24;                                       // profitAcc / (100-6) * 100 * 0.024 (nMNBetReward = Total Profit * 0.024).
        consensus.nDevRewardPermille = 6;                                         // profitAcc / (100-6) * 100 * 0.006 (nDevReward = Total Profit * 0.006).
        consensus.nBetBlockPayoutAmount = 1440;                                   // Set the number of blocks we want to look back for results already paid out.
        consensus.nMinBetPayoutRange = 25;                                        // Spam filter to prevent malicious actors congesting the chain (Only payout bets that are between 25 - 10000 WRG inclusive).
        consensus.nMaxBetPayoutRange = 10000;                                     // Minimizes maximum payout size to avoid unnecessary large numbers (Only payout bets that are between 25 - 10000 WRG inclusive).
        consensus.nMaxParlayBetPayoutRange = 4000;                                // Minimizes maximum parlay payout size to avoid unnecessary large numbers (Only payout parlay bets that are between 25 - 4000 WRG inclusive).
        consensus.nBetPlaceTimeoutBlocks = 120;                                   // Discard bets placed less than 120 seconds (approx. 2 mins) before event start time,
        consensus.nMaxParlayLegs = 5;                                             // Minimizes maximum legs in parlay bet

        pchMessageStart[0] = 0x12;
        pchMessageStart[1] = 0x76;
        pchMessageStart[2] = 0xa1;
        pchMessageStart[3] = 0xfa;
        nDefaultPort = 55006;
        nPruneAfterHeight = 1000;
        nMaxBettingUndoDepth = 101;

        genesis = CreateGenesisBlock(1518696183, 574752, 0x207fffff, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x671d0510c128608897d98d1819d26b40810c8b7e4901447a909c87a9edc2f5ec"));
//        assert(genesis.hashMerkleRoot == uint256S("0xc4d06cf72583752c23b819fa8d8cededd1dad5733d413ea1f123f98a7db6af13"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        // Testnet Wagerr addresses start with 'y'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);
        // Testnet Wagerr script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,125);
        // Testnet private keys start with '9' or 'c'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,177);
        // Testnet Wagerr BIP32 pubkeys start with 'DRKV' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x3A, 0x80, 0x61, 0xA0};
        // Testnet Wagerr BIP32 prvkeys start with 'DRKP' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = {0x3A, 0x80, 0x58, 0x37};

        // Regtest Wagerr BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_TEST] = llmq_test;
        consensus.llmqs[Consensus::LLMQ_TEST_V17] = llmq_test_v17;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_TEST;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_TEST;
        consensus.llmqTypePlatform = Consensus::LLMQ_TEST;

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fRequireRoutableExternalIP = false;
        fMineBlocksOnDemand = true;
        fAllowMultipleAddressesFromGroup = true;
        fAllowMultiplePorts = true;
        nLLMQConnectionRetryTimeout = 1; // must be lower then the LLMQ signing session timeout so that tests have control over failing behavior

        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
        nPoolMinParticipants = 2;
        nPoolMaxParticipants = 20;

        /* Spork Key for RegTest:
        WIF private key: 6xLZdACFRA53uyxz8gKDLcgVrm5kUUEu2B3BUzWUxHqa2W7irbH
        private key hex: a792662ff7b4cca1603fb9b67a4bce9e8ffb9718887977a5a0b2a522e3eab97e
        */
        vSporkAddresses = {"TPiq9YKZdbfEGuZuZhQtLNGrkKSchmL1gc"}; // 048b664010f7851071787d58c276c05701b7109fa29f2360a3e72b3bdfa32b49cf20a23fd34bcc49fc564fdbdccc54dd0dc9183a7bdf05d580d118fcdcd4abfb3f
        nMinSporkKeys = 1;
        // regtest usually has no masternodes in most tests, so don't check for upgraged MNs
        fBIP9CheckMasternodesUpgraded = false;

        /** Betting related parameters **/
        std::string strDevPayoutAddrOld = "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs";     // Development fund payout address (Regtest).
        std::string strDevPayoutAddrNew = "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs";     // Development fund payout address (Regtest).
        std::string strOMNOPayoutAddrOld = "THofaueWReDjeZQZEECiySqV9GP4byP3qr";    // OMNO fund payout address (Regtest).
        std::string strOMNOPayoutAddrNew = "THofaueWReDjeZQZEECiySqV9GP4byP3qr";    // OMNO fund payout address (Regtest).
        vOracles = {
            { "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", strDevPayoutAddrOld, strOMNOPayoutAddrOld, consensus.nWagerrProtocolV2StartHeight, consensus.nKeysRotateHeight },
            { "TFvZVYGdrxxNunQLzSnRSC58BSRA7si6zu", strDevPayoutAddrOld, strOMNOPayoutAddrOld, consensus.nWagerrProtocolV2StartHeight, consensus.nKeysRotateHeight },
            { "TXuoB9DNEuZx1RCfKw3Hsv7jNUHTt4sVG1", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, std::numeric_limits<int>::max() },
            { "TFvZVYGdrxxNunQLzSnRSC58BSRA7si6zu", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, std::numeric_limits<int>::max() },
        };

        quickGamesArr.clear();
        quickGamesArr.emplace_back(
            std::string("Dice"), // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            std::string("TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs"), // Dev address
            400, // OMNO reward permille (40%)
            100); // Dev reward permille (10%)

        checkpointData = {
            {
                {0, uint256S("0x671d0510c128608897d98d1819d26b40810c8b7e4901447a909c87a9edc2f5ec")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain, bool fHelpOnly)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::DEVNET) {
        return std::unique_ptr<CChainParams>(new CDevNetParams(fHelpOnly));
    } else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int64_t nWindowSize, int64_t nThresholdStart, int64_t nThresholdMin, int64_t nFalloffCoeff)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout, nWindowSize, nThresholdStart, nThresholdMin, nFalloffCoeff);
}

void UpdateDIP3Parameters(int nActivationHeight, int nEnforcementHeight)
{
    globalChainParams->UpdateDIP3Parameters(nActivationHeight, nEnforcementHeight);
}

void UpdateDIP8Parameters(int nActivationHeight)
{
    globalChainParams->UpdateDIP8Parameters(nActivationHeight);
}

void UpdateBudgetParameters(int nMasternodePaymentsStartBlock, int nBudgetPaymentsStartBlock, int nSuperblockStartBlock)
{
    globalChainParams->UpdateBudgetParameters(nMasternodePaymentsStartBlock, nBudgetPaymentsStartBlock, nSuperblockStartBlock);
}

void UpdateDevnetSubsidyAndDiffParams(int nMinimumDifficultyBlocks, int nHighSubsidyBlocks, int nHighSubsidyFactor)
{
    globalChainParams->UpdateSubsidyAndDiffParams(nMinimumDifficultyBlocks, nHighSubsidyBlocks, nHighSubsidyFactor);
}

void UpdateDevnetLLMQChainLocks(Consensus::LLMQType llmqType)
{
    globalChainParams->UpdateLLMQChainLocks(llmqType);
}

void UpdateDevnetLLMQInstantSend(Consensus::LLMQType llmqType)
{
    globalChainParams->UpdateLLMQInstantSend(llmqType);
}

void UpdateLLMQTestParams(int size, int threshold)
{
    globalChainParams->UpdateLLMQTestParams(size, threshold);
}

void UpdateLLMQDevnetParams(int size, int threshold)
{
    globalChainParams->UpdateLLMQDevnetParams(size, threshold);
}
