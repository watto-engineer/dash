// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_TOKEN_MINT_H
#define WAGERR_BET_TOKEN_MINT_H

#include <unordered_map>

class CTransaction;
class CValidationState;
class CCoinsViewCache;
class CTokenGroupID;
class CTokenGroupBalance;

bool CheckBetMints(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, const std::unordered_map<CTokenGroupID, CTokenGroupBalance>& tgMintMeltBalance);

#endif // WAGERR_BET_TOKEN_MINT_H