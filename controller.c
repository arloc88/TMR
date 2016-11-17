/*------------------- CONTROLLER.C ---------------------
Riccio - Tammaro - Paudice

Al controller originale sono state aggiunte due repliche del task control con algorithm diversity (è stato cambiato l'ordine delle istruzioni).
Ogni control_Task calcola il proprio errore, elabora la relativa legge di controllo la invia tramite mailbox al task voter 
e la salva in una memoria condivisa con il task aperiodico gather "gatMem".
Il task VOTER riceve tramite mailbox le leggi di controllo e decide a maggioranza la control action definitiva (anche questa viene salvata
nella memoria condivisa gatMem).
Il task SERVER intercetta una richiesta aperiodica effettuata dal task aperiodico "diag" tramite una memoria condivisa "shared" e per
servirla invoca una funzione con cui viene inviato al task gather un messaggio tramite mailbox per segnalare una richiesta aperiodica e
recuperare lo stato del sistema. 

*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <rtai_lxrt.h>
#include <rtai_shm.h>
#include <rtai_sem.h>
#include <rtai_msg.h>
#include <rtai_mbx.h>
#include <rtai_sched.h>
#include <rtai.h>
#include <sys/io.h>
#include <signal.h>
#include "parameters.h"
#define CPUMAP 0x1

//emulates the controller

static RT_TASK *main_Task;
static RT_TASK *server_Task;
static RT_TASK *read_Task;
static RT_TASK *filter_Task;
static RT_TASK *control_Task;
static RT_TASK *control_Task2;
static RT_TASK *control_Task3;
static RT_TASK *voter_Task;
static RT_TASK *write_Task;
static int keep_on_running = 1;

static pthread_t read_thread;
static pthread_t server_thread;
static pthread_t filter_thread;
static pthread_t control_thread;
static pthread_t control_2_thread;
static pthread_t control_3_thread;
static pthread_t voter_thread;
static pthread_t write_thread;
static RTIME sampl_interv;
static RTIME time1 = 1000000000;
static RTIME time2 = 1000000000;
static RTIME time3 = 1000000000;


int* sensor;
int* actuator;
int* reference;
static SharedMemory *gatMem; //memoria condivisa per memorizzare i dati utili al gather
static diagreq *shared; //memoria condivisa tra diag e DS
int buffer[BUF_SIZE];
int head = 0;
int tail = 0;

// data to be monitored
int avg = 0;
int control =0;

SEM* space_avail;
SEM* meas_avail;
SEM* sem_buf; //semaforo per l'accesso al buffer
SEM* mutex; //semaforo per la sincronizzazione tra diag e DS
MBX* mail;  //mailbox per l'invio dei segnali di controllo  tra i controller ed il voter
MBX* mbx;	//mailbox per invocare il gather da parte del DS


static void endme(int dummy) {keep_on_running = 0;}

//La funzione aperiodic_fun viene invocata dal DS, e tramite la mailbox richiama il gather 
//che invierà i dati al task diag
void aperiodic_fun(void){
	int msg = 1;
	rt_mbx_send(mbx,&msg,sizeof(int));
	printf(" \n  **** Serving aperiodic request ****\n ");
	printf("\n");	
}

static void * server_loop(void * par) {
	
	if (!(server_Task = rt_task_init_schmod(nam2num("SERVER"), 1, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT DS TASK\n");
		exit(1);
	}
	RTIME expected = rt_get_time() + sampl_interv;
	rt_task_make_periodic(server_Task, expected, sampl_interv);
	rt_make_hard_real_time();
	
	while (keep_on_running)
	{
		// DATA ACQUISITION FROM PLANT
		rt_sem_wait(mutex); //waiting on mutex, mutex tra diag e DS, se si vuole fare una richiesta 
		//diag pone a 1 shared->req
		//critical section
		if(shared->req == 1){
			aperiodic_fun(); //lancio aperiodic fun per avvisare il gather
			shared->req = 0;
			shared->res = 1; //richiesta soddisfatta
		}
		//end of critical section
		rt_sem_signal(mutex); //awaking any waiting task
		rt_task_wait_period();
	}
	rt_task_delete(server_Task);
	return 0;
}

static void * acquire_loop(void * par) {
	
	if (!(read_Task = rt_task_init_schmod(nam2num("READER"), 2, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT SENSOR TASK\n");
		exit(1);
	}

	RTIME expected = rt_get_time() + sampl_interv;
	rt_task_make_periodic(read_Task, expected, sampl_interv);
	rt_make_hard_real_time();

	while (keep_on_running)
	{
		// DATA ACQUISITION FROM PLANT
		rt_sem_wait(space_avail);
		rt_sem_wait(sem_buf); //semaforo per l'accesso in scrittura al buffer
		buffer[head] = (*sensor);
		rt_sem_signal(sem_buf);
		int i=0;
		rt_sem_wait(sem_buf);//accesso in lettura al buffer
		
		for (i=0;i<10;i++)
			gatMem->buff[i]=buffer[i];
	
		rt_sem_signal(sem_buf);
		head = (head+1) % BUF_SIZE;
		rt_sem_signal(meas_avail);

		rt_task_wait_period();
	}
	rt_task_delete(read_Task);
	return 0;
}

static void * filter_loop(void * par) {

	if (!(filter_Task = rt_task_init_schmod(nam2num("FILTER"), 3, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT FILTER TASK\n");
		exit(1);
	}

	RTIME expected = rt_get_time() + sampl_interv;
	rt_task_make_periodic(filter_Task, expected, sampl_interv);
	rt_make_hard_real_time();

	int cnt = BUF_SIZE;
	unsigned int sum = 0;
	while (keep_on_running)
	{
		// FILTERING (average)
		rt_sem_wait(meas_avail);
		rt_sem_wait(sem_buf);//accesso in scrittura al buffer
		sum += buffer[tail];
		
		rt_sem_signal(sem_buf);
		tail = (tail+1) % BUF_SIZE;

		rt_sem_signal(space_avail);

		cnt--;

		if (cnt == 0) {
			cnt = BUF_SIZE;
			avg = sum/BUF_SIZE;
			sum = 0;
			// sends the average measure to the controller
			rt_send(control_Task, avg); //invio il risultato al controller1
			rt_send(control_Task2, avg); //invio il risultato al controller2
			rt_send(control_Task3, avg); //invio il risultato al controller3

		}
		rt_task_wait_period();
	}
	rt_task_delete(filter_Task);
	return 0;
}

/*CONTROL TASK 1*/ 

