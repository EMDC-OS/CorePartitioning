#include "ld_preload.h"
#include <sys/time.h>
#include <signal.h>

#ifndef O_NONBLOCK
#define O_NONBLOCK    0x0004
#define O_ASYNC     0x0040
#define F_GETFL        3    /* get file->f_flags */
#define F_SETFL        4    /* set file->f_flags */

#define DYNAMIC_THREAD_SYSCALLCOUNT    5

#endif

static void wrap_init(void) __attribute__((constructor));     // constructor
static void end(void) __attribute__((destructor));            // destructor


static void wrap_init(void) {
    FILE *log_fd;
    DEBUG_SYSCALL("WRAP_INIT() : wrap_init() start\n")

    if (fdtable_init() == -1) {
        perror("fdtable_init()\n");
        DEBUG_SYSCALL("WRAP_INIT() : fdtable_init() error\n")
        DEBUG_FDTABLE("WRAP_INIT() : fdtable_init() error\n")
        exit(-EINVAL);
    }
#ifdef __POOL__
    if(fdtable_init_pool() == -1){
        exit(-EINVAL);
    }
#endif

    socket0_cpu = 0;
    socket1_cpu = 0;
    file_cpu = 16;
    net_cpu = 13;
    original_select = dlsym(RTLD_NEXT, "select");
    original_socket = dlsym(RTLD_NEXT, "socket");
    original_bind = dlsym(RTLD_NEXT, "bind");
    original_listen = dlsym(RTLD_NEXT, "listen");
    original_accept = dlsym(RTLD_NEXT, "accept");
    original_connect = dlsym(RTLD_NEXT, "connect");
    original_send = dlsym(RTLD_NEXT, "send");
    original_recv = dlsym(RTLD_NEXT, "recv");
    original_setsockopt = dlsym(RTLD_NEXT, "setsockopt");
    original_getsockopt = dlsym(RTLD_NEXT, "getsockopt");
    original_close = dlsym(RTLD_NEXT, "close");
    original_read = dlsym(RTLD_NEXT, "read");
    original_write = dlsym(RTLD_NEXT, "write");
    original_poll = dlsym(RTLD_NEXT, "poll");
    original_ppoll = dlsym(RTLD_NEXT, "ppoll");
    original_sendto = dlsym(RTLD_NEXT, "sendto");
    original_sendmsg = dlsym(RTLD_NEXT, "sendmsg");
    original_recvfrom = dlsym(RTLD_NEXT, "recvfrom");
    original_recvmsg = dlsym(RTLD_NEXT, "recvmsg");
    original_getsockname = dlsym(RTLD_NEXT, "getsockname");
    original_getpeername = dlsym(RTLD_NEXT, "getpeername");
    original_shutdown = dlsym(RTLD_NEXT, "shutdown");
    original_epoll_wait = dlsym(RTLD_NEXT, "epoll_wait");
    original_epoll_ctl = dlsym(RTLD_NEXT, "epoll_ctl");
    original_epoll_create = dlsym(RTLD_NEXT, "epoll_create");
    original_socketpair = dlsym(RTLD_NEXT, "socketpair");
    original_open = dlsym(RTLD_NEXT, "open");
    original_open64 = dlsym(RTLD_NEXT, "open64");
    original_openat = dlsym(RTLD_NEXT, "openat");
    original_lseek = dlsym(RTLD_NEXT, "lseek");
    original_stat = dlsym(RTLD_NEXT, "stat");
    original_openat64 = dlsym(RTLD_NEXT, "openat64");
    original_lseek64 = dlsym(RTLD_NEXT, "lseek64");
    original_stat64 = dlsym(RTLD_NEXT, "stat64");
    original_fopen = dlsym(RTLD_NEXT, "fopen");
    original_fork = dlsym(RTLD_NEXT, "fork");
}

/*
static void end(void){
    thread_info* tmp = header->next ;
    thread_info* tmp2;

    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data* recv_data_p;
    recv_data_p = &recv_data;

    while(tmp!=tail){
        tmp2 = tmp->next;

        send_data.request_type = TYPE_CLOSE;
        send_data.fildes = fildes;

        CPART_send_to_thread(send_data, tmp);
        CPART_recv_from_thread(recv_data_p, tmp);

        pthread_rwlock_wrlock(&table_rwlock);
        pthread_join(tmp->p_thread, (void **)&status);          // do we need to wait for thread?

        prev_node->next = next_node;
        next_node->prev = prev_node;
        tmp ->next == NULL;
        tmp ->prev == NULL;

        sem_destroy(&(tmp->sem_thread));
        sem_destroy(&(tmp->empty));
        sem_destroy(&(tmp->empty2));
        sem_destroy(&(tmp->full));
        sem_destroy(&(tmp->full2));
        memset(tmp->message, 0, sizeof(args_data));      // memset - cglee
        free(tmp->message);
        memset(tmp, 0, sizeof(args_data));              // memset - cglee
        free(tmp);
        thr_num--;

        pthread_rwlock_unlock(&table_rwlock);

        tmp = tmp2;
    }

    free(header);
    free(tail);

    pthread_rwlock_destroy(&table_rwlock);
}
*/

long getMicrotime() {                    // for timestamp
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int) 1e6 + currentTime.tv_usec;
}

