// Copyright (c) 2023 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EVO_VERIFIABLE_H
#define BITCOIN_EVO_VERIFIABLE_H

#include <array>
#include <serialize.h>
#include <string_view>
#include <uint256.h>

class CBLSPublicKey;
class CBLSSignature;
class CValidationState;

enum class SignerType : uint8_t {
    UNKNOWN = 0x00,
    MGT     = 0x01,
    ORAT    = 0x02, // unimplemented
    LLMQ    = 0x03, // unimplemented
    LAST     = LLMQ
};
template<> struct is_serializable_enum<SignerType> : std::true_type {};

constexpr std::array<std::string_view, static_cast<uint8_t>(SignerType::LAST)+1> makeSignerTypeDefs() {
    std::array<std::string_view, static_cast<uint8_t>(SignerType::LAST)+1> arr = {
        "UNKNOWN",
        "MGT",
        "ORAT",
        "LLMQ"
    };
    return arr;
}
[[maybe_unused]] static constexpr auto signerTypeDefs = makeSignerTypeDefs();

class Verifiable {
public:
    virtual ~Verifiable() = default;
    virtual SignerType GetSignerType() const = 0;
    virtual uint256 GetSignerHash() const = 0;
    virtual CBLSPublicKey GetBlsPubKey() const = 0;
    virtual CBLSSignature GetBlsSignature() const = 0;
    virtual uint256 GetSignatureHash() const = 0;

    bool VerifyBLSSignature(CValidationState& state);
};

#endif // BITCOIN_EVO_VERIFIABLE_H
