// Copyright (c) 2018-2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_common.h>
#include <betting/bet_tx.h>
#include <betting/bet_db.h>
#include <betting/bet_v2.h>
#include <betting/bet_v3.h>
#include <betting/bet_v4.h>

#include <core_io.h>
#include "spork.h"
#include "uint256.h"
#include <validation.h>
#include "wallet/wallet.h"
#include <boost/filesystem.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/exception/to_string.hpp>

bool ExtractPayouts(const CBlock& block, const int& nBlockHeight, std::vector<CTxOut>& vFoundPayouts, uint32_t& nPayoutOffset, uint32_t& nWinnerPayments, const CAmount& nExpectedMint, const CAmount& nExpectedMNReward)
{
    const CTransactionRef &tx = block.vtx[1];

    // Get the vin staking value so we can use it to find out how many staking TX in the vouts.
    COutPoint prevout         = tx->vin[0].prevout;
    CAmount stakeAmount       = 0;
    bool fStakesFound         = false;

    nPayoutOffset = 0;
    nWinnerPayments = 0;

    uint256 hashBlock;
    CTransactionRef txPrev = GetTransaction(nullptr, nullptr,  prevout.hash, Params().GetConsensus(), hashBlock, true);

    if (txPrev) {
        stakeAmount = txPrev->vout[prevout.n].nValue + nExpectedMint;
    } else {
        return false;
    }

    // Set the OMNO and Dev reward addresses
    CScript devPayoutScript;
    CScript OMNOPayoutScript;
    if (!GetFeePayoutScripts(nBlockHeight, devPayoutScript, OMNOPayoutScript)) {
        LogPrintf("Unable to find oracle, skipping payouts\n");
        return false;
    }

    // Count the coinbase and staking vouts in the current block TX.
    CAmount totalStakeAcc = 0;
    const size_t txVoutSize = tx->vout.size();

    size_t nMaxVoutI = txVoutSize;
    CAmount nMNReward = 0;
    if (txVoutSize > 2 && tx->vout[txVoutSize - 1].nValue == nExpectedMNReward) {
        nMaxVoutI--;
        nMNReward = nExpectedMNReward;
    }

    for (size_t i = 0; i < nMaxVoutI; i++) {
        const CTxOut &txout = tx->vout[i];
        CAmount voutValue   = txout.nValue;
        CScript voutScript = txout.scriptPubKey;
        if (fStakesFound) {
            if (voutScript != devPayoutScript && voutScript != OMNOPayoutScript) {
                nWinnerPayments++;
            }
            if (voutValue > 0) vFoundPayouts.emplace_back(voutValue, voutScript);
        } else {
            nPayoutOffset++;
            totalStakeAcc += voutValue;

            // Needs some slack?
            if (totalStakeAcc + nMNReward == stakeAmount) {
                fStakesFound = true;
            }
        }
    }
    return fStakesFound || (nWinnerPayments == 0 && totalStakeAcc + nMNReward < stakeAmount);
}

bool IsBlockPayoutsValid(CBettingsView &bettingsViewCache, const std::multimap<CPayoutInfoDB, CBetOut>& mExpectedPayoutsIn, const CBlock& block, const int nBlockHeight, const CAmount& nExpectedMint, const CAmount& nExpectedMNReward)
{
    const CTransactionRef &tx = block.vtx[1];
    std::multimap<CPayoutInfoDB, CBetOut> mExpectedPayouts = mExpectedPayoutsIn;

    std::vector<CTxOut> vFoundPayouts;
    std::multiset<CTxOut> setFoundPayouts;
    std::multiset<CTxOut> setExpectedPayouts;

    uint32_t nPayoutOffset = 0;
    uint32_t nWinnerPayments = 0; // unused

    // If we have payouts to validate. Note: bets can only happen in blocks with MN payments.
    if (!ExtractPayouts(block, nBlockHeight, vFoundPayouts, nPayoutOffset, nWinnerPayments, nExpectedMint, nExpectedMNReward)) {
        LogPrintf("%s - Not all payouts found - %s\n", __func__, block.GetHash().ToString());
        ExtractPayouts(block, nBlockHeight, vFoundPayouts, nPayoutOffset, nWinnerPayments, nExpectedMint, nExpectedMNReward);
        return false;
    }
    setFoundPayouts = std::multiset<CTxOut>(vFoundPayouts.begin(), vFoundPayouts.end());

    for (auto expectedPayout : mExpectedPayouts)
    {
        setExpectedPayouts.insert(expectedPayout.second);
    }

    if (setExpectedPayouts != setFoundPayouts) {
        LogPrintf("%s - Expected payouts:\n", __func__);
        for (auto expectedPayout : setExpectedPayouts) {
            LogPrintf("%d %s\n", expectedPayout.nValue, ScriptToAsmStr(expectedPayout.scriptPubKey));
        }
        LogPrintf("%s - Found payouts:\n", __func__);
        for (auto foundPayouts : setFoundPayouts) {
            LogPrintf("%d %s\n", foundPayouts.nValue, ScriptToAsmStr(foundPayouts.scriptPubKey));
        }
        LogPrintf("%s - Not all payouts validate - %s\n", __func__, block.GetHash().ToString());
        return false;
    }

    // Lookup txid+voutnr and store in database cache
    for (uint32_t i = 0; i < vFoundPayouts.size(); i++)
    {
        auto expectedPayout = std::find_if(mExpectedPayouts.begin(), mExpectedPayouts.end(),
                        [vFoundPayouts, i](std::pair<const CPayoutInfoDB, CBetOut> eP) {
                            return eP.second.nValue == vFoundPayouts[i].nValue && eP.second.scriptPubKey == vFoundPayouts[i].scriptPubKey;
                        });
        if (expectedPayout != mExpectedPayouts.end()) {
            PayoutInfoKey payoutInfoKey{static_cast<uint32_t>(nBlockHeight), COutPoint{tx->GetHash(), i+nPayoutOffset}};
            bettingsViewCache.payoutsInfo->Write(payoutInfoKey, expectedPayout->first);

            mExpectedPayouts.erase(expectedPayout);
        } else {
            // Redundant: should not happen after previous test 'setExpectedPayouts != setFoundPayouts'
            LogPrintf("%s - Could not find expected payout - %s\n", __func__, block.GetHash().ToString());
            return false;
        }
    }

    return true;
}

