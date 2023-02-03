// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tokens/tokendb.h"
#include "tokens/tokengroupmanager.h"
#include "ui_interface.h"
#include "validation.h"

#include <boost/thread.hpp>

std::unique_ptr<CTokenDB> pTokenDB;

CTokenDB::CTokenDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "tokens", nCacheSize, fMemory, fWipe) {}

bool CTokenDB::WriteTokenGroupsBatch(const std::vector<CTokenGroupCreation>& tokenGroups) {
    CDBBatch batch(*this);
    for (std::vector<CTokenGroupCreation>::const_iterator it = tokenGroups.begin(); it != tokenGroups.end(); it++){
        batch.Write(std::make_pair('c', it->tokenGroupInfo.associatedGroup), *it);
    }
    return WriteBatch(batch);
}

bool CTokenDB::WriteTokenGroup(const CTokenGroupID& tokenGroupID, const CTokenGroupCreation& tokenGroupCreation) {
    return Write(std::make_pair('c', tokenGroupID), tokenGroupCreation);
}

bool CTokenDB::ReadTokenGroup(const CTokenGroupID& tokenGroupID, CTokenGroupCreation& tokenGroupCreation) {
    return Read(std::make_pair('c', tokenGroupID), tokenGroupCreation);
}

bool CTokenDB::EraseTokenGroupBatch(const std::vector<CTokenGroupID>& newTokenGroupIDs) {
    CDBBatch batch(*this);
    for (std::vector<CTokenGroupID>::const_iterator it = newTokenGroupIDs.begin(); it != newTokenGroupIDs.end(); it++){
        batch.Erase(std::make_pair('c', *it));
    }
    return WriteBatch(batch);

}

bool CTokenDB::EraseTokenGroup(const CTokenGroupID& tokenGroupID) {
    return Erase(std::make_pair('c', tokenGroupID));
}

bool CTokenDB::DropTokenGroups(std::string& strError) {
    std::vector<CTokenGroupCreation> vTokenGroups;
    std::vector<CTokenGroupID> vTokenGroupIDs;
    FindTokenGroups(vTokenGroups, strError);

    for (auto tokenGroup : vTokenGroups) {
        vTokenGroupIDs.emplace_back(tokenGroup.tokenGroupInfo.associatedGroup);
        EraseTokenGroupBatch(vTokenGroupIDs);
    }

    return true;
}

bool CTokenDB::FindTokenGroups(std::vector<CTokenGroupCreation>& vTokenGroups, std::string& strError) {
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->SeekToFirst();

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, CTokenGroupID> key;
        if (pcursor->GetKey(key) && key.first == 'c') {
            CTokenGroupCreation tokenGroupCreation;
            if (pcursor->GetValue(tokenGroupCreation)) {
                vTokenGroups.push_back(tokenGroupCreation);
            } else {
                strError = "Failed to read token data from database";
                return error(strError.c_str());
            }
        }
        pcursor->Next();
    }
    return true;
}

bool CTokenDB::LoadTokensFromDB(std::string& strError) {
    tokenGroupManager.get()->ResetTokenGroups();

    std::vector<CTokenGroupCreation> vTokenGroups;
    FindTokenGroups(vTokenGroups, strError);

    tokenGroupManager->AddTokenGroups(vTokenGroups);
    return true;
}

bool VerifyTokenDB(std::string &strError) {
    std::vector<CTokenGroupCreation> vTokenGroups;
    if (!pTokenDB->FindTokenGroups(vTokenGroups, strError)) {
        return error(strError.c_str());
    }
    if (fHavePruned) {
        LogPrintf("The block database has been pruned: lowering token database validation level\n");
    }
    for (auto tgCreation : vTokenGroups) {
        uint256 hash_block;
        CTransactionRef tx;
        uint256 txHash = tgCreation.creationTransaction->GetHash();

        LOCK(cs_main);
        auto pindex = LookupBlockIndex(tgCreation.creationBlockHash);
        if (!pindex) {
            strError = "Cannot find token creation transaction's block";
            return error(strError.c_str());
        }
        if (!::ChainActive().Contains(pindex)) {
            strError = "Token creation not found in the current chain";
            return error(strError.c_str());
        }
        if (fHavePruned && !(pindex->nStatus & BLOCK_HAVE_DATA) && pindex->nTx > 0) {
            // Block is in the index, but it's data has been pruned
            continue;
        }
        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
            strError = "Cannot locate token creation transaction's block";
            return error(strError.c_str());
        }
        for (auto& tx : block.vtx) {
            if (tx->GetHash() != txHash) {
                continue;
            }
            // Found creation transaction
            CTokenGroupCreation tgCreation2;
            if (!CreateTokenGroup(tx, block.GetHash(), tgCreation2)) {
                strError = "Cannot recreate token configuration transaction";
                return error(strError.c_str());
            }
            if (!(tgCreation == tgCreation2)) {
                strError = "Cannot verify token configuration transaction";
                return error(strError.c_str());
            }
        }
    }
    return true;
}

bool ReindexTokenDB(std::string &strError) {
    if (!pTokenDB->DropTokenGroups(strError)) {
        strError = "Failed to reset token database";
        return false;
    }
    tokenGroupManager->ResetTokenGroups();

    uiInterface.ShowProgress("Reindexing token database...", 0, false);

    CBlockIndex* pindex = ::ChainActive()[Params().GetConsensus().ATPStartHeight];
    std::vector<CTokenGroupCreation> vTokenGroups;
    while (pindex) {
        uiInterface.ShowProgress("Reindexing token database...", std::max(1, std::min(99, (int)((double)(pindex->nHeight - Params().GetConsensus().ATPStartHeight) / (double)(::ChainActive().Height() - Params().GetConsensus().ATPStartHeight) * 100))), false);

        if (pindex->nHeight % 10000 == 0)
            LogPrintf("Reindexing token database: block %d...\n", pindex->nHeight);

        CBlock block;
        if (!ReadBlockFromDisk(block, pindex, Params().GetConsensus())) {
            strError = "Reindexing token database failed";
            return false;
        }

        for (const CTransactionRef& ptx : block.vtx) {
            if (!ptx->IsCoinBase() && !ptx->HasZerocoinSpendInputs() && IsAnyOutputGroupedCreation(*ptx)) {
                CTokenGroupCreation tokenGroupCreation;
                if (CreateTokenGroup(ptx, block.GetHash(), tokenGroupCreation)) {
                    vTokenGroups.push_back(tokenGroupCreation);
                }
            }
        }

        if (!vTokenGroups.empty() && !pTokenDB->WriteTokenGroupsBatch(vTokenGroups)) {
            strError = "Error writing token database to disk";
            return false;
        }
        tokenGroupManager->AddTokenGroups(vTokenGroups);
        vTokenGroups.clear();

        pindex = ::ChainActive().Next(pindex);
    }

    uiInterface.ShowProgress("", 100, false);

    return true;
}
