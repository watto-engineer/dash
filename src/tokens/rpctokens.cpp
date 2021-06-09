// Copyright (c) 2015-2018 The Bitcoin Unlimited developers
// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "core_io.h"
#include "dstencode.h"
#include <evo/specialtx.h>
#include "init.h"
#include "bytzaddrenc.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "script/tokengroup.h"
#include "tokens/tokengroupdocument.h"
#include "tokens/tokengroupmanager.h"
#include "tokens/tokengroupwallet.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "wallet/rpcwallet.h"
#include "wallet/wallet.h"

#include <boost/lexical_cast.hpp>

void TokenGroupCreationToJSON(const CTokenGroupID &tgID, const CTokenGroupCreation& tgCreation, UniValue& entry, const bool extended = false) {
    CTxOut creationOutput;
    CTxDestination creationDestination;
    GetGroupedCreationOutput(*tgCreation.creationTransaction, creationOutput);
    ExtractDestination(creationOutput.scriptPubKey, creationDestination);
    entry.push_back(Pair("groupID", EncodeTokenGroup(tgID)));
    if (tgID.isSubgroup()) {
        CTokenGroupID parentgrp = tgID.parentGroup();
        const std::vector<unsigned char> subgroupData = tgID.GetSubGroupData();
        entry.push_back(Pair("parentGroupID", EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup)));
        entry.push_back(Pair("subgroupData", std::string(subgroupData.begin(), subgroupData.end())));
    }
    entry.push_back(Pair("ticker", tgDescGetTicker(*tgCreation.pTokenGroupDescription)));
    entry.push_back(Pair("name", tgDescGetName(*tgCreation.pTokenGroupDescription)));
    entry.push_back(Pair("decimalPos", tgDescGetDecimalPos(*tgCreation.pTokenGroupDescription)));
    entry.push_back(Pair("URL", tgDescGetDocumentURL(*tgCreation.pTokenGroupDescription)));
    entry.push_back(Pair("documentHash", tgDescGetDocumentHash(*tgCreation.pTokenGroupDescription).ToString()));
    std::string flags = tgID.encodeFlags();
    if (flags != "none")
        entry.push_back(Pair("flags", flags));
    if (extended) {
        UniValue extendedEntry(UniValue::VOBJ);
        extendedEntry.push_back(Pair("txid", tgCreation.creationTransaction->GetHash().GetHex()));
        extendedEntry.push_back(Pair("blockHash", tgCreation.creationBlockHash.GetHex()));
        extendedEntry.push_back(Pair("address", EncodeDestination(creationDestination)));
        entry.push_back(Pair("creation", extendedEntry));
    }
}

