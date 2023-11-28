#pragma once

// Annoyingly, all of the methods in the Quicr delegates are pure virtual, even
// though they can be safely not implemented.  These null delegates provide
// default noop implementations to reduce boilerplate in actual implementations.

#include "quicr/quicr_client_delegate.h"

struct NullSubscribeDelegate : quicr::SubscriberDelegate
{
  void onSubscribeResponse(const quicr::Namespace& /* quicr_namespace */,
                           const quicr::SubscribeResult& /* result */) override
  {
  }

  void onSubscriptionEnded(
    const quicr::Namespace& /* quicr_namespace */,
    const quicr::SubscribeResult::SubscribeStatus& /* reason */) override
  {
  }

  void onSubscribedObject(const quicr::Name& /* quicr_name */,
                          uint8_t /* priority */,
                          uint16_t /* expiry_age_ms */,
                          bool /* use_reliable_transport */,
                          quicr::bytes&& /* data */) override
  {
  }

  void onSubscribedObjectFragment(const quicr::Name& /* quicr_name */,
                                  uint8_t /* priority */,
                                  uint16_t /* expiry_age_ms */,
                                  bool /* use_reliable_transport */,
                                  const uint64_t& /* offset */,
                                  bool /* is_last_fragment */,
                                  quicr::bytes&& /* data */) override
  {
  }
};
