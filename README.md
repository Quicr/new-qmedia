Building
--------

## Pre Conditions

On OSX you will need to install  xcode, cmake, nasm, and likely more ..


## Building

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
make broadcast   -- builds broadcastSFU
make sound       -- builds sound client
make format      -- runs clang formattin
```
  
Links
-----

* [vcpkg](https://github.com/Microsoft/vcpkg)

