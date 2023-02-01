// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/quickgames/dice.h>
#include <chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <llmq/params.h>
#include <tinyformat.h>
#include <util/ranges.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <versionbitsinfo.h>

#include <arith_uint256.h>

#include <assert.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

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

static CBlock FindDevNetGenesisBlock(const CBlock &prevBlock, const CAmount& reward)
{
    std::string devNetName = gArgs.GetDevNetName();
    assert(!devNetName.empty());

    CBlock block = CreateDevNetGenesisBlock(prevBlock.GetHash(), devNetName, prevBlock.nTime + 1, 0, prevBlock.nBits, reward);

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

void CChainParams::AddLLMQ(Consensus::LLMQType llmqType)
{
    assert(!HasLLMQ(llmqType));
    for (const auto& llmq_param : Consensus::available_llmqs) {
        if (llmq_param.type == llmqType) {
            consensus.llmqs.push_back(llmq_param);
            return;
        }
    }
    error("CChainParams::%s: unknown LLMQ type %d", __func__, static_cast<uint8_t>(llmqType));
    assert(false);
}

const Consensus::LLMQParams& CChainParams::GetLLMQ(Consensus::LLMQType llmqType) const
{
    for (const auto& llmq_param : consensus.llmqs) {
        if (llmq_param.type == llmqType) {
            return llmq_param;
        }
    }
    error("CChainParams::%s: unknown LLMQ type %d", __func__, static_cast<uint8_t>(llmqType));
    assert(false);
}

bool CChainParams::HasLLMQ(Consensus::LLMQType llmqType) const
{
    for (const auto& llmq_param : consensus.llmqs) {
        if (llmq_param.type == llmqType) {
            return true;
        }
    }
    return false;
}

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
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.nSubsidyHalvingInterval = 210240; // Note: actual number of blocks per calendar year with DGW v3 is ~200700 (for example 449750 - 249050)
        consensus.nMasternodePaymentsStartBlock = 100000; // not true, but it's ok as long as it's less then nMasternodePaymentsIncreaseBlock
        consensus.nMasternodePaymentsIncreaseBlock = 158000; // actual historical value
        consensus.nMasternodePaymentsIncreasePeriod = 576*30; // 17280 - actual historical value
        consensus.nInstantSendConfirmationsRequired = 6;
        consensus.nInstantSendKeepLock = 24;
        consensus.nBudgetPaymentsStartBlock = std::numeric_limits<int>::max();
        consensus.nBudgetPaymentsCycleBlocks = 43200; // (60*24*30)
        consensus.nBudgetPaymentsWindowBlocks = 100;
        consensus.nSuperblockStartBlock = std::numeric_limits<int>::max();
        consensus.nSuperblockStartHash = uint256(); // do not check this
        consensus.nSuperblockCycle = 43200; // (60*24*30)
        consensus.nSuperblockMaturityWindow = 1662; // ~(60*24*3)/2.6, ~3 days before actual Superblock is emitted
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nMasternodeMinimumConfirmations = 15;
        consensus.V18DeploymentHeight = std::numeric_limits<int>::max();
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("000001364c4ed20f1b240810b5aa91fee23ae9b64b6e746b594b611cf6d8c87b");
        consensus.BIP65Height = 751858;
        consensus.BIP66Height = 1; // 000002f68dbbf1fcfacb8f0b4e64083efdd2f07a906728ee068d573ffa5bcb4e
        consensus.CSVHeight = consensus.V18DeploymentHeight;
        consensus.BIP147Height = consensus.V18DeploymentHeight;
        consensus.DIP0001Height = consensus.V18DeploymentHeight;
        consensus.DIP0003Height = consensus.V18DeploymentHeight;
//        consensus.DIP0003EnforcementHeight = std::numeric_limits<int>::max();
        consensus.DIP0003EnforcementHash = uint256();
        consensus.DIP0008Height = consensus.V18DeploymentHeight;
        consensus.DIP0024Height = consensus.V18DeploymentHeight;

        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 25;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Proof of work parameters
        consensus.BRRHeight = 1374912; // 000000000000000c5a124f3eccfbe6e17876dca79cec9e63dfa70d269113c926
        consensus.MinBIP9WarningHeight = 1090656; // dip8 activation height + miner confirmation window
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
        consensus.nWagerrProtocolV5StartHeight = consensus.V18DeploymentHeight;     // Betting protocol v5 activation block
        consensus.nQuickGamesEndHeight = consensus.nWagerrProtocolV3StartHeight;    // Quick games: retired functionality
        consensus.nMaturityV2StartHeight = consensus.nWagerrProtocolV3StartHeight;  // Reduced block maturity required for spending coinstakes and betting payouts
        consensus.nKeysRotateHeight = consensus.nWagerrProtocolV3StartHeight;       // Rotate spork key, oracle keys and fee payout keys
        consensus.nPosStartHeight = 1002;
        consensus.nBlockStakeModifierV1A = 1000;
        consensus.nBlockStakeModifierV2 = 891276;
        consensus.nBlockTimeProtocolV2 = consensus.nWagerrProtocolV3StartHeight;
        consensus.ATPStartHeight = consensus.V18DeploymentHeight;

        // Proof of Stake parameters
        consensus.posLimit = uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 24
        consensus.posLimit_V2 = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
        consensus.nPosTargetSpacing = 1 * 60; // 1 minute
        consensus.nPosTargetTimespan = 40 * 60; // 40 minutes
        consensus.nTimeSlotLength = 15;
        consensus.nPosTargetTimespan_V2 = 2 * consensus.nTimeSlotLength * 60; // 30 minutes
        consensus.nStakeMinDepth = 600;
        consensus.nStakeMinAge = 60 * 60; // 1 hour

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
        m_assumed_blockchain_size = 45;
        m_assumed_chain_state_size = 1;

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
        AddLLMQ(Consensus::LLMQType::LLMQ_50_60);
        AddLLMQ(Consensus::LLMQType::LLMQ_60_75);
        AddLLMQ(Consensus::LLMQType::LLMQ_400_60);
        AddLLMQ(Consensus::LLMQType::LLMQ_400_85);
        AddLLMQ(Consensus::LLMQType::LLMQ_100_67);
        consensus.llmqTypeChainLocks = Consensus::LLMQType::LLMQ_400_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQType::LLMQ_50_60;
        consensus.llmqTypeDIP0024InstantSend = Consensus::LLMQType::LLMQ_60_75;
        consensus.llmqTypePlatform = Consensus::LLMQType::LLMQ_100_67;
        consensus.llmqTypeMnhf = Consensus::LLMQType::LLMQ_400_85;

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fRequireRoutableExternalIP = true;
        m_is_test_chain = false;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = false;
        nLLMQConnectionRetryTimeout = 60;
        m_is_mockable_chain = false;

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
            "Dice", // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            "Wm5om9hBJTyKqv5FkMSfZ2FDMeGp12fkTe", // Dev address
            400, // OMNO reward permille (40%)
            100); // Dev reward permille (10%)

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
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.nSubsidyHalvingInterval = 210240;
        consensus.nMasternodePaymentsStartBlock = 4010; // not true, but it's ok as long as it's less then nMasternodePaymentsIncreaseBlock
        consensus.nMasternodePaymentsIncreaseBlock = 4030;
        consensus.nMasternodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = 4100;
        consensus.nBudgetPaymentsCycleBlocks = 50;
        consensus.nBudgetPaymentsWindowBlocks = 2880;
        consensus.nSuperblockStartBlock = 4200; // NOTE: Should satisfy nSuperblockStartBlock > nBudgetPeymentsStartBlock
        consensus.nSuperblockStartHash = uint256(); // do not check this on testnet
        consensus.nSuperblockCycle = 24; // Superblocks can be issued hourly on testnet
        consensus.nSuperblockMaturityWindow = 24; // This is equal to SB cycle on testnet
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.V18DeploymentHeight = 1100000;
        consensus.BIP34Height = 3963;
        consensus.BIP34Hash = uint256S("0000065432f43b3efb23bd0f63fe33d00d02a5f36233fe1b982c08274d58ef12");
        consensus.BIP65Height = 600;
        consensus.BIP66Height = 1; // 0000065432f43b3efb23bd0f63fe33d00d02a5f36233fe1b982c08274d58ef12
        consensus.CSVHeight = consensus.V18DeploymentHeight;
        consensus.BIP147Height = consensus.V18DeploymentHeight;
        consensus.DIP0001Height = consensus.V18DeploymentHeight;
        consensus.DIP0003Height = consensus.V18DeploymentHeight;
