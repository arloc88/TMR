#ifndef HEADERS_H
#define HEADERS_H

#define STACK_SIZE 10000
#define SLEEP_TIME_S 1 
#define BUF_SIZE 10
#define DSGAT_MBX "MyMailbox"
#define STATUS_MBX "status"
#define MAILBOX_SIZE 256
#define SHARED_ID 12345
#define STATUS_MEM 145555
#define BUF_SEM "buffsem"

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

#endif
