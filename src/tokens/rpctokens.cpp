// Copyright (c) 2015-2018 The Bitcoin Unlimited developers
// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "core_io.h"
#include "key_io.h"
#include <evo/specialtx.h>
#include <index/txindex.h>
#include "init.h"
#include <node/context.h>
#include "wagerraddrenc.h"
#include "rpc/blockchain.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "script/tokengroup.h"
#include "tokens/tokengroupdocument.h"
#include "tokens/tokengroupmanager.h"
#include "tokens/tokengroupwallet.h"
#include "util/moneystr.h"
#include "util/strencodings.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "wallet/rpcwallet.h"
#include "wallet/wallet.h"

#include <boost/lexical_cast.hpp>

void TokenGroupCreationToJSON(const CTokenGroupID &tgID, const CTokenGroupCreation& tgCreation, UniValue& entry, const bool fShowCreation = false, const bool fShowNFTData = false) {
    CTxOut creationOutput;
    CTxDestination creationDestination;
    GetGroupedCreationOutput(*tgCreation.creationTransaction, creationOutput);
    ExtractDestination(creationOutput.scriptPubKey, creationDestination);
    entry.pushKV("groupID", EncodeTokenGroup(tgID));
    if (tgID.isSubgroup()) {
        CTokenGroupID parentgrp = tgID.parentGroup();
        const std::vector<unsigned char> subgroupData = tgID.GetSubGroupData();
        entry.pushKV("parent_groupID", EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup));
        entry.pushKV("subgroup_data", std::string(subgroupData.begin(), subgroupData.end()));
    }

    std::string flags = tgID.encodeFlags();
    if (flags != "none")
        entry.pushKV("flags", flags);

    UniValue specification = tgDescToJson(*tgCreation.pTokenGroupDescription, fShowNFTData);
    entry.pushKV("specification", specification);

    if (fShowCreation) {
        UniValue extendedEntry(UniValue::VOBJ);
        extendedEntry.pushKV("txid", tgCreation.creationTransaction->GetHash().GetHex());
        extendedEntry.pushKV("blockhash", tgCreation.creationBlockHash.GetHex());
        extendedEntry.pushKV("address", EncodeDestination(creationDestination));
        entry.pushKV("creation", extendedEntry);
    }
}

