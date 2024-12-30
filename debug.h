//
// Created by changyu Lee on 2022/11/29.
//

// debug.h for Core partitioning debug mode

// debug system call's input/output/etc
#ifdef __DEBUG_SYSCALL__
#define DEBUG_SYSCALL(X, ...) log_fd = fopen("/home/hadoop/CP_log/CP_debug_function.txt","a"); fprintf(log_fd, X, ##__VA_ARGS__); fclose(log_fd);
#else
#define DEBUG_SYSCALL(X, ...)
#endif

// debug syscall thread
#ifdef __DEBUG_SYSTHREAD__
#define DEBUG_SYSTHREAD(X, ...) log_fd = fopen("/home/hadoop/CP_log/CP_debug_systhread.txt","a"); fprintf(log_fd, X, ##__VA_ARGS__); fclose(log_fd);
#else
#define DEBUG_SYSTHREAD(X, ...)
#endif

// debug fdtable management
#ifdef __DEBUG_FDTABLE__
#define DEBUG_FDTABLE(X, ...) log_fd = fopen("/home/hadoop/CP_log/CP_debug_fdtable.txt","a"); fprintf(log_fd, X, ##__VA_ARGS__); fclose(log_fd);
#else
#define DEBUG_FDTABLE(X, ...)
#endif

// debug thread pool for fd table
#ifdef __DEBUG_POOL__
#define DEBUG_POOL(X, ...) log_fd = fopen("/home/hadoop/CP_log/CP_debug_pool.txt","a"); fprintf(log_fd, X, ##__VA_ARGS__); fclose(log_fd);
#else
#define DEBUG_POOL(X, ...)
#endif

