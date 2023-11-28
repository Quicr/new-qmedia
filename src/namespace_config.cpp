#include "../include/qmedia/namespace_config.h"

using quicr::Name;
using quicr::Namespace;
namespace tls = mls::tls;

static const auto zero_name = 0x00000000000000000000000000000000_name;

SubNamespace::SubNamespace()
  : ns(zero_name, 0)
{
}

SubNamespace::SubNamespace(Namespace ns_in)
  : ns(std::move(ns_in))
{
}

SubNamespace::operator Namespace() const
{
  return ns;
}

SubNamespace
SubNamespace::extend(uint64_t value, uint8_t bits) const
{
  if (bits > 64) {
    throw std::runtime_error("Cannot extend by more than 64 bits at once");
  }

  if (ns.length() + bits > name_width) {
    throw std::runtime_error("Cannot extend name past 128 bits");
  }

  const auto new_length = ns.length() + bits;
  const auto shift = name_width - new_length;

  auto delta = zero_name;
  if (bits != 64) {
    const auto mask = (uint64_t(1) << bits) - 1;
    value = value & mask;
  }
  delta += value;
  delta <<= shift;
  const auto new_name = ns.name() + delta;

  return SubNamespace(Namespace(new_name, new_length));
}

NamespaceConfig::NamespaceConfig(quicr::Namespace welcome_ns,
                                 quicr::Namespace group_ns,
                                 uint32_t endpoint_id)
  : welcome_sub_ns(welcome_ns)
  , group_sub_ns(group_ns)
{
  welcome_pub_ns = welcome_sub_ns.extend(endpoint_id, endpoint_bits);
  group_pub_ns = group_sub_ns.extend(endpoint_id, endpoint_bits);

  welcome_next = quicr::Namespace(welcome_pub_ns).name();
  group_next = quicr::Namespace(group_pub_ns).name();
}

Name
NamespaceConfig::for_welcome()
{
  const auto out = welcome_next;
  welcome_next += 1;
  return out;
}

Name
NamespaceConfig::for_group()
{
  const auto out = group_next;
  group_next += 1;
  return out;
}
