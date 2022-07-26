###
### Dependencies
###
find_package(OpenSSL 1.1 REQUIRED)

###
### Library Config
###

# curl for influx
find_package(CURL REQUIRED)

add_library(metrics
    src/measurements.cc
    src/metrics.cc)
target_link_libraries(metrics
    PUBLIC
        ${CURL_LIBRARIES}
        OpenSSL::SSL
        OpenSSL::Crypto)
target_include_directories(metrics
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include)
target_compile_options(metrics
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)
set_target_properties(metrics
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF)

###
### Tests
###
include(CTest)
if(BUILD_TESTING AND qmedia_BUILD_TESTS)
    add_subdirectory(test)
endif()
