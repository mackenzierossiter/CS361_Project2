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


typedef struct {
    int facID;
    int partsBuilt;
    int numIterations;
} per_factory_stats;

void clean_up() {

    Shmdt(p);

    sem_close(factoriesDone_sem);
    sem_close(printPermission_sem);

}


int main (int argc, char *argv[]) 
{
    //initilize number of factory lines from sales
    activeFactories = atoi(argv[1]);
    per_factory_stats stats[activeFactories];
    totalFactories = activeFactories;

    //initialize factory stats
    for (int i = 0; i < activeFactories; i++) {
        stats[i].facID = i + 1;
        stats[i].partsBuilt = 0;
        stats[i].numIterations = 0;
    }

    //access message queue
    key_t salesKey;
    char *salesPath = "./sales" ;
    salesKey = ftok (salesPath, 1) ;
    if (salesKey == -1) 
    {
        perror ( "Error produced by ftok mq in factory " ) ;
        clean_up();
    }

    int msgflgs = S_IRUSR | S_IWUSR | S_IWOTH | S_IWGRP | S_IRGRP | S_IROTH ;
    int msgid = Msgget(salesKey, msgflgs);

    //access sharedmem
    key_t shmkey = ftok("shmem.h", 5) ; // was 5 on semaphore lab.  Why?
    int shmflg = S_IRUSR | S_IWUSR ; // should group have read and write?

    int shmid = Shmget(shmkey, SHMEM_SIZE, shmflg) ;

    p = Shmat(shmid, NULL, 0) ; 

    //initialize rendezvour semaphores
    int semflg = O_RDWR;

    factoriesDone_sem   = Sem_open2( "/rossitma_factoriesDone_sem"   ,      semflg) ;
    printPermission_sem = Sem_open2( "/rossitma_printPermission_sem" ,      semflg) ;

    while (activeFactories > 0) {
        //need to check msgStatus
        msgStatus = msgrcv(msgid, &factoryMsg, MSG_INFO_SIZE, 0, 0);
        if (msgStatus != 0) {
            perror("Failed to Recieve message");
            fflush(stdout);
            clean_up();
        }
        if (factoryMsg.purpose == PRODUCTION_MSG) {
            printf("Factory %d produced %d in %d milliSecs\n", factoryMsg.facID, factoryMsg.partsMade, factoryMsg.duration);
            stats[factoryMsg.facID].partsBuilt = factoryMsg.partsMade;
            stats[factoryMsg.facID].numIterations++;
        } else if (factoryMsg.purpose == COMPLETION_MSG) {
            printf("Factory %d Terminated\n", factoryMsg.facID);
            activeFactories--;
        } else {
            //do nothing i guess??
            printf("Unsupported message");
        }
        fflush(stdout);
    }

    //tell sales factories are done
    sem_post(factoriesDone_sem);
    printf("\nSUPERVISOR: Manufacturing is complete. Awaiting permission to print final report\n");
    //wait for permission to print
    sem_wait(printPermission_sem);

    //Print per-factory production aggregates sorted by Factory ID ;
    printf("\n****** SUPERVISOR: Final Report ******\n");
    for (int i = 0; i < totalFactories; i++) {
        printf("Factory # %d made a total of %3d parts in \t%d iterations\n", stats[i].facID, stats[i].partsBuilt, stats[i].numIterations);
    }

    printf("=======================\n");
    printf("Grand total parts made =\t%d   vs   order size of \t%d\n", p->made, p->order_size);
    printf(">>> Supervisor Terminated");
    fflush(stdout);

    clean_up();

}