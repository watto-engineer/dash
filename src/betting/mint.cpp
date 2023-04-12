// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/mint.h>

#include <wagerraddrenc.h>
#include <util/system.h>
#include <betting/bet.h>
#include <betting/bet_db.h>
#include <betting/bet_tx.h>
#include <timedata.h>
#include <tokens/tokengroupmanager.h>

bool CreateRegularBetMintRequest(const CTransactionRef& tx, CValidationState &state, const CBettingsView& bettingsViewCache, const CAmount nWGRSpent, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, RegularBetMintRequest& req) {
    CTokenGroupID tgID;
    CTokenGroupBalance tgMintMeltBalanceItem;
    CTokenGroupCreation tgCreation;
    CBetEvent betEvent;

    bool isValid = false;

    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        tgID = mintMeltItem.first;
        tgMintMeltBalanceItem = mintMeltItem.second;
    }

    if (!tokenGroupManager.get()->GetTokenGroupCreation(tgID, tgCreation)) {
        return state.Invalid(ValidationInvalidReason::TX_BAD_SPECIAL, error("Unable to find token group %s", EncodeTokenGroup(tgID)), REJECT_INVALID, "op_group-bad-mint");
    }
    CTokenGroupDescriptionBetting *tgDesc = boost::get<CTokenGroupDescriptionBetting>(tgCreation.pTokenGroupDescription.get());

    if (!CreateBetEventFromDB(bettingsViewCache, tgDesc->nEventId, betEvent)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-event");
    };

    req = RegularBetMintRequest(tx, betEvent.nEventId, nWGRSpent, tgID, tgCreation, tgMintMeltBalanceItem);
    return true;
}

bool BetTokensMinted(const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance) {
    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        if (mintMeltItem.first.hasFlag(TokenGroupIdFlags::BETTING_TOKEN) &&
            mintMeltItem.second.output > mintMeltItem.second.input) return true;
    }
    return false;
}

bool IsRegularBetMintRequest(const CAmount nWGRSpent, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance) {
    if (nWGRSpent <= 0){
        LogPrintf("No WGR spent (%d)\n", nWGRSpent);
        return false;
    }
    if (tgMintMeltBalance.size() != 1){
        LogPrintf("MintMeltBalance not 1 (%d)\n", tgMintMeltBalance.size());
        return false;
    }
    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        if (!mintMeltItem.first.hasFlag(TokenGroupIdFlags::BETTING_TOKEN)) {
            LogPrintf("No Betting Token flag\n");
            return false;
        }
        if (mintMeltItem.second.numOutputs <= 0) {
            LogPrintf("token outputs <= 0 (%d)\n", mintMeltItem.second.output);
            return false;
        }
        if (mintMeltItem.second.numInputs != 0) {
            LogPrintf("token inputs != 0 (%d)\n", mintMeltItem.second.input);
            return false;
        }
    }
    return true;
}

bool IsParlayBetMintRequest(const CAmount nWGRSpent, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance) {
    if (nWGRSpent <= 0){
        LogPrintf("No WGR spent (%d)\n", nWGRSpent);
        return false;
    }
    if (tgMintMeltBalance.size() <= 1){
        LogPrintf("MintMeltBalance not 1 (%d)\n", tgMintMeltBalance.size());
        return false;
    }
    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        if (!mintMeltItem.first.hasFlag(TokenGroupIdFlags::PARLAY_TOKEN)) {
            LogPrintf("No Parlay Token flag\n");
            return false;
        }
        if (mintMeltItem.second.numOutputs <= 1) {
            LogPrintf("token outputs <= 1 (%d)\n", mintMeltItem.second.output);
            return false;
        }
        if (mintMeltItem.second.numInputs != 0) {
            LogPrintf("token inputs != 0 (%d)\n", mintMeltItem.second.input);
            return false;
        }
    }
    return true;
}