void *syscall_thread() {
    args_data send_data;
    retval_data recv_data;
    FILE *log_fd;

    int loop_break = 0;         // for delete -1 table - cglee
    int old_errno = 0;
    int flag;                   // for NIO->blocking IO
    thread_info *sock;          /*thread info per socket*/
    int ret;

    int syscall_counter = 0; //for dynamic thread affinity
    int syscall_thread_counter = 0; // for statistic
    DEBUG_SYSTHREAD("SYSCALL_THREAD() : syscall thread created\n")

    char *str;
    /******************************** Allocation CPU ****************************************/
#ifdef __ONE__
    cpu = 1;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif

#ifdef __RR__
    if(ONE_NODE <= cpu)
        cpu = 2;
    else
        cpu++;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif

#ifdef __RR2__
    if(TWO_NODE <= cpu)
        cpu = ONE_NODE+1;
    else
        cpu++;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif
#ifdef __DYNAMIC__
    set_cpu(1, 0);
#endif

#ifdef __WITHIN__
    cpu = set_affinity_within(0);
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif

//    sem_wait(&sem_fdtable);           // disable lock - cgl //disable for rwlock
/*
    DEBUG_SYSTHREAD("SYSCALL_THREAD() : table_rwlock lock try\n")
    pthread_rwlock_rdlock(&table_rwlock);
    */
    sock = fdtable_get_by_tid(pthread_self());
    /*
    DEBUG_SYSTHREAD("SYSCALL_THREAD() : got table lock, fdtable_get_by_tid() sock : %ul, self = %ul\n",sock, pthread_self())
    pthread_rwlock_unlock(&table_rwlock);
     */
    if (sock == NULL) {
        DEBUG_SYSTHREAD("SYSCALL_THREAD() : fdtable_get_by_tid() return NULL, syscall thread exit, error\n")

        return NULL;
    }
    send_data.request_type = -1;
#ifdef __FILEIO__
    set_cpu(1, sock->NetorFile);
#endif
//    sem_post(&sem_fdtable);           // disable lock - cgl //disable for rwlock
    if (sock->pid == getpid()) {
//        sem_post(&(sock->sem_thread));
        while (send_data.request_type != TYPE_CLOSE && send_data.request_type != TYPE_SHUTDOWN) {
            DEBUG_SYSTHREAD("SYSCALL_THREAD() : CPART_recv_from_app\n")
            send_data = CPART_recv_from_app(send_data, sock);
            recv_data.request_type = send_data.request_type;
            recv_data.thr_errno = 0;

            syscall_counter++;
#ifdef __DEBUG_SYSCALL__
            syscall_thread_counter++;
#endif
            /* call set_cpu() for dynamic thread affinity */
            if (syscall_counter >= DYNAMIC_THREAD_SYSCALLCOUNT) {
                DEBUG_SYSTHREAD("SYSCALL_THREAD() : dynamic syscall affinity activated, counter = %d\n",
                                syscall_thread_counter)
                syscall_counter = 0;
                set_cpu(1, sock->NetorFile);
            }

            switch (send_data.request_type) {

                case TYPE_SOCKET:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_socket)(send_data.socket.domain, send_data.socket.type,
                                                                send_data.socket.protocol);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;

                case TYPE_BIND:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_bind)(send_data.bind.socket, send_data.bind.address,
                                                              send_data.bind.address_len);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;

                case TYPE_LISTEN:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_listen)(send_data.listen.sockfd, send_data.listen.backlog);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;

                case TYPE_ACCEPT:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_accept)(send_data.accept.socket, send_data.accept.addr,
                                                                send_data.accept.addrlen);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_CONNECT:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_connect)(send_data.connect.socket, send_data.connect.address,
                                                                 send_data.connect.address_len);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_SEND:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_size = (*original_send)(send_data.send.socket, send_data.send.buffer,
                                                             send_data.send.length, send_data.send.flags);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_RECV:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_size = (*original_recv)(send_data.recv.socket, send_data.recv.buf,
                                                             send_data.recv.length, send_data.recv.flags);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_SETSOCKOPT:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_setsockopt)(send_data.setsockopt.socket,
                                                                    send_data.setsockopt.level,
                                                                    send_data.setsockopt.option_name,
                                                                    send_data.setsockopt.option_value,
                                                                    send_data.setsockopt.option_len);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_GETSOCKOPT:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_getsockopt)(send_data.getsockopt.socket,
                                                                    send_data.getsockopt.level,
                                                                    send_data.getsockopt.option_name,
                                                                    send_data.getsockopt.buf,
                                                                    send_data.getsockopt.addrlen);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_READ:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_size = (*original_read)(send_data.read.fildes, send_data.read.buf,
                                                             send_data.read.nbyte);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_WRITE:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_size = (*original_write)(send_data.write.fildes, send_data.write.buf,
                                                              send_data.write.nbyte);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;

                case TYPE_CLOSE:
                    //old_errno = errno;
                    //errno = 0;
                    //recv_data.return_value = (*original_close)(send_data.fildes);
                    //if(errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    //else errno = old_errno;                         // errno not changed

                    DEBUG_SYSTHREAD("SYSCALL_THREAD(close) : fd : %d  syscall_function_count : %d\n", sock->thr_fd,
                                    syscall_thread_counter)
                    break;

                case TYPE_POLL:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_poll)(send_data.poll.ufds, send_data.poll.nfds,
                                                              send_data.poll.timeout);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_PPOLL:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_ppoll)(send_data.ppoll.ufds, send_data.ppoll.nfds,
                                                               send_data.ppoll.timeout_ts, send_data.ppoll.sigmask);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_EPOLL_WAIT:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_epoll_wait)(send_data.epoll_wait.epfd,
                                                                    send_data.epoll_wait.events,
                                                                    send_data.epoll_wait.maxevents,
                                                                    send_data.epoll_wait.timeout);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_EPOLL_CREATE:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_epoll_create)(send_data.epoll_create.size);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_EPOLL_CTL:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_epoll_ctl)(send_data.epoll_ctl.epfd, send_data.epoll_ctl.op,
                                                                   send_data.epoll_ctl.fd, send_data.epoll_ctl.events);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_SOCKETPAIR:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_socketpair)(send_data.socketpair.domain,
                                                                    send_data.socketpair.type,
                                                                    send_data.socketpair.protocol,
                                                                    send_data.socketpair.sv);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_SENDTO:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_size = (*original_sendto)(send_data.sendto.socket, send_data.sendto.buffer,
                                                               send_data.sendto.length, send_data.sendto.flags,
                                                               send_data.sendto.address, send_data.sendto.address_len);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_RECVFROM:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_size = (*original_recvfrom)(send_data.recvfrom.socket, send_data.recvfrom.buf,
                                                                 send_data.recvfrom.length, send_data.recvfrom.flags,
                                                                 send_data.recvfrom.addr, send_data.recvfrom.addrlen);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_SENDMSG:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_size = (*original_sendmsg)(send_data.sendmsg.socket, send_data.sendmsg.msg,
                                                                send_data.sendmsg.flags);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_RECVMSG:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_size = (*original_recvmsg)(send_data.recvmsg.socket, send_data.recvmsg.msg_rcv,
                                                                send_data.recvmsg.flags);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_SHUTDOWN:
//                    old_errno = errno;
//                    errno = 0;
//                    recv_data.return_size = (*original_shutdown)(send_data.shutdown.socket, send_data.shutdown.flags);
//                    if(errno != 0) recv_data.thr_errno = errno;     // set changed errno
//                    else errno = old_errno;                         // errno not changed
                    DEBUG_SYSTHREAD("SYSCALL_THREAD (shutdown) : fd : %d  syscall_function_count : %d\n", sock->thr_fd,
                                    syscall_thread_counter)

                    break;
                case TYPE_GETSOCKNAME:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_getsockname)(send_data.getsockname.socket,
                                                                     send_data.getsockname.addr,
                                                                     send_data.getsockname.addrlen);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_GETPEERNAME:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_getpeername)(send_data.getpeername.socket,
                                                                     send_data.getpeername.addr,
                                                                     send_data.getpeername.addrlen);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_OPEN:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_open)(send_data.open.pathname, send_data.open.flags,
                                                              send_data.open.mode);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_OPENAT:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_openat)(send_data.openat.dirfd, send_data.openat.pathname,
                                                                send_data.openat.flags, send_data.openat.mode);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    if (recv_data.return_value == -1) loop_break = 1;  // for break when return -1 - cglee
                    break;
                case TYPE_LSEEK:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_lseek)(send_data.lseek.fd, send_data.lseek.offset,
                                                               send_data.lseek.whence);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_LSEEK64:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_lseek64)(send_data.lseek.fd, send_data.lseek.offset,
                                                                 send_data.lseek.whence);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_STAT:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_stat)(send_data.stat.path, send_data.stat.buf);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                case TYPE_STAT64:
                    old_errno = errno;
                    errno = 0;
                    recv_data.return_value = (*original_stat64)(send_data.stat.path, send_data.stat.buf);
                    if (errno != 0) recv_data.thr_errno = errno;     // set changed errno
                    else errno = old_errno;                         // errno not changed
                    break;
                default:
                    break;
            }
            DEBUG_SYSTHREAD("SYSCALL_THREAD() : CPART_send_to_app try\n")
            CPART_send_to_app(recv_data, sock);
            DEBUG_SYSTHREAD("SYSCALL_THREAD() : CPART_send_to_app success\n")

            if (loop_break == 1) {        // for break when return -1 - cgl
                DEBUG_SYSTHREAD("SYSCALL_THREAD : fd : %d  syscall_function_count : %d\n", sock->thr_fd,
                                syscall_thread_counter)
                return NULL;
            }
        }//End of while

        DEBUG_SYSTHREAD("SYSCALL_THREAD : fd : %d  syscall_function_count : %d\n", sock->thr_fd, syscall_thread_counter)
    } else {
//        sem_post(&(sock->sem_thread));
        DEBUG_SYSTHREAD("SYSCALL_THREAD : end-> there is no appropriate tid in table, error\n")
    }

    return NULL;
}


void *ku_select() {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    thread_info *sel; /*thread per select*/


    //-******************************** Allocation CPU ****************************************-/
#ifdef __ONE__
    cpu = 1;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif

#ifdef __RR__
    if(ONE_NODE <= cpu)
        cpu = 1;
    else
        cpu++;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif

#ifdef __RR2__
    if(TWO_NODE <= cpu)
        cpu = ONE_NODE+1;
    else
        cpu++;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif
//    sem_wait(&sem_fdtable); //disable for rwlock
    pthread_rwlock_rdlock(&table_rwlock);
    sel = fdtable_get_by_tid(pthread_self());
//    sem_post(&sem_fdtable); //disable for rwlock
    pthread_rwlock_unlock(&table_rwlock);
#ifdef __FILEIO__
    set_cpu(1, sel->NetorFile);
#endif
    while (1) {
        send_data = CPART_recv_from_app(send_data, sel);
        recv_data.request_type = send_data.request_type;
        recv_data.return_value = (*original_select)(send_data.select.n, send_data.select.readfds,
                                                    send_data.select.writefds, send_data.select.exceptfds,
                                                    send_data.select.stimeout);
        CPART_send_to_app(recv_data, sel);
    }
}


