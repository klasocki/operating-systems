cmake_minimum_required(VERSION 3.0)
project(sysopy C)

set(CMAKE_C_STANDARD 11)

add_executable(sysopy
      LasockiKarol/cw01/zad2/find_lib.c LasockiKarol/cw03/zad1/main.c LasockiKarol/cw03/zad3/monitor.c LasockiKarol/cw03/zad3/tester.c
        LasockiKarol/cw01/zad2/find_lib.h LasockiKarol/cw02/zad1/main.c LasockiKarol/cw02/zad2/main.c LasockiKarol/cw03/zad2/monitor.c LasockiKarol/cw03/zad2/tester.c)
