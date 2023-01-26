
add_executable(neotest neotest.cc)

link_directories(/Users/shenning/M10x/new-qmedia/build/src/extern)
target_link_libraries(neotest  PUBLIC /Users/shenning/M10x/new-qmedia/build/src/extern/libneo_media_client.dylib)

#add_executable(ctest ctest.cc)
target_compile_options(neotest
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
        $<$<CXX_COMPILER_ID:MSVC>: >)
set_target_properties(neotest
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS OFF)
