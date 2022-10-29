set(CURRENT_LIB_NAME metrics)

###
### Dependencies
###
find_package(OpenSSL 1.1 REQUIRED)

###
### Library Config
###

# curl for influx
find_package(CURL REQUIRED)

file(GLOB_RECURSE LIB_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

add_library(${CURRENT_LIB_NAME} ${LIB_HEADERS} ${LIB_SOURCES})
target_link_libraries(${CURRENT_LIB_NAME} PUBLIC ${CURL_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(${CURRENT_LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(${CURRENT_LIB_NAME}
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)
set_target_properties(${CURRENT_LIB_NAME}
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