extern UniValue tokeninfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1)
        throw std::runtime_error(
            "tokeninfo [list, all, stats, groupid, ticker, name] ( \"specifier \" ) ( \"creation_data\" ) ( \"nft_data\" )\n"
            "\nReturns information on all tokens configured on the blockchain.\n"
            "\nArguments:\n"
            "'list'           lists all token groupID's and corresponding token tickers\n"
            "'all'            shows data for all tokens\n"
            "'stats'          shows statistical information on the management tokens in a specific block.\n"
            "                      Args: block hash (optional)\n"
            "'groupid'        shows information on the token configuration with the specified grouID\n"
            "'ticker'         shows information on the token configuration with the specified ticker\n"
            "'name'           shows information on the token configuration with the specified name'\n"
            "\n"
            "'specifier'      (string, optional) parameter to couple with the main action'\n"
            "'creation_data'  (bool, optional) show token creation data'\n"
            "'nft_data'       (bool, optional) show base64 encoded data of NFT tokens'\n"
            "\n" +
            HelpExampleCli("tokeninfo", "ticker \"WAGERR\"") +
            "\n"
        );

    std::string operation;
    std::string p0 = request.params[0].get_str();
    std::transform(p0.begin(), p0.end(), std::back_inserter(operation), ::tolower);

    std::string url;

    UniValue ret(UniValue::VARR);

    unsigned int curparam = 1;
    if (operation == "list") {
        if (request.params.size() > curparam) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Too many parameters");
        }

        UniValue entry(UniValue::VOBJ);
        for (auto tokenGroupMapping : tokenGroupManager.get()->GetMapTokenGroups()) {
            entry.pushKV(tgDescGetName(*tokenGroupMapping.second.pTokenGroupDescription), EncodeTokenGroup(tokenGroupMapping.second.tokenGroupInfo.associatedGroup));
        }
        ret.push_back(entry);
    } else if (operation == "all") {
        if (request.params.size() > 2) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Too many parameters");
        }
        bool fShowCreation = false;
        if (request.params.size() > curparam) {
            std::string strShowCreation;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(strShowCreation), ::tolower);
            fShowCreation = (strShowCreation == "true");
        }

        for (auto tokenGroupMapping : tokenGroupManager.get()->GetMapTokenGroups()) {
            UniValue entry(UniValue::VOBJ);
            TokenGroupCreationToJSON(tokenGroupMapping.first, tokenGroupMapping.second, entry, fShowCreation);
            ret.push_back(entry);
        }
    } else if (operation == "stats") {
        LOCK(cs_main);

        CBlockIndex *pindex = NULL;

        if (request.params.size() > curparam) {
            uint256 blockId;

            blockId.SetHex(request.params[curparam].get_str());
            pindex = LookupBlockIndex(blockId);
            if (!pindex)
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Block not found");
        } else {
            pindex = ::ChainActive()[::ChainActive().Height()];
        }

        uint256 hash = pindex ? pindex->GetBlockHash() : uint256();
        uint64_t nHeight = pindex ? pindex->nHeight : -1;

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("height", nHeight);
        entry.pushKV("blockhash", hash.GetHex());

        ret.push_back(entry);

    } else if (operation == "groupid") {
        if (request.params.size() > 4) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Too many parameters");
        }

        CTokenGroupID grpID;
        // Get the group id from the command line
        grpID = GetTokenGroup(request.params[curparam].get_str());
        if (!grpID.isUserGroup()) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: No group specified");
        }
        curparam++;
        bool fShowCreation = false;
        if (request.params.size() > curparam) {
            std::string strShowCreation;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(strShowCreation), ::tolower);
            fShowCreation = (strShowCreation == "true");
        }
        curparam++;
        bool fShowNFTData = false;
        if (request.params.size() > curparam) {
            std::string strShowNFTData;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(strShowNFTData), ::tolower);
            fShowNFTData = (strShowNFTData == "true");
        }
        UniValue entry(UniValue::VOBJ);
        CTokenGroupCreation tgCreation;
        tokenGroupManager.get()->GetTokenGroupCreation(grpID, tgCreation);
        TokenGroupCreationToJSON(grpID, tgCreation, entry, fShowCreation, fShowNFTData);
        ret.push_back(entry);
    } else if (operation == "ticker") {
        if (request.params.size() > 4) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Too many parameters");
        }

        std::string ticker;
        CTokenGroupID grpID;
        tokenGroupManager.get()->GetTokenGroupIdByTicker(request.params[curparam].get_str(), grpID);
        if (!grpID.isUserGroup())
        {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: could not find token group");
        }
        curparam++;
        bool fShowCreation = false;
        if (request.params.size() > curparam) {
            std::string strShowCreation;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(strShowCreation), ::tolower);
            fShowCreation = (strShowCreation == "true");
        }
        curparam++;
        bool fShowNFTData = false;
        if (request.params.size() > curparam) {
            std::string strShowNFTData;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(strShowNFTData), ::tolower);
            fShowNFTData = (strShowNFTData == "true");
        }

        CTokenGroupCreation tgCreation;
        tokenGroupManager.get()->GetTokenGroupCreation(grpID, tgCreation);

        LogPrint(BCLog::TOKEN, "%s - tokenGroupCreation has [%s] [%s]\n", __func__, tgDescGetTicker(*tgCreation.pTokenGroupDescription), EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup));
        UniValue entry(UniValue::VOBJ);
        TokenGroupCreationToJSON(grpID, tgCreation, entry, fShowCreation, fShowNFTData);
        ret.push_back(entry);
    } else if (operation == "name") {
        if (request.params.size() > 4) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Too many parameters");
        }

        std::string name;
        CTokenGroupID grpID;
        tokenGroupManager.get()->GetTokenGroupIdByName(request.params[curparam].get_str(), grpID);
        if (!grpID.isUserGroup())
        {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: Could not find token group");
        }
        curparam++;
        bool fShowCreation = false;
        if (request.params.size() > curparam) {
            std::string strShowCreation;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(strShowCreation), ::tolower);
            fShowCreation = (strShowCreation == "true");
        }
        curparam++;
        bool fShowNFTData = false;
        if (request.params.size() > curparam) {
            std::string strShowNFTData;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(strShowNFTData), ::tolower);
            fShowNFTData = (strShowNFTData == "true");
        }

        CTokenGroupCreation tgCreation;
        tokenGroupManager.get()->GetTokenGroupCreation(grpID, tgCreation);

        LogPrint(BCLog::TOKEN, "%s - tokenGroupCreation has [%s] [%s]\n", __func__, tgDescGetTicker(*tgCreation.pTokenGroupDescription), EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup));
        UniValue entry(UniValue::VOBJ);
        TokenGroupCreationToJSON(grpID, tgCreation, entry, fShowCreation, fShowNFTData);
        ret.push_back(entry);
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: unknown operation");
    }
    return ret;
}

void RpcTokenTxnoutToUniv(const CTxOut& txout,
    UniValue& out)
{
    CTokenGroupInfo tokenGroupInfo(txout.scriptPubKey);

    if (!tokenGroupInfo.invalid && tokenGroupInfo.associatedGroup != NoGroup) {
        CTokenGroupCreation tgCreation;
        std::string tgTicker;
        if (tokenGroupInfo.associatedGroup.isSubgroup()) {
            CTokenGroupID parentgrp = tokenGroupInfo.associatedGroup.parentGroup();
            std::vector<unsigned char> subgroupData = tokenGroupInfo.associatedGroup.GetSubGroupData();
            tgTicker = tokenGroupManager.get()->GetTokenGroupTickerByID(parentgrp);
            out.pushKV("parent_groupID", EncodeTokenGroup(parentgrp));
            out.pushKV("subgroup_data", std::string(subgroupData.begin(), subgroupData.end()));
        } else {
            tgTicker = tokenGroupManager.get()->GetTokenGroupTickerByID(tokenGroupInfo.associatedGroup);
        }
        out.pushKV("groupID", EncodeTokenGroup(tokenGroupInfo.associatedGroup));
        if (tokenGroupInfo.isAuthority()){
            out.pushKV("type", "authority");
            out.pushKV("ticker", tgTicker);
            out.pushKV("authorities", EncodeGroupAuthority(tokenGroupInfo.controllingGroupFlags()));
        } else {
            out.pushKV("type", "amount");
            out.pushKV("ticker", tgTicker);
            out.pushKV("value", tokenGroupManager.get()->TokenValueFromAmount(tokenGroupInfo.getAmount(), tokenGroupInfo.associatedGroup));
            out.pushKV("valueSat", tokenGroupInfo.getAmount());
        }
    }
}

