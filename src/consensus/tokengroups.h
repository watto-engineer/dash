// Copyright (c) 2018 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CONSENSUS_TOKEN_GROUPS_H
#define CONSENSUS_TOKEN_GROUPS_H

#include "chainparams.h"
#include "consensus/validation.h"
#include "tokens/groups.h"
#include <unordered_map>

class CCoinsViewCache;

/** Transaction cannot be committed on my fork */
static const unsigned int REJECT_GROUP_IMBALANCE = 0x104;

// class that just track of the amounts of each group coming into and going out of a transaction
class CTokenGroupBalance
{
public:
    CTokenGroupBalance()
        : ctrlPerms(GroupAuthorityFlags::NONE), allowedCtrlOutputPerms(GroupAuthorityFlags::NONE),
          allowedSubgroupCtrlOutputPerms(GroupAuthorityFlags::NONE), ctrlOutputPerms(GroupAuthorityFlags::NONE),
          input(0), output(0), numOutputs(0)
    {
    }
    // CTokenGroupInfo groups; // possible groups
    GroupAuthorityFlags ctrlPerms; // what permissions are provided in inputs
    GroupAuthorityFlags allowedCtrlOutputPerms; // What permissions are provided in inputs with CHILD set
    GroupAuthorityFlags allowedSubgroupCtrlOutputPerms; // What permissions are provided in inputs with CHILD set
    GroupAuthorityFlags ctrlOutputPerms; // What permissions are enabled in outputs
    CAmount input;
    CAmount output;
    uint64_t numInputs;
    uint64_t numOutputs;
};

// Verify that the token groups in this transaction properly balance
bool CheckTokenGroups(const CTransaction &tx, CValidationState &state, const CCoinsViewCache &view, std::unordered_map<CTokenGroupID, CTokenGroupBalance>& gBalance);

bool AnyInputsGrouped(const CTransaction &transaction, const int nHeight, const CCoinsViewCache& view, const CTokenGroupID tgID);
bool GetTokenBalance(const CTransaction& tx, const CTokenGroupID& tgID, CValidationState& state, const CCoinsViewCache& view, CAmount& nCredit, CAmount& nDebit);

#endif
