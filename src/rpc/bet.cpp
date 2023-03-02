// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net.h"
#include "validation.h"
#include "core_io.h"
#include "betting/bet.h"
#include "betting/bet_v2.h"
#include "betting/bet_db.h"
#include "rpc/server.h"
#include <boost/assign/list_of.hpp>

#include <univalue.h>

/**
 * Looks up a given map index for a given name. If found then it will return the mapping ID.
 * If its not found then create a new mapping ID and also indicate with a boolean that a new
 * mapping OP_CODE needs to be created and broadcast to the network.
 *
 * @param params The RPC params consisting of an map index name and name.
 * @param fHelp  Help text
 * @return
 */
extern UniValue getmappingid(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() < 2))
        throw std::runtime_error(
                "getmappingid\n"
                "\nGet a mapping ID from the specified mapping index.\n"

                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"mapping index id\": \"xxx\",  (numeric) The mapping index.\n"
                "    \"exists\": \"xxx\", (boolean) mapping id exists\n"
                "    \"mapping-index\": \"xxx\" (string) The index that was searched.\n"
                "  }\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getmappingid", "\"sport\" \"Football\"") + HelpExampleRpc("getmappingid", "\"sport\" \"Football\""));

    const std::string name{request.params[1].get_str()};
    const std::string mIndex{request.params[0].get_str()};
    const MappingType type{CMappingDB::FromTypeName(mIndex)};
    UniValue result{UniValue::VARR};
    UniValue mappings{UniValue::VOBJ};

    if (static_cast<int>(type) < 0 || CMappingDB::ToTypeName(type) != mIndex) {
        throw std::runtime_error("No mapping exist for the mapping index you provided.");
    }

    bool mappingFound{false};

    LOCK(cs_main);

    // Check the map for the string name.
    auto it = bettingsView->mappings->NewIterator();
    MappingKey key;
    for (it->Seek(CBettingDB::DbTypeToBytes(MappingKey{type, 0})); it->Valid() && (CBettingDB::BytesToDbType(it->Key(), key), key.nMType == type); it->Next()) {
        CMappingDB mapping{};
        CBettingDB::BytesToDbType(it->Value(), mapping);
        LogPrint(BCLog::BETTING, "%s - mapping - it=[%d,%d] nId=[%d] nMType=[%s] [%s]\n", __func__, key.nMType, key.nId, key.nId, CMappingDB::ToTypeName(key.nMType), mapping.sName);
        if (!mappingFound) {
            if (mapping.sName == name) {
                mappings.pushKV("mapping-id", (uint64_t) key.nId);
                mappings.pushKV("exists", true);
                mappings.pushKV("mapping-index", mIndex);
                mappingFound = true;
            }
        }
    }
    if (mappingFound)
        result.push_back(mappings);

    return result;
}

/**
 * Looks up a given map index for a given ID. If found then it will return the mapping name.
 * If its not found return an error message.
 *
 * @param params The RPC params consisting of an map index name and id.
 * @param fHelp  Help text
 * @return
 */
extern UniValue getmappingname(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() != 2))
        throw std::runtime_error(
                "getmappingname\n"
                "\nGet a mapping string name from the specified map index.\n"
                "1. Mapping type  (string, requied) Type of mapping (\"sports\", \"rounds\", \"teams\", \"tournaments\", \"individualSports\", \"contenders\").\n"
                "2. Mapping id    (numeric, requied) Mapping id.\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"mapping-type\": \"xxx\",  (string) The mapping type.\n"
                "    \"mapping-name\": \"xxx\",  (string) The mapping name.\n"
                "    \"exists\": \"xxx\", (boolean) mapping transaction created or not\n"
                "    \"mapping-index\": \"xxx\" (string) The index that was searched.\n"
                "  }\n"
                "]\n"

                "\nExamples:\n" +
                HelpExampleCli("getmappingname", "\"sport\" 0") + HelpExampleRpc("getmappingname", "\"sport\" 0"));

    const std::string mIndex{request.params[0].get_str()};
    const uint32_t id{static_cast<uint32_t>(request.params[1].get_int())};
    const MappingType type{CMappingDB::FromTypeName(mIndex)};
    UniValue result{UniValue::VARR};
    UniValue mapping{UniValue::VOBJ};

    if (CMappingDB::ToTypeName(type) != mIndex) {
        throw std::runtime_error("No mapping exist for the mapping index you provided.");
    }

    LOCK(cs_main);

    CMappingDB map{};
    if (bettingsView->mappings->Read(MappingKey{type, id}, map)) {
        mapping.pushKV("mapping-type", CMappingDB::ToTypeName(type));
        mapping.pushKV("mapping-name", map.sName);
        mapping.pushKV("exists", true);
        mapping.pushKV("mapping-index", static_cast<uint64_t>(id));
    }

    result.push_back(mapping);

    return result;
}

std::string GetPayoutTypeStr(PayoutType type)
{
    switch(type) {
        case PayoutType::bettingPayout:
            return std::string("Betting Payout");
        case PayoutType::bettingRefund:
            return std::string("Betting Refund");
        case PayoutType::bettingReward:
            return std::string("Betting Reward");
        case PayoutType::chainGamesPayout:
            return std::string("Chain Games Payout");
        case PayoutType::chainGamesRefund:
            return std::string("Chain Games Refund");
        case PayoutType::chainGamesReward:
            return std::string("Chain Games Reward");
        default:
            return std::string("Undefined Payout Type");
    }
}

UniValue CreatePayoutInfoResponse(const std::vector<std::pair<bool, CPayoutInfoDB>> vPayoutsInfo)
{
    UniValue responseArr{UniValue::VARR};
    for (auto info : vPayoutsInfo) {
        UniValue retObj{UniValue::VOBJ};
        if (info.first) { // if payout info was found - add info to array
            CPayoutInfoDB &payoutInfo = info.second;
            UniValue infoObj{UniValue::VOBJ};

            infoObj.pushKV("payoutType", GetPayoutTypeStr(payoutInfo.payoutType));
            infoObj.pushKV("betBlockHeight", (uint64_t) payoutInfo.betKey.blockHeight);
            infoObj.pushKV("betTxHash", payoutInfo.betKey.outPoint.hash.GetHex());
            infoObj.pushKV("betTxOut", (uint64_t) payoutInfo.betKey.outPoint.n);
            retObj.pushKV("found", UniValue{true});
            retObj.pushKV("payoutInfo", infoObj);
        }
        else {
            retObj.pushKV("found", UniValue{false});
            retObj.pushKV("payoutInfo", UniValue{UniValue::VOBJ});
        }

        responseArr.push_back(retObj);
    }
    return responseArr;
}

