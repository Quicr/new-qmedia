#include <bit>
#include <chrono>
#include <qmedia/QCrypto.hpp>
#include <sframe/crypto.h>

using namespace std::chrono_literals;

namespace qmedia
{

MLSClient::MLSClient()
{
  key_rotation_thread = std::thread([&]() {
    static constexpr auto stop_check_interval = 250ms;
    static constexpr auto rotation_interval = 5s;
    static constexpr auto checks_per_rotation = rotation_interval / stop_check_interval;

    static const auto fixed_base_key = sframe::bytes{
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };

    auto epoch = uint64_t(0);
    while (true) {
      for (auto i = uint64_t(0); i < checks_per_rotation; i++) {
        std::this_thread::sleep_for(stop_check_interval);
        if (stop_thread) {
          return;
        }
      }

      // Make a new key for this epoch
      epoch += 1;
      auto base_key = fixed_base_key;
      base_key.at(0) ^= static_cast<uint8_t>(epoch >> 56);
      base_key.at(1) ^= static_cast<uint8_t>(epoch >> 48);
      base_key.at(2) ^= static_cast<uint8_t>(epoch >> 40);
      base_key.at(3) ^= static_cast<uint8_t>(epoch >> 32);
      base_key.at(4) ^= static_cast<uint8_t>(epoch >> 24);
      base_key.at(5) ^= static_cast<uint8_t>(epoch >> 16);
      base_key.at(6) ^= static_cast<uint8_t>(epoch >> 8);
      base_key.at(7) ^= static_cast<uint8_t>(epoch >> 0);

      // Enable the previous epoch's key for encryption, and the current epoch's
      // key for decryption.
      const auto _ = std::unique_lock(sframe_context_mutex);
      for (const auto& ctx_weak : sframe_contexts) {
        if (const auto ctx = ctx_weak.lock()) {
          ctx->enableEpoch(epoch - 1);
          ctx->addEpoch(epoch, fixed_base_key);
        }
      }
    }
  });
}

MLSClient::~MLSClient()
{
  stop_thread = true;
  if (key_rotation_thread && key_rotation_thread->joinable()) {
    key_rotation_thread->join();
  }
}

std::shared_ptr<QSFrameContext> MLSClient::make_sframe_context()
{
    // NB: Rare exception to "no naked new" rule; std::make_shared cannot access
    // non-public constructors.
    const auto _ = std::unique_lock(sframe_context_mutex);
    const auto ctx = std::shared_ptr<QSFrameContext>(new QSFrameContext(sframe_cipher_suite));

    // TODO(richbarn): It would be good to provision the SFrame context with
    // existing keys at this point; otherwise things will be black until the
    // next time a key rotation happens.

    sframe_contexts.push_back(ctx);

    return ctx;
}

QSFrameContext::QSFrameContext(sframe::CipherSuite cipher_suite) : cipher_suite(cipher_suite)
{
    // Nothing more to do
}

void QSFrameContext::addEpoch(uint64_t epoch_id, const quicr::bytes& epoch_secret)
{
    std::lock_guard<std::mutex> lock(context_mutex);
    epoch_secrets[epoch_id] = epoch_secret;
}

void QSFrameContext::enableEpoch(uint64_t epoch_id)
{
    std::lock_guard<std::mutex> lock(context_mutex);
    current_epoch = epoch_id;
}

sframe::output_bytes QSFrameContext::protect(const quicr::Namespace& quicr_namespace,
                                             sframe::Counter ctr,
                                             sframe::output_bytes ciphertext,
                                             const sframe::input_bytes plaintext)
{
    std::lock_guard<std::mutex> lock(context_mutex);
    if (!current_epoch.has_value()) return {};
    ensure_key(*current_epoch, quicr_namespace);
    return ns_contexts.at(quicr_namespace).protect(sframe::Header(*current_epoch, ctr), ciphertext, plaintext);
}

sframe::output_bytes QSFrameContext::unprotect(uint64_t epoch,
                                               const quicr::Namespace& quicr_namespace,
                                               sframe::Counter ctr,
                                               sframe::output_bytes plaintext,
                                               const sframe::input_bytes ciphertext)
{
    std::lock_guard<std::mutex> lock(context_mutex);
    ensure_key(epoch, quicr_namespace);
    return ns_contexts.at(quicr_namespace).unprotect(sframe::Header(epoch, ctr), plaintext, ciphertext);
}

void QSFrameContext::ensure_key(uint64_t epoch_id, const quicr::Namespace& quicr_namespace)
{
    // NOTE: caller must lock the mutex

    if ((ns_contexts.count(quicr_namespace) > 0) && !ns_contexts.at(quicr_namespace).has_key(epoch_id))
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

sframe::bytes QSFrameContext::derive_base_key(uint64_t epoch_id, const quicr::Namespace& quicr_namespace)
{
    // NOTE: caller must lock the mutex
    std::string salt_string = "Quicr epoch base key " + std::string(quicr_namespace);
    sframe::bytes salt(salt_string.begin(), salt_string.end());
    return sframe::hkdf_extract(cipher_suite, salt, epoch_secrets[epoch_id]);
}

}        // namespace qmedia
