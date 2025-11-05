/*
#iterations = #partsMadeByMe = 0;
While ( remain > 0 ) {
Determine how many to make = the smaller of ( remain , myCapacity).
Update remain ;
Print( “Factory #%3d: Going to make %5d parts in %4d milliSecs” , myID, data ... ) ;
sleep ( myDuration in milliseconds ) ; // Same duration even if I make less than my capacity
Create & Send Production message to Supervisor ;
Increment #iterations ;
Update my record of total #of parts I have made so far;
}
Create & Send Completion message to Supervisor ;
Print(“Factory Line %d: Completed after making total of %d parts in %d iterations” , myID, data ... ) ;
*/

// from shared mem toy code
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "shmem.h"

// for message queue
#include <sys/msg.h>

#include "message.h"

// for semaphores
#include <fcntl.h>
#include <semaphore.h>

//Wrapper
#include "wrappers.h"

// global variables needed for cleanup

// shared mem global vars
int shmid;
shData *p;

// semaphores global vars
sem_t *sharedMemMutex_sem ; 
sem_t *mutexFactory_sem ;


void clean_up ()
{

    // remove shared mem
    Shmdt(p);

    // message queue will be removed in sales

    // close semaphores
    Sem_close( sharedMemMutex_sem ) ;
    Sem_close( mutexFactory_sem   ) ;

}


int main (int argc, char *argv[]) 
{
    // initialize and declare passed arguments
    int factory_ID = atoi(argv[1]);
    int capacity   = atoi(argv[2]);
    int duration   = atoi(argv[3]);

    // connect to shared memory
    key_t shmKey;
    int   shmflg;
    //int   shmid;

    shmKey = ftok("shmem.h" , 5) ;
    if (shmKey == -1) 
    {
        perror( "Error produced by ftok shm in factory " ) ;
        fflush(stdout);
        clean_up() ;
    }
    shmflg = S_IRUSR | S_IWUSR ;
    shmid  = Shmget(shmKey, SHMEM_SIZE, shmflg ) ;
    p      = (shData *) Shmat( shmid , NULL , 0 ) ;


    // connect to message queue to supervisor
    // find key of message queue already created by sales
    key_t salesKey;
    salesKey = ftok("message.h", 1) ;
    if (salesKey == -1) 
    {
        perror( "Error produced by ftok mq in factory " ) ;
        fflush(stdout) ;
        clean_up() ;
    }

    // find sales message queue
    int msgflags = S_IWUSR ; // when creating message queue in supervisor, 
                            // supervisor will have read only access?
    int SalesMailboxID = Msgget(salesKey, msgflags) ;


    // create semaphores
    int semflg = O_RDWR ;

    sharedMemMutex_sem = Sem_open2 ("/lumsdegr_sharedMemMutex_sem" , semflg ) ;
    mutexFactory_sem   = Sem_open2 ("/lumsdegr_mutexFactory_sem"   , semflg ) ;

    //start factory process
    Sem_wait ( mutexFactory_sem ) ;
        printf("Factory #%3d: STARTED. My Capacity = %3d, in %4d milliSeconds\n", factory_ID, capacity, duration) ;
        fflush(stdout) ;
    Sem_post ( mutexFactory_sem ) ;

    // initialize and declare variables needed for loop
    // made need to get rid of cause its in while
    Sem_wait ( sharedMemMutex_sem ) ;
    int remain = p->remain ;
    Sem_post ( sharedMemMutex_sem ) ;

    //int partsIMade      = 0 ;
    int totalPartsIMade = 0 ;
    int iterations      = 0 ;

    // starting factory processes
    while ( remain > 0 ) 
    {
        int partsToMake = 0;
        
        //lock sharedmem
        Sem_wait ( sharedMemMutex_sem ) ;
        
        //check sharedmem isnt 0
        if (p->remain <= 0) {
            remain = 0 ; 
            Sem_post ( sharedMemMutex_sem ) ;
            break;  //exit if it is
        }
        //check that what remains is less than max capacity
        if (p->remain >= capacity) {
            partsToMake = capacity ;
        } else {
            partsToMake = p->remain ;
        }

        //update sharedmem vars
        p->remain -= partsToMake ;
        p->made += partsToMake   ;
        remain = p->remain       ;
        
        //unlock mutex
        Sem_post ( sharedMemMutex_sem ) ;


        Sem_wait ( mutexFactory_sem ) ;
        printf( "Factory #%3d: Going to make %5d parts in %4d milliSecs\n", factory_ID, partsToMake, duration ) ;
        fflush(stdout) ;
        Sem_post ( mutexFactory_sem ) ;

        //sleep
        usleep( duration * 1000 );

        //update gloabal vars
        iterations++;
        totalPartsIMade += partsToMake;

        //create msg to send to supervisor
        msgBuf msgBufferProd ;
        msgBufferProd.mtype     = 1 ;
        msgBufferProd.purpose   = PRODUCTION_MSG ;
        msgBufferProd.facID     = factory_ID ;
        msgBufferProd.capacity  = capacity ;
        msgBufferProd.partsMade = partsToMake ;
        msgBufferProd.iteration = iterations ;
        msgBufferProd.duration  = duration ;


        int msgStatus = msgsnd(SalesMailboxID, &msgBufferProd, MSG_INFO_SIZE, 0) ;
        if (msgStatus == -1) {
            perror("msgsnd production message failed");
        }
        
    }

    // sending completion message to supervisor
    msgBuf msgBufferComplete ;
    msgBufferComplete.mtype     = 1 ;
    msgBufferComplete.purpose   = COMPLETION_MSG ;
    msgBufferComplete.facID     = factory_ID ;
    msgBufferComplete.capacity  = capacity ;
    msgBufferComplete.partsMade = totalPartsIMade ;
    msgBufferComplete.iteration = iterations ;
    msgBufferComplete.duration  = duration ;

    
    // send message to supervisor
    int msgStatus = msgsnd ( SalesMailboxID, &msgBufferComplete,  MSG_INFO_SIZE, 0 ) ;

    // print factories done
    Sem_wait ( mutexFactory_sem ) ;
    printf(">>> Factory # %3d: Terminating after making total of %4d parts in %4d iterations\n", factory_ID, totalPartsIMade, iterations ) ;
    fflush(stdout);
    Sem_post ( mutexFactory_sem ) ;
    

    // decrease number of active factories by 1
    Sem_wait (sharedMemMutex_sem ) ;
    p->activeFactories-- ;
    Sem_post ( sharedMemMutex_sem ) ;

    clean_up() ;

    
}