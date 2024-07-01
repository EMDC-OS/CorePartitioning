#include "fdtable.h"


int fdtable_init() {
/*initialize file descriptor table*/
    FILE *log_fd;
    thr_num = 0;
    DEBUG_FDTABLE("FDTABLE_INIT() : FD table init start\n")
    if (pthread_rwlock_init(&table_rwlock, NULL)) {     //add for rwlock
        perror("Forwarding Semaphore Init Error\n");
        DEBUG_SYSCALL("FDTABLE_INIT() : pthread_rwlock_init() error\n")
        exit(-1);
    }
    header = (thread_info *) calloc(1, sizeof(thread_info));
    if (header == NULL) {
        DEBUG_FDTABLE("FDTABLE_INIT() : header entry calloc() error\n")
        exit(-1);
    }
    tail = (thread_info *) calloc(1, sizeof(thread_info));

    if (tail == NULL) {
        DEBUG_FDTABLE("FDTABLE_INIT() : tail entry calloc() error\n")
        free(header);
        exit(-1);
    }

    header->next = tail;
    header->prev = tail;
    tail->next = NULL;
    tail->prev = header;
    header->thr_fd = -10;       // header entry fd = -10
    tail->thr_fd = -11;         // tail entry fd = -11
    DEBUG_FDTABLE("FDTABLE_INIT() : FD table initiation done with return 0\n")
    return 0;                   // return correctly code = 0
}

void fdtable_destroy() {     // need to be fixed
/*clean up all the memory*/
    FILE *log_fd;
    DEBUG_FDTABLE("FDTABLE_DESTROY() : FD table destory start\n")
    thread_info *tmp = header;
    while (tmp != NULL) {
        thread_info *obj = tmp;
        tmp = tmp->next;
        free(obj);
    }
    DEBUG_FDTABLE("FDTABLE_DESTROY() : FD table destory end\n")
}

thread_info *fdtable_add() {
/*add given thread info to fdtable*/
    FILE *log_fd;
    DEBUG_FDTABLE("FDTABLE_ADD() : FD table entry add start\n")
    thread_info *new = (thread_info *) calloc(1, sizeof(thread_info));
    thread_info *next_node = header->next;
    if (new == NULL) {
        DEBUG_FDTABLE("FDTABLE_ADD() : FD table entry end with calloc() error\n")
        return NULL;
    } else {
        new->thr_fd = -12;           // new entry's init fd = -12
        new->pid = getpid();
        if (-1 == sem_init(&(new->sem_thread), 1, 1)) {
            perror("Current Thread Semaphore Init Error\n");
            DEBUG_FDTABLE("FDTABLE_ADD() : thread semaphore sem_init() error\n")
//			exit(-EINVAL);
            exit(-1);
        }
        sem_init(&(new->mutex), 1, 1);
        sem_init(&(new->mutex2), 1, 1);
        sem_init(&(new->empty), 1, 1);
        sem_init(&(new->full), 1, 0);
        sem_init(&(new->empty2), 1, 1);
        sem_init(&(new->full2), 1, 0);
        new->message = malloc(sizeof(args_data));
        memset(new->message, 0, sizeof(args_data));   // memset - cglee

        new->prev = header;
        new->next = next_node;
        header->next = new;
        next_node->prev = new;
        thr_num++;
        DEBUG_FDTABLE("FDTABLE_ADD() : FD table entry add end\n")
        return new;
    }
}

thread_info *fdtable_get_by_fd_all(int fd) {
    thread_info *tmp = header->next;
    FILE *log_fd;
    DEBUG_FDTABLE("FDTABLE_GET_BY_FD_ALL(%d) : function start\n", fd)

    while (tmp->thr_fd != -11 && tmp->thr_fd != NULL) {    // thr fd is not header or tail
        if (tmp->thr_fd == fd) {
            DEBUG_FDTABLE("FDTABLE_GET_BY_FD_ALL(%d) : end -> return %ul\n", fd, tmp->p_thread)
            return tmp;
        } else tmp = tmp->next;
    }
    DEBUG_FDTABLE("FDTABLE_GET_BY_FD_ALL(%d) : end -> return NULL\n", fd)
    return NULL;
}

