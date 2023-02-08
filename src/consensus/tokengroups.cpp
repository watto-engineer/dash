// Copyright (c) 2015-2017 The Bitcoin Unlimited developers
// Copyright (c) 2019 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <clientversion.h>
#include "coins.h"
#include "consensus/tokengroups.h"
#include "key_io.h"
#include <evo/specialtx.h>
#include "rpc/server.h"
#include "wagerraddrenc.h"
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

    // Iterate through all the outputs constructing the final balances of every group.
    for (const auto &outp : tx.vout)
    {
        const CScript &scriptPubKey = outp.scriptPubKey;
        CTokenGroupInfo tokenGrp(scriptPubKey);
        if (tokenGrp.invalid)
            return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "bad OP_GROUP");
        if (tokenGrp.associatedGroup != NoGroup)
        {
            gBalance[tokenGrp.associatedGroup].numOutputs += 1;

            if (tokenGrp.quantity > 0)
            {
                if (std::numeric_limits<CAmount>::max() - gBalance[tokenGrp.associatedGroup].output < tokenGrp.quantity)
                    return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "token overflow");
                gBalance[tokenGrp.associatedGroup].output += tokenGrp.quantity;
            }
            else if (tokenGrp.quantity == 0)
            {
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "OP_GROUP quantity is zero");
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
            return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "already-spent");
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
        if (tokenGrp.associatedGroup != NoGroup)
        {
            gBalance[tokenGrp.associatedGroup].numInputs += 1;
            if (!tokenGrp.isAuthority())
            {
                if (std::numeric_limits<CAmount>::max() - gBalance[tokenGrp.associatedGroup].input < amount)
                    return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "token overflow");
                gBalance[tokenGrp.associatedGroup].input += amount;
            }
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
            if (tx.nType == TRANSACTION_GROUP_CREATION_REGULAR) {
                CTokenGroupDescriptionRegular tgDesc;
                if (!GetTxPayload(tx, tgDesc)) {
                    return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-protx-payload");
                }
                mintGrp << tgDesc;
            } else if (tx.nType == TRANSACTION_GROUP_CREATION_MGT) {
                CTokenGroupDescriptionMGT tgDesc;
                if (!GetTxPayload(tx, tgDesc)) {
                    return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-protx-payload");
                }
                mintGrp << tgDesc;
            } else if (tx.nType == TRANSACTION_GROUP_CREATION_NFT) {
                CTokenGroupDescriptionNFT tgDesc;
                if (!GetTxPayload(tx, tgDesc)) {
                    return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-protx-payload");
                }
                mintGrp << tgDesc;
            }
            mintGrp << (((uint64_t)bal.ctrlOutputPerms) & ~((uint64_t)GroupAuthorityFlags::ALL_BITS));
            CTokenGroupID newGrpId(mintGrp.GetHash());

            if (newGrpId == txo.first) // This IS new group because id matches hash, so allow all authority.
            {
                if (bal.numOutputs != 1) // only allow the single authority tx during a create
                    return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_GROUP_IMBALANCE, "grp-invalid-create",
                        "Multiple grouped outputs created during group creation transaction");

                // Regular token
                if (!newGrpId.hasFlag(TokenGroupIdFlags::MGT_TOKEN) && !newGrpId.hasFlag(TokenGroupIdFlags::NFT_TOKEN)) {
                    if (tx.nType != TRANSACTION_GROUP_CREATION_REGULAR) {
                        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-token-flag", "This is not a regular token group");
                    }
                    bal.allowedCtrlOutputPerms = bal.ctrlPerms = GroupAuthorityFlags::ALL;
                }
                // Management token
                if (newGrpId.hasFlag(TokenGroupIdFlags::MGT_TOKEN) && !newGrpId.hasFlag(TokenGroupIdFlags::NFT_TOKEN)) {
                    if (tx.nType != TRANSACTION_GROUP_CREATION_MGT) {
                        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-token-flag", "This is not a management token group");
                    }
                    if (anyInputsGroupManagement) {
                        LogPrint(BCLog::TOKEN, "%s - Group management creation transaction. newGrpId=[%s]\n", __func__, EncodeTokenGroup(newGrpId));
                    } else {
                        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-tx",
                            "No group management capability at any input address - unable to create management token");
                    }
                    bal.allowedCtrlOutputPerms = bal.ctrlPerms = GroupAuthorityFlags::ALL;
                }
                // NFT token
                if (!newGrpId.hasFlag(TokenGroupIdFlags::MGT_TOKEN) && newGrpId.hasFlag(TokenGroupIdFlags::NFT_TOKEN)) {
                    if (tx.nType != TRANSACTION_GROUP_CREATION_NFT) {
                        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-token-flag", "This is not an NFT token group");
                    }
                    bal.allowedCtrlOutputPerms = bal.ctrlPerms = GroupAuthorityFlags::ALL_NFT;
                }
                // Invalid combination token
                if (newGrpId.hasFlag(TokenGroupIdFlags::MGT_TOKEN) && newGrpId.hasFlag(TokenGroupIdFlags::NFT_TOKEN)) {
                    return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-token-flag", "Cannot have both the Management and NFT flag");
                }

                if (newGrpId.hasFlag(TokenGroupIdFlags::STICKY_MELT))
                {
                    if (anyInputsGroupManagement) {
                        LogPrint(BCLog::TOKEN, "%s - Group with sticky melt created. newGrpId=[%s]\n", __func__, EncodeTokenGroup(newGrpId));
                    } else {
                        return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-tx",
                            "No group management capability at any input address - unable to set stick_melt");
                    }
                }
            }
            else
            {
                if (((uint64_t)bal.ctrlOutputPerms & (uint64_t)~GroupAuthorityFlags::ALL_BITS) != 0)
                {
                    return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-invalid-tx",
                         "Only mint transactions can have a nonce");
                }
            }
        }

        if ((bal.input > bal.output) && !hasCapability(bal.ctrlPerms, GroupAuthorityFlags::MELT))
        {
            return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_GROUP_IMBALANCE, "grp-invalid-melt",
                "Group input exceeds output, but no melt permission");
        }
        if (bal.input < bal.output)
        {
            if (!hasCapability(bal.ctrlPerms, GroupAuthorityFlags::MINT))
            {
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_GROUP_IMBALANCE, "grp-invalid-mint",
                    "Group output exceeds input, but no mint permission");
            }
            if (txo.first.hasFlag(TokenGroupIdFlags::NFT_TOKEN) && hasCapability(bal.allowedCtrlOutputPerms, GroupAuthorityFlags::MINT)) {
                // Redundant
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_GROUP_IMBALANCE, "grp-invalid-mint",
                    "NFT mint cannot have mint authority output");
            }
        }
        // Some output permissions are set that are not in the inputs
        if (((uint64_t)(bal.ctrlOutputPerms & GroupAuthorityFlags::ALL)) &
            ~((uint64_t)(bal.allowedCtrlOutputPerms & GroupAuthorityFlags::ALL)))
        {
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_GROUP_IMBALANCE, "grp-invalid-perm",
                "Group output permissions exceeds input permissions");
        }
    }

    return true;
}