void TokenTxToUniv(const CTransactionRef& tx, const uint256& hashBlock, UniValue& entry)
{
    entry.pushKV("txid", tx->GetHash().GetHex());
    entry.pushKV("version", tx->nVersion);
    entry.pushKV("size", (int)::GetSerializeSize(*tx, PROTOCOL_VERSION));
    entry.pushKV("locktime", (int64_t)tx->nLockTime);

    UniValue vin(UniValue::VARR);
    for (const CTxIn& txin : tx->vin) {
        UniValue in(UniValue::VOBJ);
        if (tx->IsCoinBase())
            in.pushKV("coinbase", HexStr(txin.scriptSig));
        else {
            in.pushKV("txid", txin.prevout.hash.GetHex());
            in.pushKV("vout", (int64_t)txin.prevout.n);
            UniValue o(UniValue::VOBJ);
            o.pushKV("asm", ScriptToAsmStr(txin.scriptSig));
            o.pushKV("hex", HexStr(txin.scriptSig));
            in.pushKV("scriptSig", o);
        }
        in.pushKV("sequence", (int64_t)txin.nSequence);
        vin.push_back(in);
    }
    entry.pushKV("vin", vin);

    UniValue vout(UniValue::VARR);
    for (unsigned int i = 0; i < tx->vout.size(); i++) {
        const CTxOut& txout = tx->vout[i];

        UniValue out(UniValue::VOBJ);

        UniValue outValue(UniValue::VNUM, FormatMoney(txout.nValue));
        out.pushKV("value", outValue);
        out.pushKV("n", (int64_t)i);

        UniValue o(UniValue::VOBJ);
        ScriptPubKeyToUniv(txout.scriptPubKey, o, true);
        out.pushKV("scriptPubKey", o);

        UniValue t(UniValue::VOBJ);
        RpcTokenTxnoutToUniv(txout, t);
        if (t.size() > 0)
            out.pushKV("token", t);

        vout.push_back(out);
    }
    entry.pushKV("vout", vout);

    if (tx->nType == TRANSACTION_GROUP_CREATION_REGULAR) {
        CTokenGroupDescriptionRegular tgDesc;
        if (GetTxPayload(*tx, tgDesc)) {
            UniValue creation(UniValue::VOBJ);
            tgDesc.ToJson(creation);
            entry.pushKV("token_creation", creation);
        }
    } else if (tx->nType == TRANSACTION_GROUP_CREATION_MGT) {
        CTokenGroupDescriptionMGT tgDesc;
        if (GetTxPayload(*tx, tgDesc)) {
            UniValue creation(UniValue::VOBJ);
            tgDesc.ToJson(creation);
            entry.pushKV("token_creation", creation);
        }
    } else if (tx->nType == TRANSACTION_GROUP_CREATION_NFT) {
        CTokenGroupDescriptionNFT tgDesc;
        if (GetTxPayload(*tx, tgDesc)) {
            UniValue creation(UniValue::VOBJ);
            tgDesc.ToJson(creation);
            entry.pushKV("token_creation", creation);
        }
    }

    if (hashBlock != uint256())
        entry.pushKV("blockhash", hashBlock.GetHex());
}

void TokenTxToJSON(const CTransactionRef& tx, const uint256 hashBlock, UniValue& entry)
{
    TokenTxToUniv(tx, uint256(), entry);

    if (!hashBlock.IsNull()) {
        LOCK(cs_main);

        entry.pushKV("blockhash", hashBlock.GetHex());
        CBlockIndex* pindex = LookupBlockIndex(hashBlock);
        if (pindex) {
            if (::ChainActive().Contains(pindex)) {
                entry.pushKV("height", pindex->nHeight);
                entry.pushKV("confirmations", 1 + ::ChainActive().Height() - pindex->nHeight);
                entry.pushKV("time", pindex->GetBlockTime());
                entry.pushKV("blocktime", pindex->GetBlockTime());
            }
            else
                entry.pushKV("height", -1);
                entry.pushKV("confirmations", 0);
        }
    }
}

