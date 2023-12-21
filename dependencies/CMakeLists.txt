add_subdirectory(libquicr)
add_subdirectory(json)
add_subdirectory(numero-uri)
add_subdirectory(redis)
set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libquicr/dependencies/transport/dependencies/boringssl")
set(OPENSSL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libquicr/dependencies/transport/dependencies/boringssl/include")
set(OPENSSL_CRYPTO_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/libquicr/dependencies/transport/dependencies/boringssl/crypto/libcrypto.a")
add_subdirectory(sframe)

if(BUILD_TESTING AND QMEDIA_BUILD_TESTS AND NOT TARGET doctest)
  add_subdirectory( doctest )
endif()