/**
 * Looks up a given payout tx hash and out number for getting payout info.
 * If not found return an empty array. If found - return array of info objects.
 *
 * @param params The RPC params consisting of an array of objects
 * @param fHelp  Help text
 * @return
 */
extern UniValue getpayoutinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() != 1))
        throw std::runtime_error(
                "getpayoutinfo\n"
                "\nGet an info for given  .\n"
                "1. Payout params  (array, requied)\n"
                "[\n"
                "  {\n"
                "    \"txHash\": hash (string, requied) The payout transaction hash.\n"
                "    \"nOut\": nOut (numeric, requied) The payout transaction out number.\n"
                "  }\n"
                "]\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"found\": flag (boolean) Indicate that expected payout was found.\n"
                "    \"payoutInfo\": object (object) Payout info object.\n"
                "      {\n"
                "        \"payoutType\": payoutType (string) Payout type: bet or chain game, payout or refund or reward.\n"
                "        \"betHeight\": height (numeric) Bet block height.\n"
                "        \"betTxHash\": hash (string) Bet transaction hash.\n"
                "        \"betOut\": nOut (numeric) Bet transaction out number.\n"
                "      }\n"
                "  }\n"
                "]\n"
                "\nExamples:\n" +
                HelpExampleCli("getpayoutinfo", "[{\"txHash\": 08746e1bdb6f4aebd7f1f3da25ac11e1cd3cacaf34cd2ad144e376b2e7f74d49, \"nOut\": 3}, {\"txHash\": 4c1e6b1a26808541e9e43c542adcc0eb1c67f2be41f2334ab1436029bf1791c0, \"nOut\": 4}]") +
                    HelpExampleRpc("getpayoutinfo", "[{\"txHash\": 08746e1bdb6f4aebd7f1f3da25ac11e1cd3cacaf34cd2ad144e376b2e7f74d49, \"nOut\": 3}, {\"txHash\": 4c1e6b1a26808541e9e43c542adcc0eb1c67f2be41f2334ab1436029bf1791c0, \"nOut\": 4}]"));

    UniValue paramsArr = request.params[0].get_array();
    std::vector<std::pair<bool, CPayoutInfoDB>> vPayoutsInfo;

    LOCK(cs_main);

    // parse payout params
    for (uint32_t i = 0; i < paramsArr.size(); i++) {
        const UniValue obj = paramsArr[i].get_obj();
        RPCTypeCheckObj(obj, boost::assign::map_list_of("txHash", UniValue::VSTR)("nOut", UniValue::VNUM));
        uint256 txHash = uint256S(find_value(obj, "txHash").get_str());
        uint32_t nOut = find_value(obj, "nOut").get_int();
        uint256 hashBlock;
        CTransactionRef tx = GetTransaction(nullptr, nullptr, txHash, Params().GetConsensus(), hashBlock, true);
        if (!tx) {
            vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{false, CPayoutInfoDB{}});
            continue;
        }
        if (hashBlock == uint256()) { // uncomfirmed tx
            vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{false, CPayoutInfoDB{}});
            continue;
        }
        CBlockIndex* pindex = LookupBlockIndex(hashBlock);
        uint32_t blockHeight;
        if (pindex)
            blockHeight = pindex->nHeight;

        CPayoutInfoDB payoutInfo;
        // try to find payout info from db
        if (!bettingsView->payoutsInfo->Read(PayoutInfoKey{blockHeight, COutPoint{txHash, nOut}}, payoutInfo)) {
            // not found
            vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{false, CPayoutInfoDB{}});
            continue;
        }
        vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{true, payoutInfo});
    }

    return CreatePayoutInfoResponse(vPayoutsInfo);
}

/**
 * Looks up a given block height for getting payouts info since this block height.
 * If not found return an empty array. If found - return array of info objects.
 *
 * @param params The RPC params consisting of an array of objects
 * @param fHelp  Help text
 * @return
 */
extern UniValue getpayoutinfosince(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() > 1))
        throw std::runtime_error(
                "getpayoutinfosince\n"
                "\nGet info for payouts in the specified block range.\n"
                "1. Last blocks (numeric, optional) default = 10.\n"
                "\nResult:\n"
                "[\n"
                "  {\n"
                "    \"found\": flag (boolean) Indicate that expected payout was found.\n"
                "    \"payoutInfo\": object (object) Payout info object.\n"
                "      {\n"
                "        \"payoutType\": payoutType (string) Payout type: bet or chain game, payout or refund or reward.\n"
                "        \"betHeight\": height (numeric) Bet block height.\n"
                "        \"betTxHash\": hash (string) Bet transaction hash.\n"
                "        \"betOut\": nOut (numeric) Bet transaction out number.\n"
                "      }\n"
                "  }\n"
                "]\n"
                "\nExamples:\n" +
                HelpExampleCli("getpayoutinfosince", "15") + HelpExampleRpc("getpayoutinfosince", "15"));

    std::vector<std::pair<bool, CPayoutInfoDB>> vPayoutsInfo;
    uint32_t nLastBlocks = 10;
    if (request.params.size() == 1) {
        nLastBlocks = request.params[0].get_int();
        if (nLastBlocks < 1)
            throw std::runtime_error("Invalid number of last blocks.");
    }

    LOCK(cs_main);

    int nCurrentHeight = ::ChainActive().Height();

    uint32_t startBlockHeight = static_cast<uint32_t>(nCurrentHeight) - nLastBlocks + 1;

    auto it = bettingsView->payoutsInfo->NewIterator();
    for (it->Seek(CBettingDB::DbTypeToBytes(PayoutInfoKey{startBlockHeight, COutPoint()})); it->Valid(); it->Next()) {
        PayoutInfoKey key;
        CPayoutInfoDB payoutInfo;
        CBettingDB::BytesToDbType(it->Key(), key);
        CBettingDB::BytesToDbType(it->Value(), payoutInfo);
        vPayoutsInfo.emplace_back(std::pair<bool, CPayoutInfoDB>{true, payoutInfo});
    }

    return CreatePayoutInfoResponse(vPayoutsInfo);
}

