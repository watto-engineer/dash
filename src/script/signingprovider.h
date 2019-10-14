// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SIGNINGPROVIDER_H
#define BITCOIN_SCRIPT_SIGNINGPROVIDER_H

#include <hdchain.h>
#include <key.h>
#include <pubkey.h>
#include <script/script.h>
#include <script/standard.h>
#include <sync.h>

struct KeyOriginInfo;

/** An interface to be implemented by keystores that support signing. */
class SigningProvider
{
public:
    virtual ~SigningProvider() {}
    virtual bool GetCScript(const CScriptID &scriptid, CScript& script) const { return false; }
    virtual bool HaveCScript(const CScriptID &scriptid) const { return false; }
    virtual bool GetPubKey(const CKeyID &address, CPubKey& pubkey) const { return false; }
    virtual bool GetKey(const CKeyID &address, CKey& key) const { return false; }
    virtual bool HaveKey(const CKeyID &address) const { return false; }
    virtual bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const { return false; }
};

extern const SigningProvider& DUMMY_SIGNING_PROVIDER;

/** A virtual base class for key stores */
class CKeyStore : public SigningProvider
{
public:
    //! Add a key to the store.
    virtual bool AddKeyPubKey(const CKey &key, const CPubKey &pubkey) =0;

    //! Check whether a key corresponding to a given address is present in the store.
    virtual bool HaveKey(const CKeyID &address) const =0;
    virtual std::set<CKeyID> GetKeys() const =0;

    //! Support for BIP 0013 : see https://github.com/bitcoin/bips/blob/master/bip-0013.mediawiki
    virtual bool AddCScript(const CScript& redeemScript) =0;
    virtual bool HaveCScript(const CScriptID &hash) const =0;
    virtual std::set<CScriptID> GetCScripts() const =0;

    //! Support for Watch-only addresses
    virtual bool AddWatchOnly(const CScript &dest) =0;
    virtual bool RemoveWatchOnly(const CScript &dest) =0;
    virtual bool HaveWatchOnly(const CScript &dest) const =0;
    virtual bool HaveWatchOnly() const =0;
};

class HidingSigningProvider : public SigningProvider
{
private:
    const bool m_hide_secret;
    const bool m_hide_origin;
    const SigningProvider* m_provider;

public:
    HidingSigningProvider(const SigningProvider* provider, bool hide_secret, bool hide_origin) : m_hide_secret(hide_secret), m_hide_origin(hide_origin), m_provider(provider) {}
    bool GetCScript(const CScriptID& scriptid, CScript& script) const override;
    bool GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const override;
    bool GetKey(const CKeyID& keyid, CKey& key) const override;
    bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const override;
};

struct FlatSigningProvider final : public SigningProvider
{
    std::map<CScriptID, CScript> scripts;
    std::map<CKeyID, CPubKey> pubkeys;
    std::map<CKeyID, std::pair<CPubKey, KeyOriginInfo>> origins;
    std::map<CKeyID, CKey> keys;

    bool GetCScript(const CScriptID& scriptid, CScript& script) const override;
    bool GetPubKey(const CKeyID& keyid, CPubKey& pubkey) const override;
    bool GetKeyOrigin(const CKeyID& keyid, KeyOriginInfo& info) const override;
    bool GetKey(const CKeyID& keyid, CKey& key) const override;
};

FlatSigningProvider Merge(const FlatSigningProvider& a, const FlatSigningProvider& b);

/** Fillable signing provider that keeps keys in an address->secret map */
class FillableSigningProvider : public SigningProvider
{
protected:
    using KeyMap = std::map<CKeyID, CKey>;
    using ScriptMap = std::map<CScriptID, CScript>;

    KeyMap mapKeys GUARDED_BY(cs_KeyStore);
    ScriptMap mapScripts GUARDED_BY(cs_KeyStore);

public:
    mutable CCriticalSection cs_KeyStore;

    virtual bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey);
    virtual bool AddKey(const CKey &key) { return AddKeyPubKey(key, key.GetPubKey()); }
    virtual bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const override;
    virtual bool HaveKey(const CKeyID &address) const override;
    virtual std::set<CKeyID> GetKeys() const;
    virtual bool GetKey(const CKeyID &address, CKey &keyOut) const override;
    virtual bool AddCScript(const CScript& redeemScript);
    virtual bool HaveCScript(const CScriptID &hash) const override;
    virtual std::set<CScriptID> GetCScripts() const;
    virtual bool GetCScript(const CScriptID &hash, CScript& redeemScriptOut) const override;
};

#endif // BITCOIN_SCRIPT_SIGNINGPROVIDER_H
