#include <bit>
#include "qmedia/QCrypto.hpp"
#include "sframe/crypto.h"

namespace qmedia
{
QSFrameContext::QSFrameContext(sframe::CipherSuite cipher_suite) :
    cipher_suite(cipher_suite)
{
    // Nothing more to do
}

void QSFrameContext::addEpoch(uint64_t epoch_id,
                              const quicr::bytes &epoch_secret)
{
    std::lock_guard<std::mutex> lock(context_mutex);
    epoch_secrets[epoch_id] = epoch_secret;
}

void QSFrameContext::enableEpoch(uint64_t epoch_id)
{
    std::lock_guard<std::mutex> lock(context_mutex);
    current_epoch = epoch_id;
}

sframe::output_bytes QSFrameContext::protect(
    const quicr::Namespace &quicr_namespace,
    sframe::Counter ctr,
    sframe::output_bytes ciphertext,
    const sframe::input_bytes plaintext)
{
    std::lock_guard<std::mutex> lock(context_mutex);
    if (!current_epoch.has_value()) return {};
    ensure_key(*current_epoch, quicr_namespace);
    return ns_contexts.at(quicr_namespace).protect(
        sframe::Header(*current_epoch, ctr),
        ciphertext,
        plaintext);
}

sframe::output_bytes QSFrameContext::unprotect(
    uint64_t epoch,
    const quicr::Namespace &quicr_namespace,
    sframe::Counter ctr,
    sframe::output_bytes plaintext,
    const sframe::input_bytes ciphertext)
{
    std::lock_guard<std::mutex> lock(context_mutex);
    ensure_key(epoch, quicr_namespace);
    return ns_contexts.at(quicr_namespace)
        .unprotect(sframe::Header(epoch, ctr), plaintext, ciphertext);
}

void QSFrameContext::ensure_key(uint64_t epoch_id,
                                const quicr::Namespace &quicr_namespace)
{
    // NOTE: caller must lock the mutex

    if ((ns_contexts.count(quicr_namespace) > 0) &&
        !ns_contexts.at(quicr_namespace).has_key(epoch_id))
    {
        return;
    }

    if (ns_contexts.count(quicr_namespace) == 0)
    {
        ns_contexts.emplace(quicr_namespace, sframe::ContextBase(cipher_suite));
    }

    const auto base_key = derive_base_key(epoch_id, quicr_namespace);
    ns_contexts.at(quicr_namespace).add_key(epoch_id, base_key);
}

sframe::bytes QSFrameContext::derive_base_key(
    uint64_t epoch_id,
    const quicr::Namespace &quicr_namespace)
{
    // NOTE: caller must lock the mutex
    std::string salt_string =
        "Quicr epoch base key " + std::string(quicr_namespace);
    sframe::bytes salt(salt_string.begin(), salt_string.end());
    return sframe::hkdf_extract(cipher_suite, salt, epoch_secrets[epoch_id]);
}

} // namespace qmedia