extern UniValue tokeninfo(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1)
        throw std::runtime_error(
            "tokeninfo [list, all, stats, groupid, ticker, name] ( \"specifier \" ) ( \"extended_info\" ) \n"
            "\nReturns information on all tokens configured on the blockchain.\n"
            "\nArguments:\n"
            "'list' lists all token groupID's and corresponding token tickers\n"
            "'all' shows extended information on all tokens\n"
            "'stats' shows statistical information on the management tokens in a specific block. Args: block height (optional)\n"
            "'groupid' shows information on the token configuration with the specified grouID\n"
            "'ticker' shows information on the token configuration with the specified ticker\n"
            "'name' shows information on the token configuration with the specified name'\n"
            "'extended_info' (optional) show extended information'\n"
            "\n" +
            HelpExampleCli("tokeninfo", "ticker \"BYTZ\"") +
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
            entry.push_back(Pair(tgDescGetName(*tokenGroupMapping.second.pTokenGroupDescription), EncodeTokenGroup(tokenGroupMapping.second.tokenGroupInfo.associatedGroup)));
        }
        ret.push_back(entry);
    } else if (operation == "all") {
        if (request.params.size() > 2) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Too many parameters");
        }
        bool extended = false;
        if (request.params.size() > curparam) {
            std::string sExtended;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(sExtended), ::tolower);
            extended = (sExtended == "true");
        }

        for (auto tokenGroupMapping : tokenGroupManager.get()->GetMapTokenGroups()) {
            UniValue entry(UniValue::VOBJ);
            TokenGroupCreationToJSON(tokenGroupMapping.first, tokenGroupMapping.second, entry, extended);
            ret.push_back(entry);
        }
    } else if (operation == "stats") {
        LOCK2(cs_main, pwallet->cs_wallet);

        CBlockIndex *pindex = NULL;

        if (request.params.size() > curparam) {
            uint256 blockId;

            blockId.SetHex(request.params[curparam].get_str());
            BlockMap::iterator it = mapBlockIndex.find(blockId);
            if (it != mapBlockIndex.end()) {
                pindex = it->second;
            } else {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Block not found");
            }
        } else {
            pindex = chainActive[chainActive.Height()];
        }

        uint256 hash = pindex ? pindex->GetBlockHash() : uint256();
        uint64_t nHeight = pindex ? pindex->nHeight : -1;

        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("height", nHeight));
        entry.push_back(Pair("blockhash", hash.GetHex()));

        ret.push_back(entry);

    } else if (operation == "groupid") {
        if (request.params.size() > 3) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Too many parameters");
        }

        CTokenGroupID grpID;
        // Get the group id from the command line
        grpID = GetTokenGroup(request.params[curparam].get_str());
        if (!grpID.isUserGroup()) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: No group specified");
        }
        curparam++;
        bool extended = false;
        if (request.params.size() > curparam) {
            std::string sExtended;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(sExtended), ::tolower);
            extended = (sExtended == "true");
        }
        UniValue entry(UniValue::VOBJ);
        CTokenGroupCreation tgCreation;
        tokenGroupManager.get()->GetTokenGroupCreation(grpID, tgCreation);
        TokenGroupCreationToJSON(grpID, tgCreation, entry, extended);
        ret.push_back(entry);
    } else if (operation == "ticker") {
        if (request.params.size() > 3) {
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
        bool extended = false;
        if (request.params.size() > curparam) {
            std::string sExtended;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(sExtended), ::tolower);
            extended = (sExtended == "true");
        }

        CTokenGroupCreation tgCreation;
        tokenGroupManager.get()->GetTokenGroupCreation(grpID, tgCreation);

        LogPrint(BCLog::TOKEN, "%s - tokenGroupCreation has [%s] [%s]\n", __func__, tgDescGetTicker(*tgCreation.pTokenGroupDescription), EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup));
        UniValue entry(UniValue::VOBJ);
        TokenGroupCreationToJSON(grpID, tgCreation, entry, extended);
        ret.push_back(entry);
    } else if (operation == "name") {
        if (request.params.size() > 3) {
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
        bool extended = false;
        if (request.params.size() > curparam) {
            std::string sExtended;
            std::string p = request.params[curparam].get_str();
            std::transform(p.begin(), p.end(), std::back_inserter(sExtended), ::tolower);
            extended = (sExtended == "true");
        }

        CTokenGroupCreation tgCreation;
        tokenGroupManager.get()->GetTokenGroupCreation(grpID, tgCreation);

        LogPrint(BCLog::TOKEN, "%s - tokenGroupCreation has [%s] [%s]\n", __func__, tgDescGetTicker(*tgCreation.pTokenGroupDescription), EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup));
        UniValue entry(UniValue::VOBJ);
        TokenGroupCreationToJSON(grpID, tgCreation, entry, extended);
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
            out.pushKV("parentGroupID", EncodeTokenGroup(parentgrp));
            out.pushKV("subgroupData", std::string(subgroupData.begin(), subgroupData.end()));
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
    entry.pushKV("size", (int)::GetSerializeSize(*tx, SER_NETWORK, PROTOCOL_VERSION));
    entry.pushKV("locktime", (int64_t)tx->nLockTime);

    UniValue vin(UniValue::VARR);
    for (const CTxIn& txin : tx->vin) {
        UniValue in(UniValue::VOBJ);
        if (tx->IsCoinBase())
            in.pushKV("coinbase", HexStr(txin.scriptSig.begin(), txin.scriptSig.end()));
        else {
            in.pushKV("txid", txin.prevout.hash.GetHex());
            in.pushKV("vout", (int64_t)txin.prevout.n);
            UniValue o(UniValue::VOBJ);
            o.pushKV("asm", ScriptToAsmStr(txin.scriptSig));
            o.pushKV("hex", HexStr(txin.scriptSig.begin(), txin.scriptSig.end()));
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
        entry.push_back(Pair("blockhash", hashBlock.GetHex()));
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                entry.push_back(Pair("confirmations", 1 + chainActive.Height() - pindex->nHeight));
                entry.push_back(Pair("time", pindex->GetBlockTime()));
                entry.push_back(Pair("blocktime", pindex->GetBlockTime()));
            }
            else
                entry.push_back(Pair("confirmations", 0));
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

    LOCK(cs_main);

    bool in_active_chain = true;
    uint256 hash = ParseHashV(request.params[0], "parameter 1");
    CBlockIndex* blockindex = nullptr;

    if (!request.params[1].isNull()) {
        uint256 blockhash = ParseHashV(request.params[1], "parameter 2");
        BlockMap::iterator it = mapBlockIndex.find(blockhash);
        if (it == mapBlockIndex.end()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block hash not found");
        }
        blockindex = it->second;
        in_active_chain = chainActive.Contains(blockindex);
    }

    CTransactionRef tx;
    uint256 hash_block;
    if (!GetTransaction(hash, tx, Params().GetConsensus(), hash_block, true)) {
        std::string errmsg;
        if (blockindex) {
            if (!(blockindex->nStatus & BLOCK_HAVE_DATA)) {
                throw JSONRPCError(RPC_MISC_ERROR, "Block not available");
            }
            errmsg = "No such transaction found in the provided block";
        } else {
            errmsg = fTxIndex
              ? "No such mempool or blockchain transaction"
              : "No such mempool transaction. Use -txindex to enable blockchain transaction queries";
        }
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, errmsg + ". Use gettransaction for wallet transactions.");
    }

    UniValue result(UniValue::VOBJ);
    if (blockindex) result.push_back(Pair("in_active_chain", in_active_chain));
    TokenTxToJSON(tx, hash_block, result);
    return result;
}