bool GetTokenBalance(const CTransaction& tx, const CTokenGroupID& tgID, CValidationState& state, const CCoinsViewCache& view, CAmount& nCredit, CAmount& nDebit)
{
    nCredit = 0;
    nDebit = 0;
    for (const auto &inp : tx.vin) {
        const COutPoint &prevout = inp.prevout;
        LogPrintf("%s - COutpoint prevout[%s]\n", __func__, prevout.ToString());
        const Coin &coin = view.AccessCoin(prevout);
        if (coin.IsSpent()) {
            return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "bad-protx-inputs-spent");
        }

        const CScript &script = coin.out.scriptPubKey;
        if (coin.nHeight < Params().GetConsensus().ATPStartHeight)
            continue;

        CTokenGroupInfo tokenGrp(script);
        if (tokenGrp.invalid || tokenGrp.isAuthority() || !tokenGrp.associatedGroup.isSubgroup())
            continue;

        CTokenGroupID parentgrp = tokenGrp.associatedGroup.parentGroup();
        if (!tokenGroupManager.get()->MatchesGVT(parentgrp))
            continue;

        std::vector<unsigned char> subgroupData = tokenGrp.associatedGroup.GetSubGroupData();
        std::string subgroup = std::string(subgroupData.begin(), subgroupData.end());

        if (subgroup != "credit")
            continue;

        nCredit += tokenGrp.quantity;
    }
    for (const auto &outp : tx.vout) {
        const CScript &scriptPubKey = outp.scriptPubKey;
        CTokenGroupInfo tokenGrp(scriptPubKey);

        if (tokenGrp.invalid)
            return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "bad-protx-grouped-outputs");

        if (tokenGrp.isAuthority() || !tokenGrp.associatedGroup.isSubgroup())
            continue;

        CTokenGroupID parentgrp = tokenGrp.associatedGroup.parentGroup();
        if (!tokenGroupManager.get()->MatchesGVT(parentgrp))
            continue;

        std::vector<unsigned char> subgroupData = tokenGrp.associatedGroup.GetSubGroupData();
        std::string subgroup = std::string(subgroupData.begin(), subgroupData.end());

        if (subgroup != "credit")
            continue;

        nDebit += tokenGrp.quantity;
    }
    return true;
}