// A bet mint must not include fees
bool GetBetMintRequest(const CTransactionRef& tx, CValidationState &state, const CBettingsView& bettingsViewCache, const CAmount nWGRSpent, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, std::shared_ptr<RegularBetMintRequest>& regularBetMintRequest) {
    // If there are any bet token mints, the tx must be validated
    regularBetMintRequest = nullptr;
    if (!BetTokensMinted(tgMintMeltBalance)) return true;

    if (IsRegularBetMintRequest(nWGRSpent, tgMintMeltBalance)) {
        // Validate regular bet
        RegularBetMintRequest req;
        if (!CreateRegularBetMintRequest(tx, state, bettingsViewCache, nWGRSpent, tgMintMeltBalance, req)) {
            return false;
        }
        regularBetMintRequest = std::make_shared<RegularBetMintRequest>(req);
        return true;
    } else if (IsParlayBetMintRequest(nWGRSpent, tgMintMeltBalance)) {
        return state.Invalid(ValidationInvalidReason::TX_BAD_BET, error("Not yet implemented"), REJECT_INVALID, "op_group-bad-mint");
    }
    return state.Invalid(ValidationInvalidReason::TX_BAD_BET, error("No valid bet mint transaction found"), REJECT_INVALID, "op_group-bad-mint");
}

CTokenGroupDescriptionBetting* RegularBetMintRequest::GetTokenGroupDescription() {
    if (!tgCreation.pTokenGroupDescription)
        return nullptr;
    CTokenGroupDescriptionBetting *tgDesc = boost::get<CTokenGroupDescriptionBetting>(tgCreation.pTokenGroupDescription.get());
    return tgDesc;
}

template<typename BettingTxTypeName>
std::unique_ptr<BettingTxTypeName> RegularBetMintRequest::GetBettingTx() {
    CDataStream ss(betEvent.nEventId, SER_NETWORK, PROTOCOL_VERSION);
    ss << betData;
    return DeserializeBettingTx<BettingTxTypeName>(ss);
}

template std::unique_ptr<CPeerlessBetTx> RegularBetMintRequest::GetBettingTx();
template std::unique_ptr<CFieldBetTx> RegularBetMintRequest::GetBettingTx();

bool RegularBetMintRequest::GetOdds(const CBettingsView& bettingsViewCache, uint32_t& nOdds) {
    nOdds = 0;
    switch (betEvent.type)
    {
        case BetEventType::PEERLESS:
        {
            auto betTx = GetBettingTx<CPeerlessBetTx>();
            // Find the event in DB
            CPeerlessExtendedEventDB plEvent;
            if (!bettingsViewCache.events->Read(EventKey{betEvent.nEventId}, plEvent)) return false;
            CPeerlessLegDB legDB = CPeerlessLegDB{betEvent.nEventId, (OutcomeType)betTx->nOutcome};
            nOdds = GetBetPotentialOdds(legDB, plEvent);
            return true;
        }
        case BetEventType::FIELD:
        {
            auto betTx = GetBettingTx<CFieldBetTx>();
            // Find the event in DB
            CFieldEventDB fEvent;
            if (!bettingsViewCache.fieldEvents->Read(EventKey{betEvent.nEventId}, fEvent)) return false;
            CFieldLegDB legDB = CFieldLegDB{betEvent.nEventId, (FieldBetOutcomeType)betTx->nOutcome, betTx->nContenderId};
            nOdds = GetBetPotentialOdds(legDB, fEvent);
            return true;
        }
    }
    return false;
}

bool RegularBetMintRequest::Validate(CValidationState &state, const CBettingsView& bettingsViewCache, const int nHeight) {
    isValid = false;

    if (nBetCosts < (Params().GetConsensus().MinBetPayoutRange() * COIN ) || nBetCosts > (Params().GetConsensus().MaxBetPayoutRange() * COIN)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-bet-amount");
    }

    if (!betEvent.IsOpen(bettingsViewCache, GetAdjustedTime())) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-event");
    }
    uint32_t nOdds;
    if (!GetOdds(bettingsViewCache, nOdds)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-event");
    };
    CAmount nPayout, nBurn;
    if (!CalculatePayoutBurnAmounts(GetBetCosts(), nOdds, nPayout, nBurn)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bet-bad-odds");
    }
    
    if (nPayout != tgMintMeltBalanceItem.output - tgMintMeltBalanceItem.input) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bet-bad-costs");
    };
    isValid = true;
    return IsValid();
}
