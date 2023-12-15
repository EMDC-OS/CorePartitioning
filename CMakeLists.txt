cmake_minimum_required(VERSION 3.25)
project(CorePartitioning C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(CorePartitioning
        affinity.c
        data_types.h
        debug.h
        fdtable.c
        fdtable.h
        ipc.c
        ipc.h
        ld_preload.c
        ld_preload.h
        print.c
        print.h)
