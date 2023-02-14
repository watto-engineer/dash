// Copyright (c) 2015-2017 The Bitcoin Unlimited developers
// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "tokens/tokengroupwallet.h"

#include "consensus/tokengroups.h"
#include "consensus/validation.h"
#include "key_io.h"
#include <evo/specialtx.h>
#include "net.h"
#include "rpc/util.h"
#include "script/tokengroup.h"
#include "tokens/tokengroupmanager.h"
#include "util/moneystr.h"
#include "util/strencodings.h"
#include "util/translation.h"
#include "validation.h" // for cs_main
#include "wallet/wallet.h"
#include "wallet/fees.h"

// allow this many times fee overpayment, rather than make a change output
#define FEE_FUDGE 2

// Approximate size of signature in a script -- used for guessing fees
const unsigned int TX_SIG_SCRIPT_LEN = 72;

/* Grouped transactions look like this:

GP2PKH:

OP_DATA(group identifier)
OP_DATA(SerializeAmount(amount))
OP_GROUP
OP_DROP
OP_DUP
OP_HASH160
OP_DATA(pubkeyhash)
OP_EQUALVERIFY
OP_CHECKSIG

GP2SH:

OP_DATA(group identifier)
OP_DATA(CompactSize(amount))
OP_GROUP
OP_DROP
OP_HASH160 [20-byte-hash-value] OP_EQUAL

FUTURE: GP2SH version 2:

OP_DATA(group identifier)
OP_DATA(CompactSize(amount))
OP_GROUP
OP_DROP
OP_HASH256 [32-byte-hash-value] OP_EQUAL
*/

void GetAllGroupBalances(const CWallet *wallet, std::unordered_map<CTokenGroupID, CAmount> &balances)
{
    std::vector<COutput> coins;
    wallet->FilterCoins(coins, [&balances](const CWalletTx *tx, const CTxOut *out) {
        CTokenGroupInfo tg(out->scriptPubKey);
        if ((tg.associatedGroup != NoGroup) && !tg.isAuthority()) // must be sitting in any group address
        {
            if (tg.quantity > std::numeric_limits<CAmount>::max() - balances[tg.associatedGroup])
                balances[tg.associatedGroup] = std::numeric_limits<CAmount>::max();
            else
                balances[tg.associatedGroup] += tg.quantity;
        }
        return false; // I don't want to actually filter anything
    });
}

void GetAllGroupBalancesAndAuthorities(const CWallet *wallet, std::unordered_map<CTokenGroupID, CAmount> &balances, std::unordered_map<CTokenGroupID, GroupAuthorityFlags> &authorities, const int nMinDepth)
{
    std::vector<COutput> coins;
    wallet->FilterCoins(coins, [&balances, &authorities](const CWalletTx *tx, const CTxOut *out) {
        CTokenGroupInfo tg(out->scriptPubKey);
        if ((tg.associatedGroup != NoGroup)) {
            authorities[tg.associatedGroup] |= tg.controllingGroupFlags();
            if (!tg.isAuthority()) {
                if (tg.quantity > std::numeric_limits<CAmount>::max() - balances[tg.associatedGroup])
                    balances[tg.associatedGroup] = std::numeric_limits<CAmount>::max();
                else
                    balances[tg.associatedGroup] += tg.quantity;
            } else {
                balances[tg.associatedGroup] += 0;
            }
        }
        return false; // I don't want to actually filter anything
    }, nMinDepth);
}

void ListAllGroupAuthorities(const CWallet *wallet, std::vector<COutput> &coins) {
    wallet->FilterCoins(coins, [](const CWalletTx *tx, const CTxOut *out) {
        CTokenGroupInfo tg(out->scriptPubKey);
        if (tg.isAuthority()) {
            return true;
        } else {
            return false;
        }
    });
}

void ListGroupAuthorities(const CWallet *wallet, std::vector<COutput> &coins, const CTokenGroupID &grpID) {
    wallet->FilterCoins(coins, [grpID](const CWalletTx *tx, const CTxOut *out) {
        CTokenGroupInfo tg(out->scriptPubKey);
        if (tg.isAuthority() && tg.associatedGroup == grpID) {
            return true;
        } else {
            return false;
        }
    });
}