int close(int fildes) {
    thread_info *tmp;
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;
    int func_errno = errno;
    int delete_ret;
    FILE *log_fd;

    DEBUG_SYSCALL("CLOSE(%d) function start\n", fildes)

    DEBUG_SYSCALL("CLOSE(%d) : TABLE_RWLOCK try rd lock\n", fildes)
    pthread_rwlock_wrlock(&table_rwlock);
    DEBUG_SYSCALL("CLOSE(%d) : TABLE_RWLOCK lock success\n", fildes)

#ifdef __FILEIO__
    tmp = fdtable_get_by_fd_all(fildes);
#else
    tmp = fdtable_get_by_fd(fildes);
#endif


    if (tmp == NULL) {
        DEBUG_SYSCALL("CLOSE(%d) : TABLE_RW try unlock\n", fildes)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("CLOSE(%d) : TABLE_RW unlock success\n", fildes)

        DEBUG_SYSCALL("CLOSE(%d) : fdtable_get_by_fd return NULL, there is no table entry with fd : %d\n", fildes,
                      fildes)
        errno = func_errno;
        return (*original_close)(fildes);
    } else {           // if there is appropriate entry in FDtable
        DEBUG_SYSCALL("CLOSE(%d) : fdtable_get_by_fd return success\n", fildes)
        send_data.request_type = TYPE_CLOSE;
        send_data.fildes = fildes;

        DEBUG_SYSCALL("CLOSE(%d) : send msg to syscall thread - try\n", fildes)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("CLOSE(%d) : send msg to syscall thread - success\n", fildes)

        DEBUG_SYSCALL("CLOSE(%d) : recv msg from syscall thread - try\n", fildes)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("CLOSE(%d) : recv msg from syscall thread - success\n", fildes)

        DEBUG_SYSCALL("CLOSE(%d) : pthread_join() - try\n", fildes)
        pthread_join(tmp->p_thread, (void **) &status);          // do we need to wait for thread?
        DEBUG_SYSCALL("CLOSE(%d) : pthread_join() - success\n", fildes)

        DEBUG_SYSCALL("CLOSE(%d) : fdtable_delete() - try\n", fildes)
        delete_ret = fdtable_delete(fildes);
        if (delete_ret == 0) {
            DEBUG_SYSCALL("CLOSE(%d) : fdtable_delete() - success\n", fildes)
        } else if (delete_ret == 1) {
            DEBUG_SYSCALL("CLOSE(%d) : fdtable_delete() - target's next or prev pointer is NULL, return 1 error\n",
                          fildes)
        } else if (delete_ret == 2) {
            DEBUG_SYSCALL("CLOSE(%d) : fdtable_delete() - can't find table entry with fd, return 2 error\n", fildes)
        }

        DEBUG_SYSCALL("CLOSE(%d) : TABLE_RWLOCK try unlock\n", fildes)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("CLOSE(%d) : TABLE_RWLOCK unlock success\n", fildes)

        DEBUG_SYSCALL("CLOSE(%d) : return %d\n", fildes)

/*
#ifndef __POOL__
        pthread_rwlock_wrlock(&table_rwlock);
        fdtable_entry_delete(fildes);
        pthread_rwlock_unlock(&table_rwlock);
#endif
#ifdef __POOL__
        pthread_rwlock_wrlock(&table_rwlock);
        fdtable_to_pool(fildes);
        pthread_rwlock_unlock(&table_rwlock);
#endif
*/
        return (*original_close)(fildes);
    }
}

/*old*/

int socket(int domain, int type, int protocol) {
    int thr_id;
    int num = 1;
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;
    thread_info *tmp;
    int ret = 0;
    int flag = 0;
    int thread_flag = 0; // if pthread_create needed then thread_flag = 1
    int func_errno = errno;
    FILE *log_fd;
    original_socket = dlsym(RTLD_NEXT, "socket");

#ifdef APP
    set_cpu_app();
#endif
    //errno = 0;
    DEBUG_SYSCALL("SOCKET() : function start %d\n")

    ret = (*original_socket)(domain, type, protocol);
    func_errno = errno;

    DEBUG_SYSCALL("SOCKET(%d) : TABLE_RWLOCK try lock\n", ret)
    pthread_rwlock_wrlock(&table_rwlock);
    DEBUG_SYSCALL("SOCKET(%d) : TABLE_RWLOCK lock success\n", ret)
    if (fdtable_get_by_fd_all(ret) != NULL || ret < 0) {
        DEBUG_SYSCALL("SOCKET(%d) : FD already in table or minus descriptor\n", ret)
        DEBUG_SYSCALL("SOCKET(%d) : TABLE_RWLOCK try unlock\n", ret)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SOCKET(%d) : TABLE_RWLOCK unlock success\n", ret)
        errno = func_errno;
        return ret;
    } else if (fdtable_getnumber() < MAXTHREAD) {
#ifndef __POOL__
        DEBUG_SYSCALL("SOCKET(%d) : try fdtable_add()\n", ret)
        tmp = fdtable_add();
        DEBUG_SYSCALL("SOCKET(%d) : fdtable_add return = %ul\n", ret, tmp)
        thread_flag = 1;
#endif
#ifdef __POOL__
        DEBUG_SYSCALL("SOCKET(%d) : try fdtable_from_pool()\n",ret)
        tmp = fdtable_from_pool();
        DEBUG_SYSCALL("SOCKET(%d) : fdtable_from_pool() return = %ul\n",ret, tmp)
            if(tmp == NULL){
                DEBUG_SYSCALL("SOCKET(%d) : fdtable_from_pool() return NULL, fdtable_add() try\n",ret)
                tmp = fdtable_add();
                DEBUG_SYSCALL("SOCKET(%d) : fdtable_from_pool() return NULL, fdtable_add() return = %ul\n",ret,tmp)
                thread_flag = 1;
            }
#endif

        if (thread_flag == 1) {
            thr_id = pthread_create(&(tmp->p_thread), NULL, syscall_thread, NULL);
            if (thr_id) {  // if pthread_create fails
                perror("pthread_create() error");
                DEBUG_FDTABLE("SOCKET(%d) : pthread_create() error\n", ret)
            }
        }

        tmp->NetorFile = 0;
        tmp->pid = getpid();
        tmp->thr_fd = ret;

        DEBUG_SYSCALL("SOCKET(%d) : TABLE_RWLOCK try unlock\n", ret)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SOCKET(%d) : TABLE_RWLOCK unlock success\n", ret)
    } else {
        DEBUG_SYSCALL("SOCKET(%d) : max thread exceed\n", ret)
        DEBUG_SYSCALL("SOCKET(%d) : TABLE_RWLOCK try unlock\n", ret)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SOCKET(%d) : TABLE_RWLOCK unlock success\n", ret)
        errno = func_errno;
        return ret;
    }
    errno = func_errno;
    return ret;
}

#ifdef __FILEIO__
/*old*/

