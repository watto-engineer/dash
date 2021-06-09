// Copyright (c) 2017-2018 The PIVX developers
// Copyright (c) 2018-2019 The Ion developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef POS_STAKEINPUT_H
#define POS_STAKEINPUT_H

#include "chain.h"
#include "streams.h"
#include "uint256.h"

#include "libzerocoin/CoinSpend.h"

class CKeyStore;
class CWallet;
class CWalletTx;

class CStakeInput
{
protected:
    CBlockIndex* pindexFrom = nullptr;

public:
    virtual ~CStakeInput(){};
    virtual CBlockIndex* GetIndexFrom() = 0;
    virtual bool CreateTxIn(std::shared_ptr<CWallet> pwallet, CTxIn& txIn, uint256 hashTxOut = uint256()) = 0;
    virtual bool GetTxFrom(CTransactionRef& tx) = 0;
    virtual bool GetScriptPubKeyKernel(CScript& scriptPubKeyKernel) const = 0;
    virtual CAmount GetValue() const = 0;
    virtual bool CreateTxOuts(std::shared_ptr<CWallet> pwallet, std::vector<CTxOut>& vout, CAmount nTotal) = 0;
    virtual bool GetModifier(uint64_t& nStakeModifier) = 0;
    virtual bool IsZBYTZ() = 0;
    virtual CDataStream GetUniqueness() = 0;
    virtual uint256 GetSerialHash() const = 0;

    virtual uint64_t getStakeModifierHeight() const {
        return 0;
    }
};


// zStake can take two forms
// 1) the stake candidate, which is a zcmint that is attempted to be staked
// 2) a staked zbytz, which is a zcspend that has successfully staked
class CZStake : public CStakeInput
{
private:
    uint32_t nChecksum;
    bool fMint;
    libzerocoin::CoinDenomination denom;
    uint256 hashSerial;

public:
    explicit CZStake(libzerocoin::CoinDenomination denom, const uint256& hashSerial)
    {
        this->denom = denom;
        this->hashSerial = hashSerial;
        fMint = true;
    }

    explicit CZStake(const libzerocoin::CoinSpend& spend);

    CBlockIndex* GetIndexFrom() override;
    bool GetTxFrom(CTransactionRef& tx) override;
    bool GetScriptPubKeyKernel(CScript& scriptPubKeyKernel) const override;
    CAmount GetValue() const override;
    bool GetModifier(uint64_t& nStakeModifier) override;
    CDataStream GetUniqueness() override;
    bool CreateTxIn(std::shared_ptr<CWallet> pwallet, CTxIn& txIn, uint256 hashTxOut = uint256()) override;
    bool CreateTxOuts(std::shared_ptr<CWallet> pwallet, std::vector<CTxOut>& vout, CAmount nTotal) override;
//    bool MarkSpent(CWallet* pwallet, const uint256& txid);
    bool IsZBYTZ() override { return true; }
    uint256 GetSerialHash() const override { return hashSerial; }
    int GetChecksumHeightFromMint();
    int GetChecksumHeightFromSpend();
    uint32_t GetChecksum();
};

class CStake : public CStakeInput
{
private:
    CTransactionRef txFrom;
    unsigned int nPosition;

    // cached data
    uint64_t nStakeModifier = 0;
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;
public:
    CStake(){}

    bool SetInput(CTransactionRef txPrev, unsigned int n);

    CBlockIndex* GetIndexFrom() override;
    bool GetTxFrom(CTransactionRef& tx) override;
    bool GetScriptPubKeyKernel(CScript& scriptPubKeyKernel) const override;
    CAmount GetValue() const override;
    bool GetModifier(uint64_t& nStakeModifier) override;
    CDataStream GetUniqueness() override;
    bool CreateTxIn(std::shared_ptr<CWallet> pwallet, CTxIn& txIn, uint256 hashTxOut = uint256()) override;
    bool CreateTxOuts(std::shared_ptr<CWallet> pwallet, std::vector<CTxOut>& vout, CAmount nTotal) override;
    bool IsZBYTZ() override { return false; }
    uint256 GetSerialHash() const override { return uint256(); }

    uint64_t getStakeModifierHeight() const override { return nStakeModifierHeight; }
};

bool IsValidStakeInput(const CTxOut& txOut);

#endif //POS_STAKEINPUT_H