//        consensus.DIP0003EnforcementHeight = std::numeric_limits<int>::max();
        consensus.DIP0003EnforcementHash = uint256();
        consensus.DIP0008Height = consensus.V18DeploymentHeight;
        consensus.DIP0024Height = consensus.V18DeploymentHeight;

        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 25;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        consensus.BRRHeight = 387500; // 0000001537dbfd09dea69f61c1f8b2afa27c8dc91c934e144797761c9f10367b
        consensus.MinBIP9WarningHeight = 80816;  // dip8 activation height + miner confirmation window
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
        consensus.nWagerrProtocolV1StartHeight = 1100;                          // Betting protocol v1 activation block
        consensus.nWagerrProtocolV2StartHeight = 1100;                          // Betting protocol v2 activation block
        consensus.nWagerrProtocolV3StartHeight = 2000;                          // Betting protocol v3 activation block
        consensus.nWagerrProtocolV4StartHeight = 405000;                        // Betting protocol v4 activation block
        consensus.nWagerrProtocolV5StartHeight = consensus.V18DeploymentHeight; // Betting protocol v5 activation block
        consensus.nQuickGamesEndHeight = 101650;
        consensus.nMaturityV2StartHeight = 38000;           // Reduced block maturity required for spending coinstakes and betting payouts
        consensus.nKeysRotateHeight = 102000;               // Rotate spork key, oracle keys and fee payout keys
        consensus.nPosStartHeight = 301;
        consensus.nBlockStakeModifierV1A = 1;
        consensus.nBlockStakeModifierV2 = 92500;
        consensus.nBlockTimeProtocolV2 = 139550;
        consensus.ATPStartHeight = consensus.V18DeploymentHeight;

        // Proof of stake parameters
        consensus.posLimit = uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 24
        consensus.posLimit_V2 = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
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
        consensus.nZerocoinStartHeight = std::numeric_limits<int>::max();
        consensus.nZerocoinStartTime = std::numeric_limits<int>::max();
        consensus.nBlockZerocoinV2 = 600;
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

        // Workarounds
        consensus.nSkipBetValidationStart = 5577;
        consensus.nSkipBetValidationEnd = 35619;
        
        // Chain parameters
        pchMessageStart[0] = 0x87;
        pchMessageStart[1] = 0x9e;
        pchMessageStart[2] = 0xd1;
        pchMessageStart[3] = 0x99;
        nDefaultPort = 55004;
        nPruneAfterHeight = 1000;
        nMaxBettingUndoDepth = 101;
        m_assumed_blockchain_size = 4;
        m_assumed_chain_state_size = 1;

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
        AddLLMQ(Consensus::LLMQType::LLMQ_50_60);
        AddLLMQ(Consensus::LLMQType::LLMQ_60_75);
        AddLLMQ(Consensus::LLMQType::LLMQ_400_60);
        AddLLMQ(Consensus::LLMQType::LLMQ_400_85);
        AddLLMQ(Consensus::LLMQType::LLMQ_100_67);
        consensus.llmqTypeChainLocks = Consensus::LLMQType::LLMQ_50_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQType::LLMQ_50_60;
        consensus.llmqTypeDIP0024InstantSend = Consensus::LLMQType::LLMQ_60_75;
        consensus.llmqTypePlatform = Consensus::LLMQType::LLMQ_100_67;
        consensus.llmqTypeMnhf = Consensus::LLMQType::LLMQ_50_60;

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fRequireRoutableExternalIP = true;
        m_is_test_chain = true;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = true;
        nLLMQConnectionRetryTimeout = 60;
        m_is_mockable_chain = false;

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
            { "TRNjH67Qfpfuhn3TFonqm2DNqDwwUsJ24T", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, 1063000 },
            { "TYijVoyFnJ8dt1SGHtMtn2wa34CEs8EVZq", strDevPayoutAddrNew, strOMNOPayoutAddrNew, consensus.nKeysRotateHeight, 1063000 },
            { "TBXdNxNw1t2kcCEWigDiDyVm3mG3TWCDz4", strDevPayoutAddrNew, strOMNOPayoutAddrNew, 1063000, std::numeric_limits<int>::max() },
            { "TSGvJLHNrNne96KMYsnF6L8nFnKSa2Vm2o", strDevPayoutAddrNew, strOMNOPayoutAddrNew, 1063000, std::numeric_limits<int>::max() },
        };

        quickGamesArr.clear();
        quickGamesArr.emplace_back(
            "Dice", // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            "TLceyDrdPLBu8DK6UZjKu4vCDUQBGPybcY", // Dev address
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
    explicit CDevNetParams(const ArgsManager& args) {
        strNetworkID = CBaseChainParams::DEVNET;
        consensus.nSubsidyHalvingInterval = 210240;
        consensus.nMasternodePaymentsStartBlock = 4010; // not true, but it's ok as long as it's less then nMasternodePaymentsIncreaseBlock
        consensus.nMasternodePaymentsIncreaseBlock = 4030;
        consensus.nMasternodePaymentsIncreasePeriod = 10;
        consensus.nInstantSendConfirmationsRequired = 2;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetPaymentsStartBlock = 4100;
        consensus.nBudgetPaymentsCycleBlocks = 144;
        consensus.nBudgetPaymentsWindowBlocks = 64;
        consensus.nSuperblockStartBlock = 4200;
        consensus.nSuperblockStartHash = uint256(); // do not check this on regtest
        consensus.nSuperblockCycle = 64;
        consensus.nSuperblockMaturityWindow = 24; // This is equal to SB cycle on devnet
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.V18DeploymentHeight = 600;
        consensus.BIP34Height = 1; // BIP34 activated immediately on devnet
        consensus.BIP65Height = 1; // BIP65 activated immediately on devnet
        consensus.BIP66Height = 1; // BIP66 activated immediately on devnet
        consensus.CSVHeight = consensus.V18DeploymentHeight;
        consensus.BIP147Height = consensus.V18DeploymentHeight;
        consensus.DIP0001Height = 2; // DIP0001 activated immediately on devnet
        consensus.DIP0003Height = 2; // DIP0003 activated immediately on devnet
//        consensus.DIP0003EnforcementHeight = 2; // DIP0003 activated immediately on devnet
        consensus.DIP0003EnforcementHash = uint256();
        consensus.DIP0008Height = 2; // DIP0008 activated immediately on devnet
        consensus.DIP0024Height = consensus.V18DeploymentHeight;
        consensus.BRRHeight = 300;
        consensus.MinBIP9WarningHeight = 2018; // dip8 activation height + miner confirmation window

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
        consensus.nWagerrProtocolV4StartHeight = std::numeric_limits<int>::max(); // Betting protocol v4 activation block
        consensus.nWagerrProtocolV5StartHeight = consensus.V18DeploymentHeight;   // Betting protocol v5 activation block
        consensus.nQuickGamesEndHeight = consensus.nWagerrProtocolV3StartHeight;
        consensus.nMaturityV2StartHeight = consensus.nWagerrProtocolV3StartHeight;          // Reduced block maturity required for spending coinstakes and betting payouts
        consensus.nKeysRotateHeight = 270;                                        // Rotate spork key, oracle keys and fee payout keys
        consensus.nPosStartHeight = 301;
        consensus.nBlockStakeModifierV1A = consensus.nPosStartHeight;
        consensus.nBlockStakeModifierV2 = 400;
        consensus.nBlockTimeProtocolV2 = 500;
        consensus.ATPStartHeight = consensus.V18DeploymentHeight;

        // Proof of Stake parameters
        consensus.posLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.posLimit_V2 = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20
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
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateDevnetSubsidyAndDiffParametersFromArgs(args);
        genesis = CreateGenesisBlock(1518696184, 4638953, 0x207fffff, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x174db003bb4ce38c3462e7cbd9598ae891011f0043bdaaddeb67d2b42247e530"));
        assert(genesis.hashMerkleRoot == uint256S("0xc4d06cf72583752c23b819fa8d8cededd1dad5733d413ea1f123f98a7db6af13"));

        devnetGenesis = FindDevNetGenesisBlock(genesis, 0 * COIN);
        consensus.hashDevnetGenesisBlock = devnetGenesis.GetHash();

        vFixedSeeds.clear();
        vSeeds.clear();
        //vSeeds.push_back(CDNSSeedData("wagerrevo.org",  "devnet-seed.wagerrevo.org"));

        // Testnet Wagerr addresses start with 'T'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);
        // Testnet Wagerr script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,125);
        // Testnet private keys start with '9' or 'c'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,177);
        // Testnet Wagerr BIP32 pubkeys start with 'DRKV' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = {0x3A, 0x80, 0x61, 0xA0};
        // Testnet Wagerr BIP32 prvkeys start with 'DRKP' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = {0x3A, 0x80, 0x58, 0x37};

        nExtCoinType = 1;

        // long living quorum params
        AddLLMQ(Consensus::LLMQType::LLMQ_50_60);
        AddLLMQ(Consensus::LLMQType::LLMQ_60_75);
        AddLLMQ(Consensus::LLMQType::LLMQ_400_60);
        AddLLMQ(Consensus::LLMQType::LLMQ_400_85);
        AddLLMQ(Consensus::LLMQType::LLMQ_100_67);
        AddLLMQ(Consensus::LLMQType::LLMQ_DEVNET);
        AddLLMQ(Consensus::LLMQType::LLMQ_DEVNET_DIP0024);
        consensus.llmqTypeChainLocks = Consensus::LLMQType::LLMQ_50_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQType::LLMQ_50_60;
        consensus.llmqTypeDIP0024InstantSend = Consensus::LLMQType::LLMQ_60_75;
        consensus.llmqTypePlatform = Consensus::LLMQType::LLMQ_100_67;
        consensus.llmqTypeMnhf = Consensus::LLMQType::LLMQ_50_60;

        UpdateDevnetLLMQChainLocksFromArgs(args);
        UpdateDevnetLLMQInstantSendFromArgs(args);
        UpdateDevnetLLMQInstantSendDIP0024FromArgs(args);
        UpdateLLMQDevnetParametersFromArgs(args);
        UpdateDevnetPowTargetSpacingFromArgs(args);

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fRequireRoutableExternalIP = true;
        m_is_test_chain = true;
        fAllowMultipleAddressesFromGroup = true;
        fAllowMultiplePorts = true;
        nLLMQConnectionRetryTimeout = 60;
        m_is_mockable_chain = false;

        nPoolMinParticipants = 2;
        nPoolMaxParticipants = 20;
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes

        vSporkAddresses = {"TNZgamuYWzNeupr9qD1To2rEBoEcbPA2x4"}; // 04b33722601343992c8a651fafa0f424c6ac90f797d3f58d90eebf96e817e9d7ca76a40e3c53b3d47f6f6a60b0d36dbb94ee630a5ad622f08d92782999fe7b043a
        nMinSporkKeys = 1;
        // devnets are started with no blocks and no MN, so we can't check for upgraded MN (as there are none)
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
            "Dice", // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs", // Dev address
            400, // OMNO reward permille (40%)
            100); // Dev reward permille (10%)

        checkpointData = (CCheckpointData) {
            {
                { 0, uint256S("174db003bb4ce38c3462e7cbd9598ae891011f0043bdaaddeb67d2b42247e530")},
                { 1, devnetGenesis.GetHash() },
            }
        };

        chainTxData = ChainTxData{
            devnetGenesis.GetBlockTime(), // * UNIX timestamp of devnet genesis block
            2,                            // * we only have 2 coinbase transactions when a devnet is started up
            0.01                          // * estimated number of transactions per second
        };
    }

    /**
     * Allows modifying the subsidy and difficulty devnet parameters.
     */
    void UpdateDevnetSubsidyAndDiffParameters(int nMinimumDifficultyBlocks, int nHighSubsidyBlocks, int nHighSubsidyFactor)
    {
        consensus.nMinimumDifficultyBlocks = nMinimumDifficultyBlocks;
        consensus.nHighSubsidyBlocks = nHighSubsidyBlocks;
        consensus.nHighSubsidyFactor = nHighSubsidyFactor;
    }
    void UpdateDevnetSubsidyAndDiffParametersFromArgs(const ArgsManager& args);

    /**
     * Allows modifying the LLMQ type for ChainLocks.
     */
    void UpdateDevnetLLMQChainLocks(Consensus::LLMQType llmqType)
    {
        consensus.llmqTypeChainLocks = llmqType;
    }
    void UpdateDevnetLLMQChainLocksFromArgs(const ArgsManager& args);

    /**
     * Allows modifying the LLMQ type for InstantSend.
     */
    void UpdateDevnetLLMQInstantSend(Consensus::LLMQType llmqType)
    {
        consensus.llmqTypeInstantSend = llmqType;
    }

    /**
     * Allows modifying the LLMQ type for InstantSend (DIP0024).
     */
    void UpdateDevnetLLMQDIP0024InstantSend(Consensus::LLMQType llmqType)
    {
        consensus.llmqTypeDIP0024InstantSend = llmqType;
    }

    /**
     * Allows modifying PowTargetSpacing
     */
    void UpdateDevnetPowTargetSpacing(int64_t nPowTargetSpacing)
    {
        consensus.nPowTargetSpacing = nPowTargetSpacing;
    }

    /**
     * Allows modifying parameters of the devnet LLMQ
     */
    void UpdateLLMQDevnetParameters(int size, int threshold)
    {
        auto params = ranges::find_if(consensus.llmqs, [](const auto& llmq){ return llmq.type == Consensus::LLMQType::LLMQ_DEVNET;});
        assert(params != consensus.llmqs.end());
        params->size = size;
        params->minSize = threshold;
        params->threshold = threshold;
        params->dkgBadVotesThreshold = threshold;
    }
    void UpdateLLMQDevnetParametersFromArgs(const ArgsManager& args);
    void UpdateDevnetLLMQInstantSendFromArgs(const ArgsManager& args);
    void UpdateDevnetLLMQInstantSendDIP0024FromArgs(const ArgsManager& args);
    void UpdateDevnetPowTargetSpacingFromArgs(const ArgsManager& args);
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const ArgsManager& args) {
        strNetworkID =  CBaseChainParams::REGTEST;
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
        consensus.nSuperblockMaturityWindow = 10; // This is equal to SB cycle on regtest
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 100;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.V18DeploymentHeight = 300;
        consensus.BIP34Height = 500; // BIP34 activated on regtest (Used in functional tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = consensus.V18DeploymentHeight; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in functional tests)
        consensus.CSVHeight = consensus.V18DeploymentHeight;
        consensus.BIP147Height = consensus.V18DeploymentHeight;
        consensus.DIP0001Height = 2000;
        consensus.DIP0003Height = 210;
//        consensus.DIP0003EnforcementHeight = 500;
        consensus.DIP0003EnforcementHash = uint256();
        consensus.DIP0008Height = 432;
        consensus.DIP0024Height = consensus.V18DeploymentHeight;
        consensus.BRRHeight = 2500; // see block_reward_reallocation_tests
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.nPowTargetTimespan = 24 * 60 * 60; // Wagerr: 1 day
        consensus.nPowTargetSpacing = 2.5 * 60; // Wagerr: 2.5 minutes
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;

        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 25;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;


        // Wagerr specific parameters
        // Proof of Stake parameters
        consensus.nPosStartHeight = 251;
        consensus.nBlockStakeModifierV1A = consensus.nPosStartHeight;
        consensus.nBlockStakeModifierV2 = 400;
        consensus.nBlockTimeProtocolV2 = 500;

        // Proof of stake parameters
        consensus.posLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.posLimit_V2 = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1
        consensus.nTimeSlotLength = 15;
        consensus.nPosTargetSpacing = 1 * 60; // 1 minute
        consensus.nPosTargetTimespan = 40 * 60; // 40 minutes
        consensus.nTimeSlotLength = 15;
        consensus.nPosTargetTimespan_V2 = 2 * consensus.nTimeSlotLength * 60; // 30 minutes
        consensus.nStakeMinDepth = 1;
        consensus.nStakeMinAge = 60 * 60; // 1 hour
        consensus.nBlockStakeModifierV1A = consensus.nPosStartHeight;
        consensus.nBlockStakeModifierV2 = consensus.V18DeploymentHeight;
        consensus.ATPStartHeight = consensus.V18DeploymentHeight;

        // ATP parameters
        consensus.WagerrAddrPrefix = "wagerrreg";
        consensus.strTokenManagementKey = "TDn9ZfHrYvRXyXC6KxRgN6ZRXgJH2JKZWe"; // TCH8Qby7krfugb2sFWzHQSEmTxBgzBSLkgPtt5EUnzDqfaX9dcsS
        consensus.nOpGroupNewRequiredConfirmations = 1;
        // Other
        consensus.nMaturityV1 = 100;
        consensus.nMaturityV2 = 60;
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
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateVersionBitsParametersFromArgs(args);
        UpdateDIP3ParametersFromArgs(args);
        UpdateDIP8ParametersFromArgs(args);
        UpdateBudgetParametersFromArgs(args);

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
        AddLLMQ(Consensus::LLMQType::LLMQ_TEST);
        AddLLMQ(Consensus::LLMQType::LLMQ_TEST_INSTANTSEND);
        AddLLMQ(Consensus::LLMQType::LLMQ_TEST_V18);
        AddLLMQ(Consensus::LLMQType::LLMQ_TEST_DIP0024);
        consensus.llmqTypeChainLocks = Consensus::LLMQType::LLMQ_TEST;
        consensus.llmqTypeInstantSend = Consensus::LLMQType::LLMQ_TEST_INSTANTSEND;
        consensus.llmqTypeDIP0024InstantSend = Consensus::LLMQType::LLMQ_TEST_DIP0024;
        consensus.llmqTypePlatform = Consensus::LLMQType::LLMQ_TEST;
        consensus.llmqTypeMnhf = Consensus::LLMQType::LLMQ_TEST;

        UpdateLLMQTestParametersFromArgs(args, Consensus::LLMQType::LLMQ_TEST);
        UpdateLLMQTestParametersFromArgs(args, Consensus::LLMQType::LLMQ_TEST_INSTANTSEND);

        fDefaultConsistencyChecks = true;
        fRequireStandard = true;
        fRequireRoutableExternalIP = false;
        m_is_test_chain = true;
        fAllowMultipleAddressesFromGroup = true;
        fAllowMultiplePorts = true;
        nLLMQConnectionRetryTimeout = 1; // must be lower then the LLMQ signing session timeout so that tests have control over failing behavior
        m_is_mockable_chain = true;

        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
        nPoolMinParticipants = 2;
        nPoolMaxParticipants = 20;

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
            "Dice", // Game name
            QuickGamesType::qgDice, // game type
            &quickgames::DiceHandler, // game bet handler
            &quickgames::DiceBetInfoParser, // bet info parser
            "TLuTVND9QbZURHmtuqD5ESECrGuB9jLZTs", // Dev address
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

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int64_t nWindowSize, int64_t nThresholdStart, int64_t nThresholdMin, int64_t nFalloffCoeff)
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
    void UpdateVersionBitsParametersFromArgs(const ArgsManager& args);

    /**
     * Allows modifying the DIP3 activation and enforcement height
     */
    void UpdateDIP3Parameters(int nActivationHeight, int nEnforcementHeight)
    {
        consensus.DIP0003Height = nActivationHeight;
        //consensus.DIP0003EnforcementHeight = nEnforcementHeight;
    }
    void UpdateDIP3ParametersFromArgs(const ArgsManager& args);

    /**
     * Allows modifying the DIP8 activation height
     */
    void UpdateDIP8Parameters(int nActivationHeight)
    {
        consensus.DIP0008Height = nActivationHeight;
    }
    void UpdateDIP8ParametersFromArgs(const ArgsManager& args);

    /**
     * Allows modifying the budget regtest parameters.
     */
    void UpdateBudgetParameters(int nMasternodePaymentsStartBlock, int nBudgetPaymentsStartBlock, int nSuperblockStartBlock)
    {
        consensus.nMasternodePaymentsStartBlock = nMasternodePaymentsStartBlock;
        consensus.nBudgetPaymentsStartBlock = nBudgetPaymentsStartBlock;
        consensus.nSuperblockStartBlock = nSuperblockStartBlock;
    }
    void UpdateBudgetParametersFromArgs(const ArgsManager& args);

    /**
     * Allows modifying parameters of the test LLMQ
     */
    void UpdateLLMQTestParameters(int size, int threshold, const Consensus::LLMQType llmqType)
    {
        auto params = ranges::find_if(consensus.llmqs, [llmqType](const auto& llmq){ return llmq.type == llmqType;});
        assert(params != consensus.llmqs.end());
        params->size = size;
        params->minSize = threshold;
        params->threshold = threshold;
        params->dkgBadVotesThreshold = threshold;
    }
    void UpdateLLMQTestParametersFromArgs(const ArgsManager& args, const Consensus::LLMQType llmqType);
};

