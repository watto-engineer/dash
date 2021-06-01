// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tokens/tokengroupdocument.h"
#include "util.h"
#include <utilstrencodings.h>
#include <messagesigner.h>
#include <univalue.h>

/*
{
  "atp": "1.0",
  "data": {
    "ticker": "MGT",
    "name": "Management Token",
    "chain": "BYTZ.testnet",
    "summary": "The MGT token is a tokenized management key on the BYTZ blockchain with special authorities necessary for: (1) the construction of a token system with coherent economic incentives; (2) the inception of Nucleus Tokens (special tokens that have interrelated monetary policies); and (3) the distribution of rewards that sustain this system of cryptographic tokens on the blockchain.",
    "description": "The Atomic Token Protocol (ATP) introduces cross-coin and cross-token policy. BYTZ utilizes ATP for its reward system and rights structure. Management Token (MGT), Guardian Validator Token (GVT), and Guardian Validators all participate in an interconnected managent system, and are considered the Nucleus Tokens. The MGT token itself is a tokenized management key with special authorities needed for token inception on the blockchain. The MGT token continues to play a role in the management of and access to special features.",
    "creator": "The BYTZ Core Developers",
    "contact": {
      "url": "https://github.com/bytzcurrency/bytz"
    }
  },
  "hash": "467723bc0c114a4459e05d8d422ee6766dede23019190a68005290cc053a7e36",
  "signature": ""
}
*/
void CTokenGroupDocument::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("atp", nVersion);
    UniValue dataObj(UniValue::VOBJ);
    dataObj.pushKV("ticker", strTicker);
    dataObj.pushKV("name", strName);
    dataObj.pushKV("chain", strChain);
    dataObj.pushKV("summary", strSummary);
    dataObj.pushKV("description", strDescription);
    dataObj.pushKV("creator", strCreator);
    UniValue dataContactObj(UniValue::VOBJ);
    dataContactObj.pushKV("url", strContactURL);
    dataContactObj.pushKV("email", strContactEmail);
    dataObj.pushKV("contact", dataContactObj);
    obj.pushKV("data", dataObj);
    obj.pushKV("hash", GetSignatureHash().GetHex());
    obj.pushKV("signature", HexStr(vchSig));
}


uint256 CTokenGroupDocument::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CTokenGroupDocument::GetSignatureHash() const
{
    return GetHash();
}

bool CTokenGroupDocument::Sign(const CKey& key)
{
    if (!key.IsValid()) {
        LogPrintf("CTokenGroupDocument::Sign -- signing key is not valid\n");
        return false;
    }

    CKeyID pubKeyId = key.GetPubKey().GetID();
    std::string strError = "";

    uint256 hash = GetSignatureHash();
    if (!CHashSigner::SignHash(hash, key, vchSig)) {
        LogPrintf("CTokenGroupDocument::Sign -- SignMessage() failed\n");
        return false;
    }


    if(!CHashSigner::VerifyHash(hash, pubKeyId, vchSig, strError)) {
        LogPrintf("CTokenGroupDocument::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CTokenGroupDocument::CheckSignature(const CKeyID& pubKeyId) const
{
    std::string strError = "";

    uint256 hash = GetSignatureHash();

    if(!CHashSigner::VerifyHash(hash, pubKeyId, vchSig, strError)) {
        LogPrintf("CTokenGroupDocument::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CTokenGroupDocument::GetSignerKeyID(CKeyID &retKeyidSporkSigner) const
{
    CPubKey pubkeyFromSig;
    uint256 hash = GetSignatureHash();
    CHashWriter ss(SER_GETHASH, 0);
    if (!pubkeyFromSig.RecoverCompact(hash, vchSig)) {
        return false;
    }

    retKeyidSporkSigner = pubkeyFromSig.GetID();
    return true;
}

