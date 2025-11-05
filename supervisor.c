/*
activeFactories= argv[1] ;
While ( activeFactories> 0 ) {
Receive a message from the messgaeQueue ;
If ( Production Message )
Print( “Factory xx produced nn parts in mm milliSecs” ) ;
Update per-factory production aggregates (num-parts-built, num-iterations) ;
Else if ( Termination Message )
Print( “Factory xx Terminated” ) ;
activeFactories-- ;
else
discard any unsupported message
}
Inform the Sales that manufacturing is done ;
Wait for permission from Sales to print final report ;
Print per-factory production aggregates sorted by Factory ID ;
*/

// from shared mem toy code
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "shmem.h"

// for message queue
#include <sys/msg.h>

#include "message.h"

// for semaphores
#include <fcntl.h>
#include <semaphore.h>

//Wrapper
#include "wrappers.h"

int activeFactories;
int msgStatus;
msgBuf factoryMsg;
// semaphores global vars
sem_t *factoriesDone_sem ; 
sem_t *printPermission_sem ;
shData *p;
int totalFactories;

//struct for keeping track of stats for each factory
typedef struct {
    int facID         ;
    int partsBuilt    ;
    int numIterations ;
} per_factory_stats   ;

void clean_up() {

    Shmdt(p) ;

    sem_close(factoriesDone_sem)   ;
    sem_close(printPermission_sem) ;

}


int main (int argc, char *argv[]) 
{
    //initilize number of factory lines from sales
    activeFactories = atoi(argv[1]) ;
    per_factory_stats stats[activeFactories] ;
    totalFactories = activeFactories ;

    //initialize factory stats
    for (int i = 0; i < activeFactories; i++) {
        stats[i].facID = i + 1     ;
        stats[i].partsBuilt = 0    ;
        stats[i].numIterations = 0 ;
    }

    //access message queue
    key_t salesKey;
    salesKey = ftok ("message.h", 1) ;
    if (salesKey == -1) 
    {
        perror ( "Error produced by ftok mq in factory " ) ;
        clean_up();
    }

    int msgflgs = S_IRUSR | S_IWUSR | S_IWOTH | S_IWGRP | S_IRGRP | S_IROTH ;
    int msgid = Msgget(salesKey, msgflgs) ;

    //access sharedmem
    key_t shmkey = ftok("shmem.h", 5) ;
    int shmflg = S_IRUSR | S_IWUSR ;

    int shmid = Shmget(shmkey, SHMEM_SIZE, shmflg) ;

    p = Shmat(shmid, NULL, 0) ; 

    //initialize rendezvour semaphores
    int semflg = O_RDWR;

    factoriesDone_sem   = Sem_open2( "/lumsdegr_factoriesDone_sem"   ,      semflg) ;
    printPermission_sem = Sem_open2( "/lumsdegr_printPermission_sem" ,      semflg) ;

    //start receiving messages
    printf("SUPERVISOR: Started\n") ;

    while (activeFactories > 0) {
        //need to check msgStatus
        msgStatus = msgrcv( msgid, &factoryMsg, MSG_INFO_SIZE, 0, 0 ) ;
        if (msgStatus == -1) {
            perror("Failed to Recieve message");
            fflush(stdout);
            clean_up();
        }

        //handle production and complete messages
        if (factoryMsg.purpose == PRODUCTION_MSG) {
            printf("SUPERVISOR: Factory #%3d produced %4d in %5d milliSecs\n", factoryMsg.facID, factoryMsg.partsMade, factoryMsg.duration) ;
            stats[factoryMsg.facID - 1].partsBuilt += factoryMsg.partsMade ;
            stats[factoryMsg.facID - 1].numIterations++ ;
        } else if (factoryMsg.purpose == COMPLETION_MSG) {
            printf("SUPERVISOR: Factory #%3d\tCOMPLETED its task\n", factoryMsg.facID) ;
            activeFactories-- ;
        } else {
            //do nothing i guess??
            printf("Unsupported message") ;
        }
        fflush(stdout) ;
    }

    //tell sales factories are done
    sem_post(factoriesDone_sem) ;
    printf("\nSUPERVISOR: Manufacturing is complete. Awaiting permission to print final report\n") ;
    fflush(stdout) ;
    //wait for permission to print
    sem_wait(printPermission_sem) ;

    //Print per-factory production aggregates sorted by Factory ID ;
    printf("\n****** SUPERVISOR: Final Report ******\n") ;
    for (int i = 0; i < totalFactories; i++) {
        printf("Factory # %d made a total of %3d parts in \t%d iterations\n", stats[i].facID, stats[i].partsBuilt, stats[i].numIterations) ;
        fflush(stdout) ;
    }

    printf("=======================\n") ;
    printf("Grand total parts made =\t%d   vs   order size of \t%d\n", p->made, p->order_size) ;
    printf(">>> Supervisor Terminated\n") ;
    fflush(stdout) ;

    clean_up() ;

}