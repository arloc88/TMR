/* ----------------------diag.c-------------------

Paudice - Riccio - Tammaro

Il task aperiodico diag permette di effettuare una richiesta aperiodica premendo il tasto 1 
che lasegnala al Server, il server invocherà il task gather tramite mailbox.
La richiesta viene servita dal gather che invia al diag le informazioni sul sistema.
Il diag riceve dal gather le info sul sistema tramite mailbox e le stampa a video.

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <rtai_lxrt.h>
#include <sys/io.h>
#include <rtai_mbx.h>
#include <rtai_shm.h>
#include <rtai_sem.h>
#include <signal.h>
#include <sys/io.h>
#include <rtai_sched.h>
#include <rtai.h>

#include "headers.h" 

static RT_TASK* asyncTask;
static diagreq* shared; //memoria condivisa in che viene controllata dal DS
//per verificare se è stata fatta una richiesta
static SEM* mutex;


static MBX* status;

void async_request(void){ //quando premo 1 sullo schermo scatta la richiesta
//asincrona tramite questa procedura
	int done = 0;
	rt_sem_wait(mutex);
	//start of critical section
	shared->req = 1;
	//end of critical section
	rt_sem_signal(mutex);

	do{
		sleep(SLEEP_TIME_S);
		rt_sem_wait(mutex);
		
		if(shared->res == 1){
			shared->res = 0;
			done = 1;
		}

		rt_sem_signal(mutex);

	}while(!done);

}


int main(void){
	int start;

	if(!(asyncTask = rt_task_init(nam2num("RT_ASYNC"),1,STACK_SIZE,0))){
		printf("failed creating rt task\n");
		exit(-1);
	}

	mutex = rt_typed_named_sem_init(DIADS_SEM,1,BIN_SEM | FIFO_Q);
	shared = (diagreq*)rtai_malloc(DIADS_MEM,sizeof(diagreq));
	//status = (SharedMemory*)rtai_malloc(STATUS_MEM,sizeof(SharedMemory));
	status = rt_typed_named_mbx_init(STATUS_MBX,MAILBOX_SIZE,FIFO_Q);

	start = 1;
	do{
		printf("which will it be?\n");
		printf("0.exit\n");
		printf("1.start aperiodic request\n");
		printf(">");
		scanf("%d",&start);
		if(start){
			
			async_request();
			//variabili in cui memorizzare i dati ricevuti dalla mailbox
			unsigned int c1,c2,c3,s1,s2,s3,final_c;
			
			int i=0;
			int buff[10];
			for (i=0;i<10;i++){
				rt_mbx_receive(status, &buff[i], sizeof(unsigned int));
				printf("buf[%d]=%d\n",i,buff[i]);
				}
			
			printf("\n");
			rt_mbx_receive(status, &c1, sizeof(unsigned int));
			rt_mbx_receive(status, &c2, sizeof(unsigned int));
			rt_mbx_receive(status, &c3, sizeof(unsigned int));
			rt_mbx_receive(status, &s1, sizeof(unsigned int));
			rt_mbx_receive(status, &s2, sizeof(unsigned int));
			rt_mbx_receive(status, &s3, sizeof(unsigned int));
			rt_mbx_receive(status, &final_c, sizeof(unsigned int));
			
			//SE VOGLIO STAMPARE SOLO I VALORI RICEVUTI
			if(s1==0)		
			printf("Control action di Control1 ->%d\n",c1);
			if(s2==0)
			printf("Control action di Control2 ->%d\n",c2);
			if(s3==0)
			printf("Control action di Control3 ->%d\n",c3);
			
			/* SE VOGLIO STAMPARE TUTTI I VALORI CALCOLATI DAI CONTROLLER
					
			printf("Control action di Control1 ->%d\n",c1);
			
			printf("Control action di Control2 ->%d\n",c2);
			
			printf("Control action di Control3 ->%d\n",c3);
			
			printf("\n");*/
			
			
			if(s1==0)
				printf("Stato di Control1 -> ATTIVO\n");
			else 
				printf("Stato di Control1 -> FALLITO\n");
			if(s2==0)
				printf("Stato di Control2 -> ATTIVO\n");
			else 
				printf("Stato di Control2 -> FALLITO\n");
			if(s3==0)
				printf("Stato di Control3 -> ATTIVO\n");
			else 
				printf("Stato di Control3 -> FALLITO\n");
			
			printf("\n");
				
			printf("Control action definitiva ->%d\n",final_c);
			
			printf("\n"); 
			
			
		}else
			start = 0; //if you enter something which is not 1

	}while(start);

	rt_named_sem_delete(mutex);
	rt_named_mbx_delete(status);
	rtai_free(DIADS_MEM,&shared);
	
	rt_task_delete(asyncTask);

	return 0;
}
