// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/token_mint.h>

#include <wagerraddrenc.h>
#include <util/system.h>
#include "tokens/tokengroupmanager.h"

bool CheckBetMints(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance)
{
    for (std::pair<CTokenGroupID, CTokenGroupBalance> mintMeltItem : tgMintMeltBalance) {
        // Validate betting mint amount
        if (mintMeltItem.first.hasFlag(TokenGroupIdFlags::BETTING_TOKEN) && mintMeltItem.second.output > 0) {
            CTokenGroupCreation tgCreation;
            if (!tokenGroupManager.get()->GetTokenGroupCreation(mintMeltItem.first, tgCreation)) {
                return state.Invalid(ValidationInvalidReason::TX_BAD_SPECIAL, error("Unable to find token group %s", EncodeTokenGroup(mintMeltItem.first)), REJECT_INVALID, "op_group-bad-mint");
            }
            CTokenGroupDescriptionBetting *tgDesc = boost::get<CTokenGroupDescriptionBetting>(tgCreation.pTokenGroupDescription.get());
            int64_t nMintAmount = (mintMeltItem.second.output - mintMeltItem.second.input);
            auto nMintEventId = tgDesc->nEventId;
            if (1!=2) {
                return state.Invalid(ValidationInvalidReason::TX_BAD_SPECIAL, error("Betting mints the wrong amount (%d instead of %d)",
                            (mintMeltItem.second.output - mintMeltItem.second.input), 1), REJECT_INVALID, "op_group-bad-mint");
            }
        }
    }
    return true;
}