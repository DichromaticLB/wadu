#!/bin/bash

flowtest(){
if [[  $(./wadu -e "$1" 2>&1) != "$2" ]]; then
	fatal "$3"
else
	suc "$3"
fi

}

flowtest 'if(1)PRINT("true loop"."");' 'true loop'  		  'flow test 1' 
flowtest 'if(0)PRINT("true loop"."");' '' 		   			 'flow test 2' 
flowtest '
$i=U8(5);
while($i){
	DUMP(U8(0x41));
	$i=U8($i-1);}' 'AAAAA'  'flow test 3' 

