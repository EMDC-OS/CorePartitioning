#ifndef __IPC_H__
#define __IPC_H__

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

#include "fdtable.h"
#include "data_types.h"
#include "print.h"


/*functions*/
void CPART_send_to_app(retval_data recv_data, thread_info* tmp);
args_data CPART_recv_from_app(args_data  send_data, thread_info* tmp);
void CPART_send_to_thread(args_data send_data, thread_info* tmp);
void CPART_recv_from_thread(retval_data* recv_data, thread_info* tmp);

#endif
