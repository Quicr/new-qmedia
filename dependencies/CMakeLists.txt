add_subdirectory(libquicr)
add_subdirectory(json)
add_subdirectory(numero-uri)

if(BUILD_TESTING AND QMEDIA_BUILD_TESTS AND NOT TARGET doctest)
  add_subdirectory( doctest )
endif()

