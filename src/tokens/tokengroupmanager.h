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
    std::unique_ptr<CTokenGroupCreation> tgMagicCreation;
    std::unique_ptr<CTokenGroupCreation> tgDarkMatterCreation;
    std::unique_ptr<CTokenGroupCreation> tgAtomCreation;
    std::unique_ptr<CTokenGroupCreation> tgElectronCreation;

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

    bool MatchesMagic(CTokenGroupID tgID);
    bool MatchesDarkMatter(CTokenGroupID tgID);
    bool MatchesAtom(CTokenGroupID tgID);
    bool MatchesElectron(CTokenGroupID tgID);

    CTokenGroupID GetMagicID() { return tgMagicCreation->tokenGroupInfo.associatedGroup; };
    CTokenGroupID GetDarkMatterID() { return tgDarkMatterCreation->tokenGroupInfo.associatedGroup; };
    CTokenGroupID GetAtomID() { return tgAtomCreation->tokenGroupInfo.associatedGroup; };
    CTokenGroupID GetElectronID() { return tgElectronCreation->tokenGroupInfo.associatedGroup; };

    bool MagicTokensCreated() { return tgMagicCreation ? true : false; };
    bool DarkMatterTokensCreated() { return tgDarkMatterCreation ? true : false; };
    bool AtomTokensCreated() { return tgAtomCreation ? true : false; };
    bool ElectronTokensCreated() { return tgElectronCreation ? true : false; };

    bool ManagementTokensCreated(int nHeight);

    uint16_t GetXDMInBlock(const CBlock& block);
    unsigned int GetTokenTxStats(const CTransactionRef &tx, const CCoinsViewCache& view, const CTokenGroupID &tgId, uint16_t &nTokenCount, CAmount &nTokenMint);

    bool TokenMoneyRange(CAmount nValueOut);
    CAmount AmountFromTokenValue(const UniValue& value, const CTokenGroupID& tgID);
    std::string TokenValueFromAmount(const CAmount& amount, const CTokenGroupID& tgID);

    bool GetXDMFee(const uint32_t& nXDMTransactions, CAmount& fee);
    bool GetXDMFee(const CBlockIndex* pindex, CAmount& fee);

    bool CheckXDMFees(const CTransaction &tx, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, CValidationState& state, CBlockIndex* pindex, CAmount& nXDMFees);
};

#endif
