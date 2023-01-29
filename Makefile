# This is just a convenience Makefile to avoid having to remember
# all the CMake commands and their arguments.

# Set CMAKE_GENERATOR in the environment to select how you build, e.g.:
#   CMAKE_GENERATOR=Ninja

BUILD_DIR=build
CLANG_FORMAT=clang-format -i

.PHONY: all clean cclean format

all: build
	cmake --build build

build: CMakeLists.txt test/CMakeLists.txt cmd/CMakeLists.txt
	cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug -DQMEDIA_BUILD_TESTS=ON -DBUILD_TESTING=ON  .

clean:
	cmake --build build --target clean

cclean:
	rm -rf build

format:
	find include -iname "*.hh" -or -iname "*.cc" | xargs ${CLANG_FORMAT}
	find src -iname "*.hh" -or -iname "*.cc" | xargs ${CLANG_FORMAT}
	find cmd -iname "*.hh" -or -iname "*.cc" | xargs ${CLANG_FORMAT}
	find test -iname "*.hh" -or -iname "*.cc" | xargs ${CLANG_FORMAT}