/**
 * Looks up a chain game info for a given ID.
 *
 * @param params The RPC params consisting of the event id.
 * @param fHelp  Help text
 * @return
 */
UniValue getchaingamesinfo(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "getchaingamesinfo ( \"eventID\" showWinner )\n"

            "\nArguments:\n"
            "1. eventID          (numeric) The event ID.\n"
            "2. showWinner       (bool, optional, default=false) Include a scan for the winner.\n");

    LOCK(cs_main);

    UniValue ret(UniValue::VARR);
    UniValue obj(UniValue::VOBJ);

    // Set default return values
    unsigned int eventID = request.params[0].get_int();
    int entryFee = 0;
    int totalFoundCGBets = 0;
    int gameStartTime = 0;
    int gameStartBlock = 0;
    int resultHeight = -1;
    CBetOut winningBetOut;
    bool winningBetFound = false;

    bool fShowWinner = false;
    if (request.params.size() > 1) {
        fShowWinner = request.params[1].get_bool();
    }

    CBlockIndex *BlocksIndex = NULL;
    int height = (Params().NetworkIDString() == CBaseChainParams::MAIN) ? ::ChainActive().Height() - 10500 : ::ChainActive().Height() - 14400;
    BlocksIndex = ::ChainActive()[height];

    CBlock block;
    while (BlocksIndex) {
        ReadBlockFromDisk(block, BlocksIndex, Params().GetConsensus());

        for (CTransactionRef& tx : block.vtx) {

            const CTxIn &txin = tx->vin[0];
            bool validTx = IsValidOracleTxIn(txin, height);

            // Check each TX out for values
            for (unsigned int i = 0; i < tx->vout.size(); i++) {
                const CTxOut &txout = tx->vout[i];

                auto cgBettingTx = ParseBettingTx(txout);

                if(cgBettingTx == nullptr) continue;

                auto txType = cgBettingTx->GetTxType();

                // Find any CChainGameEvents matching the specified id
                if (validTx && txType == cgEventTxType) {
                    CChainGamesEventTx* cgEvent = (CChainGamesEventTx*) cgBettingTx.get();
                    if (((unsigned int)cgEvent->nEventId) == eventID){
                        entryFee = cgEvent->nEntryFee;
                        gameStartTime = block.GetBlockTime();
                        gameStartBlock = BlocksIndex -> nHeight;
                    }
                }
                // Find a matching result transaction
                if (validTx && resultHeight == -1 && txType == cgResultTxType) {
                    CChainGamesResultTx* cgResult = (CChainGamesResultTx*) cgBettingTx.get();
                    if (cgResult->nEventId == (uint16_t)eventID) {
                        resultHeight = BlocksIndex->nHeight;
                    }
                }
                if (txType == cgBetTxType) {
                    CChainGamesBetTx* cgBet = (CChainGamesBetTx*) cgBettingTx.get();
                    if (((unsigned int)cgBet->nEventId) == eventID){
                        totalFoundCGBets = totalFoundCGBets + 1;
                    }
                }
            }
        }

        BlocksIndex = ::ChainActive().Next(BlocksIndex);
    }

    if (resultHeight > Params().GetConsensus().nWagerrProtocolV2StartHeight && fShowWinner) {
        std::vector<CBetOut> vExpectedCGLottoPayouts;
        std::vector<CPayoutInfoDB> vPayoutsInfo;
        GetCGLottoBetPayoutsV2(block, ::ChainstateActive().CoinsTip(), resultHeight, vExpectedCGLottoPayouts, vPayoutsInfo);
        for (auto lottoPayouts : vExpectedCGLottoPayouts) {
            if (!winningBetFound && lottoPayouts.nEventId == eventID) {
                winningBetOut = lottoPayouts;
                winningBetFound = true;
            }
        }
    }

    int potSize = totalFoundCGBets*entryFee;

    obj.pushKV("pot-size", potSize);
    obj.pushKV("entry-fee", entryFee);
    obj.pushKV("start-block", gameStartBlock);
    obj.pushKV("start-time", gameStartTime);
    obj.pushKV("total-bets", totalFoundCGBets);
    obj.pushKV("result-trigger-block", resultHeight);
    if (winningBetFound) {
        CTxDestination address;
        if (ExtractDestination(winningBetOut.scriptPubKey, address)) {
            obj.pushKV("winner", EncodeDestination(address));
            obj.pushKV("winnings", ValueFromAmount(winningBetOut.nValue));
        }

    }
    obj.pushKV("network", Params().NetworkIDString());

    return obj;
}

/**
 * Get total liability for each event that is currently active.
 *
 * @param params The RPC params consisting of the event id.
 * @param fHelp  Help text
 * @return
 */
