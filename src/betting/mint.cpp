// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/mint.h>

#include <wagerraddrenc.h>
#include <util/system.h>
#include "tokens/tokengroupmanager.h"

bool CreateRegularBetMintRequest(const CTransactionRef& tx, CValidationState &state, const CAmount nWagerrIn, const CAmount nWagerrOut, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, RegularBetMintRequest& req) {
    CTokenGroupID tgID;
    CTokenGroupBalance tgMintMeltBalanceItem;
    CTokenGroupCreation tgCreation;
    uint32_t nEventID;

    bool isValid = false;

    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        tgID = mintMeltItem.first;
        tgMintMeltBalanceItem = mintMeltItem.second;
    }

    if (!tokenGroupManager.get()->GetTokenGroupCreation(tgID, tgCreation)) {
        return state.Invalid(ValidationInvalidReason::TX_BAD_SPECIAL, error("Unable to find token group %s", EncodeTokenGroup(tgID)), REJECT_INVALID, "op_group-bad-mint");
    }
    CTokenGroupDescriptionBetting *tgDesc = boost::get<CTokenGroupDescriptionBetting>(tgCreation.pTokenGroupDescription.get());
    nEventID = tgDesc->nEventId;

    req = RegularBetMintRequest(nWagerrIn, nWagerrOut, nEventID, tgID, tgMintMeltBalanceItem);
    return true;
}

bool BetTokensMinted(const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance) {
    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        if (mintMeltItem.first.hasFlag(TokenGroupIdFlags::BETTING_TOKEN) &&
            mintMeltItem.second.output > mintMeltItem.second.input) return true;
    }
    return false;
}

bool IsRegularBetMintRequest(const CAmount nWagerrIn, const CAmount nWagerrOut, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance) {
    if (tgMintMeltBalance.size() != 1) return false;
    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        if (!mintMeltItem.first.hasFlag(TokenGroupIdFlags::BETTING_TOKEN)) return false;
        if (mintMeltItem.second.output <= 0) return false;
        if (mintMeltItem.second.input != 0) return false;
    }
    if (nWagerrOut >= nWagerrIn) return false;
    return true;
}

// Validate that the tokens minted are qualified parlay bet token groups
bool ValidParlayBetGroups(const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance) {
    if (tgMintMeltBalance.size() < 2) return false;
    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        if (!mintMeltItem.first.hasFlag(TokenGroupIdFlags::BETTING_TOKEN)) return false;
        if (!mintMeltItem.first.hasFlag(TokenGroupIdFlags::PARLAY_TOKEN)) return false;
        if (mintMeltItem.second.output <= 0) return false;
        if (mintMeltItem.second.input != 0) return false;
    }
    return true;
}

bool ValidRegularBetAmount(const CTransaction& tx, CValidationState &state, const CAmount nWagerrIn, const CAmount nWagerrOut, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance) {
    // Max input: 10.000 WGR
    // WGR bet costs: bet tokens minted / OnChainOdds
    // Miner fee: WGR output - WGR input - WGR bet costs
    // Oracle and dev fund fees are paid when redeeming bet tokens
    CAmount nWagerrSpent = nWagerrOut - nWagerrIn;

    if (nWagerrIn > nWagerrOut) return false;
    if (nWagerrSpent > 10000 * COIN) return false;


    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        CAmount nMintAmount = (mintMeltItem.second.output - mintMeltItem.second.input);

        CTokenGroupCreation tgCreation;
        if (!tokenGroupManager.get()->GetTokenGroupCreation(mintMeltItem.first, tgCreation)) {
            return state.Invalid(ValidationInvalidReason::TX_BAD_SPECIAL, error("Unable to find token group %s", EncodeTokenGroup(mintMeltItem.first)), REJECT_INVALID, "op_group-bad-mint");
        }
        CTokenGroupDescriptionBetting *tgDesc = boost::get<CTokenGroupDescriptionBetting>(tgCreation.pTokenGroupDescription.get());
        auto nMintEventId = tgDesc->nEventId;
    }
    return true;
}

bool CheckBetMints(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, const CBettingsView& bettingsViewCache) {
    // If there are any bet token mints, the tx must be validated
    // If not, this check passes
    if (!BetTokensMinted(tgMintMeltBalance)) return true;

    /* 
        Two scenario's:
        - One mint of a betting token for a peerless bet or field bet
        - Multiple mints of betting tokens for a parlay bet
        
    */
    
    if (IsRegularBetMintRequest(nWagerrIn, nWagerrOut, tgMintMeltBalance)) {
        // Validate regular bet
        RegularBetMintRequest req;
        return CreateRegularBetMintRequest(MakeTransactionRef(std::move(tx)), state, nWagerrIn, nWagerrOut, tgMintMeltBalance, req);
    } else {
        return state.Invalid(ValidationInvalidReason::TX_BAD_BET, error("No valid bet mint transaction found"), REJECT_INVALID, "op_group-bad-mint");
    }

    return false;
}

CTokenGroupDescriptionBetting* RegularBetMintRequest::GetTokenGroupDescription() {
    if (!tgCreation.pTokenGroupDescription)
        return nullptr;
    CTokenGroupDescriptionBetting *tgDesc = boost::get<CTokenGroupDescriptionBetting>(tgCreation.pTokenGroupDescription.get());
    return tgDesc;
}

bool RegularBetMintRequest::SetBetCosts(const CBettingsView& bettingsViewCache) {

    return false;
}

void RegularBetMintRequest::Validate() {
    
}

