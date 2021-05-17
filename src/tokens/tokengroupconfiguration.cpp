// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tokens/tokengroupconfiguration.h"
#include "tokens/tokengroupmanager.h"

#include <chain.h>
#include <evo/specialtx.h>
#include <regex>

bool CTokenGroupCreation::ValidateDescription() {
    for (std::function<void (CTokenGroupCreation&)> tgFilters : tokenGroupManager.get()->vTokenGroupFilters) {
        tgFilters(*this);
    }
    return true;
}

// Checks that the token description data fulfills basic criteria
// Such as: max ticker length, no special characters, and sane decimal positions.
// Validation is performed before data is written to the database
void TGFilterCharacters(CTokenGroupCreation &tokenGroupCreation) {
    std::regex regexAlpha("^[a-zA-Z]+$");
    std::regex regexAlphaNum("^[a-zA-Z0-9]+$");
    std::regex regexUrl(R"((https?|ftp)://(-\.)?([^\s/?\.#-]+\.?)+(/[^\s]*)?$)");

    std::smatch matchResult;

    if (tokenGroupCreation.pTokenGroupDescription->strTicker != "" &&
            !std::regex_match(tokenGroupCreation.pTokenGroupDescription->strTicker, matchResult, regexAlpha)) {
        tokenGroupCreation.status.AddMessage("Token ticker can only contain letters.");
        tokenGroupCreation.pTokenGroupDescription->strTicker = "";
    }
    if (tokenGroupCreation.pTokenGroupDescription->strName != "" &&
            !std::regex_match(tokenGroupCreation.pTokenGroupDescription->strName, matchResult, regexAlpha)) {
        tokenGroupCreation.status.AddMessage("Token name can only contain letters.");
        tokenGroupCreation.pTokenGroupDescription->strName = "";
    }
    if (tokenGroupCreation.pTokenGroupDescription->strDocumentUrl != "" &&
            !std::regex_match(tokenGroupCreation.pTokenGroupDescription->strDocumentUrl, matchResult, regexUrl)) {
        tokenGroupCreation.status.AddMessage("Token description document URL cannot be parsed.");
        tokenGroupCreation.pTokenGroupDescription->strDocumentUrl = "";
    }
}

// Checks that the token description data fulfils context dependent criteria
// Such as: no reserved names, no double names
// Validation is performed after data is written to the database and before it is written to the map
void TGFilterUniqueness(CTokenGroupCreation &tokenGroupCreation) {
    // Iterate existing token groups and verify that the new group has an unique ticker and name
    std::string strLowerTicker;
    std::string strLowerName;
    std::transform(tokenGroupCreation.pTokenGroupDescription->strTicker.begin(), tokenGroupCreation.pTokenGroupDescription->strTicker.end(), std::back_inserter(strLowerTicker), ::tolower);
    std::transform(tokenGroupCreation.pTokenGroupDescription->strName.begin(), tokenGroupCreation.pTokenGroupDescription->strName.end(), std::back_inserter(strLowerName), ::tolower);

    CTokenGroupID tgID = tokenGroupCreation.tokenGroupInfo.associatedGroup;

    std::map<CTokenGroupID, CTokenGroupCreation> mapTGs = tokenGroupManager.get()->GetMapTokenGroups();

    if (strLowerTicker != "") {
        std::find_if(
            mapTGs.begin(),
            mapTGs.end(),
            [strLowerTicker, tgID, &tokenGroupCreation](const std::pair<CTokenGroupID, CTokenGroupCreation>& tokenGroup) {
                    // Only try to match with valid token groups
                    if (tokenGroup.second.tokenGroupInfo.invalid) return false;

                    // If the ID is the same, the token group is the same
                    if (tokenGroup.second.tokenGroupInfo.associatedGroup == tgID) return false;

                    // Compare lower case
                    std::string strHeapTicker;
                    std::transform(tokenGroup.second.pTokenGroupDescription->strTicker.begin(),
                        tokenGroup.second.pTokenGroupDescription->strTicker.end(),
                        std::back_inserter(strHeapTicker), ::tolower);
                    if (strLowerTicker == strHeapTicker){
                        tokenGroupCreation.status.AddMessage("Token ticker already exists.");
                        tokenGroupCreation.pTokenGroupDescription->strTicker = "";
                        return true;
                    }

                    return false;
                });
    }

    if (strLowerName != "") {
        std::find_if(
            mapTGs.begin(),
            mapTGs.end(),
            [strLowerName, tgID, &tokenGroupCreation](const std::pair<CTokenGroupID, CTokenGroupCreation>& tokenGroup) {
                    // Only try to match with valid token groups
                    if (tokenGroup.second.tokenGroupInfo.invalid) return false;

                    // If the ID is the same, the token group is the same
                    if (tokenGroup.second.tokenGroupInfo.associatedGroup == tgID) return false;

                    std::string strHeapName;
                    std::transform(tokenGroup.second.pTokenGroupDescription->strName.begin(),
                        tokenGroup.second.pTokenGroupDescription->strName.end(),
                        std::back_inserter(strHeapName), ::tolower);
                    if (strLowerName == strHeapName){
                        tokenGroupCreation.status.AddMessage("Token name already exists.");
                        tokenGroupCreation.pTokenGroupDescription->strName = "";
                        return true;
                    }

                    return false;
                });
    }
}

