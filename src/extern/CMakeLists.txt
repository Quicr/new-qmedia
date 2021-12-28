
project(neo_media_client)
set(headers neo_media_client.hh)
set(sources neo_media_client.cc)
add_library(${PROJECT_NAME} SHARED ${sources} ${headers})
target_link_libraries(${PROJECT_NAME} PUBLIC neoMedia)
