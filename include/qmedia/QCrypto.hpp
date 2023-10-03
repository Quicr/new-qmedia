#pragma once

#include <mutex>
#include <map>
#include <vector>
#include <optional>
#include "sframe/sframe.h"
#include "quicr/namespace.h"
#include "quicr/quicr_common.h"

namespace qmedia
{

class QSFrameContext;

class MLSClient
{
public:
    [[nodiscard]] std::shared_ptr<QSFrameContext> make_sframe_context();

protected:
    std::vector<std::weak_ptr<QSFrameContext>> sframe_contexts;

    static constexpr sframe::CipherSuite sframe_cipher_suite = sframe::CipherSuite::AES_GCM_128_SHA256;
};

class QSFrameContext
{
public:
    void addEpoch(uint64_t epoch_id, const quicr::bytes& epoch_secret);
    void enableEpoch(uint64_t epoch_id);

    sframe::output_bytes protect(const quicr::Namespace& quicr_namespace,
                                 sframe::Counter ctr,
                                 sframe::output_bytes ciphertext,
                                 const sframe::input_bytes plaintext);

    sframe::output_bytes unprotect(uint64_t epoch,
                                   const quicr::Namespace& quicr_namespace,
                                   sframe::Counter ctr,
                                   sframe::output_bytes plaintext,
                                   const sframe::input_bytes ciphertext);

protected:
    // Instances must be obtained via MLSClient::make_sframe_context()
    friend class MLSClient;
    QSFrameContext(sframe::CipherSuite cipher_suite);

    void ensure_key(uint64_t epoch_id, const quicr::Namespace& quicr_namespace);
    sframe::bytes derive_base_key(uint64_t epoch_id, const quicr::Namespace& quicr_namespace);

    sframe::CipherSuite cipher_suite;
    std::optional<uint64_t> current_epoch;
    std::map<uint64_t, quicr::bytes> epoch_secrets;
    std::map<quicr::Namespace, sframe::ContextBase> ns_contexts;

    std::mutex context_mutex;
};

}        // namespace qmedia
