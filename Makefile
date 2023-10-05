# This is just a convenience Makefile to avoid having to remember
# all the CMake commands and their arguments.

# Set CMAKE_GENERATOR in the environment to select how you build, e.g.:
#   CMAKE_GENERATOR=Ninja

BUILD_DIR=build
CLANG_FORMAT=clang-format -i
TEST_BIN=${BUILD_DIR}/test/qmedia_test

.PHONY: all test clean cclean format

all: ${BUILD_DIR} src/* test/*
	cmake --build build

${BUILD_DIR}: CMakeLists.txt
	cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug -DQMEDIA_BUILD_TESTS=ON -DBUILD_TESTING=ON  .

${TEST_BIN}: 
	cmake --build build --target qmedia_test

test: ${TEST_BIN} test/*
	cmake --build build --target test

dtest: ${TEST_BIN}
	${TEST_BIN}

dbtest: ${TEST_BIN}
	lldb ${TEST_BIN}

clean:
	cmake --build build --target clean

cclean:
	rm -rf build

format:
	find include -iname "*.hpp" -or -iname "*.cpp" | xargs ${CLANG_FORMAT}
	find src -iname "*.hpp" -or -iname "*.cpp" | xargs ${CLANG_FORMAT}
	find test -iname "*.hpp" -or -iname "*.cpp" | xargs ${CLANG_FORMAT}
