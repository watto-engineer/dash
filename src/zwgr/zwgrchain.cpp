// Copyright (c) 2018-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zwgr/zwgrchain.h"

#include "consensus/validation.h"
#include "pos/checks.h"
#include "zwgr/zwgrmodule.h"
#include "zwgr/zerocoindb.h"
//#include "invalid.h"
#include "validation.h"
#include "txdb.h"
#include "ui_interface.h"

// 6 comes from OPCODE (1) + vch.size() (1) + BIGNUM size (4)
#define SCRIPT_OFFSET 6
// For Script size (BIGNUM/Uint256 size)
#define BIGNUM_SIZE   4

/*
bool BlockToMintValueVector(const CBlock& block, const libzerocoin::CoinDenomination denom, std::vector<CBigNum>& vValues)
{
    for (const CTransactionRef& tx : block.vtx) {
        if(!tx->HasZerocoinMintOutputs())
            continue;

        for (const CTxOut& txOut : tx->vout) {
            if(!txOut.IsZerocoinMint())
                continue;

            CValidationState state;
            libzerocoin::PublicCoin coin(Params().Zerocoin_Params(false));
            if(!TxOutToPublicCoin(txOut, coin, state))
                return false;

            if (coin.getDenomination() != denom)
                continue;

            vValues.push_back(coin.getValue());
        }
    }

    return true;
}
*/

bool BlockToPubcoinList(const CBlock& block, std::list<libzerocoin::PublicCoin>& listPubcoins, bool fFilterInvalid)
{
    for (const CTransactionRef& tx : block.vtx) {
        if(!tx->HasZerocoinMintOutputs())
            continue;

        uint256 txHash = tx->GetHash();
        for (unsigned int i = 0; i < tx->vout.size(); i++) {
            //Filter out mints that use invalid outpoints - edge case: invalid spend with minted change

            const CTxOut txOut = tx->vout[i];
            if(!txOut.IsZerocoinMint())
                continue;

            CValidationState state;
            libzerocoin::PublicCoin pubCoin(Params().Zerocoin_Params(false));
            if(!TxOutToPublicCoin(txOut, pubCoin, state))
                return false;

            listPubcoins.emplace_back(pubCoin);
        }
    }

    return true;
}

