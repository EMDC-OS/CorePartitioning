#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/irqnr.h>
#include <linux/cputime.h>
//#include <linux/sched/cputime.h>
#include <linux/tick.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/timer.h>

#include <linux/irq.h>

#define nr_irqs 64
#define MAX_CPUS    20
#define TIME_STEP (1*HZ)
#define T_HIGH  50      // for dynamic irq affinity
#define T_LOW   20      // for dynamic irq affinity
#define BLK_END         19
#define NET_START       10
#define BLK_QUEUES      20
#define NET_QUEUES      8
#define NUMBER_OF_SOCKETS 2   // Konkuk univ server = 2

#define SINGLE     //  select SINGLE, CROSS, PER   (Core partitioning mode)

struct proc_stat
{
    u64 user;
    u64 nice;
    u64 system;
    u64 idle;
    u64 iowait;
    u64 irq;
    u64 softirq;
    u64 steal;
    u64 guest;
    u64 guest_nice;
} proc_stat;

struct proc_stat before[MAX_CPUS];
struct proc_stat after[MAX_CPUS];

int p_user[MAX_CPUS];
int p_system[MAX_CPUS];
int p_irq[MAX_CPUS];
int p_total[MAX_CPUS];

/* for dynamic IRQ affinity */
int p_dynamic[MAX_CPUS];

int irq_blk[BLK_QUEUES];    // queue number
int irq_net[NET_QUEUES];    // queue number
int nr_blk_queue;       // blk queue count
int nr_net_queue;       // net queue count
int dynamic_turn; // 0 for blk, 1 for net

int blk_start;
int net_end;
int queue_util;

int blk_start_per[NUMBER_OF_SOCKETS];
int blk_end_per[NUMBER_OF_SOCKETS];
int net_start_per[NUMBER_OF_SOCKETS];
int net_end_per[NUMBER_OF_SOCKETS];
int socket_number;  // 1 for 1 2 for 2 (!!! can't be 0)

int blk_queue_util;
int net_queue_util;
int net_sys_util;

int blk_queue_util_avg[NUMBER_OF_SOCKETS];
int net_queue_util_avg[NUMBER_OF_SOCKETS];
int net_sys_util_avg[NUMBER_OF_SOCKETS];

int sum_avg = 0;
int socket_counter = 0;

int blk_core_counter;
int net_core_counter;
int netsys_core_counter;

int ex_net;     //ex : for socket extended core partitioning
int ex_blk;     //ex means number of cores not core number

cpumask_t affinity_mask;

module_param_array(irq_blk, int, &nr_blk_queue, 0660);
module_param_array(irq_net, int, &nr_net_queue, 0660);
/* for dynamic IRQ affinity*/

static char *dirname="KU";
struct proc_dir_entry *parent;

#ifdef arch_idle_time

static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}

#else

static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];

	return iowait;
}

#endif

typedef struct
{
	struct timer_list timer;
	unsigned long data;
} __attribute__ ((packed)) KERNEL_TIMER_MANAGER;

static KERNEL_TIMER_MANAGER *ptrmng = NULL;

void kerneltimer_timeover(struct timer_list *arg);

void kerneltimer_registertimer(KERNEL_TIMER_MANAGER * pdata, unsigned long timeover)
{
	init_timer(&(pdata->timer));
	pdata->timer.expires = get_jiffies_64() + timeover;
	pdata->timer.data = (unsigned long)pdata;

	pdata->timer.function = kerneltimer_timeover;

	add_timer(&(pdata->timer));
}

