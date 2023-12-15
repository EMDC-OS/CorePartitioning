/*
 * affinity.c
 *
 *  Created on: Jan 2, 2018
 *      Author: root
 */
#include "ld_preload.h"

int get_app_cpu(){
	FILE* fp;
	char proc_tmp[256];
	char stat[1024];
	char *token;
	int i = 0;
	int cpu = -1;
	sprintf(proc_tmp,"/proc/%d/stat",getpid());
	fp = fopen(proc_tmp, "r");
	if(fp == NULL){
		printf("file open error \n");
	}else{
		fgets(stat, 1024, fp);
		token = strtok(stat," ");
		while(token != NULL){
			token = strtok(NULL," ");
			if(i == 38) break;
			i++; 			// i == 38 unreachable - cglee
		}
	}
	cpu = atoi(token);
	fclose(fp); 		// add fclose(fp) - cglee
	return cpu;
}

int set_affinity_within(boolean NetorFile){
/*read which cpu core application is running on and set syscall affinity in same socket*/
	int app_cpu = get_app_cpu();
	int syscall_cpu = 0;
	if(app_cpu < ONE_NODE){
		set_cpu(0, NetorFile);
	}else {
		set_cpu(1, NetorFile);
	}
	return syscall_cpu ;	// changyu-lee : syscall_cpu always return 0 ?
}

void set_cpu(boolean socket, boolean NetorFile)
{
	int n=0, i=0 ;
	int k=0;
	int sum_user=0, sum_syscall=0, sum_intr=0;
	int sum_total = 0;
	int minimum = 0;


    char socket_number[1][10];
    int soc_num;
    char net_start_per_c[NUMBER_OF_SOCKETS][10];
    char net_end_per_c[NUMBER_OF_SOCKETS][10];
    char blk_start_per_c[NUMBER_OF_SOCKETS][10];
    char blk_end_per_c[NUMBER_OF_SOCKETS][10];

    int net_start_per[NUMBER_OF_SOCKETS];
    int net_end_per[NUMBER_OF_SOCKETS];
    int blk_start_per[NUMBER_OF_SOCKETS];
    int blk_end_per[NUMBER_OF_SOCKETS];


    char total_filepath[PROC_MAX_LEN], total_buf[INTEL_CPU][PROC_MAX_LEN];  //changyu-lee : total_buf[PROC_MAX_LEN][INTEL_CPU] -> [INTEL_CPU][PROC_MAX_LEN]
    char dynamic_filepath[PROC_MAX_LEN], dynamic_buf[INTEL_CPU][PROC_MAX_LEN];
    char core_start_end[NUMBER_OF_SOCKETS][10];        // 0 for net 1 for blk
	FILE *ft = NULL ;
    FILE *dynamic = NULL ;
    FILE *log_fd;

    cpu_set_t cpu_mask;

	n = sprintf(total_filepath, "/proc/KU/total");
    k = sprintf(dynamic_filepath, "/proc/KU/dynamic");

	ft = fopen(total_filepath, "r");
    dynamic = fopen(dynamic_filepath, "r");

	if(!ft || !dynamic){
		printf("total fopen failed \n");
	}
	int counter = 0 ;
    for(counter = 0 ; counter < MAX_CPUS ; counter++){
        fgets(total_buf[counter], PROC_MAX_LEN-1, ft);
    }
#ifdef SINGLE
    fgets(core_start_end[0], 9, dynamic);
    fgets(core_start_end[1], 9, dynamic);
    net_end = atoi(core_start_end[0]);
    file_start = atoi(core_start_end[1]);
#endif

#ifdef CROSS
    fgets(socket_number[0], 9, dynamic);
    soc_num = atoi(socket_number[0]);
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i++){
        fgets(net_start_per_c[i], 9, dynamic);
        fgets(net_end_per_c[i], 9, dynamic);
        fgets(blk_start_per_c[i], 9, dynamic);
        fgets(blk_end_per_c[i], 9, dynamic);
    }
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i++){
        net_start_per[i] = atoi(net_start_per_c[i]);
        net_end_per[i] = atoi(net_end_per_c[i]);
        blk_start_per[i] = atoi(blk_start_per_c[i]);
        blk_end_per[i] = atoi(blk_end_per_c[i]);
    }
#endif

