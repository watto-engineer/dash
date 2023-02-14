// Copyright (c) 2019 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tokens/tokengroupdescription.h"

#include <consensus/consensus.h>
#include <logging.h>
#include "util/strencodings.h"
#include <rpc/protocol.h>
#include <rpc/server.h>

void CTokenGroupDescriptionRegular::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("ticker", strTicker);
    obj.pushKV("name", strName);
    obj.pushKV("metadata_url", strDocumentUrl);
    obj.pushKV("metadata_hash", documentHash.ToString());
    obj.pushKV("decimal_pos", (int)nDecimalPos);
}

void CTokenGroupDescriptionMGT::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("ticker", strTicker);
    obj.pushKV("name", strName);
    obj.pushKV("metadata_url", strDocumentUrl);
    obj.pushKV("metadata_hash", documentHash.ToString());
    obj.pushKV("decimal_pos", (int)nDecimalPos);
    obj.pushKV("bls_pubkey", blsPubKey.ToString());
}

void CTokenGroupDescriptionNFT::ToJson(UniValue& obj, const bool& fFull) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("name", strName);
    obj.pushKV("metadata_url", strDocumentUrl);
    obj.pushKV("metadata_hash", documentHash.ToString());
    if (fFull) {
        obj.pushKV("data_filename", strDataFilename);
        obj.pushKV("data_base64", EncodeBase64(vchData));
    }
}

void CTokenGroupDescriptionBetting::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.pushKV("event_id", (uint64_t)nEventId);
    obj.pushKV("signer_type", (int)signerType);
    obj.pushKV("signer_hash", signerHash.ToString());
    obj.pushKV("bls_pubkey", blsPubKey.ToString());
    obj.pushKV("bls_signature", blsSig.ToString());
}

uint256 CTokenGroupDescriptionBetting::GetSignatureHash() const
{
    return SerializeHash(*this);
}

bool CTokenGroupDescriptionBetting::Sign(const CBLSSecretKey& key)
{
    CBLSSignature sig = key.Sign(GetSignatureHash());
    if (!sig.IsValid()) {
        return false;
    }
    blsSig = sig;
    return true;
}

bool CTokenGroupDescriptionBetting::CheckSignature() const
{
    if (!blsPubKey.IsValid()) {
        return false;
    }
    if (!blsSig.VerifyInsecure(blsPubKey, GetSignatureHash())) {
        LogPrintf("CTokenGroupDescriptionBetting::CheckSignature -- VerifyInsecure() failed\n");
        return false;
    }
    return true;
}


std::string ConsumeParamTicker(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Missing parameter: token name");
    }
    std::string strTicker = request.params[curparam].get_str();
    if (strTicker.size() > 10) {
        std::string strError = strprintf("Ticker %s has too many characters (10 max)", strTicker);
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    curparam++;
    return strTicker;
}

std::string ConsumeParamName(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        throw JSONRPCError(RPC_INVALID_PARAMS, "Missing parameter: token name");
    }
    std::string strName = request.params[curparam].get_str();
    if (strName.size() > 80) {
        std::string strError = strprintf("Name %s has too many characters (80 max)", strName);
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    curparam++;
    return strName;
}

std::string ConsumeParamDocumentURL(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    std::string strDocumentUrl = request.params[curparam].get_str();
    if (strDocumentUrl.size() > 98) {
        std::string strError = strprintf("URL %s has too many characters (98 max)", strDocumentUrl);
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    curparam++;
    return strDocumentUrl;
}

uint8_t ConsumeParamDecimalPos(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    std::string strCurparamValue = request.params[curparam].get_str();
    int32_t nDecimalPos32;
    if (!ParseInt32(strCurparamValue, &nDecimalPos32) || nDecimalPos32 > 16 || nDecimalPos32 < 0) {
        std::string strError = strprintf("Parameter %s is invalid - valid values are between 0 and 16", strCurparamValue);
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    curparam++;
    return (uint8_t)nDecimalPos32;
}

uint32_t ConsumeParamEventID(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    std::string strCurparamValue = request.params[curparam].get_str();
    int32_t nEventID;
    if (!ParseInt32(strCurparamValue, &nEventID)) {
        std::string strError = strprintf("Parameter %s is invalid", strCurparamValue);
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    // Check if event exists
    curparam++;
    return nEventID;
}

uint8_t ConsumeParamSignerType(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    std::string strCurparamValue = request.params[curparam].get_str();
    int32_t signerType32;
    if (!ParseInt32(strCurparamValue, &signerType32) || signerType32 > 2 || signerType32 < 0) {
        std::string strError = strprintf("Parameter %s is invalid - valid values are between 0 and 2", strCurparamValue);
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    curparam++;
    return (uint8_t)signerType32;
}

uint256 ConsumeParamHash(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        // If you have a URL to the TDD, you need to have a hash or the token creator
        // could change the document without holders knowing about it.
        throw JSONRPCError(RPC_INVALID_PARAMS, "Missing parameter: token description document hash");
    }
    std::string strCurparamValue = request.params[curparam].get_str();
    uint256 hash;
    hash.SetHex(strCurparamValue);
    curparam++;
    return hash;
}

CBLSPublicKey ConsumeParamBLSPublicKey(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    std::string strCurparamValue = request.params[curparam].get_str();
    CBLSPublicKey blsPubKey;
    blsPubKey.Reset();
    if (!blsPubKey.SetHexStr(strCurparamValue)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("bls_pubkey must be a valid BLS public key, not %s", strCurparamValue));
    }
    curparam++;
    return blsPubKey;
}

CBLSSignature ConsumeParamBLSSignature(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    std::string strCurparamValue = request.params[curparam].get_str();
    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(strCurparamValue.c_str(), &fInvalid);

    if (fInvalid) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");
    }

    CBLSSignature blsSig;
    blsSig.SetByteVector(vchSig);
    if (!blsSig.IsValid()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("bls_signature must be a valid BLS signature, not %s", strCurparamValue));
    }
    curparam++;
    return blsSig;
}