void kerneltimer_timeover(struct timer_list *arg)
{
	KERNEL_TIMER_MANAGER * pdata = NULL;

	if (arg)
	{
	    int i;
	    int j;
	    int k;
	    int core;
	    int flag = 0;
	    u64 user, system, idle, irq, total;
	    u64 dynamic; // for dynamic irq affinity
	    //double temp;	// u64? int?
	    u64 temp;

	    user = system = idle = irq = 0;
	    
		pdata= (KERNEL_TIMER_MANAGER *)arg;
		
	    for_each_online_cpu(i) {
		    
		    /*
		    // Copy values here to work around gcc-2.95.3, gcc-2.96
		    after[i].user = kcpustat_cpu(i).cpustat[CPUTIME_USER];
		    after[i].nice = kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		    after[i].system = kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		    after[i].idle = get_idle_time(i);
		    after[i].iowait = get_iowait_time(i);
		    after[i].irq = kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		    after[i].softirq = kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];

		    user = after[i].user - before[i].user;
		    system = after[i].system - before[i].system;
		    idle = after[i].idle - before[i].idle;
		    irq = (after[i].irq + after[i].softirq) - (before[i].irq + before[i].softirq);	// separate irq and softirq?
		    total = user + system + irq ; //total CPU usage
            	    dynamic = system + irq + user;       //dynamic CPU usage
		    */

		    // for real cpu utilization
		    after[i].user = kcpustat_cpu(i).cpustat[CPUTIME_USER];
            after[i].nice = kcpustat_cpu(i).cpustat[CPUTIME_NICE];
            after[i].system = kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
            after[i].idle = get_idle_time(i);
            after[i].iowait = get_iowait_time(i);
            after[i].irq = kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
            after[i].softirq = kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];

            user = after[i].user - before[i].user;
            system = after[i].system - before[i].system;
            idle = after[i].idle - before[i].idle;
            irq = after[i].irq - before[i].irq + after[i].softirq - before[i].softirq;      // separate irq and softirq?
            total = user + system + irq ; //total CPU usage
            dynamic = system + irq + user;       //dynamic CPU usage

		    if (idle == 0)
			    idle = 1;

		 	temp = 100 - ((idle * 100) / (user + idle));
		    if (temp < 1)
			    p_user[i] = 0;
		    else
			    p_user[i] = (int)temp;
			    
			temp = 100 - ((idle * 100) / (system + idle));
		    if (temp < 1)
			    p_system[i] = 0;
		    else
			    p_system[i] = (int)temp;
			    
			temp = 100 - ((idle * 100) / (irq + idle));
		    if (temp < 1)
			    p_irq[i] = 0;
		    else
			    p_irq[i] = (int)temp;				    
		    
		    temp = 100 - ((idle * 100) / (total + idle));
		    if (temp < 1)
			    p_total[i] = 0;
		    else
			    p_total[i] = (int)temp;

            temp = 100 - ((idle * 100) / (dynamic + idle));
            if (temp < 1)
                p_dynamic[i] = 0;
            else
            p_dynamic[i] = (int)temp;
		    
		    before[i].user = after[i].user;
		    before[i].nice = after[i].nice;
		    before[i].system = after[i].system;
		    before[i].idle = after[i].idle;
		    before[i].iowait = after[i].iowait;
		    before[i].irq = after[i].irq;
		    before[i].softirq = after[i].softirq;
	    }

#ifdef SINGLE       // SINGLE socket core partitioning for konkuk server
        queue_util = 0;
        blk_queue_util = 0;
        net_queue_util = 0;
        net_sys_util = 0;

        for(j = blk_start ; j <= BLK_END; j++){
            blk_queue_util += p_dynamic[j];
        }
        for(j = NET_START ; j <= net_end; j++){
            net_queue_util += p_dynamic[j];
        }
        for(j = net_end+1; j < blk_start ; j++){
            net_sys_util += p_dynamic[j];
        }

        if(dynamic_turn == 0){               // turn for blk
            if(blk_queue_util > (T_HIGH*(BLK_END-blk_start+1))){
                if(blk_queue_util/(BLK_END-blk_start+1)>net_sys_util/(blk_start-net_end-1)){
                    if(blk_start-net_end>3){
                        blk_start = blk_start - 1;
                    }
                }
                else if(blk_queue_util/(BLK_END-blk_start+1)<net_sys_util/(blk_start-NET_START-1)){
                    if(blk_start != BLK_END){
                        blk_start = blk_start + 1;
                    }
                }
            }
            else if((blk_queue_util < (T_LOW*(BLK_END-blk_start+1))) && BLK_END != blk_start){
                blk_start = blk_start +1 ;
            }
            for(j = 0; j < BLK_QUEUES; j++){
                core = j%(BLK_END-blk_start+1); // 0~core counter
	        	core = core + blk_start; // for single CP

                cpumask_clear(&affinity_mask);
                cpumask_set_cpu(core, &affinity_mask);
                irq_set_affinity_hint(irq_blk[j], &affinity_mask);
            }
            dynamic_turn = 1;
        }
        else if(dynamic_turn == 1){         // turn for net irq
            if(net_queue_util > (T_HIGH*(net_end-NET_START+1))){
                if(net_queue_util/(net_end-NET_START+1) > net_sys_util/(blk_start-net_end-1)){
                    if(blk_start-net_end>3){
                        net_end = net_end + 1;
                    }
                    else if(net_sys_util/(blk_start-net_end-1) > blk_queue_util/(BLK_END-blk_start+1)){
                        if(BLK_END != blk_start){
                            blk_start = blk_start + 1;
                            net_end = net_end+1;
                        }
                    }
                }
                else if(net_queue_util/(net_end-NET_START+1) < net_sys_util/(blk_start-net_end-1)){
                    if(NET_START != net_end){
                        net_end = net_end - 1;
                    }
                }
            }
            else if(net_queue_util < (T_LOW*(net_end-NET_START+1))){
                if(net_queue_util/(net_end-NET_START+1) < net_sys_util/(blk_start-net_end-1)){
                    if(NET_START != net_end){
                        net_end = net_end - 1;
                    }
                }
            }

            for(j = 0; j < NET_QUEUES; j++) {
                core = j % (net_end - NET_START + 1); // 0~core counter
		        core = core+NET_START; // for single CP
                cpumask_clear(&affinity_mask);
                cpumask_set_cpu(core, &affinity_mask);
                irq_set_affinity_hint(irq_net[j], &affinity_mask);
            }
            dynamic_turn = 0;
        }
#endif

#ifdef CROSS    // CROSS-socket core partitioning for konkuk server
        // Konkuk univ. hadoop server (CROSS core partitioning)
        queue_util = 0;
        blk_queue_util = 0;
        net_queue_util = 0;
        net_sys_util = 0;


        sum_avg = 0;
        socket_counter = 0;

        blk_core_counter = 0;
        net_core_counter = 0;
        netsys_core_counter = 0;



        // calculating blk_queue, net_queue, net_sys cpu utilization(dynamic = sys + irq + softirq) ( total = sys + usr + irq + softirq)
        //if socket_number = 0, socket 1 (10~19) core iteration
        for(i = 1 ; i <= socket_number ; i ++){
            blk_queue_util = 0;
            blk_core_counter = 0;
            net_queue_util = 0;
            net_core_counter = 0;
            net_sys_util = 0;
            netsys_core_counter = 0;

            for(j = blk_start_per[NUMBER_OF_SOCKETS - i] ; j <= blk_end_per[NUMBER_OF_SOCKETS - i]; j++){
                blk_queue_util += p_dynamic[j];
                blk_core_counter = blk_core_counter + 1;
            }
            blk_queue_util_avg[NUMBER_OF_SOCKETS - i] = blk_queue_util/blk_core_counter;
            sum_avg = sum_avg + blk_queue_util_avg[i];

            for(j = net_start_per[NUMBER_OF_SOCKETS - i] ; j <= net_end_per[NUMBER_OF_SOCKETS - i]; j++){
                net_queue_util += p_dynamic[j];
                net_core_counter = net_core_counter + 1;
            }
            net_queue_util_avg[NUMBER_OF_SOCKETS - i] = net_queue_util/net_core_counter;
            sum_avg = sum_avg + net_queue_util_avg[i];

            for(j = net_end_per[NUMBER_OF_SOCKETS - i]+1; j < blk_start_per[NUMBER_OF_SOCKETS - i] ; j++){
                net_sys_util += p_dynamic[j];
                netsys_core_counter = netsys_core_counter + 1;
            }
            net_sys_util_avg[NUMBER_OF_SOCKETS - i] = net_sys_util/netsys_core_counter;
            sum_avg = sum_avg + net_sys_util_avg[i];
        }
        sum_avg = sum_avg / (socket_number*3);
        // change number of sockets w/ each cpu utilization and threshold(high, low)
        //if high, expand sockets
        if(T_HIGH < sum_avg){
            if(socket_number < NUMBER_OF_SOCKETS){
                socket_number = socket_number + 1;
            }
            else if(socket_number >= NUMBER_OF_SOCKETS){
                //pass
            }
        }
        //if low,  sockets--
        else if(T_LOW > sum_avg){
            if(socket_number > 1){
                socket_number = socket_number - 1;
            }
            else if(socket_number <= 1){
                //pass
            }
        }


        sum_avg = 0;
        // calculating blk_queue, net_queue, net_sys cpu utilization(dynamic = sys + irq + softirq) ( total = sys + usr + irq + softirq)
        //if socket_number = 0, socket 1 (10~19) core iteration
        for(i = 1 ; i <= socket_number ; i ++){
            blk_queue_util = 0;
            blk_core_counter = 0;
            net_queue_util = 0;
            net_core_counter = 0;
            net_sys_util = 0;
            netsys_core_counter = 0;

            for(j = blk_start_per[NUMBER_OF_SOCKETS - i] ; j <= blk_end_per[NUMBER_OF_SOCKETS - i]; j++){
                blk_queue_util += p_dynamic[j];
                blk_core_counter = blk_core_counter + 1;
            }
            blk_queue_util_avg[NUMBER_OF_SOCKETS - i] = blk_queue_util/blk_core_counter;
            sum_avg = sum_avg + blk_queue_util_avg[i];

            for(j = net_start_per[NUMBER_OF_SOCKETS - i] ; j <= net_end_per[NUMBER_OF_SOCKETS - i]; j++){
                net_queue_util += p_dynamic[j];
                net_core_counter = net_core_counter + 1;
            }
            net_queue_util_avg[NUMBER_OF_SOCKETS - i] = net_queue_util/net_core_counter;
            sum_avg = sum_avg + net_queue_util_avg[i];

            for(j = net_end_per[NUMBER_OF_SOCKETS - i]+1; j < blk_start_per[NUMBER_OF_SOCKETS - i] ; j++){
                net_sys_util += p_dynamic[j];
                netsys_core_counter = netsys_core_counter + 1;
            }
            net_sys_util_avg[NUMBER_OF_SOCKETS - i] = net_sys_util/netsys_core_counter;
            sum_avg = sum_avg + net_sys_util_avg[i];
        }
        sum_avg = sum_avg / (socket_number*3);

        for(i = 1 ; i <= socket_number ; i++){
            if(dynamic_turn == 0){               // turn for blk
                if(T_HIGH < blk_queue_util_avg[i]){
                    if(blk_queue_util_avg[NUMBER_OF_SOCKETS - i] > net_sys_util_avg[NUMBER_OF_SOCKETS - i]){
                        if(blk_start_per[NUMBER_OF_SOCKETS - i] - net_end_per[NUMBER_OF_SOCKETS - i] > 3){
                            blk_start_per[NUMBER_OF_SOCKETS - i] = blk_start_per[NUMBER_OF_SOCKETS - i] - 1;
                        }
                    }
                    else if(blk_queue_util_avg[NUMBER_OF_SOCKETS - i] < net_sys_util_avg[NUMBER_OF_SOCKETS - i]){
                        if(blk_start_per[NUMBER_OF_SOCKETS - i] != blk_end_per[NUMBER_OF_SOCKETS - i]){
                            blk_start_per[NUMBER_OF_SOCKETS - i] = blk_start_per[NUMBER_OF_SOCKETS - i] + 1;
                        }
                    }
                }
                else if(blk_queue_util_avg[NUMBER_OF_SOCKETS - i] < T_LOW){
                    if(blk_end_per[NUMBER_OF_SOCKETS - i] != blk_start_per[NUMBER_OF_SOCKETS - i]){
                        blk_start_per[NUMBER_OF_SOCKETS - i] = blk_start_per[NUMBER_OF_SOCKETS - i] + 1;
                    }
                }
            }
            else if(dynamic_turn == 1){         // turn for net irq
                if(T_HIGH < net_queue_util_avg[NUMBER_OF_SOCKETS - i]){
                    if(net_queue_util_avg[NUMBER_OF_SOCKETS - i] > net_sys_util_avg[NUMBER_OF_SOCKETS - i]){
                        if(blk_start_per[NUMBER_OF_SOCKETS - i] - net_end_per[NUMBER_OF_SOCKETS - i] > 3){
                            net_end_per[NUMBER_OF_SOCKETS - i] = net_end_per[NUMBER_OF_SOCKETS - i] + 1;
                        }
                    }
                    else if(net_queue_util_avg[NUMBER_OF_SOCKETS - i] < net_sys_util_avg[NUMBER_OF_SOCKETS - i]){
                        if(net_start_per[NUMBER_OF_SOCKETS - i] != net_end_per[NUMBER_OF_SOCKETS - i]){
                            net_end_per[NUMBER_OF_SOCKETS - i] = net_end_per[NUMBER_OF_SOCKETS - i] - 1;
                        }
                    }
                }
                else if(net_queue_util_avg[NUMBER_OF_SOCKETS - i] < T_LOW){
                    if(net_start_per[NUMBER_OF_SOCKETS - i] != net_end_per[NUMBER_OF_SOCKETS - i]){
                        net_end_per[NUMBER_OF_SOCKETS - i] = net_end_per[NUMBER_OF_SOCKETS - i] - 1;
                    }
                }
            }
        }

        k = 0;
        i = 0;

        // redistribute soft irq queues
        if(dynamic_turn == 0){
            for(j = 0; j < BLK_QUEUES; j++){

                core = blk_start_per[NUMBER_OF_SOCKETS-k-1]+i;

                cpumask_clear(&affinity_mask);
                cpumask_set_cpu(core, &affinity_mask);
                irq_set_affinity_hint(irq_blk[j], &affinity_mask);

                if(blk_end_per[NUMBER_OF_SOCKETS-k-1] == blk_start_per[NUMBER_OF_SOCKETS-k-1]+i){
                    k = k + 1;
                    k = k % socket_number;
                    i = 0 ;
                }
                else{
                    i = i + 1;
                }
            }
            dynamic_turn = 1;
        }
        else if(dynamic_turn == 1){
            for(j = 0; j < NET_QUEUES; j++) {
                core = net_start_per[NUMBER_OF_SOCKETS-k-1]+i;

                cpumask_clear(&affinity_mask);
                cpumask_set_cpu(core, &affinity_mask);
                irq_set_affinity_hint(irq_net[j], &affinity_mask);

                if(net_start_per[NUMBER_OF_SOCKETS-k-1] + i == net_end_per[NUMBER_OF_SOCKETS-k-1]){
                    k = k + 1;
                    k = k % socket_number;
                    i = 0 ;
                }
                else{
                    i = i + 1;
                }
            }
            dynamic_turn = 0;
        }
#endif

#ifdef PER
        // Konkuk univ. hadoop server (PER core partitioning)
        queue_util = 0;
        blk_queue_util = 0;
        net_queue_util = 0;
        net_sys_util = 0;

        blk_core_counter = 0;
        net_core_counter = 0;
        netsys_core_counter = 0;

        sum_avg = 0;
        socket_counter = 0;

        socket_number = NUMBER_OF_SOCKETS;


        // calculating blk_queue, net_queue, net_sys cpu utilization(dynamic = sys + irq + softirq) ( total = sys + usr + irq + softirq)
        // if socket_number = 0, socket 1 (10~19) core iteration


        for(i = 1 ; i <= socket_number ; i ++){
            blk_queue_util = 0;
            blk_core_counter = 0;
            net_queue_util = 0;
            net_core_counter = 0;
            net_sys_util = 0;
            netsys_core_counter = 0;

            for(j = blk_start_per[NUMBER_OF_SOCKETS - i] ; j <= blk_end_per[NUMBER_OF_SOCKETS - i]; j++){
                blk_queue_util += p_dynamic[j];
                blk_core_counter = blk_core_counter + 1;
            }
            blk_queue_util_avg[NUMBER_OF_SOCKETS - i] = blk_queue_util/blk_core_counter;
            sum_avg = sum_avg + blk_queue_util_avg[i];

            for(j = net_start_per[NUMBER_OF_SOCKETS - i] ; j <= net_end_per[NUMBER_OF_SOCKETS - i]; j++){
                net_queue_util += p_dynamic[j];
                net_core_counter = net_core_counter + 1;
            }
            net_queue_util_avg[NUMBER_OF_SOCKETS - i] = net_queue_util/net_core_counter;
            sum_avg = sum_avg + net_queue_util_avg[i];

            for(j = net_end_per[NUMBER_OF_SOCKETS - i]+1; j < blk_start_per[NUMBER_OF_SOCKETS - i] ; j++){
                net_sys_util += p_dynamic[j];
                netsys_core_counter = netsys_core_counter + 1;
            }
            net_sys_util_avg[NUMBER_OF_SOCKETS - i] = net_sys_util/netsys_core_counter;
            sum_avg = sum_avg + net_sys_util_avg[i];
        }
        sum_avg = sum_avg / (socket_number*3);

        for(i = 1 ; i <= socket_number ; i++){
            if(dynamic_turn == 0){               // turn for blk
                if(T_HIGH < blk_queue_util_avg[i]){
                    if(blk_queue_util_avg[NUMBER_OF_SOCKETS - i] > net_sys_util_avg[NUMBER_OF_SOCKETS - i]){
                        if(blk_start_per[NUMBER_OF_SOCKETS - i] - net_end_per[NUMBER_OF_SOCKETS - i] > 3){
                            blk_start_per[NUMBER_OF_SOCKETS - i] = blk_start_per[NUMBER_OF_SOCKETS - i] - 1;
                        }
                    }
                    else if(blk_queue_util_avg[NUMBER_OF_SOCKETS - i] < net_sys_util_avg[NUMBER_OF_SOCKETS - i]){
                        if(blk_start_per[NUMBER_OF_SOCKETS - i] != blk_end_per[NUMBER_OF_SOCKETS - i]){
                            blk_start_per[NUMBER_OF_SOCKETS - i] = blk_start_per[NUMBER_OF_SOCKETS - i] + 1;
                        }
                    }
                }
                else if(blk_queue_util_avg[NUMBER_OF_SOCKETS - i] < T_LOW){
                    if(blk_end_per[NUMBER_OF_SOCKETS - i] != blk_start_per[NUMBER_OF_SOCKETS - i]){
                        blk_start_per[NUMBER_OF_SOCKETS - i] = blk_start_per[NUMBER_OF_SOCKETS - i] + 1;
                    }
                }
            }
            else if(dynamic_turn == 1){         // turn for net irq
                if(T_HIGH < net_queue_util_avg[NUMBER_OF_SOCKETS - i]){
                    if(net_queue_util_avg[NUMBER_OF_SOCKETS - i] > net_sys_util_avg[NUMBER_OF_SOCKETS - i]){
                        if(blk_start_per[NUMBER_OF_SOCKETS - i] - net_end_per[NUMBER_OF_SOCKETS - i] > 3){
                            net_end_per[NUMBER_OF_SOCKETS - i] = net_end_per[NUMBER_OF_SOCKETS - i] + 1;
                        }
                    }
                    else if(net_queue_util_avg[NUMBER_OF_SOCKETS - i] < net_sys_util_avg[NUMBER_OF_SOCKETS - i]){
                        if(net_start_per[NUMBER_OF_SOCKETS - i] != net_end_per[NUMBER_OF_SOCKETS - i]){
                            net_end_per[NUMBER_OF_SOCKETS - i] = net_end_per[NUMBER_OF_SOCKETS - i] - 1;
                        }
                    }
                }
                else if(net_queue_util_avg[NUMBER_OF_SOCKETS - i] < T_LOW){
                    if(net_start_per[NUMBER_OF_SOCKETS - i] != net_end_per[NUMBER_OF_SOCKETS - i]){
                        net_end_per[NUMBER_OF_SOCKETS - i] = net_end_per[NUMBER_OF_SOCKETS - i] - 1;
                    }
                }
            }
        }

        k = 0;
        i = 0;

        // redistribute soft irq queues
        if(dynamic_turn == 0){
            for(j = 0; j < BLK_QUEUES; j++){

                core = blk_start_per[NUMBER_OF_SOCKETS-k-1]+i;

                cpumask_clear(&affinity_mask);
                cpumask_set_cpu(core, &affinity_mask);
                irq_set_affinity_hint(irq_blk[j], &affinity_mask);

                if(blk_end_per[NUMBER_OF_SOCKETS-k-1] == blk_start_per[NUMBER_OF_SOCKETS-k-1]+i){
                    k = k + 1;
                    k = k % socket_number;
                    i = 0 ;
                }
                else{
                    i = i + 1;
                }
            }
            dynamic_turn = 1;
        }
        else if(dynamic_turn == 1){
            for(j = 0; j < NET_QUEUES; j++) {
                core = net_start_per[NUMBER_OF_SOCKETS-k-1]+i;

                cpumask_clear(&affinity_mask);
                cpumask_set_cpu(core, &affinity_mask);
                irq_set_affinity_hint(irq_net[j], &affinity_mask);

                if(net_start_per[NUMBER_OF_SOCKETS-k-1] + i == net_end_per[NUMBER_OF_SOCKETS-k-1]){
                    k = k + 1;
                    k = k % socket_number;
                    i = 0 ;
                }
                else{
                    i = i + 1;
                }
            }
            dynamic_turn = 0;
        }
#endif
		kerneltimer_registertimer(pdata, TIME_STEP);
	}
}

