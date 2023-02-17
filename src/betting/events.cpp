// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/events.h>

#include <betting/bet_db.h>

bool CreateBetEventFromDB(const CBettingsView& bettingsViewCache, const uint32_t nEventId, CBetEvent& event) {
    EventKey eventKey{nEventId};
    CPeerlessExtendedEventDB peerlessEventDBItem;
    if (bettingsViewCache.events->Read(eventKey, peerlessEventDBItem)) {
        event = CBetEvent(BetEventType::PEERLESS, nEventId);
        return true;
    }
    CFieldEventDB fieldEventDBItem;
    if (bettingsViewCache.fieldEvents->Read(eventKey, fieldEventDBItem)) {
        event = CBetEvent(BetEventType::FIELD, nEventId);
        return true;
    }
    return false;
}

bool CBetEvent::IsOpen(const CBettingsView& bettingsViewCache, uint32_t nTime) {
    switch (type)
    {
        case BetEventType::PEERLESS:
        {
            if (bettingsViewCache.results->Exists(ResultKey{nEventId})) {
                return error("result for event already posted");
            }
            EventKey eventKey{nEventId};
            CPeerlessExtendedEventDB eventDBItem;
            if (!bettingsViewCache.events->Read(eventKey, eventDBItem)) {
                return false;
            }
            if (eventDBItem.nStartTime >= nTime) {
                return error("past event start time");
            }
            break;
        }
        case BetEventType::FIELD:
        {
            if (bettingsViewCache.fieldResults->Exists(FieldResultKey{nEventId})) {
                return error("result for event already posted");
            }
            EventKey eventKey{nEventId};
            CFieldEventDB eventDBItem;
            if (!bettingsViewCache.fieldEvents->Read(eventKey, eventDBItem)) {
                return false;
            }
            if (eventDBItem.nStartTime >= nTime) {
                return error("past event start time");
            }
            break;
        }
        default:
            return false;
    }
    return true;
}