#ifdef __FILEIO__
thread_info* fdtable_get_by_fd(int fd, boolean NetorFile){
/*returns thread that matches given fd */
    thread_info *tmp = header->next ;
    FILE* log_fd;
    DEBUG_FDTABLE("FDTABLE_GET_BY_FD_ALL(%d) : function start\n", fd)
    while(tmp->thr_fd != -11 && tmp->thr_fd != NULL){    // until tmp == tail
        if(tmp->thr_fd == fd && tmp->NetorFile == NetorFile){
            DEBUG_FDTABLE("FDTABLE_GET_BY_FD_ALL(%d) : end -> return %ul\n",fd, tmp->p_thread)
            return tmp;
        }
        else tmp = tmp->next;
    }
    DEBUG_FDTABLE("FDTABLE_GET_BY_FD_ALL(%d) : end -> return NULL\n",fd)
    return NULL ;
}

#else

thread_info *fdtable_get_by_fd(int fd) {
/*returns thread that matches given fd */
    FILE *log_fd;
    thread_info *tmp = header->next;
    DEBUG_FDTABLE("FDTABLE_GET_BY_FD(%d) : function start\n", fd)
    while (tmp->thr_fd != -11 && tmp->thr_fd != NULL) {    // until tmp == tail
        if (tmp->thr_fd == fd) {
            DEBUG_FDTABLE("FDTABLE_GET_BY_FD(%d) : end -> return %ul\n", fd, tmp->p_thread)
            return tmp;
        } else tmp = tmp->next;
    }
    DEBUG_FDTABLE("FDTABLE_GET_BY_FD_ALL(%d) : end -> return NULL\n", fd)
    return NULL;
}

#endif

thread_info *fdtable_get_by_tid(pthread_t pthread) {
    thread_info *tmp = header->next;
    FILE *log_fd;
    DEBUG_FDTABLE("FDTABLE_GET_BY_TID(%ul) : funtion start\n", pthread)
    while (tmp->thr_fd != -11 && tmp->thr_fd != NULL) {    // thr fd is not header or tail
        if (tmp->p_thread == pthread) {
            DEBUG_FDTABLE("FDTABLE_GET_BY_TID(%ul) : end -> return %ul\n", pthread, tmp->p_thread)
            return tmp;
        }
        tmp = tmp->next;
    }
    DEBUG_FDTABLE("FDTABLE_GET_BY_TID(%ul) : end -> return NULL\n", pthread)
    return NULL;
}

int fdtable_delete(int fd) {
/*delete thread with given fd from fdtable*/
    thread_info *tmp = fdtable_get_by_fd_all(fd);
    FILE *log_fd;
    DEBUG_FDTABLE("FDTABLE_DELETE(%d) : fdtable_delete() start, counter = %d\n", fd, thr_num)
    if (tmp != NULL) {
        thread_info *next_node = tmp->next;
        thread_info *prev_node = tmp->prev;

        if (next_node != NULL && prev_node != NULL) {
            prev_node->next = next_node;
            next_node->prev = prev_node;
            tmp->next = NULL;
            tmp->prev = NULL;

            sem_destroy(&(tmp->sem_thread));
            sem_destroy(&(tmp->empty));
            sem_destroy(&(tmp->empty2));
            sem_destroy(&(tmp->full));
            sem_destroy(&(tmp->full2));
            sem_destroy(&(tmp->mutex));
            sem_destroy(&(tmp->mutex2));
            free(tmp->message);
            free(tmp);
            thr_num--;
            DEBUG_FDTABLE("FDTABLE_DELETE(%d) : fdtable_delete() end return 0, counter = %d\n", fd, thr_num)
            return 0;
        } else {
            DEBUG_FDTABLE("FDTABLE_DELETE(%d) : target's next or prev pointer is NULL return 1 error\n", fd)
            return 1;
        }
    } else {
        DEBUG_FDTABLE("FDTABLE_DELETE(%d) : can't find table entry with fd, tmp = NULL return 2 error\n", fd)
        return 2;
    }
}