int open(const char *path, int flags, mode_t mode)
{
    int thr_id;
    int num = 1;
    thread_info *tmp;
    int ret = 0;
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data* recv_data_p;
    recv_data_p = &recv_data;
    int func_errno = errno;
    int thread_flag = 0;
    FILE* log_fd;

    DEBUG_SYSCALL("OPEN() : fucntion start\n")

#ifdef APP
    set_cpu_app();
#endif
    errno = 0;
    ret = (*original_open)(path, flags, mode);

    if(errno != 0) func_errno = errno;
    else errno = func_errno;
    if(ret < 3) return ret;

    DEBUG_SYSCALL("OPEN(%d) : TABLE_RWLOCK try lock\n",ret)
    pthread_rwlock_wrlock(&table_rwlock);
    DEBUG_SYSCALL("OPEN(%d) : TABLE_RWLOCK lock success\n",ret)
    if(fdtable_get_by_fd_all(ret) != NULL || ret < 0){
        DEBUG_SYSCALL("OPEN(%d) : FD already in table or minus descriptor\n",ret)
        DEBUG_SYSCALL("OPEN(%d) : TABLE_RWLOCK try unlock\n",ret)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("OPEN(%d) : TABLE_RWLOCK unlock success\n",ret)
        errno = func_errno;
        return ret;
    }
    else if(fdtable_getnumber() < MAXTHREAD){
#ifndef __POOL__
        DEBUG_SYSCALL("OPEN(%d) : try fdtable_add()\n",ret)
        tmp = fdtable_add();
        DEBUG_SYSCALL("OPEN(%d) : fdtable_add return = %ul\n",ret, tmp)
        thread_flag = 1;
#endif
#ifdef __POOL__
        DEBUG_SYSCALL("OPEN(%d) : try fdtable_from_pool()\n",ret)
        tmp = fdtable_from_pool();
        DEBUG_SYSCALL("OPEN(%d) : fdtable_from_pool() return = %ul\n",ret, tmp)
            if(tmp == NULL){
                DEBUG_SYSCALL("OPEN(%d) : fdtable_from_pool() return NULL, fdtable_add() try\n",ret)
                tmp = fdtable_add();
                DEBUG_SYSCALL("OPEN(%d) : fdtable_from_pool() return NULL, fdtable_add() return = %ul\n",ret,tmp)
                thread_flag = 1;
            }
#endif

#ifdef __FILEIO__
        tmp->NetorFile = 1;
#endif
        tmp->pid = getpid();
        tmp->thr_fd = ret;

        if(thread_flag == 1){
            thr_id = pthread_create(&(tmp->p_thread), NULL, syscall_thread, NULL);
            if(thr_id){  // if pthread_create fails
                DEBUG_SYSCALL("OPEN(%d) : pthread_create() error\n",ret)
                perror("pthread_create error");
            }
        }

        DEBUG_SYSCALL("OPEN(%d) : TABLE_RWLOCK try unlock\n",ret)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("OPEN(%d) : TABLE_RWLOCK unlock success\n",ret)
    }
    else{
        DEBUG_SYSCALL("OPEN(%d) : max thread exceed\n",ret)
        DEBUG_SYSCALL("OPEN(%d) : TABLE_RWLOCK try unlock\n",ret)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("OPEN(%d) : TABLE_RWLOCK unlock success\n",ret)
        errno = func_errno;
        return ret;
    }
    errno = func_errno;
    return ret;
}

off_t openat(int dirfd, const char *pathname, int flags, mode_t mode){
    int thr_id;
    int num = 1;
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data* recv_data_p;
    recv_data_p = &recv_data;

    int ret = 0;
    int func_errno = errno;
    int thread_flag = 0;
    thread_info *tmp;
    FILE* log_fd;

    DEBUG_SYSCALL("OPENAT() : fucntion start\n")

    errno = 0;
    ret = (*original_openat)(dirfd, pathname, flags, mode);
    if(errno != 0) func_errno = errno;
    else errno = func_errno;
    func_errno = errno;

    DEBUG_SYSCALL("OPENAT(%d) : TABLE_RWLOCK try lock\n",ret)
    pthread_rwlock_wrlock(&table_rwlock);
    DEBUG_SYSCALL("OPENAT(%d) : TABLE_RWLOCK lock success\n",ret)
    if(fdtable_get_by_fd_all(ret) != NULL || ret < 0){
        DEBUG_SYSCALL("OPENAT(%d) : FD already in table or minus descriptor\n",ret)
        DEBUG_SYSCALL("OPENAT(%d) : TABLE_RWLOCK try unlock\n",ret)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("OPENAT(%d) : TABLE_RWLOCK unlock success\n",ret)
        errno = func_errno;
        return ret;
    }
    else if(ret >0 && (fdtable_getnumber() < MAXTHREAD)){
#ifndef __POOL__
        DEBUG_SYSCALL("OPENAT(%d) : try fdtable_add()\n",ret)
        tmp = fdtable_add();
        DEBUG_SYSCALL("OPENAT(%d) : fdtable_add return = %ul\n",ret, tmp)
        thread_flag = 1;
#endif
#ifdef __POOL__
        DEBUG_SYSCALL("OPENAT(%d) : try fdtable_from_pool()\n",ret)
        tmp = fdtable_from_pool();
        DEBUG_SYSCALL("OPENAT(%d) : fdtable_from_pool() return = %ul\n",ret, tmp)
            if(tmp == NULL){
                DEBUG_SYSCALL("OPENAT(%d) : fdtable_from_pool() return NULL, fdtable_add() try\n",ret)
                tmp = fdtable_add();
                DEBUG_SYSCALL("OPENAT(%d) : fdtable_from_pool() return NULL, fdtable_add() return = %ul\n",ret,tmp)
                thread_flag = 1;
            }
#endif

#ifdef __FILEIO__
        tmp->NetorFile = 1;
#endif
        tmp->pid = getpid();
        tmp->thr_fd = ret;

        if(thread_flag == 1){
            thr_id = pthread_create(&(tmp->p_thread), NULL, syscall_thread, NULL);
            if(thr_id){  // if pthread_create fails
                DEBUG_SYSCALL("OPENAT(%d) : pthread_create() error\n",ret)
                perror("pthread_create error");
            }
        }

        DEBUG_SYSCALL("OPENAT(%d) : TABLE_RWLOCK try unlock\n",ret)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("OPENAT(%d) : TABLE_RWLOCK unlock success\n",ret)
    }
    else{
        DEBUG_SYSCALL("OPENAT(%d) : max thread exceed\n",ret)
        DEBUG_SYSCALL("OPENAT(%d) : TABLE_RWLOCK try unlock\n",ret)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("OPENAT(%d) : TABLE_RWLOCK unlock success\n",ret)
        errno = func_errno;
        return ret;
    }
    errno = func_errno;
    return ret;
}

off_t lseek(int fd, off_t offset, int whence){
        thread_info* tmp;
        args_data send_data = {1, 0};
        retval_data recv_data = {1, 0, 0};
        retval_data* recv_data_p;
        recv_data_p = &recv_data;
        int func_errno = errno;
        FILE* log_fd;

        DEBUG_SYSCALL("LSEEK(%d) : fucntion start\n",fd)
        DEBUG_SYSCALL("LSEEK(%d) : TABLE_RWLOCK try lock\n",fd)
        pthread_rwlock_rdlock(&table_rwlock);
        DEBUG_SYSCALL("LSEEK(%d) : TABLE_RWLOCK lock success\n",fd)
        tmp = fdtable_get_by_fd(fd, 1);

        if(tmp == NULL)
        {
            DEBUG_SYSCALL("LSEEK(%d) : TABLE_RWLOCK try unlock\n",fd)
            pthread_rwlock_unlock(&table_rwlock);
            DEBUG_SYSCALL("LSEEK(%d) : TABLE_RWLOCK unlock success\n",fd)

            DEBUG_SYSCALL("LSEEK(%d) : fd not in FD table, call original system call\n",fd)
            errno = func_errno;
            return (*original_lseek)(fd, offset, whence);
        }
        else
        {
            send_data.request_type = TYPE_LSEEK;
            send_data.lseek.fd = fd;
            send_data.lseek.offset = offset;
            send_data.lseek.whence = whence;

            DEBUG_SYSCALL("LSEEK(%d) : send msg to syscall thread - try\n",fd)
            CPART_send_to_thread(send_data, tmp);
            DEBUG_SYSCALL("LSEEK(%d) : send msg to syscall thread - success\n",fd)

            DEBUG_SYSCALL("LSEEK(%d) : recv msg from syscall thread - try\n",fd)
            CPART_recv_from_thread(recv_data_p, tmp);
            DEBUG_SYSCALL("LSEEK(%d) : recv msg from syscall thread - success\n",fd)

            DEBUG_SYSCALL("LSEEK(%d) : TABLE_RWLOCK try unlock\n",fd)
            pthread_rwlock_unlock(&table_rwlock);
            DEBUG_SYSCALL("LSEEK(%d) : TABLE_RWLOCK unlock success\n",fd)

            if(recv_data_p->thr_errno != 0) errno = recv_data_p->thr_errno;
            else errno = func_errno;
            return recv_data.return_value;
        }
}


