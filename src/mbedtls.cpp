#include "mbedtls.h"
#include <iostream>
#include <mbedtls/cipher.h>
#include <mbedtls/constant_time.h>
#include <mbedtls/error.h>
#include <mbedtls/md.h>

namespace sframe {
namespace provider {
namespace mbedtls {

using scoped_cipher_ctx =
  std::unique_ptr<mbedtls_cipher_context_t, decltype(&mbedtls_cipher_free)>;

static mbedtls_md_type_t
mbedtls_from_type(HashAlgorithm algorithm)
{
  switch (algorithm) {
    case HashAlgorithm::SHA256:
      return mbedtls_md_type_t::MBEDTLS_MD_SHA256;
    case HashAlgorithm::SHA512:
      return mbedtls_md_type_t::MBEDTLS_MD_SHA512;
    default:
      throw unsupported_ciphersuite_error();
  }
}

static mbedtls_cipher_type_t
mbedtls_from_type(EncryptionAlgorithm algorithm)
{
  switch (algorithm) {
    case EncryptionAlgorithm::AES_CM_128:
      return MBEDTLS_CIPHER_AES_128_CTR;
    case EncryptionAlgorithm::AES_GCM_128:
      return MBEDTLS_CIPHER_AES_128_GCM;
    case EncryptionAlgorithm::AES_GCM_256:
      return MBEDTLS_CIPHER_AES_256_GCM;
    default:
      throw unsupported_ciphersuite_error();
  }
}

std::set<HashId>
MbedTLSProvider::supported_hash_algorithms() const
{
  return { static_cast<HashId>(HashAlgorithm::SHA256),
           static_cast<HashId>(HashAlgorithm::SHA512) };
}

std::set<EncryptionId>
MbedTLSProvider::supported_encryption_algorithms() const
{
  return { static_cast<EncryptionId>(EncryptionAlgorithm::AES_CM_128),
           static_cast<EncryptionId>(EncryptionAlgorithm::AES_GCM_128),
           static_cast<EncryptionId>(EncryptionAlgorithm::AES_GCM_256) };
}

static std::size_t
mbedtls_digest_size(HashAlgorithm algorithm)
{
  const mbedtls_md_info_t* md_info =
    mbedtls_md_info_from_type(mbedtls_from_type(algorithm));
  if (md_info == nullptr) {
    throw unsupported_ciphersuite_error();
  }
  return std::size_t(mbedtls_md_get_size(md_info));
}

std::size_t
MbedTLSProvider::digest_size(HashId algorithm) const
{
  return mbedtls_digest_size(static_cast<HashAlgorithm>(algorithm));
}

static std::size_t
mbedtls_key_size(EncryptionAlgorithm algorithm)
{
  switch (algorithm) {
    case EncryptionAlgorithm::AES_CM_128:
    case EncryptionAlgorithm::AES_GCM_128:
      return 16;
    case EncryptionAlgorithm::AES_GCM_256:
      return 32;
    default:
      throw unsupported_ciphersuite_error();
  }
}

std::size_t
MbedTLSProvider::key_size(EncryptionId algorithm) const
{
  return mbedtls_key_size(static_cast<EncryptionAlgorithm>(algorithm));
}

std::size_t
MbedTLSProvider::nonce_size(EncryptionId algorithm) const
{
  const auto typed_algorithm = static_cast<EncryptionAlgorithm>(algorithm);
  switch (typed_algorithm) {
    case EncryptionAlgorithm::AES_CM_128:
    case EncryptionAlgorithm::AES_GCM_128:
    case EncryptionAlgorithm::AES_GCM_256:
      return 12;

    default:
      throw unsupported_ciphersuite_error();
  }
}

MbedTLSProvider::MbedTLSHMAC::MbedTLSHMAC(HashAlgorithm algorithm,
                                          input_bytes key)
  : ctx(scoped_hmac_ctx(new mbedtls_md_context_t(), mbedtls_md_free))
  , algorithm(algorithm)
{
  mbedtls_md_init(ctx.get());
  const mbedtls_md_info_t* md_info =
    mbedtls_md_info_from_type(mbedtls_from_type(algorithm));
  const int DOING_HMAC = 1;
  const int setup = mbedtls_md_setup(ctx.get(), md_info, DOING_HMAC);
  if (setup != 0) {
    throw std::runtime_error("Failed to setup MD context");
  }
  const int start = mbedtls_md_hmac_starts(ctx.get(), key.data(), key.size());
  if (start != 0) {
    throw std::runtime_error("Failed to start MD context");
  }
}

void
MbedTLSProvider::MbedTLSHMAC::write(input_bytes data)
{
  const int updated =
    mbedtls_md_hmac_update(ctx.get(), data.data(), data.size());
  if (updated != 0) {
    throw std::runtime_error("Failed to update MD context");
  }
}

bytes
MbedTLSProvider::MbedTLSHMAC::digest()
{
  const auto finished = mbedtls_md_hmac_finish(ctx.get(), md.data());
  if (finished != 0) {
    throw std::runtime_error("Failed to finish MD context");
  }
  return { md.data(), md.data() + mbedtls_digest_size(algorithm) };
}

void
MbedTLSProvider::ctr_crypt(EncryptionAlgorithm algorithm,
                           input_bytes key,
                           input_bytes nonce,
                           output_bytes out,
                           input_bytes in) const
{
  if (out.size() != in.size()) {
    throw buffer_too_small_error("CTR size mismatch");
  }

  static auto padded_nonce =
    std::array<uint8_t, 16>{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  std::copy(nonce.begin(), nonce.end(), padded_nonce.begin());

  auto ctx =
    scoped_cipher_ctx(new mbedtls_cipher_context_t(), mbedtls_cipher_free);
  mbedtls_cipher_init(ctx.get());

  const auto* cipher =
    mbedtls_cipher_info_from_type(mbedtls_from_type(algorithm));
  if (cipher == nullptr) {
    throw unsupported_ciphersuite_error();
  }

  const auto setup = mbedtls_cipher_setup(ctx.get(), cipher);
  if (setup != 0) {
    throw std::runtime_error("Failed to setup cipher context");
  }
  const auto keyed = mbedtls_cipher_setkey(
    ctx.get(), key.data(), key.size() * 8, MBEDTLS_ENCRYPT);
  if (keyed != 0) {
    throw std::runtime_error("Failed to set key");
  }

  std::size_t outlen = 0;
  const int crypt = mbedtls_cipher_crypt(ctx.get(),
                                         padded_nonce.data(),
                                         padded_nonce.size(),
                                         in.data(),
                                         in.size(),
                                         out.data(),
                                         &outlen);
  if (crypt != 0) {
    throw std::runtime_error("Failed to encrypt");
  }

  const int finish = mbedtls_cipher_finish(ctx.get(), out.data(), &outlen);
  if (finish != 0) {
    throw std::runtime_error("Failed to finish cipher");
  }
}

output_bytes
MbedTLSProvider::seal_ctr(EncryptionAlgorithm encryption_algorithm,
                          HashAlgorithm hash_algorithm,
                          std::size_t tag_size,
                          const bytes& key,
                          const bytes& nonce,
                          output_bytes ct,
                          input_bytes aad,
                          input_bytes pt) const
{
  if (ct.size() < pt.size() + tag_size) {
    throw buffer_too_small_error("Ciphertext buffer too small");
  }

  // Split the key into enc and auth subkeys
  auto key_span = input_bytes(key);
  auto enc_key_size = mbedtls_key_size(encryption_algorithm);
  auto enc_key = key_span.subspan(0, enc_key_size);
  auto auth_key = key_span.subspan(enc_key_size);

  // Encrypt with AES-CM
  auto inner_ct = ct.subspan(0, pt.size());
  ctr_crypt(encryption_algorithm, enc_key, nonce, inner_ct, pt);

  // Authenticate with truncated HMAC
  auto hmac = MbedTLSHMAC(hash_algorithm, auth_key);
  hmac.write(aad);
  hmac.write(inner_ct);
  auto mac = hmac.digest();
  auto tag = ct.subspan(pt.size(), tag_size);
  std::copy(mac.begin(), mac.begin() + tag_size, tag.begin());

  return ct.subspan(0, pt.size() + tag_size);
}

output_bytes
MbedTLSProvider::seal_aead(EncryptionAlgorithm algorithm,
                           std::size_t tag_size,
                           const bytes& key,
                           const bytes& nonce,
                           output_bytes ct,
                           input_bytes aad,
                           input_bytes pt) const
{
  if (ct.size() < pt.size() + tag_size) {
    throw buffer_too_small_error("Ciphertext buffer too small");
  }

  auto ctx =
    scoped_cipher_ctx(new mbedtls_cipher_context_t(), mbedtls_cipher_free);
  if (ctx.get() == nullptr) {
    throw std::runtime_error("Failed to allocate cipher context");
  }
  mbedtls_cipher_init(ctx.get());
  const auto* cipher =
    mbedtls_cipher_info_from_type(mbedtls_from_type(algorithm));
  if (cipher == nullptr) {
    throw unsupported_ciphersuite_error();
  }

  const auto setup = mbedtls_cipher_setup(ctx.get(), cipher);
  if (setup != 0) {
    throw std::runtime_error("Failed to setup cipher context");
  }

  const auto keyed = mbedtls_cipher_setkey(
    ctx.get(), key.data(), key.size() * 8, MBEDTLS_ENCRYPT);
  if (keyed != 0) {
    throw std::runtime_error("Failed to set key");
  }

  std::size_t outlen = 0;
  const int crypt = mbedtls_cipher_auth_encrypt_ext(ctx.get(),
                                                    nonce.data(),
                                                    nonce.size(),
                                                    aad.data(),
                                                    aad.size(),
                                                    pt.data(),
                                                    pt.size(),
                                                    ct.data(),
                                                    ct.size(),
                                                    &outlen,
                                                    tag_size);
  if (crypt != 0) {
    throw std::runtime_error("Failed to encrypt");
  }
  return ct.subspan(0, pt.size() + tag_size);
}

output_bytes
MbedTLSProvider::seal(EncryptionId encryption_algorithm,
                      HashId hash_algorithm,
                      std::size_t tag_size,
                      const bytes& key,
                      const bytes& nonce,
                      output_bytes ct,
                      input_bytes aad,
                      input_bytes pt) const
{
  const auto typed_encryption_algorithm = static_cast<EncryptionAlgorithm>(encryption_algorithm);
  const auto typed_hash_algorithm = static_cast<HashAlgorithm>(hash_algorithm);
  switch (typed_encryption_algorithm) {
    case EncryptionAlgorithm::AES_CM_128:
      return seal_ctr(typed_encryption_algorithm,
                      typed_hash_algorithm,
                      tag_size,
                      key,
                      nonce,
                      ct,
                      aad,
                      pt);
    case EncryptionAlgorithm::AES_GCM_128:
    case EncryptionAlgorithm::AES_GCM_256:
      return seal_aead(typed_encryption_algorithm, tag_size, key, nonce, ct, aad, pt);
    default:
      throw unsupported_ciphersuite_error();
  }
}

output_bytes
MbedTLSProvider::open(EncryptionId encryption_algorithm,
                      HashId hash_algorithm,
                      std::size_t tag_size,
                      const bytes& key,
                      const bytes& nonce,
                      output_bytes pt,
                      input_bytes aad,
                      input_bytes ct) const
{
  const auto typed_encryption_algorithm = static_cast<EncryptionAlgorithm>(encryption_algorithm);
  const auto typed_hash_algorithm = static_cast<HashAlgorithm>(hash_algorithm);
  switch (typed_encryption_algorithm) {
    case EncryptionAlgorithm::AES_CM_128:
      return open_ctr(typed_encryption_algorithm,
                      typed_hash_algorithm,
                      tag_size,
                      key,
                      nonce,
                      pt,
                      aad,
                      ct);
    case EncryptionAlgorithm::AES_GCM_128:
    case EncryptionAlgorithm::AES_GCM_256:
      return open_aead(typed_encryption_algorithm, tag_size, key, nonce, pt, aad, ct);
  }
  throw unsupported_ciphersuite_error();
}

Provider::HMACPtr
MbedTLSProvider::create_hmac(HashId algorithm, input_bytes key) const
{
  return create_hmac(static_cast<HashAlgorithm>(algorithm), key);
}

Provider::HMACPtr
MbedTLSProvider::create_hmac(HashAlgorithm algorithm, input_bytes key) const
{
  return std::unique_ptr<MbedTLSHMAC>(new MbedTLSHMAC(algorithm, key));
}

output_bytes
MbedTLSProvider::open_ctr(EncryptionAlgorithm encryption_algorithm,
                          HashAlgorithm hash_algorithm,
                          std::size_t tag_size,
                          const bytes& key,
                          const bytes& nonce,
                          output_bytes pt,
                          input_bytes aad,
                          input_bytes ct) const
{
  if (ct.size() < tag_size) {
    throw buffer_too_small_error("Ciphertext buffer too small");
  }

  auto inner_ct_size = ct.size() - tag_size;
  auto inner_ct = ct.subspan(0, inner_ct_size);
  auto tag = ct.subspan(inner_ct_size, tag_size);

  // Split the key into enc and auth subkeys
  auto key_span = input_bytes(key);
  auto enc_key_size = key_size(static_cast<EncryptionId>(encryption_algorithm));
  auto enc_key = key_span.subspan(0, enc_key_size);
  auto auth_key = key_span.subspan(enc_key_size);

  // Authenticate with truncated HMAC
  auto hmac = create_hmac(hash_algorithm, auth_key);
  hmac->write(aad);
  hmac->write(inner_ct);
  auto mac = hmac->digest();
  // if (mbedtls_ct_memcmp(mac.data(), tag.data(), tag.size()) != 0) {
  //   throw authentication_error();
  // }

  // Decrypt with AES-CM
  ctr_crypt(encryption_algorithm, enc_key, nonce, pt, ct.subspan(0, inner_ct_size));

  return pt.subspan(0, inner_ct_size);
}

output_bytes
MbedTLSProvider::open_aead(EncryptionAlgorithm algorithm,
                           std::size_t tag_size,
                           const bytes& key,
                           const bytes& nonce,
                           output_bytes pt,
                           input_bytes aad,
                           input_bytes ct) const
{
  if (ct.size() < tag_size) {
    throw buffer_too_small_error("Ciphertext buffer too small");
  }

  auto inner_ct_size = ct.size() - tag_size;
  if (pt.size() < inner_ct_size) {
    throw buffer_too_small_error("Plaintext buffer too small");
  }

  auto ctx =
    scoped_cipher_ctx(new mbedtls_cipher_context_t(), mbedtls_cipher_free);
  if (ctx.get() == nullptr) {
    throw std::runtime_error("Failed to allocate cipher context");
  }

  mbedtls_cipher_init(ctx.get());
  const auto* cipher =
    mbedtls_cipher_info_from_type(mbedtls_from_type(algorithm));
  if (cipher == nullptr) {
    throw unsupported_ciphersuite_error();
  }

  const auto setup = mbedtls_cipher_setup(ctx.get(), cipher);
  if (setup != 0) {
    throw std::runtime_error("Failed to setup cipher context");
  }

  const auto keyed = mbedtls_cipher_setkey(
    ctx.get(), key.data(), key.size() * 8, MBEDTLS_ENCRYPT);
  if (keyed != 0) {
    throw std::runtime_error("Failed to set key");
  }

  std::size_t outlen = 0;
  const int decrypt = mbedtls_cipher_auth_decrypt_ext(ctx.get(),
                                                      nonce.data(),
                                                      nonce.size(),
                                                      aad.data(),
                                                      aad.size(),
                                                      ct.data(),
                                                      ct.size(),
                                                      pt.data(),
                                                      pt.size(),
                                                      &outlen,
                                                      tag_size);
  if (decrypt != 0) {
    throw std::runtime_error("Failed to encrypt");
  }
  return pt.subspan(0, inner_ct_size);
}

} // namespace mbedtls
} // namespace provider
} // namespace sframe