UniValue getalleventliabilities(const JSONRPCRequest& request)
{
  if (request.fHelp || (request.params.size() != 0))
        throw std::runtime_error(
            "geteventliability\n"
            "Return the payout liabilities for all events.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"event-id\": \"xxx\", (numeric) The id of the event.\n"
            "    \"event-status\": \"status\", (string) The status of the event (running | resulted).\n"
            "    \"moneyline-home-bets\": \"xxx\", (numeric) The number of bets to moneyline home (parlays included).\n"
            "    \"moneyline-home-liability\": \"xxx\", (numeric) The moneyline home potentional liability (without parlays).\n"
            "    \"moneyline-away-bets\": \"xxx\", (numeric) The number of bets to moneyline away (parlays included).\n"
            "    \"moneyline-away-liability\": \"xxx\", (numeric) The moneyline away potentional liability (without parlays).\n"
            "    \"moneyline-draw-bets\": \"xxx\", (numeric) The number of bets to moneyline draw (parlays included).\n"
            "    \"moneyline-draw-liability\": \"xxx\", (numeric) The moneyline draw potentional liability (without parlays).\n"
            "    \"spread-home-bets\": \"xxx\", (numeric) The number of bets to spread home (parlays included).\n"
            "    \"spread-home-liability\": \"xxx\", (numeric) The spreads home potentional liability (without parlays).\n"
            "    \"spread-away-bets\": \"xxx\", (numeric) The number of bets to spread away (parlays included).\n"
            "    \"spread-away-liability\": \"xxx\", (numeric) The spread away potentional liability (without parlays).\n"
            "    \"spread-push-bets\": \"xxx\", (numeric) The number of bets to spread push (parlays included).\n"
            "    \"spread-push-liability\": \"xxx\", (numeric) The spread push potentional liability (without parlays).\n"
            "    \"total-over-bets\": \"xxx\", (numeric) The number of bets to total over (parlays included).\n"
            "    \"total-over-liability\": \"xxx\", (numeric) The total over potentional liability (without parlays).\n"
            "    \"total-under-bets\": \"xxx\", (numeric) The number of bets to total under (parlays included).\n"
            "    \"total-under-liability\": \"xxx\", (numeric) The total under potentional liability (without parlays).\n"
            "    \"total-push-bets\": \"xxx\", (numeric) The number of bets to total push (parlays included).\n"
            "    \"total-push-liability\": \"xxx\", (numeric) The total push potentional liability (without parlays).\n"
            "    ]\n"
            "  }\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("getalleventliabilities", "") + HelpExampleRpc("getalleventliabilities", ""));

    LOCK(cs_main);

    UniValue result{UniValue::VARR};

    auto it = bettingsView->events->NewIterator();
    for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        CPeerlessExtendedEventDB plEvent;
        CBettingDB::BytesToDbType(it->Value(), plEvent);

        // Only list active events.
        /*
        if (plEvent.nEventCreationHeight < ::ChainActive().Height() - Params().BetBlocksIndexTimespan()) {
            continue;
        }
        */
        // Only list active events.
        if ((time_t) plEvent.nStartTime < std::time(0)) {
            continue;
        }

        UniValue event(UniValue::VOBJ);

        event.pushKV("event-id", (uint64_t) plEvent.nEventId);
        event.pushKV("event-status", "running");
        event.pushKV("moneyline-home-bets", (uint64_t) plEvent.nMoneyLineHomeBets);
        event.pushKV("moneyline-home-liability", (uint64_t) plEvent.nMoneyLineHomePotentialLiability);
        event.pushKV("moneyline-away-bets", (uint64_t) plEvent.nMoneyLineAwayBets);
        event.pushKV("moneyline-away-liability", (uint64_t) plEvent.nMoneyLineAwayPotentialLiability);
        event.pushKV("moneyline-draw-bets", (uint64_t) plEvent.nMoneyLineDrawBets);
        event.pushKV("moneyline-draw-liability", (uint64_t) plEvent.nMoneyLineDrawPotentialLiability);
        event.pushKV("spread-home-bets", (uint64_t) plEvent.nSpreadHomeBets);
        event.pushKV("spread-home-liability", (uint64_t) plEvent.nSpreadHomePotentialLiability);
        event.pushKV("spread-home-bets", (uint64_t) plEvent.nSpreadHomeBets);
        event.pushKV("spread-home-liability", (uint64_t) plEvent.nSpreadHomePotentialLiability);
        event.pushKV("spread-away-bets", (uint64_t) plEvent.nSpreadAwayBets);
        event.pushKV("spread-away-liability", (uint64_t) plEvent.nSpreadAwayPotentialLiability);
        event.pushKV("spread-push-bets", (uint64_t) plEvent.nSpreadPushBets);
        event.pushKV("spread-push-liability", (uint64_t) plEvent.nSpreadPushPotentialLiability);
        event.pushKV("total-over-bets", (uint64_t) plEvent.nTotalOverBets);
        event.pushKV("total-over-liability", (uint64_t) plEvent.nTotalOverPotentialLiability);
        event.pushKV("total-under-bets", (uint64_t) plEvent.nTotalUnderBets);
        event.pushKV("total-under-liability", (uint64_t) plEvent.nTotalUnderPotentialLiability);
        event.pushKV("total-push-bets", (uint64_t) plEvent.nTotalPushBets);
        event.pushKV("total-push-liability", (uint64_t) plEvent.nTotalPushPotentialLiability);

        result.push_back(event);
    }

    return result;
}

/**
 * Get total liability for each event that is currently active.
 *
 * @param params The RPC params consisting of the event id.
 * @param fHelp  Help text
 * @return
 */