extern UniValue gettokentransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "gettokentransaction \"txid\" ( \"blockhash\" )\n"

            "\nReturn the token transaction data.\n"

            "\nArguments:\n"
            "1. \"txid\"      (string, required) The transaction id\n"
            "2. \"blockhash\" (string, optional) The block in which to look for the transaction\n"

            "\nExamples:\n"
            + HelpExampleCli("gettokentransaction", "\"mytxid\"")
            + HelpExampleCli("gettokentransaction", "\"mytxid\" true")
            + HelpExampleRpc("gettokentransaction", "\"mytxid\", true")
            + HelpExampleCli("gettokentransaction", "\"mytxid\" false \"myblockhash\"")
            + HelpExampleCli("gettokentransaction", "\"mytxid\" true \"myblockhash\"")
        );

    const NodeContext& node = EnsureNodeContext(request.context);

    bool in_active_chain = true;
    uint256 hash = ParseHashV(request.params[0], "parameter 1");
    CBlockIndex* blockindex = nullptr;

    if (!request.params[1].isNull()) {
        LOCK(cs_main);

        uint256 blockhash = ParseHashV(request.params[1], "parameter 2");
        blockindex = LookupBlockIndex(blockhash);
        if (!blockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block hash not found");
        }
        in_active_chain = ::ChainActive().Contains(blockindex);
    }

    bool f_txindex_ready = false;
    if (g_txindex && !blockindex) {
        f_txindex_ready = g_txindex->BlockUntilSyncedToCurrentChain();
    }

    uint256 hash_block;
    CTransactionRef tx = GetTransaction(blockindex, node.mempool, hash, Params().GetConsensus(), hash_block);
    if (!tx) {
        std::string errmsg;
        if (blockindex) {
            if (!(blockindex->nStatus & BLOCK_HAVE_DATA)) {
                throw JSONRPCError(RPC_MISC_ERROR, "Block not available");
            }
            errmsg = "No such transaction found in the provided block";
        } else if (!g_txindex) {
            errmsg = "No such mempool transaction. Use -txindex or provide a block hash to enable blockchain transaction queries";
        } else if (!f_txindex_ready) {
            errmsg = "No such mempool transaction. Blockchain transactions are still in the process of being indexed";
        } else {
            errmsg = "No such mempool or blockchain transaction";
        }
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, errmsg + ". Use gettransaction for wallet transactions.");
    }

    UniValue result(UniValue::VOBJ);
    if (blockindex) result.pushKV("in_active_chain", in_active_chain);
    TokenTxToJSON(tx, hash_block, result);
    return result;
}

extern UniValue getsubgroupid(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1)
        throw std::runtime_error(
            "getsubgroupid \"groupid\" \"data\" \n"
            "\nTranslates a group and additional data into a subgroup identifier.\n"
            "\n"
            "\nArguments:\n"
            "1. \"groupID\"     (string, required) the group identifier\n"
            "2. \"data\"        (string, required) data that specifies the subgroup\n"
            "\n"
        );

    unsigned int curparam = 0;
    if (curparam >= request.params.size())
    {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Missing parameters");
    }
    CTokenGroupID grpID;
    std::vector<unsigned char> postfix;
    // Get the group id from the command line
    grpID = GetTokenGroup(request.params[curparam].get_str());
    if (!grpID.isUserGroup())
    {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: No group specified");
    }
    curparam++;

    int64_t postfixNum = 0;
    bool isNum = false;
    if (request.params[curparam].isNum())
    {
        postfixNum = request.params[curparam].get_int64();
        isNum = true;
    }
    else // assume string
    {
        std::string postfixStr = request.params[curparam].get_str();
        if ((postfixStr[0] == '0') && (postfixStr[0] == 'x'))
        {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: Hex not implemented yet");
        }
        try
        {
            postfixNum = boost::lexical_cast<int64_t>(postfixStr);
            isNum = true;
        }
        catch (const boost::bad_lexical_cast &)
        {
            for (unsigned int i = 0; i < postfixStr.size(); i++)
                postfix.push_back(postfixStr[i]);
        }
    }

    if (isNum)
    {
        CDataStream ss(0, 0);
        ser_writedata64(ss, postfixNum);
        for (auto c : ss)
            postfix.push_back(c);
    }

    if (postfix.size() == 0)
    {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: no subgroup postfix provided");
    }
    std::vector<unsigned char> subgroupbytes(grpID.bytes().size() + postfix.size());
    unsigned int i;
    for (i = 0; i < grpID.bytes().size(); i++)
    {
        subgroupbytes[i] = grpID.bytes()[i];
    }
    for (unsigned int j = 0; j < postfix.size(); j++, i++)
    {
        subgroupbytes[i] = postfix[j];
    }
    CTokenGroupID subgrpID(subgroupbytes);
    return EncodeTokenGroup(subgrpID);
};

