// Mackenzie Rossiter and Gwen Lumsden

// {
// Set up shared memory & initialize its objects ;
// Set up the semaphores & the message queue ;
// Fork / Execute Supervisor process ;
// Fork / Execute all Factory processes ;
// Wait for supervisor to indicate that manufacturing is done
// Grant supervisor permission to print the Final Report ;
// Clean up after zombie processes (Supervisor + all Factories) ;
// Destroy the shared memory ;
// Destroy the semaphores & the message queue ;
// }

// import h files

// from shared mem toy code
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "shmem.h"

// from signals toy code
#include  <signal.h>
#include  <sys/wait.h>

// needed for opening files  
#include <fcntl.h>

// needed for time
#include <time.h>

// needed for signals
#include  <signal.h>


//Wrapper
#include "wrappers.h"



shData    *p;
int        shmid ;

pid_t supervisor_pid;
pid_t *factory_pids;
//shared memory vars
key_t      shmkey;
int        shmflg;
//named semaphore vars
int        semMode;
int        semflg;
sem_t      *factoriesDone_sem,  *printPermission_sem ;
sem_t      *sharedMemMutex_sem, *mutexFactory_sem    ;
//message queue vars
key_t msgkey;
int msgqid;
int msgflags;

//number of factories, global var needed for cleanup?
int num_factories;

// child status for waitpid
int child_status;


#define  STDOUT_FD	1


void clean_up() {
    //send KILL signal to all child processes
    kill(supervisor_pid, SIGKILL);
    for (int i = 0; i < num_factories; i++) {
        kill(factory_pids[i], SIGKILL);
    }

    //clean up semaphores
    Sem_close (factoriesDone_sem)   ;
    Sem_close (printPermission_sem) ;
    Sem_close (sharedMemMutex_sem)       ;
    Sem_close (mutexFactory_sem)    ;

    Sem_unlink ("/lumsdegr_factoriesDone_sem"   ) ;
    Sem_unlink ("/lumsdegr_printPermission_sem" ) ;
    Sem_unlink ("/lumsdegr_sharedMemMutex_sem"       ) ;
    Sem_unlink ("/lumsdegr_mutexFactory_sem"    ) ;


    //remove the shared memory and destroy message queue
    Shmdt(p);
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        printf ("\nP1 Failed to REMOVE shared memory id=%d\n" , shmid );
        perror( "Reason: " );      
        exit( -1 ) ;
    }

    msgctl(msgqid, IPC_RMID, NULL);
    
    //wait
    waitpid(supervisor_pid, &child_status, 0);
    for (int i = 0; i < num_factories; i++) {
        waitpid(factory_pids[i], &child_status, 0);
    }

    //free factory_pid list
    free(factory_pids) ;
}

void sig_handler ( int sig ) {
    fflush ( stdout ) ;
    printf( "\n sales has been ") ;

    switch( sig ) {
        case SIGTERM:
            printf("nicely asked to TERMINATE by SIGTERM ( %d ).\n" , sig ) ;
            break ;
        
        case SIGINT:
            printf("INTERRUPTED by SIGINT ( %d )\n" , sig ) ;
            break ;

        default:
            printf("unexpectedly SIGNALed by ( %d )\n" , sig ) ;
    }
    clean_up() ;
}

