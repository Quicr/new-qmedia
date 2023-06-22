/*
 *  quic_varint.cpp
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

#include <limits>
#include "quic_varint.h"

/*
 *  QUICVarIntSize
 *
 *  Description:
 *      This function will determine the number of octets required to encode
 *      the given integer as a variable-width integer as per RFC 9000.  The
 *      largest allowable integer is (2^62) - 1.
 *
 *  Parameters:
 *      value [in]
 *          The value to be encoded.
 *
 *  Returns:
 *      The number of octets required to encode the integer or 0 if the integer
 *      is too large.
 *
 *  Comments:
 *      None.
 */
std::size_t QUICVarIntSize(std::uint64_t value)
{
    if (value < std::uint64_t(64)) return 1;
    if (value < std::uint64_t(16384)) return 2;
    if (value < std::uint64_t(1073741824)) return 4;
    if (value < std::uint64_t(4611686018427387904)) return 8;

    return 0;
}

/*
 *  QUICVarIntSize
 *
 *  Description:
 *      This function accepts a pointer to a buffer that points to an encoded
 *      variable-length integer and determines the length of that encoded
 *      variable length integer per RFC 9000.
 *
 *  Parameters:
 *      value [in]
 *          A pointer to a variable-length integer encoded per RFC 9000.
 *
 *  Returns:
 *      The number of octets used to encode the integer.
 *
 *  Comments:
 *      None.
 */
std::size_t QUICVarIntSize(const std::uint8_t *value)
{
    return std::size_t(1) << ((*value & 0xc0) >> 6);
}

/*
 *  QUICVarIntEncode
 *
 *  Description:
 *      This function accepts an unsigned integer and associated buffer,
 *      encoding that integer as per RFC 9000 and returning a count of octets
 *      required to store the encoded integer.
 *
 *  Parameters:
 *      value [in]
 *          The unsigned integer to be encoded as a variable-length integer.
 *
 *  Returns:
 *      The number of octets produced or 0 if there was an error.
 *
 *  Comments:
 *      None.
 */
std::size_t QUICVarIntEncode(std::uint64_t value, std::uint8_t *buffer)
{
    // Determine the length of the encoded integer
    std::size_t length = QUICVarIntSize(value);

    // Encode given the expected length
    switch (length)
    {
        case 1:
            *buffer = value & 0xff;
            break;

        case 2:
            *(buffer    ) = (value >> 8) | 0x40;
            *(buffer + 1) = (value     ) & 0xff;
            break;

        case 4:
            *(buffer    ) = (value >> 24) | 0x80;
            *(buffer + 1) = (value >> 16) & 0xff;
            *(buffer + 2) = (value >>  8) & 0xff;
            *(buffer + 3) = (value      ) & 0xff;
            break;

        case 8:
            *(buffer    ) = (value >> 56) | 0xc0;
            *(buffer + 1) = (value >> 48) & 0xff;
            *(buffer + 2) = (value >> 40) & 0xff;
            *(buffer + 3) = (value >> 32) & 0xff;
            *(buffer + 4) = (value >> 24) & 0xff;
            *(buffer + 5) = (value >> 16) & 0xff;
            *(buffer + 6) = (value >>  8) & 0xff;
            *(buffer + 7) = (value      ) & 0xff;
            break;

        default:
            break;
    }

    return length;
}

/*
 *  QUICVarIntDecode
 *
 *  Description:
 *      This function accepts an encoded buffer and decodes the variable-length
 *      integer as specified in RFC 9000.
 *
 *  Parameters:
 *      buffer [in]
 *          A buffer pointing to the variable-length integer.  It is the
 *          caller's responsibility to ensure the buffer size is sufficient
 *          for the encoded integer.  This can be verified by first calling
 *          QUICVarIntSize().
 *
 *  Returns:
 *      The the decoded variable-length integer or the constant value
 *      std::numeric_limits<std::uint64_t>::max() if the encoded integer
 *      is not valid.  A valid value will always be in the range of
 *      0 .. (2^63) - 1.
 *
 *  Comments:
 *      None.
 */
std::uint64_t QUICVarIntDecode(const std::uint8_t *buffer)
{
    std::uint64_t result = std::numeric_limits<std::uint64_t>::max();

    // Determine the length of the encoded integer
    std::size_t length = QUICVarIntSize(buffer);

    // Decode the expected number of octets
    switch (length)
    {
        case 1:
            result = (*buffer) & 0x3f;
            break;

        case 2:
            result = (*buffer) & 0x3f;
            result = (result << 8) | (*(buffer + 1));
            break;

        case 4:
            result = ((static_cast<std::uint64_t>(*(buffer) & 0x3f) << 24) |
                      (static_cast<std::uint64_t>(*(buffer + 1)   ) << 16) |
                      (static_cast<std::uint64_t>(*(buffer + 2)   ) <<  8) |
                      (static_cast<std::uint64_t>(*(buffer + 3)   )      ));
            break;

        case 8:
            result = ((static_cast<std::uint64_t>(*(buffer) & 0x3f) << 56) |
                      (static_cast<std::uint64_t>(*(buffer + 1)   ) << 48) |
                      (static_cast<std::uint64_t>(*(buffer + 2)   ) << 40) |
                      (static_cast<std::uint64_t>(*(buffer + 3)   ) << 32) |
                      (static_cast<std::uint64_t>(*(buffer + 4)   ) << 24) |
                      (static_cast<std::uint64_t>(*(buffer + 5)   ) << 16) |
                      (static_cast<std::uint64_t>(*(buffer + 6)   ) <<  8) |
                      (static_cast<std::uint64_t>(*(buffer + 7)   )      ));
            if (result >= (std::uint64_t(1) << 62))
            {
                result = std::numeric_limits<std::uint64_t>::max();
            }
            break;

        default:
            result = std::numeric_limits<std::uint64_t>::max();
            break;
    }

    return result;
}
