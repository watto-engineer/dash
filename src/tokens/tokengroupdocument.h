// Copyright (c) 2021 The Bytz Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_GROUP_DOCUMENT
#define TOKEN_GROUP_DOCUMENT

#include <key.h>
#include <univalue.h>

class CTokenGroupDocument
{
public:
    static const uint16_t CURRENT_VERSION = 1;

private:
    uint16_t nVersion{CURRENT_VERSION};
    int nSpecialTxType;

    // data
    UniValue data;
    std::vector<unsigned char> vchData;

    // signature data
    std::vector<unsigned char> vchSig;

    /// Failed to parse object data
    bool fUnparsable;
    bool fJsonLoaded;
    bool fRawLoaded;
    bool fParsed;
    bool fValidated;

public:
    CTokenGroupDocument() : nVersion(CURRENT_VERSION), nSpecialTxType(), vchData(), vchSig(), fUnparsable(false), fJsonLoaded(false), fRawLoaded(false), fParsed(false), fValidated(false) {
        data = UniValue(UniValue::VOBJ);
    };

    CTokenGroupDocument(const std::vector<unsigned char>& vchDataIn) :
            nVersion(CURRENT_VERSION), nSpecialTxType(nSpecialTxType), vchData(vchDataIn), vchSig(), fUnparsable(false), fJsonLoaded(false), fRawLoaded(true), fParsed(false), fValidated(false) {
        fJsonLoaded = LoadJSONData();
        if (!fJsonLoaded) {
            fUnparsable = true;
        }
        if (!ParseATPParams()) {
            fUnparsable = true;
            return;
        }
        fParsed = true;
        vchData = GetRawDataFromJson();
        fRawLoaded = true;

        if (!ValidateData()) {
            return;
        }
        fValidated = true;
    };

    CTokenGroupDocument(const UniValue& data) :
            nVersion(CURRENT_VERSION), nSpecialTxType(), data(data), vchData(), vchSig(), fUnparsable(false), fJsonLoaded(true), fRawLoaded(false), fParsed(false) {
        if (!ParseATPParams()) {
            fUnparsable = true;
            return;
        }
        fParsed = true;
        vchData = GetRawDataFromJson();
        fRawLoaded = true;

        if (!ValidateData()) {
            return;
        }
        fValidated = true;
    };

    void SetNull() {
        data = UniValue(UniValue::VOBJ);
        vchData.clear();
        vchSig.clear();
        fUnparsable = false;
        nSpecialTxType = 0;
    }

    /**
     * ValidateData will ensure that the json data matches the required format
     */
    bool ValidateData() const;

    /**
     * GetSignatureHash returns the hash of the data's text representation.
     * The intent of this method is to get the hash to be signed.
     */
    uint256 GetSignatureHash() const;

    /**
     * Sign will sign the item with the given key.
     */
    bool Sign(const CKey& key);

    /**
     * CheckSignature will ensure the item signature matches the provided public
     * key hash.
     */
    bool CheckSignature(const CKeyID& pubKeyId) const;

    /**
     * GetSignerKeyID is used to recover the address of the key used to
     * sign this item.
     */
    bool GetSignerKeyID(CKeyID& retKeyidSporkSigner) const;

    /**
     * SetSignature is used to set the signature data to a given string
     */
    void SetSignature(const std::string& strSignature);

    /**
     * GetSignature is used to get the signature data as a string
     */
    std::string GetSignature();

    // FUNCTIONS FOR DEALING WITH DATA STRING

    std::string GetDataAsHexString() const;
    std::string GetDataAsPlainString() const;
    std::vector<unsigned char> GetRawDataFromJson() const;

    bool LoadJSONData();
    bool GetJSONFromRawData(UniValue& objResult);

    bool ParseATPParams();

    void ToJson(UniValue& obj) const;
};

#endif