// Copyright (c) 2015-2017 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <clientversion.h>
#include "coins.h"
#include "consensus/tokengroups.h"
#include "dstencode.h"
#include "bytzaddrenc.h"
#include "logging.h"
#include "rpc/server.h"
#include <streams.h>
#include "tokens/tokengroupmanager.h"

bool AnyInputsGrouped(const CTransaction &transaction, const int nHeight, const CCoinsViewCache& view, const CTokenGroupID tgID) {
    bool anyInputsGrouped = false;
    if (!transaction.IsGenerated() && !transaction.HasZerocoinSpendInputs()) {

        if (!view.HaveInputs(transaction))
            return false;

        if (nHeight >= Params().GetConsensus().ATPStartHeight) {
            // Now iterate through the inputs to match to token inputs
            for (const auto &inp : transaction.vin)
            {
                const COutPoint &prevout = inp.prevout;
                const Coin &coin = view.AccessCoin(prevout);
                if (coin.IsSpent()) {
                    LogPrint(BCLog::TOKEN, "%s - Checking token group for spent coin\n", __func__);
                    return false;
                }
                // no prior coins can be grouped.
                if (coin.nHeight < Params().GetConsensus().ATPStartHeight)
                    continue;
                const CScript &script = coin.out.scriptPubKey;

                CTokenGroupInfo tokenGrp(script);
                // The prevout should never be invalid because that would mean that this node accepted a block with an
                // invalid OP_GROUP tx in it.
                if (tokenGrp.invalid)
                    continue;
                if (tokenGrp.associatedGroup == tgID) {
                    LogPrint(BCLog::TOKEN, "%s - Matched a TokenGroup input: [%s] at height [%d]\n", __func__, coin.out.ToString(), coin.nHeight);
                    anyInputsGrouped = true;
                }
            }
        }
    }

    return anyInputsGrouped;
}

bool IsTokenManagementKey(CScript script) {
    // Initially, the TokenManagementKey enables management token operations
    // When the MGTToken is created, the MGTToken enables management token operations
    if (!tokenGroupManager.get()->MGTTokensCreated()) {
        CTxDestination payeeDest;
        ExtractDestination(script, payeeDest);
        return EncodeDestination(payeeDest) == Params().GetConsensus().strTokenManagementKey;
    }
    return false;
}

bool IsMGTInput(CScript script) {
    // Initially, the TokenManagementKey enables management token operations
    // When the MGTToken is created, the MGTToken enables management token operations
    if (tokenGroupManager.get()->MGTTokensCreated()) {
        CTokenGroupInfo grp(script);
        return grp.associatedGroup == tokenGroupManager.get()->GetMGTID();
    }
    return false;
}

