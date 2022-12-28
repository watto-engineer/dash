// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef WAGERR_DICE_H
#define WAGERR_DICE_H

#include "serialize.h"

class arith_uint256;

namespace quickgames {

typedef enum QuickGamesDiceBetType {
    qgDiceEqual = 0x00,
    qgDiceNotEqual = 0x01,
    qgDiceTotalOver = 0x02,
    qgDiceTotalUnder = 0x03,
    qgDiceEven = 0x04,
    qgDiceOdd = 0x05,
    qgDiceUndefined = 0xff,
} QuickGamesDiceBetType;

struct DiceBetInfo {
    QuickGamesDiceBetType betType;
    uint32_t betNumber;

    SERIALIZE_METHODS(DiceBetInfo, obj){
        uint8_t bet_type;

        SER_WRITE(obj, bet_type = (uint8_t) obj.betType);
        READWRITE(bet_type);
        SER_READ(obj, obj.betType = (QuickGamesDiceBetType) bet_type);

        if (type != qgDiceEven && type != qgDiceOdd) {
            READWRITE(betNumber);
        }
    }
};

std::map<std::string, std::string> DiceBetInfoParser(std::vector<unsigned char>& betInfo, arith_uint256 seed);

uint32_t DiceHandler(std::vector<unsigned char>& betInfo, arith_uint256 seed);

std::string DiceGameTypeToStr(QuickGamesDiceBetType type);
QuickGamesDiceBetType StrToDiceGameType(std::string strType);

} // namespace quickgames


#endif //WAGERR_DICE_H
