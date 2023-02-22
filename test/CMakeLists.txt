cmake_minimum_required(VERSION 3.16)

project(qmedia_test VERSION 0.0.1 LANGUAGES CXX)

find_package(doctest REQUIRED)

#add_executable(qmedia_test media_client_test.cc)

#target_compile_features(qmedia_test PRIVATE cxx_std_20)
#
#target_include_directories(qmedia_test PUBLIC ${PROJECT_SOURCE_DIR}/../include)
#
##target_link_libraries(qmedia_test PRIVATE qmedia quicr quicr-transport doctest::doctest)
#target_link_libraries(qmedia_test PRIVATE quicr doctest::doctest)
#
##include(${CMAKE_SOURCE_DIR}/dependencies/doctest/scripts/cmake/doctest.cmake)
##doctest_discover_tests(qmedia_test)

## Test Binary
##file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
#
#add_test(NAME qmedia_test 
         #COMMAND qmedia_test )
#
#set_target_properties(qmedia_test
    #PROPERTIES
        #CXX_STANDARD 20
        #CXX_STANDARD_REQUIRED YES
        #CXX_EXTENSIONS OFF)
#
#
#target_link_libraries(qmedia_test PRIVATE  quicr)
#
##target_compile_definitions(${PROJECT_NAME} INTERFACE -DDATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/")
#
##include(doctest)
##enable_testing()
##doctest_discover_tests(qmedia_test ADD_LABELS 0)