bool CheckTokenGroups(const CTransaction &tx, CValidationState &state, const CCoinsViewCache &view, std::unordered_map<CTokenGroupID, CTokenGroupBalance>& gBalance)
{
    gBalance.clear();

    // Tokens minted from the tokenGroupManagement address can create management tokens
    bool anyInputsGroupManagement = false;

    CScript firstOpReturn;

    // Iterate through all the outputs constructing the final balances of every group.
    for (const auto &outp : tx.vout)
    {
        const CScript &scriptPubKey = outp.scriptPubKey;
        CTokenGroupInfo tokenGrp(scriptPubKey);
        if ((outp.nValue == 0) && (firstOpReturn.size() == 0) && (outp.scriptPubKey[0] == OP_RETURN))
        {
            firstOpReturn = outp.scriptPubKey; // Used later if this is a group creation transaction
        }
        if (tokenGrp.invalid)
            return state.Invalid(false, REJECT_INVALID, "bad OP_GROUP");
        if (tokenGrp.associatedGroup != NoGroup)
        {
            gBalance[tokenGrp.associatedGroup].numOutputs += 1;

            if (tokenGrp.quantity > 0)
            {
                if (std::numeric_limits<CAmount>::max() - gBalance[tokenGrp.associatedGroup].output < tokenGrp.quantity)
                    return state.Invalid(false, REJECT_INVALID, "token overflow");
                gBalance[tokenGrp.associatedGroup].output += tokenGrp.quantity;
            }
            else if (tokenGrp.quantity == 0)
            {
                return state.Invalid(false, REJECT_INVALID, "OP_GROUP quantity is zero");
            }
            else // this is an authority output
            {
                gBalance[tokenGrp.associatedGroup].ctrlOutputPerms |= (GroupAuthorityFlags)tokenGrp.quantity;
            }
        }
    }

    // Now iterate through the inputs applying them to match outputs.
    // If any input utxo address matches a non-bitcoin group address, defer since this could be a mint or burn
    for (const auto &inp : tx.vin)
    {
        const COutPoint &prevout = inp.prevout;
        const Coin &coin = view.AccessCoin(prevout);
        if (coin.IsSpent()) // should never happen because you've already CheckInputs(tx,...)
        {
            LogPrint(BCLog::TOKEN, "%s - Checking token group for spent coin\n", __func__);
            return state.Invalid(false, REJECT_INVALID, "already-spent");
        }

        const CScript &script = coin.out.scriptPubKey;
        anyInputsGroupManagement = anyInputsGroupManagement || IsTokenManagementKey(script);

        // no prior coins can be grouped.
        if (coin.nHeight < Params().GetConsensus().ATPStartHeight)
            continue;

        anyInputsGroupManagement = anyInputsGroupManagement || IsMGTInput(script);

        CTokenGroupInfo tokenGrp(script);
        // The prevout should never be invalid because that would mean that this node accepted a block with an
        // invalid OP_GROUP tx in it.
        if (tokenGrp.invalid)
            continue;
        CAmount amount = tokenGrp.quantity;
        if (tokenGrp.controllingGroupFlags() != GroupAuthorityFlags::NONE)
        {
            auto temp = tokenGrp.controllingGroupFlags();
            // outputs can have all the permissions of inputs, except for 1 special case
            // If CCHILD is not set, no outputs can be authorities (so unset the CTRL flag)
            if (hasCapability(temp, GroupAuthorityFlags::CCHILD))
            {
                gBalance[tokenGrp.associatedGroup].allowedCtrlOutputPerms |= temp;
                if (hasCapability(temp, GroupAuthorityFlags::SUBGROUP))
                    gBalance[tokenGrp.associatedGroup].allowedSubgroupCtrlOutputPerms |= temp;
            }
            // Track what permissions this transaction has
            gBalance[tokenGrp.associatedGroup].ctrlPerms |= temp;
        }
        if (tokenGrp.associatedGroup.hasFlag(TokenGroupIdFlags::STICKY_MELT)) {
            gBalance[tokenGrp.associatedGroup].ctrlPerms |= GroupAuthorityFlags::MELT;
        }
        if ((tokenGrp.associatedGroup != NoGroup) && !tokenGrp.isAuthority())
        {
            if (std::numeric_limits<CAmount>::max() - gBalance[tokenGrp.associatedGroup].input < amount)
                return state.Invalid(false, REJECT_INVALID, "token overflow");
            gBalance[tokenGrp.associatedGroup].input += amount;
        }
    }

    // Now pass thru the outputs applying parent group capabilities to any subgroups
    for (auto &txo : gBalance)
    {
        CTokenGroupID group = txo.first;
        CTokenGroupBalance &bal = txo.second;
        if (group.isSubgroup())
        {
            CTokenGroupID parentgrp = group.parentGroup();
            auto parentSearch = gBalance.find(parentgrp);
            if (parentSearch != gBalance.end()) // The parent group is part of the inputs
            {
                CTokenGroupBalance &parentData = parentSearch->second;
                if (hasCapability(parentData.ctrlPerms, GroupAuthorityFlags::SUBGROUP))
                {
                    // Give the subgroup has all the capabilities the parent group had,
                    // except no recursive subgroups so remove the subgrp authority bit.
                    bal.ctrlPerms |= parentData.ctrlPerms & ~(GroupAuthorityFlags::SUBGROUP);
                }

                // Give the subgroup authority printing permissions as specified by the parent group
                bal.allowedCtrlOutputPerms |=
                    parentData.allowedSubgroupCtrlOutputPerms & ~(GroupAuthorityFlags::SUBGROUP);
            }
        }
    }

    // Now pass thru the outputs ensuring balance or mint/melt permission
    for (auto &txo : gBalance)
    {
        CTokenGroupBalance &bal = txo.second;
        // If it has an authority, with no input authority, check mint
        if (hasCapability(bal.ctrlOutputPerms, GroupAuthorityFlags::CTRL) &&
            (bal.ctrlPerms == GroupAuthorityFlags::NONE))
        {
            CHashWriter mintGrp(SER_GETHASH, PROTOCOL_VERSION);
            mintGrp << tx.vin[0].prevout;
            if (firstOpReturn.size())
            {
                std::vector<unsigned char> data(firstOpReturn.begin(), firstOpReturn.end());
                mintGrp << data;
            }
            mintGrp << (((uint64_t)bal.ctrlOutputPerms) & ~((uint64_t)GroupAuthorityFlags::ALL_BITS));
            CTokenGroupID newGrpId(mintGrp.GetHash());

            if (newGrpId == txo.first) // This IS new group because id matches hash, so allow all authority.
            {
                if (bal.numOutputs != 1) // only allow the single authority tx during a create
                    return state.Invalid(false, REJECT_GROUP_IMBALANCE, "grp-invalid-create",
                         "Multiple grouped outputs created during group creation transaction");

                if (newGrpId.hasFlag(TokenGroupIdFlags::MGT_TOKEN))
                {
                    if (anyInputsGroupManagement) {
                        LogPrint(BCLog::TOKEN, "%s - Group management creation transaction. newGrpId=[%s]\n", __func__, EncodeTokenGroup(newGrpId));
                    } else {
                        return state.Invalid(false, REJECT_INVALID, "grp-invalid-tx",
                            "No group management capability at any input address - unable to create management token");
                    }
                }
                if (newGrpId.hasFlag(TokenGroupIdFlags::STICKY_MELT))
                {
                    if (anyInputsGroupManagement) {
                        LogPrint(BCLog::TOKEN, "%s - Group with sticky melt created. newGrpId=[%s]\n", __func__, EncodeTokenGroup(newGrpId));
                    } else {
                        return state.Invalid(false, REJECT_INVALID, "grp-invalid-tx",
                            "No group management capability at any input address - unable to set stick_melt");
                    }
                }

                bal.allowedCtrlOutputPerms = bal.ctrlPerms = GroupAuthorityFlags::ALL;
            }
            else
            {
                if (((uint64_t)bal.ctrlOutputPerms & (uint64_t)~GroupAuthorityFlags::ALL_BITS) != 0)
                {
                    return state.Invalid(false, REJECT_INVALID, "grp-invalid-tx",
                         "Only mint transactions can have a nonce");
                }
            }
        }

        if ((bal.input > bal.output) && !hasCapability(bal.ctrlPerms, GroupAuthorityFlags::MELT))
        {
            return state.Invalid(false, REJECT_GROUP_IMBALANCE, "grp-invalid-melt",
                "Group input exceeds output, but no melt permission");
        }
        if ((bal.input < bal.output) && !hasCapability(bal.ctrlPerms, GroupAuthorityFlags::MINT))
        {
            return state.Invalid(false, REJECT_GROUP_IMBALANCE, "grp-invalid-mint",
                "Group output exceeds input, but no mint permission");
        }
        // Some output permissions are set that are not in the inputs
        if (((uint64_t)(bal.ctrlOutputPerms & GroupAuthorityFlags::ALL)) &
            ~((uint64_t)(bal.allowedCtrlOutputPerms & GroupAuthorityFlags::ALL)))
        {
                return state.Invalid(false, REJECT_GROUP_IMBALANCE, "grp-invalid-perm",
                "Group output permissions exceeds input permissions");
        }
    }

    return true;
}