static void * control_loop(void * par) {

	if (!(control_Task = rt_task_init_schmod(nam2num("CTRL1"), 4, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT CONTROL TASK 1\n");
		exit(1);
	}

	RTIME expected = rt_get_time() + sampl_interv;
	rt_task_make_periodic(control_Task, expected, BUF_SIZE*sampl_interv);
	rt_make_hard_real_time();

	unsigned int plant_state = 0;
	int error1 = 0;
	unsigned int control_action = 0;
	while (keep_on_running)
	{
		// receiving the average plant state from the filter
		rt_receive(0, &plant_state);

		// computation of the control law
		error1 = (*reference) - plant_state;
			if (error1 > 0) control_action = 1;
			else if (error1 < 0) control_action = 2;
			else control_action = 3;
		
		rt_mbx_send_timed(mail,&control_action,sizeof(int),time1); //mando la control action al voter
		gatMem->c1=control_action; //memorizzo control action nella memoria condivisa
		
		rt_task_wait_period();

	}
	rt_task_delete(control_Task);
	return 0;
}


//CONTROL TASK 2

static void * control_loop2(void * par) {

	if (!(control_Task2 = rt_task_init_schmod(nam2num("CTRL2"), 5, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT CONTROL TASK 2\n");
		exit(1);
	}

	RTIME expected = rt_get_time() + sampl_interv;
	rt_task_make_periodic(control_Task2, expected, BUF_SIZE*sampl_interv);
	rt_make_hard_real_time();

	unsigned int plant_state = 0;
	int error2 = 0;
	unsigned int control_action = 0;
	while (keep_on_running)
	{
		// receiving the average plant state from the filter
		rt_receive(0, &plant_state);
		// computation of the control law
		error2 = (*reference) - plant_state;
			if (error2 < 0) control_action = 2;
			else if (error2 > 0) control_action = 1;
			else control_action = 3;
		
		rt_mbx_send_timed(mail,&control_action,sizeof(int),time2);//mando la control action al voter
		gatMem->c2=control_action;//memorizzo control action nella memoria condivisa
		rt_task_wait_period();

	}
	rt_task_delete(control_Task2);
	return 0;
}

//CONTROL TASK 3 

static void * control_loop3(void * par) {

	if (!(control_Task3 = rt_task_init_schmod(nam2num("CTRL3"), 6, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT CONTROL TASK 3\n");
		exit(1);
	}

	RTIME expected = rt_get_time() + sampl_interv;
	rt_task_make_periodic(control_Task3, expected, BUF_SIZE*sampl_interv);
	rt_make_hard_real_time();

	unsigned int plant_state = 0;
	int error3 = 0;
	unsigned int control_action = 0;
	
	while (keep_on_running)
	{
		// receiving the average plant state from the filter
		rt_receive(0, &plant_state);

		// computation of the control law
		error3 = (*reference) - plant_state;
			if (error3 == 0) control_action = 3;
			else if (error3 < 0) control_action = 2;
			else control_action = 1;
		
		// PROVE DI FAULT
		 
		//control_action=7; //stuck-at-N
		//rt_sleep(10000000000); //ritardiamo il task di 10 secondi
			
		rt_mbx_send_timed(mail,&control_action,sizeof(int),time3);//mando la control action al voter
		gatMem->c3=control_action;//memorizzo control action nella memoria condivisa
		
		rt_task_wait_period();

	}
	rt_task_delete(control_Task3);
	return 0;
}
//VOTER TASK 

static void * voter_loop(void * par) {

	if (!(voter_Task = rt_task_init_schmod(nam2num("VOTER"), 7, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT VOTER TASK\n");
		exit(1);
	}

	RTIME expected = rt_get_time() + sampl_interv;
	rt_task_make_periodic(voter_Task, expected, BUF_SIZE*sampl_interv);
	rt_make_hard_real_time();
	
	//variabili in cui salvo i dati prelevati dalla mailbox
	unsigned int ca1 = 0;
	unsigned int ca2 = 0;
	unsigned int ca3 = 0;
	unsigned int control_action = 0;
	
	while (keep_on_running)
	{ 

		//ricevo dai task control
		int e1=rt_mbx_receive_timed(mail,&ca1,sizeof(int),time1);
		int e2=rt_mbx_receive_timed(mail,&ca2,sizeof(int),time2);
		int e3=rt_mbx_receive_timed(mail,&ca3,sizeof(int),time3);
		gatMem->s1=e1;
		gatMem->s2=e2;
		gatMem->s3=e3;

              if(e1!=0){ //quando uguale a zero la receive va a buon fine
                        printf(" Ritardo di ctrl1, la decisione verrà presa da ctrl2 e ctrl3  \n");
                        ca1=0; //se la receive fallisce, pongo la control action pari a zero
                        }
                else 
                        printf(" Ctrl1 ha ricevuto : control_action1=%d \n",ca1);
                
                if(e2!=0){
                      printf(" Ritardo di ctrl2, la decisione verrà presa da ctrl1 e ctrl3  \n");
                      ca2=0;
                      }
                else    
                        printf(" Ctrl2 ha ricevuto : control_action2=%d \n",ca2);
                     
                if(e3!=0){
                        printf(" Ritardo di ctrl3, la decisione verrà presa da ctrl1 e ctrl2  \n");
                        ca3=0;
                        }
                else 
                        printf(" Ctrl3 ha ricevuto : control_action3=%d \n",ca3);


		// computation of the control action tramite maggioranza
		
	if(ca1==0 && ca2==0 && ca3==0){
		printf("falliti tutti i task di controllo");
		control_action=3;
		}
	else if(ca1!=0 && ca2!=0 && ca3!=0){
			if((ca1==ca2 && ca1==ca3)||(ca1==ca2 && ca1!=ca3)||(ca1==ca3 && ca1!=ca2))
				control_action=ca1;
			else if ((ca2==ca1 && ca2==ca3)||(ca2==ca1 && ca2!=ca3)||(ca2==ca3 && ca2!=ca1))
				control_action=ca2;
			else if((ca3==ca1 && ca3==ca2)||(ca3==ca1 && ca3!=ca2)||(ca3==ca2 && ca3!=ca1))
				control_action=ca3;
			}
	else if(ca1!=0 && ca2!=0 && ca3==0){
			if((ca1==ca2))
				control_action=ca1;
			else control_action=3;
			}
	else if(ca1!=0 && ca2==0 && ca3!=0){
			if((ca1==ca3))
				control_action=ca1;
			else control_action=3;
			}
	else if(ca1==0 && ca2!=0 && ca3!=0){
			if((ca2==ca3))
				control_action=ca2;
			else control_action=3;
			}
	else if (ca1!=0 && ca2==0 && ca3 ==0)
			control_action=ca1;
	else if (ca1==0 && ca2!=0 && ca3 ==0)
			control_action=ca2;
	else if (ca1==0 && ca2==0 && ca3 !=0)
			control_action=ca3;
	else printf(" La decisione è presa da un solo controllore \n ");

		gatMem->final_c=control_action;
		rt_send(write_Task, control_action); //mando la control action definitiva all'actuator
		rt_task_wait_period();

	}
	rt_task_delete(control_Task);
	return 0;
}


static void * actuator_loop(void * par) {

	if (!(write_Task = rt_task_init_schmod(nam2num("WRITE"), 6, 0, 0, SCHED_FIFO, CPUMAP))) {
		printf("CANNOT INIT ACTUATOR TASK\n");
		exit(1);
	}

	RTIME expected = rt_get_time() + sampl_interv;
	rt_task_make_periodic(write_Task, expected, BUF_SIZE*sampl_interv);
	rt_make_hard_real_time();

	unsigned int control_action = 0;

	while (keep_on_running)
	{
		// receiving the control action from the controller
		rt_receive(0, &control_action);
		
		switch (control_action) {
			case 1: control = 1; break;
			case 2:	control = -1; break;
			case 3:	control = 0; break;
			default: control = 0;
		}
		
		(*actuator) = control;

		rt_task_wait_period();
	}
	rt_task_delete(write_Task);
	return 0;
}

int main(void)
{
	printf("The controller is STARTED!\n");
 	signal(SIGINT, endme);	//The SIGINT signal is sent to a process by its controlling terminal when a user wishes to interrupt the process.
				// This is typically initiated by pressing Control-C

	if (!(main_Task = rt_task_init_schmod(nam2num("MAINTSK"), 0, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT MAIN TASK\n");
		exit(1);
	}

	//attach to data shared with the controller -- init memory
	sensor = rtai_malloc(SEN_SHM, sizeof(int));
	actuator = rtai_malloc(ACT_SHM, sizeof(int));
	reference = rtai_malloc(REFSENS, sizeof(int));
	gatMem = (SharedMemory*)rtai_malloc(SHARED_ID,sizeof(SharedMemory));
	shared = (diagreq*)rtai_malloc(DIADS_MEM,sizeof(diagreq));
	
	(*reference) = 110;
	
	// init mailbox
	mbx = rt_typed_named_mbx_init(DSGAT_MBX,1,FIFO_Q);
	mail = rt_typed_mbx_init(MAIL_MBX,3,FIFO_Q);
	
	// init semafori
	mutex = rt_typed_named_sem_init(DIADS_SEM,1,BIN_SEM | FIFO_Q);
	sem_buf = rt_typed_named_sem_init(BUF_SEM,1,BIN_SEM | FIFO_Q);
	space_avail = rt_typed_sem_init(SPACE_SEM, BUF_SIZE, CNT_SEM | PRIO_Q);
	meas_avail = rt_typed_sem_init(MEAS_SEM, 0, CNT_SEM | PRIO_Q);
	
	if (rt_is_hard_timer_running()) {
		printf("Skip hard real_timer setting...\n");
	} else {
		rt_set_oneshot_mode();
		start_rt_timer(0);
	}

	sampl_interv = nano2count(CNTRL_TIME);
	
	// CONTROL THREADS 
	pthread_create(&server_thread, NULL, server_loop, NULL);
	pthread_create(&read_thread, NULL, acquire_loop, NULL);
	pthread_create(&filter_thread, NULL, filter_loop, NULL);
	pthread_create(&control_thread, NULL, control_loop, NULL);
	pthread_create(&control_2_thread, NULL, control_loop2, NULL);
	pthread_create(&control_3_thread, NULL, control_loop3, NULL);
	pthread_create(&voter_thread, NULL, voter_loop, NULL);
	pthread_create(&write_thread, NULL, actuator_loop, NULL);
		
	rt_spv_RMS(0xF);

	while (keep_on_running) {
		printf("Control: %d\n",(*actuator));
		rt_sleep(1000000000);
	}

    	stop_rt_timer();
    	
	//free memory
	rt_shm_free(SEN_SHM);
	rt_shm_free(ACT_SHM);
	rt_shm_free(REFSENS);
	rtai_free(DIADS_MEM,&shared);
	rtai_free(SHARED_ID,&gatMem);

	//free mailbox
	rt_mbx_delete(mbx);
	rt_mbx_delete(mail);
	
	//free semafori
	rt_named_sem_delete(mutex);
	rt_named_sem_delete(sem_buf);
	rt_sem_delete(meas_avail);
	rt_sem_delete(space_avail);

	rt_task_delete(main_Task);

 	printf("The controller is STOPPED\n");
	return 0;
}




