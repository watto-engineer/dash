// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <tokens/tokengroupdocument.h>

#include <primitives/transaction.h>
#include <rpc/server.h>
#include <util.h>
#include <utilstrencodings.h>
#include <messagesigner.h>
#include <univalue.h>

// Sets the json object, returns status
bool CTokenGroupDocument::LoadJSONData()
{
    bool res = false;
    if (!fRawLoaded) {
        fUnparsable = true;
        return false;
    }

    try {
        // ATTEMPT TO LOAD JSON STRING FROM VCHDATA
        res = GetJSONFromRawData(data);
        LogPrint(BCLog::TOKEN, "CTokenGroupDocument::LoadJSONData -- GetDataAsPlainString = %s\n", GetDataAsPlainString());
    } catch (std::exception& e) {
        std::ostringstream ostr;
        ostr << "CTokenGroupDocument::LoadJSONData Error parsing JSON"
             << ", e.what() = " << e.what();
        LogPrintf("%s\n", ostr.str());
        return false;
    } catch (...) {
        std::ostringstream ostr;
        ostr << "CTokenGroupDocument::LoadJSONData Unknown Error parsing JSON";
        LogPrintf("%s\n", ostr.str());
        return false;
    }
    return res;
}

// GetJSONFromRawData loads vchData into objResult. Throws an exception on failure.
bool CTokenGroupDocument::GetJSONFromRawData(UniValue& objResult)
{
    UniValue o(UniValue::VOBJ);
    std::string s = GetDataAsPlainString();
    bool res = o.read(s);
    objResult = o;
    return res;
}

std::string CTokenGroupDocument::GetDataAsHexString() const
{
    return HexStr(vchData);
}

std::string CTokenGroupDocument::GetDataAsPlainString() const
{
    return std::string(vchData.begin(), vchData.end());
}

std::vector<unsigned char> CTokenGroupDocument::GetRawDataFromJson() const
{
    std::string strPlainText = data.write(true, 2);
    return std::vector<unsigned char>(strPlainText.begin(), strPlainText.end());
}


bool CTokenGroupDocument::ParseATPParams() {
    bool res = false;

    const UniValue& atp = data["atp"];
    const UniValue& jsonVersion = find_value(atp, "version");

    if (!jsonVersion.isNum() && !jsonVersion.isStr())
        throw JSONRPCError(RPC_TYPE_ERROR, "Version is not a number or string");

    int64_t nVersionIn;
    if (!ParseFixedPoint(jsonVersion.getValStr(), 0, &nVersionIn))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid version");

    if (nVersionIn < 0 || nVersionIn > std::numeric_limits<uint16_t>::max()) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Version out of range");
    }
    nVersion = nVersionIn;

    std::string strType = atp["type"].get_str();
    if (strType == "regular") {
        nSpecialTxType = TRANSACTION_GROUP_CREATION_REGULAR;
    } else if (strType == "management") {
        nSpecialTxType = TRANSACTION_GROUP_CREATION_MGT;
    } else if (strType == "nft") {
        nSpecialTxType = TRANSACTION_GROUP_CREATION_NFT;
    } else {
        std::string err = strprintf("Invalid token type %s", strType);
        throw JSONRPCError(RPC_TYPE_ERROR, err);
    }
    return true;
}

void CTokenGroupDocument::ToJson(UniValue& obj) const
{
    obj = data;
}

bool CTokenGroupDocument::ValidateData() const {
    if (!fParsed || !fJsonLoaded) {
        return false;
    }
    switch (nSpecialTxType)
    {
    case TRANSACTION_GROUP_CREATION_REGULAR:
    case TRANSACTION_GROUP_CREATION_MGT:
        // The following fields must be present - other fields are also allowed
        RPCTypeCheckObj(data,
            {
                {"atp", UniValueType(UniValue::VOBJ)},
                {"ticker", UniValueType(UniValue::VSTR)},
                {"name", UniValueType(UniValue::VSTR)},
                {"chain", UniValueType(UniValue::VSTR)},
                {"creator", UniValueType(UniValue::VSTR)},
                {"description", UniValueType(UniValue::VSTR)},
                {"attributes_url", UniValueType(UniValue::VSTR)}
            },
            false, false);

        // If following optional fields are present, they must have these types
        RPCTypeCheckObj(data,
            {
                {"external_url",  UniValueType(UniValue::VSTR)},
                {"image",  UniValueType(UniValue::VSTR)},
                {"summary",  UniValueType(UniValue::VSTR)},
                {"attributes",  UniValueType(UniValue::VARR)},
                {"properties",  UniValueType(UniValue::VARR)},
                {"localization",  UniValueType(UniValue::VOBJ)}
            },
            true, false);

        break;
    case TRANSACTION_GROUP_CREATION_NFT:
        // The following fields must be present - other fields are also allowed
        RPCTypeCheckObj(data,
            {
                {"atp", UniValueType(UniValue::VOBJ)},
                {"name", UniValueType(UniValue::VSTR)},
                {"chain", UniValueType(UniValue::VSTR)},
                {"creator", UniValueType(UniValue::VSTR)},
                {"description", UniValueType(UniValue::VSTR)},
                {"attributes_url", UniValueType(UniValue::VSTR)}
            },
            false, false);

        // If following optional fields are present, they must have these types
        RPCTypeCheckObj(data,
            {
                {"external_url",  UniValueType(UniValue::VSTR)},
                {"image",  UniValueType(UniValue::VSTR)},
                {"attributes",  UniValueType(UniValue::VARR)},
                {"properties",  UniValueType(UniValue::VARR)},
                {"localization",  UniValueType(UniValue::VOBJ)}
            },
            true, false);

        break;

    default:
        return false;
        break;
    }

    RPCTypeCheckObj(data["atp"],
        {
            {"version", UniValueType()}, // Checked explicitly in ParseATPParams()
            {"type", UniValueType(UniValue::VSTR)},
        },
        false, true);

    if (data.exists("localization")) {
        UniValue localization = data["localization"];
        RPCTypeCheckObj(localization,
            {
                {"uri", UniValueType(UniValue::VSTR)},
                {"default", UniValueType(UniValue::VSTR)},
                {"locales", UniValueType(UniValue::VARR)},
            },
            false, true);
    }

    return true;
}

uint256 CTokenGroupDocument::GetSignatureHash() const
{
    return SerializeHash(vchData);
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

void CTokenGroupDocument::SetSignature(const std::string& strSignature) {
    vchSig = ParseHex(strSignature);
}

std::string CTokenGroupDocument::GetSignature() {
    return HexStr(vchSig);
}