UniValue createrawtokentransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4)
        throw std::runtime_error(
            "createrawtokentransaction [{\"txid\":\"id\",\"vout\":n},...] {\"address\":amount,\"data\":\"hex\",...} ( locktime )\n"
            "\nCreate a transaction spending the given inputs and creating new outputs.\n"
            "Outputs can be addresses or data.\n"
            "Returns hex-encoded raw transaction.\n"
            "Note that the transaction's inputs are not signed, and\n"
            "it is not stored in the wallet or transmitted to the network.\n"

            "\nArguments:\n"
            "1. \"inputs\"                (array, required) A json array of json objects\n"
            "     [\n"
            "       {\n"
            "         \"txid\":\"id\",    (string, required) The transaction id\n"
            "         \"vout\":n,         (numeric, required) The output number\n"
            "         \"sequence\":n      (numeric, optional) The sequence number\n"
            "       } \n"
            "       ,...\n"
            "     ]\n"
            "2. \"outputs\"               (object, required) a json object with outputs\n"
            "    {\n"
            "      \"address\": x.xxx,    (numeric or string, required) The key is the address, the numeric value (can be string) is the " + CURRENCY_UNIT + " amount\n"
            "      \"data\": \"hex\"      (string, required) The key is \"data\", the value is hex encoded data\n"
            "      ,...\n"
            "    }\n"
            "3. \"token_outputs\"         (string, required) a json object with addresses as keys and a json objects with the WAGERR and tokens to send\n"
            "    {\n"
            "      \"address\":           (numeric, required) The key is the Wagerr address, the value is a json object with an WAGERR amount, tokengroup ID and token value as values\n"
            "      {\n"
            "        \"amount\":\"x.xxx\"       (numeric, required) The WAGERR amount\n"
            "        \"group_id\":\"hex\"       (string, required) The tokengroup ID\n"
            "        \"token_amount\":\"x.xxx\" (numeric, required) The token amount\n"
            "      },...\n"
            "    }\n"
            "4. locktime                  (numeric, optional, default=0) Raw locktime. Non-0 value also locktime-activates inputs\n"
            "\nResult:\n"
            "\"transaction\"              (string) hex string of the transaction\n"

            "\nExamples:\n"
            + HelpExampleCli("createrawtokentransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01}\" \"{\\\"address\\\": {\\\"amount\\\":0.00000001, \\\"group_id\\\":\\\"asdfasdf\\\", \\\"token_amount\\\":0.1}}\"")
        );

    RPCTypeCheck(request.params, {UniValue::VARR, UniValue::VOBJ, UniValue::VOBJ, UniValue::VNUM}, true);
    if (request.params[0].isNull() || request.params[1].isNull() || request.params[2].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, arguments 1, 2 and 3 must be non-null");

    UniValue inputs = request.params[0].get_array();
    UniValue sendTo = request.params[1].get_obj();
    UniValue sendTokensTo = request.params[2].get_obj();

    CMutableTransaction rawTx;

    if (request.params.size() > 3 && !request.params[3].isNull()) {
        int64_t nLockTime = request.params[3].get_int64();
        if (nLockTime < 0 || nLockTime > std::numeric_limits<uint32_t>::max())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, locktime out of range");
        rawTx.nLockTime = nLockTime;
    }

    for (unsigned int idx = 0; idx < inputs.size(); idx++) {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        uint256 txid = ParseHashO(o, "txid");

        const UniValue& vout_v = find_value(o, "vout");
        if (!vout_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        uint32_t nSequence = (rawTx.nLockTime ? std::numeric_limits<uint32_t>::max() - 1 : std::numeric_limits<uint32_t>::max());

        // set the sequence number if passed in the parameters object
        const UniValue& sequenceObj = find_value(o, "sequence");
        if (sequenceObj.isNum()) {
            int64_t seqNr64 = sequenceObj.get_int64();
            if (seqNr64 < 0 || seqNr64 > std::numeric_limits<uint32_t>::max())
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, sequence number is out of range");
            else
                nSequence = (uint32_t)seqNr64;
        }

        CTxIn in(COutPoint(txid, nOutput), CScript(), nSequence);

        rawTx.vin.push_back(in);
    }

    std::set<CTxDestination> setAddress;
    std::vector<std::string> addrList = sendTo.getKeys();
    for (const std::string& name_ : addrList) {

        if (name_ == "data") {
            std::vector<unsigned char> data = ParseHexV(sendTo[name_].getValStr(),"Data");

            CTxOut out(0, CScript() << OP_RETURN << data);
            rawTx.vout.push_back(out);
        } else {
            CTxDestination address = DecodeDestination(name_);
            if (!IsValidDestination(address)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid address: ")+name_);
            }

            if (setAddress.count(address))
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ")+name_);
            setAddress.insert(address);

            CScript scriptPubKey = GetScriptForDestination(address);
            CAmount nAmount = AmountFromValue(sendTo[name_]);

            CTxOut out(nAmount, scriptPubKey);
            rawTx.vout.push_back(out);
        }
    }

    std::set<CTxDestination> setDestinations;
    std::vector<std::string> tokenAddrList = sendTokensTo.getKeys();
    for (const std::string& name_ : tokenAddrList) {
        UniValue recipientObj = sendTokensTo[name_];

        CTxDestination dst = DecodeDestination(name_, Params());
        if (dst == CTxDestination(CNoDestination())) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid parameter: destination address");
        }
        if (setDestinations.count(dst))
            throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ")+name_);
        setDestinations.insert(dst);

        std::string sTokenGroupID = recipientObj["group_id"].get_str();
        CTokenGroupID tgID = GetTokenGroup(sTokenGroupID);
        if (!tgID.isUserGroup()) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: No group specified");
        }
        CTokenGroupCreation tgCreation;
        if (!tokenGroupManager.get()->GetTokenGroupCreation(tgID, tgCreation)) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: Token group configuration transaction not found. Has it confirmed?");
        }

        CAmount nAmount = AmountFromValue(recipientObj["amount"]);

        CAmount nTokenAmount = tokenGroupManager.get()->AmountFromTokenValue(recipientObj["token_amount"], tgID);
        if (nTokenAmount <= 0)
            throw JSONRPCError(RPC_TYPE_ERROR, "Invalid parameter: token_amount");
        CScript script;

        script = GetScriptForDestination(dst, tgID, nTokenAmount);
        CTxOut txout(nAmount, script);

        rawTx.vout.push_back(txout);
    }

    return EncodeHexTx(CTransaction(rawTx));
}

