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

// needed for dup2  START HERE include below didnt work
#include <fcntl.h>

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
//message queue vars
key_t msgkey;
int msgqid;
int msgflags;

//number of factories, global var needed for cleanup?
int num_factories;

// child status for waitpid
int child_status;


void clean_up() {
    //send KILL signal to all child processes
    kill(supervisor_pid, SIGKILL);
    for (int i = 0; i < num_factories; i++) {
        kill(factory_pids[i], SIGKILL);
    }

    //clean up semaphores

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
}


int main( int argc , char *argv[] ) {

    //set up the shared memory & initialize its objects
    shmkey = ftok("shmem.h", 5) ; // why is this 5?
    shmflg = IPC_CREAT | IPC_EXCL  | S_IRUSR | S_IWUSR ; // should group have read and write?

    shmid = Shmget(shmkey, SHMEM_SIZE, shmflg) ;

    p = Shmat(shmid, NULL, 0) ; 

    num_factories = atoi(argv[1]);

    if (num_factories > MAXFACTORIES) {
        num_factories = MAXFACTORIES;
    }

    int order_size = atoi(argv[2]);


    p->order_size = order_size;    p->made = 0;    p->remain = order_size;

    //set up the semaphores & the message queue
    //idk what semaphores yet :(

    //message queue set up?

    msgflags = IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IWOTH | S_IWGRP | S_IRGRP | S_IROTH ;
    msgkey = ftok(".", 6); // why is this 6?

    msgqid = Msgget(msgkey, msgflags);

    // Fork / Execute Supervisor process ;
    
    supervisor_pid = Fork();

    if (supervisor_pid == 0) {
        //redirect stdout
        FILE *supervisor_log = fopen("supervisor.log", "w");
        dup2(stdout, "supervisor.log");

        //execute
        execlp("./Supervisor", num_factories , NULL);
    }

    // making factory_pids a list by dynamically allocating mem?
    factory_pids = malloc(num_factories * sizeof(pid_t));
    if (factory_pids == NULL) {
        perror("malloc failed");
        clean_up();  
    }


    for (int i = 0; i < num_factories; i++) {
        pid_t factory_pid = Fork();

        if (factory_pid == 0){
            
            // open factory.log and check if it failed
            FILE *factory_log = fopen("factory.log", "w");
            if (fopen == NULL) {
                perror("fopen failed");
                clean_up();
            }


            //redirect stdout
            dup2(stdout, "factory.log");

            srandom(time(NULL));

            int factory_ID = i;
            int capacity = (random() % (50 - 10 + 1) + 10);
            int duration = (random() % (1200 - 500 + 1) + 500);

            if (execlp("./Factory", factory_ID, capacity, duration, NULL) == -1) {
                perror("execlp of Factory failed");
                clean_up();
            }
        }

        factory_pids[i] = factory_pid;
    }

    //handle abnormal termination
    sigactionWrapper(SIGINT, clean_up());
    sigactionWrapper(SIGTERM, clean_up());

    //wait for supervisor via rendezvous semaphore

    //simulate printer by sleeping
    Usleep(2);

    //give permission using rendezvous semaphore to the supervisor to print

    //wait for children/clean up
    // wait for factory processes
    
    for (int i = 0; i < num_factories; i++) {
        waitpid(factory_pids[i], &child_status, 0);
    }

    // wait for supervisor
    waitpid(supervisor_pid, &child_status, 0);
   
    // clean up
    clean_up();
    
}