void *stat_syscall()
{
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};

    thread_info *stat; /*thread per stat*/

    //-******************************** Allocation CPU ****************************************-/
#ifdef __ONE__
    cpu = 1;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif
#ifdef __RR__
    if(ONE_NODE <= cpu)
        cpu = 1;
    else
        cpu++;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif

#ifdef __RR2__
    if(TWO_NODE <= cpu)
        cpu = ONE_NODE+1;
    else
        cpu++;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &mask);
#endif

//	sem_wait(&sem_fdtable); //disable for rwlock
    pthread_rwlock_rdlock(&table_rwlock);
    stat = fdtable_get_by_tid(pthread_self());
//	sem_post(&sem_fdtable); //disable for rwlock
    pthread_rwlock_unlock(&table_rwlock);

#ifdef __FILEIO__
        set_cpu(1, stat->NetorFile);
#endif


    send_data = CPART_recv_from_app(send_data, stat);
    recv_data.request_type = send_data.request_type;
    recv_data.return_value = (*original_stat)(send_data.stat.path, send_data.stat.buf);
    CPART_send_to_app(recv_data, stat);
    errno = recv_data.thr_errno;

    return NULL;
}

ssize_t read(int fildes, void *buf, size_t nbyte)
{
    thread_info* tmp;
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data* recv_data_p;
    recv_data_p = &recv_data;
    int func_errno = errno;
    FILE* log_fd;

    DEBUG_SYSCALL("READ(%d) : fucntion start\n",fildes)
    DEBUG_SYSCALL("READ(%d) : TABLE_RWLOCK try lock\n",fildes)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("READ(%d) : TABLE_RWLOCK lock success\n",fildes)
    tmp = fdtable_get_by_fd(fildes, 1);


    if(tmp == NULL)
    {
        DEBUG_SYSCALL("READ(%d) : TABLE_RWLOCK try unlock\n",fildes)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("READ(%d) : TABLE_RWLOCK unlock success\n",fildes)

        DEBUG_SYSCALL("READ(%d) : fd not in FD table, call original system call\n",fildes)
        errno = func_errno;
        return (*original_read)(fildes, buf, nbyte);
    }
    else
    {
        send_data.request_type = TYPE_READ;
        send_data.read.fildes = fildes;
        send_data.read.buf = buf;
        send_data.read.nbyte = nbyte;


        DEBUG_SYSCALL("READ(%d) : send msg to syscall thread - try\n",fildes)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("READ(%d) : send msg to syscall thread - success\n",fildes)

        DEBUG_SYSCALL("READ(%d) : recv msg from syscall thread - try\n",fildes)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("READ(%d) : recv msg from syscall thread - success\n",fildes)

        DEBUG_SYSCALL("READ(%d) : TABLE_RWLOCK try unlock\n",fildes)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("READ(%d) : TABLE_RWLOCK unlock success\n",fildes)

        if(recv_data_p->thr_errno != 0) errno = recv_data_p->thr_errno;
        else errno = func_errno;
        return recv_data.return_size;
    }
}

ssize_t write(int fildes, const void *buf, size_t nbyte)
{
    thread_info* tmp;
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data* recv_data_p;
    recv_data_p = &recv_data;
    int func_errno = errno;
    FILE* log_fd;

    DEBUG_SYSCALL("WRITE(%d) : fucntion start\n",fildes)
    DEBUG_SYSCALL("WRITE(%d) : TABLE_RWLOCK try lock\n",fildes)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("WRITE(%d) : TABLE_RWLOCK lock success\n",fildes)
    tmp = fdtable_get_by_fd(fildes, 1);


    if(tmp == NULL)
    {
        DEBUG_SYSCALL("WRITE(%d) : TABLE_RWLOCK try unlock\n",fildes)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("WRITE(%d) : TABLE_RWLOCK unlock success\n",fildes)

        DEBUG_SYSCALL("WRITE(%d) : fd not in FD table, call original system call\n",fildes)
        errno = func_errno;
        return (*original_write)(fildes, buf, nbyte);
    }
    else
    {
        send_data.request_type = TYPE_WRITE;
        send_data.write.fildes = fildes;
        send_data.write.buf = buf;
        send_data.write.nbyte = nbyte;


        DEBUG_SYSCALL("WRITE(%d) : send msg to syscall thread - try\n",fildes)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("WRITE(%d) : send msg to syscall thread - success\n",fildes)

        DEBUG_SYSCALL("WRITE(%d) : recv msg from syscall thread - try\n",fildes)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("WRITE(%d) : recv msg from syscall thread - success\n",fildes)

        DEBUG_SYSCALL("WRITE(%d) : TABLE_RWLOCK try unlock\n",fildes)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("WRITE(%d) : TABLE_RWLOCK unlock success\n",fildes)

        errno = func_errno;
        return recv_data.return_size;
    }
}
#endif