extern UniValue getsubgroupid(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

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
            "3. \"token_outputs\"         (string, required) a json object with addresses as keys and a json objects with the BYTZ and tokens to send\n"
            "    {\n"
            "      \"address\":           (numeric, required) The key is the Bytz address, the value is a json object with an BYTZ amount, tokengroup ID and token value as values\n"
            "      {\n"
            "        \"amount\":\"x.xxx\"       (numeric, required) The BYTZ amount\n"
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

    return EncodeHexTx(rawTx);
}

UniValue createrawtokendocument(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3) {
        throw std::runtime_error(
            "createrawtokendocument {\"ticker\":\"ticker\",\"name\":\"token name\",...} ( verbose )\n"
            "\nCreate a token document that is to be signed and published online.\n"
            "\nThe document's hash is included when configuring a new token.\n"
            "Returns either a hex-encoded representation or a json representation.\n"
            "\n"
            "Note that the created document is not signed, and that it is not stored in the wallet or transmitted to the network. .\n"

            "\nArguments:\n"
            "1. \"specification\"                  (json, required) The document specification.\n"
            "     {\n"
            "       \"ticker\":\"ticker\",           (string, optional) The ticker\n"
            "       \"name\":\"name\",               (string, optional) The token name\n"
            "       \"chain\":\"chain\",             (string, optional) Chain identifier, e.g. \"BYTZ\" (for mainnet) or \"BYTZ.testnet\" or \"BYTZ.regtest\"\n"
            "       \"summary\":\"summary\",         (string, optional) Short introduction to the token\n"
            "       \"description\":\"description\", (string, optional) Description of the token\n"
            "       \"creator\":\"creator\",         (string, optional) Token creator\n"
            "       \"contact\": {                 (object, optional) Contact information\n"
            "         \"url\": \"id\",                (string, optional) URL that points to token contact information\n"
            "         \"email\": \"email\"           (string, optional) Mail address\n"
            "       }\n"
            "     }\n"
            "2. \"signature\"                      (string, optional, default="") Fill out the signature field with a given signature string\n"
            "3. \"verbose\"                        (bool, optional, default=false) Output the json encoded specification instead of the hex-encoded serialized data\n"

            "\nResult:\n"
            "\"hex\" : \"value\",           (string) The hex-encoded raw token document\n"

            "\nExamples:\n"
            "\nCreate the MGT testnet document\n"
            + HelpExampleCli("createrawtokendocument",
                "\"{\\\"ticker\\\": \\\"MGT\\\", \\\"name\\\": \\\"Management Token\\\", \\\"chain\\\": \\\"BYTZ.testnet\\\", "
                "\\\"summary\\\": \\\"The MGT token is a tokenized management key on the BYTZ blockchain with special authorities "
                "necessary for: (1) the construction of a token system with coherent economic incentives; (2) the inception of "
                "Nucleus Tokens (special tokens that have interrelated monetary policies); and (3) the distribution of rewards that "
                "sustain this system of cryptographic tokens on the blockchain.\\\", \\\"description\\\": \\\"The Atomic Token "
                "Protocol (ATP) introduces cross-coin and cross-token policy. BYTZ utilizes ATP for its reward system and rights "
                "structure. Management Token (MGT), Guardian Validator Token (GVT), and Guardian Validators all participate in an "
                "interconnected managent system, and are considered the Nucleus Tokens. The MGT token itself is a tokenized "
                "management key with special authorities needed for token inception on the blockchain. The MGT token continues "
                "to play a role in the management of and access to special features.\\\", \\\"creator\\\": \\\"The BYTZ Core "
                "Developers\\\", \\\"contact\\\":{\\\"url\\\":\\\"https://github.com/bytzcurrency/bytz\\\"}}\"") +
            "\nCreate a partial document, add a signature, output the json specification\n"
            + HelpExampleCli("createrawtokendocument",
                "\"{\\\"ticker\\\": \\\"MGT\\\", \\\"name\\\": \\\"Management Token\\\", \\\"chain\\\": \\\"BYTZ.testnet\\\"}\" "
                "20fa4cc8f93c6d52ce6690b6997b7ae3c785fe291c5c6e44370ef1557f61aeb1242fddd9aa13941e4b5be53d07998ebb201ce2cfa96c832d5fee743c5600c7277b true") + 
            "\nCreate a partial document as a json rpc call\n"
            + HelpExampleRpc("createrawtokendocument",
                "\"{\\\"ticker\\\": \\\"MGT\\\", \\\"name\\\": \\\"Management Token\\\", \\\"chain\\\": \\\"BYTZ.testnet\\\"}\"")
        );
    }
    RPCTypeCheck(request.params, {UniValue::VOBJ, UniValue::VSTR, UniValue::VBOOL});

    std::string strTicker;
    std::string strName;
    std::string strChain;
    std::string strSummary;
    std::string strDescription;
    std::string strCreator;
    std::string strContactURL;
    std::string strContactEmail;

    UniValue spec = request.params[0];
    RPCTypeCheckObj(spec,
        {
            {"ticker", UniValueType(UniValue::VSTR)},
            {"name", UniValueType(UniValue::VSTR)},
            {"chain", UniValueType(UniValue::VSTR)},
            {"summary", UniValueType(UniValue::VSTR)},
            {"description", UniValueType(UniValue::VSTR)},
            {"creator", UniValueType(UniValue::VSTR)},
            {"contact", UniValueType()}, // will be checked below
        },
        true, true);

    if (spec.exists("ticker")) {
        strTicker = spec["ticker"].get_str();
    }
    if (spec.exists("name")) {
        strName = spec["name"].get_str();
    }
    if (spec.exists("chain")) {
        strChain = spec["chain"].get_str();
    }
    if (spec.exists("summary")) {
        strSummary = spec["summary"].get_str();
    }
    if (spec.exists("description")) {
        strDescription = spec["description"].get_str();
    }
    if (spec.exists("creator")) {
        strCreator = spec["creator"].get_str();
    }
    if (spec.exists("contact")) {
        UniValue contact = spec["contact"];
        RPCTypeCheckObj(contact,
            {
                {"url", UniValueType(UniValue::VSTR)},
                {"email", UniValueType(UniValue::VSTR)},
            },
            true, true);
        if (contact.exists("url")) {
            strContactURL = contact["url"].get_str();
        }
        if (contact.exists("email")) {
            strContactEmail = contact["email"].get_str();
        }
    }

    CTokenGroupDocument tgDocument = CTokenGroupDocument(strTicker, strName, strChain, strSummary, strDescription, strCreator, strContactURL, strContactEmail);

    if (request.params.size() > 1) {
        std::string strSignature = request.params[1].get_str();
        if (!IsHex(strSignature))
            throw std::runtime_error("invalid signature data");
        tgDocument.SetSignature(strSignature);
    }

    bool fVerbose = false;
    if (request.params.size() > 2) {
        fVerbose = request.params[2].get_bool();
    }

    if (fVerbose) {
        UniValue ret(UniValue::VOBJ);
        tgDocument.ToJson(ret);
        return ret;
    }
    CDataStream ssTGDocumentOut(SER_NETWORK, PROTOCOL_VERSION);
    ssTGDocumentOut << tgDocument;
    std::string strData = HexStr(ssTGDocumentOut.begin(), ssTGDocumentOut.end());

    return strData;
}