void CRegTestParams::UpdateVersionBitsParametersFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-vbparams")) return;

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
        if (vDeploymentParams.size() != 3 && vDeploymentParams.size() != 5 && vDeploymentParams.size() != 7) {
            throw std::runtime_error("Version bits parameters malformed, expecting "
                    "<deployment>:<start>:<end> or "
                    "<deployment>:<start>:<end>:<window>:<threshold> or "
                    "<deployment>:<start>:<end>:<window>:<thresholdstart>:<thresholdmin>:<falloffcoeff>");
        }
        int64_t nStartTime, nTimeout, nWindowSize = -1, nThresholdStart = -1, nThresholdMin = -1, nFalloffCoeff = -1;
        if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }
        if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }
        if (vDeploymentParams.size() >= 5) {
            if (!ParseInt64(vDeploymentParams[3], &nWindowSize)) {
                throw std::runtime_error(strprintf("Invalid nWindowSize (%s)", vDeploymentParams[3]));
            }
            if (!ParseInt64(vDeploymentParams[4], &nThresholdStart)) {
                throw std::runtime_error(strprintf("Invalid nThresholdStart (%s)", vDeploymentParams[4]));
            }
        }
        if (vDeploymentParams.size() == 7) {
            if (!ParseInt64(vDeploymentParams[5], &nThresholdMin)) {
                throw std::runtime_error(strprintf("Invalid nThresholdMin (%s)", vDeploymentParams[5]));
            }
            if (!ParseInt64(vDeploymentParams[6], &nFalloffCoeff)) {
                throw std::runtime_error(strprintf("Invalid nFalloffCoeff (%s)", vDeploymentParams[6]));
            }
        }
        bool found = false;
        for (int j=0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout, nWindowSize, nThresholdStart, nThresholdMin, nFalloffCoeff);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld, window=%ld, thresholdstart=%ld, thresholdmin=%ld, falloffcoeff=%ld\n",
                          vDeploymentParams[0], nStartTime, nTimeout, nWindowSize, nThresholdStart, nThresholdMin, nFalloffCoeff);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}