UniValue geteventliability(const JSONRPCRequest& request)
{
  if (request.fHelp || (request.params.size() != 1))
        throw std::runtime_error(
            "geteventliability\n"
            "Return the payout of each event.\n"
            "\nArguments:\n"
            "1. Event id (numeric, required) The event id required for get liability.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"event-id\": \"xxx\", (numeric) The id of the event.\n"
            "    \"event-status\": \"status\", (string) The status of the event (running | resulted).\n"
            "    \"moneyline-home-bets\": \"xxx\", (numeric) The number of bets to moneyline home (parlays included).\n"
            "    \"moneyline-home-liability\": \"xxx\", (numeric) The moneyline home potentional liability (without parlays).\n"
            "    \"moneyline-away-bets\": \"xxx\", (numeric) The number of bets to moneyline away (parlays included).\n"
            "    \"moneyline-away-liability\": \"xxx\", (numeric) The moneyline away potentional liability (without parlays).\n"
            "    \"moneyline-draw-bets\": \"xxx\", (numeric) The number of bets to moneyline draw (parlays included).\n"
            "    \"moneyline-draw-liability\": \"xxx\", (numeric) The moneyline draw potentional liability (without parlays).\n"
            "    \"spread-home-bets\": \"xxx\", (numeric) The number of bets to spread home (parlays included).\n"
            "    \"spread-home-liability\": \"xxx\", (numeric) The spreads home potentional liability (without parlays).\n"
            "    \"spread-away-bets\": \"xxx\", (numeric) The number of bets to spread away (parlays included).\n"
            "    \"spread-away-liability\": \"xxx\", (numeric) The spread away potentional liability (without parlays).\n"
            "    \"spread-push-bets\": \"xxx\", (numeric) The number of bets to spread push (parlays included).\n"
            "    \"spread-push-liability\": \"xxx\", (numeric) The spread push potentional liability (without parlays).\n"
            "    \"total-over-bets\": \"xxx\", (numeric) The number of bets to total over (parlays included).\n"
            "    \"total-over-liability\": \"xxx\", (numeric) The total over potentional liability (without parlays).\n"
            "    \"total-under-bets\": \"xxx\", (numeric) The number of bets to total under (parlays included).\n"
            "    \"total-under-liability\": \"xxx\", (numeric) The total under potentional liability (without parlays).\n"
            "    \"total-push-bets\": \"xxx\", (numeric) The number of bets to total push (parlays included).\n"
            "    \"total-push-liability\": \"xxx\", (numeric) The total push potentional liability (without parlays).\n"
            "    ]\n"
            "  }\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("geteventliability", "10") + HelpExampleRpc("geteventliability", "10"));

    LOCK(cs_main);

    uint32_t eventId = static_cast<uint32_t>(request.params[0].get_int());

    UniValue event(UniValue::VOBJ);

    CPeerlessExtendedEventDB plEvent;
    if (bettingsView->events->Read(EventKey{eventId}, plEvent)) {

        event.pushKV("event-id", (uint64_t) plEvent.nEventId);
        if (!bettingsView->results->Exists(ResultKey{eventId})) {
            event.pushKV("event-status", "running");
            event.pushKV("moneyline-home-bets", (uint64_t) plEvent.nMoneyLineHomeBets);
            event.pushKV("moneyline-home-liability", (uint64_t) plEvent.nMoneyLineHomePotentialLiability);
            event.pushKV("moneyline-away-bets", (uint64_t) plEvent.nMoneyLineAwayBets);
            event.pushKV("moneyline-away-liability", (uint64_t) plEvent.nMoneyLineAwayPotentialLiability);
            event.pushKV("moneyline-draw-bets", (uint64_t) plEvent.nMoneyLineDrawBets);
            event.pushKV("moneyline-draw-liability", (uint64_t) plEvent.nMoneyLineDrawPotentialLiability);
            event.pushKV("spread-home-bets", (uint64_t) plEvent.nSpreadHomeBets);
            event.pushKV("spread-home-liability", (uint64_t) plEvent.nSpreadHomePotentialLiability);
            event.pushKV("spread-home-bets", (uint64_t) plEvent.nSpreadHomeBets);
            event.pushKV("spread-home-liability", (uint64_t) plEvent.nSpreadHomePotentialLiability);
            event.pushKV("spread-away-bets", (uint64_t) plEvent.nSpreadAwayBets);
            event.pushKV("spread-away-liability", (uint64_t) plEvent.nSpreadAwayPotentialLiability);
            event.pushKV("spread-push-bets", (uint64_t) plEvent.nSpreadPushBets);
            event.pushKV("spread-push-liability", (uint64_t) plEvent.nSpreadPushPotentialLiability);
            event.pushKV("total-over-bets", (uint64_t) plEvent.nTotalOverBets);
            event.pushKV("total-over-liability", (uint64_t) plEvent.nTotalOverPotentialLiability);
            event.pushKV("total-under-bets", (uint64_t) plEvent.nTotalUnderBets);
            event.pushKV("total-under-liability", (uint64_t) plEvent.nTotalUnderPotentialLiability);
            event.pushKV("total-push-bets", (uint64_t) plEvent.nTotalPushBets);
            event.pushKV("total-push-liability", (uint64_t) plEvent.nTotalPushPotentialLiability);
        }
        else {
            event.pushKV("event-status", "resulted");
        }
    }

    return event;
}

/**
 * Get total liability for each field event that is currently active.
 *
 * @param params The RPC params consisting of the field event id.
 * @param fHelp  Help text
 * @return
 */
UniValue getfieldeventliability(const JSONRPCRequest& request)
{
  if (request.fHelp || (request.params.size() != 1))
        throw std::runtime_error(
            "getfieldeventliability\n"
            "Return the payout of each field event.\n"
            "\nArguments:\n"
            "1. FieldEvent id (numeric, required) The field event id required for get liability.\n"

            "\nResult:\n"
            "  {\n"
            "    \"event-id\": \"xxx\", (numeric) The id of the field event.\n"
            "    \"event-status\": \"status\", (string) The status of the event (running | resulted).\n"
            "    \"contenders\":\n"
            "      [\n"
            "         {\n"
            "           \"contender-id\" : xxx (numeric) contender id,\n"
            "           \"outright-bets\": \"xxx\", (numeric) The number of bets to outright market (parlays included).\n"
            "           \"outright-liability\": \"xxx\", (numeric) The outright market potentional liability (without parlays).\n"
            "         }\n"
            "      ]\n"
            "  }\n"

            "\nExamples:\n" +
            HelpExampleCli("getfieldeventliability", "10") + HelpExampleRpc("getfieldeventliability", "10"));

    LOCK(cs_main);

    uint32_t eventId = static_cast<uint32_t>(request.params[0].get_int());
    UniValue vEvent(UniValue::VOBJ);
    CFieldEventDB fEvent;
    if (bettingsView->fieldEvents->Read(FieldEventKey{eventId}, fEvent)) {
        vEvent.pushKV("event-id", (uint64_t) fEvent.nEventId);
        if (!bettingsView->fieldResults->Exists(FieldResultKey{eventId})) {
            vEvent.pushKV("event-status", "running");
            UniValue vContenders(UniValue::VARR);
            for (const auto& contender : fEvent.contenders) {
                UniValue vContender(UniValue::VOBJ);
                vContender.pushKV("contender-id", (uint64_t) contender.first);
                vContender.pushKV("outright-bets", (uint64_t) contender.second.nOutrightBets);
                vContender.pushKV("outright-liability", (uint64_t) contender.second.nOutrightPotentialLiability);
                vContender.pushKV("place-bets", (uint64_t) contender.second.nPlaceBets);
                vContender.pushKV("place-liability", (uint64_t) contender.second.nPlacePotentialLiability);
                vContender.pushKV("show-bets", (uint64_t) contender.second.nShowBets);
                vContender.pushKV("show-liability", (uint64_t) contender.second.nShowPotentialLiability);
                vContenders.push_back(vContender);
            }
            vEvent.pushKV("contenders", vContenders);
        }
        else {
            vEvent.pushKV("event-status", "resulted");
        }
    }

    return vEvent;
}

UniValue listbetsdb(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "listbetsdb\n"
            "\nGet bets form bets DB.\n"

            "\nArguments:\n"
            "1. \"includeHandled\"   (bool, optional) Include bets that are already handled (default: false).\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"legs\":\n"
            "      [\n"
            "        {\n"
            "          \"event-id\": id,\n"
            "          \"outcome\": type,\n"
            "          \"lockedEvent\": {\n"
            "            \"homeOdds\": homeOdds\n"
            "            \"awayOdds\": awayOdds\n"
            "            \"drawOdds\": drawOdds\n"
            "            \"spreadVersion\": spreadVersion\n"
            "            \"spreadPoints\": spreadPoints\n"
            "            \"spreadHomeOdds\": spreadHomeOdds\n"
            "            \"spreadAwayOdds\": spreadAwayOdds\n"
            "            \"totalPoints\": totalPoints\n"
            "            \"totalOverOdds\": totalOverOdds\n"
            "            \"totalUnderOdds\": totalUnderOdds\n"
            "          }\n"
            "        },\n"
            "        ...\n"
            "      ],                          (list) The list of legs.\n"
            "    \"address\": playerAddress    (string) The player address.\n"
            "    \"amount\": x.xxx,            (numeric) The amount bet in WGR.\n"
            "    \"time\":\"betting time\",    (string) The betting time.\n"
            "  },\n"
            "  ...\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listbetsdb", "true"));

    UniValue ret(UniValue::VARR);

    bool includeHandled = false;

    if (request.params.size() > 0) {
        includeHandled = request.params[0].get_bool();
    }

    LOCK(cs_main);

    auto it = bettingsView->bets->NewIterator();
    for(it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        PeerlessBetKey key;
        CPeerlessBetDB uniBet;
        CBettingDB::BytesToDbType(it->Value(), uniBet);
        CBettingDB::BytesToDbType(it->Key(), key);

        if (!includeHandled && uniBet.IsCompleted()) continue;

        UniValue uValue(UniValue::VOBJ);
        UniValue uLegs(UniValue::VARR);

        for (uint32_t i = 0; i < uniBet.legs.size(); i++) {
            auto &leg = uniBet.legs[i];
            auto &lockedEvent = uniBet.lockedEvents[i];
            UniValue uLeg(UniValue::VOBJ);
            UniValue uLockedEvent(UniValue::VOBJ);
            uLeg.pushKV("event-id", (uint64_t) leg.nEventId);
            uLeg.pushKV("outcome", (uint64_t) leg.nOutcome);
            uLockedEvent.pushKV("homeOdds", (uint64_t) lockedEvent.nHomeOdds);
            uLockedEvent.pushKV("awayOdds", (uint64_t) lockedEvent.nAwayOdds);
            uLockedEvent.pushKV("drawOdds", (uint64_t) lockedEvent.nDrawOdds);
            uLockedEvent.pushKV("spreadPoints", (int64_t) lockedEvent.nSpreadPoints);
            uLockedEvent.pushKV("spreadHomeOdds", (uint64_t) lockedEvent.nSpreadHomeOdds);
            uLockedEvent.pushKV("spreadAwayOdds", (uint64_t) lockedEvent.nSpreadAwayOdds);
            uLockedEvent.pushKV("totalPoints", (uint64_t) lockedEvent.nTotalPoints);
            uLockedEvent.pushKV("totalOverOdds", (uint64_t) lockedEvent.nTotalOverOdds);
            uLockedEvent.pushKV("totalUnderOdds", (uint64_t) lockedEvent.nTotalUnderOdds);
            uLeg.pushKV("lockedEvent", uLockedEvent);
            uLegs.push_back(uLeg);
        }
        uValue.pushKV("betBlockHeight", (uint64_t) key.blockHeight);
        uValue.pushKV("betTxHash", key.outPoint.hash.GetHex());
        uValue.pushKV("betTxOut", (uint64_t) key.outPoint.n);
        uValue.pushKV("legs", uLegs);
        uValue.pushKV("address", EncodeDestination(uniBet.playerAddress));
        uValue.pushKV("amount", ValueFromAmount(uniBet.betAmount));
        uValue.pushKV("time", (uint64_t) uniBet.betTime);
        ret.push_back(uValue);
    }

    return ret;
}

