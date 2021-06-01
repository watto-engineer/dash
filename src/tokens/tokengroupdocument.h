// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_GROUP_DOCUMENT
#define TOKEN_GROUP_DOCUMENT

#include <key.h>

class UniValue;

class CTokenGroupDocument
{
public:
    static const uint16_t CURRENT_VERSION = 1;

public:
    uint16_t nVersion{CURRENT_VERSION};

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
    // data
    std::string strTicker;
    std::string strName;
    std::string strChain;
    std::string strSummary;
    std::string strDescription;
    std::string strCreator;
    std::string strContactURL;
    std::string strContactEmail;

    std::vector<unsigned char> vchSig;

    CTokenGroupDocument() {
        SetNull();
    };
    CTokenGroupDocument(std::string strTicker, std::string strName, std::string strChain, std::string strSummary,
        std::string strDescription, std::string strCreator, std::string strContactURL, std::string strContactEmail) :
        strTicker(strTicker), strName(strName), strChain(strChain), strSummary(strSummary), strDescription(strDescription),
        strCreator(strCreator), strContactURL(strContactURL), strContactEmail(strContactEmail) {};

    void SetNull() {
        strTicker = "";
        strName = "";
        strChain = "";
        strSummary = "";
        strDescription = "";
        strCreator = "";
        strContactURL = "";
        strContactEmail = "";
        vchSig.clear();
    }

    /**
     * GetHash returns the double-sha256 hash of the serialized item.
     */
    uint256 GetHash() const;

    /**
     * GetSignatureHash returns the hash of the serialized item
     * without the signature included. The intent of this method is to get the
     * hash to be signed.
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

    // FUNCTIONS FOR DEALING WITH DATA STRING

    std::string GetDataAsHexString() const;
    std::string GetDataAsPlainString() const;

    // SERIALIZER

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(nVersion);
        }
        READWRITE(strTicker);
        READWRITE(strName);
        READWRITE(strChain);
        READWRITE(strSummary);
        READWRITE(strDescription);
        READWRITE(strCreator);
        READWRITE(strContactURL);
        READWRITE(strContactEmail);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(vchSig);
        }
    }

    void ToJson(UniValue& obj) const;

    // FUNCTIONS FOR DEALING WITH DATA STRING
    void LoadData();
    void GetData(UniValue& objResult);
};

#endif