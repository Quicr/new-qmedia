add_subdirectory(libquicr)

if(BUILD_TESTING AND QMEDIA_BUILD_TESTS)
  add_subdirectory( doctest )
endif()

