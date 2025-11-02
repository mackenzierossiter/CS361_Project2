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
    Sem_close ( sharedMemMutex_sem ) ;
    Sem_close ( mutexFactory_sem   ) ;

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
    int   shmid;

    shmKey = ftok ("shmem.h" , 5) ;
    if (shmKey == -1) 
    {
        perror ( "Error produced by ftok shm in factory " ) ;
        fflush(stdout);
        clean_up() ;
    }
    shmflg = S_IRUSR | S_IWUSR ;
    shmid  = Shmget (shmKey, SHMEM_SIZE, shmflg ) ;
    p      = (shData *) Shmat( shmid , NULL , 0 ) ;


    // connect to message queue to supervisor

    // find key of message queue already created by sales
    key_t salesKey;
    char *salesPath = "./sales" ;
    salesKey = ftok (salesPath, 1) ;
    if (salesKey = -1) 
    {
        perror ( "Error produced by ftok mq in factory " ) ;
        fflush(stdout);
        clean_up() ;
    }

    // find sales message queue
    int msgflags = S_IWUSR; // when creating message queue in supervisor, 
                            // supervisor will have read only access?
    int SalesMailboxID = Msgget(salesKey, msgflags);


    // create semaphores
    int semflg = O_RDWR;

    sharedMemMutex_sem = Sem_open2 ("/rossitma_sharedMemMutex_sem" , semflg ) ;
    mutexFactory_sem   = Sem_open2 ("/rossitma_mutexFactory_sem"   , semflg ) ;

    

    // initialize and declare variables needed for loop
    // made need to get rid of cause its in while
    Sem_wait ( sharedMemMutex_sem ) ;
    int remain = p->remain ;
    Sem_post ( sharedMemMutex_sem ) ;

    int partsIMade      = 0 ;
    int totalPartsIMade = 0;
    int iterations      = 0 ;

    // starting item creation loop
    Sem_wait ( sharedMemMutex_sem ) ;
    while ( p->remain > 0 )
    {
        Sem_post ( sharedMemMutex_sem ) ;

        //update remain based on expected products to be made
        if ( remain > capacity )
        {
            
            Sem_wait (sharedMemMutex_sem ) ;
            p->remain -= capacity;
            Sem_post (sharedMemMutex_sem ) ;
            partsIMade = capacity ;

        } else {
            
            Sem_wait (sharedMemMutex_sem ) ;
            partsIMade = p->remain;
            p->remain = 0;
            Sem_post (sharedMemMutex_sem ) ;
            
        }

        // hi
        // does supervisor get the same messages at factory.log?

        // print to factory log
        Sem_wait ( mutexFactory_sem ) ;
        printf( "Factory #%3d: Going to make %5d parts in %4d milliSecs" , factory_ID, capacity, duration) ;
        Sem_post ( mutexFactory_sem ) ;
        fflush(stdout);

        // simulate making items
        int sleep_time = duration * 1000 ;
        usleep( sleep_time ) ;

        
        // creating message buf to send message
        msgBuf msgBufferProd ;
        msgBufferProd.purpose   = PRODUCTION_MSG ;
        msgBufferProd.facID     = factory_ID ;
        msgBufferProd.capacity  = capacity ;
        msgBufferProd.partsMade = partsIMade ;
        msgBufferProd.iteration  = iterations ;
        msgBufferProd.duration  = duration;

        
        // send message to supervisor
        int msgStatus = msgsnd ( SalesMailboxID, &msgBufferProd,  MSG_INFO_SIZE, 0 ) ;


        // how do i keep track of how many items the factory actually made?
        totalPartsIMade += partsIMade ;
        iterations++;



        Sem_wait (sharedMemMutex_sem) ; // needed for while condition
    }

    Sem_post ( sharedMemMutex_sem ) ; // needed to unlock final while condition



    // sending completion message to supervisor
    msgBuf msgBufferComplete ;
    msgBufferComplete.purpose   = COMPLETION_MSG ;
    msgBufferComplete.facID     = factory_ID ;
    msgBufferComplete.capacity  = capacity ;
    msgBufferComplete.partsMade = partsIMade ;
    msgBufferComplete.iteration  = iterations ;
    msgBufferComplete.duration  = duration;

    
    // send message to supervisor
    int msgStatus = msgsnd ( SalesMailboxID, &msgBufferComplete,  MSG_INFO_SIZE, 0 ) ;

    // print go log
    Sem_wait ( mutexFactory_sem ) ;
    printf("Factory Line %d: Completed after making total of %d parts in %d iterations", factory_ID, totalPartsIMade, iterations ) ;
    Sem_post ( mutexFactory_sem ) ;
    fflush(stdout);

    // decrease number of active factories by 1
    Sem_wait (sharedMemMutex_sem ) ;
    p->activeFactories --;
    Sem_post ( sharedMemMutex_sem ) ;

    clean_up() ;

    
}