UniValue encodetokenmetadata(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "encodetokenmetadata {\"ticker\":\"ticker\",\"name\":\"token name\",...} \n"
            "\nCreate the hash and hexadecimal representation of a token metadata document.\n"
            "\nThe document's hash is included when configuring a new token.\n"
            "\nThe document's hexadecimal representation is used to sign the metadata document.\n"
            "\nNote that the attribute format is a suggestion, not a requirement.\n"

            "\nArguments:\n"
            "1. \"specification\":                 (json, required) The document specification.\n"
            "     {\n"
            "       \"atp\": {                     (object, required) ATP specification data\n"
            "         \"version\": \"1\",            (string, required) ATP version number. Current version: 1\n"
            "         \"type\": \"type\"             (string, required) 'regular|mgt|nft' for either regular\n"
            "                                            tokens, management tokens or non-fungible tokens\n"
            "       }\n"
            "       \"ticker\":\"ticker\",           (string) The ticker. Required for regular tokens and management tokens\n"
            "       \"name\":\"name\",               (string, required) The token name\n"
            "       \"chain\":\"chain\",             (string, required) Chain identifier, e.g. \"WAGERR\" (for mainnet) or \"WAGERR.testnet\" or \"WAGERR.regtest\"\n"
            "       \"summary\":\"summary\",         (string, optional) Short introduction to the token.\n"
            "       \"description\":\"description\", (string, optional) Description of the token\n"
            "       \"creator\":\"creator\",         (string, optional) Token creator\n"
            "       \"attributes\": [              (array, required for NFT) URL that points to a json file that holds an array with attributes\n"
            "         {                           (object, optional) object with attribute data\n"
            "           \"trait_type\": \"type\",     (string, required) Trait type; e.g., 'Seat number', 'Shape', 'Power'\n"
            "           \"display_type\": \"type\",   (string, optional) Display type; e.g., 'Number', 'Percentage', 'Currency'\n"
            "           \"value\": \"value\"          (any, required) Attribute value\n"
            "         }\n"
            "       ]\n"
            "       \"attributes_url\": \"id\",      (string, required for NFT) URL that points to a dynamic json file that holds an array with attributes\n"
            "       }\n"
            "     }\n"

            "\nResult:\n"
            "\"hash\" : \"value\",           (string) The hash of the token metadata document\n"
            "\"hex_data\" : \"value\",       (string) The hex-encoded token metadata document\n"

            "\nExamples:\n"
            "\nCreate the MGT testnet document\n"
            + HelpExampleCli("encodetokenmetadata",
                "\"{\\\"atp\\\":{\\\"version\\\":1,\\\"type\\\":\\\"nft\\\"},\\\"name\\\":\\\"John Doe concert tickets - Garden of Eden"
                " tour\\\",\\\"chain\\\":\\\"WAGERR.testnet\\\",\\\"creator\\\":\\\"DoeTours Ltd.\\\",\\\"description\\\":\\\"From April "
                "1st through April 9th, John Doe will visit Eden, NC. This booking grants you access.\\\",\\\"external_url\\\":\\\"http"
                "s://yourtickettomusic.com/nft/{id}/\\\",\\\"image\\\":\\\"https://www.stockvault.net/data/2018/10/09/255077/preview16."
                "jpg\\\",\\\"attributes\\\":[{\\\"trait_type\\\":\\\"Ticket class\\\",\\\"value\\\":\\\"Gold\\\"},{\\\"display_type\\\""
                ":\\\"currency_dollar\\\",\\\"trait_type\\\":\\\"Base price\\\",\\\"value\\\":\\\"50.0\\\"},{\\\"trait_type\\\":\\\"All"
                "ow resale\\\",\\\"value\\\":\\\"Yes\\\"}],\\\"attributes_url\\\":\\\"https://yourtickettomusic.com/nft/{id}_attributes"
                ".json\\\"}\"")
        );
    }

    RPCTypeCheck(request.params, {UniValue::VOBJ, UniValue::VBOOL});

    UniValue data = request.params[0].get_obj();

    CTokenGroupDocument tgDocument = CTokenGroupDocument(data);

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("hash", tgDocument.GetSignatureHash().GetHex());
    ret.pushKV("hex_data", tgDocument.GetDataAsHexString());
    return ret;
}