bool CheckBettingTx(const CCoinsViewCache &view, CBettingsView& bettingsViewCache, const CTransaction& tx, const int height)
{
    // if is not wagerr v3 - do not check tx
    if (height < Params().GetConsensus().WagerrProtocolV3StartHeight()) return true;

    if (!HasOpReturnOutput(tx)) return true;

    for (const CTxOut &txOut : tx.vout) {
        // parse betting TX
        auto bettingTx = ParseBettingTx(txOut);

        if (bettingTx == nullptr) continue;

        if (height >= sporkManager->GetSporkValue(SPORK_20_BETTING_MAINTENANCE_MODE)) {
            return error("CheckBettingTX : Betting transactions are temporarily disabled for maintenance");
        }

        CAmount betAmount{txOut.nValue};

        switch(bettingTx->GetTxType()) {
            case plBetTxType:
            {
                CPeerlessBetTx* betTx = (CPeerlessBetTx*) bettingTx.get();
                CPeerlessLegDB plBet{betTx->nEventId, (OutcomeType) betTx->nOutcome};
                // Validate bet amount so its between 25 - 10000 WGR inclusive.
                if (betAmount < (Params().GetConsensus().MinBetPayoutRange()  * COIN ) || betAmount > (Params().GetConsensus().MaxBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                CPeerlessExtendedEventDB plEvent;
                // Find the event in DB
                if (bettingsViewCache.events->Read(EventKey{plBet.nEventId}, plEvent)) {
                    if (bettingsViewCache.results->Exists(ResultKey{plBet.nEventId})) {
                        return error("CheckBettingTX: Bet placed to resulted event %lu!", plBet.nEventId);
                    }

                    if (::ChainActive().Height() >= Params().GetConsensus().WagerrProtocolV4StartHeight()) {
                        if (GetBetPotentialOdds(plBet, plEvent) == 0) {
                            return error("CheckBettingTX: Bet potential odds is zero for Event %lu outcome %d!", plBet.nEventId, plBet.nOutcome);
                        }
                    }
                }
                else {
                    return error("CheckBettingTX: Failed to find event %lu!", plBet.nEventId);
                }
                break;
            }
            case plParlayBetTxType:
            {
                CPeerlessParlayBetTx* parlayBetTx = (CPeerlessParlayBetTx*) bettingTx.get();
                std::vector<CPeerlessBetTx> &legs = parlayBetTx->legs;

                if (legs.size() > Params().GetConsensus().MaxParlayLegs())
                    return error("CheckBettingTX: The invalid parlay bet count of legs!");

                // Validate parlay bet amount so its between 25 - 4000 WGR inclusive.
                if (betAmount < (Params().GetConsensus().MinBetPayoutRange()  * COIN ) || betAmount > (Params().GetConsensus().MaxParlayBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                // check event ids in legs and deny if some is equal
                {
                    std::set<uint32_t> ids;
                    for (const CPeerlessBetTx& leg : legs) {
                        if (ids.find(leg.nEventId) != ids.end())
                            return error("CheckBettingTX: Parlay bet has some legs with same event id!");
                        else
                            ids.insert(leg.nEventId);
                    }
                }

                for (const CPeerlessBetTx& leg : legs) {
                    CPeerlessExtendedEventDB plEvent;
                    // Find the event in DB
                    if (bettingsViewCache.events->Read(EventKey{leg.nEventId}, plEvent)) {
                        if (bettingsViewCache.results->Exists(ResultKey{leg.nEventId})) {
                            return error("CheckBettingTX: Bet placed to resulted event %lu!", leg.nEventId);
                        }

                        if (::ChainActive().Height() >= Params().GetConsensus().WagerrProtocolV4StartHeight()) {
                            if (GetBetPotentialOdds(CPeerlessLegDB{leg.nEventId, (OutcomeType)leg.nOutcome}, plEvent) == 0) {
                                return error("CheckBettingTX: Bet potential odds is zero for Event %lu outcome %d!", leg.nEventId, leg.nOutcome);
                            }
                            if (plEvent.nStage != 0) {
                                return error("CheckBettingTX: event %lu cannot be part of parlay bet!", leg.nEventId);
                            }
                        }
                    }
                    else {
                        return error("CheckBettingTX: Failed to find event %lu!", leg.nEventId);
                    }
                }
                break;
            }
            case fBetTxType:
            {
                if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) return error("CheckBettingTX: Spork is not active for FieldBetTx");

                // Validate bet amount so its between 25 - 10000 WGR inclusive.
                if (betAmount < (Params().GetConsensus().MinBetPayoutRange()  * COIN ) || betAmount > (Params().GetConsensus().MaxBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }

                CFieldBetTx* betTx = (CFieldBetTx*) bettingTx.get();
                CFieldEventDB fEvent;
                if (!bettingsViewCache.fieldEvents->Read(FieldEventKey{betTx->nEventId}, fEvent)) {
                    return error("CheckBettingTX: Failed to find field event %lu!", betTx->nEventId);
                }

                if (bettingsViewCache.fieldResults->Exists(FieldResultKey{betTx->nEventId})) {
                    return error("CheckBettingTX: Bet placed to resulted field event %lu!", betTx->nEventId);
                }

                if (!fEvent.IsMarketOpen((FieldBetOutcomeType)betTx->nOutcome)) {
                    return error("CheckBettingTX: market %lu is closed for event %lu!", betTx->nOutcome, betTx->nEventId);
                }

                if (fEvent.contenders.find(betTx->nContenderId) == fEvent.contenders.end()) {
                    return error("CheckBettingTX: Unknown contenderId %lu for event %lu!", betTx->nContenderId, betTx->nEventId);
                }

                CFieldLegDB legDB{betTx->nEventId, (FieldBetOutcomeType)betTx->nOutcome, betTx->nContenderId};
                if (GetBetPotentialOdds(legDB, fEvent) == 0) {
                    return error("CheckBettingTX: Bet odds is zero for Event %lu contenderId %d!", betTx->nEventId, betTx->nContenderId);
                }

                break;
            }
            case fParlayBetTxType:
            {
                if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) return error("CheckBettingTX: Spork is not active for FieldParlayBetTx");

                // Validate bet amount so its between 25 - 10000 WGR inclusive.
                if (betAmount < (Params().GetConsensus().MinBetPayoutRange()  * COIN ) || betAmount > (Params().GetConsensus().MaxBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }

                CFieldParlayBetTx* betTx = (CFieldParlayBetTx*) bettingTx.get();
                std::vector<CFieldBetTx> &legs = betTx->legs;

                if (legs.size() > Params().GetConsensus().MaxParlayLegs()) {
                    return error("CheckBettingTX: The invalid field parlay bet count of legs!");
                }

                // check event ids in legs and deny if some is equal
                {
                    std::set<uint32_t> ids;
                    for (const auto& leg : legs) {
                        if (ids.find(leg.nEventId) != ids.end())
                            return error("CheckBettingTX: Parlay bet has some legs with same event id!");
                        else
                            ids.insert(leg.nEventId);
                    }
                }

                for (const auto& leg : legs) {
                    CFieldEventDB fEvent;
                    if (!bettingsViewCache.fieldEvents->Read(FieldEventKey{leg.nEventId}, fEvent)) {
                        return error("CheckBettingTX: Failed to find field event %lu!", leg.nEventId);
                    }

                    if (bettingsViewCache.fieldResults->Exists(FieldResultKey{leg.nEventId})) {
                        return error("CheckBettingTX: Bet placed to resulted field event %lu!", leg.nEventId);
                    }

                    if (!fEvent.IsMarketOpen((FieldBetOutcomeType)leg.nOutcome)) {
                        return error("CheckBettingTX: market %lu is closed for event %lu!", leg.nOutcome, leg.nEventId);
                    }

                    if (fEvent.contenders.find(leg.nContenderId) == fEvent.contenders.end()) {
                        return error("CheckBettingTX: Unknown contenderId %lu for event %lu!", leg.nContenderId, leg.nEventId);
                    }

                    CFieldLegDB legDB{leg.nEventId, (FieldBetOutcomeType)leg.nOutcome, leg.nContenderId};
                    if (GetBetPotentialOdds(legDB, fEvent) == 0) {
                        return error("CheckBettingTX: Bet odds is zero for Event %lu contenderId %d!", leg.nEventId, leg.nContenderId);
                    }

                    if (fEvent.nStage != 0) {
                        return error("CheckBettingTX: event %lu cannot be part of parlay bet!", leg.nEventId);
                    }
                }

                break;
            }
            case cgBetTxType:
            {
                if (height >= Params().GetConsensus().QuickGamesEndHeight()) {
                    return error("CheckBettingTX : Chain games transactions are disabled");
                }
                CChainGamesBetTx* cgBetTx = (CChainGamesBetTx*) bettingTx.get();

                CChainGamesEventDB cgEvent;
                EventKey eventKey{cgBetTx->nEventId};
                if (!bettingsViewCache.chainGamesLottoEvents->Read(eventKey, cgEvent)) {
                    return error("CheckBettingTX: Failed to find event %lu!", cgBetTx->nEventId);
                }
                // Check event result
                if (bettingsViewCache.chainGamesLottoResults->Exists(eventKey)) {
                    return error("CheckBettingTX: Bet placed to resulted event %lu!", cgBetTx->nEventId);
                }
                // Validate chain game bet amount
                if (betAmount != cgEvent.nEntryFee * COIN) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                break;
            }
            case qgBetTxType:
            {
                if (height >= Params().GetConsensus().QuickGamesEndHeight()) {
                    return error("CheckBettingTX : Quick games transactions are disabled");
                }

                CQuickGamesBetTx* qgBetTx = (CQuickGamesBetTx*) bettingTx.get();
                if (!(qgBetTx->gameType == QuickGamesType::qgDice)) {
                    return error("CheckBettingTX: Invalid game type (%d)", qgBetTx->gameType);
                }
                // Validate quick game bet amount so its between 25 - 10000 WGR inclusive.
                if (betAmount < (Params().GetConsensus().MinBetPayoutRange()  * COIN ) || betAmount > (Params().GetConsensus().MaxBetPayoutRange() * COIN)) {
                    return error("CheckBettingTX: Bet placed with invalid amount %lu!", betAmount);
                }
                break;
            }
            default:
            {
                // Get player address
                const CTxIn& txin{tx.vin[0]};
                uint256 hashBlock;
                CTxDestination address;
                CTxDestination prevAddr;
                // if we cant extract playerAddress - skip tx
                CTransactionRef txPrev = GetTransaction(nullptr, nullptr,  txin.prevout.hash, Params().GetConsensus(), hashBlock, true);

                if (!txPrev ||
                        !ExtractDestination(txPrev->vout[txin.prevout.n].scriptPubKey, prevAddr)) {
                    return true;
                }
                address = prevAddr;

                bool validOracleTx{IsValidOraclePrevTxOut(txPrev->vout[txin.prevout.n], height)};
                switch(bettingTx->GetTxType()) {
                    case mappingTxType:
                    {
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CMappingTx* mapTx = (CMappingTx*) bettingTx.get();

                        auto mappingType = MappingType(mapTx->nMType);
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight() &&
                        (mappingType == individualSportMapping || mappingType == contenderMapping ) )
                        {
                            return error("CheckBettingTX: Spork is not active for mapping type %lu!", mappingType);
                        }

                        if (bettingsViewCache.mappings->Exists(MappingKey{mappingType, mapTx->nId}))
                            return error("CheckBettingTX: trying to create existed mapping!");
                        break;
                    }
                    case plEventTxType:
                    {
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CPeerlessEventTx* plEventTx = (CPeerlessEventTx*) bettingTx.get();

                        if (bettingsViewCache.events->Exists(EventKey{plEventTx->nEventId}))
                            return error("CheckBettingTX: trying to create existed event id %lu!", plEventTx->nEventId);

                        if (!bettingsViewCache.mappings->Exists(MappingKey{sportMapping, (uint32_t) plEventTx->nSport}))
                            return error("CheckBettingTX: trying to create event with unknown sport id %lu!", plEventTx->nSport);

                        if (!bettingsViewCache.mappings->Exists(MappingKey{tournamentMapping, (uint32_t) plEventTx->nTournament}))
                            return error("CheckBettingTX: trying to create event with unknown tournament id %lu!", plEventTx->nTournament);

                        if (!bettingsViewCache.mappings->Exists(MappingKey{roundMapping, (uint32_t) plEventTx->nStage}))
                            return error("CheckBettingTX: trying to create event with unknown round id %lu!", plEventTx->nStage);

                        if (!bettingsViewCache.mappings->Exists(MappingKey{teamMapping, plEventTx->nHomeTeam}))
                            return error("CheckBettingTX: trying to create event with unknown home team id %lu!", plEventTx->nHomeTeam);

                        if (!bettingsViewCache.mappings->Exists(MappingKey{teamMapping, plEventTx->nAwayTeam}))
                            return error("CheckBettingTX: trying to create event with unknown away team id %lu!", plEventTx->nAwayTeam);

                        /*
                        if (!CHECK_ODDS(plEventTx->nHomeOdds))
                            return error("CheckBettingTX: invalid home odds %lu!", plEventTx->nHomeOdds);

                        if (!CHECK_ODDS(plEventTx->nAwayOdds))
                            return error("CheckBettingTX: invalid away odds %lu!", plEventTx->nAwayOdds);

                        if (!CHECK_ODDS(plEventTx->nDrawOdds))
                            return error("CheckBettingTX: invalid draw odds %lu!", plEventTx->nDrawOdds);
                        */

                        break;
                    }
                    case fEventTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) return error("CheckBettingTX: Spork is not active for FieldEventTx!");
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CFieldEventTx* fEventTx = (CFieldEventTx*) bettingTx.get();

                        if (bettingsViewCache.fieldEvents->Exists(FieldEventKey{fEventTx->nEventId}))
                            return error("CheckBettingTX: trying to create existed field event id %lu!", fEventTx->nEventId);

                        if (fEventTx->nGroupType < FieldEventGroupType::other || fEventTx->nGroupType > FieldEventGroupType::animalRacing)
                            return error("CheckBettingTx: trying to create field event with bad group type %lu!", fEventTx->nGroupType);

                        if (fEventTx->nMarketType < FieldEventMarketType::all_markets || fEventTx->nMarketType > FieldEventMarketType::outrightOnly)
                            return error("CheckBettingTx: trying to create field event with bad market type %lu!", fEventTx->nMarketType);

                        if (!bettingsViewCache.mappings->Exists(MappingKey{individualSportMapping, (uint32_t) fEventTx->nSport}))
                            return error("CheckBettingTX: trying to create field event with unknown individual sport id %lu!", fEventTx->nSport);

                        if (!bettingsViewCache.mappings->Exists(MappingKey{tournamentMapping, (uint32_t) fEventTx->nTournament}))
                            return error("CheckBettingTX: trying to create field event with unknown tournament id %lu!", fEventTx->nTournament);

                        if (!bettingsViewCache.mappings->Exists(MappingKey{roundMapping, (uint32_t) fEventTx->nStage}))
                            return error("CheckBettingTX: trying to create field event with unknown round id %lu!", fEventTx->nStage);

                        for (const auto& contender : fEventTx->mContendersInputOdds) {
                            if (!bettingsViewCache.mappings->Exists(MappingKey{contenderMapping, contender.first}))
                                return error("CheckBettingTx: trying to create field event with unknown contender %lu!", contender.first);
                        }

                        break;
                    }
                    case fUpdateOddsTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) return error("CheckBettingTX: Spork is not active for FieldUpdateOddsTx!");
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CFieldUpdateOddsTx* fUpdateOddsTx = (CFieldUpdateOddsTx*) bettingTx.get();

                        if (!bettingsViewCache.fieldEvents->Exists(FieldEventKey{fUpdateOddsTx->nEventId}))
                            return error("CheckBettingTX: trying to update not existed field event id %lu!", fUpdateOddsTx->nEventId);

                        for (const auto& contender : fUpdateOddsTx->mContendersInputOdds) {
                            if (!bettingsViewCache.mappings->Exists(MappingKey{contenderMapping, contender.first}))
                                return error("CheckBettingTx: trying to update odds for unknown contender %lu!", contender.first);
                        }

                        break;
                    }
                    case fUpdateModifiersTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) return error("CheckBettingTX: Spork is not active for FieldUpdateOddsTx!");
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CFieldUpdateModifiersTx* fUpdateOddsTx = (CFieldUpdateModifiersTx*) bettingTx.get();

                        if (!bettingsViewCache.fieldEvents->Exists(FieldEventKey{fUpdateOddsTx->nEventId}))
                            return error("CheckBettingTX: trying to update not existed field event id %lu!", fUpdateOddsTx->nEventId);

                        for (const auto& contender : fUpdateOddsTx->mContendersModifires) {
                            if (!bettingsViewCache.mappings->Exists(MappingKey{contenderMapping, contender.first}))
                                return error("CheckBettingTx: trying to update modifier for unknown contender %lu!", contender.first);
                        }

                        break;
                    }
                    case fUpdateMarginTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) return error("CheckBettingTX: Spork is not active for FieldUpdateMarginTx!");
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CFieldUpdateMarginTx* fUpdateMarginTx = (CFieldUpdateMarginTx*) bettingTx.get();

                        if (!bettingsViewCache.fieldEvents->Exists(FieldEventKey{fUpdateMarginTx->nEventId}))
                            return error("CheckBettingTX: trying to updating margin for not existed field event id %lu!", fUpdateMarginTx->nEventId);

                        break;
                    }
                    case fZeroingOddsTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) return error("CheckBettingTX: Spork is not active for FieldZeroingOddsTx!");
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CFieldZeroingOddsTx* fZeroingOddsTx = (CFieldZeroingOddsTx*) bettingTx.get();

                        if (!bettingsViewCache.fieldEvents->Exists(FieldEventKey{fZeroingOddsTx->nEventId}))
                            return error("CheckBettingTX: trying to zeroing odds for not existed field event id %lu!", fZeroingOddsTx->nEventId);

                        break;
                    }
                    case fResultTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) return error("CheckBettingTX: Spork is not active for FieldResultTx!");
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CFieldResultTx* fResultTx = (CFieldResultTx*) bettingTx.get();

                        if (fResultTx->nResultType != ResultType::standardResult &&
                                fResultTx->nResultType != ResultType::eventRefund &&
                                fResultTx->nResultType != ResultType::eventClosed)
                            return error("CheckBettingTX: unsupported result type for field event: %d!", fResultTx->nResultType);

                        if (!bettingsViewCache.fieldEvents->Exists(FieldEventKey{fResultTx->nEventId}))
                            return error("CheckBettingTX: trying to result not existed field event id %lu!", fResultTx->nEventId);

                        if (bettingsViewCache.fieldResults->Exists(FieldResultKey{fResultTx->nEventId}))
                            return error("CheckBettingTX: trying to result already resulted field event id %lu!", fResultTx->nEventId);

                        CFieldEventDB fEvent;
                        if (!bettingsViewCache.fieldEvents->Read(FieldEventKey{fResultTx->nEventId}, fEvent)) {
                            return error("CheckBettingTX: cannot read event %lu!", fResultTx->nEventId);
                        }

                        for (const auto& result : fResultTx->contendersResults) {
                            if (!bettingsViewCache.mappings->Exists(MappingKey{contenderMapping, (uint32_t) result.first}))
                                return error("CheckBettingTx: trying to create result for field event with unknown contender %lu!", result.first);

                            if (fEvent.contenders.find(result.first) == fEvent.contenders.end())
                                return error("CheckBettingTx: there is no contender %lu in event %lu!", result.first, fResultTx->nEventId);

                            if (result.second != ContenderResult::place1 &&
                                result.second != ContenderResult::place2 &&
                                result.second != ContenderResult::place3 &&
                                result.second != ContenderResult::DNF    &&
                                result.second != ContenderResult::DNR )
                            {
                                return error("CheckBettingTx: trying to create result for field event with unknown result %lu!", result.second);
                            }
                        }

                        break;
                    }
                    case plResultTxType:
                    {
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CPeerlessResultTx* plResultTx = (CPeerlessResultTx*) bettingTx.get();

                        if (!bettingsViewCache.events->Exists(EventKey{plResultTx->nEventId}))
                            return error("CheckBettingTX: trying to result not existed event id %lu!", plResultTx->nEventId);

                        if (bettingsViewCache.results->Exists(ResultKey{plResultTx->nEventId}))
                            return error("CheckBettingTX: trying to result already resulted event id %lu!", plResultTx->nEventId);
                        break;
                    }
                    case plUpdateOddsTxType:
                    {
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CPeerlessUpdateOddsTx* plUpdateOddsTx = (CPeerlessUpdateOddsTx*) bettingTx.get();

                        if (!bettingsViewCache.events->Exists(EventKey{plUpdateOddsTx->nEventId}))
                            return error("CheckBettingTX: trying to update not existed event id %lu!", plUpdateOddsTx->nEventId);

                        /*
                        if (!CHECK_ODDS(plUpdateOddsTx->nHomeOdds))
                            return error("CheckBettingTX: invalid home odds %lu!", plUpdateOddsTx->nHomeOdds);

                        if (!CHECK_ODDS(plUpdateOddsTx->nAwayOdds))
                            return error("CheckBettingTX: invalid away odds %lu!", plUpdateOddsTx->nAwayOdds);

                        if (!CHECK_ODDS(plUpdateOddsTx->nDrawOdds))
                            return error("CheckBettingTX: invalid draw odds %lu!", plUpdateOddsTx->nDrawOdds);
                        */

                        break;
                    }
                    case cgEventTxType:
                    {
                        if (height >= Params().GetConsensus().QuickGamesEndHeight()) {
                            return error("CheckBettingTX : Chain games transactions are disabled");
                        }
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CChainGamesEventTx* cgEventTx = (CChainGamesEventTx*) bettingTx.get();

                        if (bettingsViewCache.chainGamesLottoEvents->Exists(EventKey{cgEventTx->nEventId}))
                            return error("CheckBettingTX: trying to create existed chain games event id %lu!", cgEventTx->nEventId);

                        break;
                    }
                    case cgResultTxType:
                    {
                        if (height >= Params().GetConsensus().QuickGamesEndHeight()) {
                            return error("CheckBettingTX : Chain games transactions are disabled");
                        }
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CChainGamesResultTx* cgResultTx = (CChainGamesResultTx*) bettingTx.get();

                        if (!bettingsViewCache.chainGamesLottoEvents->Exists(EventKey{cgResultTx->nEventId}))
                            return error("CheckBettingTX: trying to result not existed chain games event id %lu!", cgResultTx->nEventId);

                        if (bettingsViewCache.chainGamesLottoResults->Exists(ResultKey{cgResultTx->nEventId}))
                            return error("CheckBettingTX: trying to result already resulted chain games event id %lu!", cgResultTx->nEventId);

                        break;
                    }
                    case plSpreadsEventTxType:
                    {
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CPeerlessSpreadsEventTx* plSpreadsEventTx = (CPeerlessSpreadsEventTx*) bettingTx.get();

                        if (!bettingsViewCache.events->Exists(EventKey{plSpreadsEventTx->nEventId}))
                            return error("CheckBettingTX: trying to create spreads at not existed event id %lu!", plSpreadsEventTx->nEventId);

                        /*
                        if (!CHECK_ODDS(plSpreadsEventTx->nHomeOdds))
                            return error("CheckBettingTX: invalid spreads home odds %lu!", plSpreadsEventTx->nHomeOdds);

                        if (!CHECK_ODDS(plSpreadsEventTx->nAwayOdds))
                            return error("CheckBettingTX: invalid spreads away odds %lu!", plSpreadsEventTx->nAwayOdds);
                        */

                        break;
                    }
                    case plTotalsEventTxType:
                    {
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CPeerlessTotalsEventTx* plTotalsEventTx = (CPeerlessTotalsEventTx*) bettingTx.get();

                        if (!bettingsViewCache.events->Exists(EventKey{plTotalsEventTx->nEventId}))
                            return error("CheckBettingTX: trying to create totals at not existed event id %lu!", plTotalsEventTx->nEventId);

                        /*
                        if (!CHECK_ODDS(plTotalsEventTx->nOverOdds))
                            return error("CheckBettingTX: invalid totals over odds %lu!", plTotalsEventTx->nOverOdds);

                        if (!CHECK_ODDS(plTotalsEventTx->nUnderOdds))
                            return error("CheckBettingTX: invalid totals under odds %lu!", plTotalsEventTx->nUnderOdds);
                        */

                        break;
                    }
                    case plEventPatchTxType:
                    {
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CPeerlessEventPatchTx* plEventPatchTx = (CPeerlessEventPatchTx*) bettingTx.get();

                        if (!bettingsViewCache.events->Exists(EventKey{plEventPatchTx->nEventId}))
                            return error("CheckBettingTX: trying to patch not existed event id %lu!", plEventPatchTx->nEventId);

                        break;
                    }
                    case plEventZeroingOddsTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) return error("CheckBettingTX: Spork is not active for EventZeroingOddsTx!");
                        if (!validOracleTx) return error("CheckBettingTX: Oracle tx from not oracle address!");

                        CPeerlessEventZeroingOddsTx* plEventZeroingOddsTx = (CPeerlessEventZeroingOddsTx*) bettingTx.get();

                        for (uint32_t eventId : plEventZeroingOddsTx->vEventIds) {
                            if (!bettingsViewCache.events->Exists(EventKey{eventId}))
                                return error("CheckBettingTX: trying to update not existed event id %lu!", eventId);
                        }

                        break;
                    }
                    default:
                        continue;
                }
            }
        }
    }
    return true;
}

