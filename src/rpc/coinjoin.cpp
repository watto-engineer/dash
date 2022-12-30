// Copyright (c) 2019-2022 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/context.h>
#include <validation.h>
#ifdef ENABLE_WALLET
#include <coinjoin/client.h>
#include <coinjoin/options.h>
#include <wallet/rpcwallet.h>
#endif // ENABLE_WALLET
#include <coinjoin/server.h>
#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <util/strencodings.h>

#include <univalue.h>

void RegisterCoinJoinRPCCommands(CRPCTable &t)
{
}
