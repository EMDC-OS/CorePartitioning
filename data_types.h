#ifndef __DATA_TYPES_H__
#define __DATA_TYPES_H__

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sched.h>
#include <semaphore.h>

typedef int socklen_t;
typedef struct {
    int fd;
    short events;
    short revents;
} pollfd;
typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;
struct epoll_event {
    uint32_t events;
    epoll_data_t data;
};
typedef struct {
    long tv_sec;
    long tv_usec;
} timeval;
typedef struct {
    long tv_sec;
    long tv_nsec;
} timespec;
struct sockaddr {
    u_short sa_family;
    char sa_data[14];
};
struct in_addr {
    u_long s_addr;
};
struct sockaddr_in {
    short sin_family;
    u_short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};
struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

typedef char cacheline_pad_t; /*1byte padding*/
typedef char cacheline_pad4_t[4]; /*4bytes padding*/
typedef char cacheline_pad8_t[8]; /*8bytes padding*/
typedef char boolean;

struct _socket {
    int domain;
    int type;
    int protocol;
};
struct _bindconnect {
    int socket;
    const struct sockaddr *address;
    socklen_t address_len;
};

struct _listen {
    int sockfd;
    int backlog;
};

struct _acceptget {
    int socket;
    struct sockaddr *addr;
    socklen_t *addrlen;
}; /*getpeername, accept, getsockname*/
/*about connect ?*/
struct _send {
    int socket;
    const void *buffer;
    size_t length;
    int flags;
};
struct _recv {
    int socket;
    void *buf;
    size_t length;
    int flags;
};
struct _setsockopt {
    int socket;
    int level;
    int option_name;
    const void *option_value;
    socklen_t option_len;
};
struct _getsockopt {
    int socket;
    int level;
    int option_name;
    void *buf;
    socklen_t *addrlen;
};
struct _read {
    int fildes;
    void *buf;

    size_t nbyte;
};
struct _write {
    int fildes;
    const void *buf;

    size_t nbyte;
};
struct _sndto {
    int socket;
    const void *buffer;
    size_t length;
    int flags;
    const struct sockaddr *address;
    socklen_t address_len;
}; /*48*/
struct _rcvfrom {
    int socket;
    void *buf;
    size_t length;
    int flags;
    struct sockaddr *addr;
    socklen_t *addrlen;
}; /*48*/

struct _open {
    const char *pathname;
    int flags;
    unsigned int mode;      // - cglee
};
struct _fopen {
    const char *path;
    const char *mode;
};
struct _openat {
    int dirfd;
    const char *pathname;
    int flags;
    unsigned int mode;
};
struct _lseek {
    int fd;
    off_t offset;
    int whence;
};
struct _stat {
    const char *path;
    struct stat *buf;
};
struct _poll {
    pollfd *ufds;
    unsigned int nfds;
    int timeout;
};
struct _ppoll {
    pollfd *ufds;
    unsigned int nfds;
    const struct timespec *timeout_ts;
    const sigset_t *sigmask;
};
struct _epoll_wait {
    int epfd;
    struct epoll_event *events;
    int maxevents;
    int timeout;
};
struct _epoll_ctl {
    int epfd;
    int op;
    int fd;
    struct epoll_event *events;
};
struct _epoll_create {
    int size;
};
struct _socketpair {
    int domain;
    int type;
    int protocol;
    int sv[2];
};
struct _sendmsg {
    int socket;
    const struct msghdr *msg;
    struct msghdr *msg_rcv;
    int flags;
};
struct _select {
    int n;
    fd_set *readfds;
    fd_set *writefds;
    fd_set *exceptfds;
    timeval *stimeout;
};/*40 bytes*/

typedef struct {
    long msgtype;  /*8byte*/
    int request_type; /*8byte*/
    union {
        int fildes; /*for close()*/
        struct _socket socket;
        struct _bindconnect bind;
        struct _bindconnect connect;
        struct _listen listen;
        struct _acceptget accept;
        struct _acceptget getsockname;
        struct _acceptget getpeername;
        struct _setsockopt setsockopt;
        struct _getsockopt getsockopt;
        struct _sndto sendto;
        struct _rcvfrom recvfrom;
        struct _send send;
        struct _recv recv;
        struct _send shutdown;
        struct _read read;
        struct _write write;
        struct _open open;
        struct _poll poll;
        struct _ppoll ppoll;
        struct _epoll_wait epoll_wait;
        struct _epoll_ctl epoll_ctl;
        struct _epoll_create epoll_create;
        struct _socketpair socketpair;
        struct _sendmsg sendmsg;
        struct _sendmsg recvmsg;
        struct _select select;
        struct _lseek lseek;
        struct _stat stat;
        struct _openat openat;
        struct _fopen fopen;
    }; /*48 byte*/
} args_data; /*64 byte */


typedef struct {
    long msgtype;
    int request_type;
    int return_value;
    /**************************** For Send, Recv, Read, Write Function ************************************/
    ssize_t return_size;
    int thr_errno;          // 8byte
    //cacheline_pad8_t pad;         //disable for errno
} retval_data; /*32 byte*/

typedef struct _thread_info {
    pthread_t p_thread;
    int thr_fd;
    void *message;
    int pid;
#ifdef __FILEIO__
    boolean NetorFile; /*network I/O : 0 , file I/O : 1 */
#endif
    sem_t sem_thread; /*time synch*/
    sem_t full, full2; // Consumer, Producer lock on thread
    sem_t empty, empty2; // Consumer, Produce lock on app
    sem_t mutex, mutex2;
    struct _thread_info *next;
    struct _thread_info *prev;
} thread_info;

thread_info *header;
thread_info *tail;
unsigned int thr_num;

thread_info *pool_header;
thread_info *pool_tail;
unsigned int pool_thr_num;

sem_t sem_aq;
sem_t sem_rq;

#endif