UniValue decodetokenmetadata(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
                "decodetokenmetadata \"data\"\n"

                "\nDecode a hex-encoded token document to json\n"

                "\nArguments:\n"
                "1. \"hex_data\"           (hex, required) The hex-encoded token metadata document\n"

            "\nResult:\n"
            "     {\n"
            "       \"atp\": {                     (object, required) ATP specification data\n"
            "         \"version\": \"1\",            (string, required) ATP version number. Current version: 1\n"
            "         \"type\": \"type\"             (string, required) 'regular|mgt|nft' for either regular\n"
            "                                            tokens, management tokens or non-fungible tokens\n"
            "       }\n"
            "       \"ticker\":\"ticker\",           (string) The ticker. Required for regular tokens and management tokens\n"
            "       \"name\":\"name\",               (string, required) The token name\n"
            "       \"chain\":\"chain\",             (string, required) Chain identifier, e.g. \"WAGERR\" (for mainnet) or \"WAGERR.testnet\" or \"WAGERR.regtest\"\n"
            "       \"summary\":\"summary\",         (string, optional) Short introduction to the token.\n"
            "       \"description\":\"description\", (string, optional) Description of the token\n"
            "       \"creator\":\"creator\",         (string, optional) Token creator\n"
            "       \"attributes\": [              (array, required for NFT) URL that points to a json file that holds an array with attributes\n"
            "         {                           (object, optional) object with attribute data\n"
            "           \"trait_type\": \"type\",     (string, required) Trait type; e.g., 'Seat number', 'Shape', 'Power'\n"
            "           \"display_type\": \"type\",   (string, optional) Display type; e.g., 'Number', 'Percentage', 'Currency'\n"
            "           \"value\": \"value\"          (any, required) Attribute value\n"
            "         }\n"
            "       ]\n"
            "       \"attributes_url\": \"id\",      (string, required for NFT) URL that points to a dynamic json file that holds an array with attributes\n"
            "       }\n"
            "     }\n"

            "\nExamples:\n"
            "\nDecode the hex-encoded MGT testnet document\n"
            + HelpExampleCli("decodetokenmetadata",
                "7b0a202022617470223a207b0a2020202276657273696f6e223a20312c0a2020202274797065223a20226d616e6167656d656e74220a20207d2c0a2020227469636b6572223"
                "a20224d4754222c0a2020226e616d65223a20224d616e6167656d656e7420546f6b656e222c0a202022636861696e223a20224259545a2e746573746e6574222c0a20202263"
                "726561746f72223a2022546865204279747a20436f726520646576656c6f70657273222c0a20202273756d6d617279223a2022546865204d475420746f6b656e20697320612"
                "0746f6b656e697a6564206d616e6167656d656e74206b6579206f6e20746865204259545a20626c6f636b636861696e2077697468207370656369616c20617574686f726974"
                "696573206e656365737361727920666f723a202831292074686520636f6e737472756374696f6e206f66206120746f6b656e2073797374656d207769746820636f686572656"
                "e742065636f6e6f6d696320696e63656e74697665733b202832292074686520696e63657074696f6e206f66204e75636c65757320546f6b656e7320287370656369616c2074"
                "6f6b656e732074686174206861766520696e74657272656c61746564206d6f6e657461727920706f6c6963696573293b20616e6420283329207468652064697374726962757"
                "4696f6e206f6620726577617264732074686174207375737461696e20746869732073797374656d206f662063727970746f6772617068696320746f6b656e73206f6e207468"
                "6520626c6f636b636861696e2e222c0a2020226465736372697074696f6e223a20225468652041746f6d696320546f6b656e2050726f746f636f6c20284154502920696e747"
                "26f64756365732063726f73732d636f696e20616e642063726f73732d746f6b656e20706f6c6963792e204259545a207574696c697a65732041545020666f72206974732072"
                "65776172642073797374656d20616e6420726967687473207374727563747572652e204d616e6167656d656e7420546f6b656e20284d4754292c20477561726469616e20566"
                "16c696461746f7220546f6b656e2028475654292c20616e6420477561726469616e2056616c696461746f727320616c6c20706172746963697061746520696e20616e20696e"
                "746572636f6e6e6563746564206d616e6167656e742073797374656d2c20616e642061726520636f6e7369646572656420746865204e75636c65757320546f6b656e732e205"
                "46865204d475420746f6b656e20697473656c66206973206120746f6b656e697a6564206d616e6167656d656e74206b65792077697468207370656369616c20617574686f72"
                "6974696573206e656564656420666f7220746f6b656e20696e63657074696f6e206f6e2074686520626c6f636b636861696e2e20546865204d475420746f6b656e20636f6e7"
                "4696e75657320746f20706c6179206120726f6c6520696e20746865206d616e6167656d656e74206f6620616e642061636365737320746f207370656369616c206665617475"
                "7265732e222c0a20202265787465726e616c5f75726c223a202268747470733a2f2f6769746875622e636f6d2f6279747a63757272656e63792f6279747a222c0a202022696"
                "d616765223a202268747470733a2f2f6279747a2e67672f696d616765732f6272616e64696e672f6279747a2d686f72697a6f6e74616c2d6c6f676f2e737667222c0a202022"
                "617474726962757465735f75726c223a202268747470733a2f2f6769746875622e636f6d2f6279747a63757272656e63792f4154502d6465736372697074696f6e732f74657"
                "3746e65742f7b69647d5f617474726962757465732e6a736f6e220a207d")
        );
    }
    RPCTypeCheck(request.params, {UniValue::VSTR});

    UniValue ret(UniValue::VOBJ);
    CTokenGroupDocument tgDocument(ParseHexV(request.params[0], "data"));
    tgDocument.ToJson(ret);

    return ret;
}