void ProcessBettingTx(const CCoinsViewCache  &view, CBettingsView& bettingsViewCache, const CTransactionRef& tx, const CBlockIndex* pindex, const CBlock &block, const bool wagerrProtocolV3)
{
    const int64_t blockTime = block.GetBlockTime();
    LogPrint(BCLog::BETTING, "ProcessBettingTx: start, time: %lu, tx hash: %s\n", blockTime, tx->GetHash().GetHex());

    if (!HasOpReturnOutput(*tx)) return;

    const int height = pindex->nHeight;

    for (size_t i = 0; i < tx->vout.size(); i++) {
        const CTxOut &txOut = tx->vout[i];
        // parse betting TX
        auto bettingTx = ParseBettingTx(txOut);

        if (bettingTx == nullptr) continue;

        const CTxIn& txin{tx->vin[0]};
        CTxDestination address;
        CTxDestination prevAddr;
        uint256 hashBlock;
        CTransactionRef txPrev = GetTransaction(nullptr, nullptr, txin.prevout.hash, Params().GetConsensus(), hashBlock, true);
        // if we cant extract playerAddress - skip vout
        if (!txPrev || !ExtractDestination(txPrev->vout[txin.prevout.n].scriptPubKey, prevAddr)) {
            for (const auto& blockTx : block.vtx) {
                if (blockTx->GetHash() == txin.prevout.hash) {
                    if (!ExtractDestination(blockTx->vout[txin.prevout.n].scriptPubKey, prevAddr)) {
                        return;
                    } else {
                        txPrev = blockTx;
                        break;
                    }
                }
            }
        }
        address = prevAddr;

        CAmount betAmount{txOut.nValue};
        COutPoint outPoint{tx->GetHash(), (uint32_t) i};
        uint256 bettingTxId = SerializeHash(outPoint);

        switch(bettingTx->GetTxType()) {
            /* Player's tx types */
            case plBetTxType:
            {
                CPeerlessBetTx* betTx = (CPeerlessBetTx*) bettingTx.get();
                CPeerlessLegDB plBet{betTx->nEventId, (OutcomeType) betTx->nOutcome};
                CPeerlessExtendedEventDB plEvent, plCachedEvent;

                LogPrint(BCLog::BETTING, "CPeerlessBet: id: %lu, outcome: %lu\n", plBet.nEventId, plBet.nOutcome);
                // Find the event in DB
                EventKey eventKey{plBet.nEventId};
                // get locked event from upper level cache for getting correct odds
                if (bettingsView->events->Read(eventKey, plCachedEvent) &&
                        bettingsViewCache.events->Read(eventKey, plEvent)) {
                    CAmount payout = 0 * COIN;
                    CAmount burn = 0;

                    LogPrint(BCLog::BETTING, "plCachedEvent: homeOdds: %lu, awayOdds: %lu, drawOdds: %lu, spreadHomeOdds: %lu, spreadAwayOdds: %lu, totalOverOdds: %lu, totalUnderOdds: %lu\n",
                        plCachedEvent.nHomeOdds, plCachedEvent.nAwayOdds, plCachedEvent.nDrawOdds, plCachedEvent.nSpreadHomeOdds, plCachedEvent.nSpreadAwayOdds, plCachedEvent.nTotalOverOdds, plCachedEvent.nTotalUnderOdds);

                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});
                    // Check which outcome the bet was placed on and add to accumulators
                    switch (plBet.nOutcome) {
                        case moneyLineHomeWin:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nHomeOdds, payout, burn);

                            plEvent.nMoneyLineHomePotentialLiability += payout / COIN ;
                            plEvent.nMoneyLineHomeBets += 1;
                            break;
                        case moneyLineAwayWin:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nAwayOdds, payout, burn);

                            plEvent.nMoneyLineAwayPotentialLiability += payout / COIN ;
                            plEvent.nMoneyLineAwayBets += 1;
                            break;
                        case moneyLineDraw:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nDrawOdds, payout, burn);

                            plEvent.nMoneyLineDrawPotentialLiability += payout / COIN ;
                            plEvent.nMoneyLineDrawBets += 1;
                            break;
                        case spreadHome:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nSpreadHomeOdds, payout, burn);

                            plEvent.nSpreadHomePotentialLiability += payout / COIN ;
                            plEvent.nSpreadPushPotentialLiability += betAmount / COIN;
                            plEvent.nSpreadHomeBets += 1;
                            plEvent.nSpreadPushBets += 1;
                            break;
                        case spreadAway:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nSpreadAwayOdds, payout, burn);

                            plEvent.nSpreadAwayPotentialLiability += payout / COIN ;
                            plEvent.nSpreadPushPotentialLiability += betAmount / COIN;
                            plEvent.nSpreadAwayBets += 1;
                            plEvent.nSpreadPushBets += 1;
                            break;
                        case totalOver:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nTotalOverOdds, payout, burn);

                            plEvent.nTotalOverPotentialLiability += payout / COIN ;
                            plEvent.nTotalPushPotentialLiability += betAmount / COIN;
                            plEvent.nTotalOverBets += 1;
                            plEvent.nTotalPushBets += 1;
                            break;
                        case totalUnder:
                            CalculatePayoutBurnAmounts(betAmount, plCachedEvent.nTotalUnderOdds, payout, burn);

                            plEvent.nTotalUnderPotentialLiability += payout / COIN;
                            plEvent.nTotalPushPotentialLiability += betAmount / COIN;
                            plEvent.nTotalUnderBets += 1;
                            plEvent.nTotalPushBets += 1;
                            break;
                        default:
                            std::runtime_error("Unknown bet outcome type!");
                            break;
                    }
                    if (!bettingsViewCache.events->Update(eventKey, plEvent)) {
                        // should not happen ever
                        LogPrintf("Failed to update event!\n");
                        continue;
                    }

                    bettingsViewCache.bets->Write(PeerlessBetKey{static_cast<uint32_t>(height), outPoint}, CPeerlessBetDB(betAmount, address, {plBet}, {plCachedEvent}, blockTime));
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            case plParlayBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CPeerlessParlayBetTx* parlayBetTx = (CPeerlessParlayBetTx*) bettingTx.get();
                std::vector<CPeerlessBaseEventDB> lockedEvents;
                std::vector<CPeerlessLegDB> legs;
                // convert tx legs format to db
                LogPrint(BCLog::BETTING, "ParlayBet: legs: ");
                for (auto leg : parlayBetTx->legs) {
                    LogPrint(BCLog::BETTING, "(id: %lu, outcome: %lu), ", leg.nEventId, leg.nOutcome);
                    legs.emplace_back(leg.nEventId, (OutcomeType) leg.nOutcome);
                }
                LogPrint(BCLog::BETTING, "\n");

                std::vector<CBettingUndoDB> vUndos;
                for (const CPeerlessLegDB& leg : legs) {
                    CPeerlessExtendedEventDB plEvent, plCachedEvent;
                    EventKey eventKey{leg.nEventId};
                    // Find the event in DB
                    // get locked event from upper level cache of betting view for getting correct odds
                    if (bettingsView->events->Read(eventKey, plCachedEvent) &&
                            bettingsViewCache.events->Read(eventKey, plEvent)) {

                        LogPrint(BCLog::BETTING, "plCachedEvent: homeOdds: %lu, awayOdds: %lu, drawOdds: %lu, spreadHomeOdds: %lu, spreadAwayOdds: %lu, totalOverOdds: %lu, totalUnderOdds: %lu\n",
                            plCachedEvent.nHomeOdds, plCachedEvent.nAwayOdds, plCachedEvent.nDrawOdds, plCachedEvent.nSpreadHomeOdds, plCachedEvent.nSpreadAwayOdds, plCachedEvent.nTotalOverOdds, plCachedEvent.nTotalUnderOdds);

                        vUndos.emplace_back(BettingUndoVariant{plEvent}, (uint32_t)height);
                        switch (leg.nOutcome) {
                            case moneyLineHomeWin:
                                plEvent.nMoneyLineHomeBets += 1;
                                break;
                            case moneyLineAwayWin:
                                plEvent.nMoneyLineAwayBets += 1;
                                break;
                            case moneyLineDraw:
                                plEvent.nMoneyLineDrawBets += 1;
                                break;
                            case spreadHome:
                                plEvent.nSpreadHomeBets += 1;
                                plEvent.nSpreadPushBets += 1;
                                break;
                            case spreadAway:
                                plEvent.nSpreadAwayBets += 1;
                                plEvent.nSpreadPushBets += 1;
                                break;
                            case totalOver:
                                plEvent.nTotalOverBets += 1;
                                plEvent.nTotalPushBets += 1;
                                break;
                            case totalUnder:
                                plEvent.nTotalUnderBets += 1;
                                plEvent.nTotalPushBets += 1;
                                break;
                            default:
                                std::runtime_error("Unknown bet outcome type!");
                        }

                        lockedEvents.emplace_back(plCachedEvent);
                        bettingsViewCache.events->Update(eventKey, plEvent);
                    }
                    else {
                        LogPrintf("Failed to find event!\n");
                        continue;
                    }
                }
                if (!legs.empty()) {
                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(bettingTxId, vUndos);
                    bettingsViewCache.bets->Write(PeerlessBetKey{static_cast<uint32_t>(height), outPoint}, CPeerlessBetDB(betAmount, address, legs, lockedEvents, blockTime));
                }
                break;
            }
            case fBetTxType:
            {
                if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) break;

                CFieldBetTx* fBetTx = (CFieldBetTx*) bettingTx.get();
                LogPrint(BCLog::BETTING, "CFieldBet: eventId: %lu, contenderId: %lu marketType: %lu\n",
                    fBetTx->nEventId, fBetTx->nContenderId, fBetTx->nOutcome);

                CFieldEventDB fEvent, fCachedEvent;
                FieldEventKey fEventKey{fBetTx->nEventId};
                // get locked event from upper level cache for getting correct odds
                if (!bettingsView->fieldEvents->Read(fEventKey, fCachedEvent)) {
                    LogPrint(BCLog::BETTING, "Failed to find field event %lu in upper level cache!", fBetTx->nEventId);
                    break;
                }

                if (!bettingsViewCache.fieldEvents->Read(fEventKey, fEvent)) {
                    LogPrint(BCLog::BETTING, "Failed to find field event %lu!", fBetTx->nEventId);
                    break;
                }

                LogPrint(BCLog::BETTING, "fCachedEvent:\n");
                for (const auto& contender : fCachedEvent.contenders) {
                    LogPrint(BCLog::BETTING, "contenderId %lu : outright odds %lu place odds %lu show odds %lu\n",
                        contender.first, contender.second.nOutrightOdds, contender.second.nPlaceOdds, contender.second.nShowOdds);
                }

                // save prev event state to undo
                bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{fEvent}, (uint32_t)height}});

                CAmount payout;
                switch (fBetTx->nOutcome) {
                    case outright:
                    {
                        uint32_t effectiveOdds = CalculateEffectiveOdds(fCachedEvent.contenders[fBetTx->nContenderId].nOutrightOdds);
                        payout = betAmount * effectiveOdds / BET_ODDSDIVISOR;
                        fEvent.contenders[fBetTx->nContenderId].nOutrightPotentialLiability += payout / COIN;
                        fEvent.contenders[fBetTx->nContenderId].nOutrightBets += 1;
                        break;
                    }
                    case place:
                    {
                        uint32_t effectiveOdds = CalculateEffectiveOdds(fCachedEvent.contenders[fBetTx->nContenderId].nPlaceOdds);
                        payout = betAmount * effectiveOdds / BET_ODDSDIVISOR;
                        fEvent.contenders[fBetTx->nContenderId].nPlacePotentialLiability += payout / COIN;
                        fEvent.contenders[fBetTx->nContenderId].nPlaceBets += 1;
                        break;
                    }
                    case show:
                    {
                        uint32_t effectiveOdds = CalculateEffectiveOdds(fCachedEvent.contenders[fBetTx->nContenderId].nShowOdds);
                        payout = betAmount * effectiveOdds / BET_ODDSDIVISOR;
                        fEvent.contenders[fBetTx->nContenderId].nShowPotentialLiability += payout / COIN;
                        fEvent.contenders[fBetTx->nContenderId].nShowBets += 1;
                        break;
                    }
                }

                if (!bettingsViewCache.fieldEvents->Update(fEventKey, fEvent)) {
                    // should not happen ever
                    LogPrintf("Failed to update field event!\n");
                    break;
                }

                CFieldLegDB fLeg{fBetTx->nEventId, (FieldBetOutcomeType)fBetTx->nOutcome, fBetTx->nContenderId};
                if (!bettingsViewCache.fieldBets->Write(
                    FieldBetKey{static_cast<uint32_t>(height), outPoint},
                    CFieldBetDB(betAmount, address, {fLeg}, {fCachedEvent}, blockTime)))
                {
                    LogPrintf("Failed to write bet!\n");
                    break;
                }

                break;
            }
            case fParlayBetTxType:
            {
                if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) break;

                CFieldParlayBetTx* fParlayBetTx = (CFieldParlayBetTx*) bettingTx.get();
                std::vector<CFieldEventDB> lockedEvents;
                std::vector<CFieldLegDB> legs;

                // convert tx legs format to db
                LogPrint(BCLog::BETTING, "FieldParlayBet: legs: ");
                for (const auto& leg : fParlayBetTx->legs) {
                    LogPrint(BCLog::BETTING, "CFieldBet: eventId: %lu, contenderId: %lu marketType: %lu\n",
                        leg.nEventId, leg.nContenderId, leg.nOutcome);
                    legs.emplace_back(leg.nEventId, (FieldBetOutcomeType)leg.nOutcome, leg.nContenderId);
                }

                std::vector<CBettingUndoDB> vUndos;
                for (const auto& leg : legs) {
                    CFieldEventDB fEvent, fCachedEvent;
                    FieldEventKey fEventKey{leg.nEventId};
                    // get locked event from upper level cache for getting correct odds
                    if (!bettingsView->fieldEvents->Read(fEventKey, fCachedEvent)) {
                        LogPrint(BCLog::BETTING, "Failed to find field event %lu in upper level cache!", leg.nEventId);
                        continue;
                    }

                    if (!bettingsViewCache.fieldEvents->Read(fEventKey, fEvent)) {
                        LogPrint(BCLog::BETTING, "Failed to find field event %lu!", leg.nEventId);
                        continue;
                    }

                    LogPrint(BCLog::BETTING, "fCachedEvent:\n");
                    for (const auto& contender : fCachedEvent.contenders) {
                        LogPrint(BCLog::BETTING, "contenderId %lu : outright odds %lu place odds %lu show odds %lu\n",
                            contender.first, contender.second.nOutrightOdds, contender.second.nPlaceOdds, contender.second.nShowOdds);
                    }

                    lockedEvents.emplace_back(fCachedEvent);
                    vUndos.emplace_back(BettingUndoVariant{fEvent}, (uint32_t)height);

                    switch (leg.nOutcome) {
                        case outright:
                        {
                            fEvent.contenders[leg.nContenderId].nOutrightBets += 1;
                            break;
                        }
                        case place:
                        {
                            fEvent.contenders[leg.nContenderId].nPlaceBets += 1;
                            break;
                        }
                        case show:
                        {
                            fEvent.contenders[leg.nContenderId].nShowBets += 1;
                            break;
                        }
                    }

                    bettingsViewCache.fieldEvents->Update(fEventKey, fEvent);
                }

                if (!legs.empty()) {
                    // save prev event state to undo
                    bettingsViewCache.SaveBettingUndo(bettingTxId, vUndos);
                    bettingsViewCache.fieldBets->Write(
                        FieldBetKey{static_cast<uint32_t>(height), outPoint},
                        CFieldBetDB(betAmount, address, legs, lockedEvents, blockTime)
                    );
                }

                break;
            }
            case cgBetTxType:
            {
                if (!wagerrProtocolV3) break;
                if (height >= Params().GetConsensus().QuickGamesEndHeight()) {
                    LogPrintf("ProcessBettingTx : Chain games transactions are disabled\n");
                    break;
                }

                CChainGamesBetTx* cgBetTx = (CChainGamesBetTx*) bettingTx.get();

                LogPrint(BCLog::BETTING, "CChainGamesBetTx: nEventId: %lu,", cgBetTx->nEventId);
                if (!bettingsView->chainGamesLottoEvents->Exists(EventKey{cgBetTx->nEventId})) {
                    LogPrintf("Failed to find event!\n");
                    continue;
                }

                if (!bettingsViewCache.chainGamesLottoBets->Write(
                        ChainGamesBetKey{static_cast<uint32_t>(height), outPoint},
                        CChainGamesBetDB{cgBetTx->nEventId, betAmount, address, blockTime})) {
                    LogPrintf("Failed to write bet!\n");
                    continue;
                }
                break;
            }
            case qgBetTxType:
            {
                if (!wagerrProtocolV3) break;
                if (height >= Params().GetConsensus().QuickGamesEndHeight()) {
                    LogPrintf("ProcessBettingTx : Chain games transactions are disabled\n");
                    break;
                }

                CQuickGamesBetTx* qgBetTx = (CQuickGamesBetTx*) bettingTx.get();

                LogPrint(BCLog::BETTING, "CQuickGamesBetTx: gameType: %d, betInfo: %s\n", qgBetTx->gameType, std::string(qgBetTx->vBetInfo.begin(), qgBetTx->vBetInfo.end()));
                if (!bettingsViewCache.quickGamesBets->Write(
                        QuickGamesBetKey{static_cast<uint32_t>(height), outPoint},
                        CQuickGamesBetDB{ (QuickGamesType) qgBetTx->gameType, qgBetTx->vBetInfo, betAmount, address, blockTime})) {
                    LogPrintf("Failed to write bet!\n");
                }
                break;
            }
            default:
            {
                // Ensure the event TX has come from Oracle wallet.
                const bool validOracleTx{IsValidOraclePrevTxOut(txPrev->vout[txin.prevout.n], height)};
                if (!validOracleTx) break;

                /* Oracle's tx types */
                switch(bettingTx->GetTxType()) {
                    case mappingTxType:
                    {
                        if (!validOracleTx) break;

                        CMappingTx* mapTx = (CMappingTx*) bettingTx.get();

                        auto mappingType = MappingType(mapTx->nMType);
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight() &&
                        (mappingType == individualSportMapping || mappingType == contenderMapping ) )
                        {
                            break;
                        }

                        LogPrint(BCLog::BETTING, "CMapping: type: %lu, id: %lu, name: %s\n", mapTx->nMType, mapTx->nId, mapTx->sName);
                        if (!bettingsViewCache.mappings->Write(MappingKey{mappingType, mapTx->nId}, CMappingDB{mapTx->sName})) {
                            if (!wagerrProtocolV3) {
                                // save failed tx to db, for avoiding undo issues
                                bettingsViewCache.SaveFailedTx(bettingTxId);
                            }
                            LogPrintf("Failed to write new mapping!\n");
                        }
                        break;
                    }
                    case plEventTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessEventTx* plEventTx = (CPeerlessEventTx*) bettingTx.get();

                        LogPrint(BCLog::BETTING, "CPeerlessEvent: id: %lu, sport: %lu, tournament: %lu, stage: %lu,\n\t\t\thome: %lu, away: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n",
                            plEventTx->nEventId,
                            plEventTx->nSport,
                            plEventTx->nTournament,
                            plEventTx->nStage,
                            plEventTx->nHomeTeam,
                            plEventTx->nAwayTeam,
                            plEventTx->nHomeOdds,
                            plEventTx->nAwayOdds,
                            plEventTx->nDrawOdds);

                        CPeerlessExtendedEventDB plEvent;
                        plEvent.ExtractDataFromTx(*plEventTx);

                        if (!wagerrProtocolV3) {
                            plEvent.nEventCreationHeight = height;
                            plEvent.fLegacyInitialHomeFavorite =  plEventTx->nHomeOdds < plEventTx->nAwayOdds ? true : false;
                        }

                        EventKey eventKey{plEvent.nEventId};

                        if (!bettingsViewCache.events->Write(eventKey, plEvent)) {
                            CPeerlessExtendedEventDB plEventToPatch;
                            if (!wagerrProtocolV3 &&
                                    bettingsViewCache.events->Read(eventKey, plEventToPatch)) {
                                LogPrint(BCLog::BETTING, "CPeerlessEvent - Legacy - try to patch with new event data: id: %lu, time: %lu\n", plEvent.nEventId, plEvent.nStartTime);
                                // save prev event state to undo
                                bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEventToPatch}, (uint32_t)height}});

                                plEventToPatch.nStartTime = plEvent.nStartTime;
                                plEventToPatch.nSport = plEvent.nSport;
                                plEventToPatch.nTournament = plEvent.nTournament;
                                plEventToPatch.nStage = plEvent.nStage;
                                plEventToPatch.nHomeTeam = plEvent.nHomeTeam;
                                plEventToPatch.nAwayTeam = plEvent.nAwayTeam;
                                plEventToPatch.nHomeOdds = plEvent.nHomeOdds;
                                plEventToPatch.nAwayOdds = plEvent.nAwayOdds;
                                plEventToPatch.nDrawOdds = plEvent.nDrawOdds;

                                if (!bettingsViewCache.events->Update(eventKey, plEventToPatch)) {
                                    // should not happen ever
                                    LogPrintf("Failed to update event!\n");
                                }
                            } else {
                                if (!wagerrProtocolV3) {
                                    // save failed tx to db, for avoiding undo issues
                                    bettingsViewCache.SaveFailedTx(bettingTxId);
                                }
                                LogPrintf("Failed to write new event!\n");
                            }
                        }
                        break;
                    }
                    case fEventTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) break;
                        if (!validOracleTx) break;

                        CFieldEventTx* fEventTx = (CFieldEventTx*) bettingTx.get();
                        LogPrint(BCLog::BETTING, "CFieldEventTx: id: %lu, sport: %lu, tournament: %lu, stage: %lu, subgroup: %lu, marketType: %lu\n",
                            fEventTx->nEventId,
                            fEventTx->nSport,
                            fEventTx->nTournament,
                            fEventTx->nStage,
                            fEventTx->nGroupType,
                            fEventTx->nMarketType
                        );
                        for (auto& contender : fEventTx->mContendersInputOdds) {
                            LogPrint(BCLog::BETTING, "%lu : %lu\n", contender.first, contender.second);
                        }

                        CFieldEventDB fEvent;
                        fEvent.ExtractDataFromTx(*fEventTx);
                        fEvent.CalcOdds();

                        FieldEventKey eventKey{fEvent.nEventId};
                        if (!bettingsViewCache.fieldEvents->Write(eventKey, fEvent))
                            LogPrintf("Failed to write new event!\n");

                        break;
                    }
                    case fUpdateOddsTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) break;
                        if (!validOracleTx) break;

                        CFieldUpdateOddsTx* fUpdateOddsTx = (CFieldUpdateOddsTx*) bettingTx.get();
                        LogPrint(BCLog::BETTING, "CFieldUpdateOddsTx: id: %lu\n", fUpdateOddsTx->nEventId);
                        for (auto& contender : fUpdateOddsTx->mContendersInputOdds) {
                            LogPrint(BCLog::BETTING, "%lu : %lu\n", contender.first, contender.second);
                        }

                        FieldEventKey fEventKey{fUpdateOddsTx->nEventId};
                        CFieldEventDB fEvent;
                        if (bettingsViewCache.fieldEvents->Read(fEventKey, fEvent)) {
                            // save prev event state to undo
                            bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{fEvent}, (uint32_t)height}});

                            fEvent.ExtractDataFromTx(*fUpdateOddsTx);
                            fEvent.CalcOdds();

                            if (!bettingsViewCache.fieldEvents->Update(fEventKey, fEvent))
                                LogPrintf("Failed to update field event!\n");
                        }
                        else {
                            LogPrintf("Failed to find field event!\n");
                        }

                        break;
                    }
                    case fUpdateModifiersTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) break;
                        if (!validOracleTx) break;

                        CFieldUpdateModifiersTx* fUpdateModifiersTx = (CFieldUpdateModifiersTx*) bettingTx.get();
                        LogPrint(BCLog::BETTING, "CFieldUpdateModifiersTx: id: %lu\n", fUpdateModifiersTx->nEventId);
                        for (auto& contender : fUpdateModifiersTx->mContendersModifires) {
                            LogPrint(BCLog::BETTING, "%lu : %lu\n", contender.first, contender.second);
                        }

                        FieldEventKey fEventKey{fUpdateModifiersTx->nEventId};
                        CFieldEventDB fEvent;
                        if (bettingsViewCache.fieldEvents->Read(fEventKey, fEvent)) {
                            // save prev event state to undo
                            bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{fEvent}, (uint32_t)height}});

                            fEvent.ExtractDataFromTx(*fUpdateModifiersTx);
                            fEvent.CalcOdds();

                            if (!bettingsViewCache.fieldEvents->Update(fEventKey, fEvent))
                                LogPrintf("Failed to update field event!\n");
                        }
                        else {
                            LogPrintf("Failed to find field event!\n");
                        }

                        break;
                    }
                    case fUpdateMarginTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) break;
                        if (!validOracleTx) break;

                        CFieldUpdateMarginTx* fUpdateMarginTx = (CFieldUpdateMarginTx*) bettingTx.get();

                        FieldEventKey fEventKey{fUpdateMarginTx->nEventId};
                        CFieldEventDB fEvent;
                        if (bettingsViewCache.fieldEvents->Read(fEventKey, fEvent)) {
                            // save prev event state to undo
                            bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{fEvent}, (uint32_t)height}});

                            fEvent.ExtractDataFromTx(*fUpdateMarginTx);
                            fEvent.CalcOdds();

                            if (!bettingsViewCache.fieldEvents->Update(fEventKey, fEvent))
                                LogPrintf("Failed to update field event!\n");
                        }
                        else {
                            LogPrintf("Failed to find field event!\n");
                        }

                        break;
                    }
                    case fZeroingOddsTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) break;
                        if (!validOracleTx) break;

                        CFieldZeroingOddsTx* fZeroingOddsTx = (CFieldZeroingOddsTx*) bettingTx.get();
                        LogPrint(BCLog::BETTING, "CFieldZeroingOddsTx: id: %lu\n", fZeroingOddsTx->nEventId);

                        FieldEventKey fEventKey{fZeroingOddsTx->nEventId};
                        CFieldEventDB fEvent;
                        if (bettingsViewCache.fieldEvents->Read(fEventKey, fEvent)) {
                            // save prev event state to undo
                            bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{fEvent}, (uint32_t)height}});

                            for (auto& contender : fEvent.contenders) {
                                contender.second.nInputOdds = 0;
                                contender.second.nOutrightOdds = 0;
                                contender.second.nPlaceOdds = 0;
                                contender.second.nShowOdds = 0;
                            }

                            if (!bettingsViewCache.fieldEvents->Update(fEventKey, fEvent))
                                LogPrintf("Failed to update field event!\n");
                        }
                        else {
                            LogPrintf("Failed to find field event!\n");
                        }

                        break;
                    }
                    case fResultTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) break;
                        if (!validOracleTx) break;

                        CFieldResultTx* fResultTx = (CFieldResultTx*) bettingTx.get();

                        LogPrint(BCLog::BETTING, "CFieldResultTx: id: %lu, resultType: %lu\n", fResultTx->nEventId, fResultTx->nResultType);
                        for (auto& contender : fResultTx->contendersResults) {
                            LogPrint(BCLog::BETTING, "id %lu : place %lu\n", contender.first, contender.second);
                        }

                        CFieldEventDB fieldEvent;
                        if (!bettingsViewCache.fieldEvents->Read(FieldEventKey{fResultTx->nEventId}, fieldEvent)) {
                            LogPrintf("Failed to find field event!\n");
                            break;
                        }

                        CFieldResultDB fEventResult{fResultTx->nEventId, fResultTx->nResultType};
                        for (auto& contender : fieldEvent.contenders) {
                            if (fResultTx->contendersResults.find(contender.first) != fResultTx->contendersResults.end()) {
                                fEventResult.contendersResults.emplace(contender.first, fResultTx->contendersResults[contender.first]);
                            }
                            else {
                                fEventResult.contendersResults.emplace(contender.first, ContenderResult::DNF);
                            }
                        }

                        if (!bettingsViewCache.fieldResults->Write(FieldResultKey{fEventResult.nEventId}, fEventResult)) {
                            LogPrintf("Failed to write field result!\n");
                            break;
                        }

                        break;
                    }
                    case plResultTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessResultTx* plResultTx = (CPeerlessResultTx*) bettingTx.get();

                        LogPrint(BCLog::BETTING, "CPeerlessResult: id: %lu, resultType: %lu, homeScore: %lu, awayScore: %lu\n",
                            plResultTx->nEventId, plResultTx->nResultType, plResultTx->nHomeScore, plResultTx->nAwayScore);

                        CPeerlessResultDB plResult{plResultTx->nEventId, plResultTx->nResultType, plResultTx->nHomeScore, plResultTx->nAwayScore};

                        if (!bettingsViewCache.events->Exists(EventKey{plResult.nEventId})) {
                            if (!wagerrProtocolV3) {
                                // save failed tx to db, for avoiding undo issues
                                bettingsViewCache.SaveFailedTx(bettingTxId);
                            }
                            LogPrintf("Failed to find event!\n");
                            break;
                        }

                        if (!bettingsViewCache.results->Write(ResultKey{plResult.nEventId}, plResult)) {
                            if (!wagerrProtocolV3) {
                                // save failed tx to db, for avoiding undo issues
                                bettingsViewCache.SaveFailedTx(bettingTxId);
                            }
                            LogPrintf("Failed to write result!\n");
                            break;
                        }
                        break;
                    }
                    case plUpdateOddsTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessUpdateOddsTx* plUpdateOddsTx = (CPeerlessUpdateOddsTx*) bettingTx.get();

                        LogPrint(BCLog::BETTING, "CPeerlessUpdateOdds: id: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n", plUpdateOddsTx->nEventId, plUpdateOddsTx->nHomeOdds, plUpdateOddsTx->nAwayOdds, plUpdateOddsTx->nDrawOdds);

                        EventKey eventKey{plUpdateOddsTx->nEventId};
                        CPeerlessExtendedEventDB plEvent;
                        // First check a peerless event exists in DB.
                        if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                            // save prev event state to undo
                            bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});

                            plEvent.ExtractDataFromTx(*plUpdateOddsTx);

                            // Update the event in the DB.
                            if (!bettingsViewCache.events->Update(eventKey, plEvent))
                                LogPrintf("Failed to update event!\n");
                        }
                        else {
                            if (!wagerrProtocolV3) {
                                // save failed tx to db, for avoiding undo issues
                                bettingsViewCache.SaveFailedTx(bettingTxId);
                            }
                            LogPrintf("Failed to find event!\n");
                        }
                        break;
                    }
                    case cgEventTxType:
                    {
                        if (!wagerrProtocolV3) break;
                        if (height >= Params().GetConsensus().QuickGamesEndHeight()) {
                            LogPrintf("ProcessBettingTx : Chain games transactions are disabled\n");
                            break;
                        }
                        CChainGamesEventTx* cgEventTx = (CChainGamesEventTx*) bettingTx.get();

                        LogPrint(BCLog::BETTING, "CChainGamesEventTx: nEventId: %d, nEntryFee: %d\n", cgEventTx->nEventId, cgEventTx->nEntryFee);

                        EventKey eventKey{cgEventTx->nEventId};
                        if (!bettingsViewCache.chainGamesLottoEvents->Write(
                                eventKey,
                                CChainGamesEventDB{ cgEventTx->nEventId, cgEventTx->nEntryFee })) {
                            LogPrintf("Failed to write new event!\n");
                            break;
                        }
                        break;
                    }
                    case cgResultTxType:
                    {
                        if (!wagerrProtocolV3) break;
                        if (height >= Params().GetConsensus().QuickGamesEndHeight()) {
                            LogPrintf("ProcessBettingTx : Chain games transactions are disabled\n");
                            break;
                        }

                        CChainGamesResultTx* cgResultTx = (CChainGamesResultTx*) bettingTx.get();

                        LogPrint(BCLog::BETTING, "CChainGamesResultTx: nEventId: %d\n", cgResultTx->nEventId);

                        if (!bettingsViewCache.chainGamesLottoEvents->Exists(EventKey{cgResultTx->nEventId})) {
                            LogPrintf("Failed to find event!\n");
                            break;
                        }
                        if (!bettingsViewCache.chainGamesLottoResults->Write(
                                ResultKey{cgResultTx->nEventId},
                                CChainGamesResultDB{ cgResultTx->nEventId })) {
                            LogPrintf("Failed to write result!\n");
                            break;
                        }
                        break;
                    }
                    case plSpreadsEventTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessSpreadsEventTx* plSpreadsEventTx = (CPeerlessSpreadsEventTx*) bettingTx.get();

                        LogPrint(BCLog::BETTING, "CPeerlessSpreadsEvent: id: %lu, spreadPoints: %lu, homeOdds: %lu, awayOdds: %lu\n",
                            plSpreadsEventTx->nEventId, plSpreadsEventTx->nPoints, plSpreadsEventTx->nHomeOdds, plSpreadsEventTx->nAwayOdds);

                        CPeerlessExtendedEventDB plEvent;
                        EventKey eventKey{plSpreadsEventTx->nEventId};
                        // First check a peerless event exists in the event index.
                        if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                            // save prev event state to undo
                            bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});

                            plEvent.ExtractDataFromTx(*plSpreadsEventTx);
                            // Update the event in the DB.
                            if (!bettingsViewCache.events->Update(eventKey, plEvent))
                                LogPrintf("Failed to update event!\n");
                        }
                        else {
                            if (!wagerrProtocolV3) {
                                // save failed tx to db, for avoiding undo issues
                                bettingsViewCache.SaveFailedTx(bettingTxId);
                            }
                            LogPrintf("Failed to find event!\n");
                        }
                        break;
                    }
                    case plTotalsEventTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessTotalsEventTx* plTotalsEventTx = (CPeerlessTotalsEventTx*) bettingTx.get();

                        LogPrint(BCLog::BETTING, "CPeerlessTotalsEvent: id: %lu, totalPoints: %lu, overOdds: %lu, underOdds: %lu\n",
                            plTotalsEventTx->nEventId, plTotalsEventTx->nPoints, plTotalsEventTx->nOverOdds, plTotalsEventTx->nUnderOdds);

                        CPeerlessExtendedEventDB plEvent;
                        EventKey eventKey{plTotalsEventTx->nEventId};
                        // First check a peerless event exists in the event index.
                        if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                            // save prev event state to undo
                            bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});

                            plEvent.ExtractDataFromTx(*plTotalsEventTx);

                            // Update the event in the DB.
                            if (!bettingsViewCache.events->Update(eventKey, plEvent))
                                LogPrintf("Failed to update event!\n");
                        }
                        else {
                            if (!wagerrProtocolV3) {
                                // save failed tx to db, for avoiding undo issues
                                bettingsViewCache.SaveFailedTx(bettingTxId);
                            }
                            LogPrintf("Failed to find event!\n");
                        }
                        break;
                    }
                    case plEventPatchTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessEventPatchTx* plEventPatchTx = (CPeerlessEventPatchTx*) bettingTx.get();
                        LogPrint(BCLog::BETTING, "CPeerlessEventPatch: id: %lu, time: %lu\n", plEventPatchTx->nEventId, plEventPatchTx->nStartTime);
                        CPeerlessExtendedEventDB plEvent;
                        EventKey eventKey{plEventPatchTx->nEventId};
                        // First check a peerless event exists in DB
                        if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                            // save prev event state to undo
                            bettingsViewCache.SaveBettingUndo(bettingTxId, {CBettingUndoDB{BettingUndoVariant{plEvent}, (uint32_t)height}});

                            plEvent.ExtractDataFromTx(*plEventPatchTx);

                            if (!bettingsViewCache.events->Update(eventKey, plEvent))
                                LogPrintf("Failed to update event!\n");
                        }
                        else {
                            if (!wagerrProtocolV3) {
                                // save failed tx to db, for avoiding undo issues
                                bettingsViewCache.SaveFailedTx(bettingTxId);
                            }
                            LogPrintf("Failed to find event!\n");
                        }
                        break;
                    }
                    case plEventZeroingOddsTxType:
                    {
                        if (::ChainActive().Height() < Params().GetConsensus().WagerrProtocolV4StartHeight()) break;
                        if (!validOracleTx) break;

                        CPeerlessEventZeroingOddsTx* plEventZeroingOddsTx = (CPeerlessEventZeroingOddsTx*) bettingTx.get();

                        std::stringstream eventIdsStream;
                        std::vector<CBettingUndoDB> vUndos;
                        for (uint32_t eventId : plEventZeroingOddsTx->vEventIds) {
                            eventIdsStream << eventId << " ";
                            EventKey eventKey{eventId};
                            CPeerlessExtendedEventDB plEvent;
                            // Check a peerless event exists in DB.
                            if (bettingsViewCache.events->Read(eventKey, plEvent)) {
                                // save prev event state to undo
                                vUndos.emplace_back(BettingUndoVariant{plEvent}, (uint32_t)height);

                                plEvent.nHomeOdds = 0;
                                plEvent.nAwayOdds = 0;
                                plEvent.nDrawOdds = 0;
                                plEvent.nSpreadHomeOdds = 0;
                                plEvent.nSpreadAwayOdds = 0;
                                plEvent.nTotalOverOdds  = 0;
                                plEvent.nTotalUnderOdds = 0;

                                // Update the event in the DB.
                                if (!bettingsViewCache.events->Update(eventKey, plEvent))
                                    LogPrintf("Failed to update event!\n");
                            }
                        }

                        LogPrint(BCLog::BETTING, "CPeerlessEventZeroingOddsTx: ids: %s,\n", eventIdsStream.str());

                        if (!vUndos.empty()) {
                            bettingsViewCache.SaveBettingUndo(bettingTxId, vUndos);
                        }

                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }
    LogPrint(BCLog::BETTING, "ProcessBettingTx: end\n");
}

