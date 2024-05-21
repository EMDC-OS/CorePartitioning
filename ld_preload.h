#define _GNU_SOURCE

#ifndef __LD_PRELOAD_H__
#define __LD_PRELOAD_H__

#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <semaphore.h>

#include "ipc.h"
#include "fdtable.h"
#include "print.h"
#include "data_types.h"

#define QKEY (key_t)0xFFFF
#define MKEY (key_t)0xFF00
#define MAXTHREAD 65535
#define ONE_NODE 10
#define TWO_NODE 20
#define PROC_MAX_LEN 1024
#define INTEL_CPU 20
#define THRESHOLD 50

#define MAX_CPUS 20
#define NUMBER_OF_SOCKETS 2
#define CORES_PER_SOCKET 10

//static void wrap_init(void) __attribute__((constructor));     // constructor
//static void end(void) __attribute__((destructor));            // destructor
void *ku_socket();

//-****** Declare Original Function Variable***************-/
int (*original_socket)(int domain, int type, int protocol);
int (*original_bind)(int socket, const struct sockaddr* address, socklen_t address_len);
int (*original_listen)(int sockfd, int backlog);
int (*original_accept)(int socket, struct sockaddr* addr, socklen_t *addrlen);
int (*original_connect)(int socket, const struct sockaddr* address, socklen_t address_len);
ssize_t (*original_send)(int socket, const void* buffer, size_t length, int flags);
ssize_t (*original_recv)(int socket, void * buf, size_t length, int flags);
int (*original_setsockopt)(int socket, int level, int option_name, const void* option_value, socklen_t option_len);
int (*original_getsockopt)(int socket, int level, int option_name, void* buf, socklen_t *addrlen);
//ssize_t (*original_read)(int fildes, const void * buf, size_t nbyte);
ssize_t (*original_read)(int fildes, void * buf, size_t nbyte);
ssize_t (*original_write)(int fildes, const void * buf, size_t nbyte);
int (*original_close)(int fildes);
int (*original_poll)(pollfd *ufds, unsigned int nfds, int timeout);
int (*original_ppoll)(pollfd *ufds, unsigned int nfds, const struct timespec* timeout_ts, const sigset_t *sigmask);
ssize_t (*original_sendto)(int socket, const void* buffer, size_t length, int flags, const struct sockaddr *address, socklen_t address_len);
ssize_t (*original_sendmsg)(int socket, const struct msghdr *msg, int flags);
ssize_t (*original_recvfrom)(int socket, void * buf, size_t length, int flags, struct sockaddr *addr, socklen_t *addrlen);
ssize_t (*original_recvmsg)(int socket, struct msghdr *msg_rcv, int flags);
int (*original_getsockname)(int socket, struct sockaddr *addr, socklen_t *addrlen);
int (*original_getpeername)(int socket, struct sockaddr *addr, socklen_t *addrlen);
int (*original_shutdown)(int socket, int flags);
int (*original_select)(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, timeval *stimeout);
int (*original_epoll_wait)(int epfd, struct epoll_event *events, int maxevents, int timeout);
int (*original_epoll_ctl)(int epfd, int op, int fd, struct epoll_event* events);
int (*original_epoll_create)(int size);
off_t (*original_openat)(int dirfd, const char *pathname, int flags, mode_t mode);
off_t (*original_lseek)(int fd, off_t offset, int whence);
off_t (*original_stat)(const char *path, struct stat *buf);
off_t (*original_openat64)(int dirfd, const char *pathname, int flags, mode_t mode);
off_t (*original_lseek64)(int fd, off_t offset, int whence);
off_t (*original_stat64)(const char *path, struct stat *buf);
int (*original_open64)(const char *path, int flags, mode_t mode);
int (*original_open)(const char *path, int flags, mode_t mode);
int (*original_fopen)(const char *path, const char* mode);
int (*original_socketpair)(int domain, int type, int protocol, int sv[2]);
pid_t (*original_fork)();

int set_affinity_within(boolean NetorFile);
int get_app_cpu();
void set_cpu(boolean socket, boolean NetorFile);
int set_cpu_app();
//pthread_t select_thread;
int socket0_cpu;
int socket1_cpu;
int file_cpu;
int net_cpu;
int cpu;
int status;
void *syscall_thread();

/* for dynamic system call affinity */
int file_start;
int net_end;
/* for dynamic system call affinity */

thread_info* sock_thread;
thread_info* open_thread;

cpu_set_t mask;

#endif
