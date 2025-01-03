#ifndef __PRINT_H__
#define __PRINT_H__

#include "data_types.h"

enum REQ_MSG {
    TYPE_OPEN,
    TYPE_FOPEN,
    TYPE_SOCKET,
    TYPE_BIND,
    TYPE_LISTEN,
    TYPE_ACCEPT,
    TYPE_CONNECT,
    TYPE_SEND,
    TYPE_RECV,
    TYPE_SETSOCKOPT,
    TYPE_READ,
    TYPE_WRITE,
    TYPE_POLL,
    TYPE_PPOLL,
    TYPE_EPOLL_WAIT,
    TYPE_EPOLL_CREATE,
    TYPE_EPOLL_CTL,
    TYPE_SOCKETPAIR,
    TYPE_SELECT,
    TYPE_SENDTO,
    TYPE_RECVFROM,
    TYPE_SENDMSG,
    TYPE_RECVMSG,
    TYPE_SHUTDOWN,
    TYPE_GETSOCKNAME,
    TYPE_GETPEERNAME,
    TYPE_GETSOCKOPT,
    TYPE_OPENAT,
    TYPE_OPENAT64,
    TYPE_OPEN64,
    TYPE_LSEEK,
    TYPE_LSEEK64,
    TYPE_STAT,
    TYPE_STAT64,
    TYPE_CLOSE,
};

enum CALLER {
    CALL_BY_FUNCTION,
    CALL_BY_THREAD,
    CALL_BY_FUNCRECV,
    CALL_BY_FUNCSEND,
    CALL_BY_THRRECV,
    CALL_BY_THRSEND,
    CALL_BY_FORKCHILD,
    CALL_BY_FORKPARENT,
};

char *type_iton(int type, char *str);

void print_msg_info(args_data data);

void print_table_info(int caller);

#endif
