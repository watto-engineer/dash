// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tokens/tokengroupmanager.h"

#include "chain.h"
#include "coins.h"
#include "key_io.h"
#include "wagerraddrenc.h"
#include "logging.h"
#include "policy/policy.h"
#include "rpc/protocol.h"
#include "rpc/request.h"
#include "script/tokengroup.h"
#include "util/strencodings.h"
#include "tokens/tokengroupconfiguration.h"
#include "tokens/tokendb.h"

std::shared_ptr<CTokenGroupManager> tokenGroupManager;

CTokenGroupManager::CTokenGroupManager() {
    mapTokenGroups.clear();
}

bool CTokenGroupManager::StoreManagementTokenGroups(CTokenGroupCreation tokenGroupCreation) {
    if (tokenGroupCreation.pTokenGroupDescription->which() != 1)
        return false;

    std::string ticker = tgDescGetTicker(*tokenGroupCreation.pTokenGroupDescription);
    
    if (!tgMGTCreation && ticker == "MGT") {
        this->tgMGTCreation = std::unique_ptr<CTokenGroupCreation>(new CTokenGroupCreation((tokenGroupCreation)));
        return true;
    } else if (!tgORATCreation && ticker == "ORAT") {
        this->tgORATCreation = std::unique_ptr<CTokenGroupCreation>(new CTokenGroupCreation((tokenGroupCreation)));
        return true;
    }
    return false;
}

void CTokenGroupManager::ClearManagementTokenGroups() {
    tgMGTCreation.reset();
    tgORATCreation.reset();
}

bool CTokenGroupManager::MatchesMGT(CTokenGroupID tgID) {
    if (!tgMGTCreation) return false;
    return tgID == tgMGTCreation->tokenGroupInfo.associatedGroup;
}

bool CTokenGroupManager::MatchesORAT(CTokenGroupID tgID) {
    if (!tgORATCreation) return false;
    return tgID == tgORATCreation->tokenGroupInfo.associatedGroup;
}

bool CTokenGroupManager::AddTokenGroups(const std::vector<CTokenGroupCreation>& newTokenGroups) {
    AssertLockHeld(cs_main);

    for (auto tokenGroupCreation : newTokenGroups) {
        if (!tokenGroupCreation.ValidateDescription()) {
            LogPrint(BCLog::TOKEN, "%s - Validation of token %s failed", __func__, EncodeTokenGroup(tokenGroupCreation.tokenGroupInfo.associatedGroup));
        }

        StoreManagementTokenGroups(tokenGroupCreation);

        auto ret = mapTokenGroups.insert(std::pair<CTokenGroupID, CTokenGroupCreation>(tokenGroupCreation.tokenGroupInfo.associatedGroup, tokenGroupCreation));

        CTokenGroupCreation& tokenGroupCreationRet = (*ret.first).second;
        bool fInsertedNew = ret.second;
        if (!fInsertedNew) {
            LogPrint(BCLog::TOKEN, "%s - Double token creation with tokenGroupID %s.\n", __func__, EncodeTokenGroup(tokenGroupCreationRet.tokenGroupInfo.associatedGroup));
        }
    }
    return true;
}

void CTokenGroupManager::ResetTokenGroups() {
    mapTokenGroups.clear();
    ClearManagementTokenGroups();

    CTokenGroupInfo tgInfoWAGERR(NoGroup, (CAmount)GroupAuthorityFlags::ALL);
    CTransaction tgTxWagerr;
    CTokenGroupDescriptionVariant tgDescriptionWAGERR = CTokenGroupDescriptionRegular("WAGERR", "Wagerr", 8, "https://wagerr.com", uint256());
    CTokenGroupStatus tokenGroupStatus;
    CTokenGroupCreation tgCreationWAGERR(MakeTransactionRef(tgTxWagerr), uint256(), tgInfoWAGERR, std::make_shared<CTokenGroupDescriptionVariant>(tgDescriptionWAGERR), tokenGroupStatus);
    mapTokenGroups.insert(std::pair<CTokenGroupID, CTokenGroupCreation>(NoGroup, tgCreationWAGERR));

}

