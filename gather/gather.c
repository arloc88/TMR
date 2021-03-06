/*----------------gather.c--------------
Paudice - Riccio - Tammaro

Il task aperiodico gather è in attesa di una richiesta aperiodica, quando questa arriva il gather viene invocato dal Server DS tramite 
mailbox quindi raccoglie le informazioni sullo stato del sistema dalla memoria condivisa gatMem e le invia trmaite mailbox al task diag
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <rtai_lxrt.h>
#include <rtai_mbx.h>
#include <rtai_shm.h>
#include <rtai_sem.h>
#include <signal.h>
#include <sys/io.h>
#include <rtai_sched.h>
#include <rtai.h>
#include "headers.h"

static RT_TASK* asyncTask;
static MBX* mbx; //tramite questa mailbox il DS invoca il gather
static MBX* status; //uso la mailbox per inviare i dati del sistema al diag
static SharedMemory *gatMem;//memoria condivisa in cui sono memorizzati i dati per lo status del sistema
SEM* sem_buf; //semaforo per l'accesso al buffer

static int start;

void sig_handler(int signum){
	start = 0;
}

int main(void){

	printf("\n Waiting for Request...\n");
	int data;
	signal(SIGINT, sig_handler);

	if(!(asyncTask = rt_task_init(nam2num("RT_MONITOR"),1,STACK_SIZE,0))){
		printf("failed creating rt task\n");
		exit(-1);
	}

	start = 1;
	gatMem = (SharedMemory*)rtai_malloc(SHARED_ID,sizeof(SharedMemory));
	sem_buf = rt_typed_named_sem_init(BUF_SEM,1,BIN_SEM | FIFO_Q);
	mbx = rt_typed_named_mbx_init(DSGAT_MBX,1,FIFO_Q);
	status = rt_typed_named_mbx_init(STATUS_MBX,MAILBOX_SIZE,FIFO_Q);
	while(start){
		if(!rt_mbx_receive(mbx,&data,sizeof(int))) {
			printf("\nHo ricevuto una richiesta dal server...\n");
			printf("\n RECUPERO LO STATO DEL SISTEMA.....\n");
			//invio il buffer

			int i=0; 
			rt_sem_wait(sem_buf);
			for (i=0;i<10;i++){
				rt_mbx_send(status, &gatMem->buff[i], sizeof(unsigned int));
						
			}
			rt_sem_signal(sem_buf);
			rt_mbx_send(status, &gatMem->c1, sizeof(unsigned int));
			rt_mbx_send(status, &gatMem->c2, sizeof(unsigned int));
			rt_mbx_send(status, &gatMem->c3, sizeof(unsigned int));
			rt_mbx_send(status, &gatMem->s1, sizeof(unsigned int));
			rt_mbx_send(status, &gatMem->s2, sizeof(unsigned int));
			rt_mbx_send(status, &gatMem->s3, sizeof(unsigned int));
			rt_mbx_send(status, &gatMem->final_c, sizeof(unsigned int));
		
			}
		else
			printf("failed retrieving message from mailbox\n");
			printf("\n Waiting for Request...\n");
	}
	
	rt_named_mbx_delete(mbx);
	rt_named_mbx_delete(status);
	rtai_free(SHARED_ID,&gatMem);
	rt_named_sem_delete(sem_buf);
	rt_task_delete(asyncTask);

	return 0;
}
