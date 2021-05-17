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
    entry.push_back(Pair("ticker", tgCreation.pTokenGroupDescription->strTicker));
    entry.push_back(Pair("name", tgCreation.pTokenGroupDescription->strName));
    if (tgCreation.creationTransaction->nType == TRANSACTION_GROUP_CREATION_REGULAR)
        entry.push_back(Pair("decimalPos", ((CTokenGroupDescriptionRegular *)(tgCreation.pTokenGroupDescription.get()))->nDecimalPos));
    if (tgCreation.creationTransaction->nType == TRANSACTION_GROUP_CREATION_MGT)
        entry.push_back(Pair("decimalPos", ((CTokenGroupDescriptionMGT *)(tgCreation.pTokenGroupDescription.get()))->nDecimalPos));
    entry.push_back(Pair("URL", tgCreation.pTokenGroupDescription->strDocumentUrl));
    entry.push_back(Pair("documentHash", tgCreation.pTokenGroupDescription->documentHash.ToString()));
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
            entry.push_back(Pair(tokenGroupMapping.second.pTokenGroupDescription->strTicker, EncodeTokenGroup(tokenGroupMapping.second.tokenGroupInfo.associatedGroup)));
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

        LogPrint(BCLog::TOKEN, "%s - tokenGroupCreation has [%s] [%s]\n", __func__, tgCreation.pTokenGroupDescription->strTicker, EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup));
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

        LogPrint(BCLog::TOKEN, "%s - tokenGroupCreation has [%s] [%s]\n", __func__, tgCreation.pTokenGroupDescription->strTicker, EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup));
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
    }
    if (tx->nType == TRANSACTION_GROUP_CREATION_MGT) {
        CTokenGroupDescriptionMGT tgDesc;
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

static const CRPCCommand commands[] =
{ //  category              name                        actor (function)            argNames
  //  --------------------- --------------------------  --------------------------  ----------
    { "tokens",             "tokeninfo",                &tokeninfo,                 {} },
    { "tokens",             "gettokentransaction",      &gettokentransaction,       {} },
    { "tokens",             "getsubgroupid",            &getsubgroupid,             {} },
    { "tokens",             "createrawtokentransaction",&createrawtokentransaction, {} },
};

void RegisterTokensRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
