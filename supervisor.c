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

int activeFactories;

int main (int argc, char *argv[]) 
{
    // //initilize number of factory lines from sales
    // activeFactories = argv[1];

    // //set up message queue

    // key_t salesKey;
    // char *salesPath = "./sales" ;
    // salesKey = ftok (salesPath, 1) ;
    // if (salesKey = -1) 
    // {
    //     perror ( "Error produced by ftok mq in factory " ) ;
    //     clean_up() ;
    // }

    


}