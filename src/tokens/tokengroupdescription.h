// Copyright (c) 2019 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_GROUP_DESCRIPTION_H
#define TOKEN_GROUP_DESCRIPTION_H

#include "amount.h"
#include <bls/bls.h>
#include "script/script.h"
#include <primitives/transaction.h>
#include "uint256.h"
#include <tinyformat.h>
#include <univalue.h>

#include <boost/variant.hpp>

class JSONRPCRequest;

static CAmount COINFromDecimalPos(const uint8_t& nDecimalPos) {
    uint8_t n = nDecimalPos <= 16 ? nDecimalPos : 0;
    static CAmount pow10[17] = {
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000,
        100000000, 1000000000, 10000000000, 100000000000, 1000000000000, 10000000000000, 100000000000000, 1000000000000000, 10000000000000000
    };

    return pow10[n];
}

class CTokenGroupDescriptionRegular
{
public:
    static const uint16_t CURRENT_VERSION = 1;
    static const int SPECIALTX_TYPE = TRANSACTION_GROUP_CREATION_REGULAR;

public:
    uint16_t nVersion{CURRENT_VERSION};

    std::string strTicker; // Token ticker name
    std::string strName; // Token name
    std::string strDocumentUrl; // Extended token description document URL
    uint256 documentHash;
    uint8_t nDecimalPos; // Decimal position to translate between token value and amount

    CTokenGroupDescriptionRegular() {
        SetNull();
    };
    CTokenGroupDescriptionRegular(std::string strTicker, std::string strName, uint8_t nDecimalPos, std::string strDocumentUrl, uint256 documentHash) :
        strTicker(strTicker), strName(strName), strDocumentUrl(strDocumentUrl), documentHash(documentHash), nDecimalPos(nDecimalPos) { };

    void SetNull() {
        strTicker = "";
        strName = "";
        strDocumentUrl = "";
        documentHash = uint256();
    }

    SERIALIZE_METHODS(CTokenGroupDescriptionRegular, obj)
    {
        READWRITE(obj.nVersion);
        READWRITE(obj.strTicker);
        READWRITE(obj.strName);
        READWRITE(obj.strDocumentUrl);
        READWRITE(obj.documentHash);
        READWRITE(obj.nDecimalPos);
    }
    void ToJson(UniValue& obj) const;
    void WriteHashable(CHashWriter& ss) const {
        ss << nVersion;
        ss << strTicker;
        ss << strName;
        ss << strDocumentUrl;
        ss << documentHash;
        ss << nDecimalPos;
    }

    bool operator==(const CTokenGroupDescriptionRegular &c)
    {
        return (strTicker == c.strTicker &&
                strName == c.strName &&
                nDecimalPos == c.nDecimalPos &&
                strDocumentUrl == c.strDocumentUrl &&
                documentHash == c.documentHash);
    }
};

class CTokenGroupDescriptionMGT
{
public:
    static const uint16_t CURRENT_VERSION = 1;
    static const int SPECIALTX_TYPE = TRANSACTION_GROUP_CREATION_MGT;

    uint16_t nVersion{CURRENT_VERSION};

    std::string strTicker; // Token ticker name
    std::string strName; // Token name
    std::string strDocumentUrl; // Extended token description document URL
    uint256 documentHash;
    uint8_t nDecimalPos; // Decimal position to translate between token value and amount

    CBLSPublicKey blsPubKey; // BLS Public Key that enables signing

    CTokenGroupDescriptionMGT() {
        SetNull();
    };
    CTokenGroupDescriptionMGT(std::string strTicker, std::string strName, uint8_t nDecimalPos, std::string strDocumentUrl, uint256 documentHash, CBLSPublicKey blsPubKey) :
        strTicker(strTicker), strName(strName), strDocumentUrl(strDocumentUrl), documentHash(documentHash), nDecimalPos(nDecimalPos), blsPubKey(blsPubKey) { };

    void SetNull() {
        strTicker = "";
        strName = "";
        strDocumentUrl = "";
        documentHash = uint256();
        nDecimalPos = 0;
        blsPubKey = CBLSPublicKey();
    }

