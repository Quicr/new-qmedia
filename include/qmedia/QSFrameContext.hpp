#pragma once

#include <mutex>
#include <map>
#include <vector>
#include <optional>
#include "sframe/sframe.h"
#include "quicr/quicr_namespace.h"
#include "quicr/quicr_common.h"

namespace qmedia
{

class QSFrameContext
{
public:
    QSFrameContext(sframe::CipherSuite cipher_suite);

    void addEpoch(uint64_t epoch_id, const quicr::bytes &epoch_secret);
    void enableEpoch(uint64_t epoch_id);

    sframe::output_bytes protect(const quicr::Namespace &quicr_namespace,
                                 sframe::Counter ctr,
                                 sframe::output_bytes ciphertext,
                                 const sframe::input_bytes plaintext);

    sframe::output_bytes unprotect(uint64_t epoch,
                                   const quicr::Namespace &quicr_namespace,
                                   sframe::Counter ctr,
                                   sframe::output_bytes plaintext,
                                   const sframe::input_bytes ciphertext);

protected:
    void ensure_key(uint64_t epoch_id, const quicr::Namespace &quicr_namespace);

    sframe::CipherSuite cipher_suite;
    std::optional<uint64_t> current_epoch;
    std::map<uint64_t, quicr::bytes> epoch_secrets;
    std::map<quicr::Namespace, sframe::ContextBase> ns_contexts; // key_id=epoch

    std::mutex context_mutex;
};

} // namespace qmedia
