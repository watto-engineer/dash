// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tokens/tokengroupconfiguration.h"
#include "tokens/tokengroupmanager.h"

#include <chain.h>
#include <consensus/consensus.h>
#include <evo/specialtx.h>
#include <index/txindex.h>
#include <regex>

#include <boost/variant.hpp>

class TokenGroupDescriptionFilterVisitor : public boost::static_visitor<bool>
{
private:
    const CTokenGroupCreation* tgCreation;
public:
    TokenGroupDescriptionFilterVisitor(CTokenGroupCreation* tgCreation) : tgCreation(tgCreation) {};

    bool operator()(CTokenGroupDescriptionRegular& tgDesc) const {
        TGFilterTickerCharacters(tgDesc);
        TGFilterNameCharacters(tgDesc);
        TGFilterURLCharacters(tgDesc);
        TGFilterTickerUniqueness(tgDesc, tgCreation->tokenGroupInfo.associatedGroup);
        TGFilterNameUniqueness(tgDesc, tgCreation->tokenGroupInfo.associatedGroup);
        TGFilterUpperCaseTicker(tgDesc);
        return true;
    }
    bool operator()(CTokenGroupDescriptionMGT& tgDesc) const {
        TGFilterTickerCharacters(tgDesc);
        TGFilterNameCharacters(tgDesc);
        TGFilterURLCharacters(tgDesc);
        TGFilterTickerUniqueness(tgDesc, tgCreation->tokenGroupInfo.associatedGroup);
        TGFilterNameUniqueness(tgDesc, tgCreation->tokenGroupInfo.associatedGroup);
        TGFilterUpperCaseTicker(tgDesc);
        return true;
    }
    bool operator()(CTokenGroupDescriptionNFT& tgDesc) const {
        TGFilterNameCharacters(tgDesc);
        TGFilterURLCharacters(tgDesc);
        TGFilterNameUniqueness(tgDesc, tgCreation->tokenGroupInfo.associatedGroup);
        return true;
    }
};

bool CTokenGroupCreation::ValidateDescription() {
    return boost::apply_visitor(TokenGroupDescriptionFilterVisitor(this), *pTokenGroupDescription.get());
}

// Checks that the token description data fulfills basic criteria
// Such as: max ticker length, no special characters, and sane decimal positions.
// Validation is performed before data is written to the database
template <typename T>
void TGFilterTickerCharacters(T& tgDesc) {
    std::regex regexTicker("^[a-zA-Z0-9]+$"); // only letters and numbers
    std::regex regexUrl(R"((https?|ftp)://(-\.)?([^\s/?\.#-]+\.?)+(/[^\s]*)?$)");

    std::smatch matchResult;

    if (tgDesc.strTicker != "" &&
            !std::regex_match(tgDesc.strTicker, matchResult, regexTicker)) {
        // Token ticker can only contain letters
        tgDesc.strTicker = "<FILTERED>";
    }
}
template void TGFilterTickerCharacters(CTokenGroupDescriptionRegular& tgDesc);
template void TGFilterTickerCharacters(CTokenGroupDescriptionMGT& tgDesc);

template <typename T>
void TGFilterNameCharacters(T& tgDesc) {
    std::regex regexName("^[a-zA-Z0-9][a-zA-Z0-9- ]*[a-zA-Z0-9]$"); // letters, numbers and spaces; at least 2 characters; no space or dash at beginning or end

    std::smatch matchResult;

    if (tgDesc.strName != "" &&
            !std::regex_match(tgDesc.strName, matchResult, regexName)) {
        // Token name can only contain letters, numbers and spaces. At least 2 characters. No space at beginning or end
        tgDesc.strName = "<FILTERED>";
    }
}
template void TGFilterNameCharacters(CTokenGroupDescriptionRegular& tgDesc);
template void TGFilterNameCharacters(CTokenGroupDescriptionMGT& tgDesc);
template void TGFilterNameCharacters(CTokenGroupDescriptionNFT& tgDesc);

template <typename T>
void TGFilterURLCharacters(T& tgDesc) {
    std::regex regexUrl(R"((http?|ftp|wagerr)://(-\.)?([^\s/?\.#-]+\.?)+(/[^\s]*)?$)");

    std::smatch matchResult;

    if (tgDesc.strDocumentUrl != "" &&
            !std::regex_match(tgDesc.strDocumentUrl, matchResult, regexUrl)) {
        // Token description document URL cannot be parsed
        tgDesc.strDocumentUrl = tgDesc.strDocumentUrl + " (non-standard URL)";
    }
}
template void TGFilterURLCharacters(CTokenGroupDescriptionRegular& tgDesc);
template void TGFilterURLCharacters(CTokenGroupDescriptionMGT& tgDesc);
template void TGFilterURLCharacters(CTokenGroupDescriptionNFT& tgDesc);

