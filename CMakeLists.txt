cmake_minimum_required(VERSION 3.22)
project(RCOM C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(RCOM
        linkLayer.c
        linkLayer.h
        read_noncanonical.c
        write_noncanonical.c applicationLayer.c applicationLayer.h)
