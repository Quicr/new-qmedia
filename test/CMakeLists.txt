# Test Binary

add_executable(qmedia_test
               main.cpp
               manifest.cpp
               qmedia.cpp
               relay.cpp)
target_include_directories(qmedia_test PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(qmedia_test PRIVATE qmedia doctest::doctest)

target_compile_options(qmedia_test
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)

set_target_properties(qmedia_test
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS ON)
if(MSVC)
    target_compile_definitions(qmedia_test _CRT_SECURE_NO_WARNINGS)
endif()

include(${CMAKE_SOURCE_DIR}/dependencies/doctest/scripts/cmake/doctest.cmake)
doctest_discover_tests(qmedia_test
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/test")