int fdtable_entry_delete(int fd) {
    FILE *log_fd;
    DEBUG_FDTABLE("FDTABLE_ENTRY_DELETE(%d) : fdtable_entry_delete() start, counter = %d\n", fd, thr_num)
    thread_info *tmp = fdtable_get_by_fd_all(fd);

    if (tmp != NULL) {
        thread_info *next_node = tmp->next;
        thread_info *prev_node = tmp->prev;

        if (next_node != NULL && prev_node != NULL) {
            prev_node->next = next_node;
            next_node->prev = prev_node;
            tmp->next = NULL;
            tmp->prev = NULL;
            thr_num--;
            DEBUG_FDTABLE("FDTABLE_ENTRY_DELETE(%d) : fdtable_entry_delete() end with return 0, counter = %d\n", fd,
                          thr_num)
        } else {
            DEBUG_FDTABLE("FDTABLE_ENTRY_DELETE(%d) : can't find table entry with fd, tmp = NULL return -1\n", fd)
            return 2;
        }
    }
    return 0;
}

void fdtable_traversal() {
    FILE *log_fd;                                        // changyu-lee : for log
    DEBUG_FDTABLE("FDTABLE_TRAVERSAL() : fdtable_traversal() start\n")
    thread_info *tmp = header->next;
    DEBUG_FDTABLE("FDTABLE_TRAVERSAL() : ------------------------fd table---------------------\n")
    while (tail != tmp) {
        DEBUG_FDTABLE("FDTABLE_TRAVERSAL() : fd :: %d   tid :: %lu\n", tmp->thr_fd, tmp->p_thread)
        tmp = tmp->next;
    }
    DEBUG_FDTABLE("FDTABLE_TRAVERSAL() : ------------------------end table---------------------\n")
    DEBUG_FDTABLE("FDTABLE_TRAVERSAL() : fdtable_traversal() end\n")

    return 0;
}

void fdtable_traversal_reverse() {
    FILE *log_fd;                                        // changyu-lee : for log
    DEBUG_FDTABLE("FDTABLE_TRAVERSAL_REVERSE() : fdtable_traversal_reverse() start\n")
    thread_info *tmp = tail->prev;
    DEBUG_FDTABLE("FDTABLE_TRAVERSAL_REVERSE() : ------------------------fd table---------------------\n")
    while (header != tmp) {
        DEBUG_FDTABLE("FDTABLE_TRAVERSAL() : fd :: %d   tid :: %lu\n", tmp->thr_fd, tmp->p_thread)
        tmp = tmp->prev;
    }
    DEBUG_FDTABLE("FDTABLE_TRAVERSAL_REVERSE() : ------------------------end table---------------------\n")
    DEBUG_FDTABLE("FDTABLE_TRAVERSAL_REVERSE() : fdtable_traversal_reverse() end\n")

    return 0;
}


