#!/bin/bash

if [ -z "$(./wadu -f ../common/empty.json 2>&1| grep 'Required image, arguments and text offset to intialize trace')" ]; then
	error "didnt complain on empty configuration"
else
	suc "empty configuration test"
fi

if [[  $(./wadu -e "PRINT(CONCAT('test'.'123'.0x4142434445464748).'');") != "test123HGFEDCBA" ]]; then
	error "Failed concatenations"
else
	suc "concatenations"
fi

cleanup

./wadu -e \
'FILE("out.temp","temp");' -e \
'DUMP("watchadoing","temp");'

if  [ ! -f  out.temp ]  || [[ $(cat out.temp) != "watchadoing" ]]; then
	error "opening stream"
else
	suc "opening stream"
fi

if [[  $(./wadu -e "DUMP(MAPFROM('../common/ascii.txt'));") != \
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ]]; then
	error "file mapping"
else
	suc "file mapping"
fi

if [[  $(./wadu -e "DUMP(MAPFROM('../common/ascii.txt',10,15));") != \
	"klmno" ]]; then
	error "file mapping slice"
else
	suc "file mapping slice"
fi

if [[  $(./wadu -e "DUMP(MAPFROM('../common/ascii.txt',10,15).MAPFROM('../common/ascii.txt',40,150));") != \
	"klmnoOPQRSTUVWXYZ0123456789" ]]; then
	error "file mapping slice 2"
else
	suc "file mapping slice 2"
fi

if [[  $( ./wadu -e '$p1=U8(20); PRINT(SCRIPT("../common/fib"));') != "1a6d" ]]; then
	error "calling script"
else
	suc "calling script"
fi

if [[  $(./wadu -e 'PRINT(LEN(U8(0))|LEN(U16(0))
		<<8|LEN(U32(0))<<16|LEN(0)<<24|LEN("sixtencharacters")<<32);' )\
		!= "0000001008040201" ]]; then
	error "len checks"
else
	suc "len checks"
fi

if [[  $( ./wadu -e 'if(MEMCMP("test","test"))PRINT("OK"."");' ) != "OK" ]]; then
	error "MEMCMP1"
else
	suc "MEMCMP1"
fi
 

if [[  $( ./wadu -e 'if(MEMCMP("test","tesx"))PRINT("OK"."");' ) == "OK" ]]; then
	error "MEMCMP2"
else
	suc "MEMCMP2"
fi


if [[  $( ./wadu -e 'if(MEMCMP("test","tesx",3))PRINT("OK"."");' ) != "OK" ]]; then
	error "MEMCMP3"
else
	suc "MEMCMP3"
fi

if [[  $( ./wadu -e 'if(MEMCMP("abcdabcdabcdabcd",PATTERN("abcd",4)))
						PRINT("OK"."");' ) != "OK" ]]; then
	error "Generating pattern"
else
	suc "Generating pattern"
fi

if [[  $( ./wadu -e 'PRINT(SYSTEM("cat","call me redford")."");' ) != "call me redford" ]]; then
	error "exev 1"
else
	suc "execv 1"
fi

if [[  $( ./wadu -e 'PRINT(SYSTEM("pwd","","-P")."");' ) != "$(pwd -P)" ]]; then
	error "exev 2"
else
	suc "execv 2"
fi

