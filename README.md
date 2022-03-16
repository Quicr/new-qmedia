Building
--------

## Pre Conditions

On OSX you will need to install  xcode, cmake, nasm, and likely more ..


## Building

### Prerequisite
Building QuicR transport requires following 3 projects
1: picoTLS
2: picoQuic
3: quicrq

These 3 projects needs to be clone in the directories
parallel to qmedia project and have to be buit successfully.

Todo: future versions might simply with either a docker image
or prebuilt binaries


Create build configuration.  From inside the project directory, issue the following command to build the project configuration.

```
cmake -B build -S .
```

CMake options:

* BUILD_TESTS defaults to OFF
* BUILD_EXTERN defaults to ON

Once the configuration is created, you can build the project using the following command.

```
cmake --build build
```

If you prefer makefile option

```
make all         -- builds library
make test        -- buids test binary under buid/test
make echo        -- builds echoSFU
make format      -- runs clang formatting
```
  
Links
-----

* [vcpkg](https://github.com/Microsoft/vcpkg)