CAmount GetGroupBalance(const CTokenGroupID &grpID, const CTxDestination &dest, const CWallet *wallet)
{
    std::vector<COutput> coins;
    CAmount balance = 0;
    wallet->FilterCoins(coins, [grpID, dest, &balance](const CWalletTx *tx, const CTxOut *out) {
        CTokenGroupInfo tg(out->scriptPubKey);
        if ((grpID == tg.associatedGroup) && !tg.isAuthority()) // must be sitting in group address
        {
            bool useit = dest == CTxDestination(CNoDestination());
            if (!useit)
            {
                CTxDestination address;
                txnouttype whichType;
                if (ExtractDestination(out->scriptPubKey, address, whichType))
                {
                    if (address == dest)
                        useit = true;
                }
            }
            if (useit)
            {
                if (tg.quantity > std::numeric_limits<CAmount>::max() - balance)
                    balance = std::numeric_limits<CAmount>::max();
                else
                    balance += tg.quantity;
            }
        }
        return false;
    });
    return balance;
}

void GetGroupBalanceAndAuthorities(CAmount &balance, GroupAuthorityFlags &authorities, const CTokenGroupID &grpID, const CTxDestination &dest, const CWallet *wallet, const int nMinDepth)
{
    std::vector<COutput> coins;
    balance = 0;
    authorities = GroupAuthorityFlags::NONE;
    wallet->FilterCoins(coins, [grpID, dest, &balance, &authorities](const CWalletTx *tx, const CTxOut *out) {
        CTokenGroupInfo tg(out->scriptPubKey);
        if ((grpID == tg.associatedGroup)) // must be sitting in group address
        {
            bool useit = dest == CTxDestination(CNoDestination());
            if (!useit)
            {
                CTxDestination address;
                txnouttype whichType;
                if (ExtractDestination(out->scriptPubKey, address, whichType))
                {
                    if (address == dest)
                        useit = true;
                }
            }
            if (useit)
            {
                authorities |= tg.controllingGroupFlags();
                if (!tg.isAuthority()) {
                    if (tg.quantity > std::numeric_limits<CAmount>::max() - balance)
                        balance = std::numeric_limits<CAmount>::max();
                    else
                        balance += tg.quantity;
                } else {
                    balance += 0;
                }
            }
        }
        return false;
    }, nMinDepth);
}

void GetGroupCoins(const CWallet *wallet, std::vector<COutput>& coins, CAmount& balance, const CTokenGroupID &grpID, const CTxDestination &dest) {
    wallet->FilterCoins(coins, [dest, grpID, &balance](const CWalletTx *tx, const CTxOut *out) {
        CTokenGroupInfo tg(out->scriptPubKey);
        if ((grpID == tg.associatedGroup) && !tg.isAuthority()) {
            bool useit = dest == CTxDestination(CNoDestination());
            if (!useit) {
                CTxDestination address;
                txnouttype whichType;
                if (ExtractDestination(out->scriptPubKey, address, whichType)) {
                    if (address == dest)
                        useit = true;
                }
            }
            if (useit) {
                if (tg.quantity > std::numeric_limits<CAmount>::max() - balance) {
                    balance = std::numeric_limits<CAmount>::max();
                } else {
                    balance += tg.quantity;
                }
                return true;
            }
        }
        return false;
    });
}

void GetGroupAuthority(const CWallet *wallet, std::vector<COutput>& coins, GroupAuthorityFlags flags, const CTokenGroupID &grpID, const CTxDestination &dest) {
    // For now: return only the first matching coin (in coins[0])
    // Todo:
    // - Find the coin with the minimum amount of authorities
    // - If needed, combine coins to provide the requested authorities
    wallet->FilterCoins(coins, [flags, dest, grpID](const CWalletTx *tx, const CTxOut *out) {
        CTokenGroupInfo tg(out->scriptPubKey);
        if ((grpID == tg.associatedGroup) && tg.isAuthority() && hasCapability(tg.controllingGroupFlags(), flags)) {
            bool useit = dest == CTxDestination(CNoDestination());
            if (!useit) {
                CTxDestination address;
                txnouttype whichType;
                if (ExtractDestination(out->scriptPubKey, address, whichType)) {
                    if (address == dest)
                        useit = true;
                }
            }
            if (useit) {
                return true;
            }
        }
        return false;
    });
}