    SERIALIZE_METHODS(CTokenGroupDescriptionMGT, obj)
    {
        READWRITE(obj.nVersion);
        READWRITE(obj.strTicker);
        READWRITE(obj.strName);
        READWRITE(obj.strDocumentUrl);
        READWRITE(obj.documentHash);
        READWRITE(obj.nDecimalPos);
        READWRITE(obj.blsPubKey);
    }
    void ToJson(UniValue& obj) const;
    void WriteHashable(CHashWriter& ss) const {
        ss << nVersion;
        ss << strTicker;
        ss << strName;
        ss << strDocumentUrl;
        ss << documentHash;
        ss << nDecimalPos;
    }

    bool operator==(const CTokenGroupDescriptionMGT &c)
    {
        return (strTicker == c.strTicker &&
                strName == c.strName &&
                strDocumentUrl == c.strDocumentUrl &&
                documentHash == c.documentHash &&
                nDecimalPos == c.nDecimalPos &&
                blsPubKey == c.blsPubKey);
    }
};

class CTokenGroupDescriptionNFT
{
public:
    static const uint16_t CURRENT_VERSION = 1;
    static const int SPECIALTX_TYPE = TRANSACTION_GROUP_CREATION_NFT;

    uint16_t nVersion{CURRENT_VERSION};

    std::string strName; // Token name
    uint64_t nMintAmount; // Fixed token mint amount
    std::string strDocumentUrl; // Extended token description document URL
    uint256 documentHash;

    std::vector<unsigned char> vchData;
    std::string strDataFilename; // File name for the data

    CTokenGroupDescriptionNFT() {
        SetNull();
    };
    CTokenGroupDescriptionNFT(std::string strName, uint64_t nMintAmount, std::string strDocumentUrl, uint256 documentHash, std::vector<unsigned char> vchData, std::string strDataFilename) :
        strName(strName), nMintAmount(nMintAmount), strDocumentUrl(strDocumentUrl), documentHash(documentHash), vchData(vchData), strDataFilename(strDataFilename) { };

    void SetNull() {
        strName = "";
        nMintAmount = 0;
        strDocumentUrl = "";
        documentHash = uint256();
        vchData.clear();
        strDataFilename = "";
    }

    SERIALIZE_METHODS(CTokenGroupDescriptionNFT, obj)
    {
        READWRITE(obj.nVersion);
        READWRITE(obj.strName);
        READWRITE(obj.nMintAmount);
        READWRITE(obj.strDocumentUrl);
        READWRITE(obj.documentHash);
        READWRITE(obj.vchData);
        READWRITE(obj.strDataFilename);
    }
    void ToJson(UniValue& obj, const bool& fFull = false) const;

    void WriteHashable(CHashWriter& ss) const {
        ss << nVersion;
        ss << strName;
        ss << nMintAmount;
        ss << strDocumentUrl;
        ss << documentHash;
        ss << vchData;
        ss << strDataFilename;
    }

    bool operator==(const CTokenGroupDescriptionNFT &c)
    {
        return (strName == c.strName &&
                nMintAmount == c.nMintAmount &&
                strDocumentUrl == c.strDocumentUrl &&
                documentHash == c.documentHash &&
                vchData == c.vchData &&
                strDataFilename == c.strDataFilename);
    }
};

class CTokenGroupDescriptionBetting
{
public:
    static const uint16_t CURRENT_VERSION = 1;
    static const int SPECIALTX_TYPE = TRANSACTION_GROUP_CREATION_BETTING;

    uint16_t nVersion{CURRENT_VERSION};

    uint32_t nEventId; // Unique ID, referring to either a peerless event or a field bet event
    std::string strDocumentUrl; // E.g. wagerr://api/bet/events?eventId=120841
    uint256 documentHash;

    uint8_t signerType; // '1' when signed by a MGT token, '2' when signed by an ORACLE token, '3' when signed by a LLMQ
    uint256 signerHash; // hash of the signer ID (either token ID or quorum hash)
    CBLSPublicKey blsPubKey; // BLS Public Key that enables signing
    CBLSSignature blsSig; // BLS Signature over nVersion, nEventId, strDocumentUrl, documentHash, signerType, signerHash and blsPubKey

    CTokenGroupDescriptionBetting() {
        SetNull();
    };
    CTokenGroupDescriptionBetting(uint32_t nEventId, std::string strDocumentUrl, uint256 documentHash, uint8_t signerType, uint256 signerHash, CBLSPublicKey blsPubKey, CBLSSignature blsSig) :
        nEventId(nEventId), strDocumentUrl(strDocumentUrl), documentHash(documentHash), signerType(signerType), signerHash(signerHash), blsPubKey(blsPubKey), blsSig(blsSig) { };

