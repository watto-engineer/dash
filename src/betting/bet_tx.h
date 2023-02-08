// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_TX_H
#define WAGERR_BET_TX_H

//#include <util/system.h>
#include <clientversion.h>
#include <script/script.h>
#include <serialize.h>
#include <streams.h>

class CTxOut;
class CTransaction;

#define BTX_PREFIX 'B'

typedef enum BetTxVersions {
    BetTxVersion4   = 0x01,
    BetTxVersion5   = 0x02,
} BetTxVersions;

const uint8_t BetTxVersion_CURRENT = BetTxVersion4;

// The supported betting TX types.
typedef enum BetTxTypes{
    mappingTxType            = 0x01,  // Mapping transaction type identifier.
    plEventTxType            = 0x02,  // Peerless event transaction type identifier.
    plBetTxType              = 0x03,  // Peerless Bet transaction type identifier.
    plResultTxType           = 0x04,  // Peerless Result transaction type identifier.
    plUpdateOddsTxType       = 0x05,  // Peerless update odds transaction type identifier.
    cgEventTxType            = 0x06,  // Chain games event transaction type identifier.
    cgBetTxType              = 0x07,  // Chain games bet transaction type identifier.
    cgResultTxType           = 0x08,  // Chain games result transaction type identifier.
    plSpreadsEventTxType     = 0x09,  // Spread odds transaction type identifier.
    plTotalsEventTxType      = 0x0a,  // Totals odds transaction type identifier.
    plEventPatchTxType       = 0x0b,  // Peerless event patch transaction type identifier.
    plParlayBetTxType        = 0x0c,  // Peerless Parlay Bet transaction type identifier.
    qgBetTxType              = 0x0d,  // Quick Games Bet transaction type identifier.
    plEventZeroingOddsTxType = 0x0e,  // Zeroing odds for event ids transaction type identifier.
    fEventTxType             = 0x0f,  // Field event transaction type identifier.
    fUpdateOddsTxType        = 0x10,  // Field event update odds transaction type identifier.
    fZeroingOddsTxType       = 0x11,  // Field event zeroing odds transaction type identifier.
    fResultTxType            = 0x12,  // Field event result transaction type identifier.
    fBetTxType               = 0x13,  // Field bet transaction type indetifier.
    fParlayBetTxType         = 0x14,  // Field parlay bet transaction type identifier.
    fUpdateMarginTxType      = 0x15,  // Field event update margin transaction type identifier
    fUpdateModifiersTxType   = 0x16,  // Field event update modifiers transaction type identifier.
} BetTxTypes;

bool HasOpReturnOutput(const CTransaction &tx);

// class for serialization common betting header from opcode
class CBettingTxHeader
{
public:
    uint8_t prefix;
    uint8_t version;
    uint8_t txType;

    CBettingTxHeader() : prefix(BTX_PREFIX), version(BetTxVersion_CURRENT), txType(0) {};
    CBettingTxHeader(uint8_t version, uint8_t txType) : prefix(BTX_PREFIX), version(version), txType(txType) {};

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 3;
    }
    SERIALIZE_METHODS(CBettingTxHeader, obj)
    {
        READWRITE(
                obj.prefix,
                obj.version,
                obj.txType
                );
    }
};

// Virtual class for all TX classes
class CBettingTx
{
public:
    virtual ~CBettingTx() = default;
    virtual BetTxTypes GetTxType() const = 0;
};

class CMappingTx : public CBettingTx
{
public:

    uint8_t nMType;
    uint32_t nId;
    std::string sName;

    CMappingTx(): nMType(0), nId(0) {}

    BetTxTypes GetTxType() const override { return mappingTxType; }

    template <typename Stream>
    void Serialize(Stream& s) const
    {
        ser_writedata8(s, nMType);
        // if mapping type is teamMapping (0x03) or contenderMapping (0x06) nId is 4 bytes, else - 2 bytes
        if (nMType == 0x03 || nMType == 0x06) {
            ser_writedata32(s, nId);
        } else {
            ser_writedata16(s, nId);
        }
        char ch;
        for (size_t i = 0; i < sName.size(); i++) {
            ch = (uint8_t) sName[i];
            ser_writedata8(s, ch);
        }
    }

