cmake_minimum_required(VERSION 3.0)
project(sysopy C)

set(CMAKE_C_STANDARD 99)

add_executable(sysopy cw01/zad1/find_lib.c cw01/zad1/find_lib.h cw01/zad2/main.c
        cw01/zad3/main.c cw01/zad3/find_lib.c cw01/zad3/find_lib.h cw01/zad2/find_lib.c cw01/zad2/find_lib.h)
