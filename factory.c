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