void fdtable_forked(void *function, void *function2) {
    /*called when process forked or cloned*/
    FILE *log_fd;
    pthread_t thr_id;
    header->pid = getpid();
    thread_info *tmp = header->next;
    DEBUG_FDTABLE("FDTABLE_FORKED() : fdtable_forked() start\n")
    /*reproduce thread, and ipc key value*/
    while (tmp != tail && tmp->thr_fd != -10) {
        sem_init(&(tmp->sem_thread), 1, 1);
        if (tmp->thr_fd == -10000) {
            tmp->pid = getpid();
            thr_id = pthread_create(&(tmp->p_thread), NULL, function2, NULL);
            tmp->message = malloc(sizeof(args_data));
            sem_init(&(tmp->empty), 1, 1);
            sem_init(&(tmp->full), 1, 0);
            sem_init(&(tmp->empty2), 1, 1);
            sem_init(&(tmp->full2), 1, 0);
            sem_init(&(tmp->mutex), 1, 1);
            sem_init(&(tmp->mutex2), 1, 1);
        } else {
            tmp->pid = getpid();
            thr_id = pthread_create(&(tmp->p_thread), NULL, function, NULL);
            if (thr_id) {  // if pthread_create fails
                DEBUG_FDTABLE("FDTABLE_FORKED() : pthread_create() error\n")
            }
            tmp->message = malloc(sizeof(args_data));
            sem_init(&(tmp->empty), 1, 1);
            sem_init(&(tmp->full), 1, 0);
            sem_init(&(tmp->empty2), 1, 1);
            sem_init(&(tmp->full2), 1, 0);
            sem_init(&(tmp->mutex), 1, 1);
            sem_init(&(tmp->mutex2), 1, 1);
        }
        tmp = tmp->next;
    }
#ifdef __POOL__
    tmp = pool_header->next;
    while(tmp != pool_tail){
        sem_init(&(tmp->sem_thread), 1, 1);
        if (tmp->thr_fd == -10000) {
            tmp->pid = getpid();
            thr_id = pthread_create(&(tmp->p_thread), NULL, function2, NULL);
            tmp->message = malloc(sizeof(args_data));
            sem_init(&(new->mutex), 1, 1);
            sem_init(&(new->mutex2), 1, 1);
            sem_init(&(new->empty), 1, 1);
            sem_init(&(new->full), 1, 0);
            sem_init(&(new->empty2), 1, 1);
            sem_init(&(new->full2), 1, 0);
            new->message = malloc(sizeof(args_data));
            memset(new->message, 0, sizeof(args_data));
        } else {
            tmp->pid = getpid();
            thr_id = pthread_create(&(tmp->p_thread), NULL, function, NULL);
            tmp->message = malloc(sizeof(args_data));
            sem_init(&(new->mutex), 1, 1);
            sem_init(&(new->mutex2), 1, 1);
            sem_init(&(new->empty), 1, 1);
            sem_init(&(new->full), 1, 0);
            sem_init(&(new->empty2), 1, 1);
            sem_init(&(new->full2), 1, 0);
        }
        tmp = tmp->next;
    }
#endif

    DEBUG_FDTABLE("FDTABLE_FORKED() : fdtable_forked() end\n")
}

int fdtable_getnumber() {
    FILE *log_fd;

    DEBUG_FDTABLE("FDTABLE_GETNUMBER() : fdtable_getnumber() return %d\n", thr_num)
    return thr_num;
}

int fdtable_isEmpty() {
    if (thr_num > 0) return 0;
    else return 1;
}

#ifdef __POOL__
int fdtable_init_pool(){
/*initialize file descriptor table*/
    pool_thr_num = 0;

    if(pthread_rwlock_init(&pool_rwlock;, NULL)){     //add for rwlock
        perror("Forwarding Semaphore Init Error\n");
        DEBUG_POOL("FDTABLE_INIT_POOL() : pthread_rwlock_init() error\n")
        exit(-EINVAL);
    }

    pool_header = (thread_info*)calloc(1,sizeof(thread_info));
    if(pool_header == NULL) return -1 ;
    pool_tail = (thread_info*)calloc(1,sizeof(thread_info));

    if(pool_tail == NULL){
        free(pool_header);
        return -1;
    }

    pool_header->next = pool_tail;
    pool_header->prev = pool_tail;
    pool_tail ->next = NULL;
    pool_tail->prev = pool_header;
    pool_header->thr_fd = -10;
    pool_tail->thr_fd = -10;

    return 0;
}

void fdtable_destroy_pool() {     // need fix
/*clean up all the memory*/
    thread_info *tmp = pool_header;
    while (tmp != NULL) {
        thread_info *obj = tmp;
        tmp = tmp->next;
        free(obj);
    }
}

thread_info* fdtable_to_pool(int fd){
    thread_info *tmp = fdtable_get_by_fd_all(fd);

    if(tmp != NULL) {
        tmp->thr_fd = -1;

        thread_info *next_node = tmp->next;
        thread_info *prev_node = tmp->prev;

        if (next_node != NULL && prev_node != NULL) {

            prev_node->next = next_node;
            next_node->prev = prev_node;

            tmp ->next = pool_header->next;
            tmp->next->prev = tmp;
            tmp ->prev = pool_header;
            pool_header->next = tmp;

            thr_num--;
        } else {
            exit(-1);
        }
    }
}

thread_info* fdtable_from_pool(){
    thread_info *tmp = NULL;
    if(pool_header->next != pool_tail){
        tmp = pool_header->next;

        pool_header->next = tmp->next;
        tmp->next->prev = pool_header;

        tmp->prev = header;
        tmp->next = header->next;
        header->next = tmp;
        tmp->next->prev = tmp;
        return tmp;
    }
    return NULL;
}
#endif