void CRegTestParams::UpdateDIP3ParametersFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-dip3params")) return;

    std::string strParams = args.GetArg("-dip3params", "");
    std::vector<std::string> vParams;
    boost::split(vParams, strParams, boost::is_any_of(":"));
    if (vParams.size() != 2) {
        throw std::runtime_error("DIP3 parameters malformed, expecting <activation>:<enforcement>");
    }
    int nDIP3ActivationHeight, nDIP3EnforcementHeight;
    if (!ParseInt32(vParams[0], &nDIP3ActivationHeight)) {
        throw std::runtime_error(strprintf("Invalid activation height (%s)", vParams[0]));
    }
    if (!ParseInt32(vParams[1], &nDIP3EnforcementHeight)) {
        throw std::runtime_error(strprintf("Invalid enforcement height (%s)", vParams[1]));
    }
    LogPrintf("Setting DIP3 parameters to activation=%ld, enforcement=%ld\n", nDIP3ActivationHeight, nDIP3EnforcementHeight);
    UpdateDIP3Parameters(nDIP3ActivationHeight, nDIP3EnforcementHeight);
}

void CRegTestParams::UpdateDIP8ParametersFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-dip8params")) return;

    std::string strParams = args.GetArg("-dip8params", "");
    std::vector<std::string> vParams;
    boost::split(vParams, strParams, boost::is_any_of(":"));
    if (vParams.size() != 1) {
        throw std::runtime_error("DIP8 parameters malformed, expecting <activation>");
    }
    int nDIP8ActivationHeight;
    if (!ParseInt32(vParams[0], &nDIP8ActivationHeight)) {
        throw std::runtime_error(strprintf("Invalid activation height (%s)", vParams[0]));
    }
    LogPrintf("Setting DIP8 parameters to activation=%ld\n", nDIP8ActivationHeight);
    UpdateDIP8Parameters(nDIP8ActivationHeight);
}