int main( int argc , char *argv[] ) {

    // Set num Factories and order_size (arguments)
    num_factories = atoi(argv[1]);

    if (num_factories > MAXFACTORIES) {
        num_factories = MAXFACTORIES;
    }

    int order_size = atoi(argv[2]);

    
    //set up the semaphores & the message queue

    // semaphores needed: 1. all factories are done, 2. permission to print
    //                    3. shared mem semaphore 4. mutex semaphore for factories?
    semflg = O_CREAT | O_EXCL  ;
    semMode = S_IRUSR | S_IWUSR ;

    factoriesDone_sem   = (sem_t *) Sem_open( "/lumsdegr_factoriesDone_sem"   ,      semflg, semMode, 0 ) ;
    printPermission_sem = (sem_t *) Sem_open( "/lumsdegr_printPermission_sem" ,      semflg, semMode, 0 ) ;
    sharedMemMutex_sem  = (sem_t *) Sem_open( "/lumsdegr_sharedMemMutex_sem"  ,      semflg, semMode, 1 ) ;
    mutexFactory_sem    = (sem_t *) Sem_open( "/lumsdegr_mutexFactory_sem"    ,      semflg, semMode, 1 ) ;


    //set up the shared memory & initialize its objects
    shmkey = ftok("shmem.h", 5) ; // was 5 on semaphore lab.  Why?
    shmflg = IPC_CREAT | IPC_EXCL  | S_IRUSR | S_IWUSR ; // should group have read and write?

    shmid = Shmget(shmkey, SHMEM_SIZE, shmflg) ;

    p = Shmat(shmid, NULL, 0) ; 

    
    // lock shared memory semaphore
    Sem_wait ( sharedMemMutex_sem ) ;

    p->order_size = order_size;    p->made = 0;    p->remain = order_size;

    // unlock shared memory semaphore
    Sem_post ( sharedMemMutex_sem ) ;


    //message queue set up

    msgflags = IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IWOTH | S_IWGRP | S_IRGRP | S_IROTH ;
    msgkey = ftok(".", 1);

    msgqid = Msgget(msgkey, msgflags);


    // Fork / Execute Supervisor process ;
    
    // lock print semaphore until ready
    //Sem_wait ( printPermission_sem ) ;

    printf("SALES: Will Request an Order of Size = %d parts\n", order_size);
    fflush(stdout);

    supervisor_pid = Fork();

    if (supervisor_pid == 0) {

        // open supervisor file
        int supervisorFD = open( "supervisor.log" , O_RDWR | O_CREAT | O_EXCL ) ;
        if (supervisorFD == -1) {
            perror("supervisor open failed");
            clean_up();
        }


        //redirect stdout
        if (dup2(supervisorFD, STDOUT_FD) == -1) {
            perror("supervisor dup2 failed");
            clean_up();
        }

        char factories_amount_buf[MAXFACTORIES];
        
        // exec supervisor
        if (execlp("./supervisor", "Supervisor", factories_amount_buf , NULL) == -1) {
            perror("Execlp of Supervisor failed");
            clean_up();
        }
    }

    // making factory_pids a list by dynamically allocating mem?
    factory_pids = malloc(num_factories * sizeof(pid_t));
    if (factory_pids == NULL) {
        perror("malloc failed");
        clean_up();  
    }

    printf("Creating %d Factory(ies)", num_factories);
    for (int i = 0; i < num_factories; i++) {
        pid_t factory_pid = Fork();

        if (factory_pid == 0){
            
            // open factory.log and check if it failed    
            int factoryFD = open( "factory.log", O_RDWR | O_CREAT, 0666 ) ;
            if (factoryFD == -1) {
                perror( "factory open failed" ) ;
                clean_up();
            }

            //redirect stdout
            dup2(  factoryFD, STDOUT_FD );

            srandom(time(NULL));

            int factory_ID = i + 1;
            int capacity = (random() % (50 - 10 + 1) + 10);
            int duration = (random() % (1200 - 500 + 1) + 500);

            //cast to string for exec
            char factory_ID_buf[16], capacity_buf[16], duration_buf[16];
            snprintf(factory_ID_buf, sizeof(factory_ID_buf), "%d", factory_ID);
            snprintf(capacity_buf, sizeof(capacity_buf), "%d", capacity);
            snprintf(duration_buf, sizeof(duration_buf), "%d", duration);

            if (execlp("./factory", "Factory", factory_ID_buf, capacity_buf, duration_buf, NULL) == -1) {
                perror("execlp of Factory failed");
                clean_up();
            }

            printf("SALES: Factory # %4d was created, with Capacity=%4d and Duration=%4d\n", factory_ID, capacity, duration);
            fflush(stdout);
        
        }

        factory_pids[i] = factory_pid;
    }

    //handle abnormal termination
    sigactionWrapper ( SIGINT ,  sig_handler ) ;
    sigactionWrapper ( SIGTERM,  sig_handler ) ;

    //wait for supervisor via rendezvous semaphore
    Sem_wait ( factoriesDone_sem ) ;
    printf("SALES: Supervisor says all Factories have completed their mission\n");
    fflush(stdout);

    //simulate printer by sleeping
    Usleep(2);
    
    //give permission using rendezvous semaphore to the supervisor to print
    Sem_post ( printPermission_sem ) ;
    printf("SALES: Permission granted to print final report\n");
    fflush(stdout);

    //wait for children/clean up
    printf("SALES: Cleaning up after the Supervisor Factory Processes");
    fflush(stdout);
    clean_up();
    
}