bool CTokenGroupManager::RemoveTokenGroup(CTransaction tx, CTokenGroupID &toRemoveTokenGroupID) {
    CTokenGroupInfo tokenGroupInfo;

    bool hasNewTokenGroup = false;

    if (tx.nType == TRANSACTION_GROUP_CREATION_REGULAR) {
        CTokenGroupDescriptionRegular tgDesc;
        hasNewTokenGroup = GetTokenConfigurationParameters(tx, tokenGroupInfo, tgDesc);
    } else if (tx.nType == TRANSACTION_GROUP_CREATION_MGT) {
        CTokenGroupDescriptionMGT tgDesc;
        hasNewTokenGroup = GetTokenConfigurationParameters(tx, tokenGroupInfo, tgDesc);
    } else if (tx.nType == TRANSACTION_GROUP_CREATION_NFT) {
        CTokenGroupDescriptionNFT tgDesc;
        hasNewTokenGroup = GetTokenConfigurationParameters(tx, tokenGroupInfo, tgDesc);
    }

    if (hasNewTokenGroup) {
        if (MatchesMGT(tokenGroupInfo.associatedGroup)) {
            tgMGTCreation.reset();
        } else if (MatchesORAT(tokenGroupInfo.associatedGroup)) {
            tgORATCreation.reset();
        }

        auto iter = mapTokenGroups.find(tokenGroupInfo.associatedGroup);
        if (iter != mapTokenGroups.end()) {
            toRemoveTokenGroupID = (*iter).first;
            mapTokenGroups.erase(iter);
            return true;
        }
    }
    return false;
}

bool CTokenGroupManager::GetTokenGroupCreation(const CTokenGroupID& tgID, CTokenGroupCreation& tgCreation) {
    const CTokenGroupID grpID = tgID.isSubgroup() ? tgID.parentGroup() : tgID;

    std::map<CTokenGroupID, CTokenGroupCreation>::iterator iter = mapTokenGroups.find(grpID);
    if (iter != mapTokenGroups.end()) {
        tgCreation = mapTokenGroups.at(grpID);
    } else {
        return false;
    }
    return true;
}
std::string CTokenGroupManager::GetTokenGroupNameByID(CTokenGroupID tokenGroupId) {
    CTokenGroupCreation tokenGroupCreation;
    return GetTokenGroupCreation(tokenGroupId, tokenGroupCreation) ? tgDescGetName(*tokenGroupCreation.pTokenGroupDescription) : "";
}

std::string CTokenGroupManager::GetTokenGroupTickerByID(CTokenGroupID tokenGroupId) {
    CTokenGroupCreation tokenGroupCreation;
    return GetTokenGroupCreation(tokenGroupId, tokenGroupCreation) ? tgDescGetName(*tokenGroupCreation.pTokenGroupDescription) : "";
}

bool CTokenGroupManager::GetTokenGroupIdByTicker(std::string strTicker, CTokenGroupID &tokenGroupID) {
    std::string strNeedleTicker;
    std::transform(strTicker.begin(), strTicker.end(), std::back_inserter(strNeedleTicker), ::tolower);
    auto result = std::find_if(
        mapTokenGroups.begin(), mapTokenGroups.end(),
        [strNeedleTicker](const std::pair<CTokenGroupID, CTokenGroupCreation>& tokenGroup) {
            std::string strHeapTickerLower;
            std::string strHeapTicker = tgDescGetTicker(*tokenGroup.second.pTokenGroupDescription);
            std::transform(strHeapTicker.begin(), strHeapTicker.end(), std::back_inserter(strHeapTickerLower), ::tolower);
            return strHeapTickerLower == strNeedleTicker;
        });
    if (result != mapTokenGroups.end()) {
        tokenGroupID = result->first;
        return true;
    };
    return false;
}

bool CTokenGroupManager::GetTokenGroupIdByName(std::string strName, CTokenGroupID &tokenGroupID) {
    std::string strNeedleName;
    std::transform(strName.begin(), strName.end(), std::back_inserter(strNeedleName), ::tolower);
    auto result = std::find_if(
        mapTokenGroups.begin(), mapTokenGroups.end(),
        [strNeedleName](const std::pair<CTokenGroupID, CTokenGroupCreation>& tokenGroup) {
            std::string strHeapNameLower;
            std::string strHeapName = tgDescGetName(*tokenGroup.second.pTokenGroupDescription);
            std::transform(strHeapName.begin(), strHeapName.end(), std::back_inserter(strHeapNameLower), ::tolower);
            return strHeapNameLower == strNeedleName;
        });
    if (result != mapTokenGroups.end()) {
        tokenGroupID = result->first;
        return true;
    };
    return false;
}

