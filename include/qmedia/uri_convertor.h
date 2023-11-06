#pragma once

#include <UrlEncoder.h>
#include "quicr/quicr_common.h"

struct NumeroURIConvertor : public quicr::UriConvertor
{
  virtual ~NumeroURIConvertor() = default;

  NumeroURIConvertor() {
  }

  void add_uri_templates(std::vector<std::string> templates) {
    for (auto uri : templates) {
      url_encoder.AddTemplate(uri);
    }
  }

  std::string to_namespace_uri(quicr::Namespace ns) override {
    auto uri = url_encoder.DecodeUrl(ns);
    return uri;
  }

  std::string to_name_uri(quicr::Namespace /*n*/) override {
    return "";
  }

  quicr::Namespace to_quicr_namespace(const std::string& uri) override {
    return url_encoder.EncodeUrl(uri);
  }

  UrlEncoder url_encoder;

};