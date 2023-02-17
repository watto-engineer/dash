// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_BET_EVENT_H
#define WAGERR_BET_EVENT_H

#include <betting/bet_common.h>
#include <betting/bet_tx.h>

#include <array>
#include <string_view>

class CBettingsView;

enum class BetEventType : uint8_t {
    UNKNOWN  = 0x00,
    PEERLESS = 0x01,
    FIELD    = 0x02,
    LAST     = FIELD
};
template<> struct is_serializable_enum<BetEventType> : std::true_type {};

constexpr std::array<std::string_view, static_cast<uint8_t>(BetEventType::LAST)+1> makeBetEventTypeDefs() {
    std::array<std::string_view, static_cast<uint8_t>(BetEventType::LAST)+1> arr = {
        "UNKNOWN",
        "PEERLESS",
        "FIELD"
    };
    return arr;
}
[[maybe_unused]] static constexpr auto betEventTypeDefs = makeBetEventTypeDefs();

class CBetEvent
{
public:
    BetEventType type;
    uint32_t nEventId;

    CBetEvent() : type(BetEventType::UNKNOWN), nEventId(0) {};
    CBetEvent(const CPeerlessEventTx eventTx) : type(BetEventType::PEERLESS), nEventId(eventTx.nEventId) { };
    CBetEvent(const CFieldEventTx eventTx) : type(BetEventType::FIELD), nEventId(eventTx.nEventId) { };
    CBetEvent(const BetEventType type, const uint32_t nEventId) : type(type), nEventId(nEventId) { };

    bool IsOpen(const CBettingsView& bettingsViewCache, uint32_t nTime);

};

bool CreateBetEventFromDB(const CBettingsView& bettingsViewCache, const uint32_t nEventId, CBetEvent& event);

#endif // WAGERR_BET_EVENT_H