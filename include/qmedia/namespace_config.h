#pragma once
#include <mls/core_types.h>
#include <quicr/namespace.h>

// # Namespace Structure
//
// Each group has two namespaces:
//
// 1. A Welcome namespace, to which joiners subscribe and Welcomes are sent
// 2. A group namespace, to which everyone subscribes and everything else is
// sent
//
// Each sender allocates a 24-bit sub-namespace underneath these namespaces,
// based on their endpoint ID.  The sender then sets the low-order bits to
// ensure unique names.
struct SubNamespace
{
  quicr::Namespace ns;

  SubNamespace();
  SubNamespace(quicr::Namespace ns_in);

  operator quicr::Namespace() const;

  // Extend by a specific number of bits (no more than 64)
  SubNamespace extend(uint64_t value, uint8_t bits) const;

private:
  static const size_t name_width = 128;
};

struct NamespaceConfig
{
  NamespaceConfig(quicr::Namespace welcome_ns,
                  quicr::Namespace group_ns,
                  uint32_t endpoint_id);

  // Namespaces to subscribe to
  quicr::Namespace welcome_sub() const { return welcome_sub_ns; }
  quicr::Namespace group_sub() const { return group_sub_ns; }

  // Namespaces to publish within
  quicr::Namespace welcome_pub() const { return welcome_pub_ns; }
  quicr::Namespace group_pub() const { return group_pub_ns; }

  // Form specific names
  quicr::Name for_welcome();
  quicr::Name for_group();

private:
  static const size_t endpoint_bits = 24;

  SubNamespace welcome_sub_ns;
  SubNamespace group_sub_ns;

  SubNamespace welcome_pub_ns;
  SubNamespace group_pub_ns;

  quicr::Name welcome_next;
  quicr::Name group_next;
};