UniValue decoderawtokendocument(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
                "decoderawtokendocument \"data\"\n"

                "\nDecode a hex-encoded token document to json\n"

                "\nArguments:\n"
                "1. \"data\"           (hex, required) The serialized token document\n"

            "\nResult:\n"
            "{\n"
            "  \"atp\":\"atp\",                   (string) Atomic Token Protocol version number\n"
            "  \"data\": {                      (object) Data object\n"
            "    \"ticker\":\"ticker\",           (string) The ticker\n"
            "    \"name\":\"name\",               (string) The token name\n"
            "    \"chain\":\"chain\",             (string) Chain identifier, e.g. \"BYTZ\" or \"BYTZ.testnet\"\n"
            "    \"summary\":\"summary\",         (string) Short introduction to the token\n"
            "    \"description\":\"description\", (string) Description of the token\n"
            "    \"creator\":\"creator\",         (string) Token creator\n"
            "    \"contact\": {                 (object) Contact information\n"
            "       \"url\": \"id\",              (string) URL that points to token contact information\n"
            "        \"email\": \"email\"         (string) Mail address\n"
            "      }\n"
            "    },\n"
            "  \"hash\":\"hash\",                 (string) Hash of the serialized document (excluding the signature)\n"
            "  \"signature\":\"signature\",       (string) Signature of the serialized document\n"

            "\nExamples:\n"
            "\nDecode the hex-encoded MGT testnet document\n"
            + HelpExampleCli("decoderawtokendocument",
                "0100034d4754104d616e6167656d656e7420546f6b656e0c4259545a2e746573746e6574fd7b01546865204d475420746f6b656e206973206120746f6b656e697a6564206d6"
                "16e6167656d656e74206b6579206f6e20746865204259545a20626c6f636b636861696e2077697468207370656369616c20617574686f726974696573206e65636573736172"
                "7920666f723a202831292074686520636f6e737472756374696f6e206f66206120746f6b656e2073797374656d207769746820636f686572656e742065636f6e6f6d6963206"
                "96e63656e74697665733b202832292074686520696e63657074696f6e206f66204e75636c65757320546f6b656e7320287370656369616c20746f6b656e7320746861742068"
                "61766520696e74657272656c61746564206d6f6e657461727920706f6c6963696573293b20616e64202833292074686520646973747269627574696f6e206f6620726577617"
                "264732074686174207375737461696e20746869732073797374656d206f662063727970746f6772617068696320746f6b656e73206f6e2074686520626c6f636b636861696e"
                "2efd0e025468652041746f6d696320546f6b656e2050726f746f636f6c20284154502920696e74726f64756365732063726f73732d636f696e20616e642063726f73732d746"
                "f6b656e20706f6c6963792e204259545a207574696c697a65732041545020666f7220697473207265776172642073797374656d20616e642072696768747320737472756374"
                "7572652e204d616e6167656d656e7420546f6b656e20284d4754292c20477561726469616e2056616c696461746f7220546f6b656e2028475654292c20616e6420477561726"
                "469616e2056616c696461746f727320616c6c20706172746963697061746520696e20616e20696e746572636f6e6e6563746564206d616e6167656e742073797374656d2c20"
                "616e642061726520636f6e7369646572656420746865204e75636c65757320546f6b656e732e20546865204d475420746f6b656e20697473656c66206973206120746f6b656"
                "e697a6564206d616e6167656d656e74206b65792077697468207370656369616c20617574686f726974696573206e656564656420666f7220746f6b656e20696e6365707469"
                "6f6e206f6e2074686520626c6f636b636861696e2e20546865204d475420746f6b656e20636f6e74696e75657320746f20706c6179206120726f6c6520696e20746865206d6"
                "16e6167656d656e74206f6620616e642061636365737320746f207370656369616c2066656174757265732e18546865204259545a20436f726520446576656c6f7065727324"
                "68747470733a2f2f6769746875622e636f6d2f6279747a63757272656e63792f6279747a004120fa4cc8f93c6d52ce6690b6997b7ae3c785fe291c5c6e44370ef1557f61aeb"
                "1242fddd9aa13941e4b5be53d07998ebb201ce2cfa96c832d5fee743c5600c7277b")
        );
    }
    RPCTypeCheck(request.params, {UniValue::VSTR});

    CDataStream ssTGDocument(ParseHexV(request.params[0], "data"), SER_NETWORK, PROTOCOL_VERSION);
    CTokenGroupDocument tgDocument;
    ssTGDocument >> tgDocument;

    UniValue ret(UniValue::VOBJ);
    tgDocument.ToJson(ret);

    return ret;
}