void CRegTestParams::UpdateBudgetParametersFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-budgetparams")) return;

    std::string strParams = args.GetArg("-budgetparams", "");
    std::vector<std::string> vParams;
    boost::split(vParams, strParams, boost::is_any_of(":"));
    if (vParams.size() != 3) {
        throw std::runtime_error("Budget parameters malformed, expecting <masternode>:<budget>:<superblock>");
    }
    int nMasternodePaymentsStartBlock, nBudgetPaymentsStartBlock, nSuperblockStartBlock;
    if (!ParseInt32(vParams[0], &nMasternodePaymentsStartBlock)) {
        throw std::runtime_error(strprintf("Invalid masternode start height (%s)", vParams[0]));
    }
    if (!ParseInt32(vParams[1], &nBudgetPaymentsStartBlock)) {
        throw std::runtime_error(strprintf("Invalid budget start block (%s)", vParams[1]));
    }
    if (!ParseInt32(vParams[2], &nSuperblockStartBlock)) {
        throw std::runtime_error(strprintf("Invalid superblock start height (%s)", vParams[2]));
    }
    LogPrintf("Setting budget parameters to masternode=%ld, budget=%ld, superblock=%ld\n", nMasternodePaymentsStartBlock, nBudgetPaymentsStartBlock, nSuperblockStartBlock);
    UpdateBudgetParameters(nMasternodePaymentsStartBlock, nBudgetPaymentsStartBlock, nSuperblockStartBlock);
}

