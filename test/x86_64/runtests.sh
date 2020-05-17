#!/bin/bash

cleanup(){
rm -f *.temp
}

error(){
	echo -e "\e[31mx86_64 Test failed: $1\e[39m"
}

fatal(){
	echo -e "\e[31mx86_64 FATAL Test failed: $1\e[39m"
	exit -1
}

suc(){
	echo -e "\e[32m86_64  OK: $1\e[39m"
}




source ../common/arithmetics.sh
source ../common/variables.sh
source ../common/control_flow.sh
source ../common/misc_commands.sh
source ../common/sequences.sh
source ../common/functions.sh


if [ -n "$(./wadu -f bf1.json -l TRACE 2>&1 | grep -E -o 'executing manager is done')"   ];then
	suc "register editing and control flow manipulation 1"
else	
	fatal "register editing and control flow manipulation 1"
fi


if [ -n "$(./wadu -f bf2.json| grep 'this is string has been inserted')" ];then
	suc "process memory editing"
else	
	fatal "process memory editing"
fi

cleanup
./wadu -f bf3.json 2>&1 >/dev/null
if [ ! -f  out.temp ]  || [[ $(cat out.temp) != "This message was never supposed to be printed" ]];then
	fatal "memory dumping"
else
	suc "memory dumping"
fi

cleanup
./wadu -f bf4.json 2>&1 >/dev/null

if [ ! -f  out.temp ]  || [[ $(cat out.temp) != "largest digit
largest digit
the first letter of the alphabet, but bigger
the first letter of the alphabet, but bigger
Im sorry i dont understand" ]];then
	fatal "data editing+multiple breaks"
else
	suc "data editing+multiple breaks"
fi

if [ -n "$(./wadu -f bf5.json| grep 'mysterious string 4')" ];then
	suc "local variable manipulation"
else	
	fatal "local variable manipulation"
fi

if [ -n "$(./wadu -f bf6.json| grep 'This message was never supposed to be printed')" ];then
	suc "replace function call"
else	
	fatal "replace function call"
fi

cleanup
./wadu -f bf7.json 2>&1 >/dev/null
if [ ! -f  out.temp ]  || [[ $(cat out.temp) != "string number 3
string number 2
the first letter has been repl'd
REPLACED TOO BY THIS MAPPED FILE            
the last  digit
smallestdigit
HOTFIXHOTFIXHOTFIXHOTFIXHOTFIXHOTFIXHOTFIXHOTFIXHOTFIXX
HOTFIXHOTFIXXX
HOTFIXHOTFIXHOTFIXHOTFIXXX" ]];then
	fatal "mapping file to process memory"
else
	suc "mapping file to process memory"
fi

cleanup
./wadu -f bf8.json 2>&1 >/dev/null
if [ ! -f  out.temp ]  || [[ $(cat out.temp) != "string number 3
string number 2
the first letter of the alphabet, but bigger
Im sorry i dont understand" ]];then
	fatal "breakpoint status"
else
	suc "breakpoint status"
fi

cleanup
./wadu -f bf9.json 2>&1 >/dev/null
if [ ! -f  out.temp ]  || [[ $(cat out.temp) != "marks the spot
the first letter of the alphabet, but bigger
marks the spot
the first letter of the alphabet, but bigger
marks the spot
the first letter of the alphabet, but bigger
marks the spot
the first letter of the alphabet, but bigger
marks the spot
the first letter of the alphabet, but bigger
marks the spot
the first letter of the alphabet, but bigger
marks the spot
the first letter of the alphabet, but bigger
marks the spot
the first letter of the alphabet, but bigger
Im sorry i dont understand" ]];then
	fatal "breakpoint enable/disable"
else
	suc "breakpoint enable/disable"
fi

if [ -n "$(./wadu -f bf10.json| grep 'Im sorry i dont understand')"   ];then
	suc "stream closing"
else	
	fatal "stream closing"
fi

cleanup
./wadu -f bf11.json 2>&1 >/dev/null
if [ ! -f  out.temp ]  || [[ $(cat out.temp) != "string number 3
string number 2
marks the spot
marks the spot
marks the spot
marks the spot
marks the spot
Im sorry i dont understand" ]];then
	fatal "writing to stdin"
else
	suc "writing to stdin"
fi

./wadu -e '$p1=U8(5);  while(1)SCRIPT("../common/fib");'  &
wid="$!"
sleep 1
mu=$(ps -p $wid -o %mem|tail -n 1)
echo -n "checking script loading memory management"
for i in $(seq 2 5); do
	current=$(ps -p "$wid" -o %mem|tail -n 1);
	if [[ "$current" != "$mu" ]];then
		kill $wid
		echo ""
		fatal "Memory leak while running scripts expected:$mu, got:$current"
	fi
	echo -n "."
	sleep 1
done

kill $wid 
echo ""
suc "memcheck"

if [ -n "$(./wadu -f bf12.json| grep -E 'RIP: 0x.+817')"   ];then
	suc "on exit handlers"
else	
	fatal "on exit handlers"
fi

if [[ "$(./wadu -f bf13.json)" ==  "managed to finish" ]];then
	suc "segfault handler"
else	
	fatal "segfault handler"
fi


if [[ "$(./wadu -f bf14.json)" != "restarting
restarting
restarting
restarting
restarting" ]];then
	fatal "restarting process"
else
	suc "restarting process"
fi

if [[ "$(./wadu -f bf15.json)" != "the first letter" ]];then
	fatal "On exit process access"
else
	suc "On exit process access"
fi

if [[ "$(./wadu -f bf16.json)" != "IT'S  SUPPOSED TO BE WRITEN" ]];then
	fatal "Polymorphic code"
else
	suc "Polymorphic code"
fi

if [[ "$(./wadu -f bf17.json)" != "restart number 01
restart number 02
restart number 03
restart number 04
restart number 05
restart number 06
restart number 07
restart number 08
restart number 09
restart number 0a
string number 3
string number 2
the first letter of the alphabet, but bigger
the first letter of the alphabet
marks the spot
Im sorry i dont understand" ]];then
	fatal "restart request from interp"
else
	suc "restart request from interp"
fi


out="$(./wadu -f bf18.json)"
if [[ "$(echo "$out"|grep -c 'FROM CHILD' )" != "54" ]] ||  [[ "$(echo "$out"|grep -c 'FROM PARENT' )" != "54" ]] || [[ "$(echo "$out"|grep -c '55' )" != 1 ]]; then
	fatal "child processes"
else
	suc "child processes"
fi

if [[ "$(./wadu -f bf19.json)" != "5" ]];then
	fatal "forked process edition"
else
	suc "forked process edition"
fi


cleanup