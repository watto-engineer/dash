// Copyright (c) 2017-2018 The PIVX developers
// Copyright (c) 2018-2019 The Ion developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "arith_uint256.h"
#include "chain.h"
#include "kernel.h"
#include "stakeinput.h"
#include "validation.h"
#include "wallet/wallet.h"

typedef std::vector<unsigned char> valtype;

//!Bytz Stake
bool CStake::SetInput(CTransactionRef txPrev, unsigned int n)
{
    this->txFrom = txPrev;
    this->nPosition = n;
    return true;
}

bool CStake::GetTxFrom(CTransactionRef& tx)
{
    tx = txFrom;
    return true;
}

bool CStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
    txIn = CTxIn(txFrom->GetHash(), nPosition);
    return true;
}

CAmount CStake::GetValue()
{
    return txFrom->vout[nPosition].nValue;
}

bool CStake::CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    CScript scriptPubKeyKernel = txFrom->vout[nPosition].scriptPubKey;
    if (!Solver(scriptPubKeyKernel, whichType, vSolutions)) {
        LogPrintf("CreateCoinStake : failed to parse kernel\n");
        return false;
    }

    if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH)
        return false; // only support pay to public key and pay to address

    CScript scriptPubKey;
    if (whichType == TX_PUBKEYHASH) // pay to address type
    {
        //convert to pay to public key type
        CKey key;
        CKeyID keyID = CKeyID(uint160(vSolutions[0]));
        if (!pwallet->GetKey(keyID, key))
            return false;

        scriptPubKey << ToByteVector(key.GetPubKey()) << OP_CHECKSIG;
    } else
        scriptPubKey = scriptPubKeyKernel;

    vout.emplace_back(CTxOut(0, scriptPubKey));

    // Calculate if we need to split the output
    if (nTotal / 2 > (CAmount)(2000 * COIN))
        vout.emplace_back(CTxOut(0, scriptPubKey));

    return true;
}

bool CStake::GetModifier(uint64_t& nStakeModifier)
{
    if (this->nStakeModifier == 0) {
        // look for the modifier
        GetIndexFrom();
        if (!pindexFrom)
            return error("%s: failed to get index from", __func__);
        // TODO: This method must be removed from here in the short terms.. it's a call to an static method in kernel.cpp when this class method is only called from kernel.cpp, no comments..
        if (pindexFrom->nHeight >= Params().GetConsensus().DGWStartHeight) {
            if (!GetKernelStakeModifier(pindexFrom->GetBlockHash(), this->nStakeModifier, this->nStakeModifierHeight, this->nStakeModifierTime, false))
                return error("CheckStakeKernelHash(): failed to get kernel stake modifier");
        } else {
            if (!GetKernelStakeModifierPreDGW(pindexFrom->GetBlockHash(), this->nStakeModifier, this->nStakeModifierHeight, this->nStakeModifierTime, false))
                return error("CheckStakeKernelHash(): failed to get kernel stake modifier");
        }
    }
    nStakeModifier = this->nStakeModifier;

    return true;
}

CDataStream CStake::GetUniqueness()
{
    //The unique identifier for a stake is the outpoint
    if (chainActive.Height() >= Params().GetConsensus().DGWStartHeight) {
        CDataStream ss(SER_NETWORK, 0);
        ss << nPosition << txFrom->GetHash();
        return ss;
    } else {
        CDataStream ss(SER_GETHASH, 0);
        return ss;
    }
}

//The block that the UTXO was added to the chain
CBlockIndex* CStake::GetIndexFrom()
{
    if (pindexFrom)
        return pindexFrom;
    uint256 hashBlock;
    CTransactionRef tx;
    if (GetTransaction(txFrom->GetHash(), tx, Params().GetConsensus(), hashBlock, true)) {
        // If the index is in the chain, then set it as the "index from"
        if (mapBlockIndex.count(hashBlock)) {
            CBlockIndex* pindex = mapBlockIndex.at(hashBlock);
            if (chainActive.Contains(pindex))
                pindexFrom = pindex;
        }
    } else {
        LogPrintf("%s : failed to find tx %s\n", __func__, txFrom->GetHash().GetHex());
    }

    return pindexFrom;
}