// TODO The Wagerr functions in this file are being placed here for speed of
// implementation, but should be moved to more appropriate locations once time
// allows.
UniValue listevents(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() > 2))
        throw std::runtime_error(
            "listevents\n"
            "\nGet live Wagerr events.\n"
            "\nArguments:\n"
            "1. \"openedOnly\" (bool, optional) Default - false. Gets only events which has no result.\n"
            "2. \"sportFilter\" (string, optional) Gets only events with input sport name.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": \"xxx\",         (string) The event ID\n"
            "    \"name\": \"xxx\",       (string) The name of the event\n"
            "    \"round\": \"xxx\",      (string) The round of the event\n"
            "    \"starting\": n,         (numeric) When the event will start\n"
            "    \"teams\": [\n"
            "      {\n"
            "        \"name\": \"xxxx\",  (string) Team to win\n"
            "        \"odds\": n          (numeric) Odds to win\n"
            "      }\n"
            "      ,...\n"
            "    ]\n"
            "  }\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listevents", "") +
            HelpExampleCli("listevents", "true" "football") +
            HelpExampleRpc("listevents", "false" "tennis"));

    UniValue result{UniValue::VARR};

    std::string sportFilter = "";
    bool openedOnly = true;

    if (request.params.size() > 0) {
        openedOnly = request.params[0].get_bool();
    }
    if (request.params.size() > 1) {
        sportFilter = request.params[0].get_str();
    }

    LOCK(cs_main);

    auto it = bettingsView->events->NewIterator();
    for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        CPeerlessExtendedEventDB plEvent;
        CMappingDB mapping;
        CBettingDB::BytesToDbType(it->Value(), plEvent);

        if (!bettingsView->mappings->Read(MappingKey{sportMapping, plEvent.nSport}, mapping))
            continue;

        std::string sport = mapping.sName;

        // if event filter is set the don't list event if it doesn't match the filter.
        if (!sportFilter.empty() && sportFilter != sport)
            continue;

        /*
        // Only list active events.
        if ((time_t) plEvent.nStartTime < std::time(0)) {
            continue;
        }
        */

        // list only unresulted events
        if (openedOnly && bettingsView->results->Exists(ResultKey{plEvent.nEventId}))
            continue;

        //std::string round    = roundsIndex.find(plEvent.nStage)->second.sName;
        if (!bettingsView->mappings->Read(MappingKey{tournamentMapping, plEvent.nTournament}, mapping))
            continue;
        std::string tournament = mapping.sName;
        if (!bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nHomeTeam}, mapping))
            continue;
        std::string homeTeam = mapping.sName;
        if (!bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nAwayTeam}, mapping))
            continue;
        std::string awayTeam = mapping.sName;

        UniValue evt(UniValue::VOBJ);

        evt.pushKV("event_id", (uint64_t) plEvent.nEventId);
        evt.pushKV("sport", sport);
        evt.pushKV("tournament", tournament);
        //evt.pushKV("round", "");

        evt.pushKV("starting", (uint64_t) plEvent.nStartTime);
        evt.pushKV("tester", (uint64_t) plEvent.nAwayTeam);

        UniValue teams(UniValue::VOBJ);

        teams.pushKV("home", homeTeam);
        teams.pushKV("away", awayTeam);

        evt.pushKV("teams", teams);

        UniValue odds(UniValue::VARR);

        UniValue mlOdds(UniValue::VOBJ);
        UniValue spreadOdds(UniValue::VOBJ);
        UniValue totalsOdds(UniValue::VOBJ);

        mlOdds.pushKV("mlHome", (uint64_t) plEvent.nHomeOdds);
        mlOdds.pushKV("mlAway", (uint64_t) plEvent.nAwayOdds);
        mlOdds.pushKV("mlDraw", (uint64_t) plEvent.nDrawOdds);

        if (plEvent.nEventCreationHeight < Params().GetConsensus().nWagerrProtocolV3StartHeight) {
            spreadOdds.pushKV("favorite", plEvent.fLegacyInitialHomeFavorite ? "home" : "away");
        } else {
            spreadOdds.pushKV("favorite", plEvent.nHomeOdds <= plEvent.nAwayOdds ? "home" : "away");
        }
        spreadOdds.pushKV("spreadPoints", (int64_t) plEvent.nSpreadPoints);
        spreadOdds.pushKV("spreadHome", (uint64_t) plEvent.nSpreadHomeOdds);
        spreadOdds.pushKV("spreadAway", (uint64_t) plEvent.nSpreadAwayOdds);

        totalsOdds.pushKV("totalsPoints", (uint64_t) plEvent.nTotalPoints);
        totalsOdds.pushKV("totalsOver", (uint64_t) plEvent.nTotalOverOdds);
        totalsOdds.pushKV("totalsUnder", (uint64_t) plEvent.nTotalUnderOdds);

        odds.push_back(mlOdds);
        odds.push_back(spreadOdds);
        odds.push_back(totalsOdds);

        evt.pushKV("odds", odds);

        result.push_back(evt);
    }

    return result;
}