//return a list of zerocoin mints contained in a specific block
bool BlockToZerocoinMintList(const CBlock& block, std::list<CZerocoinMint>& vMints, bool fFilterInvalid)
{
    for (const CTransactionRef& tx : block.vtx) {
        if(!tx->HasZerocoinMintOutputs())
            continue;

        uint256 txHash = tx->GetHash();
        for (unsigned int i = 0; i < tx->vout.size(); i++) {
            const CTxOut txOut = tx->vout[i];
            if(!txOut.IsZerocoinMint())
                continue;

            CValidationState state;
            libzerocoin::PublicCoin pubCoin(Params().Zerocoin_Params(false));
            if(!TxOutToPublicCoin(txOut, pubCoin, state))
                return false;

            //version should not actually matter here since it is just a reference to the pubcoin, not to the privcoin
            uint8_t version = 1;
            CZerocoinMint mint = CZerocoinMint(pubCoin.getDenomination(), pubCoin.getValue(), 0, 0, false, version, nullptr);
            mint.SetTxHash(tx->GetHash());
            vMints.push_back(mint);
        }
    }

    return true;
}
/*
void FindMints(std::vector<CMintMeta> vMintsToFind, std::vector<CMintMeta>& vMintsToUpdate, std::vector<CMintMeta>& vMissingMints)
{
    // see which mints are in our public zerocoin database. The mint should be here if it exists, unless
    // something went wrong
    for (CMintMeta meta : vMintsToFind) {
        uint256 txHash;
        if (!zerocoinDB->ReadCoinMint(meta.hashPubcoin, txHash)) {
            vMissingMints.push_back(meta);
            continue;
        }

        // make sure the txhash and block height meta data are correct for this mint
        CTransaction tx;
        uint256 hashBlock;
        if (!GetTransaction(txHash, tx, hashBlock, true)) {
            LogPrintf("%s : cannot find tx %s\n", __func__, txHash.GetHex());
            vMissingMints.push_back(meta);
            continue;
        }

        if (!mapBlockIndex.count(hashBlock)) {
            LogPrintf("%s : cannot find block %s\n", __func__, hashBlock.GetHex());
            vMissingMints.push_back(meta);
            continue;
        }

        //see if this mint is spent
        uint256 hashTxSpend = 0;
        bool fSpent = zerocoinDB->ReadCoinSpend(meta.hashSerial, hashTxSpend);

        //if marked as spent, check that it actually made it into the chain
        CTransaction txSpend;
        uint256 hashBlockSpend;
        if (fSpent && !GetTransaction(hashTxSpend, txSpend, hashBlockSpend, true)) {
            LogPrintf("%s : cannot find spend tx %s\n", __func__, hashTxSpend.GetHex());
            meta.isUsed = false;
            vMintsToUpdate.push_back(meta);
            continue;
        }

        //The mint has been incorrectly labelled as spent in zerocoinDB and needs to be undone
        int nHeightTx = 0;
        uint256 hashSerial = meta.hashSerial;
        uint256 txidSpend;
        if (fSpent && !IsSerialInBlockchain(hashSerial, nHeightTx, txidSpend)) {
            LogPrintf("%s : cannot find block %s. Erasing coinspend from zerocoinDB.\n", __func__, hashBlockSpend.GetHex());
            meta.isUsed = false;
            vMintsToUpdate.push_back(meta);
            continue;
        }

        // is the denomination correct?
        for (auto& out : tx->vout) {
            if (!out.IsZerocoinMint())
                continue;
            libzerocoin::PublicCoin pubcoin(Params().Zerocoin_Params(meta.nVersion < libzerocoin::PrivateCoin::PUBKEY_VERSION));
            CValidationState state;
            TxOutToPublicCoin(out, pubcoin, state);
            if (GetPubCoinHash(pubcoin.getValue()) == meta.hashPubcoin && pubcoin.getDenomination() != meta.denom) {
                LogPrintf("%s: found mismatched denom pubcoinhash = %s\n", __func__, meta.hashPubcoin.GetHex());
                meta.denom = pubcoin.getDenomination();
                vMintsToUpdate.emplace_back(meta);
            }
        }

        // if meta data is correct, then no need to update
        if (meta.txid == txHash && meta.nHeight == mapBlockIndex[hashBlock]->nHeight && meta.isUsed == fSpent)
            continue;

        //mark this mint for update
        meta.txid = txHash;
        meta.nHeight = mapBlockIndex[hashBlock]->nHeight;
        meta.isUsed = fSpent;
        LogPrintf("%s: found updates for pubcoinhash = %s\n", __func__, meta.hashPubcoin.GetHex());

        vMintsToUpdate.push_back(meta);
    }
}

int GetZerocoinStartHeight()
{
    return Params().GetConsensus().nBlockZerocoinV2;
}

bool GetZerocoinMint(const CBigNum& bnPubcoin, uint256& txHash)
{
    txHash = 0;
    return zerocoinDB->ReadCoinMint(bnPubcoin, txHash);
}

bool IsPubcoinInBlockchain(const uint256& hashPubcoin, uint256& txid)
{
    txid = 0;
    return zerocoinDB->ReadCoinMint(hashPubcoin, txid);
}

bool IsSerialKnown(const CBigNum& bnSerial)
{
    uint256 txHash = 0;
    return zerocoinDB->ReadCoinSpend(bnSerial, txHash);
}
*/
bool IsSerialInBlockchain(const CBigNum& bnSerial, int& nHeightTx)
{
    uint256 txHash = uint256();
    // if not in zerocoinDB then its not in the blockchain
    if (!zerocoinDB->ReadCoinSpend(bnSerial, txHash))
        return false;

    return IsTransactionInChain(txHash, nHeightTx);
}

bool IsSerialInBlockchain(const uint256& hashSerial, int& nHeightTx, uint256& txidSpend)
{
    CTransactionRef tx;
    return IsSerialInBlockchain(hashSerial, nHeightTx, txidSpend, tx);
}