UniValue verifyrawtokendocument(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
                "verifyrawtokendocument \"data\" \"address\"\n"
                "\nCalculates a diff between two deterministic masternode lists. The result also contains proof data.\n"
                "\nArguments:\n"
                "1. \"data\"           (hex, required) The starting block height.\n"
                "2. \"address\"        (string, required) The ending block height.\n"

            "\nResult:\n"
            "true|false    (boolean) If the signature is verified or not\n"

            "\nExamples:\n"
            "\nVerify the hex-encoded MGT testnet document\n"
            + HelpExampleCli("verifyrawtokendocument",
                "0100034d4754104d616e6167656d656e7420546f6b656e0c4259545a2e746573746e6574fd7b01546865204d475420746f6b656e206973206120746f6b656e697a6564206d6"
                "16e6167656d656e74206b6579206f6e20746865204259545a20626c6f636b636861696e2077697468207370656369616c20617574686f726974696573206e65636573736172"
                "7920666f723a202831292074686520636f6e737472756374696f6e206f66206120746f6b656e2073797374656d207769746820636f686572656e742065636f6e6f6d6963206"
                "96e63656e74697665733b202832292074686520696e63657074696f6e206f66204e75636c65757320546f6b656e7320287370656369616c20746f6b656e7320746861742068"
                "61766520696e74657272656c61746564206d6f6e657461727920706f6c6963696573293b20616e64202833292074686520646973747269627574696f6e206f6620726577617"
                "264732074686174207375737461696e20746869732073797374656d206f662063727970746f6772617068696320746f6b656e73206f6e2074686520626c6f636b636861696e"
                "2efd0e025468652041746f6d696320546f6b656e2050726f746f636f6c20284154502920696e74726f64756365732063726f73732d636f696e20616e642063726f73732d746"
                "f6b656e20706f6c6963792e204259545a207574696c697a65732041545020666f7220697473207265776172642073797374656d20616e642072696768747320737472756374"
                "7572652e204d616e6167656d656e7420546f6b656e20284d4754292c20477561726469616e2056616c696461746f7220546f6b656e2028475654292c20616e6420477561726"
                "469616e2056616c696461746f727320616c6c20706172746963697061746520696e20616e20696e746572636f6e6e6563746564206d616e6167656e742073797374656d2c20"
                "616e642061726520636f6e7369646572656420746865204e75636c65757320546f6b656e732e20546865204d475420746f6b656e20697473656c66206973206120746f6b656"
                "e697a6564206d616e6167656d656e74206b65792077697468207370656369616c20617574686f726974696573206e656564656420666f7220746f6b656e20696e6365707469"
                "6f6e206f6e2074686520626c6f636b636861696e2e20546865204d475420746f6b656e20636f6e74696e75657320746f20706c6179206120726f6c6520696e20746865206d6"
                "16e6167656d656e74206f6620616e642061636365737320746f207370656369616c2066656174757265732e18546865204259545a20436f726520446576656c6f7065727324"
                "68747470733a2f2f6769746875622e636f6d2f6279747a63757272656e63792f6279747a004120fa4cc8f93c6d52ce6690b6997b7ae3c785fe291c5c6e44370ef1557f61aeb"
                "1242fddd9aa13941e4b5be53d07998ebb201ce2cfa96c832d5fee743c5600c7277b Tq15q6NNKDLKsD8uRwLo8Za355afgavuVb")
        );
    }

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VSTR});

    std::string strHexDescription = request.params[0].get_str();
    std::string strAddress = request.params[1].get_str();
    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Bytz address");
    }
    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    }

    CDataStream ssTGDocument(ParseHexV(request.params[0], "data"), SER_NETWORK, PROTOCOL_VERSION);
    CTokenGroupDocument tgDocument;
    ssTGDocument >> tgDocument;

    return tgDocument.CheckSignature(*keyID);
}

static const CRPCCommand commands[] =
{ //  category              name                        actor (function)            argNames
  //  --------------------- --------------------------  --------------------------  ----------
    { "tokens",             "tokeninfo",                &tokeninfo,                 {} },
    { "tokens",             "gettokentransaction",      &gettokentransaction,       {} },
    { "tokens",             "getsubgroupid",            &getsubgroupid,             {} },
    { "tokens",             "createrawtokentransaction",&createrawtokentransaction, {} },
    { "tokens",             "createrawtokendocument",   &createrawtokendocument,    {"options", "verbose"} },
    { "tokens",             "decoderawtokendocument",   &decoderawtokendocument,    {} },
    { "tokens",             "verifyrawtokendocument",   &verifyrawtokendocument,    {"hexstring","address"} },
};

void RegisterTokensRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
