// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKEN_GROUP_CONFIGURATION_H
#define TOKEN_GROUP_CONFIGURATION_H

#include "blockencodings.h"
#include "consensus/tokengroups.h"
#include "tokens/tokengroupdescription.h"

#include <unordered_map>

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
    CTokenGroupDescription tokenGroupDescription;
    CTokenGroupStatus status;

    CTokenGroupCreation() : creationTransaction(MakeTransactionRef()){};

    CTokenGroupCreation(CTransactionRef creationTransaction, uint256 creationBlockHash, CTokenGroupInfo tokenGroupInfo, CTokenGroupDescription tokenGroupDescription, CTokenGroupStatus tokenGroupStatus)
        : creationTransaction(creationTransaction), creationBlockHash(creationBlockHash), tokenGroupInfo(tokenGroupInfo), tokenGroupDescription(tokenGroupDescription), status(tokenGroupStatus) {}

    bool ValidateDescription();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(REF(TransactionCompressor(creationTransaction)));
        READWRITE(creationBlockHash);
        READWRITE(tokenGroupInfo);
        READWRITE(tokenGroupDescription);
    }
    bool operator==(const CTokenGroupCreation &c)
    {
        if (c.tokenGroupInfo.invalid || tokenGroupInfo.invalid)
            return false;
        return (*creationTransaction == *c.creationTransaction && creationBlockHash == c.creationBlockHash && tokenGroupInfo == c.tokenGroupInfo && tokenGroupDescription == c.tokenGroupDescription);
    }
};

void TGFilterCharacters(CTokenGroupCreation &tokenGroupCreation);
void TGFilterUniqueness(CTokenGroupCreation &tokenGroupCreation);
void TGFilterUpperCaseTicker(CTokenGroupCreation &tokenGroupCreation);

bool GetTokenConfigurationParameters(const CTransaction &tx, CTokenGroupInfo &tokenGroupInfo, CScript &firstOpReturn);
bool CreateTokenGroup(const CTransactionRef tx, const uint256& blockHash, CTokenGroupCreation &newTokenGroupCreation);

#endif