bool IsSerialInBlockchain(const uint256& hashSerial, int& nHeightTx, uint256& txidSpend, CTransactionRef tx)
{
    txidSpend = uint256();
    // if not in zerocoinDB then its not in the blockchain
    if (!zerocoinDB->ReadCoinSpend(hashSerial, txidSpend))
        return false;

    return IsTransactionInChain(txidSpend, nHeightTx, tx);
}

std::string ReindexZerocoinDB()
{
    if (!zerocoinDB->WipeCoins("spends") || !zerocoinDB->WipeCoins("mints")) {
        return _("Failed to wipe zerocoinDB");
    }

    uiInterface.ShowProgress(_("Reindexing zerocoin database..."), 0, false);

    CBlockIndex* pindex = chainActive[Params().GetConsensus().nBlockZerocoinV2];
    std::vector<std::pair<libzerocoin::CoinSpend, uint256> > vSpendInfo;
    std::vector<std::pair<libzerocoin::PublicCoin, uint256> > vMintInfo;
    while (pindex) {
        uiInterface.ShowProgress(_("Reindexing zerocoin database..."), std::max(1, std::min(99, (int)((double)(pindex->nHeight - Params().GetConsensus().nBlockZerocoinV2) / (double)(chainActive.Height() - Params().GetConsensus().nBlockZerocoinV2) * 100))), false);

        if (pindex->nHeight % 1000 == 0)
            LogPrintf("Reindexing zerocoin : block %d...\n", pindex->nHeight);

        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
            return _("Reindexing zerocoin failed");
        }

        for (const CTransactionRef& tx : block.vtx) {
            for (unsigned int i = 0; i < tx->vin.size(); i++) {
                if (tx->IsCoinBase())
                    break;

                if (tx->ContainsZerocoins()) {
                    uint256 txid = tx->GetHash();
                    //Record Serials
                    if (tx->HasZerocoinSpendInputs()) {
                        for (auto& in : tx->vin) {
                            bool isPublicSpend = in.IsZerocoinPublicSpend();
                            if (!in.IsZerocoinSpend() && !isPublicSpend)
                                continue;
                            if (isPublicSpend) {
                                libzerocoin::ZerocoinParams* params = Params().Zerocoin_Params(false);
                                PublicCoinSpend publicSpend(params);
                                CValidationState state;
                                if (!ZWGRModule::ParseZerocoinPublicSpend(in, *tx, state, publicSpend)){
                                    return _("Failed to parse public spend");
                                }
                                vSpendInfo.push_back(std::make_pair(publicSpend, txid));
                            } else {
                                libzerocoin::CoinSpend spend = TxInToZerocoinSpend(in);
                                vSpendInfo.push_back(std::make_pair(spend, txid));
                            }
                        }
                    }

                    //Record mints
                    if (tx->HasZerocoinMintOutputs()) {
                        for (auto& out : tx->vout) {
                            if (!out.IsZerocoinMint())
                                continue;

                            CValidationState state;
                            libzerocoin::PublicCoin coin(Params().Zerocoin_Params(pindex->nHeight < Params().GetConsensus().nBlockZerocoinV2));
                            TxOutToPublicCoin(out, coin, state);
                            vMintInfo.push_back(std::make_pair(coin, txid));
                        }
                    }
                }
            }
        }

        // Flush the zerocoinDB to disk every 100 blocks
        if (pindex->nHeight % 100 == 0) {
            if ((!vSpendInfo.empty() && !zerocoinDB->WriteCoinSpendBatch(vSpendInfo)) || (!vMintInfo.empty() && !zerocoinDB->WriteCoinMintBatch(vMintInfo)))
                return _("Error writing zerocoinDB to disk");
            vSpendInfo.clear();
            vMintInfo.clear();
        }

        pindex = chainActive.Next(pindex);
    }
    uiInterface.ShowProgress("", 100, false);

    // Final flush to disk in case any remaining information exists
    if ((!vSpendInfo.empty() && !zerocoinDB->WriteCoinSpendBatch(vSpendInfo)) || (!vMintInfo.empty() && !zerocoinDB->WriteCoinMintBatch(vMintInfo)))
        return _("Error writing zerocoinDB to disk");

    uiInterface.ShowProgress("", 100, false);

    return "";
}

