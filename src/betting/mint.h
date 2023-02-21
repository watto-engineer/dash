// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_TOKEN_MINT_H
#define WAGERR_BET_TOKEN_MINT_H

#include <amount.h>
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
    CTokenGroupID tgID;
    CTokenGroupBalance tgMintMeltBalanceItem;
    CTokenGroupCreation tgCreation;
    CAmount nWagerrIn = 0;
    CAmount nWagerrOut = 0;
    CAmount nWagerrSpent = 0;
    uint32_t nEventID = 0;

    // Calculated by SetBetCosts() (after event updates are processed)
    CAmount nWagerrBetCosts = 0;
    CAmount nWagerrFees = 0;

    bool isValid = false;
public:

    RegularBetMintRequest() {};

    RegularBetMintRequest(const CAmount nWagerrIn, const CAmount nWagerrOut, const uint32_t nEventID, const CTokenGroupID& tgID, const CTokenGroupBalance& tgMintMeltBalanceItem) :
        tx(tx), nWagerrIn(nWagerrIn), nWagerrOut(nWagerrOut), nWagerrSpent(nWagerrIn - nWagerrOut), nEventID(nEventID), tgID(tgID), tgCreation(tgCreation)
    {};

    CTokenGroupDescriptionBetting* GetTokenGroupDescription();

    bool SetBetCosts(const CBettingsView& bettingsViewCache);
    void Validate();
    bool IsValid() {
        return isValid;
    }
};

bool CreateRegularBetMintRequest(const CTransactionRef& tx,  CValidationState &state, const CAmount nWagerrIn, const CAmount nWagerrOut, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, RegularBetMintRequest& req);

bool CheckBetMints(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, const CBettingsView& bettingsViewCache);

#endif // WAGERR_BET_TOKEN_MINT_H