// Copyright (c) 2012-2014 The Bitcoin Core developers
// Copyright (c) 2014-2022 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

/**
 * network protocol versioning
 */


static const int PROTOCOL_VERSION = 70930;
static const int TORV3_SERVICES_VERSION = 70930;

//! initial proto version, to be increased after version/verack negotiation
static const int INIT_PROTO_VERSION = 214;

//! In this version, 'getheaders' was introduced.
static const int GETHEADERS_VERSION = 70930;

//! In this version, 'segwit' was enabled.
static const int SEGWIT_VERSION = 70930;

//! New spork protocol
static const int MIN_SPORK_VERSION = 70939;

//! disconnect from peers older than this proto version
static const int MIN_PEER_PROTO_VERSION = 70923;
static const int MIN_PEER_PROTO_VERSION_BEFORE_ENFORCEMENT = 70928;
static const int MIN_PEER_PROTO_VERSION_AFTER_ENFORCEMENT = 70930;

//! minimum proto version of masternode to accept in DKGs
static const int MIN_MASTERNODE_PROTO_VERSION = 70930;

//! nTime field added to CAddress, starting with this version;
//! if possible, avoid requesting addresses nodes older than this
static const int CADDR_TIME_VERSION = 66666;

//! protocol version is included in MNAUTH starting with this version
static const int MNAUTH_NODE_VER_VERSION = 70926;

//! introduction of QGETDATA/QDATA messages
static const int LLMQ_DATA_MESSAGES_VERSION = 70930;

//! introduction of instant send deterministic lock (ISDLOCK)
static const int ISDLOCK_PROTO_VERSION = 70920;

//! GOVSCRIPT was activated in this version
static const int GOVSCRIPT_PROTO_VERSION = 70921;

//! ADDRV2 was introduced in this version
static const int ADDRV2_PROTO_VERSION = 70923;

// Make sure that none of the values above collide with `ADDRV2_FORMAT`.

#endif // BITCOIN_VERSION_H