UniValue listeventsdebug(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() > 0))
        throw std::runtime_error(
            "listeventsdebug\n"
            "\nGet all Wagerr events from db.\n"

            "\nResult:\n"

            "\nExamples:\n" +
            HelpExampleCli("listeventsdebug", "") + HelpExampleRpc("listeventsdebug", ""));

    UniValue result{UniValue::VARR};

    auto time = std::time(0);

    LOCK(cs_main);

    auto it = bettingsView->events->NewIterator();
    for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        CPeerlessExtendedEventDB plEvent;
        CMappingDB mapping;
        CBettingDB::BytesToDbType(it->Value(), plEvent);

        std::stringstream strStream;

        auto started = ((time_t) plEvent.nStartTime < time) ? std::string("true") : std::string("false");

        strStream << "eventId = " << plEvent.nEventId << ", sport: " << plEvent.nSport << ", tournament: " << plEvent.nTournament << ", round: " << plEvent.nStage << ", home: " << plEvent.nHomeTeam << ", away: " << plEvent.nAwayTeam
            << ", homeOdds: " << plEvent.nHomeOdds << ", awayOdds: " << plEvent.nAwayOdds << ", drawOdds: " << plEvent.nDrawOdds
            << ", spreadPoints: " << plEvent.nSpreadPoints << ", spreadHomeOdds: " << plEvent.nSpreadHomeOdds << ", spreadAwayOdds: " << plEvent.nSpreadAwayOdds
            << ", totalPoints: " << plEvent.nTotalPoints << ", totalOverOdds: " << plEvent.nTotalOverOdds << ", totalUnderOdds: " << plEvent.nTotalUnderOdds
            << ", started: " << started << ".";

        if (!bettingsView->mappings->Read(MappingKey{sportMapping, plEvent.nSport}, mapping)) {
            strStream << " No sport mapping!";
        }
        if (!bettingsView->mappings->Read(MappingKey{tournamentMapping, plEvent.nTournament}, mapping)) {
            strStream << " No tournament mapping!";
        }
        if (!bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nHomeTeam}, mapping)) {
            strStream << " No home team mapping!";
        }
        if (!bettingsView->mappings->Read(MappingKey{teamMapping, plEvent.nAwayTeam}, mapping)) {
            strStream << " No away team mapping!";
        }

        result.push_back(strStream.str().c_str());
        strStream.clear();
    }

    return result;
}