    void SetNull() {
        nEventId = 0;
        strDocumentUrl = "";
        documentHash = uint256();
        signerType = 0;
        signerHash = uint256();
        blsPubKey = CBLSPublicKey();
        blsSig = CBLSSignature();
    }

    SERIALIZE_METHODS(CTokenGroupDescriptionBetting, obj)
    {
        READWRITE(obj.nVersion);
        READWRITE(obj.nEventId);
        READWRITE(obj.strDocumentUrl);
        READWRITE(obj.documentHash);
        READWRITE(obj.signerType);
        READWRITE(obj.signerHash);
        READWRITE(obj.blsPubKey);
        READWRITE(obj.blsSig);
    }
    void ToJson(UniValue& obj) const;
    void WriteHashable(CHashWriter& ss) const {
        ss << nVersion;
        ss << nEventId;
        ss << strDocumentUrl;
        ss << documentHash;
        ss << signerType;
        ss << signerHash;
        ss << blsPubKey;
        ss << blsSig;
    }

    bool operator==(const CTokenGroupDescriptionBetting &c)
    {
        return (nVersion == c.nVersion &&
                nEventId == c.nEventId &&
                strDocumentUrl == c.strDocumentUrl &&
                documentHash == c.documentHash &&
                signerType == c.signerType &&
                signerHash == c.signerHash &&
                blsPubKey == c.blsPubKey &&
                blsSig == c.blsSig);
    }
};

typedef boost::variant<CTokenGroupDescriptionRegular, CTokenGroupDescriptionMGT, CTokenGroupDescriptionNFT, CTokenGroupDescriptionBetting> CTokenGroupDescriptionVariant;
class tgdesc_to_json : public boost::static_visitor<UniValue>
{
private:
    const bool fFull = false;
public:
    tgdesc_to_json(const bool& fFull = false) : fFull(fFull) {}

    UniValue operator()(CTokenGroupDescriptionRegular& tgDesc) const
    {
        UniValue obj(UniValue::VOBJ);
        tgDesc.ToJson(obj);
        return obj;
    }
    UniValue operator()(CTokenGroupDescriptionMGT& tgDesc) const
    {
        UniValue obj(UniValue::VOBJ);
        tgDesc.ToJson(obj);
        return obj;
    }
    UniValue operator()(CTokenGroupDescriptionNFT& tgDesc) const
    {
        UniValue obj(UniValue::VOBJ);
        tgDesc.ToJson(obj, fFull);
        return obj;
    }
    UniValue operator()(CTokenGroupDescriptionBetting& tgDesc) const
    {
        UniValue obj(UniValue::VOBJ);
        tgDesc.ToJson(obj);
        return obj;
    }
};
inline UniValue tgDescToJson(CTokenGroupDescriptionVariant& tgDesc, const bool& fFull = false) {
    return boost::apply_visitor(tgdesc_to_json(fFull), tgDesc);
}

class tgdesc_get_ticker : public boost::static_visitor<std::string>
{
public:
    std::string operator()(CTokenGroupDescriptionRegular& tgDesc) const {
        return tgDesc.strTicker;
    }
    std::string operator()(CTokenGroupDescriptionMGT& tgDesc) const {
        return tgDesc.strTicker;
    }
    std::string operator()(CTokenGroupDescriptionNFT& tgDesc) const {
        return "";
    }
    std::string operator()(CTokenGroupDescriptionBetting& tgDesc) const {
        return "";
    }
};
inline std::string tgDescGetTicker(CTokenGroupDescriptionVariant& tgDesc) {
    return boost::apply_visitor(tgdesc_get_ticker(), tgDesc);
}

    // ADD_SERIALIZE_METHODS;