bool NearestGreaterCoin(const std::vector<COutput> &coins, CAmount amt, COutput &chosenCoin)
{
    bool ret = false;
    CAmount curBest = std::numeric_limits<CAmount>::max();

    for (const auto &coin : coins)
    {
        CAmount camt = coin.GetValue();
        if ((camt > amt) && (camt < curBest))
        {
            curBest = camt;
            chosenCoin = coin;
            ret = true;
        }
    }

    return ret;
}


CAmount CoinSelection(const std::vector<COutput> &coins, CAmount amt, std::vector<COutput> &chosenCoins)
{
    // simple algorithm grabs until amount exceeded
    CAmount cur = 0;

    for (const auto &coin : coins)
    {
        chosenCoins.push_back(coin);
        cur += coin.GetValue();
        if (cur >= amt)
            break;
    }
    return cur;
}

CAmount GroupCoinSelection(const std::vector<COutput> &coins, CAmount amt, std::vector<COutput> &chosenCoins)
{
    // simple algorithm grabs until amount exceeded
    CAmount cur = 0;

    for (const auto &coin : coins)
    {
        chosenCoins.push_back(coin);
        CTokenGroupInfo tg(coin.tx->tx->vout[coin.i].scriptPubKey);
        cur += tg.quantity;
        if (cur >= amt)
            break;
    }
    return cur;
}

bool RenewAuthority(const COutput &authority, std::vector<CRecipient> &outputs, ReserveDestination &childAuthorityKey)
{
    // The melting authority is consumed.  A wallet can decide to create a child authority or not.
    // In this simple wallet, we will always create a new melting authority if we spend a renewable
    // (CCHILD is set) one.
    CTokenGroupInfo tg(authority.GetScriptPubKey());

    if (tg.allowsRenew())
    {
        // Get a new address from the wallet to put the new mint authority in.
        CTxDestination authDest;
        childAuthorityKey.GetReservedDestination(authDest, true);
        CScript script = GetScriptForDestination(authDest, tg.associatedGroup, (CAmount)(tg.controllingGroupFlags() & GroupAuthorityFlags::ALL_BITS));
        CRecipient recipient = {script, GROUPED_SATOSHI_AMT, false};
        outputs.push_back(recipient);
    }

    return true;
}

template <typename TokenGroupDescription>
void ConstructTx(CTransactionRef &txNew, const std::vector<COutput> &chosenCoins, const std::vector<CRecipient> &outputs,
    CAmount totalGroupedNeeded, CTokenGroupID grpID, CWallet *wallet, const TokenGroupDescription& ptgDesc)
{
    CAmount totalGroupedAvailable = 0;

    CMutableTransaction tx;
    ReserveDestination groupChangeKeyReservation(wallet);

    {
        // Add group outputs based on the passed recipient data to the tx.
        for (const CRecipient &recipient : outputs)
        {
            CTxOut txout(recipient.nAmount, recipient.scriptPubKey);
            tx.vout.push_back(txout);
        }

        for (const auto &coin : chosenCoins)
        {
            CTxIn txin(coin.GetOutPoint());
            tx.vin.push_back(txin);

            // Gather data on the provided inputs, and add them to the tx.
            CTokenGroupInfo tg(coin.GetScriptPubKey());
            if (!tg.isInvalid() && tg.associatedGroup != NoGroup && !tg.isAuthority())
            {
                if (tg.associatedGroup == grpID){
                    totalGroupedAvailable += tg.quantity;
                }
            }
        }

        if (totalGroupedAvailable > totalGroupedNeeded) // need to make a group change output
        {
            CTxDestination newDest;
            if (!groupChangeKeyReservation.GetReservedDestination(newDest, true))
                throw JSONRPCError(
                    RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

            CTxOut txout(GROUPED_SATOSHI_AMT,
                GetScriptForDestination(newDest, grpID, totalGroupedAvailable - totalGroupedNeeded));
            tx.vout.push_back(txout);
        }

        // Now add fee
        CAmount fee;
        int nChangePosRet = -1;
        bilingual_str strError;
        bool lockUnspents;
        std::set<int> setSubtractFeeFromOutputs;
        CCoinControl coinControl;

        if (ptgDesc != nullptr) {
            tx.nVersion = 3;
            tx.nType = ptgDesc->SPECIALTX_TYPE;
            SetTxPayload(tx, *ptgDesc);
        };

        if (!wallet->FundTransaction(tx, fee, nChangePosRet, strError, lockUnspents, setSubtractFeeFromOutputs, coinControl)) {
            throw JSONRPCError(RPC_WALLET_ERROR, strError.translated);
        }

        if (!wallet->SignTransaction(tx))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, "Signing transaction failed");
        }
    }

    txNew = MakeTransactionRef(std::move(tx));

    for (auto vout : txNew->vout) {
        CTokenGroupInfo tgInfo(vout.scriptPubKey);
        if (!tgInfo.isInvalid()) {
            CTokenGroupCreation tgCreation;
            tokenGroupManager.get()->GetTokenGroupCreation(tgInfo.associatedGroup, tgCreation);
            LogPrint(BCLog::TOKEN, "%s - name[%s] amount[%d]\n", __func__, tgDescGetName(*tgCreation.pTokenGroupDescription), tgInfo.quantity);
        }
    }

    // I'll manage my own keys because I have multiple.  Passing a valid key down breaks layering.
    ReserveDestination dummy(wallet);
    CValidationState state;
    wallet->CommitTransaction(txNew, {}, {});

    groupChangeKeyReservation.KeepDestination();
}
template void ConstructTx(CTransactionRef &txNew, const std::vector<COutput> &chosenCoins, const std::vector<CRecipient> &outputs,
    CAmount totalGroupedNeeded, CTokenGroupID grpID, CWallet *wallet, const std::shared_ptr<CTokenGroupDescriptionRegular>& ptgDesc);