bool RemoveSerialFromDB(const CBigNum& bnSerial)
{
    return zerocoinDB->EraseCoinSpend(bnSerial);
}

libzerocoin::CoinSpend TxInToZerocoinSpend(const CTxIn& txin)
{
    // extract the CoinSpend from the txin
    std::vector<char, zero_after_free_allocator<char> > dataTxIn;
    dataTxIn.insert(dataTxIn.end(), txin.scriptSig.begin() + BIGNUM_SIZE, txin.scriptSig.end());
    CDataStream serializedCoinSpend(dataTxIn, SER_NETWORK, PROTOCOL_VERSION);

    libzerocoin::ZerocoinParams* paramsAccumulator = Params().Zerocoin_Params(chainActive.Height() < Params().GetConsensus().nBlockZerocoinV2);
    libzerocoin::CoinSpend spend(Params().Zerocoin_Params(true), paramsAccumulator, serializedCoinSpend);

    return spend;
}

bool TxOutToPublicCoin(const CTxOut& txout, libzerocoin::PublicCoin& pubCoin, CValidationState& state)
{
    CBigNum publicZerocoin;
    std::vector<unsigned char> vchZeroMint;
    vchZeroMint.insert(vchZeroMint.end(), txout.scriptPubKey.begin() + SCRIPT_OFFSET,
                       txout.scriptPubKey.begin() + txout.scriptPubKey.size());
    publicZerocoin.setvch(vchZeroMint);

    libzerocoin::CoinDenomination denomination = libzerocoin::AmountToZerocoinDenomination(txout.nValue);
    if (denomination == libzerocoin::ZQ_ERROR)
        return state.DoS(100, error("TxOutToPublicCoin : txout.nValue is not correct"));

    libzerocoin::PublicCoin checkPubCoin(Params().Zerocoin_Params(false), publicZerocoin, denomination);
    pubCoin = checkPubCoin;

    return true;
}

//return a list of zerocoin spends contained in a specific block, list may have many denominations
std::list<libzerocoin::CoinDenomination> ZerocoinSpendListFromBlock(const CBlock& block, bool fFilterInvalid)
{
    std::list<libzerocoin::CoinDenomination> vSpends;
    for (const CTransactionRef& tx : block.vtx) {
        if (!tx->HasZerocoinSpendInputs())
            continue;

        for (const CTxIn& txin : tx->vin) {
            bool isPublicSpend = txin.IsZerocoinPublicSpend();
            if (!txin.IsZerocoinSpend() && !isPublicSpend)
                continue;

            libzerocoin::CoinDenomination c = libzerocoin::IntToZerocoinDenomination(txin.nSequence);
            vSpends.push_back(c);
        }
    }
    return vSpends;
}