class tgdesc_get_name : public boost::static_visitor<std::string>
{
public:
    std::string operator()(CTokenGroupDescriptionRegular& tgDesc) const {
        return tgDesc.strName;
    }
    std::string operator()(CTokenGroupDescriptionMGT& tgDesc) const {
        return tgDesc.strName;
    }
    std::string operator()(CTokenGroupDescriptionNFT& tgDesc) const {
        return tgDesc.strName;
    }
    std::string operator()(CTokenGroupDescriptionBetting& tgDesc) const {
        return strprintf("%d", tgDesc.nEventId);
    }
};
inline std::string tgDescGetName(CTokenGroupDescriptionVariant& tgDesc) {
    return boost::apply_visitor(tgdesc_get_name(), tgDesc);
}
class tgdesc_get_document_url : public boost::static_visitor<std::string>
{
public:
    std::string operator()(CTokenGroupDescriptionRegular& tgDesc) const {
        return tgDesc.strDocumentUrl;
   }
    std::string operator()(CTokenGroupDescriptionMGT& tgDesc) const {
        return tgDesc.strDocumentUrl;
    }
    std::string operator()(CTokenGroupDescriptionNFT& tgDesc) const {
        return tgDesc.strDocumentUrl;
    }
    std::string operator()(CTokenGroupDescriptionBetting& tgDesc) const {
        return tgDesc.strDocumentUrl;
    }
};
inline std::string tgDescGetDocumentURL(CTokenGroupDescriptionVariant& tgDesc) {
    return boost::apply_visitor(tgdesc_get_document_url(), tgDesc);
}

class tgdesc_get_document_hash : public boost::static_visitor<uint256>
{
public:
    uint256 operator()(CTokenGroupDescriptionRegular& tgDesc) const {
        return tgDesc.documentHash;
    }
    uint256 operator()(CTokenGroupDescriptionMGT& tgDesc) const {
        return tgDesc.documentHash;
    }
    uint256 operator()(CTokenGroupDescriptionNFT& tgDesc) const {
        return tgDesc.documentHash;
    }
    uint256 operator()(CTokenGroupDescriptionBetting& tgDesc) const {
        return tgDesc.documentHash;
    }
};
inline uint256 tgDescGetDocumentHash(CTokenGroupDescriptionVariant& tgDesc) {
    return boost::apply_visitor(tgdesc_get_document_hash(), tgDesc);
}

class tgdesc_get_decimal_pos : public boost::static_visitor<uint8_t>
{
public:
    uint8_t operator()(CTokenGroupDescriptionRegular& tgDesc) const {
        return tgDesc.nDecimalPos;
    }
    uint8_t operator()(CTokenGroupDescriptionMGT& tgDesc) const {
        return tgDesc.nDecimalPos;
    }
    uint8_t operator()(CTokenGroupDescriptionNFT& tgDesc) const {
        return 0;
    }
    uint8_t operator()(CTokenGroupDescriptionBetting& tgDesc) const {
        return 8;
    }
 };
inline uint8_t tgDescGetDecimalPos(CTokenGroupDescriptionVariant& tgDesc) {
    return boost::apply_visitor(tgdesc_get_decimal_pos(), tgDesc);
}

// Tokens with no fractional quantities have nDecimalPos=0
// Wagerr has has decimalPos=8 (1 WAGERR is 100000000 satoshi)
// Maximum value is 10^16
class tgdesc_get_coin_amount : public boost::static_visitor<CAmount>
{
public:
    CAmount operator()(CTokenGroupDescriptionRegular& tgDesc) const {
        return COINFromDecimalPos(tgDesc.nDecimalPos);
    }
    CAmount operator()(CTokenGroupDescriptionMGT& tgDesc) const {
        return COINFromDecimalPos(tgDesc.nDecimalPos);
    }
    CAmount operator()(CTokenGroupDescriptionNFT& tgDesc) const {
        return COINFromDecimalPos(0);
    }
    CAmount operator()(CTokenGroupDescriptionBetting& tgDesc) const {
        return COINFromDecimalPos(8);
    }
};
inline CAmount tgDescGetCoinAmount(CTokenGroupDescriptionVariant& tgDesc) {
    return boost::apply_visitor(tgdesc_get_coin_amount(), tgDesc);
}

inline std::string GetStringFromChars(const std::vector<unsigned char> chars, const uint32_t maxChars) {
    return std::string(chars.begin(), chars.size() < maxChars ? chars.end() : std::next(chars.begin(), maxChars));
}

bool ParseGroupDescParamsRegular(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionRegular>& tgDesc, bool &confirmed);
bool ParseGroupDescParamsMGT(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionMGT>& tgDesc, bool &stickyMelt, bool &confirmed);
bool ParseGroupDescParamsNFT(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionNFT>& tgDesc, bool &confirmed);
bool ParseGroupDescParamsBetting(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionNFT>& tgDesc, bool &confirmed);

#endif