static int show_stat_user(struct seq_file *p, void *v)
{
	int i;

	for_each_online_cpu(i) {
		seq_printf(p, "%d", p_user[i]);
		seq_putc(p, '\n');
	}

	return 0;
}

static int show_stat_system(struct seq_file *p, void *v)
{
	int i;

	for_each_online_cpu(i) {
		seq_printf(p, "%d", p_system[i]);
		seq_putc(p, '\n');
	}

	return 0;
}

static int show_stat_irq(struct seq_file *p, void *v)
{
	int i;

	for_each_online_cpu(i) {
		seq_printf(p, "%d", p_irq[i]);
		seq_putc(p, '\n');
	}

	return 0;
}

static int show_stat_total(struct seq_file *p, void *v)
{
	int i;

	for_each_online_cpu(i) {
		seq_printf(p, "%d", p_total[i]);
		seq_putc(p, '\n');
	}

	return 0;
}

#ifdef SINGLE
static int show_stat_dynamic(struct seq_file *p, void *v)
{
    seq_printf(p, "%d", net_end);
    seq_putc(p, '\n');
    seq_printf(p, "%d", blk_start);
    seq_putc(p, '\n');

    return 0;
}
#endif

#ifdef CROSS
static int show_stat_dynamic(struct seq_file *p, void *v)
{
    seq_printf(p, "%d", socket_number);             // how many cores C.P use
    seq_putc(p, '\n');
    int i = 0 ;
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i ++){
        seq_printf(p, "%d", net_start_per[i]);
        seq_putc(p, '\n');
        seq_printf(p, "%d", net_end_per[i]);
        seq_putc(p, '\n');
        seq_printf(p, "%d", blk_start_per[i]);
        seq_putc(p, '\n');
        seq_printf(p, "%d", blk_end_per[i]);
        seq_putc(p, '\n');
    }
    return 0;
}
#endif



