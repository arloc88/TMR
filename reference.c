//---------------------- REFERENCE.C ----------------------------

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <rtai_shm.h>
#include "parameters.h"

 
// SETS THE REFERENCE SIGNAL

 
int main (int argc, char ** argv)

{

    if (argc != 2) {	//controlla se ho inserito qualcosa oltre al nome del programma "./reference val" altrimenti mi printa l'errore Usage:...
	printf("Usage: reference <val>\n");
	return -1;
    }

    int reference = atoi(argv[1]);//prende il secondo element di argv e lo converte da stringa ad intero

    printf("Reference set to: %d\n",reference);

    int *ref_sensor; // creo un puntatore per poterlo utilizzare nella memoria condivisa col valore dato a reference

    ref_sensor = rtai_malloc (REFSENS,1); //alloco un segmento di memoria condivisa

    (*ref_sensor) = reference; //assegno al valore puntato da ref_sensor il valore di reference

    rtai_free (REFSENS, &data); //libero la memoria

    return 0;

}
