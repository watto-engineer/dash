// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_tx.h>
#include <betting/bet_common.h>

bool HasOpReturnOutput(const CTransaction &tx) {
    for (size_t i = 0; i < tx.vout.size(); i++) {
        const CScript& script = tx.vout[i].scriptPubKey;
        if (script.size() > 0 && *script.begin() == OP_RETURN) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<CBettingTx> ParseBettingTx(const CTxOut& txOut)
{
    CScript const & script = txOut.scriptPubKey;
    CScript::const_iterator pc = script.begin();
    std::vector<unsigned char> opcodeData;
    opcodetype opcode;

    if (!script.GetOp(pc, opcode) || opcode != OP_RETURN)
        return nullptr;

    if (!script.GetOp(pc, opcode, opcodeData) ||
            (opcode > OP_PUSHDATA1 &&
             opcode != OP_PUSHDATA2 &&
             opcode != OP_PUSHDATA4))
    {
        return nullptr;
    }

    CDataStream ss(opcodeData, SER_NETWORK, PROTOCOL_VERSION);
    // deserialize betting tx header
    CBettingTxHeader header;
    if (ss.size() < header.GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION))
        return nullptr;
    ss >> header;
    if (header.prefix != BTX_PREFIX ||
            header.version != BetTxVersion_CURRENT)
        return nullptr;
    
    return DeserializeBettingTxFromType(ss, (BetTxTypes) header.txType);
}

std::unique_ptr<CBettingTx> DeserializeBettingTxFromType(CDataStream& ss, BetTxTypes type) {
    // deserialize opcode data to tx classes
    switch (type)
    {
        case mappingTxType:
            return DeserializeBettingTx<CMappingTx>(ss);
        case plEventTxType:
            return DeserializeBettingTx<CPeerlessEventTx>(ss);
        case fEventTxType:
            return DeserializeBettingTx<CFieldEventTx>(ss);
        case fUpdateOddsTxType:
            return DeserializeBettingTx<CFieldUpdateOddsTx>(ss);
        case fUpdateMarginTxType:
            return DeserializeBettingTx<CFieldUpdateMarginTx>(ss);
        case fZeroingOddsTxType:
            return DeserializeBettingTx<CFieldZeroingOddsTx>(ss);
        case fResultTxType:
            return DeserializeBettingTx<CFieldResultTx>(ss);
        case fBetTxType:
            return DeserializeBettingTx<CFieldBetTx>(ss);
        case fParlayBetTxType:
            return DeserializeBettingTx<CFieldParlayBetTx>(ss);
        case plBetTxType:
            return DeserializeBettingTx<CPeerlessBetTx>(ss);
        case plResultTxType:
            return DeserializeBettingTx<CPeerlessResultTx>(ss);
        case plUpdateOddsTxType:
            return DeserializeBettingTx<CPeerlessUpdateOddsTx>(ss);
        case cgEventTxType:
            return DeserializeBettingTx<CChainGamesEventTx>(ss);
        case cgBetTxType:
            return DeserializeBettingTx<CChainGamesBetTx>(ss);
        case cgResultTxType:
            return DeserializeBettingTx<CChainGamesResultTx>(ss);
        case plSpreadsEventTxType:
            return DeserializeBettingTx<CPeerlessSpreadsEventTx>(ss);
        case plTotalsEventTxType:
            return DeserializeBettingTx<CPeerlessTotalsEventTx>(ss);
        case plEventPatchTxType:
            return DeserializeBettingTx<CPeerlessEventPatchTx>(ss);
        case plParlayBetTxType:
            return DeserializeBettingTx<CPeerlessParlayBetTx>(ss);
        case qgBetTxType:
            return DeserializeBettingTx<CQuickGamesBetTx>(ss);
        case plEventZeroingOddsTxType:
            return DeserializeBettingTx<CPeerlessEventZeroingOddsTx>(ss);
        default:
            return nullptr;
    }
}