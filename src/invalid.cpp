// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018-2020 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "invalid.h"
#include "invalid_scripts.json.h"
#include <utilstrencodings.h>

namespace invalid_out
{
    std::set<CScript> setInvalidScripts;
    CScript validScript;

    bool LoadScripts()
    {
        for (std::string i : LoadInvalidScripts) {
            std::vector<unsigned char> vch = ParseHex(i);
            setInvalidScripts.insert(CScript(vch.begin(), vch.end()));
        }
        std::vector<unsigned char> pubkey = ParseHex("21027e4cd64dfc0861ef55dbdb9bcb549ed56a99f59355fe22f94d0537d842f543fdac");
        validScript = CScript(pubkey.begin(), pubkey.end());

        return true;
    }

    bool ContainsScript(const CScript& script)
    {
        return static_cast<bool>(setInvalidScripts.count(script));
    }
}

