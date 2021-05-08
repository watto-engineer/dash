// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tokens/tokengroupmanager.h"

#include "chain.h"
#include "coins.h"
#include "dstencode.h"
#include "bytzaddrenc.h"
#include "policy/policy.h"
#include "rpc/protocol.h"
#include "script/tokengroup.h"
#include "utilstrencodings.h"
#include "tokens/tokengroupconfiguration.h"

std::shared_ptr<CTokenGroupManager> tokenGroupManager;

CTokenGroupManager::CTokenGroupManager() {
    mapTokenGroups.clear();
    vTokenGroupFilters.emplace_back(TGFilterCharacters);
    vTokenGroupFilters.emplace_back(TGFilterUniqueness);
    vTokenGroupFilters.emplace_back(TGFilterUpperCaseTicker);
}

bool CTokenGroupManager::StoreManagementTokenGroups(CTokenGroupCreation tokenGroupCreation) {
    if (!tgMagicCreation && tokenGroupCreation.tokenGroupDescription.strTicker == "MAGIC") {
        this->tgMagicCreation = std::unique_ptr<CTokenGroupCreation>(new CTokenGroupCreation((tokenGroupCreation)));
        return true;
    } else if (!tgAtomCreation && tokenGroupCreation.tokenGroupDescription.strTicker == "ATOM") {
        this->tgAtomCreation = std::unique_ptr<CTokenGroupCreation>(new CTokenGroupCreation((tokenGroupCreation)));
        return true;
    } else if (!tgElectronCreation && tokenGroupCreation.tokenGroupDescription.strTicker == "ELEC") {
        this->tgElectronCreation = std::unique_ptr<CTokenGroupCreation>(new CTokenGroupCreation((tokenGroupCreation)));
        return true;
    }
    return false;
}

void CTokenGroupManager::ClearManagementTokenGroups() {
    tgMagicCreation.reset();
    tgAtomCreation.reset();
}

bool CTokenGroupManager::MatchesMagic(CTokenGroupID tgID) {
    if (!tgMagicCreation) return false;
    return tgID == tgMagicCreation->tokenGroupInfo.associatedGroup;
}

bool CTokenGroupManager::MatchesAtom(CTokenGroupID tgID) {
    if (!tgAtomCreation) return false;
    return tgID == tgAtomCreation->tokenGroupInfo.associatedGroup;
}

bool CTokenGroupManager::MatchesElectron(CTokenGroupID tgID) {
    if (!tgElectronCreation) return false;
    return tgID == tgElectronCreation->tokenGroupInfo.associatedGroup;
}

bool CTokenGroupManager::AddTokenGroups(const std::vector<CTokenGroupCreation>& newTokenGroups) {
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

    CTokenGroupInfo tgInfoBYTZ(NoGroup, (CAmount)GroupAuthorityFlags::ALL);
    CTransaction tgTxBytz;
    CTokenGroupDescription tgDescriptionBYTZ("BYTZ", "Bytz", 8, "https://bytz.gg", uint256());
    CTokenGroupStatus tokenGroupStatus;
    CTokenGroupCreation tgCreationBYTZ(MakeTransactionRef(tgTxBytz), uint256(), tgInfoBYTZ, tgDescriptionBYTZ, tokenGroupStatus);
    mapTokenGroups.insert(std::pair<CTokenGroupID, CTokenGroupCreation>(NoGroup, tgCreationBYTZ));

}

bool CTokenGroupManager::RemoveTokenGroup(CTransaction tx, CTokenGroupID &toRemoveTokenGroupID) {
    CScript firstOpReturn;
    CTokenGroupInfo tokenGroupInfo;

    bool hasNewTokenGroup = GetTokenConfigurationParameters(tx, tokenGroupInfo, firstOpReturn);

    if (hasNewTokenGroup) {
        if (MatchesMagic(tokenGroupInfo.associatedGroup)) {
            tgMagicCreation.reset();
        } else if (MatchesAtom(tokenGroupInfo.associatedGroup)) {
            tgAtomCreation.reset();
        } else if (MatchesElectron(tokenGroupInfo.associatedGroup)) {
            tgElectronCreation.reset();
        }

        std::map<CTokenGroupID, CTokenGroupCreation>::iterator iter = mapTokenGroups.find(tokenGroupInfo.associatedGroup);
        if (iter != mapTokenGroups.end()) {
            mapTokenGroups.erase(iter);
            toRemoveTokenGroupID = iter->first;
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
    return GetTokenGroupCreation(tokenGroupId, tokenGroupCreation) ? tokenGroupCreation.tokenGroupDescription.strName : "";
}

std::string CTokenGroupManager::GetTokenGroupTickerByID(CTokenGroupID tokenGroupId) {
    CTokenGroupCreation tokenGroupCreation;
    return GetTokenGroupCreation(tokenGroupId, tokenGroupCreation) ? tokenGroupCreation.tokenGroupDescription.strTicker : "";
}

bool CTokenGroupManager::GetTokenGroupIdByTicker(std::string strTicker, CTokenGroupID &tokenGroupID) {
    std::string strNeedleTicker;
    std::transform(strTicker.begin(), strTicker.end(), std::back_inserter(strNeedleTicker), ::tolower);
    auto result = std::find_if(
        mapTokenGroups.begin(), mapTokenGroups.end(),
        [strNeedleTicker](const std::pair<CTokenGroupID, CTokenGroupCreation>& tokenGroup) {
            std::string strHeapTicker;
            std::transform(tokenGroup.second.tokenGroupDescription.strTicker.begin(),
                tokenGroup.second.tokenGroupDescription.strTicker.end(),
                std::back_inserter(strHeapTicker), ::tolower);
            return strHeapTicker == strNeedleTicker;
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
            std::string strHeapName;
            std::transform(tokenGroup.second.tokenGroupDescription.strName.begin(),
                tokenGroup.second.tokenGroupDescription.strName.end(),
                std::back_inserter(strHeapName), ::tolower);
            return strHeapName == strNeedleName;
        });
    if (result != mapTokenGroups.end()) {
        tokenGroupID = result->first;
        return true;
    };
    return false;
}

bool CTokenGroupManager::ManagementTokensCreated(int nHeight) {
    // if (!ElectronTokensCreated() && nHeight >= Params().GetConsensus().POSPOWStartHeight)
    //    return false;
    return MagicTokensCreated() && AtomTokensCreated();
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
    if (!ParseFixedPoint(value.getValStr(), tgCreation.tokenGroupDescription.nDecimalPos, &amount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid token amount");
    if (!TokenMoneyRange(amount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Token amount out of range");
    return amount;
}

std::string CTokenGroupManager::TokenValueFromAmount(const CAmount& amount, const CTokenGroupID& tgID) {
    CTokenGroupCreation tgCreation;
    GetTokenGroupCreation(tgID, tgCreation);
    CAmount tokenCOIN = tgCreation.tokenGroupDescription.GetCoin();
    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / tokenCOIN;
    int64_t remainder = n_abs % tokenCOIN;
    if (tgCreation.tokenGroupDescription.nDecimalPos == 0) {
        return strprintf("%s%d", sign ? "-" : "", quotient);
    } else {
        return strprintf("%s%d.%0*d", sign ? "-" : "", quotient, tgCreation.tokenGroupDescription.nDecimalPos, remainder);
    }
}

bool CTokenGroupManager::CheckFees(const CTransaction &tx, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance, CValidationState& state, const CBlockIndex* pindex) {
    if (!tgMagicCreation) return true;
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