#ifdef PER

//per socket core partitioning
static int show_stat_dynamic(struct seq_file *p, void *v)
{
    int i = 0 ;
    for(i = 0 ; i < NUMBER_OF_SOCKETS ; i ++){
        seq_printf(p, "%d", net_start_per[i]);
        seq_putc(p, '\n');
        seq_printf(p, "%d", net_end_per[i]);
        seq_putc(p, '\n');
        seq_printf(p, "%d", blk_start_per[i]);
        seq_putc(p, '\n');
        seq_printf(p, "%d", blk_end_per[i]);
        seq_putc(p, '\n');
    }
    return 0;
}
#endif

static int stat_open_user(struct inode *inode, struct file *file)
{
	size_t size = 1024 + 128 * num_online_cpus();

	/* minimum size to display an interrupt count : 2 bytes */
	size += 2 * nr_irqs;
	return single_open_size(file, show_stat_user, NULL, size);
}

static int stat_open_system(struct inode *inode, struct file *file)
{
	size_t size = 1024 + 128 * num_online_cpus();

	/* minimum size to display an interrupt count : 2 bytes */
	size += 2 * nr_irqs;
	return single_open_size(file, show_stat_system, NULL, size);
}

static int stat_open_irq(struct inode *inode, struct file *file)
{
	size_t size = 1024 + 128 * num_online_cpus();

	/* minimum size to display an interrupt count : 2 bytes */
	size += 2 * nr_irqs;
	return single_open_size(file, show_stat_irq, NULL, size);
}

