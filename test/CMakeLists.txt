
# Test Binary
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

add_executable(qmedia_test
  qmedia_test.cc
#  metrics.cc
#  packet.cc
#  rtx_manager.cc
#  simple_packetize.cc
#  simple_video_assembler.cc
#  transport.cc
  )

target_link_libraries(qmedia_test ${LIB_NAME} doctest::doctest)

#target_compile_definitions(${PROJECT_NAME} INTERFACE -DDATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/")


#include(doctest)
#enable_testing()
#doctest_discover_tests(qmedia_test ADD_LABELS 0)

add_test(NAME qmedia_test 
         COMMAND qmedia_test )

