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

class UniValue;

static CAmount COINFromDecimalPos(const uint8_t& nDecimalPos) {
    uint8_t n = nDecimalPos <= 16 ? nDecimalPos : 0;
    static CAmount pow10[17] = {
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000,
        100000000, 1000000000, 10000000000, 100000000000, 1000000000000, 10000000000000, 100000000000000, 1000000000000000, 10000000000000000
    };

    return pow10[n];
}

class CTokenGroupDescriptionBase
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION};

    // Token ticker name
    std::string strTicker;

    // Token name
    std::string strName;

    // Extended token description document URL
    std::string strDocumentUrl;

    uint256 documentHash;

    CTokenGroupDescriptionBase() {
        SetNull();
    };
    CTokenGroupDescriptionBase(std::string strTicker, std::string strName, std::string strDocumentUrl, uint256 documentHash) :
        strTicker(strTicker), strName(strName), strDocumentUrl(strDocumentUrl), documentHash(documentHash) {};

    void SetNull() {
        strTicker = "";
        strName = "";
        strDocumentUrl = "";
        documentHash = uint256();
    }

private:
    inline std::string GetStringFromChars(const std::vector<unsigned char> chars, const uint32_t maxChars) const {
        return std::string(chars.begin(), chars.size() < maxChars ? chars.end() : std::next(chars.begin(), maxChars));
    }

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nVersion);
        READWRITE(strTicker);
        READWRITE(strName);
        READWRITE(strDocumentUrl);
        READWRITE(documentHash);
    }
    void ToJson(UniValue& obj) const;
    void WriteHashable(CHashWriter& ss) const {
        ss << nVersion;
        ss << strTicker;
        ss << strName;
        ss << strDocumentUrl;
        ss << documentHash;
    }

    bool operator==(const CTokenGroupDescriptionBase &c)
    {
        return (strTicker == c.strTicker && strName == c.strName && strDocumentUrl == c.strDocumentUrl && documentHash == c.documentHash);
    }
};

class CTokenGroupDescriptionRegular : public CTokenGroupDescriptionBase
{
public:
    static const int SPECIALTX_TYPE = TRANSACTION_GROUP_CREATION_REGULAR;

    // Decimal position to translate between token value and amount
    uint8_t nDecimalPos;

    CTokenGroupDescriptionRegular() {
        SetNull();
    };
    CTokenGroupDescriptionRegular(std::string strTicker, std::string strName, uint8_t nDecimalPos, std::string strDocumentUrl, uint256 documentHash) :
        CTokenGroupDescriptionBase(strTicker, strName, strDocumentUrl, documentHash), nDecimalPos(nDecimalPos) { };

    CTokenGroupDescriptionRegular(CTokenGroupDescriptionBase tgDescBase) :
        CTokenGroupDescriptionBase(tgDescBase), nDecimalPos(0) { };

    void SetNull() {
        CTokenGroupDescriptionBase::SetNull();
        nDecimalPos = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITEAS(CTokenGroupDescriptionBase, *this);
        READWRITE(nDecimalPos);
    }
    void ToJson(UniValue& obj) const;
    void WriteHashable(CHashWriter& ss) const {
        CTokenGroupDescriptionBase::WriteHashable(ss);
        ss << nDecimalPos;
    }

    bool operator==(const CTokenGroupDescriptionRegular &c)
    {
        return (strTicker == c.strTicker && strName == c.strName && nDecimalPos == c.nDecimalPos && strDocumentUrl == c.strDocumentUrl && documentHash == c.documentHash);
    }
};

class CTokenGroupDescriptionMGT : public CTokenGroupDescriptionBase
{
public:
    static const int SPECIALTX_TYPE = TRANSACTION_GROUP_CREATION_MGT;

    // Decimal position to translate between token value and amount
    uint8_t nDecimalPos;

    // BLS Public Key that enables signing
    CBLSPublicKey blsPubKey;

    CTokenGroupDescriptionMGT() {
        SetNull();
    };
    CTokenGroupDescriptionMGT(std::string strTicker, std::string strName, uint8_t nDecimalPos, std::string strDocumentUrl, uint256 documentHash, CBLSPublicKey blsPubKey) :
        CTokenGroupDescriptionBase(strTicker, strName, strDocumentUrl, documentHash), nDecimalPos(nDecimalPos), blsPubKey(blsPubKey) { };

    CTokenGroupDescriptionMGT(CTokenGroupDescriptionBase tgDescBase) :
        CTokenGroupDescriptionBase(tgDescBase), nDecimalPos(0) {
        blsPubKey = CBLSPublicKey();
    };

    void SetNull() {
        CTokenGroupDescriptionBase::SetNull();
        nDecimalPos = 0;
        blsPubKey = CBLSPublicKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITEAS(CTokenGroupDescriptionBase, *this);
        READWRITE(nDecimalPos);
        READWRITE(blsPubKey);
    }
    void ToJson(UniValue& obj) const;

    // BLS public key is excluded from hash
    void WriteHashable(CHashWriter& ss) const {
        CTokenGroupDescriptionBase::WriteHashable(ss);
        ss << nDecimalPos;
    }

    bool operator==(const CTokenGroupDescriptionMGT &c)
    {
        return (strTicker == c.strTicker && strName == c.strName && strDocumentUrl == c.strDocumentUrl &&
            documentHash == c.documentHash && nDecimalPos == c.nDecimalPos && blsPubKey == c.blsPubKey);
    }
};

// Tokens with no fractional quantities have nDecimalPos=0
// Bytz has has decimalpos=8 (1 BYTZ is 100000000 satoshi)
// Maximum value is 10^16
template <typename TokenGroupDescription>
CAmount GetCoinAmount(const TokenGroupDescription* tgDesc) {
    return COINFromDecimalPos(tgDesc->nDecimalPos);
}
template CAmount GetCoinAmount(const CTokenGroupDescriptionRegular* tgDesc);
template CAmount GetCoinAmount(const CTokenGroupDescriptionMGT* tgDesc);

#endif
