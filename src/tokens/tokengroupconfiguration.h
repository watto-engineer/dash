// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_GROUP_CONFIGURATION_H
#define TOKEN_GROUP_CONFIGURATION_H

#include "blockencodings.h"
#include "consensus/tokengroups.h"
#include "tokens/tokengroupdescription.h"

#include <unordered_map>

class CBlockIndex;
class CBettingsView;

class CTokenGroupStatus
{
public:
    std::string messages;

    CTokenGroupStatus() {
        messages = "";
    };

    void AddMessage(std::string statusMessage) {
        messages = statusMessage;
    }
};

class CTokenGroupCreation
{
public:
    CTransactionRef creationTransaction;
    uint256 creationBlockHash;
    CTokenGroupInfo tokenGroupInfo;
    std::shared_ptr<CTokenGroupDescriptionVariant> pTokenGroupDescription;
    CTokenGroupStatus status;

    CTokenGroupCreation() : creationTransaction(MakeTransactionRef()), pTokenGroupDescription(std::make_shared<CTokenGroupDescriptionVariant>()) {};

    CTokenGroupCreation(CTransactionRef creationTransaction, uint256 creationBlockHash, CTokenGroupInfo tokenGroupInfo, std::shared_ptr<CTokenGroupDescriptionVariant> pTokenGroupDescription, CTokenGroupStatus tokenGroupStatus)
        : creationTransaction(creationTransaction), creationBlockHash(creationBlockHash), tokenGroupInfo(tokenGroupInfo), pTokenGroupDescription(pTokenGroupDescription), status(tokenGroupStatus) {}

    bool ValidateDescription();

    SERIALIZE_METHODS(CTokenGroupCreation, obj)
    {
        READWRITE(obj.creationTransaction);
        READWRITE(obj.creationBlockHash);
        READWRITE(obj.tokenGroupInfo);

        if (obj.creationTransaction->nType == TRANSACTION_GROUP_CREATION_REGULAR) {
            CTokenGroupDescriptionRegular tgDesc;
            SER_WRITE(obj, tgDesc = boost::get<CTokenGroupDescriptionRegular>(*obj.pTokenGroupDescription));
            READWRITE(tgDesc);
            SER_READ(obj, obj.pTokenGroupDescription = std::make_shared<CTokenGroupDescriptionVariant>(tgDesc));
        } else if (obj.creationTransaction->nType == TRANSACTION_GROUP_CREATION_MGT) {
            CTokenGroupDescriptionMGT tgDesc;
            SER_WRITE(obj, tgDesc = boost::get<CTokenGroupDescriptionMGT>(*obj.pTokenGroupDescription));
            READWRITE(tgDesc);
            SER_READ(obj, obj.pTokenGroupDescription = std::make_shared<CTokenGroupDescriptionVariant>(tgDesc));
        } else if (obj.creationTransaction->nType == TRANSACTION_GROUP_CREATION_NFT) {
            CTokenGroupDescriptionNFT tgDesc;
            SER_WRITE(obj, tgDesc = boost::get<CTokenGroupDescriptionNFT>(*obj.pTokenGroupDescription));
            READWRITE(tgDesc);
            SER_READ(obj, obj.pTokenGroupDescription = std::make_shared<CTokenGroupDescriptionVariant>(tgDesc));
        }
    }
    bool operator==(const CTokenGroupCreation &c)
    {
        if (c.tokenGroupInfo.invalid || tokenGroupInfo.invalid)
            return false;
        return (*creationTransaction == *c.creationTransaction && creationBlockHash == c.creationBlockHash && tokenGroupInfo == c.tokenGroupInfo);
    }
};

template <typename T>
void TGFilterTickerCharacters(T& tgDesc);
template <typename T>
void TGFilterNameCharacters(T& tgDesc);
template <typename T>
void TGFilterURLCharacters(T& tgDesc);
template <typename T>
void TGFilterTickerUniqueness(T& tgDesc, const CTokenGroupID& tgID);
template <typename T>
void TGFilterNameUniqueness(T& tgDesc, const CTokenGroupID& tgID);
template <typename T>
void TGFilterUpperCaseTicker(T& tgDesc);

template <typename TokenGroupDescription>
bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, TokenGroupDescription& tgDesc);

bool CreateTokenGroup(const CTransactionRef tx, const uint256& blockHash, CTokenGroupCreation &newTokenGroupCreation);

bool CheckGroupConfigurationTxRegular(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view);
bool CheckGroupConfigurationTxMGT(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view);
bool CheckGroupConfigurationTxNFT(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view);
bool CheckGroupConfigurationTxBetting(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view, const CBettingsView& bettingsView);

#endif