bool CTokenGroupManager::ManagementTokensCreated() {
    return MGTTokensCreated() && ORATTokensCreated();
}

uint16_t CTokenGroupManager::GetTokensInBlock(const CBlock& block, const CTokenGroupID& tgId) {
    uint16_t nTokenCount = 0;
    if (tokenGroupManager) {
        for (unsigned int i = 0; i < block.vtx.size(); i++)
        {
            for (const auto &outp : block.vtx[i]->vout)
            {
                const CScript &scriptPubKey = outp.scriptPubKey;
                CTokenGroupInfo tokenGrp(scriptPubKey);
                if (!tokenGrp.invalid && tokenGrp.associatedGroup == tgId)
                {
                    nTokenCount++;
                    break;
                }
            }
        }
    }
    return nTokenCount;
}

unsigned int CTokenGroupManager::GetTokenTxStats(const CTransactionRef &tx, const CCoinsViewCache& view, const CTokenGroupID &tgId,
                uint16_t &nTokenCount, CAmount &nTokenMint) {

    CAmount nTxValueOut = 0;
    CAmount nTxValueIn = 0;

    if (!tx->IsCoinBase() && !tx->IsCoinStake() && !tx->HasZerocoinSpendInputs()) {
        for (const auto &outp : tx->vout)
        {
            const CScript &scriptPubKey = outp.scriptPubKey;
            CTokenGroupInfo tokenGrp(scriptPubKey);
            if (!tokenGrp.invalid && tokenGrp.associatedGroup == tgId && !tokenGrp.isAuthority())
            {
                nTxValueOut += tokenGrp.quantity;
            }
        }
        for (const auto &inp : tx->vin)
        {
            const COutPoint &prevout = inp.prevout;
            const Coin &coin = view.AccessCoin(prevout);

            if (coin.nHeight < Params().GetConsensus().ATPStartHeight)
                continue;
            const CScript &script = coin.out.scriptPubKey;

            CTokenGroupInfo tokenGrp(script);
            if (!tokenGrp.invalid && tokenGrp.associatedGroup == tgId && !tokenGrp.isAuthority())
            {
                nTxValueIn += tokenGrp.quantity;
            }
        }
        nTokenMint += nTxValueOut - nTxValueIn;
        if (nTxValueIn > 0 || nTxValueOut > 0) {
            nTokenCount++;
        }
    }

    return nTokenCount;
}

bool CTokenGroupManager::TokenMoneyRange(CAmount nValueOut) {
    // Token amount max is 2^63-1 = 9223372036854775807
    return nValueOut >= 0 && nValueOut <= 922337203685477580;
}

