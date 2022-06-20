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
list(APPEND LIBRARIES ${CURL_LIBRARIES})

file(GLOB_RECURSE LIB_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh")
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

add_library(${CURRENT_LIB_NAME} ${LIB_HEADERS} ${LIB_SOURCES})
target_link_libraries(${CURRENT_LIB_NAME} PRIVATE ${LIBRARIES})
target_include_directories(${CURRENT_LIB_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

###
### Tests
###

#if (TESTING)
    add_subdirectory(test)
#endif()