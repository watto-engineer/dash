// Copyright (c) 2015-2017 The Bitcoin Unlimited developers
// Copyright (c) 2019-2020 The ION Core developers
// Copyright (c) 2022 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <clientversion.h>
#include <coins.h>
#include <key_io.h>
#include <logging.h>
#include <wagerraddrenc.h>
#include <tokens/groups.h>
#include <streams.h>

CTokenGroupID NoGroup; // No group specified.

std::string EncodeGroupAuthority(const GroupAuthorityFlags flags) {
    std::string sflags = "none";
    if (hasCapability(flags, GroupAuthorityFlags::CTRL)) {
        sflags = "";
        if (hasCapability(flags, GroupAuthorityFlags::MINT)) {
            sflags += "mint";
        }
        if (hasCapability(flags, GroupAuthorityFlags::MELT)) {
            if (sflags != "") sflags += " ";
            sflags += "melt";
        }
        if (hasCapability(flags, GroupAuthorityFlags::CCHILD)) {
            if (sflags != "") sflags += " ";
            sflags += "child";
        } else {
            if (sflags != "") sflags += " ";
            sflags += "nochild";
        }
        if (hasCapability(flags, GroupAuthorityFlags::RESCRIPT)) {
            if (sflags != "") sflags += " ";
            sflags += "rescript";
        }
        if (hasCapability(flags, GroupAuthorityFlags::SUBGROUP)) {
            if (sflags != "") sflags += " ";
            sflags += "subgroup";
        }
        if (hasCapability(flags, GroupAuthorityFlags::WAGERR)) {
            if (sflags != "") sflags += " ";
            sflags += "wagerr";
        }
    }
    return sflags;
}

bool IsOutputGrouped(const CTxOut &txout) {
    CTokenGroupInfo grp(txout.scriptPubKey);
    if (grp.invalid)
        return true; // Its still grouped even if invalid
    if (grp.associatedGroup != NoGroup)
        return true;

    return false;
}

bool IsOutputGroupedAuthority(const CTxOut &txout) {
    CTokenGroupInfo grp(txout.scriptPubKey);
    if (grp.invalid)
        return true;
    if (grp.associatedGroup != NoGroup && grp.isAuthority())
        return true;

    return false;
}

bool IsAnyOutputGrouped(const CTransaction &tx)
{
    for (const CTxOut &txout : tx.vout)
    {
        if (IsOutputGrouped(txout)) {
            return true;
        }
    }

    return false;
}

bool IsAnyOutputGroupedAuthority(const CTransaction &tx) {
    for (const CTxOut &txout : tx.vout)
    {
        if (IsOutputGroupedAuthority(txout)) {
            return true;
        }
    }

    return false;
}

bool IsAnyOutputGroupedCreation(const CTransaction &tx, const TokenGroupIdFlags tokenGroupIdFlags)
{
    for (const CTxOut& txout : tx.vout) {
        CTokenGroupInfo grp(txout.scriptPubKey);
        if (grp.invalid)
            return false;
        if (grp.isGroupCreation(tokenGroupIdFlags))
            return true;
    }
    return false;
}

bool GetGroupedCreationOutput(const CTransaction &tx, CTxOut &creationOutput, const TokenGroupIdFlags tokenGroupIdFlags) {
    creationOutput = CTxOut();
    for (const CTxOut& txout : tx.vout) {
        CTokenGroupInfo grp(txout.scriptPubKey);
        if (grp.invalid)
            return false;
        if (grp.isGroupCreation(tokenGroupIdFlags)) {
            creationOutput = txout;
            return true;
        }
    }
    return false;
}

std::vector<unsigned char> SerializeAmount(CAmount num)
{
    CDataStream strm(SER_NETWORK, CLIENT_VERSION);
    if (num < 0) // negative numbers are serialized at full length
    {
        ser_writedata64(strm, num);
    }
    /* Disallow amounts to be encoded as a single byte because these may need to have special encodings if
       the SCRIPT_VERIFY_MINIMALDATA flag is set
    else if (num < 256)
    {
        ser_writedata8(strm, num);
    }
    */
    else if (num <= std::numeric_limits<unsigned short>::max())
    {
        ser_writedata16(strm, num);
    }
    else if (num <= std::numeric_limits<unsigned int>::max())
    {
        ser_writedata32(strm, num);
    }
    else
    {
        ser_writedata64(strm, num);
    }
    return std::vector<unsigned char>(strm.begin(), strm.end());
}

CAmount DeserializeAmount(opcodetype opcodeQty, std::vector<unsigned char> &vec)
{
    /* Disallow raw opcodes or single byte sizes, because having them is an unnecessary decode complication
    if ((opcodeQty >= OP_1) && (opcodeQty <= OP_16))
    {
        return opcodeQty-OP_1+1;
    }
    if (opcodeQty == OP_1NEGATE)
    {
        return 0x81;
    }

    int sz = vec.size();
    if (sz == 1)
    {
        return vec[0];
    }
    */
    int sz = vec.size();
    CDataStream strm(vec, SER_NETWORK, CLIENT_VERSION);
    if (sz == 2)
    {
        return ser_readdata16(strm);
    }
    if (sz == 4)
    {
        return ser_readdata32(strm);
    }
    if (sz == 8)
    {
        uint64_t v = ser_readdata64(strm);
        return (CAmount)v;
    }
    throw std::ios_base::failure("DeserializeAmount(): invalid format");
}

