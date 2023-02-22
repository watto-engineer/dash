// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_TOKEN_MINT_H
#define WAGERR_BET_TOKEN_MINT_H

#include <amount.h>
#include <betting/events.h>
#include <consensus/tokengroups.h>
#include <primitives/transaction.h>
#include <tokens/groups.h>
#include <tokens/tokengroupconfiguration.h>

#include <unordered_map>

class CCoinsViewCache;
class BettingsView;
class CValidationState;

class RegularBetMintRequest {
private:
    CTransactionRef tx;
    CBetEvent betEvent;
    std::vector<unsigned char> betData;
    CAmount nBetCosts = 0;
    CTokenGroupID tgID;
    CTokenGroupCreation tgCreation;
    CTokenGroupBalance tgMintMeltBalanceItem;

    bool isValid = false;
public:

    RegularBetMintRequest() {};

    RegularBetMintRequest(const CTransactionRef& tx, const uint32_t nEventID, const CAmount nWGRSpent, const CTokenGroupID& tgID, const CTokenGroupCreation& tgCreation, const CTokenGroupBalance& tgMintMeltBalanceItem) :
        tx(tx), betEvent(betEvent), nBetCosts(nWGRSpent), tgID(tgID), tgCreation(tgCreation), tgMintMeltBalanceItem(tgMintMeltBalanceItem)
    {
        betData = tgID.GetSubGroupData();
    };

    CTokenGroupDescriptionBetting* GetTokenGroupDescription();

    template<typename BettingTxTypeName>
    std::unique_ptr<BettingTxTypeName> GetBettingTx();

    bool GetOdds(const CBettingsView& bettingsViewCache, uint32_t& nOdds);

    CAmount GetBetCosts() { return nBetCosts; };
    bool ValidateBetCosts(const CBettingsView& bettingsViewCache);
    bool Validate(CValidationState &state, const CBettingsView& bettingsViewCache, const int nHeight);
    bool IsValid() {
        return isValid;
    }
};

bool CreateRegularBetMintRequest(const CTransactionRef& tx,  CValidationState &state, const CBettingsView& bettingsViewCache, const CAmount nWGRSpent, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, RegularBetMintRequest& req);

bool CheckBetMints(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, const CAmount nWagerrIn, const CAmount nWagerrOut, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance);
bool GetBetMintRequest(const CTransactionRef& tx, CValidationState &state, const CBettingsView& bettingsViewCache, const CAmount nWGRSpent, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, std::shared_ptr<RegularBetMintRequest>& regularBetMintRequest);

#endif // WAGERR_BET_TOKEN_MINT_H