bool UpdateZWGRSupply(const CBlock& block, CBlockIndex* pindex, bool fJustCheck)
{
    // Only update zWAGERR supply when zerocoin mint amount can change
    if (!(pindex->nVersion > 3 && pindex->nVersion < 7)) return true;

    std::list<CZerocoinMint> listMints;
    bool fFilterInvalid = false;//pindex->nHeight >= Params().Zerocoin_Block_RecalculateAccumulators();
    BlockToZerocoinMintList(block, listMints, fFilterInvalid);
    std::list<libzerocoin::CoinDenomination> listSpends = ZerocoinSpendListFromBlock(block, fFilterInvalid);

    // Initialize zerocoin supply to the supply from previous block
    if (pindex->pprev && pindex->pprev->GetBlockHeader().nVersion > 3) {
        for (auto& denom : libzerocoin::zerocoinDenomList) {
            uint16_t nMints = pindex->pprev->GetZcMints(denom);
            if (nMints != 0) pindex->mapZerocoinSupply[denom] = nMints;
        }
    }

    // Track zerocoin money supply
    CAmount nAmountZerocoinSpent = 0;
    pindex->vMintDenominationsInBlock.clear();
    if (pindex->pprev) {
        std::set<uint256> setAddedToWallet;
        for (auto& m : listMints) {
            libzerocoin::CoinDenomination denom = m.GetDenomination();
            pindex->vMintDenominationsInBlock.push_back(m.GetDenomination());
            pindex->mapZerocoinSupply[denom] = pindex->GetZcMints(denom) + 1;

/*
            //Remove any of our own mints from the mintpool
            if (!fJustCheck && pwalletMain) {
                if (pwalletMain->IsMyMint(m.GetValue())) {
                    pwalletMain->UpdateMint(m.GetValue(), pindex->nHeight, m.GetTxHash(), m.GetDenomination());

                    // Add the transaction to the wallet
                    for (auto& tx : block.vtx) {
                        uint256 txid = tx->GetHash();
                        if (setAddedToWallet.count(txid))
                            continue;
                        if (txid == m.GetTxHash()) {
                            CWalletTx wtx(pwalletMain, tx);
                            wtx->nTimeReceived = block.GetBlockTime();
                            wtx->SetMerkleBranch(block);
                            pwalletMain->AddToWallet(wtx);
                            setAddedToWallet.insert(txid);
                        }
                    }
                }
            }
*/
        }

        for (auto& denom : listSpends) {
            uint16_t nMints = pindex->GetZcMints(denom);

            // zerocoin failsafe
            if (nMints == 0)
                return error("Block contains zerocoins that spend more than are in the available supply to spend");

            pindex->mapZerocoinSupply[denom] = nMints - 1;
            nAmountZerocoinSpent += libzerocoin::ZerocoinDenominationToAmount(denom);

        }
    }

    for (auto& denom : libzerocoin::zerocoinDenomList)
        LogPrint(BCLog::ZEROCOIN, "%s coins for denomination %d pubcoin %s\n", __func__, denom, pindex->GetZcMints(denom));

    return true;
}

void AddWrappedSerialsInflation()
{
    CBlockIndex* pindex = chainActive[Params().GetConsensus().nFakeSerialBlockheightEnd];
    if (pindex->nHeight > chainActive.Height())
        return;

    uiInterface.ShowProgress(_("Adding Wrapped Serials supply..."), 0, false);
    while (true) {
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = std::max(1, std::min(99, (int)((double)(pindex->nHeight - Params().GetConsensus().nFakeSerialBlockheightEnd) * 100 / (chainActive.Height() - Params().GetConsensus().nFakeSerialBlockheightEnd))));
            uiInterface.ShowProgress(_("Adding Wrapped Serials supply..."), percent, false);
        }

        // Add inflated denominations to block index mapSupply
        for (auto denom : libzerocoin::zerocoinDenomList) {
            pindex->mapZerocoinSupply.at(denom) += GetWrapppedSerialInflation(denom);
        }
        // Update current block index to disk
        if (!pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)))
            assert(!"cannot write block index");
        // next block
        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100, false);
}

void RecalculateZWGRMinted()
{
    const Consensus::Params& consensusParams = Params().GetConsensus();
    CBlockIndex *pindex = chainActive[consensusParams.nZerocoinStartHeight];
    uiInterface.ShowProgress(_("Recalculating minted ZWGR..."), 0, false);
    while (true) {
        // Log Message and feedback message every 1000 blocks
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = std::max(1, std::min(99, (int)((double)(pindex->nHeight - consensusParams.nZerocoinStartHeight) * 100 / (chainActive.Height() - consensusParams.nZerocoinStartHeight))));
            uiInterface.ShowProgress(_("Recalculating minted ZWGR..."), percent, false);
        }

        //overwrite possibly wrong vMintsInBlock data
        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, consensusParams))
            assert(!"cannot load block from disk");

        std::list<CZerocoinMint> listMints;
        BlockToZerocoinMintList(block, listMints, true);

        std::vector<libzerocoin::CoinDenomination> vDenomsBefore = pindex->vMintDenominationsInBlock;
        pindex->vMintDenominationsInBlock.clear();
        for (auto mint : listMints)
            pindex->vMintDenominationsInBlock.emplace_back(mint.GetDenomination());

        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100, false);
}