#if 0
CTokenGroupID ExtractControllingGroup(const CScript &scriptPubKey)
{
    typedef std::vector<unsigned char> valtype;
    std::vector<valtype> vSolutions;
    txnouttype whichType = Solver(scriptPubKey, vSolutions);
    if (whichType == TX_NONSTANDARD)
        return CTokenGroupID();

    // only certain well known script types are allowed to mint or melt
    if ((whichType == TX_PUBKEYHASH) || (whichType == TX_GRP_PUBKEYHASH) || (whichType == TX_SCRIPTHASH) ||
        (whichType == TX_GRP_SCRIPTHASH))
    {
        return CTokenGroupID(uint160(vSolutions[0]));
    }
    return CTokenGroupID();
}
#endif

CTokenGroupInfo::CTokenGroupInfo(const CScript &script)
    : associatedGroup(), quantity(0), invalid(false)
{
    CScript::const_iterator pc = script.begin();
    std::vector<unsigned char> groupId;
    std::vector<unsigned char> tokenQty;
    std::vector<unsigned char> data;
    opcodetype opcode;
    opcodetype opcodeGrp;
    opcodetype opcodeQty;

    // mintMeltGroup = ExtractControllingGroup(script);

    if (!script.GetOp(pc, opcodeGrp, groupId))
    {
        associatedGroup = NoGroup;
        return;
    }

    if (!script.GetOp(pc, opcodeQty, tokenQty))
    {
        associatedGroup = NoGroup;
        return;
    }

    if (!script.GetOp(pc, opcode, data))
    {
        associatedGroup = NoGroup;
        return;
    }

    if (opcode != OP_GROUP)
    {
        associatedGroup = NoGroup;
        return;
    }
    else // If OP_GROUP is used, enforce rules on the other fields
    {
        // group must be 32 bytes or more
        if (opcodeGrp < 0x20)
        {
            invalid = true;
            return;
        }
        /* Disallow amounts to be encoded as a single byte because these may need to have special encodings if
           the SCRIPT_VERIFY_MINIMALDATA flag is set
        // quantity must be 1, 2, 4, or 8 bytes
        if (((opcodeQty < OP_1)||(opcodeQty > OP_16)) && (opcodeQty != OP_1NEGATE) && (opcodeQty != 1) && (opcodeQty != 2)
           && (opcodeQty != 4) && (opcodeQty != 8))
        {
            invalid = true;
            return;
        }
        */

        // Quantity must be a 2, 4, or 8 byte number
        if ((opcodeQty != 2) && (opcodeQty != 4) && (opcodeQty != 8))
        {
            invalid = true;
            return;
        }
    }

    try
    {
        quantity = DeserializeAmount(opcodeQty, tokenQty);
    }
    catch (std::ios_base::failure &f)
    {
        invalid = true;
    }
    associatedGroup = groupId;
}


CTokenGroupID::CTokenGroupID(const std::vector<unsigned char> &id)
{
    // for the conceivable future there is no possible way a group could be bigger but the spec does allow larger
    if (!(id.size() < OP_PUSHDATA1)) {
        LogPrint(BCLog::TOKEN, "%s - Debug Assertion failed", __func__);
    };
    data = id;
}

CTokenGroupID::CTokenGroupID(const CTokenGroupID& tgID, const std::string strSubgroup)
{
    data = std::vector<unsigned char>(tgID.bytes().size() + strSubgroup.size());

    unsigned int i;
    for (i = 0; i < tgID.bytes().size(); i++)
    {
        data[i] = tgID.bytes()[i];
    }
    for (unsigned int j = 0; j < strSubgroup.size(); j++, i++)
    {
        data[i] = strSubgroup[j];
    }
}

bool CTokenGroupID::isUserGroup(void) const { return (!data.empty()); }
bool CTokenGroupID::isSubgroup(void) const { return (data.size() > PARENT_GROUP_ID_SIZE); }

CTokenGroupID CTokenGroupID::parentGroup(void) const
{
    if (data.size() <= PARENT_GROUP_ID_SIZE)
        return CTokenGroupID(data);
    return CTokenGroupID(std::vector<unsigned char>(data.begin(), data.begin() + PARENT_GROUP_ID_SIZE));
}

const std::vector<unsigned char> CTokenGroupID::GetSubGroupData() const {
    std::vector<unsigned char> subgroupData;
    if (data.size() > PARENT_GROUP_ID_SIZE) {
        subgroupData = std::vector<unsigned char>(data.begin() + PARENT_GROUP_ID_SIZE, data.end());
    }
    return subgroupData;
}

bool CTokenGroupID::hasFlag(TokenGroupIdFlags flag) const {
    return data.size() >= PARENT_GROUP_ID_SIZE ? hasTokenGroupIdFlag((TokenGroupIdFlags)data[31], flag) : false;
}

std::string CTokenGroupID::encodeFlags() const {
    std::string sflags = "";
    if (data.size() < CTokenGroupID::PARENT_GROUP_ID_SIZE) return sflags;

    if (hasTokenGroupIdFlag((TokenGroupIdFlags)data[31], TokenGroupIdFlags::MGT_TOKEN)) {
        sflags = "management";
    }
    if (hasTokenGroupIdFlag((TokenGroupIdFlags)data[31], TokenGroupIdFlags::NFT_TOKEN)) {
        if (sflags != "") sflags += " ";
        sflags += "nft";
    }
    if (hasTokenGroupIdFlag((TokenGroupIdFlags)data[31], TokenGroupIdFlags::BETTING_TOKEN)) {
        if (sflags != "") sflags += " ";
        sflags += "betting";
    }
    if (hasTokenGroupIdFlag((TokenGroupIdFlags)data[31], TokenGroupIdFlags::STICKY_MELT)) {
        if (sflags != "") sflags += " ";
        sflags += "sticky_melt";
    }
    return sflags;
}
