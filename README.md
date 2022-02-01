# libssq

☢️ A modern [Source Server query protocol](https://developer.valvesoftware.com/wiki/Server_queries) library written in C.

(This is the up-to-date version of a previous `libssq` repository I have deleted.)

## Support

This library supports all of the undeprecated A2S queries:
- `A2S_INFO`
- `A2S_PLAYER`
- `A2S_RULES`

It has **no dependencies** and is meant to be built on both **MS/Windows** and **UNIX** operating systems.

## Build

This library is meant to be built using [CMake](https://cmake.org/).

Here is an example of how you would build the library from the command line:

```sh
$ mkdir build
$ cd build/
$ cmake ..
$ cmake --build .
```

## Documentation

Please refer to this repository's [wiki](https://github.com/BinaryAlien/libssq/wiki) for documentation about the library.

In addition, you will find an `example` folder at the root of this repository which contains the source code of a simple program. This program takes a hostname and a port number as arguments and pretty-prints the responses of all the supported A2S queries to the standard output.

If you just want to get started right away, click [here](https://github.com/BinaryAlien/libssq/wiki/Getting-started)!
