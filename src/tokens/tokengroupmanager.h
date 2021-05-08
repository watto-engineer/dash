// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_GROUP_MANAGER_H
#define TOKEN_GROUP_MANAGER_H

#include "consensus/tokengroups.h"
#include "tokens/tokengroupconfiguration.h"
#include "wallet/wallet.h"

#include <unordered_map>

class CBlockIndex;
class CTokenGroupManager;
class UniValue;

extern std::shared_ptr<CTokenGroupManager> tokenGroupManager;

// TokenGroup Class
// Keeps track of all of the token groups
class CTokenGroupManager
{
private:
    std::map<CTokenGroupID, CTokenGroupCreation> mapTokenGroups;
    std::unique_ptr<CTokenGroupCreation> tgMGTCreation;
    std::unique_ptr<CTokenGroupCreation> tgGVTCreation;

public:
    CTokenGroupManager();

    std::vector<std::function<void (CTokenGroupCreation&)>> vTokenGroupFilters;

    bool AddTokenGroups(const std::vector<CTokenGroupCreation>& newTokenGroups);
    bool RemoveTokenGroup(CTransaction tx, CTokenGroupID &toRemoveTokenGroupID);
    void ResetTokenGroups();

    bool GetTokenGroupCreation(const CTokenGroupID& tgID, CTokenGroupCreation& tgCreation);
    std::string GetTokenGroupNameByID(CTokenGroupID tokenGroupId);
    std::string GetTokenGroupTickerByID(CTokenGroupID tokenGroupId);
    bool GetTokenGroupIdByTicker(std::string strTicker, CTokenGroupID &tokenGroupID);
    bool GetTokenGroupIdByName(std::string strName, CTokenGroupID &tokenGroupID);
    std::map<CTokenGroupID, CTokenGroupCreation> GetMapTokenGroups() { return mapTokenGroups; };

    bool StoreManagementTokenGroups(CTokenGroupCreation tokenGroupCreation);
    void ClearManagementTokenGroups();

    bool MatchesMGT(CTokenGroupID tgID);
    bool MatchesGVT(CTokenGroupID tgID);

    CTokenGroupID GetMGTID() { return tgMGTCreation->tokenGroupInfo.associatedGroup; };
    CTokenGroupID GetGVTID() { return tgGVTCreation->tokenGroupInfo.associatedGroup; };

    bool MGTTokensCreated() { return tgMGTCreation ? true : false; };
    bool GVTTokensCreated() { return tgGVTCreation ? true : false; };

    bool ManagementTokensCreated(int nHeight);

    uint16_t GetTokensInBlock(const CBlock& block, const CTokenGroupID& tgId);
    unsigned int GetTokenTxStats(const CTransactionRef &tx, const CCoinsViewCache& view, const CTokenGroupID &tgId, uint16_t &nTokenCount, CAmount &nTokenMint);

    bool TokenMoneyRange(CAmount nValueOut);
    CAmount AmountFromTokenValue(const UniValue& value, const CTokenGroupID& tgID);
    std::string TokenValueFromAmount(const CAmount& amount, const CTokenGroupID& tgID);

    bool CheckFees(const CTransaction &tx, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, CValidationState& state, const CBlockIndex* pindex);
};

#endif