// Transforms tickers into upper case
// Returns true
void TGFilterUpperCaseTicker(CTokenGroupCreation &tokenGroupCreation) {
    std::string strUpperTicker;
    std::transform(tokenGroupCreation.pTokenGroupDescription->strTicker.begin(), tokenGroupCreation.pTokenGroupDescription->strTicker.end(), std::back_inserter(strUpperTicker), ::toupper);

    tokenGroupCreation.pTokenGroupDescription->strTicker = strUpperTicker;
}

template <typename TokenGroupDescription>
bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, std::shared_ptr<TokenGroupDescription>& tgDesc) {
    bool hasNewTokenGroup = false;
    for (const auto &txout : tx.vout) {
        const CScript &scriptPubKey = txout.scriptPubKey;
        CTokenGroupInfo tokenGrp(scriptPubKey);
        if (tokenGrp.invalid)
            return false;
        if (tokenGrp.associatedGroup != NoGroup && tokenGrp.isGroupCreation() && !hasNewTokenGroup) {
            hasNewTokenGroup = true;
            tokenGroupInfo = tokenGrp;
        }
    }
    if (hasNewTokenGroup) {
        return GetTxPayload(tx, *tgDesc);
    }
    return false;
}
template bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, std::shared_ptr<CTokenGroupDescriptionRegular>& tgDesc);
template bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, std::shared_ptr<CTokenGroupDescriptionMGT>& tgDesc);

bool CreateTokenGroup(const CTransactionRef tx, const uint256& blockHash, CTokenGroupCreation &newTokenGroupCreation) {
    CTokenGroupInfo tokenGroupInfo;
    CTokenGroupStatus tokenGroupStatus;

    if (tx->nType == TRANSACTION_GROUP_CREATION_REGULAR) {
        std::shared_ptr<CTokenGroupDescriptionRegular> pTokenGroupDescription = std::make_shared<CTokenGroupDescriptionRegular>();
        if (!GetTokenConfigurationParameters(*tx, tokenGroupInfo, pTokenGroupDescription)) return false;
        newTokenGroupCreation = CTokenGroupCreation(tx, blockHash, tokenGroupInfo, pTokenGroupDescription, tokenGroupStatus);
    } else if (tx->nType == TRANSACTION_GROUP_CREATION_MGT) {
        std::shared_ptr<CTokenGroupDescriptionMGT> pTokenGroupDescription = std::make_shared<CTokenGroupDescriptionMGT>();
        if (!GetTokenConfigurationParameters(*tx, tokenGroupInfo, pTokenGroupDescription)) return false;
        newTokenGroupCreation = CTokenGroupCreation(tx, blockHash, tokenGroupInfo, pTokenGroupDescription, tokenGroupStatus);
    }
    return true;
}

bool CheckGroupConfigurationTxRegular(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view)
{
    if (tx.nType != TRANSACTION_GROUP_CREATION_REGULAR) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-protx-type");
    }

    if (!IsAnyOutputGroupedCreation(tx)) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-tx");
    }

    CTokenGroupDescriptionRegular tgDesc;
    if (!GetTxPayload(tx, tgDesc)) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-protx-payload");
    }
    if (tgDesc.nDecimalPos > 16) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-param");
    }

    if (tgDesc.nVersion == 0 || tgDesc.nVersion > CTokenGroupDescriptionRegular::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-version");
    }

    return true;
}

bool CheckGroupConfigurationTxMGT(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view)
{
    if (tx.nType != TRANSACTION_GROUP_CREATION_MGT) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-protx-type");
    }

    if (!IsAnyOutputGroupedCreation(tx)) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-tx");
    }

    CTokenGroupDescriptionMGT tgDesc;
    if (!GetTxPayload(tx, tgDesc)) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-protx-payload");
    }
    if (tgDesc.nDecimalPos > 16) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-param");
    }
    if (!tgDesc.blsPubKey.IsValid()) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-key");
    }

    if (tgDesc.nVersion == 0 || tgDesc.nVersion > CTokenGroupDescriptionMGT::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "grp-bad-version");
    }

    return true;
}