void CRegTestParams::UpdateLLMQTestParametersFromArgs(const ArgsManager& args, const Consensus::LLMQType llmqType)
{
    assert(llmqType == Consensus::LLMQType::LLMQ_TEST || llmqType == Consensus::LLMQType::LLMQ_TEST_INSTANTSEND);

    std::string cmd_param{"-llmqtestparams"}, llmq_name{"LLMQ_TEST"};
    if (llmqType == Consensus::LLMQType::LLMQ_TEST_INSTANTSEND) {
        cmd_param = "-llmqtestinstantsendparams";
        llmq_name = "LLMQ_TEST_INSTANTSEND";
    }

    if (!args.IsArgSet(cmd_param)) return;

    std::string strParams = args.GetArg(cmd_param, "");
    std::vector<std::string> vParams;
    boost::split(vParams, strParams, boost::is_any_of(":"));
    if (vParams.size() != 2) {
        throw std::runtime_error(strprintf("%s parameters malformed, expecting <size>:<threshold>", llmq_name));
    }
    int size, threshold;
    if (!ParseInt32(vParams[0], &size)) {
        throw std::runtime_error(strprintf("Invalid %s size (%s)", llmq_name, vParams[0]));
    }
    if (!ParseInt32(vParams[1], &threshold)) {
        throw std::runtime_error(strprintf("Invalid %s threshold (%s)", llmq_name, vParams[1]));
    }
    LogPrintf("Setting %s parameters to size=%ld, threshold=%ld\n", llmq_name, size, threshold);
    UpdateLLMQTestParameters(size, threshold, llmqType);
}

