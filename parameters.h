//---------------- PARAMETERS.H ----------------------- 

#define TICK_TIME 100000000
#define CNTRL_TIME 50000000

#define TASK_PRIORITY 1

#define STACK_SIZE 10000

#define BUF_SIZE 10

#define SEN_SHM 121111
#define ACT_SHM 112112
#define REFSENS 111213
#define SHARED_ID 12345 //memoria dati gather legato a SharedMemory
#define DIADS_MEM 13555 //memoria tra diag e ds legato a diagreq

#define SPACE_SEM 1234444
#define MEAS_SEM 1234445
#define DIADS_SEM "diagdssem"
#define BUF_SEM "buffsem"

#define MAIL_MBX 1235000
#define DSGAT_MBX "MyMailbox"
//#define MAILBOX_ID "MyMailbox"
#define MAILBOX_SIZE 256

typedef struct{
	int buff[BUF_SIZE];  //buffer 	
	unsigned int c1;     //segnali di controllo dei 3 control
	unsigned int c2;
	unsigned int c3;	
	int s1;		     //stati dei task di controllo
	int s2;
	int s3;
	unsigned int final_c;//segnale di controllo del voter
}SharedMemory;

typedef struct{
	int req;
	int res;
}diagreq;
