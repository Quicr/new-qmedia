
set(TEST_APP_NAME "${LIB_NAME}_test")

# Dependencies
find_package(doctest REQUIRED)

# Test Binary
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

add_executable(${TEST_APP_NAME} ${TEST_SOURCES})
target_link_libraries(${TEST_APP_NAME} ${LIB_NAME} doctest::doctest)
target_compile_definitions(${PROJECT_NAME} INTERFACE -DDATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/")

# Enable CTest
include(doctest)
enable_testing()
doctest_discover_tests(${TEST_APP_NAME} ADD_LABELS 0)