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