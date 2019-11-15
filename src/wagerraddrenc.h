// Copyright (c) 2017 The Bitcoin developers
// Copyright (c) 2018 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_WAGERRADDRENC_H
#define BITCOIN_WAGERRADDRENC_H

#include "script/standard.h"
#include "consensus/tokengroups.h"

#include <string>
#include <vector>

class CChainParams;
//class CTokenGroupID;

enum WagerrAddrType : uint8_t
{
    PUBKEY_TYPE = 0,
    SCRIPT_TYPE = 1,
    GROUP_TYPE = 2,
};

std::string EncodeWagerrAddr(const CTxDestination &, const CChainParams &);
std::string EncodeWagerrAddr(const std::vector<uint8_t> &id, const WagerrAddrType addrtype, const CChainParams &params);

struct WagerrAddrContent
{
    WagerrAddrType type;
    std::vector<uint8_t> hash;
};

CTxDestination DecodeWagerrAddr(const std::string &addr, const CChainParams &params);
WagerrAddrContent DecodeWagerrAddrContent(const std::string &addr, const CChainParams &params);
CTxDestination DecodeWagerrAddrDestination(const WagerrAddrContent &content);

std::vector<uint8_t> PackWagerrAddrContent(const WagerrAddrContent &content);
#endif