// Checks that the token description data fulfils context dependent criteria
// Such as: no reserved names, no double names
// Validation is performed after data is written to the database and before it is written to the map
template <typename T>
void TGFilterTickerUniqueness(T& tgDesc, const CTokenGroupID& tgID) {
    // Iterate existing token groups and verify that the new group has a unique ticker
    std::string strLowerTicker;
    std::transform(tgDesc.strTicker.begin(), tgDesc.strTicker.end(), std::back_inserter(strLowerTicker), ::tolower);

    std::map<CTokenGroupID, CTokenGroupCreation> mapTGs = tokenGroupManager.get()->GetMapTokenGroups();

    if (strLowerTicker != "") {
        std::find_if(
            mapTGs.begin(),
            mapTGs.end(),
            [strLowerTicker, tgID, &tgDesc](const std::pair<CTokenGroupID, CTokenGroupCreation>& tokenGroup) {
                    // Only try to match with valid token groups
                    if (tokenGroup.second.tokenGroupInfo.invalid) return false;

                    // If the ID is the same, the token group is the same
                    if (tokenGroup.second.tokenGroupInfo.associatedGroup == tgID) return false;

                    // Compare lower case
                    std::string strHeapTicker = tgDescGetTicker(*tokenGroup.second.pTokenGroupDescription);
                    std::string strHeapTickerLower;
                    std::transform(strHeapTicker.begin(), strHeapTicker.end(), std::back_inserter(strHeapTickerLower), ::tolower);
                    if (strLowerTicker == strHeapTickerLower){
                        // Token ticker already exists
                        tgDesc.strTicker = "";
                        return true;
                    }

                    return false;
                });
    }
}
template void TGFilterTickerUniqueness(CTokenGroupDescriptionRegular& tgDesc, const CTokenGroupID& tgID);
template void TGFilterTickerUniqueness(CTokenGroupDescriptionMGT& tgDesc, const CTokenGroupID& tgID);

template <typename T>
void TGFilterNameUniqueness(T& tgDesc, const CTokenGroupID& tgID) {
    // Iterate existing token groups and verify that the new group has a unique name
    std::string strLowerName;
    std::transform(tgDesc.strName.begin(), tgDesc.strName.end(), std::back_inserter(strLowerName), ::tolower);

    std::map<CTokenGroupID, CTokenGroupCreation> mapTGs = tokenGroupManager.get()->GetMapTokenGroups();

    if (strLowerName != "") {
        std::find_if(
            mapTGs.begin(),
            mapTGs.end(),
            [strLowerName, tgID, &tgDesc](const std::pair<CTokenGroupID, CTokenGroupCreation>& tokenGroup) {
                    // Only try to match with valid token groups
                    if (tokenGroup.second.tokenGroupInfo.invalid) return false;

                    // If the ID is the same, the token group is the same
                    if (tokenGroup.second.tokenGroupInfo.associatedGroup == tgID) return false;

                    std::string strHeapName = tgDescGetName(*tokenGroup.second.pTokenGroupDescription);
                    std::string strHeapNameLower;
                    std::transform(strHeapName.begin(), strHeapName.end(), std::back_inserter(strHeapNameLower), ::tolower);
                    if (strLowerName == strHeapNameLower){
                        // Token name already exists
                        tgDesc.strName = "";
                        return true;
                    }

                    return false;
                });
    }
}
template void TGFilterNameUniqueness(CTokenGroupDescriptionRegular& tgDesc, const CTokenGroupID& tgID);
template void TGFilterNameUniqueness(CTokenGroupDescriptionMGT& tgDesc, const CTokenGroupID& tgID);
template void TGFilterNameUniqueness(CTokenGroupDescriptionNFT& tgDesc, const CTokenGroupID& tgID);

// Transforms tickers into upper case
// Returns true
template <typename T>
void TGFilterUpperCaseTicker(T& tgDesc) {
    std::string strUpperTicker;
    std::transform(tgDesc.strTicker.begin(), tgDesc.strTicker.end(), std::back_inserter(strUpperTicker), ::toupper);

    tgDesc.strTicker = strUpperTicker;
}
template void TGFilterUpperCaseTicker(CTokenGroupDescriptionRegular& tgDesc);
template void TGFilterUpperCaseTicker(CTokenGroupDescriptionMGT& tgDesc);

