// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRANSACTIONRECORD_H
#define BITCOIN_QT_TRANSACTIONRECORD_H

#include <amount.h>
#include <dstencode.h>
#include <script/ismine.h>
#include <uint256.h>


namespace interfaces {
class Node;
class Wallet;
struct WalletTx;
struct WalletTxStatus;
}

class UniValue;
class CWallet;
class CWalletTx;

/** UI model for transaction status. The transaction status is the part of a transaction that will change over time.
 */
class TransactionStatus
{
public:
    TransactionStatus():
        countsForBalance(false), lockedByInstantSend(false), lockedByChainLocks(false), sortKey(""),
        matures_in(0), status(Unconfirmed), depth(0), open_for(0), cur_num_blocks(-1),
        cachedChainLockHeight(-1), needsUpdate(false)
    { }

    enum Status {
        Confirmed,          /**< Have 6 or more confirmations (normal tx) or fully mature (mined tx) **/
        /// Normal (sent/received) transactions
        OpenUntilDate,      /**< Transaction not yet final, waiting for date */
        OpenUntilBlock,     /**< Transaction not yet final, waiting for block */
        Unconfirmed,        /**< Not yet mined into a block **/
        Confirming,         /**< Confirmed, but waiting for the recommended number of confirmations **/
        Conflicted,         /**< Conflicts with other transaction or mempool **/
        Abandoned,          /**< Abandoned from the wallet **/
        /// Generated (mined) transactions
        Immature,           /**< Mined but waiting for maturity */
        NotAccepted         /**< Mined but not accepted */
    };

    /// Transaction counts towards available balance
    bool countsForBalance;
    /// Transaction was locked via InstantSend
    bool lockedByInstantSend;
    /// Transaction was locked via ChainLocks
    bool lockedByChainLocks;
    /// Sorting key based on status
    std::string sortKey;

    /** @name Generated (mined) transactions
       @{*/
    int matures_in;
    /**@}*/

    /** @name Reported status
       @{*/
    Status status;
    int64_t depth;
    int64_t open_for; /**< Timestamp if status==OpenUntilDate, otherwise number
                      of additional blocks that need to be mined before
                      finalization */
    /**@}*/

    /** Current number of blocks (to know whether cached status is still valid) */
    int cur_num_blocks;

    //** Know when to update transaction for chainlocks **/
    int cachedChainLockHeight;

    bool needsUpdate;
};

/** UI model for a transaction. A core transaction can be represented by multiple UI transactions if it has
    multiple outputs.
 */
class TransactionRecord{
public:
    enum Type
    {
        Other,
        Generated,
        StakeMint,
        MNReward,
        SendToAddress,
        SendToOther,
        RecvWithAddress,
        RecvFromOther,
        SendToSelf,
        RecvWithCoinJoin,
        CoinJoinMixing,
        CoinJoinCollateralPayment,
        CoinJoinMakeCollaterals,
        CoinJoinCreateDenominations,
        CoinJoinSend
    };

    /** Number of confirmation recommended for accepting a transaction */
    static const int RecommendedNumConfirmations = 6;

    TransactionRecord():
            hash(), time(0), type(Other), strAddress(""), debit(0), credit(0), idx(0)
    {
        txDest = DecodeDestination(strAddress);
    }

    TransactionRecord(uint256 _hash, int64_t _time):
            hash(_hash), time(_time), type(Other), strAddress(""), debit(0),
            credit(0), idx(0)
    {
        txDest = DecodeDestination(strAddress);
    }

    TransactionRecord(uint256 _hash, int64_t _time,
                Type _type, const std::string &_strAddress,
                const CAmount& _debit, const CAmount& _credit):
            hash(_hash), time(_time), type(_type), strAddress(_strAddress), debit(_debit), credit(_credit),
            idx(0)
    {
        txDest = DecodeDestination(strAddress);
    }

    /** Decompose CWallet transaction to model transaction records.
     */
    static bool showTransaction();
    static std::vector<TransactionRecord> decomposeTransaction(interfaces::Wallet& wallet, const interfaces::WalletTx& wtx);

    /** @name Immutable transaction attributes
      @{*/
    uint256 hash;
    int64_t time;
    Type type;
    std::string strAddress;
    CTxDestination txDest;

    CAmount debit;
    CAmount credit;
    /**@}*/

    /** Subtransaction index, for sort key */
    int idx;

    /** Status: can change with block chain update */
    TransactionStatus status;

    /** Whether the transaction was sent/received with a watch-only address */
    bool involvesWatchAddress;

    /// Label
    std::string label;

    /** Return the unique identifier for this transaction (part) */
    std::string getTxHash() const;

    /** Return the output index of the subtransaction  */
    int getOutputIndex() const;

    /** Update status from core wallet tx.
     */
    void updateStatus(const interfaces::WalletTxStatus& wtx, int numBlocks, int64_t adjustedTime, int chainLockHeight);

    /** Return whether a status update is needed.
     */
    bool statusUpdateNeeded(int numBlocks, int chainLockHeight) const;

    /** Update label from address book.
     */
    void updateLabel(interfaces::Wallet& wallet);

    /**
     * Return stringified transaction record type
     */
    std::string GetTransactionRecordType() const;
    std::string GetTransactionRecordType(Type type) const;

    /**
     * Return stringified transaction status
     */
    std::string GetTransactionStatus() const;
    std::string GetTransactionStatus(TransactionStatus::Status status) const;
};

void ListTransactionRecords(std::shared_ptr<CWallet> pwallet, const uint256& hash, const std::string& strAccount, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter);

#endif // BITCOIN_QT_TRANSACTIONRECORD_H