#ifdef PER
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i++){
        fgets(net_start_per_c[i], 9, dynamic);
        fgets(net_end_per_c[i], 9, dynamic);
        fgets(blk_start_per_c[i], 9, dynamic);
        fgets(blk_end_per_c[i], 9, dynamic);
    }
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i++){
        net_start_per[i] = atoi(net_start_per_c[i]);
        net_end_per[i] = atoi(net_end_per_c[i]);
        blk_start_per[i] = atoi(blk_start_per_c[i]);
        blk_end_per[i] = atoi(blk_end_per_c[i]);
    }
#endif

    fclose(ft);
    fclose(dynamic);


#ifdef __FILEIO__
#ifdef SINGLE
    if(NetorFile == 0){ /* NetIO */
		minimum = net_end+1;

		CPU_ZERO(&mask);
		for(net_cpu = net_end+1; net_cpu < file_start; net_cpu++){
			CPU_SET(net_cpu, &mask);
		}
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	}
	else{/* FileIO*/
		minimum = file_start;

		CPU_ZERO(&mask);
		for(file_cpu = file_start; file_cpu < MAX_CPUS; file_cpu++){
			CPU_SET(file_cpu, &mask);
		}
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	}
#endif

#ifdef CROSS
	if(NetorFile == 0){ /* NetIO */
		CPU_ZERO(&mask);
        for(i = 1 ; i <= soc_num ; i ++){
            k = NUMBER_OF_SOCKETS - i;
            for(n = net_end_per[k]+1 ; n < blk_start_per[k] ; n++){
                CPU_SET(n, &mask);
            }
        }
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	}
	else{/* FileIO*/
		CPU_ZERO(&mask);
		for(i = 1 ; i <= soc_num ; i ++){
            k = NUMBER_OF_SOCKETS - i;
            for(n = blk_start_per[k] ; n <= blk_end_per[k] ; n++){
                CPU_SET(n, &mask);
            }
        }
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	}
#endif

#ifdef PER
	int currnet_core = sched_getcpu();
	int current_node = (current_core / CORES_PER_SOCKET);

	if(NetorFile == 0){ /* NetIO */
		CPU_ZERO(&mask);

        for(i = net_end_per[current_node]+1 ; i < blk_start_per[current_node]; i++){
            CPU_SET(i, &mask);
        }
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	}
	else{/* FileIO*/
		CPU_ZERO(&mask);
		for(i = blk_start_per[current_node] ; i < blk_end_per[current_node]; i++){
		    CPU_SET(i, &mask);
        }
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	}
#endif

#ifdef __DEBUG_AFFINITY__
    log_fd = fopen("~/CP_debug_affinity.txt", "a+");

    fprintf(log_fd, "DEBUG_AFFINITY : affinity is set with this mask\n");
    for(k=0; k<MAX_CPUS; k++){
        if(CPU_ISSET(k, &mask))
            fprintf(log_fd, " %d",k);
    }

    fprintf(log_fd, "DEBUG_AFFINITY : thread affinity is\n");
    for(k=0; k<MAX_CPUS; k++){
        if(CPU_ISSET(k, &mask))
            fprintf(log_fd, " %d",k);
    }
    fprintf(log_fd, "\n  current cpu = %d", sched_getcpu());
    fclose(log_fd);
#endif

#endif
#ifndef __FILEIO__
	/* for konkuk server cross core partitioning
	if(socket == 0){ //processor socket 0 
		if(atoi(total_buf[socket0_cpu]) > THRESHOLD){
			if(socket0_cpu <= 10 && socket0_cpu > 0) socket0_cpu ++;
			else socket0_cpu = 3;
		}
		CPU_ZERO(&mask);
		CPU_SET(socket0_cpu, &mask);
		sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	}
	else{//processor socket 1 
		if(atoi(total_buf[socket1_cpu]) > THRESHOLD){
			if(socket1_cpu <= 20 && socket1_cpu > 10 ) socket1_cpu++;
			else socket1_cpu = 11;
		}
		CPU_ZERO(&mask);
		CPU_SET(socket1_cpu, &mask);
		sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	}
	*/
#endif
}

