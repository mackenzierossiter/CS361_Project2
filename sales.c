{
Set up shared memory & initialize its objects ;
Set up the semaphores & the message queue ;
Fork / Execute Supervisor process ;
Fork / Execute all Factory processes ;
Wait for supervisor to indicate that manufacturing is done
Grant supervisor permission to print the Final Report ;
Clean up after zombie processes (Supervisor + all Factories) ;
Destroy the shared memory ;
Destroy the semaphores & the message queue ;
}