template void ConstructTx(CTransactionRef &txNew, const std::vector<COutput> &chosenCoins, const std::vector<CRecipient> &outputs,
    CAmount totalGroupedNeeded, CTokenGroupID grpID, CWallet *wallet, const std::shared_ptr<CTokenGroupDescriptionMGT>& ptgDesc);
template void ConstructTx(CTransactionRef &txNew, const std::vector<COutput> &chosenCoins, const std::vector<CRecipient> &outputs,
    CAmount totalGroupedNeeded, CTokenGroupID grpID, CWallet *wallet, const std::shared_ptr<CTokenGroupDescriptionNFT>& ptgDesc);
template void ConstructTx(CTransactionRef &txNew, const std::vector<COutput> &chosenCoins, const std::vector<CRecipient> &outputs,
    CAmount totalGroupedNeeded, CTokenGroupID grpID, CWallet *wallet, const std::shared_ptr<CTokenGroupDescriptionBetting>& ptgDesc);

void ConstructTx(CTransactionRef &txNew, const std::vector<COutput> &chosenCoins, const std::vector<CRecipient> &outputs,
    CAmount totalGroupedNeeded, CTokenGroupID grpID, CWallet *wallet)
{
    CAmount totalGroupedAvailable = 0;

    CMutableTransaction tx;
    ReserveDestination groupChangeKeyReservation(wallet);

    {
        // Add group outputs based on the passed recipient data to the tx.
        for (const CRecipient &recipient : outputs)
        {
            CTxOut txout(recipient.nAmount, recipient.scriptPubKey);
            tx.vout.push_back(txout);
        }

        for (const auto &coin : chosenCoins)
        {
            CTxIn txin(coin.GetOutPoint());
            tx.vin.push_back(txin);

            // Gather data on the provided inputs, and add them to the tx.
            CTokenGroupInfo tg(coin.GetScriptPubKey());
            if (!tg.isInvalid() && tg.associatedGroup != NoGroup && !tg.isAuthority())
            {
                if (tg.associatedGroup == grpID){
                    totalGroupedAvailable += tg.quantity;
                }
            }
        }

        if (totalGroupedAvailable > totalGroupedNeeded) // need to make a group change output
        {
            CTxDestination newDest;
            CPubKey newKey;
            if (!groupChangeKeyReservation.GetReservedDestination(newDest, true))
                throw JSONRPCError(
                    RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

            CTxOut txout(GROUPED_SATOSHI_AMT,
                GetScriptForDestination(newDest, grpID, totalGroupedAvailable - totalGroupedNeeded));
            tx.vout.push_back(txout);
        }

        // Now add fee
        CAmount fee;
        int nChangePosRet = -1;
        bilingual_str strError;
        bool lockUnspents;
        std::set<int> setSubtractFeeFromOutputs;
        CCoinControl coinControl;

        if (!wallet->FundTransaction(tx, fee, nChangePosRet, strError, lockUnspents, setSubtractFeeFromOutputs, coinControl)) {
            throw JSONRPCError(RPC_WALLET_ERROR, strError.translated);
        }

        if (!wallet->SignTransaction(tx))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, "Signing transaction failed");
        }
    }

    txNew = MakeTransactionRef(std::move(tx));

    for (auto vout : txNew->vout) {
        CTokenGroupInfo tgInfo(vout.scriptPubKey);
        if (!tgInfo.isInvalid()) {
            CTokenGroupCreation tgCreation;
            tokenGroupManager.get()->GetTokenGroupCreation(tgInfo.associatedGroup, tgCreation);
            LogPrint(BCLog::TOKEN, "%s - name[%s] amount[%d]\n", __func__, tgDescGetName(*tgCreation.pTokenGroupDescription), tgInfo.quantity);
        }
    }

    // I'll manage my own keys because I have multiple.  Passing a valid key down breaks layering.
    ReserveDestination dummy(wallet);
    CValidationState state;
    wallet->CommitTransaction(txNew, {}, {});

    groupChangeKeyReservation.KeepDestination();
}

