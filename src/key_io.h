// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KEY_IO_H
#define BITCOIN_KEY_IO_H

#include <key.h>
#include <pubkey.h>
#include <script/standard.h>

#include <string>

class CChainParams;

CKey DecodeSecret(const std::string& str);
std::string EncodeSecret(const CKey& key);

CExtKey DecodeExtKey(const std::string& str);
std::string EncodeExtKey(const CExtKey& extkey);
CExtPubKey DecodeExtPubKey(const std::string& str);
std::string EncodeExtPubKey(const CExtPubKey& extpubkey);

std::string EncodeLegacyAddr(const CTxDestination& dest, const CChainParams& params);
CTxDestination DecodeLegacyAddr(const std::string& str, const CChainParams& params);

CTxDestination DecodeDestination(const std::string &addr, const CChainParams &);
std::string EncodeDestination(const CTxDestination &, const CChainParams &/*, const Config &*/);
bool IsValidDestinationString(const std::string &addr, const CChainParams &params);

// Temporary workaround. Don't rely on global state, pass all parameters in new
// code.
CTxDestination DecodeDestination(const std::string &addr);
std::string EncodeDestination(const CTxDestination &);
bool IsValidDestinationString(const std::string &addr);

#endif // BITCOIN_KEY_IO_H