CAmount CTokenGroupManager::AmountFromTokenValue(const UniValue& value, const CTokenGroupID& tgID) {
    if (!value.isNum() && !value.isStr())
        throw JSONRPCError(RPC_TYPE_ERROR, "Token amount is not a number or string");
    CAmount amount;
    CTokenGroupCreation tgCreation;
    GetTokenGroupCreation(tgID, tgCreation);
    uint8_t nDecimalPos = 0;
    nDecimalPos = tgDescGetDecimalPos(*tgCreation.pTokenGroupDescription);
    if (!ParseFixedPoint(value.getValStr(), nDecimalPos, &amount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid token amount");
    if (!TokenMoneyRange(amount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Token amount out of range");
    return amount;
}

std::string CTokenGroupManager::TokenValueFromAmount(const CAmount& amount, const CTokenGroupID& tgID) {
    CTokenGroupCreation tgCreation;
    GetTokenGroupCreation(tgID, tgCreation);
    CAmount tokenCOIN = tgDescGetCoinAmount(*tgCreation.pTokenGroupDescription);

    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / tokenCOIN;
    int64_t remainder = n_abs % tokenCOIN;

    uint8_t nDecimalPos = 0;
    nDecimalPos = tgDescGetDecimalPos(*tgCreation.pTokenGroupDescription);

    if (nDecimalPos == 0) {
        return strprintf("%s%d", sign ? "-" : "", quotient);
    } else {
        return strprintf("%s%d.%0*d", sign ? "-" : "", quotient, nDecimalPos, remainder);
    }
}

bool CTokenGroupManager::CheckFees(const CTransaction &tx, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, CValidationState& state, const CBlockIndex* pindex) {
    if (!tgMGTCreation) return true;
    // A token group creation costs 5x the standard TX fee
    // A token mint transaction costs 2x the standard TX fee
    // Sending tokens costs the standard fee

    uint32_t tokenMelts = 0;
    uint32_t tokenMints = 0;
    uint32_t tokensCreated = 0;
    uint32_t tokenOutputs = 0;

    CFeeRate feeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);

    for (auto txout : tx.vout) {
        CTokenGroupInfo grp(txout.scriptPubKey);
        if (grp.invalid)
            return false;
        if (grp.isGroupCreation() && !grp.associatedGroup.hasFlag(TokenGroupIdFlags::MGT_TOKEN)) {
            // Creation tx of regular token
            tokensCreated++;
        }
        if (grp.getAmount() > 0) {
            // Token output (send or mint)
            tokenOutputs++;
        }
    }
    for (auto bal : tgMintMeltBalance) {
        CTokenGroupCreation tgCreation;
        CTokenGroupID tgID = bal.first;
        CTokenGroupBalance tgBalance = bal.second;
        if (tgBalance.output - tgBalance.input > 0) {
            // Mint
            if (!tgID.hasFlag(TokenGroupIdFlags::MGT_TOKEN)) {
                // Regular token mint tx
                tokenMints++;
            } else {
                // Management token mint tx
                tokenMints++;
            }
        } else if (tgBalance.output - tgBalance.input < 0) {
            // Melt
            tokenMelts--;
        }
    }
    return true;
}

bool CTokenGroupManager::CollectTokensFromBlock(const CBlock& block, const CBlockIndex* pindex, CValidationState& _state, const CCoinsViewCache& view, bool fJustCheck)
{
    AssertLockHeld(cs_main);

    const auto& consensusParams = Params().GetConsensus();
    bool fATPActive = pindex->nHeight >= consensusParams.ATPStartHeight;
    if (!fATPActive) {
        return true;
    }

    LOCK(cs);

    newTokenGroups.clear();

    // Get new token groups from block
    for (const auto& ptx : block.vtx) {
        if (ptx->nVersion != 3)
            continue;

        switch (ptx->nType)
        {
            case TRANSACTION_GROUP_CREATION_REGULAR:
            case TRANSACTION_GROUP_CREATION_MGT:
            case TRANSACTION_GROUP_CREATION_NFT:
            {
                CTokenGroupCreation newTokenGroupCreation;
                if (!CreateTokenGroup(ptx, block.GetHash(), newTokenGroupCreation)) {
                    return _state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "bad-op-group");
                } else {
                    newTokenGroups.push_back(newTokenGroupCreation);
                }
                break;
            }
            default:
                break;
        }
    }
    return true;
}

void CTokenGroupManager::ApplyTokensFromBlock()
{
    AssertLockHeld(cs_main);

    LOCK(cs);

    pTokenDB->WriteTokenGroupsBatch(newTokenGroups);
    tokenGroupManager->AddTokenGroups(newTokenGroups);
    newTokenGroups.clear();
}

bool CTokenGroupManager::UndoBlock(const CBlock& block, const CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    const auto& consensusParams = Params().GetConsensus();
    bool fATPActive = pindex->nHeight >= consensusParams.ATPStartHeight;
    if (!fATPActive) {
        return true;
    }

    LOCK(cs);

    std::vector<CTokenGroupID> toRemoveTokenGroupIDs;
    for (const auto& ptx : block.vtx) {
        if (ptx->nVersion != 3)
            continue;

        CTokenGroupID toRemoveTokenGroupID;
        if (!tokenGroupManager.get()->RemoveTokenGroup(*ptx, toRemoveTokenGroupID))
            continue;
        toRemoveTokenGroupIDs.push_back(toRemoveTokenGroupID);
    }

    if (!pTokenDB->EraseTokenGroupBatch(toRemoveTokenGroupIDs)) {
        return false;
    }

    return true;
}