UniValue listchaingamesevents(const JSONRPCRequest& request)
{
    if (request.fHelp || (request.params.size() > 0))
        throw std::runtime_error(
            "listchaingamesevents\n"
            "\nGet live Wagerr chain game events.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": \"xxx\",         (string) The event ID\n"
            "    \"version\": \"xxx\",    (string) The current version\n"
            "    \"event-id\": \"xxx\",   (string) The ID of the chain games event\n"
            "    \"entry-fee\": n         (numeric) Fee to join game\n"
            "  }\n"
            "]\n"

            "\nExamples:\n" +
            HelpExampleCli("listchaingamesevents", "") + HelpExampleRpc("listchaingamesevents", ""));

    UniValue ret(UniValue::VARR);

    CBlockIndex *BlocksIndex = NULL;

    LOCK(cs_main);

    int height = (Params().NetworkIDString() == CBaseChainParams::MAIN) ? ::ChainActive().Height() - 10500 : ::ChainActive().Height() - 1500;
    BlocksIndex = ::ChainActive()[height];

    while (BlocksIndex) {
        CBlock block;
        ReadBlockFromDisk(block, BlocksIndex, Params().GetConsensus());

        for (CTransactionRef& tx : block.vtx) {

            uint256 txHash = tx->GetHash();

            const CTxIn &txin = tx->vin[0];
            bool validTx = IsValidOracleTxIn(txin, height);

            // Check each TX out for values
            for (unsigned int i = 0; i < tx->vout.size(); i++) {
                const CTxOut &txout = tx->vout[i];

                auto cgBettingTx = ParseBettingTx(txout);

                if (cgBettingTx == nullptr) continue;

                // Find any CChainGameEvents matching the specified id
                if (validTx && cgBettingTx->GetTxType() == cgEventTxType) {
                    CChainGamesEventTx* cgEvent = (CChainGamesEventTx*) cgBettingTx.get();
                    UniValue evt(UniValue::VOBJ);
                    evt.pushKV("tx-id", txHash.ToString().c_str());
                    evt.pushKV("event-id", (uint64_t) cgEvent->nEventId);
                    evt.pushKV("entry-fee", (uint64_t) cgEvent->nEntryFee);
                    ret.push_back(evt);
                }
            }
        }

        BlocksIndex = ::ChainActive().Next(BlocksIndex);
    }

    return ret;
}

UniValue createeventpayload(const JSONRPCRequest& request)
{
    RPCHelpMan{"createeventpayload",
        "\nCreate the oracle tx payload for creating a peerless event\n",
        {
            {"event_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "The event ID"},
            {"start_time", RPCArg::Type::NUM, RPCArg::Optional::NO, "The start time"},
            {"sport_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "The sport ID"},
            {"tournament_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "The tournament ID"},
            {"stage", RPCArg::Type::NUM, RPCArg::Optional::NO, "The stage"},
            {"home_team_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "The home team ID"},
            {"away_team_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "The away team ID"},
            {"version", RPCArg::Type::NUM, /* default*/ "2", "The betting tx format version number; '2' for Wagerr protocol version 5, '1' for earlier version."},
        },
        RPCResult{
            RPCResult::Type::STR_HEX, "payload", "The hex encoded payload."
        },
        RPCExamples{
            HelpExampleCli("sendtoaddress", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\" 0.1")
    + HelpExampleCli("sendtoaddress", "\"XwnLY9Tf7Zsef8gMGL2fhWA9ZmMjt4KPwG\" 0.1 \"donation\" \"seans outpost\"")
        },
    }.Check(request);

    uint8_t nVersion = BetTxVersion5;
    if (request.params.size() >= 8) {
        int nVersionRequested = request.params[7].get_int();
        if (nVersionRequested >=1 && nVersionRequested <= 2) {
            nVersion = nVersionRequested;
        } else {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameters: wrong version number");
        }
    }
    CBettingTxHeader betTxHeader = CBettingTxHeader(nVersion, plEventTxType);

    CPeerlessEventTx eventTx;
    eventTx.nEventId = static_cast<uint32_t>(request.params[0].get_int64());
    eventTx.nStartTime = static_cast<uint32_t>(request.params[1].get_int64());
    eventTx.nSport = static_cast<uint16_t>(request.params[2].get_int64());
    eventTx.nTournament = static_cast<uint16_t>(request.params[3].get_int64());
    eventTx.nStage = static_cast<uint16_t>(request.params[4].get_int64());
    eventTx.nHomeTeam = static_cast<uint32_t>(request.params[5].get_int64());
    eventTx.nAwayTeam = static_cast<uint32_t>(request.params[6].get_int64());

    std::vector<unsigned char> betData;
    EncodeBettingTxPayload(betTxHeader, eventTx, betData);

    return HexStr(betData);
}

static const CRPCCommand commands[] =
{ //  category              name                        actor (function)            argNames
  //  --------------------- --------------------------  --------------------------  ----------
    { "betting",            "getmappingid",             &getmappingid,              {} },
    { "betting",            "getmappingname",           &getmappingname,            {} },
    { "betting",            "getpayoutinfo",            &getpayoutinfo,             {} },
    { "betting",            "getpayoutinfosince",       &getpayoutinfosince,        {} },
    { "betting",            "listevents",               &listevents,                {} },
    { "betting",            "listeventsdebug",          &listeventsdebug,           {} },
    { "betting",            "listchaingamesevents",     &listchaingamesevents,      {} },
    { "betting",            "getchaingamesinfo",        &getchaingamesinfo,         {} },
    { "betting",            "getalleventliabilities",   &getalleventliabilities,    {} },
    { "betting",            "geteventliability",        &geteventliability,         {} },
    { "betting",            "getfieldeventliability",   &getfieldeventliability,    {} },
    { "betting",            "getbetbytxid",             &getbetbytxid,              {} },
    { "betting",            "listbetsdb",               &listbetsdb,                {} },

    { "betting",            "createeventpayload",       &createeventpayload,        {"event_id", "start_time", "sport_id", "tournament_id", "stage", "home_team_id", "away_team_id", "version"} },
};

void RegisterBettingRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
