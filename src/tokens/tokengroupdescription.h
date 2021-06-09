// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_GROUP_DESCRIPTION_H
#define TOKEN_GROUP_DESCRIPTION_H

#include "amount.h"
#include <bls/bls.h>
#include "script/script.h"
#include <primitives/transaction.h>
#include "uint256.h"

#include <boost/variant.hpp>

class JSONRPCRequest;
class UniValue;

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
        nDecimalPos = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(strTicker);
        READWRITE(strName);
        READWRITE(strDocumentUrl);
        READWRITE(documentHash);
        READWRITE(nDecimalPos);
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

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(strTicker);
        READWRITE(strName);
        READWRITE(strDocumentUrl);
        READWRITE(documentHash);
        READWRITE(nDecimalPos);
        READWRITE(blsPubKey);
    }
    void ToJson(UniValue& obj) const;

    // BLS public key is excluded from hash
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

typedef boost::variant<CTokenGroupDescriptionRegular, CTokenGroupDescriptionMGT> CTokenGroupDescriptionVariant;

class tgdesc_get_ticker : public boost::static_visitor<std::string>
{
public:
    std::string operator()(CTokenGroupDescriptionRegular& tgDesc) const {
        return tgDesc.strTicker;
    }
    std::string operator()(CTokenGroupDescriptionMGT& tgDesc) const {
        return tgDesc.strTicker;
    }
};
inline std::string tgDescGetTicker(CTokenGroupDescriptionVariant& tgDesc) {
    return boost::apply_visitor(tgdesc_get_ticker(), tgDesc);
}

class tgdesc_get_name : public boost::static_visitor<std::string>
{
public:
    std::string operator()(CTokenGroupDescriptionRegular& tgDesc) const {
        return tgDesc.strName;
    }
    std::string operator()(CTokenGroupDescriptionMGT& tgDesc) const {
        return tgDesc.strName;
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
};
inline uint8_t tgDescGetDecimalPos(CTokenGroupDescriptionVariant& tgDesc) {
    return boost::apply_visitor(tgdesc_get_decimal_pos(), tgDesc);
}

// Tokens with no fractional quantities have nDecimalPos=0
// Bytz has has decimalpos=8 (1 BYTZ is 100000000 satoshi)
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
};
inline CAmount tgDescGetCoinAmount(CTokenGroupDescriptionVariant& tgDesc) {
    return boost::apply_visitor(tgdesc_get_coin_amount(), tgDesc);
}

inline std::string GetStringFromChars(const std::vector<unsigned char> chars, const uint32_t maxChars) {
    return std::string(chars.begin(), chars.size() < maxChars ? chars.end() : std::next(chars.begin(), maxChars));
}

bool ParseGroupDescParamsRegular(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionRegular>& tgDesc, bool &confirmed);
bool ParseGroupDescParamsMGT(const JSONRPCRequest& request, unsigned int &curparam, std::shared_ptr<CTokenGroupDescriptionMGT>& tgDesc, bool &stickyMelt, bool &confirmed);

#endif
