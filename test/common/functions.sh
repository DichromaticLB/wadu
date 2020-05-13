#!/bin/bash

if [[ $(./wadu -e 'DEFUN fib(num) {
		if($num<2)
			1;
		else
			fib($num-1)+fib($num-2);
	}' -e 'PRINT("0x".U8(fib(10)));') != '0x59' ]]
then
	fatal "Basic function"
else
	suc "Basic function"
fi

if [[ $(./wadu -e 'DEFUN fib(num) {
		if($num<2)
			1;
		else
			fib($num-1)+fib($num-2);
	}'  -e 'DEFUN fib(num) {
		if($num<2)
			1;
		else
			fib($num-1)+fib($num-2);
	}' 2>&1 ) != 'Fatal error while evaluating dummy, Redefinition of function: fib' ]]
then
	fatal "Redefinition of function not allowed"
else
	suc "Redefinition of function allowed"
fi

if [[ $(./wadu -e 'DEFUN get64(data,index)
			U64(SLICE($data,$index*8,$index*8+8));' \
 -e '$data=VECTOR("abcdefghijklmnopqrtuvwxyz");'\
 -e '$i=U8(0);'\
 -e 'while($i<3){
	PRINT("".VECTOR(get64($data,$i)));
	$i=U8($i+1);}') != "abcdefgh
ijklmnop
qrtuvwxy" ]]
then
	fatal "multiparam fn"
else
	suc "multiparam fn"
fi
