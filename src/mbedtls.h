#pragma once

#include <mbedtls/md.h>
#include <sframe/provider.h>

namespace sframe {
namespace provider {
namespace mbedtls {

class MbedTLSProvider : public Provider
{
  using scoped_hmac_ctx =
    std::unique_ptr<mbedtls_md_context_t, decltype(&mbedtls_md_free)>;

  ///
  /// Information about algorithms
  ///
  std::set<HashId> supported_hash_algorithms() const override;
  std::set<EncryptionId> supported_encryption_algorithms() const override;
  std::size_t digest_size(HashId algorithm) const override;
  std::size_t key_size(EncryptionId algorithm) const override;
  std::size_t nonce_size(EncryptionId algorithm) const override;

  ///
  /// AEAD Algorithms
  ///
  output_bytes seal(EncryptionId encryption_algorithm,
                    HashId hash_algorithm,
                    std::size_t tag_size,
                    const bytes& key,
                    const bytes& nonce,
                    output_bytes ct,
                    input_bytes aad,
                    input_bytes pt) const override;
  output_bytes open(EncryptionId encryption_algorithm,
                    HashId hash_algorithm,
                    std::size_t tag_size,
                    const bytes& key,
                    const bytes& nonce,
                    output_bytes pt,
                    input_bytes aad,
                    input_bytes ct) const override;

protected:
  struct MbedTLSHMAC : HMAC
  {
    MbedTLSHMAC(HashAlgorithm algorithm, input_bytes key);
    void write(input_bytes data) override;
    bytes digest() override;

    scoped_hmac_ctx ctx;
    std::array<std::uint8_t, MBEDTLS_MD_MAX_SIZE> md;
    const HashAlgorithm algorithm;
  };

  Provider::HMACPtr create_hmac(HashId algorithm,
                                input_bytes key) const override;
  Provider::HMACPtr create_hmac(HashAlgorithm algorithm, input_bytes key) const;
  output_bytes seal_ctr(EncryptionAlgorithm encryption_algorithm,
                        HashAlgorithm hash_algorithm,
                        std::size_t tag_size,
                        const bytes& key,
                        const bytes& nonce,
                        output_bytes ct,
                        input_bytes aad,
                        input_bytes pt) const;
  output_bytes open_ctr(EncryptionAlgorithm encryption_algorithm,
                        HashAlgorithm hash_algorithm,
                        std::size_t tag_size,
                        const bytes& key,
                        const bytes& nonce,
                        output_bytes pt,
                        input_bytes aad,
                        input_bytes ct) const;

  void ctr_crypt(EncryptionAlgorithm encryption_algorithm,
                 input_bytes key,
                 input_bytes nonce,
                 output_bytes out,
                 input_bytes in) const;

  output_bytes seal_aead(EncryptionAlgorithm encryption_algorithm,
                         std::size_t tag_size,
                         const bytes& key,
                         const bytes& nonce,
                         output_bytes ct,
                         input_bytes aad,
                         input_bytes pt) const;

  output_bytes open_aead(EncryptionAlgorithm encryption_algorithm,
                         std::size_t tag_size,
                         const bytes& key,
                         const bytes& nonce,
                         output_bytes pt,
                         input_bytes aad,
                         input_bytes ct) const;
};

} // namespace mbedtls
} // namespace provider
} // namespace sframe
