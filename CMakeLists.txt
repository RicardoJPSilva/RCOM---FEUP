cmake_minimum_required(VERSION 3.23)
project(RCOM___FEUP C)

set(CMAKE_C_STANDARD 11)

add_executable(read read_noncanonical.c)

add_executable(write data-link.c data-link.h application.c)

add_executable(emissor data-link.c data-link.h application.c)

add_executable(receptor data-link.c data-link.h application.c)