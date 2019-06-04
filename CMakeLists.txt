cmake_minimum_required(VERSION 3.0)
project(sysopy C)

set(CMAKE_C_STANDARD 11)

add_executable(sysopy
      LasockiKarol/cw01/zad2/find_lib.c LasockiKarol/cw03/zad1/main.c LasockiKarol/cw03/zad3/monitor.c LasockiKarol/cw03/zad3/tester.c LasockiKarol/cw04/zad2/monitor.c
        LasockiKarol/cw01/zad2/find_lib.h LasockiKarol/cw02/zad1/main.c LasockiKarol/cw02/zad2/main.c LasockiKarol/cw03/zad2/monitor.c LasockiKarol/cw03/zad2/tester.c LasockiKarol/cw04/zad1/main.c LasockiKarol/cw04/zad1/main_script.c LasockiKarol/cw04/zad3a/sender.c LasockiKarol/cw04/zad3a/catcher.c LasockiKarol/cw04/zad3b/sender.c LasockiKarol/cw04/zad3b/catcher.c LasockiKarol/cw05/zad1/main.c "LasockiKarol/cw05/zad2/master.c" LasockiKarol/cw05/zad2/slave.c LasockiKarol/cw06/zad1/chat.h LasockiKarol/cw06/zad1/server.c LasockiKarol/cw06/zad1/client.c LasockiKarol/cw06/zad1/server.c LasockiKarol/cw06/zad1/chat.h LasockiKarol/cw06/zad1/server.c LasockiKarol/cw06/zad1/client.c
        LasockiKarol/cw06/zad2/chat.h LasockiKarol/cw06/zad2/server.c LasockiKarol/cw06/zad2/client.c LasockiKarol/cw06/zad2/server.c LasockiKarol/cw06/zad2/chat.h LasockiKarol/cw06/zad2/server.c LasockiKarol/cw06/zad2/client.c LasockiKarol/cw07/zad1/trucker.c LasockiKarol/cw07/zad1/loader.c LasockiKarol/cw07/zad1/spawner.c
        LasockiKarol/cw07/zad2/trucker.c LasockiKarol/cw07/zad2/loader.c LasockiKarol/cw07/zad2/spawner.c LasockiKarol/cw08/main.c LasockiKarol/cw09/main.c LasockiKarol/cw10/zad1/client.c LasockiKarol/cw10/zad1/server.c LasockiKarol/cw10/zad2/client.c LasockiKarol/cw10/zad2/server.c)