void CDevNetParams::UpdateDevnetSubsidyAndDiffParametersFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-minimumdifficultyblocks") && !args.IsArgSet("-highsubsidyblocks") && !args.IsArgSet("-highsubsidyfactor")) return;

    int nMinimumDifficultyBlocks = gArgs.GetArg("-minimumdifficultyblocks", consensus.nMinimumDifficultyBlocks);
    int nHighSubsidyBlocks = gArgs.GetArg("-highsubsidyblocks", consensus.nHighSubsidyBlocks);
    int nHighSubsidyFactor = gArgs.GetArg("-highsubsidyfactor", consensus.nHighSubsidyFactor);
    LogPrintf("Setting minimumdifficultyblocks=%ld, highsubsidyblocks=%ld, highsubsidyfactor=%ld\n", nMinimumDifficultyBlocks, nHighSubsidyBlocks, nHighSubsidyFactor);
    UpdateDevnetSubsidyAndDiffParameters(nMinimumDifficultyBlocks, nHighSubsidyBlocks, nHighSubsidyFactor);
}

void CDevNetParams::UpdateDevnetLLMQChainLocksFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-llmqchainlocks")) return;

    std::string strLLMQType = gArgs.GetArg("-llmqchainlocks", std::string(GetLLMQ(consensus.llmqTypeChainLocks).name));

    Consensus::LLMQType llmqType = Consensus::LLMQType::LLMQ_NONE;
    for (const auto& params : consensus.llmqs) {
        if (params.name == strLLMQType) {
            if (params.useRotation) {
                throw std::runtime_error("LLMQ type specified for -llmqchainlocks must NOT use rotation");
            }
            llmqType = params.type;
        }
    }
    if (llmqType == Consensus::LLMQType::LLMQ_NONE) {
        throw std::runtime_error("Invalid LLMQ type specified for -llmqchainlocks.");
    }
    LogPrintf("Setting llmqchainlocks to size=%ld\n", static_cast<uint8_t>(llmqType));
    UpdateDevnetLLMQChainLocks(llmqType);
}

