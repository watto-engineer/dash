// Copyright (c) 2019 The ION Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOKENS_TOKENDB_H
#define TOKENS_TOKENDB_H

#include "dbwrapper.h"

#include <boost/filesystem/path.hpp>

class CTokenGroupCreation;
class CTokenGroupID;

class CTokenDB : public CDBWrapper
{
public:
    CTokenDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CTokenDB(const CTokenDB&);
    void operator=(const CTokenDB&);

public:
    /** Write token group creations to the tokenDB in a batch */
    bool WriteTokenGroupsBatch(const std::vector<CTokenGroupCreation>& tokenGroups);
    bool WriteTokenGroup(const CTokenGroupID& tokenGroupID, const CTokenGroupCreation& tokenGroupCreation);
    bool ReadTokenGroup(const CTokenGroupID& tokenGroupID, CTokenGroupCreation& tokenGroupCreation);
    bool EraseTokenGroupBatch(const std::vector<CTokenGroupID>& newTokenGroupIDs);
    bool EraseTokenGroup(const CTokenGroupID& tokenGroupID);
    bool DropTokenGroups(std::string& strError);
    bool FindTokenGroups(std::vector<CTokenGroupCreation>& vTokenGroups, std::string& strError);
    bool LoadTokensFromDB(std::string& strError); // populates mapTokenGroups
};

bool ReindexTokenDB(std::string &strError); // Drops db, scans for token creations, but does not populate mapTokenGroups

#endif //TOKENS_TOKENDB_H