std::vector<unsigned char> ConsumeParamNFTData(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    std::string strData = request.params[curparam].get_str();
    bool fInvalid = false;
    std::vector<unsigned char> vchData = DecodeBase64(strData.c_str(), &fInvalid);
    if (fInvalid) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Malformed base64 encoding");
    }
    if (vchData.size() > MAX_TX_NFT_DATA) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Too much data");
    }
    curparam++;
    return vchData;
}

uint64_t ConsumeParamMintAmount(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    std::string strCurparamValue = request.params[curparam].get_str();
    uint64_t nMintAmount;
    if (!ParseUInt64(strCurparamValue, &nMintAmount) || !MoneyRange(nMintAmount)) {
        std::string strError = strprintf("Parameter %s is invalid", strCurparamValue);
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    curparam++;
    return nMintAmount;
}

std::string ConsumeParamFilename(const JSONRPCRequest& request, unsigned int &curparam) {
    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    std::string strFilename = request.params[curparam].get_str();
    if (strFilename.size() > 98) {
        std::string strError = strprintf("Filename %s has too many characters (98 max)", strFilename);
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    curparam++;
    return strFilename;
}

bool ParseGroupDescParamsRegular(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionRegular>& tgDesc, bool &confirmed)
{
    std::string strCurparamValue;

    confirmed = false;

    std::string strTicker = ConsumeParamTicker(request, curparam);
    std::string strName = ConsumeParamName(request, curparam);
    uint8_t nDecimalPos = ConsumeParamDecimalPos(request, curparam);
    std::string strDocumentUrl = ConsumeParamDocumentURL(request, curparam);
    uint256 documentHash = ConsumeParamHash(request, curparam);

    tgDesc = std::make_shared<CTokenGroupDescriptionRegular>(strTicker, strName, nDecimalPos, strDocumentUrl, documentHash);

    if (curparam >= request.params.size())
    {
        return true;
    }
    if (request.params[curparam].get_str() == "true") {
        confirmed = true;
        return true;
    }
    return true;
}

bool ParseGroupDescParamsMGT(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionMGT>& tgDesc, bool &stickyMelt, bool &confirmed)
{
    std::string strCurparamValue;

    confirmed = false;
    stickyMelt = false;

    std::string strTicker = ConsumeParamTicker(request, curparam);
    std::string strName = ConsumeParamName(request, curparam);
    uint8_t nDecimalPos = ConsumeParamDecimalPos(request, curparam);
    std::string strDocumentUrl = ConsumeParamDocumentURL(request, curparam);
    uint256 documentHash = ConsumeParamHash(request, curparam);
    CBLSPublicKey blsPubKey = ConsumeParamBLSPublicKey(request, curparam);

    tgDesc = std::make_shared<CTokenGroupDescriptionMGT>(strTicker, strName, nDecimalPos, strDocumentUrl, documentHash, blsPubKey);

    if (curparam >= request.params.size())
    {
        std::string strError = strprintf("Not enough paramaters");
        throw JSONRPCError(RPC_INVALID_PARAMS, strError);
    }
    strCurparamValue = request.params[curparam].get_str();
    if (strCurparamValue == "true") {
        stickyMelt = true;
    }
    curparam++;

    if (curparam >= request.params.size())
    {
        return true;
    }
    if (request.params[curparam].get_str() == "true") {
        confirmed = true;
        return true;
    }
    return true;
}

bool ParseGroupDescParamsNFT(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionNFT>& tgDesc, bool &confirmed)
{
    std::string strCurparamValue;

    confirmed = false;

    std::string strName = ConsumeParamName(request, curparam);
    uint64_t nMintAmount = ConsumeParamMintAmount(request, curparam);
    std::string strDocumentUrl = ConsumeParamDocumentURL(request, curparam);
    uint256 documentHash = ConsumeParamHash(request, curparam);
    std::vector<unsigned char> vchData = ConsumeParamNFTData(request, curparam);
    std::string strDataFilename = ConsumeParamFilename(request, curparam);

    tgDesc = std::make_shared<CTokenGroupDescriptionNFT>(strName, nMintAmount, strDocumentUrl, documentHash, vchData, strDataFilename);

    if (curparam >= request.params.size())
    {
        return true;
    }
    if (request.params[curparam].get_str() == "true") {
        confirmed = true;
        return true;
    }
    return true;
}

bool ParseGroupDescParamsBetting(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionBetting>& tgDesc, bool &confirmed)
{
    std::string strCurparamValue;

    confirmed = false;

    uint32_t nEventId = ConsumeParamEventID(request, curparam);
    uint8_t signerType = ConsumeParamSignerType(request, curparam);
    uint256 signerHash = ConsumeParamHash(request, curparam);
    CBLSPublicKey blsPubKey = ConsumeParamBLSPublicKey(request, curparam);
    CBLSSignature blsSig = CBLSSignature();

    tgDesc = std::make_shared<CTokenGroupDescriptionBetting>(nEventId, signerType, signerHash, blsPubKey, blsSig);

    if (curparam >= request.params.size())
    {
        return true;
    }
    tgDesc->blsSig = ConsumeParamBLSSignature(request, curparam);

    if (curparam >= request.params.size())
    {
        return true;
    }
    if (request.params[curparam].get_str() == "true") {
        confirmed = true;
        return true;
    }
    return true;
}
