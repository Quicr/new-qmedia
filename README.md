qmedia
========

[![CMake](https://github.com/Quicr/new-qmedia/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/Quicr/new-qmedia/actions/workflows/cmake.yml)

QMedia is a data driven interface to the QUIC media transmission library [libquicr](https://github.com/Quicr/libquicr). 

A JSON "manifest" drives QMedia to automatically create delegates for `libquicr` subscriptions and publications.

## Build

> [!WARNING]
> Submodules are used, make sure they are up to date. 

### Update Submodules

```
git submodule update --init --recursive
```

### Make
Use `make` to build and test.

