# Ensure hiredis is build with TLS support
set(ENABLE_SSL ON CACHE BOOL "Enable TLS in Hiredis")

# Request Hiredis to build static libraries
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Do not build Hiredis as shared libraries")

# Disable tests in hiredis
set(DISABLE_TESTS ON CACHE BOOL "Disable building Hiredis tests")

# Add the hiredis directory
add_subdirectory(hiredis)

# Set the following for redis-plus-plus
set(HIREDIS_HEADER ${CMAKE_CURRENT_SOURCE_DIR})
set(HIREDIS_LIB ${hiredis_BINARY_DIR}/libhiredisd.a)
set(HIREDIS_TLS_LIB ${hiredis_BINARY_DIR}/libhiredis_ssld.a)

# Do not build Redis++ tests
set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "Disable Resis++ Tests")

# Enable TLS in Redis++
set(REDIS_PLUS_PLUS_USE_TLS ON CACHE BOOL "Enable TLS support in Redis++")

add_subdirectory(redis-plus-plus)