UniValue verifytokenmetadata(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3) {
        throw std::runtime_error(
                "verifytokenmetadata \"hex_data\" \"creation_address\" \"signature\"\n"
                "\nVerifies that the given address is used to sign the token metadata document.\n"
                "\nArguments:\n"
                "1. \"hex_data\"           (hex, required) The hex-encoded token metadata document as returned by encodetokenmetadata\n"
                "2. \"creation_address\"   (string, required) The token creation address, which will be used to sign the token document\n"
                "3. \"signature\"          (string, optional) The token metadata document signature.\n"

            "\nResult:\n"
            "true|false    (boolean) If the signature is verified or not\n"

            "\nExamples:\n"
            "\nVerify the hex-encoded MGT testnet document\n"
            + HelpExampleCli("verifytokenmetadata",
                "7b0a202022617470223a207b0a2020202276657273696f6e223a20312c0a2020202274797065223a20226d616e6167656d656e74220a20207d2c0a2020227469636b6572223"
                "a20224d4754222c0a2020226e616d65223a20224d616e6167656d656e7420546f6b656e222c0a202022636861696e223a20224259545a2e746573746e6574222c0a20202263"
                "726561746f72223a2022546865204279747a20436f726520646576656c6f70657273222c0a20202273756d6d617279223a2022546865204d475420746f6b656e20697320612"
                "0746f6b656e697a6564206d616e6167656d656e74206b6579206f6e20746865204259545a20626c6f636b636861696e2077697468207370656369616c20617574686f726974"
                "696573206e656365737361727920666f723a202831292074686520636f6e737472756374696f6e206f66206120746f6b656e2073797374656d207769746820636f686572656"
                "e742065636f6e6f6d696320696e63656e74697665733b202832292074686520696e63657074696f6e206f66204e75636c65757320546f6b656e7320287370656369616c2074"
                "6f6b656e732074686174206861766520696e74657272656c61746564206d6f6e657461727920706f6c6963696573293b20616e6420283329207468652064697374726962757"
                "4696f6e206f6620726577617264732074686174207375737461696e20746869732073797374656d206f662063727970746f6772617068696320746f6b656e73206f6e207468"
                "6520626c6f636b636861696e2e222c0a2020226465736372697074696f6e223a20225468652041746f6d696320546f6b656e2050726f746f636f6c20284154502920696e747"
                "26f64756365732063726f73732d636f696e20616e642063726f73732d746f6b656e20706f6c6963792e204259545a207574696c697a65732041545020666f72206974732072"
                "65776172642073797374656d20616e6420726967687473207374727563747572652e204d616e6167656d656e7420546f6b656e20284d4754292c20477561726469616e20566"
                "16c696461746f7220546f6b656e2028475654292c20616e6420477561726469616e2056616c696461746f727320616c6c20706172746963697061746520696e20616e20696e"
                "746572636f6e6e6563746564206d616e6167656e742073797374656d2c20616e642061726520636f6e7369646572656420746865204e75636c65757320546f6b656e732e205"
                "46865204d475420746f6b656e20697473656c66206973206120746f6b656e697a6564206d616e6167656d656e74206b65792077697468207370656369616c20617574686f72"
                "6974696573206e656564656420666f7220746f6b656e20696e63657074696f6e206f6e2074686520626c6f636b636861696e2e20546865204d475420746f6b656e20636f6e7"
                "4696e75657320746f20706c6179206120726f6c6520696e20746865206d616e6167656d656e74206f6620616e642061636365737320746f207370656369616c206665617475"
                "7265732e222c0a20202265787465726e616c5f75726c223a202268747470733a2f2f6769746875622e636f6d2f6279747a63757272656e63792f6279747a222c0a202022696"
                "d616765223a202268747470733a2f2f6279747a2e67672f696d616765732f6272616e64696e672f6279747a2d686f72697a6f6e74616c2d6c6f676f2e737667222c0a202022"
                "617474726962757465735f75726c223a202268747470733a2f2f6769746875622e636f6d2f6279747a63757272656e63792f4154502d6465736372697074696f6e732f74657"
                "3746e65742f7b69647d5f617474726962757465732e6a736f6e220a207d TwXyY5uJmzU9bMXPDbf5LyqrBczboMdeNL "
                "1f8c4276ade8a8c2f6ba20bfa3e48b6bf520e35a65dbbd070a9583b097d0e78b7d4f68abb1df4393f31dd3e961bc1a49e59dd8378c0ffcb8f2361e87bfdf7535fe")
        );
    }

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR, UniValue::VSTR});

    std::string strHexDescription = request.params[0].get_str();
    std::string strAddress = request.params[1].get_str();
    std::string strSignature = request.params[2].get_str();
    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Wagerr address");
    }
    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    }

    UniValue ret(UniValue::VOBJ);
    CTokenGroupDocument tgDocument(ParseHexV(request.params[0], "data"));
    tgDocument.ToJson(ret);

    tgDocument.SetSignature(strSignature);

    return tgDocument.CheckSignature(*keyID);
}

static const CRPCCommand commands[] =
{ //  category              name                        actor (function)            argNames
  //  --------------------- --------------------------  --------------------------  ----------
    { "tokens",             "tokeninfo",                &tokeninfo,                 {} },
    { "tokens",             "gettokentransaction",      &gettokentransaction,       {} },
    { "tokens",             "getsubgroupid",            &getsubgroupid,             {} },
    { "tokens",             "createrawtokentransaction",&createrawtokentransaction, {} },
    { "tokens",             "encodetokenmetadata",   &encodetokenmetadata,    {"spec"} },
    { "tokens",             "decodetokenmetadata",   &decodetokenmetadata,    {} },
    { "tokens",             "verifytokenmetadata",   &verifytokenmetadata,    {"hex_data","creation_address", "signature"} },
};

void RegisterTokenRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
