#include "ipc.h"

void CPART_send_to_app(retval_data recv_data, thread_info* tmp){
    //sem_wait(&(tmp->ta));       //cglee
	sem_wait(&(tmp->empty2));
	sem_wait(&(tmp->mutex2));
	//memset(tmp->message, 0, sizeof(args_data));                 // memset -cglee
	memcpy(tmp->message, &recv_data, sizeof(retval_data));
	sem_post(&(tmp->mutex2));
	sem_post(&(tmp->full2));
	#ifdef __PRINT__
	char * str = (char *)malloc(sizeof(char)*10);
	memset(str, 0, 10*sizeof(char));
	if(recv_data.request_type != 0){
		printf("recv data request type :: %d, recv return value :: %d \n", recv_data.request_type, recv_data.return_value);
		printf("[Thread SEND][%s]  getpid = %d \n", type_iton(recv_data.request_type, str),  getpid());
	}
	free(str);
	#endif
}

args_data CPART_recv_from_app(args_data  send_data, thread_info* tmp){
//	char * str = (char *)malloc(sizeof(char)*10);
	sem_wait(&(tmp->full));
	sem_wait(&(tmp->mutex));
	//memset(&send_data, 0, sizeof(args_data));                 // memset -cglee
	memcpy(&send_data, (args_data*)(tmp->message), sizeof(args_data));
	sem_post(&(tmp->mutex));
	sem_post(&(tmp->empty));

	#ifdef __PRINT__
	char * str = (char *)malloc(sizeof(char)*10);
	if(send_data.request_type != 0){
	printf("[Thread RECV][%s] getpid = %d\n",type_iton(send_data.request_type, str),  getpid());
	}
	free(str);
    #endif

	//sem_post(&(tmp->ta));       //cglee
	return send_data;
}

void CPART_send_to_thread(args_data send_data, thread_info* tmp){
    //sem_wait(&(tmp->at));       //cglee
	sem_wait(&(tmp->empty));
	sem_wait(&(tmp->mutex));
	//memset(tmp->message, 0, sizeof(args_data));                 // memset -cglee
	memcpy(tmp->message, &send_data, sizeof(args_data));
	sem_post(&(tmp->mutex));
	sem_post(&(tmp->full));
	#ifdef __PRINT__
	char * str = (char *)malloc(sizeof(char)*10);
	memset(str, 0, 10*sizeof(char));
	if(send_data.request_type != 0){
		printf("[Function SEND][%s]  getpid = %d\n", type_iton(send_data.request_type, str), getpid());
	}
	free(str);
	#endif
}

void CPART_recv_from_thread(retval_data* recv_data, thread_info* tmp){
	sem_wait(&(tmp->full2));
	sem_wait(&(tmp->mutex2));
	//memset(recv_data, 0, sizeof(retval_data));                  // memset -cglee
	memcpy((void*)recv_data, tmp->message, sizeof(retval_data));
	sem_post(&(tmp->mutex2));
	sem_post(&(tmp->empty2));

	#ifdef __PRINT__	
	char * str = (char *)malloc(sizeof(char)*10);
	switch(recv_data->request_type){
		case TYPE_SEND:
		case TYPE_SENDTO:
		case TYPE_SENDMSG:
		case TYPE_RECV:
		case TYPE_RECVFROM:
		case TYPE_RECVMSG:
		case TYPE_READ:
		case TYPE_CONNECT:
			if(recv_data->return_value == -1) perror("Connect Error \n");
		case TYPE_WRITE:
			printf("[Function RECV][%s] ReturnSize = %d  Thr_num = %d\n", type_iton(recv_data->request_type, str), (int)recv_data->return_size,  fdtable_getnumber());
			break;
		default:
			printf("[Function RECV][%s] ReturnValue = %d  Thr_num = %d\n", type_iton(recv_data->request_type, str), recv_data->return_value, fdtable_getnumber());
			break;
			
	}
	free(str);
    #endif
    //sem_post(&(tmp->at));       //cglee
}