    /**
     * Unserialize from a stream.
     */
    template <typename Stream>
    void Unserialize(Stream& s)
    {
        nMType = ser_readdata8(s);
        // if mapping type is teamMapping (0x03) or contenderMapping (0x06) nId is 4 bytes, else - 2 bytes
        if (nMType == 0x03 || nMType == 0x06) {
            nId = ser_readdata32(s);
        } else {
            uint16_t nId16 = ser_readdata16(s);
            nId = nId16;
        }
        char ch;
        sName.clear();
        while (s.size() != 0) {
            ch = ser_readdata8(s);
            sName += ch;
        }
    }
};

/*
 * Peerless betting TX structures
 */

class CPeerlessEventTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint32_t nStartTime;
    uint16_t nSport;
    uint16_t nTournament;
    uint16_t nStage;
    uint32_t nHomeTeam;
    uint32_t nAwayTeam;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;

    // Default Constructor.
    CPeerlessEventTx() {}

    BetTxTypes GetTxType() const override { return plEventTxType; }

    SERIALIZE_METHODS(CPeerlessEventTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nStartTime);
        READWRITE(obj.nSport);
        READWRITE(obj.nTournament);
        READWRITE(obj.nStage);
        READWRITE(obj.nHomeTeam);
        READWRITE(obj.nAwayTeam);
        READWRITE(obj.nHomeOdds);
        READWRITE(obj.nAwayOdds);
        READWRITE(obj.nDrawOdds);
    }
};

class CFieldEventTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint32_t nStartTime;
    uint16_t nSport;
    uint16_t nTournament;
    uint16_t nStage;
    uint8_t nGroupType;
    uint8_t nMarketType;
    uint32_t nMarginPercent;
    // contenderId : input odds
    std::map<uint32_t, uint32_t> mContendersInputOdds;

    // Default Constructor.
    CFieldEventTx() {}

    BetTxTypes GetTxType() const override { return fEventTxType; }

    SERIALIZE_METHODS(CFieldEventTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nStartTime);
        READWRITE(obj.nSport);
        READWRITE(obj.nTournament);
        READWRITE(obj.nStage);
        READWRITE(obj.nGroupType);
        READWRITE(obj.nMarketType);
        READWRITE(obj.nMarginPercent);
        READWRITE(obj.mContendersInputOdds);
    }
};

class CFieldUpdateOddsTx : public CBettingTx
{
public:
    uint32_t nEventId;
    // contenderId : inputOdds
    std::map<uint32_t, uint32_t> mContendersInputOdds;

    // Default Constructor.
    CFieldUpdateOddsTx() {}

    BetTxTypes GetTxType() const override { return fUpdateOddsTxType; }

    SERIALIZE_METHODS(CFieldUpdateOddsTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.mContendersInputOdds);
    }
};

class CFieldUpdateModifiersTx : public CBettingTx
{
public:
    uint32_t nEventId;
    // contenderId : modifiers
    std::map<uint32_t, uint32_t> mContendersModifires;

    // Default Constructor.
    CFieldUpdateModifiersTx() {}

    BetTxTypes GetTxType() const override { return fUpdateModifiersTxType; }

    SERIALIZE_METHODS(CFieldUpdateModifiersTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.mContendersModifires);
    }
};

class CFieldUpdateMarginTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint32_t nMarginPercent;

    // Default Constructor.
    CFieldUpdateMarginTx() {}

    BetTxTypes GetTxType() const override { return fUpdateMarginTxType; }

    SERIALIZE_METHODS(CFieldUpdateMarginTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nMarginPercent);
    }
};

class CFieldZeroingOddsTx : public CBettingTx
{
public:
    uint32_t nEventId;

    // Default Constructor.
    CFieldZeroingOddsTx() {}