static int stat_open_total(struct inode *inode, struct file *file)
{
	size_t size = 1024 + 128 * num_online_cpus();

	/* minimum size to display an interrupt count : 2 bytes */
	size += 2 * nr_irqs;
	return single_open_size(file, show_stat_total, NULL, size);
}

static int stat_open_dynamic(struct inode *inode, struct file *file)
{
    size_t size = 1024 + 128 * num_online_cpus();

    /* minimum size to display an interrupt count : 2 bytes */
    size += 2 * nr_irqs;
    return single_open_size(file, show_stat_dynamic, NULL, size);
}


static const struct file_operations user_fops = {
	.open		= stat_open_user,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations system_fops = {
	.open		= stat_open_system,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations irq_fops = {
	.open		= stat_open_irq,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations total_fops = {
	.open		= stat_open_total,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations dynamic_fops = {
    .open		= stat_open_dynamic,
    .read		= seq_read,
    .llseek		= seq_lseek,
    .release	= single_release,
};

static int __init init_simpleproc (void)
{
    printk(KERN_INFO "[simple_proc_app] module is now initiating.\n");

    blk_start = 16;
    net_end = 12;
    dynamic_turn = 0; // 0 for blk, 1 for net
    socket_number = 1;  // 1 for 1 2 for 2 (!!! can't be 0)

    ex_net = 0;
    //ex_blk = 10;   // cross socket
    ex_blk = 0; // single socket

    blk_core_counter = 0;
    net_core_counter = 0;
    netsys_core_counter = 0;

    blk_start_per[0] = 6;
    blk_end_per[0] = 9;
    blk_start_per[1] = 16;
    blk_end_per[1] = 19;
    net_start_per[0] = 0;
    net_end_per[0] = 2;
    net_start_per[1] = 10;
    net_end_per[1] = 12;


	parent = proc_mkdir(dirname, NULL);
	if (! parent) {
		printk(KERN_INFO "ERROR! proc_mkdir\n");
		remove_proc_entry(dirname,NULL);
		return -1;
	}

	if (! proc_create("user",0666,parent,&user_fops)) {
		printk(KERN_INFO "ERROR! proc_create user\n");
		remove_proc_entry("user",parent);
		remove_proc_entry(dirname,NULL);
		return -1;
	}

	if (! proc_create("system",0666,parent,&system_fops)) {
		printk(KERN_INFO "ERROR! proc_create system\n");
		remove_proc_entry("system",parent);
		remove_proc_entry("user",parent);
		remove_proc_entry(dirname,NULL);
		return -1;
	}

	if (! proc_create("intr",0666,parent,&irq_fops)) {
		printk(KERN_INFO "ERROR! proc_create intr\n");
		remove_proc_entry("intr",parent);
		remove_proc_entry("system",parent);
		remove_proc_entry("user",parent);
		remove_proc_entry(dirname,NULL);
		return -1;
	}
	
	if (! proc_create("total",0666,parent,&total_fops)) {
		printk(KERN_INFO "ERROR! proc_create intr\n");
		remove_proc_entry("total",parent);
		remove_proc_entry("intr",parent);
		remove_proc_entry("system",parent);
		remove_proc_entry("user",parent);
		remove_proc_entry(dirname,NULL);
		return -1;
	}

    if (! proc_create("dynamic",0666,parent,&dynamic_fops)) {
        printk(KERN_INFO "ERROR! proc_create intr\n");
        remove_proc_entry("dynamic",parent);
        remove_proc_entry("total",parent);
        remove_proc_entry("intr",parent);
        remove_proc_entry("system",parent);
        remove_proc_entry("user",parent);
        remove_proc_entry(dirname,NULL);
        return -1;
    }
	ptrmng = kmalloc(sizeof(KERNEL_TIMER_MANAGER), GFP_KERNEL);
	if (ptrmng == NULL) return - ENOMEM;
	memset(ptrmng, 0, sizeof(KERNEL_TIMER_MANAGER));
	ptrmng->data = 0;

    kerneltimer_registertimer(ptrmng, TIME_STEP);

    printk(KERN_INFO "[simple_proc] module initiated successfully.\n");
	return 0;
}

static void __exit exit_simpleproc(void)
{
    if (ptrmng != NULL)
	{
		del_timer(&(ptrmng->timer));
		kfree(ptrmng);
	}
	
	remove_proc_entry("user",parent);
	remove_proc_entry("system",parent);
	remove_proc_entry("intr",parent);
	remove_proc_entry("total",parent);
    remove_proc_entry("dynamic",parent);
	remove_proc_entry(dirname,NULL);
	
	printk(KERN_INFO "[simple_proc] module exit\n");
}

module_init(init_simpleproc);
module_exit(exit_simpleproc);

MODULE_LICENSE("GPL");
