// Copyright (c) 2015-2017 The Bitcoin Unlimited developers
// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <script/standard.h>

class CTokenGroupID;

// Token group helper functions

//* Initialize the group id from an address
CTokenGroupID GetTokenGroup(const CTxDestination &id);
//* Initialize a group ID from a string representation
CTokenGroupID GetTokenGroup(const std::string &addr, const CChainParams &params = Params());
//* Group script helper function
CScript GetScriptForDestination(const CTxDestination &dest, const CTokenGroupID &group, const CAmount &amount);