CAmount GetBettingPayouts(const CCoinsViewCache &view, CBettingsView& bettingsViewCache, const int nNewBlockHeight, std::multimap<CPayoutInfoDB, CBetOut>& mExpectedPayouts)
{
    CAmount expectedMint = 0;
    std::vector<CBetOut> vExpectedPayouts;
    std::vector<CPayoutInfoDB> vPayoutsInfo;

    // Get the previous block so we can look for any results in it.
    CBlockIndex *resultsBocksIndex = NULL;
    resultsBocksIndex = ::ChainActive()[nNewBlockHeight-1];

    CBlock block;
    bool fFoundBlock = ReadBlockFromDisk(block, resultsBocksIndex, Params().GetConsensus());
    if (!fFoundBlock) {
        LogPrint(BCLog::BETTING, "Unable to read block at height %d\n", resultsBocksIndex->nHeight);
        return 0;
    }

    // Get the PL and CG bet payout TX's so we can calculate the winning bet vector which is used to mint coins and payout bets.
    switch (Params().GetConsensus().GetWBPVersion(nNewBlockHeight)) {
        case WBP05:
            //GetPLBetPayoutsV5(bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
            break;
        case WBP04:
            // V4 events are handled as a special case of the V3 protocol
            GetPLBetPayoutsV3(view, block, bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
            GetCGLottoBetPayoutsV3(block, view, bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
            GetQuickGamesBetPayouts(bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
            // collect field bets payouts
            GetFieldBetPayoutsV4(view, bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
            break;
        case WBP03:
            // V4 events are handled as a special case of the V3 protocol
            GetPLBetPayoutsV3(view, block, bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
            GetCGLottoBetPayoutsV3(block, view, bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
            GetQuickGamesBetPayouts(bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
            break;
        case WBP02:
            GetPLBetPayoutsV3(view, block, bettingsViewCache, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
            GetCGLottoBetPayoutsV2(block, view, nNewBlockHeight, vExpectedPayouts, vPayoutsInfo);
        case WBP01:
        default:
            break;
    }

    assert(vExpectedPayouts.size() == vPayoutsInfo.size());
    mExpectedPayouts.clear();

    for (unsigned int i = 0; i < vExpectedPayouts.size(); i++) {
        expectedMint += vExpectedPayouts[i].nValue;
        mExpectedPayouts.insert(std::pair<const CPayoutInfoDB, CBetOut>(vPayoutsInfo[i], vExpectedPayouts[i]));
    }

    return expectedMint;
}

/*
 * Undo betting
 */

bool UndoEventChanges(CBettingsView& bettingsViewCache, const BettingUndoKey& undoKey, const uint32_t height)
{
    std::vector<CBettingUndoDB> vUndos = bettingsViewCache.GetBettingUndo(undoKey);
    for (auto undo : vUndos) {
        // undo data is inconsistent
        if (!undo.Inited() || undo.height != height) {
            std::runtime_error("Invalid undo state!");
        }

        switch (undo.Get().which()) {
            case UndoPeerlessEvent:
            {
                CPeerlessExtendedEventDB event = boost::get<CPeerlessExtendedEventDB>(undo.Get());
                LogPrint(BCLog::BETTING, "UndoEventChanges: CPeerlessEvent: id: %lu, sport: %lu, tournament: %lu, stage: %lu,\n\t\t\thome: %lu, away: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu favorite: %s\n",
                    event.nEventId,
                    event.nSport,
                    event.nTournament,
                    event.nStage,
                    event.nHomeTeam,
                    event.nAwayTeam,
                    event.nHomeOdds,
                    event.nAwayOdds,
                    event.nDrawOdds,
                    event.fLegacyInitialHomeFavorite ? "home" : "away"
                );

                if (!bettingsViewCache.events->Update(EventKey{event.nEventId}, event)) {
                    std::runtime_error("Couldn't revert event when undo!");
                }

                break;
            }
            case UndoFieldEvent:
            {
                CFieldEventDB event = boost::get<CFieldEventDB>(undo.Get());
                LogPrint(BCLog::BETTING, "UndoFieldEventChanges: CFieldEventDB: id: %lu, group: %lu, sport: %lu, tournament: %lu, stage: %lu\n",
                    event.nEventId,
                    event.nGroupType,
                    event.nSport,
                    event.nTournament,
                    event.nStage
                );

                if (!bettingsViewCache.fieldEvents->Update(FieldEventKey{event.nEventId}, event)) {
                    std::runtime_error("Couldn't revert event when undo!");
                }

                break;
            }
            default:
                std::runtime_error("Invalid undo state!");
        }
    }

    return bettingsViewCache.EraseBettingUndo(undoKey);
}

bool UndoBettingTx(const CCoinsViewCache &view, CBettingsView& bettingsViewCache, const CTransactionRef& tx, const uint32_t height)
{
    LogPrintf("UndoBettingTx: start undo, block heigth %lu, tx hash %s\n", height, tx->GetHash().GetHex());

    bool wagerrProtocolV3 = height >= (uint32_t)Params().GetConsensus().WagerrProtocolV3StartHeight();
    bool wagerrProtocolV4 = height >= (uint32_t)Params().GetConsensus().WagerrProtocolV4StartHeight();

    // undo changes in back order
    for (int i = tx->vout.size() - 1; i >= 0 ; i--) {
        const CTxOut &txOut = tx->vout[i];
        // parse betting TX
        auto bettingTx = ParseBettingTx(txOut);

        if (bettingTx == nullptr) continue;

        COutPoint outPoint{tx->GetHash(), (uint32_t) i};
        uint256 bettingTxId = SerializeHash(outPoint);

        if (!wagerrProtocolV3 && bettingsViewCache.ExistFailedTx(bettingTxId)) {
            // failed tx, just skip it
            bettingsViewCache.EraseFailedTx(bettingTxId);
            continue;
        }

        switch(bettingTx->GetTxType()) {
            /* Player's tx types */
            case plBetTxType:
            {
                CPeerlessBetTx* betTx = (CPeerlessBetTx*) bettingTx.get();
                CPeerlessLegDB plBet{betTx->nEventId, (OutcomeType) betTx->nOutcome};
                CPeerlessExtendedEventDB plEvent, lockedEvent;

                LogPrintf("CPeerlessBet: id: %lu, outcome: %lu\n", plBet.nEventId, plBet.nOutcome);

                if (bettingsViewCache.events->Exists(EventKey{plBet.nEventId})) {
                    if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    // erase bet from db
                    PeerlessBetKey key{static_cast<uint32_t>(height), outPoint};
                    bettingsViewCache.bets->Erase(key);
                }
                else {
                    LogPrintf("Failed to find event!\n");
                }
                break;
            }
            case plParlayBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CPeerlessParlayBetTx* parlayBetTx = (CPeerlessParlayBetTx*) bettingTx.get();
                std::vector<CPeerlessLegDB> legs;
                // convert tx legs format to db
                LogPrintf("ParlayBet: legs: ");
                for (auto leg : parlayBetTx->legs) {
                    LogPrintf("(id: %lu, outcome: %lu), ", leg.nEventId, leg.nOutcome);
                    legs.emplace_back(leg.nEventId, (OutcomeType) leg.nOutcome);
                }
                LogPrintf("\n");

                bool allEventsExist = true;

                for (auto leg : legs) {
                    if (!bettingsViewCache.events->Exists(EventKey{leg.nEventId})) {
                        LogPrintf("Failed to find event!\n");
                        allEventsExist = false;
                        break;
                    }
                }

                if (!legs.empty() && allEventsExist) {
                    if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }
                    // erase bet from db
                    PeerlessBetKey key{static_cast<uint32_t>(height), outPoint};
                    bettingsViewCache.bets->Erase(key);
                }

                break;
            }
            case fBetTxType:
            {
                if (!wagerrProtocolV4) break;

                CFieldBetTx* fBetTx = (CFieldBetTx*) bettingTx.get();
                LogPrint(BCLog::BETTING, "CFieldBet: eventId: %lu, contenderId: %lu marketType: %lu\n",
                    fBetTx->nEventId, fBetTx->nContenderId, fBetTx->nOutcome);

                if (!bettingsViewCache.fieldEvents->Exists(FieldEventKey{fBetTx->nEventId})) {
                    LogPrintf("Failed to find event!\n");
                    break;
                }

                if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                    LogPrintf("Revert failed!\n");
                    return false;
                }

                bettingsViewCache.fieldBets->Erase(FieldBetKey{static_cast<uint32_t>(height), outPoint});

                break;
            }
            case fParlayBetTxType:
            {
                if (!wagerrProtocolV4) break;

                CFieldParlayBetTx* fParlayBetTx = (CFieldParlayBetTx*) bettingTx.get();
                std::vector<CFieldLegDB> legs;

                LogPrint(BCLog::BETTING, "FieldParlayBet: legs: ");
                for (const auto& leg : fParlayBetTx->legs) {
                    LogPrint(BCLog::BETTING, "CFieldBet: eventId: %lu, contenderId: %lu marketType: %lu\n",
                        leg.nEventId, leg.nContenderId, leg.nOutcome);
                    legs.emplace_back(leg.nEventId, (FieldBetOutcomeType)leg.nOutcome, leg.nContenderId);
                }

                bool allEventsExist = true;
                for (const auto& leg : legs) {
                    if (!bettingsViewCache.fieldEvents->Exists(FieldEventKey{leg.nEventId})) {
                        LogPrint(BCLog::BETTING, "Failed to find event %lu!\n", leg.nEventId);
                        allEventsExist = false;
                        break;
                    }
                }

                if (!legs.empty() && allEventsExist) {
                    if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                        LogPrintf("Revert failed!\n");
                        return false;
                    }

                    bettingsViewCache.fieldBets->Erase(FieldBetKey{static_cast<uint32_t>(height), outPoint});
                }

                break;
            }
            case cgBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CChainGamesBetTx* cgBetTx = (CChainGamesBetTx*) bettingTx.get();

                LogPrintf("CChainGamesBetTx: nEventId: %d\n", cgBetTx->nEventId);

                if (!bettingsView->chainGamesLottoEvents->Exists(EventKey{cgBetTx->nEventId})) {
                    LogPrintf("Failed to find event!\n");
                    continue;
                }

                if (!bettingsViewCache.chainGamesLottoBets->Erase(ChainGamesBetKey{static_cast<uint32_t>(height), outPoint})) {
                    LogPrintf("Revert failed!\n");
                    return false;
                }
                break;
            }
            case qgBetTxType:
            {
                if (!wagerrProtocolV3) break;

                CQuickGamesBetTx* qgBetTx = (CQuickGamesBetTx*) bettingTx.get();

                LogPrintf("CQuickGamesBetTx: gameType: %d, betInfo: %s\n", qgBetTx->gameType, std::string(qgBetTx->vBetInfo.begin(), qgBetTx->vBetInfo.end()));

                if (!bettingsViewCache.quickGamesBets->Erase(QuickGamesBetKey{static_cast<uint32_t>(height), outPoint})) {
                    LogPrintf("Revert failed!\n");
                    return false;
                }
                break;
            }
            default:
            {

                // Ensure the event TX has come from Oracle wallet.
                const bool validOracleTx{IsValidOracleTx(view, tx, height)};
                if (!validOracleTx) break;

                /* Oracle's tx types */
                switch(bettingTx->GetTxType()){
                    case mappingTxType:
                    {
                        if (!validOracleTx) break;

                        CMappingTx* mapTx = (CMappingTx*) bettingTx.get();
                        auto mappingType = MappingType(mapTx->nMType);
                        if ((mappingType == individualSportMapping || mappingType == contenderMapping ) &&
                            !wagerrProtocolV4)
                        {
                            return error("CheckBettingTX: Spork is not active for mapping type %lu!", mappingType);
                        }

                        LogPrintf("CMapping: type: %lu, id: %lu, name: %s\n", mapTx->nMType, mapTx->nId, mapTx->sName);

                        MappingKey key{mappingType, mapTx->nId};
                        if (bettingsViewCache.mappings->Exists(key)) {
                            if (!bettingsViewCache.mappings->Erase(key)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        break;
                    }
                    case plEventTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessEventTx* plEventTx = (CPeerlessEventTx*) bettingTx.get();

                        LogPrintf("CPeerlessEvent: id: %lu, sport: %lu, tournament: %lu, stage: %lu,\n\t\t\thome: %lu, away: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n",
                            plEventTx->nEventId,
                            plEventTx->nSport,
                            plEventTx->nTournament,
                            plEventTx->nStage,
                            plEventTx->nHomeTeam,
                            plEventTx->nAwayTeam,
                            plEventTx->nHomeOdds,
                            plEventTx->nAwayOdds,
                            plEventTx->nDrawOdds);

                        if (bettingsViewCache.events->Exists(EventKey{plEventTx->nEventId})) {
                            // try to undo legacy event patch
                            if (!wagerrProtocolV3 && bettingsViewCache.ExistsBettingUndo(bettingTxId)) {
                                if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                    LogPrintf("Revert failed!\n");
                                    return false;
                                }
                            }
                            else if (!bettingsViewCache.events->Erase(EventKey{plEventTx->nEventId})) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find event!\n");
                        }

                        break;
                    }
                    case fEventTxType:
                    {
                        if (!wagerrProtocolV4) break;
                        if (!validOracleTx) break;

                        CFieldEventTx* fEventTx = (CFieldEventTx*) bettingTx.get();
                        LogPrint(BCLog::BETTING, "CFieldEventTx: id: %lu, sport: %lu, tournament: %lu, stage: %lu, subgroup: %lu\n",
                            fEventTx->nEventId,
                            fEventTx->nSport,
                            fEventTx->nTournament,
                            fEventTx->nStage,
                            fEventTx->nGroupType
                        );
                        for (auto& contender : fEventTx->mContendersInputOdds) {
                            LogPrint(BCLog::BETTING, "%lu : %lu\n", contender.first, contender.second);
                        }

                        if (bettingsViewCache.fieldEvents->Exists(FieldEventKey{fEventTx->nEventId})) {
                            if (!bettingsViewCache.fieldEvents->Erase(FieldEventKey{fEventTx->nEventId})) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find event!\n");
                        }

                        break;
                    }
                    case fUpdateOddsTxType:
                    {
                        if (!wagerrProtocolV4) break;
                        if (!validOracleTx) break;

                        CFieldUpdateOddsTx* fUpdateOddsTx = (CFieldUpdateOddsTx*) bettingTx.get();
                        LogPrint(BCLog::BETTING, "CFieldUpdateOddsTx: id: %lu\n", fUpdateOddsTx->nEventId);
                        for (auto& contender : fUpdateOddsTx->mContendersInputOdds) {
                            LogPrint(BCLog::BETTING, "%lu : %lu\n", contender.first, contender.second);
                        }

                        if (bettingsViewCache.fieldEvents->Exists(EventKey{fUpdateOddsTx->nEventId})) {
                            if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find field event!\n");
                        }

                        break;
                    }
                    case fUpdateModifiersTxType:
                    {
                        if (!wagerrProtocolV4) break;
                        if (!validOracleTx) break;

                        CFieldUpdateModifiersTx* fUpdateModifiersTx = (CFieldUpdateModifiersTx*) bettingTx.get();
                        LogPrint(BCLog::BETTING, "CFieldUpdateModifiersTx: id: %lu\n", fUpdateModifiersTx->nEventId);
                        for (auto& contender : fUpdateModifiersTx->mContendersModifires) {
                            LogPrint(BCLog::BETTING, "%lu : %lu\n", contender.first, contender.second);
                        }

                        if (bettingsViewCache.fieldEvents->Exists(EventKey{fUpdateModifiersTx->nEventId})) {
                            if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find field event!\n");
                        }

                        break;
                    }
                    case fUpdateMarginTxType:
                    {
                        if (!wagerrProtocolV4) break;
                        if (!validOracleTx) break;

                        CFieldUpdateMarginTx* fUpdateMarginTx = (CFieldUpdateMarginTx*) bettingTx.get();
                        if (bettingsViewCache.fieldEvents->Exists(EventKey{fUpdateMarginTx->nEventId})) {
                            if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find field event!\n");
                        }

                        break;
                    }
                    case fZeroingOddsTxType:
                    {
                        if (!wagerrProtocolV4) break;
                        if (!validOracleTx) break;

                        CFieldZeroingOddsTx* fZeroingOddsTx = (CFieldZeroingOddsTx*) bettingTx.get();
                        LogPrint(BCLog::BETTING, "CFieldZeroingOddsTx: id: %lu\n", fZeroingOddsTx->nEventId);

                        if (bettingsViewCache.fieldEvents->Exists(EventKey{fZeroingOddsTx->nEventId})) {
                            if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find field event!\n");
                        }

                        break;
                    }
                    case fResultTxType:
                    {
                        if (!wagerrProtocolV4) break;
                        if (!validOracleTx) break;

                        CFieldResultTx* fResultTx = (CFieldResultTx*) bettingTx.get();

                        if (fResultTx->nResultType != ResultType::standardResult &&
                                fResultTx->nResultType != ResultType::eventRefund &&
                                fResultTx->nResultType != ResultType::eventClosed)
                            break;
                        if (!bettingsViewCache.fieldEvents->Exists(FieldEventKey{fResultTx->nEventId}))
                            break;

                        LogPrint(BCLog::BETTING, "CFieldResultTx: id: %lu, resultType: %lu\n", fResultTx->nEventId, fResultTx->nResultType);
                        for (auto& contender : fResultTx->contendersResults) {
                            LogPrint(BCLog::BETTING, "id %lu : place %lu\n", contender.first, contender.second);
                        }

                        if (bettingsViewCache.fieldResults->Exists(FieldResultKey{fResultTx->nEventId})) {
                            if (!bettingsViewCache.fieldResults->Erase(FieldResultKey{fResultTx->nEventId})) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find result!\n");
                        }

                        break;
                    }
                    case plResultTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessResultTx* plResultTx = (CPeerlessResultTx*) bettingTx.get();

                        LogPrintf("CPeerlessResult: id: %lu, resultType: %lu, homeScore: %lu, awayScore: %lu\n",
                            plResultTx->nEventId, plResultTx->nResultType, plResultTx->nHomeScore, plResultTx->nAwayScore);

                        if (bettingsViewCache.results->Exists(ResultKey{plResultTx->nEventId})) {
                            if (!bettingsViewCache.results->Erase(ResultKey{plResultTx->nEventId})) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find result!\n");
                        }
                        break;
                    }
                    case plUpdateOddsTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessUpdateOddsTx* plUpdateOddsTx = (CPeerlessUpdateOddsTx*) bettingTx.get();

                        LogPrintf("CPeerlessUpdateOdds: id: %lu, homeOdds: %lu, awayOdds: %lu, drawOdds: %lu\n",
                            plUpdateOddsTx->nEventId, plUpdateOddsTx->nHomeOdds, plUpdateOddsTx->nAwayOdds, plUpdateOddsTx->nDrawOdds);

                        if (bettingsViewCache.events->Exists(EventKey{plUpdateOddsTx->nEventId})) {
                            if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find event!\n");
                        }
                        break;
                    }
                    case cgEventTxType:
                    {
                        if (!validOracleTx) break;

                        if (!wagerrProtocolV3) break;

                        CChainGamesEventTx* cgEventTx = (CChainGamesEventTx*) bettingTx.get();

                        LogPrintf("CChainGamesEventTx: nEventId: %d, nEntryFee: %d\n", cgEventTx->nEventId, cgEventTx->nEntryFee);

                        if (!bettingsViewCache.chainGamesLottoEvents->Erase(EventKey{cgEventTx->nEventId})) {
                            LogPrintf("Revert failed!\n");
                            return false;
                        }
                        break;
                    }
                    case cgResultTxType:
                    {
                        if (!validOracleTx) break;

                        if (!wagerrProtocolV3) break;

                        CChainGamesResultTx* cgResultTx = (CChainGamesResultTx*) bettingTx.get();

                        LogPrintf("CChainGamesEventTx: nEventId: %d\n", cgResultTx->nEventId);

                        if (!bettingsViewCache.chainGamesLottoResults->Erase(ResultKey{cgResultTx->nEventId})) {
                            LogPrintf("Revert failed!\n");
                            return false;
                        }
                        break;
                    }
                    case plSpreadsEventTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessSpreadsEventTx* plSpreadsEventTx = (CPeerlessSpreadsEventTx*) bettingTx.get();

                        LogPrintf("CPeerlessSpreadsEvent: id: %lu, spreadPoints: %lu, homeOdds: %lu, awayOdds: %lu\n",
                            plSpreadsEventTx->nEventId, plSpreadsEventTx->nPoints, plSpreadsEventTx->nHomeOdds, plSpreadsEventTx->nAwayOdds);

                        if (bettingsViewCache.events->Exists(EventKey{plSpreadsEventTx->nEventId})) {
                            if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find event!\n");
                        }
                        break;
                    }
                    case plTotalsEventTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessTotalsEventTx* plTotalsEventTx = (CPeerlessTotalsEventTx*) bettingTx.get();

                        LogPrintf("CPeerlessTotalsEvent: id: %lu, totalPoints: %lu, overOdds: %lu, underOdds: %lu\n",
                            plTotalsEventTx->nEventId, plTotalsEventTx->nPoints, plTotalsEventTx->nOverOdds, plTotalsEventTx->nUnderOdds);

                        if (bettingsViewCache.events->Exists(EventKey{plTotalsEventTx->nEventId})) {
                            if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find event!\n");
                        }

                        break;
                    }
                    case plEventPatchTxType:
                    {
                        if (!validOracleTx) break;

                        CPeerlessEventPatchTx* plEventPatchTx = (CPeerlessEventPatchTx*) bettingTx.get();
                        LogPrintf("CPeerlessEventPatch: id: %lu, time: %lu\n", plEventPatchTx->nEventId, plEventPatchTx->nStartTime);

                        if (bettingsViewCache.events->Exists(EventKey{plEventPatchTx->nEventId})) {
                            if (!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Failed to find event!\n");
                        }
                        break;
                    }
                    case plEventZeroingOddsTxType:
                    {
                        if (!wagerrProtocolV4) break;
                        if (!validOracleTx) break;

                        CPeerlessEventZeroingOddsTx* plEventZeroingOddsTx = (CPeerlessEventZeroingOddsTx*) bettingTx.get();

                        std::stringstream eventIdsStream;
                        for (uint32_t eventId : plEventZeroingOddsTx->vEventIds) {
                            eventIdsStream << eventId << " ";
                        }
                        LogPrint(BCLog::BETTING, "CPeerlessEventZeroingOddsTx: ids: %s,\n", eventIdsStream.str());

                        bool isEventsExists = true;
                        for (uint32_t eventId : plEventZeroingOddsTx->vEventIds) {
                            if (!bettingsViewCache.events->Exists(EventKey{eventId})) {
                                isEventsExists = false;
                            }
                        }

                        if (isEventsExists) {
                            if(!UndoEventChanges(bettingsViewCache, bettingTxId, height)) {
                                LogPrintf("Revert failed!\n");
                                return false;
                            }
                        }
                        else {
                            LogPrintf("Not all events exists\n");
                        }

                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }

    LogPrintf("UndoBettingTx: end\n");
    return true;
}

/* Revert payouts info from DB */
bool UndoPayoutsInfo(CBettingsView &bettingsViewCache, int height)
{
    // we should save array of entries to delete because
    // changing (add/delete) of flushable DB when iterating is not allowed
    std::vector<PayoutInfoKey> entriesToDelete;
    auto it = bettingsViewCache.payoutsInfo->NewIterator();
    for (it->Seek(CBettingDB::DbTypeToBytes(PayoutInfoKey{static_cast<uint32_t>(height), COutPoint()})); it->Valid(); it->Next()) {
        PayoutInfoKey key;
        CBettingDB::BytesToDbType(it->Key(), key);
        if ((int64_t)key.blockHeight != height)
            break;
        else
            entriesToDelete.emplace_back(key);
    }

    // delete all entries with height of disconnected block
    for (auto&& key : entriesToDelete) {
        if (!bettingsViewCache.payoutsInfo->Erase(key))
            return false;
    }

    return true;
}

bool BettingUndo(const CCoinsViewCache &view, CBettingsView& bettingsViewCache, int height, const std::vector<CTransactionRef>& vtx)
{
        // Revert betting dats
    if (height > Params().GetConsensus().WagerrProtocolV2StartHeight()) {
        // Get the previous block so we can look for any results in it.
        CBlockIndex *resultsBocksIndex = NULL;
        resultsBocksIndex = ::ChainActive()[height-1];

        CBlock block;
        ReadBlockFromDisk(block, resultsBocksIndex, Params().GetConsensus());

        // revert complete bet payouts marker
        if (!UndoPLBetPayouts(view, block, bettingsViewCache, height)) {
            error("DisconnectBlock(): undo payout data is inconsistent");
            return false;
        }
        if (!UndoQGBetPayouts(bettingsViewCache, height)) {
            error("DisconnectBlock(): undo payout data for quick games bets is inconsistent");
            return false;
        }
        if (height > Params().GetConsensus().WagerrProtocolV4StartHeight()) {
            if (!UndoFieldBetPayouts(view, bettingsViewCache, height)) {
                error("DisconnectBlock(): undo payout data for field bets is inconsistent");
                return false;
            }
        }
        if (!UndoPayoutsInfo(bettingsViewCache, height)) {
            error("DisconnectBlock(): undo payouts info failed");
            return false;
        }

        // undo betting txs in back order
        for (auto it = vtx.crbegin(); it != vtx.crend(); it++) {
            if (!UndoBettingTx(view, bettingsViewCache, *it, height)) {
                error("DisconnectBlock(): custom transaction and undo data inconsistent");
                return false;
            }
        }
    }
    return true;
}