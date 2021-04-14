// Copyright (c) 2015-2018 The Bitcoin Unlimited developers
// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tokens/tokengroupwallet.h"
#include "core_io.h"
#include "dstencode.h"
#include "init.h"
#include "bytzaddrenc.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "script/tokengroup.h"
#include "tokens/tokengroupmanager.h"
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
    entry.push_back(Pair("ticker", tgCreation.tokenGroupDescription.strTicker));
    entry.push_back(Pair("name", tgCreation.tokenGroupDescription.strName));
    entry.push_back(Pair("decimalPos", tgCreation.tokenGroupDescription.nDecimalPos));
    entry.push_back(Pair("URL", tgCreation.tokenGroupDescription.strDocumentUrl));
    entry.push_back(Pair("documentHash", tgCreation.tokenGroupDescription.documentHash.ToString()));
    if (extended) {
        UniValue extendedEntry(UniValue::VOBJ);
        extendedEntry.push_back(Pair("txid", tgCreation.creationTransaction->GetHash().GetHex()));
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
            HelpExampleCli("tokeninfo", "ticker \"XDM\"") +
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
        for (auto tokenGroupMapping : tokenGroupManager->GetMapTokenGroups()) {
            entry.push_back(Pair(tokenGroupMapping.second.tokenGroupDescription.strTicker, EncodeTokenGroup(tokenGroupMapping.second.tokenGroupInfo.associatedGroup)));
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

        for (auto tokenGroupMapping : tokenGroupManager->GetMapTokenGroups()) {
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
        uint64_t nXDMTransactions = pindex ? pindex->nChainXDMTransactions : 0;
        uint64_t nXDMSupply = pindex ? pindex->nXDMSupply : 0;
        uint64_t nHeight = pindex ? pindex->nHeight : -1;

        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("height", nHeight));
        entry.push_back(Pair("blockhash", hash.GetHex()));


        if (tokenGroupManager->DarkMatterTokensCreated()) {
            entry.push_back(Pair("XDM_supply", tokenGroupManager->TokenValueFromAmount(nXDMSupply, tokenGroupManager->GetDarkMatterID())));
            entry.push_back(Pair("XDM_transactions", (uint64_t)nXDMTransactions));
        }
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
        tokenGroupManager->GetTokenGroupCreation(grpID, tgCreation);
        TokenGroupCreationToJSON(grpID, tgCreation, entry, extended);
        ret.push_back(entry);
    } else if (operation == "ticker") {
        if (request.params.size() > 3) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Too many parameters");
        }

        std::string ticker;
        CTokenGroupID grpID;
        tokenGroupManager->GetTokenGroupIdByTicker(request.params[curparam].get_str(), grpID);
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
        tokenGroupManager->GetTokenGroupCreation(grpID, tgCreation);

        LogPrint(BCLog::TOKEN, "%s - tokenGroupCreation has [%s] [%s]\n", __func__, tgCreation.tokenGroupDescription.strTicker, EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup));
        UniValue entry(UniValue::VOBJ);
        TokenGroupCreationToJSON(grpID, tgCreation, entry, extended);
        ret.push_back(entry);
    } else if (operation == "name") {
        if (request.params.size() > 3) {
            throw JSONRPCError(RPC_INVALID_PARAMS, "Too many parameters");
        }

        std::string name;
        CTokenGroupID grpID;
        tokenGroupManager->GetTokenGroupIdByName(request.params[curparam].get_str(), grpID);
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
        tokenGroupManager->GetTokenGroupCreation(grpID, tgCreation);

        LogPrint(BCLog::TOKEN, "%s - tokenGroupCreation has [%s] [%s]\n", __func__, tgCreation.tokenGroupDescription.strTicker, EncodeTokenGroup(tgCreation.tokenGroupInfo.associatedGroup));
        UniValue entry(UniValue::VOBJ);
        TokenGroupCreationToJSON(grpID, tgCreation, entry, extended);
        ret.push_back(entry);
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid parameter: unknown operation");
    }
    return ret;
}

void RpcTokenTxnoutToUniv(const CTxOut& txout,
    UniValue& out, bool& fExpectFirstOpReturn)
{
    CTokenGroupInfo tokenGroupInfo(txout.scriptPubKey);

    if (fExpectFirstOpReturn && (txout.nValue == 0) && (txout.scriptPubKey[0] == OP_RETURN)) {
        fExpectFirstOpReturn = false;

        CTokenGroupDescription tokenGroupDescription = CTokenGroupDescription(txout.scriptPubKey);

        out.pushKV("outputType", "description");
        out.pushKV("ticker", tokenGroupDescription.strTicker);
        out.pushKV("name", tokenGroupDescription.strName);
        out.pushKV("decimalPos", tokenGroupDescription.nDecimalPos);
        out.pushKV("URL", tokenGroupDescription.strDocumentUrl);
        out.pushKV("documentHash", tokenGroupDescription.documentHash.ToString());
    } else if (!tokenGroupInfo.invalid && tokenGroupInfo.associatedGroup != NoGroup) {
        CTokenGroupCreation tgCreation;
        std::string tgTicker;
        if (tokenGroupInfo.associatedGroup.isSubgroup()) {
            CTokenGroupID parentgrp = tokenGroupInfo.associatedGroup.parentGroup();
            std::vector<unsigned char> subgroupData = tokenGroupInfo.associatedGroup.GetSubGroupData();
            tgTicker = tokenGroupManager->GetTokenGroupTickerByID(parentgrp);
            out.pushKV("parentGroupID", EncodeTokenGroup(parentgrp));
            out.pushKV("subgroupData", std::string(subgroupData.begin(), subgroupData.end()));
        } else {
            tgTicker = tokenGroupManager->GetTokenGroupTickerByID(tokenGroupInfo.associatedGroup);
        }
        out.pushKV("groupIdentifier", EncodeTokenGroup(tokenGroupInfo.associatedGroup));
        if (tokenGroupInfo.isAuthority()){
            out.pushKV("outputType", "authority");
            out.pushKV("ticker", tgTicker);
            out.pushKV("authorities", EncodeGroupAuthority(tokenGroupInfo.controllingGroupFlags()));
        } else {
            out.pushKV("outputType", "amount");
            out.pushKV("ticker", tgTicker);
            out.pushKV("value", tokenGroupManager->TokenValueFromAmount(tokenGroupInfo.getAmount(), tokenGroupInfo.associatedGroup));
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
    CScript firstOpReturn;
    bool fIsGroupConfigurationTX = IsAnyOutputGroupedCreation(*tx);
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
        RpcTokenTxnoutToUniv(txout, t, fIsGroupConfigurationTX);
        if (t.size() > 0)
            out.pushKV("token", t);

        vout.push_back(out);
    }
    entry.pushKV("vout", vout);

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

static const CRPCCommand commands[] =
{ //  category              name                        actor (function)            argNames
  //  --------------------- --------------------------  --------------------------  ----------
    { "tokens",             "tokeninfo",                &tokeninfo,                 {} },
    { "tokens",             "gettokentransaction",      &gettokentransaction,       {} },
    { "tokens",             "getsubgroupid",            &getsubgroupid,             {} },
};

void RegisterTokensRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