void set_cpu_app(boolean NetorFile)
{
    int n=0, i=0 ;
    int k=0;
    int sum_user=0, sum_syscall=0, sum_intr=0;
    int sum_total = 0;
    int minimum = 0;


    char socket_number[1][10];
    int soc_num;
    char net_start_per_c[NUMBER_OF_SOCKETS][10];
    char net_end_per_c[NUMBER_OF_SOCKETS][10];
    char blk_start_per_c[NUMBER_OF_SOCKETS][10];
    char blk_end_per_c[NUMBER_OF_SOCKETS][10];

    int net_start_per[NUMBER_OF_SOCKETS];
    int net_end_per[NUMBER_OF_SOCKETS];
    int blk_start_per[NUMBER_OF_SOCKETS];
    int blk_end_per[NUMBER_OF_SOCKETS];


    char total_filepath[PROC_MAX_LEN], total_buf[INTEL_CPU][PROC_MAX_LEN];  //changyu-lee : total_buf[PROC_MAX_LEN][INTEL_CPU] -> [INTEL_CPU][PROC_MAX_LEN]
    char dynamic_filepath[PROC_MAX_LEN], dynamic_buf[INTEL_CPU][PROC_MAX_LEN];
    char core_start_end[NUMBER_OF_SOCKETS][10];        // 0 for net 1 for blk
    FILE *ft = NULL ;
    FILE *dynamic = NULL ;
    FILE *log_fd;

    cpu_set_t cpu_mask;

    n = sprintf(total_filepath, "/proc/KU/total");
    k = sprintf(dynamic_filepath, "/proc/KU/dynamic");

    ft = fopen(total_filepath, "r");
    dynamic = fopen(dynamic_filepath, "r");

    if(!ft || !dynamic){
        printf("total fopen failed \n");
    }
    int counter = 0 ;
    for(counter = 0 ; counter < MAX_CPUS ; counter++){
        fgets(total_buf[counter], PROC_MAX_LEN-1, ft);
    }
#ifdef SINGLE
    fgets(core_start_end[0], 9, dynamic);
    fgets(core_start_end[1], 9, dynamic);
    net_end = atoi(core_start_end[0]);
    file_start = atoi(core_start_end[1]);
#endif

#ifdef CROSS
    fgets(socket_number[0], 9, dynamic);
    soc_num = atoi(socket_number[0]);
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i++){
        fgets(net_start_per_c[i], 9, dynamic);
        fgets(net_end_per_c[i], 9, dynamic);
        fgets(blk_start_per_c[i], 9, dynamic);
        fgets(blk_end_per_c[i], 9, dynamic);
    }
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i++){
        net_start_per[i] = atoi(net_start_per_c[i]);
        net_end_per[i] = atoi(net_end_per_c[i]);
        blk_start_per[i] = atoi(blk_start_per_c[i]);
        blk_end_per[i] = atoi(blk_end_per_c[i]);
    }
#endif

#ifdef PER
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i++){
        fgets(net_start_per_c[i], 9, dynamic);
        fgets(net_end_per_c[i], 9, dynamic);
        fgets(blk_start_per_c[i], 9, dynamic);
        fgets(blk_end_per_c[i], 9, dynamic);
    }
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i++){
        net_start_per[i] = atoi(net_start_per_c[i]);
        net_end_per[i] = atoi(net_end_per_c[i]);
        blk_start_per[i] = atoi(blk_start_per_c[i]);
        blk_end_per[i] = atoi(blk_end_per_c[i]);
    }
#endif

#ifdef __FILEIO__
    #ifdef SINGLE
    if(NetorFile == 0){ /* NetIO */
		minimum = net_end+1;

		CPU_ZERO(&mask);
		for(net_cpu = net_end+1; net_cpu < file_start; net_cpu++){
			if(atoi(total_buf[minimum]) > atoi(total_buf[net_cpu])){
                minimum = net_cpu;
            }
		}
        CPU_SET(minimum, &mask);
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	}
	else{/* FileIO*/
		minimum = file_start;

		CPU_ZERO(&mask);
		for(file_cpu = file_start; file_cpu < MAX_CPUS; file_cpu++){
            if(atoi(total_buf[minimum]) > atoi(total_buf[file_cpu])){
                minimum = file_cpu;
            }
		}
        CPU_SET(file_cpu, &mask);
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask);
	}
#endif

#ifdef CROSS
#endif

#ifdef PER
#endif

#ifdef __DEBUG_AFFINITY__
    log_fd = fopen("~/CP_debug_affinity.txt", "a+");

    fprintf(log_fd, "DEBUG_AFFINITY : process affinity is set with this mask\n");
    for(k=0; k<MAX_CPUS; k++){
        if(CPU_ISSET(k, &mask))
            fprintf(log_fd, " %d",k);
    }

    fprintf(log_fd, "DEBUG_AFFINITY : thread affinity is\n");
    for(k=0; k<MAX_CPUS; k++){
        if(CPU_ISSET(k, &mask))
            fprintf(log_fd, " %d",k);
    }
    fprintf(log_fd, "\n  current cpu = %d", sched_getcpu());
    fclose(log_fd);
#endif

#endif

    fclose(ft);
    fclose(dynamic);
}