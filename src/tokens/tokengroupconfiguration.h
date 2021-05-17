// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_GROUP_CONFIGURATION_H
#define TOKEN_GROUP_CONFIGURATION_H

#include "blockencodings.h"
#include "consensus/tokengroups.h"
#include "tokens/tokengroupdescription.h"

#include <unordered_map>

class CBlockIndex;

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
    std::shared_ptr<CTokenGroupDescriptionBase> pTokenGroupDescription;
    CTokenGroupStatus status;

    CTokenGroupCreation() : creationTransaction(MakeTransactionRef()), pTokenGroupDescription(std::make_shared<CTokenGroupDescriptionBase>()) {};

    CTokenGroupCreation(CTransactionRef creationTransaction, uint256 creationBlockHash, CTokenGroupInfo tokenGroupInfo, std::shared_ptr<CTokenGroupDescriptionBase> pTokenGroupDescription, CTokenGroupStatus tokenGroupStatus)
        : creationTransaction(creationTransaction), creationBlockHash(creationBlockHash), tokenGroupInfo(tokenGroupInfo), pTokenGroupDescription(pTokenGroupDescription), status(tokenGroupStatus) {}

    bool ValidateDescription();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(REF(TransactionCompressor(creationTransaction)));
        READWRITE(creationBlockHash);
        READWRITE(tokenGroupInfo);
        if (ser_action.ForRead()) {
            if (creationTransaction->nType == TRANSACTION_GROUP_CREATION_REGULAR) {
                CTokenGroupDescriptionRegular tgDesc;
                READWRITE(tgDesc);
                pTokenGroupDescription = std::make_shared<CTokenGroupDescriptionRegular>(tgDesc);
            } else if (creationTransaction->nType == TRANSACTION_GROUP_CREATION_MGT) {
                CTokenGroupDescriptionMGT tgDesc;
                READWRITE(tgDesc);
                pTokenGroupDescription = std::make_shared<CTokenGroupDescriptionMGT>(tgDesc);
            }
        } else {
            if (creationTransaction->nType == TRANSACTION_GROUP_CREATION_REGULAR) {
                CTokenGroupDescriptionRegular* tgDesc = static_cast<CTokenGroupDescriptionRegular*>(pTokenGroupDescription.get());
                READWRITE(*tgDesc);
            } else if (creationTransaction->nType == TRANSACTION_GROUP_CREATION_MGT) {
                CTokenGroupDescriptionMGT* tgDesc = static_cast<CTokenGroupDescriptionMGT*>(pTokenGroupDescription.get());
                READWRITE(*tgDesc);
            }
        }
    }
    bool operator==(const CTokenGroupCreation &c)
    {
        if (c.tokenGroupInfo.invalid || tokenGroupInfo.invalid)
            return false;
        return (*creationTransaction == *c.creationTransaction && creationBlockHash == c.creationBlockHash && tokenGroupInfo == c.tokenGroupInfo);
    }
};

void TGFilterCharacters(CTokenGroupCreation &tokenGroupCreation);
void TGFilterUniqueness(CTokenGroupCreation &tokenGroupCreation);
void TGFilterUpperCaseTicker(CTokenGroupCreation &tokenGroupCreation);

template <typename TokenGroupDescription>
bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, std::shared_ptr<TokenGroupDescription>& tgDesc);

bool CreateTokenGroup(const CTransactionRef tx, const uint256& blockHash, CTokenGroupCreation &newTokenGroupCreation);

bool CheckGroupConfigurationTxRegular(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view);
bool CheckGroupConfigurationTxMGT(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state, const CCoinsViewCache& view);

#endif
