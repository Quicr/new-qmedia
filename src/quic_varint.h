/*
 *  quic_varint.h
 *
 *  Copyright (C) 2023
 *  Cisco Systems, Inc.
 *  All Rights Reserved.
 *
 *  Description:
 *      QUIC encodes integers as variable-length values on the wire (see
 *      RFC 9000 section 16).  These utility functions facilitate the
 *      encoding and decoding of those integers.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <cstdlib>
#include <cstdint>

//  Function prototypes
std::size_t QUICVarIntSize(std::uint64_t value);
std::size_t QUICVarIntSize(const std::uint8_t *value);
std::size_t QUICVarIntEncode(std::uint64_t value, std::uint8_t *buffer);
std::uint64_t QUICVarIntDecode(const std::uint8_t *buffer);