    BetTxTypes GetTxType() const override { return fZeroingOddsTxType; }

    SERIALIZE_METHODS(CFieldZeroingOddsTx, obj)
    {
        READWRITE(obj.nEventId);
    }
};

class CFieldResultTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint8_t nResultType;
    // contenderId : ContenderResult
    std::map<uint32_t, uint8_t> contendersResults;

    // Default Constructor.
    CFieldResultTx() {}

    BetTxTypes GetTxType() const override { return fResultTxType; }

    SERIALIZE_METHODS(CFieldResultTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nResultType);
        READWRITE(obj.contendersResults);
    }
};

class CFieldBetTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint8_t nOutcome;
    uint32_t nContenderId;

    // Default constructor.
    CFieldBetTx() {}
    CFieldBetTx(const uint32_t eventId, const uint8_t marketType, const uint32_t contenderId)
        : nEventId(eventId)
        , nOutcome(marketType)
        , nContenderId(contenderId)
    {}

    BetTxTypes GetTxType() const override { return fBetTxType; }

    SERIALIZE_METHODS(CFieldBetTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nOutcome);
        READWRITE(obj.nContenderId);
    }
};

class CFieldParlayBetTx : public CBettingTx
{
public:
    std::vector<CFieldBetTx> legs;

    // Default constructor.
    CFieldParlayBetTx() {}

    BetTxTypes GetTxType() const override { return fParlayBetTxType; }

    SERIALIZE_METHODS(CFieldParlayBetTx, obj)
    {
        READWRITE(obj.legs);
    }
};

class CPeerlessBetTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint8_t nOutcome;

    // Default constructor.
    CPeerlessBetTx() {}
    CPeerlessBetTx(uint32_t eventId, uint8_t outcome) : nEventId(eventId), nOutcome(outcome) {}

    BetTxTypes GetTxType() const override { return plBetTxType; }

    SERIALIZE_METHODS(CPeerlessBetTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nOutcome);
    }
};

class CPeerlessResultTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint8_t nResultType;
    uint16_t nHomeScore;
    uint16_t nAwayScore;


    // Default Constructor.
    CPeerlessResultTx() {}

    BetTxTypes GetTxType() const override { return plResultTxType; }

    SERIALIZE_METHODS(CPeerlessResultTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nResultType);
        READWRITE(obj.nHomeScore);
        READWRITE(obj.nAwayScore);
    }
};

class CPeerlessUpdateOddsTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;
    uint32_t nDrawOdds;

    // Default Constructor.
    CPeerlessUpdateOddsTx() {}

    BetTxTypes GetTxType() const override { return plUpdateOddsTxType; }

    SERIALIZE_METHODS(CPeerlessUpdateOddsTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nHomeOdds);
        READWRITE(obj.nAwayOdds);
        READWRITE(obj.nDrawOdds);
    }
};

class CPeerlessSpreadsEventTx : public CBettingTx
{
public:
    uint32_t nEventId;
    int16_t nPoints;
    uint32_t nHomeOdds;
    uint32_t nAwayOdds;

    // Default Constructor.
    CPeerlessSpreadsEventTx() {}

    BetTxTypes GetTxType() const override { return plSpreadsEventTxType; }

    SERIALIZE_METHODS(CPeerlessSpreadsEventTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nPoints);
        READWRITE(obj.nHomeOdds);
        READWRITE(obj.nAwayOdds);
    }

};

class CPeerlessTotalsEventTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint16_t nPoints;
    uint32_t nOverOdds;
    uint32_t nUnderOdds;

    // Default Constructor.
    CPeerlessTotalsEventTx() {}

    BetTxTypes GetTxType() const override { return plTotalsEventTxType; }

    SERIALIZE_METHODS(CPeerlessTotalsEventTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nPoints);
        READWRITE(obj.nOverOdds);
        READWRITE(obj.nUnderOdds);
    }
};

class CPeerlessEventPatchTx : public CBettingTx
{
public:
    uint32_t nEventId;
    uint32_t nStartTime;