void GroupMelt(CTransactionRef &txNew, const CTokenGroupID &grpID, CAmount totalNeeded, CWallet *wallet)
{
    std::string strError;
    std::vector<CRecipient> outputs; // Melt has no outputs (except change)
    CAmount totalAvailable = 0;
    LOCK2(cs_main, wallet->cs_wallet);

    std::vector<COutput> coins;

    if (grpID.hasFlag(TokenGroupIdFlags::STICKY_MELT)) {
        // Find meltable coins
        coins.clear();
        wallet->FilterCoins(coins, [grpID](const CWalletTx *tx, const CTxOut *out) {
            CTokenGroupInfo tg(out->scriptPubKey);
            // must be a grouped output sitting in group address
            return ((grpID == tg.associatedGroup) && !tg.isAuthority());
        });

        // Get a near but greater quantity
        std::vector<COutput> chosenCoins;
        totalAvailable = GroupCoinSelection(coins, totalNeeded, chosenCoins);

        if (totalAvailable < totalNeeded)
        {
            std::string strError;
            strError = strprintf("Not enough tokens in the wallet.  Need %d more.", tokenGroupManager.get()->TokenValueFromAmount(totalNeeded - totalAvailable, grpID));
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strError);
        } else if (totalAvailable == totalNeeded) {
            CRecipient recipient = {CScript() << OP_RETURN, GROUPED_SATOSHI_AMT, false};

            outputs.emplace_back(recipient);
        }

        // by passing a fewer tokens available than are actually in the inputs, there is a surplus.
        // This surplus will be melted.
        ConstructTx(txNew, chosenCoins, outputs, totalNeeded, grpID, wallet);
    } else {
        // Find melt authority
        int nOptions = wallet->FilterCoins(coins, [grpID](const CWalletTx *tx, const CTxOut *out) {
            CTokenGroupInfo tg(out->scriptPubKey);
            if ((tg.associatedGroup == grpID) && tg.allowsMelt())
            {
                return true;
            }
            return false;
        });

        // if its a subgroup look for a parent authority that will work
        // As an idiot-proofing step, we only allow parent authorities that can be renewed, but that is a
        // preference coded in this wallet, not a group token requirement.
        if ((nOptions == 0) && (grpID.isSubgroup()))
        {
            // if its a subgroup look for a parent authority that will work
            nOptions = wallet->FilterCoins(coins, [grpID](const CWalletTx *tx, const CTxOut *out) {
                CTokenGroupInfo tg(out->scriptPubKey);
                if (tg.isAuthority() && tg.allowsRenew() && tg.allowsSubgroup() && tg.allowsMelt() &&
                    (tg.associatedGroup == grpID.parentGroup()))
                {
                    return true;
                }
                return false;
            });
        }

        if (nOptions == 0)
        {
            strError = "To melt coins, an authority output with melt capability is needed.";
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strError);
        }
        COutput authority(nullptr, 0, 0, false, false, false);
        // Just pick the first one for now.
        for (auto coin : coins)
        {
            authority = coin;
            break;
        }

        // Find meltable coins
        coins.clear();
        wallet->FilterCoins(coins, [grpID](const CWalletTx *tx, const CTxOut *out) {
            CTokenGroupInfo tg(out->scriptPubKey);
            // must be a grouped output sitting in group address
            return ((grpID == tg.associatedGroup) && !tg.isAuthority());
        });

        // Get a near but greater quantity
        std::vector<COutput> chosenCoins;
        totalAvailable = GroupCoinSelection(coins, totalNeeded, chosenCoins);

        if (totalAvailable < totalNeeded)
        {
            std::string strError;
            strError = strprintf("Not enough tokens in the wallet.  Need %d more.", tokenGroupManager.get()->TokenValueFromAmount(totalNeeded - totalAvailable, grpID));
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strError);
        }

        chosenCoins.push_back(authority);

        ReserveDestination childAuthorityKey(wallet);
        RenewAuthority(authority, outputs, childAuthorityKey);
        // by passing a fewer tokens available than are actually in the inputs, there is a surplus.
        // This surplus will be melted.
        ConstructTx(txNew, chosenCoins, outputs, totalNeeded, grpID, wallet);
        childAuthorityKey.KeepDestination();
    }
}

