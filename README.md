## Building

### Prerequisite

Building QuicR transport requires following 3 projects

1: picoTLS

2: picoQuic

3: quicrq

These 3 projects needs to be clone in the directories
parallel to qmedia project and have to be buit successfully.

On OSX you will need to install  xcode, cmake, nasm, and likely more ..

Please see BUILD-PREREQUISITES.md for additional details.

### Build 

Using the convenient Makefile that wraps CMake:

```
> make
> make test
```

CMake options:

* BUILD_TESTS defaults to OFF
* BUILD_EXTERN defaults to ON