int bind(int socket, const struct sockaddr *address, socklen_t address_len) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    thread_info *tmp;
    recv_data_p = &recv_data;
    int func_errno = errno;
    FILE *log_fd;

    DEBUG_SYSCALL("BIND(%d) : fucntion start\n", socket)
    DEBUG_SYSCALL("BIND(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("BIND(%d) : TABLE_RWLOCK lock success\n", socket)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif


    if (tmp == NULL) {
        DEBUG_SYSCALL("BIND(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("BIND(%d) : TABLE_RWLOCK unlock success\n", socket)

        DEBUG_SYSCALL("BIND(%d) : fd not in FD table, call original system call\n", socket)
        errno = func_errno;
        return (*original_bind)(socket, address, address_len);
    } else {
        send_data.request_type = TYPE_BIND;
        send_data.bind.socket = socket;
        send_data.bind.address = address;
        send_data.bind.address_len = address_len;

        DEBUG_SYSCALL("BIND(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("BIND(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("BIND(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("BIND(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("BIND(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("BIND(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_value;
    }
}

int listen(int sockfd, int backlog) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;

    recv_data_p = &recv_data;
    FILE *log_fd;
    int func_errno = errno;
    thread_info *tmp;

    DEBUG_SYSCALL("LISTEN(%d) : function start\n", sockfd)
    DEBUG_SYSCALL("LISTEN(%d) : TABLE_RWLOCK try lock\n", sockfd)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("LISTEN(%d) : TABLE_RWLOCK lock success\n", sockfd)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(sockfd, 0);
#else
    tmp = fdtable_get_by_fd(sockfd);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("LISTEN(%d) : TABLE_RWLOCK try unlock\n", sockfd)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("LISTEN(%d) : TABLE_RWLOCK unlock success\n", sockfd)

        DEBUG_SYSCALL("LISTEN(%d) : fd not in FD table, call original system call\n", sockfd)
        errno = func_errno;
        return (*original_listen)(sockfd, backlog);
    } else {
        send_data.request_type = TYPE_LISTEN;
        send_data.listen.sockfd = sockfd;
        send_data.listen.backlog = backlog;

        DEBUG_SYSCALL("LISTEN(%d) : send msg to syscall thread - try\n", sockfd)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("LISTEN(%d) : send msg to syscall thread - success\n", sockfd)

        DEBUG_SYSCALL("LISTEN(%d) : recv msg from syscall thread - try\n", sockfd)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("LISTEN(%d) : recv msg from syscall thread - success\n", sockfd)

        DEBUG_SYSCALL("LISTEN(%d) : TABLE_RWLOCK try unlock\n", sockfd)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("LISTEN(%d) : TABLE_RWLOCK unlock success\n", sockfd)

        errno = recv_data_p->thr_errno;
        return recv_data.return_value;
    }
}


int accept(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p = &recv_data;
    int thr_id;
    int num = 1;
    thread_info *tmp;
    thread_info *tmp2;
    int func_errno = 0;
    int ret = 0;
    FILE *log_fd;
    //int thread_flag = 0;

    DEBUG_SYSCALL("ACCEPT(%d) : function start\n", socket)
    DEBUG_SYSCALL("ACCEPT(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("ACCEPT(%d) : TABLE_RWLOCK lock success\n", socket)
    tmp = fdtable_get_by_fd(socket, 0);


    if (tmp == NULL) {        // no table for parameter socket
        DEBUG_SYSCALL("ACCEPT(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("ACCEPT(%d) : TABLE_RWLOCK unlock success\n", socket)

        DEBUG_SYSCALL("ACCEPT(%d) : fd not in FD table, call original system call\n", socket)
        errno = func_errno;
        return (*original_accept)(socket, addr, addrlen);
    } else {                   // parameter socket has fd table entry
        send_data.request_type = TYPE_ACCEPT;
        send_data.accept.socket = socket;
        send_data.accept.addr = addr;
        send_data.accept.addrlen = addrlen;

        DEBUG_SYSCALL("ACCEPT(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("ACCEPT(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("ACCEPT(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("ACCEPT(%d) : recv msg from syscall thread - success\n", socket)

        if (recv_data_p->return_value > 0 &&
            fdtable_get_by_fd_all(recv_data_p->return_value) == NULL) {              // when accept() success
            tmp2 = fdtable_add();

            tmp2->NetorFile = 0;
            tmp->pid = getpid();
            tmp2->thr_fd = recv_data_p->return_value;

            thr_id = pthread_create(&(tmp2->p_thread), NULL, syscall_thread, NULL);
            if (thr_id < 0) {
                DEBUG_SYSCALL("LISTEN(%d) : tmp2 thread create error\n", socket)
                perror("pthread_create error");
            }
            DEBUG_SYSCALL("ACCEPT(%d) : TABLE_RWLOCK try unlock\n", socket)
            pthread_rwlock_unlock(&table_rwlock);
            DEBUG_SYSCALL("ACCEPT(%d) : TABLE_RWLOCK unlock success\n", socket)

            errno = recv_data_p->thr_errno;
            return recv_data_p->return_value;
        } else {                                           // when accept() failed
            DEBUG_SYSCALL("ACCEPT(%d) : TABLE_RWLOCK try unlock\n", socket)
            pthread_rwlock_unlock(&table_rwlock);
            DEBUG_SYSCALL("ACCEPT(%d) : TABLE_RWLOCK unlock success\n", socket)

            DEBUG_SYSCALL("ACCEPT(%d) : Accept return %d<0 or fd alread exist in table\n", socket,
                          recv_data_p->return_value)
            errno = recv_data_p->thr_errno;
            return recv_data_p->return_value;
        }
    }
}


int connect(int socket, const struct sockaddr *address, socklen_t address_len) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    struct sockaddr_in *sin = (struct sockaddr_in *) address;

    recv_data_p = &recv_data;
    int func_errno = errno;
    FILE *log_fd;
    thread_info *tmp;

    DEBUG_SYSCALL("CONNECT(%d) : function start\n", socket)
    DEBUG_SYSCALL("CONNECT(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("CONNECT(%d) : TABLE_RWLOCK lock success\n", socket)

#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("CONNECT(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("CONNECT(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = func_errno;
        return (*original_connect)(socket, address, address_len);
    } else {
        send_data.request_type = TYPE_CONNECT;
        send_data.connect.socket = socket;
        send_data.connect.address = address;
        send_data.connect.address_len = address_len;

        DEBUG_SYSCALL("CONNECT(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("CONNECT(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("CONNECT(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("CONNECT(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("CONNECT(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("CONNECT(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_value;
    }
}


ssize_t send(int socket, const void *buffer, size_t length, int flags) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;

    recv_data_p = &recv_data;

    thread_info *tmp;
    int func_errno = errno;
    FILE *log_fd;

    DEBUG_SYSCALL("SEND(%d) : function start\n", socket)
    DEBUG_SYSCALL("SEND(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("SEND(%d) : TABLE_RWLOCK lock success\n", socket)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif


    if (tmp == NULL) {
        DEBUG_SYSCALL("SEND(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SEND(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = func_errno;
        return (*original_send)(socket, buffer, length, flags);
    } else {
        send_data.request_type = TYPE_SEND;
        send_data.send.socket = socket;
        send_data.send.buffer = buffer;
        send_data.send.length = length;
        send_data.send.flags = flags;

        DEBUG_SYSCALL("SEND(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("SEND(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("SEND(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("SEND(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("SEND(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SEND(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_size;
    }

}


ssize_t recv(int socket, void *buf, size_t length, int flags) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;

    recv_data_p = &recv_data;
    int func_errno = errno;
    FILE *log_fd;
    thread_info *tmp;

    DEBUG_SYSCALL("RECV(%d) : function start\n", socket)
    DEBUG_SYSCALL("RECV(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("RECV(%d) : TABLE_RWLOCK lock success\n", socket)

#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("RECV(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("RECV(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = func_errno;
        return (*original_recv)(socket, buf, length, flags);
    } else {
        send_data.request_type = TYPE_RECV;
        send_data.recv.socket = socket;
        send_data.recv.buf = buf;
        send_data.recv.length = length;
        send_data.recv.flags = flags;

        DEBUG_SYSCALL("RECV(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("RECV(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("RECV(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("RECV(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("RECV(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("RECV(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_size;
    }
}

int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    int func_errno = errno;
    recv_data_p = &recv_data;
    thread_info *tmp;
    FILE *log_fd;

    DEBUG_SYSCALL("SETSOCKOPT(%d) : function start\n", socket)
    DEBUG_SYSCALL("SETSOCKOPT(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("SETSOCKOPT(%d) : TABLE_RWLOCK lock success\n", socket)

#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("SETSOCKOPT(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SETSOCKOPT(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = func_errno;
        return (*original_setsockopt)(socket, level, option_name, option_value, option_len);
    } else {
        send_data.request_type = TYPE_SETSOCKOPT;
        send_data.setsockopt.socket = socket;
        send_data.setsockopt.level = level;
        send_data.setsockopt.option_name = option_name;
        send_data.setsockopt.option_value = option_value;
        send_data.setsockopt.option_len = option_len;

        DEBUG_SYSCALL("SETSOCKOPT(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("SETSOCKOPT(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("SETSOCKOPT(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("SETSOCKOPT(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("SETSOCKOPT(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SETSOCKOPT(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_value;
    }
}

int getsockopt(int socket, int level, int option_name, void *buf, socklen_t *addrlen) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    thread_info *tmp;
    recv_data_p = &recv_data;

    int func_errno = errno;
    FILE *log_fd;

    DEBUG_SYSCALL("GETSOCKOPT(%d) : function start\n", socket)
    DEBUG_SYSCALL("GETSOCKOPT(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("GETSOCKOPT(%d) : TABLE_RWLOCK lock success\n", socket)

#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("GETSOCKOPT(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("GETSOCKOPT(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = func_errno;
        return (*original_getsockopt)(socket, level, option_name, buf, addrlen);
    } else {

        send_data.request_type = TYPE_GETSOCKOPT;
        send_data.getsockopt.socket = socket;
        send_data.getsockopt.level = level;
        send_data.getsockopt.option_name = option_name;
        send_data.getsockopt.buf = buf;
        send_data.getsockopt.addrlen = addrlen;


        DEBUG_SYSCALL("GETSOCKOPT(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("GETSOCKOPT(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("GETSOCKOPT(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("GETSOCKOPT(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("GETSOCKOPT(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("GETSOCKOPT(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_value;
    }
}


int epoll_create(int size) {
    int thr_id;
    int num = 1;
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;
    int ret;
    int func_errno = errno;
    thread_info *tmp;
    int thread_flag = 0;
    FILE *log_fd;
    errno = 0;

    DEBUG_SYSCALL("EPOLL_CREATE(%d) : function start\n", socket)
    ret = (*original_epoll_create)(size);
    if (errno != 0) {
        func_errno = errno;
    } else {
        errno = func_errno;
    }
    DEBUG_SYSCALL("EPOLL_CREATE(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("EPOLL_CREATE(%d) : TABLE_RWLOCK lock success\n", socket)

    if (fdtable_get_by_fd_all(ret) != NULL || ret < 0) {
        DEBUG_SYSCALL("EPOLL_CREATE(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_rdlock(&table_rwlock);
        DEBUG_SYSCALL("EPOLL_CREATE(%d) : TABLE_RWLOCK unlock success\n", socket)
        errno = func_errno;
        return ret;
    } else if (fdtable_getnumber() < MAXTHREAD) {
#ifndef __POOL__
        DEBUG_SYSCALL("EPOLL_CREATE() : try fdtable_add()\n")
        tmp = fdtable_add();
        DEBUG_SYSCALL("EPOLL_CREATE() : fdtable_add return = %ul\n", tmp)
        thread_flag = 1;
#endif
#ifdef __POOL__
        DEBUG_SYSCALL("EPOLL_CREATE() : try fdtable_from_pool()\n")
        tmp = fdtable_from_pool();
        DEBUG_SYSCALL("EPOLL_CREATE() : fdtable_from_pool() return = %ul\n", tmp)
        if(tmp == NULL){
            DEBUG_SYSCALL("EPOLL_CREATE() : fdtable_from_pool() return NULL, fdtable_add() try\n")
            tmp = fdtable_add();
            DEBUG_SYSCALL("EPOLL_CREATE() : fdtable_from_pool() return NULL, fdtable_add() return = %ul\n", tmp)
            thread_flag = 1;
        }
#endif
        tmp->pid = getpid();
        tmp->NetorFile = 0;
        tmp->thr_fd = ret;
        if (thread_flag == 1) {
            thr_id = pthread_create(&(tmp->p_thread), NULL, syscall_thread, NULL);
            if (thr_id) {  // if pthread_create fails
                DEBUG_SYSCALL("EPOLL_CREATE() : pthread_create() error\n", ret)
                perror("pthread_create error");
            }
        }

        if (thr_id < 0) {
            DEBUG_SYSCALL("EPOLL_CREATE() : pthread_create() error\n")
            perror("pthread_create error");
        }
        DEBUG_SYSCALL("EPOLL_CREATE() : TABLE_RWLOCK try unlock\n")
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("EPOLL_CREATE() : TABLE_RWLOCK unlock success\n")
    } else {
        DEBUG_SYSCALL("EPOLL_CREATE() : TABLE_RWLOCK try unlock\n")
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("EPOLL_CREATE() : TABLE_RWLOCK unlock success\n")
        errno = func_errno;
        return ret;
    }

    errno = func_errno;
    return ret;
}


int epoll_ctl(int epfd, int op, int fd, struct epoll_event *events) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;

    thread_info *tmp;
    int func_errno = errno;
    FILE *log_fd;

    DEBUG_SYSCALL("EPOLL_CTL(%d) : function start\n", epfd)
    DEBUG_SYSCALL("EPOLL_CTL(%d) : TABLE_RWLOCK try lock\n", epfd)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("EPOLL_CTL(%d) : TABLE_RWLOCK lock success\n", epfd)

#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(epfd, 0);
#else
    tmp = fdtable_get_by_fd(epfd);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("EPOLL_CTL(%d) : TABLE_RWLOCK try unlock\n", epfd)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("EPOLL_CTL(%d) : TABLE_RWLOCK unlock success\n", epfd)

        errno = func_errno;
        return (*original_epoll_ctl)(epfd, op, fd, events);
    } else {
        send_data.request_type = TYPE_EPOLL_CTL;
        send_data.epoll_ctl.epfd = epfd;
        send_data.epoll_ctl.op = op;
        send_data.epoll_ctl.fd = fd;
        send_data.epoll_ctl.events = events;

        CPART_send_to_thread(send_data, tmp);
        CPART_recv_from_thread(recv_data_p, tmp);

        DEBUG_SYSCALL("EPOLL_CTL(%d) : TABLE_RWLOCK try unlock\n", epfd)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("EPOLL_CTL(%d) : TABLE_RWLOCK unlock success\n", epfd)

        if (recv_data_p->thr_errno != 0) errno = recv_data_p->thr_errno;
        else errno = func_errno;
        return recv_data.return_value;
    }
}

int poll(pollfd *ufds, unsigned int nfds, int timeout) {
    int (*original_poll)(pollfd *ufds, unsigned int nfds, int timeout);
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;

    recv_data_p = &recv_data;

#ifdef  __DEBUG_SYSCALL__
    FILE* log_fd;
    log_fd = fopen("/home/hadoop/syscall_counter_function.txt","a+");
    fprintf(log_fd, "poll()");
    fclose(log_fd);
#endif
    original_poll = dlsym(RTLD_NEXT, "poll");
    thread_info *tmp;
    int func_errno = errno;
//    sem_wait(&sem_fdtable); //disable for rwlock
    pthread_rwlock_rdlock(&table_rwlock);
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(ufds[nfds-1].fd, 0);
#else
    tmp = fdtable_get_by_fd(ufds[nfds - 1].fd);
#endif
//    sem_post(&sem_fdtable); //disable for rwlock

    if (tmp == NULL) {
        pthread_rwlock_unlock(&table_rwlock);
        errno = func_errno;
        return (*original_poll)(ufds, nfds, timeout);
    } else {
        send_data.request_type = TYPE_POLL;
        send_data.poll.ufds = ufds;
        send_data.poll.nfds = nfds;
        send_data.poll.timeout = timeout;

        CPART_send_to_thread(send_data, tmp);
        CPART_recv_from_thread(recv_data_p, tmp);

        pthread_rwlock_unlock(&table_rwlock);

        if (recv_data_p->thr_errno != 0) errno = recv_data_p->thr_errno;
        else errno = func_errno;
        return recv_data.return_value;
    }
}


ssize_t sendto(int socket, const void *buffer, size_t length, int flags, const struct sockaddr *address,
               socklen_t address_len) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;

    int func_errno = errno;
    thread_info *tmp;
    FILE *log_fd;

    DEBUG_SYSCALL("SENDTO(%d) : function start\n", socket)
    DEBUG_SYSCALL("SENDTO(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("SENDTO(%d) : TABLE_RWLOCK lock success\n", socket)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif


    if (tmp == NULL) {
        DEBUG_SYSCALL("SENDTO(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SENDTO(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = func_errno;
        return (*original_sendto)(socket, buffer, length, flags, address, address_len);
    } else {
        send_data.request_type = TYPE_SENDTO;
        send_data.sendto.socket = socket;
        send_data.sendto.buffer = buffer;
        send_data.sendto.length = length;
        send_data.sendto.flags = flags;
        send_data.sendto.address = address;
        send_data.sendto.address_len = address_len;


        DEBUG_SYSCALL("SENDTO(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("SENDTO(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("SENDTO(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("SENDTO(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("SENDTO(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SENDTO(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_size;
    }
}

ssize_t sendmsg(int socket, const struct msghdr *msg, int flags) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;
    FILE *log_fd;
    int func_errno = errno;
    thread_info *tmp;

    DEBUG_SYSCALL("SENDMSG(%d) : function start\n", socket)
    DEBUG_SYSCALL("SENDMSG(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("SENDMSG(%d) : TABLE_RWLOCK lock success\n", socket)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("SENDMSG(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SENDMSG(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = 0;
        return (*original_sendmsg)(socket, msg, flags);
    } else {
        send_data.request_type = TYPE_SENDMSG;
        send_data.sendmsg.socket = socket;
        send_data.sendmsg.msg = msg;
        send_data.sendmsg.flags = flags;


        DEBUG_SYSCALL("SENDMSG(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("SENDMSG(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("SENDMSG(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("SENDMSG(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("SENDMSG(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SENDMSG(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_size;
    }
}

ssize_t recvfrom(int socket, void *buf, size_t length, int flags, struct sockaddr *addr, socklen_t *addrlen) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;
    int func_errno = errno;
    thread_info *tmp;
    FILE *log_fd;

    DEBUG_SYSCALL("RECVFROM(%d) : function start\n", socket)
    DEBUG_SYSCALL("RECVFROM(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("RECVFROM(%d) : TABLE_RWLOCK lock success\n", socket)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("RECVFROM(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("RECVFROM(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = func_errno;
        return (*original_recvfrom)(socket, buf, length, flags, addr, addrlen);
    } else {
        send_data.request_type = TYPE_RECVFROM;
        send_data.recvfrom.socket = socket;
        send_data.recvfrom.buf = buf;
        send_data.recvfrom.length = length;
        send_data.recvfrom.flags = flags;
        send_data.recvfrom.addr = addr;
        send_data.recvfrom.addrlen = addrlen;

        DEBUG_SYSCALL("RECVFROM(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("RECVFROM(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("RECVFROM(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("RECVFROM(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("RECVFROM(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("RECVFROM(%d) : TABLE_RWLOCK unlock success\n", socket)

        if (recv_data_p->thr_errno != 0) errno = recv_data_p->thr_errno;
        else errno = func_errno;
        return recv_data.return_size;
    }
}

ssize_t recvmsg(int socket, struct msghdr *msg_rcv, int flags) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;
    FILE *log_fd;
    thread_info *tmp;
    int func_errno = errno;

    DEBUG_SYSCALL("RECVMSG(%d) : function start\n", socket)
    DEBUG_SYSCALL("RECVMSG(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("RECVMSG(%d) : TABLE_RWLOCK lock success\n", socket)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("RECVMSG(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("RECVMSG(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = func_errno;
        return (*original_recvmsg)(socket, msg_rcv, flags);
    } else {
        send_data.request_type = TYPE_RECVMSG;
        send_data.recvmsg.socket = socket;
        send_data.recvmsg.msg_rcv = msg_rcv;
        send_data.recvmsg.flags = flags;

        DEBUG_SYSCALL("RECVMSG(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("RECVMSG(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("RECVMSG(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("RECVMSG(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("RECVMSG(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("RECVMSG(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_size;
    }
}

int getsockname(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;
    FILE *log_fd;
    thread_info *tmp;

    int func_errno = errno;

    DEBUG_SYSCALL("GETSOCKNAME(%d) : function start\n", socket)
    DEBUG_SYSCALL("GETSOCKNAME(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("GETSOCKNAME(%d) : TABLE_RWLOCK lock success\n", socket)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("GETSOCKNAME(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("GETSOCKNAME(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = func_errno;
        return (*original_getsockname)(socket, addr, addrlen);
    } else {
        send_data.request_type = TYPE_GETSOCKNAME;
        send_data.getsockname.socket = socket;
        send_data.getsockname.addr = addr;
        send_data.getsockname.addrlen = addrlen;

        DEBUG_SYSCALL("GETSOCKNAME(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("GETSOCKNAME(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("GETSOCKNAME(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("GETSOCKNAME(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("GETSOCKNAME(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("GETSOCKNAME(%d) : TABLE_RWLOCK unlock success\n", socket)

        errno = recv_data_p->thr_errno;
        return recv_data.return_value;
    }
}

int getpeername(int socket, struct sockaddr *addr, socklen_t *addrlen) {
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;
    thread_info *tmp;
    FILE *log_fd;

    int func_errno = errno;

    DEBUG_SYSCALL("GETPEERNAME(%d) : function start\n", socket)
    DEBUG_SYSCALL("GETPEERNAME(%d) : TABLE_RWLOCK try lock\n", socket)
    pthread_rwlock_rdlock(&table_rwlock);
    DEBUG_SYSCALL("GETPEERNAME(%d) : TABLE_RWLOCK lock success\n", socket)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd(socket, 0);
#else
    tmp = fdtable_get_by_fd(socket);
#endif

    if (tmp == NULL) {
        DEBUG_SYSCALL("GETPEERNAME(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("GETPEERNAME(%d) : TABLE_RWLOCK unlock success\n", socket)

        return (*original_getpeername)(socket, addr, addrlen);
    } else {
        send_data.request_type = TYPE_GETPEERNAME;
        send_data.getpeername.socket = socket;
        send_data.getpeername.addr = addr;
        send_data.getpeername.addrlen = addrlen;


        DEBUG_SYSCALL("GETPEERNAME(%d) : send msg to syscall thread - try\n", socket)
        CPART_send_to_thread(send_data, tmp);
        DEBUG_SYSCALL("GETPEERNAME(%d) : send msg to syscall thread - success\n", socket)

        DEBUG_SYSCALL("GETPEERNAME(%d) : recv msg from syscall thread - try\n", socket)
        CPART_recv_from_thread(recv_data_p, tmp);
        DEBUG_SYSCALL("GETPEERNAME(%d) : recv msg from syscall thread - success\n", socket)

        DEBUG_SYSCALL("GETPEERNAME(%d) : TABLE_RWLOCK try unlock\n", socket)
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("GETPEERNAME(%d) : TABLE_RWLOCK unlock success\n", socket)

        if (recv_data_p->thr_errno != 0) errno = recv_data_p->thr_errno;
        else errno = func_errno;
        return recv_data.return_value;
    }
}


int shutdown(int socket, int flags) {
    thread_info *tmp;
    args_data send_data = {1, 0};
    retval_data recv_data = {1, 0, 0};
    retval_data *recv_data_p;
    recv_data_p = &recv_data;
    int func_errno = errno;
    FILE *log_fd;

    DEBUG_SYSCALL("SHUTDOWN(%d) function start\n", socket)

    DEBUG_SYSCALL("SHUTDOWN(%d) : TABLE_RWLOCK try rd lock\n", socket)
    pthread_rwlock_wrlock(&table_rwlock);
    DEBUG_SYSCALL("SHUTDOWN(%d) : TABLE_RWLOCK lock success\n", socket)
#ifdef __FILEIO__
    tmp = fdtable_get_by_fd_all(socket);
#else
    tmp = fdtable_get_by_fd(socket);
#endif


    if (tmp == NULL) {
        pthread_rwlock_unlock(&table_rwlock);
        DEBUG_SYSCALL("SHUTDOWN(%d) : fdtable_get_by_fd return NULL, there is no table entry with fd : %d\n", socket,
                      socket)
        return (*original_shutdown)(socket, flags);
    } else {           // if there is appropriate entry in FDtable
        DEBUG_SYSCALL("SHUTDOWN(%d) : fdtable_get_by_fd return success\n", socket)
        send_data.request_type = TYPE_SHUTDOWN;
        send_data.send.socket = socket;
        send_data.send.flags = flags;

        CPART_send_to_thread(send_data, tmp);
        CPART_recv_from_thread(recv_data_p, tmp);

        pthread_join(tmp->p_thread, (void **) &status);          // do we need to wait for thread?

#ifndef __POOL__
        fdtable_delete(socket);
#endif
#ifdef __POOL__
        fdtable_to_pool(socket);
#endif
        pthread_rwlock_unlock(&table_rwlock);

        return (*original_shutdown)(socket, flags);
    }
}


int clone(int (*fn)(void *arg), void *child_stack, int flags, void *arg, ...) {
    int return_pid_t;
    int (*original_clone)(int (*fn)(void *arg), void *child_stack, int flags, void *arg);
    FILE *log_fd;

    DEBUG_SYSCALL("CLONE() function start")
    original_clone = dlsym(RTLD_NEXT, "clone");

    return_pid_t = (*original_clone)(fn, child_stack, flags, arg);

    switch (return_pid_t) {
        case 0:
            //pthread_rwlock_init(&table_rwlock, NULL);
            //fdtable_init();
#ifdef __POOL__
            fdtable_init_pool();
#endif
            break;
        case -1:
            perror("error fork\n");
            break;
        default:
            break;
    }
    return return_pid_t;
}


pid_t fork() {
    pid_t return_pid_t;
    FILE *log_fd;

    DEBUG_SYSCALL("FORK() function start")
    return_pid_t = (*original_fork)();

    switch (return_pid_t) {
        case 0:
//            pthread_rwlock_init(&table_rwlock, NULL);
//            fdtable_init();
#ifdef __POOL__
            fdtable_init_pool();
#endif
            //sem_init(&table_rwlock,1,1);
            fdtable_forked(syscall_thread, ku_select);
            break;
        case -1:
            perror("error fork\n");
            break;
        default:
            break;
    }
    return return_pid_t;
}
