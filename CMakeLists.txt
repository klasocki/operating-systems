cmake_minimum_required(VERSION 3.0)
project(sysopy C)

set(CMAKE_C_STANDARD 99)

add_executable(sysopy LasockiKarol/cw01/zad1/find_lib.c LasockiKarol/cw01/zad1/find_lib.h LasockiKarol/cw01/zad2/main.c
        LasockiKarol/cw01/zad3/main.c LasockiKarol/cw01/zad3/find_lib.c LasockiKarol/cw01/zad3/find_lib.h LasockiKarol/cw01/zad2/find_lib.c LasockiKarol/cw01/zad2/find_lib.h)
