

add_subdirectory(libquicr)

option (INCLUDE_JSON "Turn on/off json build." ON)
if (INCLUDE_JSON)
add_subdirectory(json)
endif()


add_subdirectory(numero-uri)
if (USE_MBEDTLS)
  message(STATUS "Setting SFrame to use MbedTLS")
  set(BUILTIN_PROVIDER OFF CACHE BOOL "Use the built-in OpenSSL crypto provider")
  set(BUILD_OPENSSL OFF CACHE BOOL "Build the OpenSSL crypto provider")
  cmake_policy(SET CMP0077 NEW)
  add_subdirectory(sframe-mbedtls)
else()
  set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libquicr/dependencies/transport/dependencies/boringssl")
  set(OPENSSL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libquicr/dependencies/transport/dependencies/boringssl/include")
  set(OPENSSL_CRYPTO_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/libquicr/dependencies/transport/dependencies/boringssl/crypto/libcrypto.a")
endif()
add_subdirectory(sframe)

if(BUILD_TESTING AND QMEDIA_BUILD_TESTS AND NOT TARGET doctest)
  add_subdirectory( doctest )
endif()

