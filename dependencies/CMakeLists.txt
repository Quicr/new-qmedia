add_subdirectory(libquicr)

if(BUILD_TESTING AND QMEDIA_BUILD_TESTS AND NOT TARGET doctest)
  add_subdirectory( doctest )
endif()