void CDevNetParams::UpdateDevnetLLMQInstantSendFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-llmqinstantsend")) return;

    std::string strLLMQType = gArgs.GetArg("-llmqinstantsend", std::string(GetLLMQ(consensus.llmqTypeInstantSend).name));

    Consensus::LLMQType llmqType = Consensus::LLMQType::LLMQ_NONE;
    for (const auto& params : consensus.llmqs) {
        if (params.name == strLLMQType) {
            if (params.useRotation) {
                throw std::runtime_error("LLMQ type specified for -llmqinstantsend must NOT use rotation");
            }
            llmqType = params.type;
        }
    }
    if (llmqType == Consensus::LLMQType::LLMQ_NONE) {
        throw std::runtime_error("Invalid LLMQ type specified for -llmqinstantsend.");
    }
    LogPrintf("Setting llmqinstantsend to size=%ld\n", static_cast<uint8_t>(llmqType));
    UpdateDevnetLLMQInstantSend(llmqType);
}

void CDevNetParams::UpdateDevnetLLMQInstantSendDIP0024FromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-llmqinstantsenddip0024")) return;

    std::string strLLMQType = gArgs.GetArg("-llmqinstantsenddip0024", std::string(GetLLMQ(consensus.llmqTypeDIP0024InstantSend).name));

    Consensus::LLMQType llmqType = Consensus::LLMQType::LLMQ_NONE;
    for (const auto& params : consensus.llmqs) {
        if (params.name == strLLMQType) {
            if (!params.useRotation) {
                throw std::runtime_error("LLMQ type specified for -llmqinstantsenddip0024 must use rotation");
            }
            llmqType = params.type;
        }
    }
    if (llmqType == Consensus::LLMQType::LLMQ_NONE) {
        throw std::runtime_error("Invalid LLMQ type specified for -llmqinstantsenddip0024.");
    }
    LogPrintf("Setting llmqinstantsenddip0024 to size=%ld\n", static_cast<uint8_t>(llmqType));
    UpdateDevnetLLMQDIP0024InstantSend(llmqType);
}

void CDevNetParams::UpdateDevnetPowTargetSpacingFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-powtargetspacing")) return;

    std::string strPowTargetSpacing = gArgs.GetArg("-powtargetspacing", "");

    int64_t powTargetSpacing;
    if (!ParseInt64(strPowTargetSpacing, &powTargetSpacing)) {
        throw std::runtime_error(strprintf("Invalid parsing of powTargetSpacing (%s)", strPowTargetSpacing));
    }

    if (powTargetSpacing < 1) {
        throw std::runtime_error(strprintf("Invalid value of powTargetSpacing (%s)", strPowTargetSpacing));
    }

    LogPrintf("Setting powTargetSpacing to %ld\n", powTargetSpacing);
    UpdateDevnetPowTargetSpacing(powTargetSpacing);
}

void CDevNetParams::UpdateLLMQDevnetParametersFromArgs(const ArgsManager& args)
{
    if (!args.IsArgSet("-llmqdevnetparams")) return;

    std::string strParams = args.GetArg("-llmqdevnetparams", "");
    std::vector<std::string> vParams;
    boost::split(vParams, strParams, boost::is_any_of(":"));
    if (vParams.size() != 2) {
        throw std::runtime_error("LLMQ_DEVNET parameters malformed, expecting <size>:<threshold>");
    }
    int size, threshold;
    if (!ParseInt32(vParams[0], &size)) {
        throw std::runtime_error(strprintf("Invalid LLMQ_DEVNET size (%s)", vParams[0]));
    }
    if (!ParseInt32(vParams[1], &threshold)) {
        throw std::runtime_error(strprintf("Invalid LLMQ_DEVNET threshold (%s)", vParams[1]));
    }
    LogPrintf("Setting LLMQ_DEVNET parameters to size=%ld, threshold=%ld\n", size, threshold);
    UpdateLLMQDevnetParameters(size, threshold);
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::make_unique<CMainParams>();
    else if (chain == CBaseChainParams::TESTNET)
        return std::make_unique<CTestNetParams>();
    else if (chain == CBaseChainParams::DEVNET)
        return std::make_unique<CDevNetParams>(gArgs);
    else if (chain == CBaseChainParams::REGTEST)
        return std::make_unique<CRegTestParams>(gArgs);

    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}
