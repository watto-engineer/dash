// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2014-2022 The Dash Core developers
// Copyright (c) 2020-2021 The ION Core developers
// Copyright (c) 2021 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <transactionrecord.h>

#include <chain.h>
#include <interfaces/wallet.h>

#include <wallet/ismine.h>
#include <timedata.h>
#include <univalue.h>
#include <validation.h>

#include <llmq/quorums_chainlocks.h>

#include <stdint.h>

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction()
{
    // There are currently no cases where we hide transactions, but
    // we may want to use this in the future for things like RBF.
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
std::vector<TransactionRecord> TransactionRecord::decomposeTransaction(interfaces::Wallet& wallet, const interfaces::WalletTx& wtx)
{
    std::vector<TransactionRecord> parts;
    int64_t nTime = wtx.time;
    CAmount nCredit = wtx.credit;
    CAmount nDebit = wtx.debit;
    CAmount nNet = nCredit - nDebit;
    uint256 hash = wtx.tx->GetHash();
    std::map<std::string, std::string> mapValue = wtx.value_map;

    if (wtx.tx->IsCoinStake()) {
        TransactionRecord sub(hash, nTime);
        CTxDestination address;
        if (!ExtractDestination(wtx.tx->vout[1].scriptPubKey, address))
            return parts;

        isminetype mine = wtx.txout_is_mine[1];
        if (mine) {
            // WAGERR stake reward
            sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
            sub.type = TransactionRecord::StakeMint;
            sub.credit = nNet + wtx.immature_credit;
            sub.strAddress = EncodeDestination(wtx.txout_address[1]);
            sub.txDest = wtx.txout_address[1];
            sub.updateLabel(wallet);
        } else {
            //Masternode reward
            CTxDestination destMN;
            int nIndexMN = wtx.tx->vout.size() - 1;
            mine = wtx.txout_is_mine[nIndexMN];
            if (ExtractDestination(wtx.tx->vout[nIndexMN].scriptPubKey, destMN) && mine) {
                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                sub.type = TransactionRecord::MNReward;
                sub.credit = wtx.tx->vout[nIndexMN].nValue;
                sub.strAddress = EncodeDestination(wtx.txout_address[nIndexMN]);
                sub.txDest = wtx.txout_address[nIndexMN];
                sub.updateLabel(wallet);
            }
        }
        parts.push_back(sub);
    } else if (nNet > 0 || wtx.tx->IsCoinBase())
    {
        //
        // Credit
        //
        for(unsigned int i = 0; i < wtx.tx->vout.size(); i++)
        {
            const CTxOut& txout = wtx.tx->vout[i];
            isminetype mine = wtx.txout_is_mine[i];
            if(mine)
            {
                TransactionRecord sub(hash, nTime);
                sub.idx = i; // vout index
                sub.credit = txout.nValue;
                sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
                if (wtx.txout_address_is_mine[i])
                {
                    // Received by Wagerr Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.strAddress = EncodeDestination(wtx.txout_address[i]);
                    sub.txDest = wtx.txout_address[i];
                    sub.updateLabel(wallet);
                }
                else
                {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = TransactionRecord::RecvFromOther;
                    sub.strAddress = mapValue["from"];
                    sub.txDest = DecodeDestination(sub.strAddress);
                }
                if (wtx.is_coinbase)
                {
                    // Generated
                    sub.type = TransactionRecord::Generated;
                }

                parts.push_back(sub);
            }
        }
    }
    else
    {
        bool involvesWatchAddress = false;
        isminetype fAllFromMe = ISMINE_SPENDABLE;
        for (const isminetype mine : wtx.txin_is_mine)
        {
            if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if(fAllFromMe > mine) fAllFromMe = mine;
        }

        isminetype fAllToMe = ISMINE_SPENDABLE;
        for (const isminetype mine : wtx.txout_is_mine)
        {
            if(mine & ISMINE_WATCH_ONLY) involvesWatchAddress = true;
            if(fAllToMe > mine) fAllToMe = mine;
        }

        if(wtx.is_denominate) {
            parts.push_back(TransactionRecord(hash, nTime, TransactionRecord::CoinJoinMixing, "", -nDebit, nCredit));
            parts.back().involvesWatchAddress = false;   // maybe pass to TransactionRecord as constructor argument
        }
        else if (fAllFromMe && fAllToMe)
        {
            // Payment to self
            // TODO: this section still not accurate but covers most cases,
            // might need some additional work however

            TransactionRecord sub(hash, nTime);
            // Payment to self by default
            sub.type = TransactionRecord::SendToSelf;
            sub.strAddress = "";
            for (auto it = wtx.txout_address.begin(); it != wtx.txout_address.end(); ++it) {
                if (it != wtx.txout_address.begin()) sub.strAddress += ", ";
                sub.strAddress += EncodeDestination(*it);
            }

            if(mapValue["DS"] == "1")
            {
                sub.type = TransactionRecord::CoinJoinSend;
                CTxDestination address;
                if (ExtractDestination(wtx.tx->vout[0].scriptPubKey, address))
                {
                    // Sent to Wagerr Address
                    sub.strAddress = EncodeDestination(address);
                    sub.txDest = address;
                    sub.updateLabel(wallet);
                }
                else
                {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.strAddress = mapValue["to"];
                    sub.txDest = DecodeDestination(sub.strAddress);
                }
            }

            CAmount nChange = wtx.change;

            sub.debit = -(nDebit - nChange);
            sub.credit = nCredit - nChange;
            parts.push_back(sub);
            parts.back().involvesWatchAddress = involvesWatchAddress;   // maybe pass to TransactionRecord as constructor argument
        }
        else if (fAllFromMe)
        {
            //
            // Debit
            //
            CAmount nTxFee = nDebit - wtx.tx->GetValueOut();

            for (unsigned int nOut = 0; nOut < wtx.tx->vout.size(); nOut++)
            {
                const CTxOut& txout = wtx.tx->vout[nOut];
                TransactionRecord sub(hash, nTime);
                sub.idx = nOut;
                sub.involvesWatchAddress = involvesWatchAddress;

                if(wtx.txout_is_mine[nOut])
                {
                    // Ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                if (!boost::get<CNoDestination>(&wtx.txout_address[nOut]))
                {
                    // Sent to Wagerr Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.strAddress = EncodeDestination(wtx.txout_address[nOut]);
                    sub.txDest = wtx.txout_address[nOut];
                    sub.updateLabel(wallet);
                }
                else
                {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type = TransactionRecord::SendToOther;
                    sub.strAddress = mapValue["to"];
                    sub.txDest = DecodeDestination(sub.strAddress);
                }

                if(mapValue["DS"] == "1")
                {
                    sub.type = TransactionRecord::CoinJoinSend;
                }

                CAmount nValue = txout.nValue;
                /* Add fee to first output */
                if (nTxFee > 0)
                {
                    nValue += nTxFee;
                    nTxFee = 0;
                }
                sub.debit = -nValue;

                parts.push_back(sub);
            }
        }
        else
        {
            //
            // Mixed debit transaction, can't break down payees
            //
            parts.push_back(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0));
            parts.back().involvesWatchAddress = involvesWatchAddress;
        }
    }

    return parts;
}

void TransactionRecord::updateStatus(const interfaces::WalletTxStatus& wtx, int numBlocks, int chainLockHeight, int64_t block_time)
{
    // Determine transaction status

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%01d-%010u-%03d",
        wtx.block_height,
        wtx.is_coinbase ? 1 : 0,
        wtx.is_coinstake ? 1 : 0,
        wtx.time_received,
        idx);
    status.countsForBalance = wtx.is_trusted && !(wtx.blocks_to_maturity > 0);
    status.depth = wtx.depth_in_main_chain;
    status.cur_num_blocks = numBlocks;
    status.cachedChainLockHeight = chainLockHeight;
    status.lockedByChainLocks = wtx.is_chainlocked;
    status.lockedByInstantSend = wtx.is_islocked;

    const bool up_to_date = ((int64_t)QDateTime::currentMSecsSinceEpoch() / 1000 - block_time < MAX_BLOCK_TIME_GAP);
    if (up_to_date && !wtx.is_final) {
        if (wtx.lock_time < LOCKTIME_THRESHOLD) {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.lock_time - numBlocks;
        }
        else
        {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.lock_time;
        }
    }
    // For generated transactions, determine maturity
    else if(type == TransactionRecord::Generated || type == TransactionRecord::StakeMint || type == TransactionRecord::MNReward)
    {
        if (wtx.blocks_to_maturity > 0)
        {
            status.status = TransactionStatus::Immature;

            if (wtx.is_in_main_chain)
            {
                status.matures_in = wtx.blocks_to_maturity;
            }
            else
            {
                status.status = TransactionStatus::NotAccepted;
            }
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    else
    {
        if (status.depth < 0)
        {
            status.status = TransactionStatus::Conflicted;
        }
        else if (status.depth == 0)
        {
            status.status = TransactionStatus::Unconfirmed;
            if (wtx.is_abandoned)
                status.status = TransactionStatus::Abandoned;
        }
        else if (status.depth < RecommendedNumConfirmations && !status.lockedByChainLocks)
        {
            status.status = TransactionStatus::Confirming;
        }
        else
        {
            status.status = TransactionStatus::Confirmed;
        }
    }
    status.needsUpdate = false;
}

bool TransactionRecord::statusUpdateNeeded(int numBlocks, int chainLockHeight) const
{
    return status.cur_num_blocks != numBlocks || status.needsUpdate
        || (!status.lockedByChainLocks && status.cachedChainLockHeight != chainLockHeight);
}

void TransactionRecord::updateLabel(interfaces::Wallet& wallet)
{
    if (IsValidDestination(txDest)) {
        std::string name;
        if (wallet.getAddress(txDest, &name, /* is_mine= */ nullptr, /* purpose= */ nullptr)) {
            label = name;
        } else {
            label = "";
        }
    }
}

std::string TransactionRecord::getTxHash() const
{
    return hash.ToString();
}

int TransactionRecord::getOutputIndex() const
{
    return idx;
}

std::string TransactionRecord::GetTransactionRecordType() const
{
    return GetTransactionRecordType(type);
}

std::string TransactionRecord::GetTransactionRecordType(Type type) const
{
    switch (type)
    {
        case Other: return "Other";
        case Generated: return "Generated";
        case StakeMint: return "StakeMint";
        case MNReward: return "MNReward";
        case SendToAddress: return "SendToAddress";
        case SendToOther: return "SendToOther";
        case RecvWithAddress: return "RecvWithAddress";
        case RecvFromOther: return "RecvFromOther";
        case SendToSelf: return "SendToSelf";
        case RecvWithCoinJoin: return "RecvWithCoinJoin";
        case CoinJoinMixing: return "CoinJoinMixing";
        case CoinJoinCollateralPayment: return "CoinJoinCollateralPayment";
        case CoinJoinMakeCollaterals: return "CoinJoinMakeCollaterals";
        case CoinJoinCreateDenominations: return "CoinJoinCreateDenominations";
        case CoinJoinSend: return "CoinJoinSend";
    }
    return NULL;
}

std::string TransactionRecord::GetTransactionStatus() const
{
    return GetTransactionStatus(status.status);
}
std::string TransactionRecord::GetTransactionStatus(TransactionStatus::Status status) const
{
    switch (status)
    {
        case TransactionStatus::Confirmed: return "Confirmed";           /**< Have 6 or more confirmations (normal tx) or fully mature (mined tx) **/
            /// Normal (sent/received) transactions
        case TransactionStatus::OpenUntilDate: return "OpenUntilDate";   /**< Transaction not yet final, waiting for date */
        case TransactionStatus::OpenUntilBlock: return "OpenUntilBlock"; /**< Transaction not yet final, waiting for block */
        case TransactionStatus::Unconfirmed: return "Unconfirmed";       /**< Not yet mined into a block **/
        case TransactionStatus::Confirming: return "Confirmed";          /**< Confirmed, but waiting for the recommended number of confirmations **/
        case TransactionStatus::Conflicted: return "Conflicted";         /**< Conflicts with other transaction or mempool **/
        case TransactionStatus::Abandoned: return "Abandoned";           /**< Abandoned from the wallet **/
            /// Generated (mined) transactions
        case TransactionStatus::Immature: return "Immature";             /**< Mined but waiting for maturity */
        case TransactionStatus::NotAccepted: return "NotAccepted";       /**< Mined but not accepted */
    }
    return NULL;
}

void ListTransactionRecords(std::shared_ptr<CWallet> pwallet, const uint256& hash, const std::string& strAccount, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter)
{
    auto wallet = interfaces::MakeWallet(pwallet);

    interfaces::WalletTxStatus iStatus;
    interfaces::WalletOrderForm iOrderForm;
    bool inMempool;
    int numBlocks;
    int64_t adjustedTime;
    interfaces::WalletTx iWtx = wallet->getWalletTxDetails(hash, iStatus, iOrderForm, inMempool, numBlocks, adjustedTime);

    std::vector<TransactionRecord> vRecs = TransactionRecord::decomposeTransaction(*wallet, iWtx);
    for(auto&& vRec: vRecs) {
        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("type", vRec.GetTransactionRecordType()));
        entry.push_back(Pair("transactionid", vRec.getTxHash()));
        entry.push_back(Pair("outputindex", vRec.getOutputIndex()));
        entry.push_back(Pair("time", vRec.time));
        entry.push_back(Pair("debit", vRec.debit));
        entry.push_back(Pair("credit", vRec.credit));
        entry.push_back(Pair("involvesWatchonly", vRec.involvesWatchAddress));

        if (fLong) {
            int chainlockHeight;
            llmq::CChainLockSig clsig = llmq::chainLocksHandler->GetBestChainLock();
            if (clsig.IsNull()) {
                chainlockHeight = 0;
            } else {
                chainlockHeight = clsig.nHeight;
            }
            if (vRec.statusUpdateNeeded(::::ChainActive().Height(), chainlockHeight))
                vRec.updateStatus(iStatus, numBlocks, adjustedTime, chainlockHeight);

            entry.push_back(Pair("depth", vRec.status.depth));
            entry.push_back(Pair("status", vRec.GetTransactionStatus()));
            entry.push_back(Pair("countsForBalance", vRec.status.countsForBalance));
            entry.push_back(Pair("lockedByInstantSend", vRec.status.lockedByInstantSend));
            entry.push_back(Pair("lockedByChainLocks", vRec.status.lockedByChainLocks));
            entry.push_back(Pair("matures_in", vRec.status.matures_in));
            entry.push_back(Pair("open_for", vRec.status.open_for));
            entry.push_back(Pair("cur_num_blocks", vRec.status.cur_num_blocks));
            entry.push_back(Pair("chainLockHeight", vRec.status.cachedChainLockHeight)); // TODO: identify chainLockHeight / cachedChainLockHeight
        }
        ret.push_back(entry);
    }
}
