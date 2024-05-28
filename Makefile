# This is just a convenience Makefile to avoid having to remember
# all the CMake commands and their arguments.

# Set CMAKE_GENERATOR in the environment to select how you build, e.g.:
#   CMAKE_GENERATOR=Ninja

BUILD_DIR=build
CLANG_FORMAT=clang-format -i
TEST_DIR=test
TEST_BIN=${BUILD_DIR}/${TEST_DIR}/qmedia_test

.PHONY: all test clean cclean format

all: ${BUILD_DIR} src/* test/*
	cmake --build build --parallel 8

${BUILD_DIR}: CMakeLists.txt
	cmake -Bbuild -DCMAKE_BUILD_TYPE=Debug -DQMEDIA_BUILD_TESTS=ON -DBUILD_TESTING=ON .

${TEST_BIN}: test/*
	cmake --build build --target qmedia_test

test: ${TEST_BIN} test/*
	cmake --build build --target test

dtest: ${TEST_BIN}
	cd ${TEST_DIR} && ../${TEST_BIN}

dbtest: ${TEST_BIN}
	cd ${TEST_DIR} && lldb ../${TEST_BIN}

clean:
	cmake --build build --target clean

cclean:
	rm -rf build

format:
	find include -iname "*.hpp" -or -iname "*.cpp" | xargs ${CLANG_FORMAT}
	find src -iname "*.hpp" -or -iname "*.cpp" | xargs ${CLANG_FORMAT}
	find test -iname "*.hpp" -or -iname "*.cpp" | xargs ${CLANG_FORMAT}
