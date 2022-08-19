// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_INVALID_H
#define WAGERR_INVALID_H

#endif //WAGERR_INVALID_H

#include <primitives/transaction.h>

namespace invalid_out
{
    extern CScript validScript;

    bool ContainsScript(const CScript& script);
    bool LoadScripts();
}