void RecalculateZWGRSpent()
{
    const Consensus::Params& consensusParams = Params().GetConsensus();
    CBlockIndex* pindex = chainActive[consensusParams.nZerocoinStartHeight];
    uiInterface.ShowProgress(_("Recalculating spent ZWGR..."), 0, false);
    while (true) {
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = std::max(1, std::min(99, (int)((double)(pindex->nHeight - consensusParams.nZerocoinStartHeight) * 100 / (chainActive.Height() - consensusParams.nZerocoinStartHeight))));
            uiInterface.ShowProgress(_("Recalculating spent ZWGR..."), percent, false);
        }

        //Rewrite zWGR supply
        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, consensusParams))
            assert(!"cannot load block from disk");

        std::list<libzerocoin::CoinDenomination> listDenomsSpent = ZerocoinSpendListFromBlock(block, true);

        //Reset the supply to previous block
        pindex->mapZerocoinSupply = pindex->pprev->mapZerocoinSupply;

        //Add mints to zWGR supply
        for (auto denom : libzerocoin::zerocoinDenomList) {
            long nDenomAdded = count(pindex->vMintDenominationsInBlock.begin(), pindex->vMintDenominationsInBlock.end(), denom);
            pindex->mapZerocoinSupply.at(denom) += nDenomAdded;
        }

        //Remove spends from zWGR supply
        for (auto denom : listDenomsSpent)
            pindex->mapZerocoinSupply.at(denom)--;

        // Add inflation from Wrapped Serials if block is nFakeSerialBlockheightEnd
        if (pindex->nHeight == consensusParams.nFakeSerialBlockheightEnd + 1)
            for (auto denom : libzerocoin::zerocoinDenomList) {
                pindex->mapZerocoinSupply.at(denom) += GetWrapppedSerialInflation(denom);
            }

        //Rewrite money supply
        if (!pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)))
            assert(!"cannot write block index");

        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100, false);
}

bool RecalculateWGRSupply(int nHeightStart)
{
    if (nHeightStart > chainActive.Height())
        return false;

    CBlockIndex* pindex = chainActive[nHeightStart];
    CAmount nSupplyPrev = pindex->pprev->nMoneySupply;

    uiInterface.ShowProgress(_("Recalculating WGR supply..."), 0, false);
    while (true) {
        if (pindex->nHeight % 1000 == 0) {
            LogPrintf("%s : block %d...\n", __func__, pindex->nHeight);
            int percent = std::max(1, std::min(99, (int)((double)((pindex->nHeight - nHeightStart) * 100) / (chainActive.Height() - nHeightStart))));
            uiInterface.ShowProgress(_("Recalculating WGR supply..."), percent, false);
        }

        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus()))
            assert(!"Recalculating zerocoins upply failed: cannot read block from disk");

        CAmount nValueIn = 0;
        CAmount nValueOut = 0;
        CAmount nValueBurned = 0;
        for (const CTransactionRef& tx : block.vtx) {
            for (unsigned int i = 0; i < tx->vin.size(); i++) {
                if (tx->IsCoinBase())
                    break;

                if (tx->vin[i].IsZerocoinSpend()) {
                    nValueIn += tx->vin[i].nSequence * COIN;
                    continue;
                }

                COutPoint prevout = tx->vin[i].prevout;
                CTransactionRef txPrev;
                uint256 hashBlock;
                if (!GetTransaction(prevout.hash, txPrev, Params().GetConsensus(), hashBlock, true))
                    assert(!"Recalculating zerocoins upply failed: cannot get transaction");
                nValueIn += txPrev->vout[prevout.n].nValue;
            }
            tx->AddVoutValues(nValueOut, nValueBurned);
        }

        // Rewrite money supply
        pindex->nMoneySupply = nSupplyPrev + nValueOut - nValueIn - nValueBurned;
        nSupplyPrev = pindex->nMoneySupply;

        if (!pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)))
            assert(!"cannot write block index");

        if (pindex->nHeight < chainActive.Height())
            pindex = chainActive.Next(pindex);
        else
            break;
    }
    uiInterface.ShowProgress("", 100, false);
    return true;
}
