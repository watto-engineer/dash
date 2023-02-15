// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <evo/verifiable.h>

#include <clientversion.h>
#include <logging.h>
#include <tokens/tokengroupmanager.h>

bool Verifiable::VerifyBLSSignature(CValidationState& state) {
    switch (GetSignerType())
    {
        case SignerType::MGT:
        {
            // Management Key
            if (!tokenGroupManager->MGTTokensCreated()) {
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-token-signer");
            }
            CTokenGroupID tgID = tokenGroupManager->GetMGTID();
            std::shared_ptr<CTokenGroupDescriptionMGT> mgtDesc;
            if (!tokenGroupManager->GetTokenGroupDescription(tgID, mgtDesc)) {
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-token-signer");
            }

            CHashWriter hasher(SER_DISK, CLIENT_VERSION);
            hasher << tgID;
            auto tgHash = hasher.GetHash();
            if (GetSignerHash() != tgHash) {
                LogPrintf("%s - %s vs %s\n", __func__, GetSignerHash().ToString(), tgHash.ToString());
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-token-signer");
            }
            CBLSPublicKey blsPubKey = GetBlsPubKey();
            if (!blsPubKey.IsValid()) {
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-token-pubkey");
            }
            if (blsPubKey != mgtDesc->blsPubKey) {
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-token-pubkey");
            }
            CBLSSignature blsSig = GetBlsSignature();
            if (!blsSig.VerifyInsecure(blsPubKey, GetSignatureHash())) {
                return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-token-signature");
            }
            return true;
        }
        default:
            return state.Invalid(ValidationInvalidReason::CONSENSUS, false, REJECT_INVALID, "grp-bad-token-signertype");
    }
}
