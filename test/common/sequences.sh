#!/bin/bash

seq(){
if [[  $(./wadu -e 'SEQUENCE("seq",'"$1"');' -e "$2"  2>&1) != "$3" ]]; then
	error "$4"
else
	suc "$4"
fi
}


seq '{0->10}' 'while(INCREMENT("seq")){
	PRINT(U8(GET("seq",0)));
}' \
'01
02
03
04
05
06
07
08
09' 'sequence test1'

seq '{1->4}{"one","three","two"}' 'while(INCREMENT("seq")){
	PRINT(U8(GET("seq",0)).GET("seq",1));
}' \
'02one
03one
01three
02three
03three
01two
02two
03two' 'sequence test2'

seq '{1->3}{"one","three","two"}{3->5}' 'while(INCREMENT("seq")){
	PRINT(U8(GET("seq",0)).GET("seq",1).U8(GET("seq",2)));
}' \
'02one03
01three03
02three03
01two03
02two03
01one04
02one04
01three04
02three04
01two04
02two04' 'sequence test3'