template <typename TokenGroupDescription>
bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, TokenGroupDescription& tgDesc) {
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
        return GetTxPayload(tx, tgDesc);
    }
    return false;
}
template bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, CTokenGroupDescriptionRegular& tgDesc);
template bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, CTokenGroupDescriptionMGT& tgDesc);
template bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, CTokenGroupDescriptionNFT& tgDesc);

bool CreateTokenGroup(const CTransactionRef tx, const uint256& blockHash, CTokenGroupCreation &newTokenGroupCreation) {
    CTokenGroupInfo tokenGroupInfo;
    CTokenGroupStatus tokenGroupStatus;

    if (tx->nType == TRANSACTION_GROUP_CREATION_REGULAR) {
        CTokenGroupDescriptionRegular tokenGroupDescription;
        if (!GetTokenConfigurationParameters(*tx, tokenGroupInfo, tokenGroupDescription)) return false;
        newTokenGroupCreation = CTokenGroupCreation(tx, blockHash, tokenGroupInfo, std::make_shared<CTokenGroupDescriptionVariant>(tokenGroupDescription), tokenGroupStatus);
    } else if (tx->nType == TRANSACTION_GROUP_CREATION_MGT) {
        CTokenGroupDescriptionMGT tokenGroupDescription;
        if (!GetTokenConfigurationParameters(*tx, tokenGroupInfo, tokenGroupDescription)) return false;
        newTokenGroupCreation = CTokenGroupCreation(tx, blockHash, tokenGroupInfo, std::make_shared<CTokenGroupDescriptionVariant>(tokenGroupDescription), tokenGroupStatus);
    } else if (tx->nType == TRANSACTION_GROUP_CREATION_NFT) {
        CTokenGroupDescriptionNFT tokenGroupDescription;
        if (!GetTokenConfigurationParameters(*tx, tokenGroupInfo, tokenGroupDescription)) return false;
        newTokenGroupCreation = CTokenGroupCreation(tx, blockHash, tokenGroupInfo, std::make_shared<CTokenGroupDescriptionVariant>(tokenGroupDescription), tokenGroupStatus);
    }
    return true;
}

bool CheckGroupConfigurationTxRegular(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view)
{
    if (tx.nType != TRANSACTION_GROUP_CREATION_REGULAR) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-protx-type");
    }

    if (!IsAnyOutputGroupedCreation(tx)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-tx");
    }

    CTokenGroupDescriptionRegular tgDesc;
    if (!GetTxPayload(tx, tgDesc)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-protx-payload");
    }
    if (tgDesc.nDecimalPos > 16) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-param");
    }

    if (tgDesc.nVersion == 0 || tgDesc.nVersion > CTokenGroupDescriptionRegular::CURRENT_VERSION) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-version");
    }

    return true;
}

bool CheckGroupConfigurationTxMGT(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view)
{
    if (tx.nType != TRANSACTION_GROUP_CREATION_MGT) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-protx-type");
    }

    if (!IsAnyOutputGroupedCreation(tx)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-tx");
    }

    CTokenGroupDescriptionMGT tgDesc;
    if (!GetTxPayload(tx, tgDesc)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-protx-payload");
    }
    if (tgDesc.nDecimalPos > 16) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-param");
    }
    if (!tgDesc.blsPubKey.IsValid()) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-key");
    }

    if (tgDesc.nVersion == 0 || tgDesc.nVersion > CTokenGroupDescriptionMGT::CURRENT_VERSION) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-version");
    }

    return true;
}

bool CheckGroupConfigurationTxNFT(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view)
{
    if (tx.nType != TRANSACTION_GROUP_CREATION_NFT) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-protx-type");
    }

    if (!IsAnyOutputGroupedCreation(tx)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-tx");
    }

    CTokenGroupDescriptionNFT tgDesc;
    if (!GetTxPayload(tx, tgDesc)) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-protx-payload");
    }
    if (!tgDesc.vchData.size() > MAX_TX_NFT_DATA) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-data");
    }

    if (tgDesc.nVersion == 0 || tgDesc.nVersion > CTokenGroupDescriptionNFT::CURRENT_VERSION) {
        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-version");
    }

    return true;
}