    CPeerlessEventPatchTx() {}

    BetTxTypes GetTxType() const override { return plEventPatchTxType; }

    SERIALIZE_METHODS(CPeerlessEventPatchTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nStartTime);
    }
};

class CPeerlessParlayBetTx : public CBettingTx
{
public:
    std::vector<CPeerlessBetTx> legs;

    // Default constructor.
    CPeerlessParlayBetTx() {}

    BetTxTypes GetTxType() const override { return plParlayBetTxType; }

    SERIALIZE_METHODS(CPeerlessParlayBetTx, obj)
    {
        READWRITE(obj.legs);
    }
};

/*
 * Chain Games betting TX structures
 */

class CChainGamesEventTx : public CBettingTx
{
public:
    uint16_t nEventId;
    uint16_t nEntryFee;

    // Default Constructor.
    CChainGamesEventTx() {}

    BetTxTypes GetTxType() const override { return cgEventTxType; }

    SERIALIZE_METHODS(CChainGamesEventTx, obj)
    {
        READWRITE(obj.nEventId);
        READWRITE(obj.nEntryFee);
    }
};

class CChainGamesBetTx : public CBettingTx
{
public:
    uint16_t nEventId;

    // Default Constructor.
    CChainGamesBetTx() {}
    CChainGamesBetTx(uint16_t eventId) : nEventId(eventId) {}

    BetTxTypes GetTxType() const override { return cgBetTxType; }

    SERIALIZE_METHODS(CChainGamesBetTx, obj)
    {
        READWRITE(obj.nEventId);
    }

};

class CChainGamesResultTx : public CBettingTx
{
public:
    uint16_t nEventId;

    // Default Constructor.
    CChainGamesResultTx() {}

    BetTxTypes GetTxType() const override { return cgResultTxType; }

    SERIALIZE_METHODS(CChainGamesResultTx, obj)
    {
        READWRITE(obj.nEventId);
    }

};

/*
 * Quick Games betting TX structures
 */

class CQuickGamesBetTx : public CBettingTx
{
public:
    uint8_t gameType;
    std::vector<unsigned char> vBetInfo;

    // Default constructor.
    CQuickGamesBetTx() {}

    BetTxTypes GetTxType() const override { return qgBetTxType; }

    SERIALIZE_METHODS(CQuickGamesBetTx, obj)
    {
        READWRITE(obj.gameType);
        READWRITE(obj.vBetInfo);
    }
};

class CPeerlessEventZeroingOddsTx : public CBettingTx
{
public:
    std::vector<uint32_t> vEventIds;

    // Default Constructor.
    CPeerlessEventZeroingOddsTx() {}

    BetTxTypes GetTxType() const override { return plEventZeroingOddsTxType; }

    SERIALIZE_METHODS(CPeerlessEventZeroingOddsTx, obj)
    {
        READWRITE(obj.vEventIds);
    }
};

std::unique_ptr<CBettingTx> ParseBettingTx(const CTxOut& txOut);

template<typename BetTx>
bool EncodeBettingTxPayload(const CBettingTxHeader& header, const BetTx& bettingTx, std::vector<unsigned char>& betData) {
    betData.clear();
    switch (header.version) {
        case BetTxVersion4:
        {
            CDataStream ss(SER_NETWORK, CLIENT_VERSION);
            ss << (uint8_t) BTX_PREFIX << (uint8_t) header.version << (uint8_t) header.txType << bettingTx;
            for (auto it = ss.begin(); it < ss.end(); ++it) {
                betData.emplace_back((unsigned char)(*it));
            }
            return true;
        }
        case BetTxVersion5:
        {
            CDataStream ss(SER_NETWORK, CLIENT_VERSION);
            ss << (uint8_t) BTX_PREFIX << (uint8_t) header.version << (uint8_t) header.txType << bettingTx;
            for (auto it = ss.begin(); it < ss.end(); ++it) {
                betData.emplace_back((unsigned char)(*it));
            }
            return true;
        }
        default:
            return false;
    }
}

#endif