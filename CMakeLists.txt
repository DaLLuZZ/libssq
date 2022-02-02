cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_STANDARD 99)

if (UNIX)
    add_definitions(-D_POSIX_C_SOURCE=200112L)
endif (UNIX)

project(ssq VERSION 0.0.0)

add_library(ssq STATIC
    src/buffer.c
    src/packet.c
    src/query.c
    src/a2s/info.c
    src/a2s/player.c
    src/a2s/rules.c
)

target_include_directories(ssq PUBLIC include/)
