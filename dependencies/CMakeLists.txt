add_subdirectory(libquicr)
add_subdirectory(json)
add_subdirectory(numero-uri)
add_subdirectory(sframe)

if(BUILD_TESTING AND QMEDIA_BUILD_TESTS AND NOT TARGET doctest)
  add_subdirectory( doctest )
endif()