void GroupSend(CTransactionRef &txNew,
    const CTokenGroupID &grpID,
    const std::vector<CRecipient> &outputs,
    CAmount totalNeeded,
    CWallet *wallet)
{
    LOCK2(cs_main, wallet->cs_wallet);
    std::string strError;
    std::vector<COutput> coins;
    std::vector<COutput> chosenCoins;

    CAmount totalAvailable = 0;
    wallet->FilterCoins(coins, [grpID, &totalAvailable](const CWalletTx *tx, const CTxOut *out) {
        CTokenGroupInfo tg(out->scriptPubKey);
        if ((grpID == tg.associatedGroup) && !tg.isAuthority())
        {
            totalAvailable += tg.quantity;
            return true;
        }
        return false;
    });

    if (totalAvailable < totalNeeded)
    {
        strError = strprintf("Not enough tokens in the wallet.  Need %d more.", tokenGroupManager.get()->TokenValueFromAmount(totalNeeded - totalAvailable, grpID));
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strError);
    }

    // Get a near but greater quantity
    totalAvailable = GroupCoinSelection(coins, totalNeeded, chosenCoins);

    // Display outputs
    for (auto output : outputs) {
        CTokenGroupInfo tgInfo(output.scriptPubKey);
        if (!tgInfo.isInvalid()) {
            CTokenGroupCreation tgCreation;
            tokenGroupManager.get()->GetTokenGroupCreation(tgInfo.associatedGroup, tgCreation);
            LogPrint(BCLog::TOKEN, "%s - name[%s] amount[%d]\n", __func__, tgDescGetName(*tgCreation.pTokenGroupDescription), tgInfo.quantity);
        }
    }

    ConstructTx(txNew, chosenCoins, outputs, totalNeeded, grpID, wallet);
}

template <typename TokenGroupDescription>
CTokenGroupID findGroupId(const COutPoint &input, const TokenGroupDescription& tgDesc, TokenGroupIdFlags flags, uint64_t &nonce)
{
    CTokenGroupID ret;
    do
    {
        nonce += 1;
        CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
        // mask off any flags in the nonce
        nonce &= ~((uint64_t)GroupAuthorityFlags::ALL_BITS);
        hasher << input;
        hasher << tgDesc;
        hasher << nonce;
        ret = hasher.GetHash();
    } while (ret.bytes()[31] != (uint8_t)flags);
    return ret;
}
template CTokenGroupID findGroupId(const COutPoint &input, const std::shared_ptr<CTokenGroupDescriptionRegular>& tgDesc, TokenGroupIdFlags flags, uint64_t &nonce);
template CTokenGroupID findGroupId(const COutPoint &input, const std::shared_ptr<CTokenGroupDescriptionMGT>& tgDesc, TokenGroupIdFlags flags, uint64_t &nonce);
template CTokenGroupID findGroupId(const COutPoint &input, const std::shared_ptr<CTokenGroupDescriptionNFT>& tgDesc, TokenGroupIdFlags flags, uint64_t &nonce);
template CTokenGroupID findGroupId(const COutPoint &input, const std::shared_ptr<CTokenGroupDescriptionBetting>& tgDesc, TokenGroupIdFlags flags, uint64_t &nonce);
