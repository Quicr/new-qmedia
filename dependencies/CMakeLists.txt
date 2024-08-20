add_subdirectory(libquicr)

option (INCLUDE_JSON "Turn on/off json build." ON)
if (INCLUDE_JSON)
add_subdirectory(json)
endif()


add_subdirectory(numero-uri)
set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libquicr/dependencies/transport/dependencies/boringssl")
set(OPENSSL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libquicr/dependencies/transport/dependencies/boringssl/include")
set(OPENSSL_CRYPTO_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/libquicr/dependencies/transport/dependencies/boringssl/crypto/libcrypto.a")
add_subdirectory(sframe)

if(BUILD_TESTING AND QMEDIA_BUILD_TESTS AND NOT TARGET doctest)
  add_subdirectory( doctest )
endif()
