#!/bin/bash

vartest(){
if [[  $(./wadu -e "$1" -e 'PRINT('"$2"',"stdout");' 2>&1) != "$3" ]]; then
	fatal "$4"
else
	suc "$4"
fi

}

vartest '$var1=U64(0xdeadbeefbadc00de);' '$var1' 'deadbeefbadc00de' 'vartest 1'
vartest '$var1=U64(0xdeadbeefbadc00de);'\
'$var1=U32($var1);'  '$var1' 'badc00de' 'vartest 2'
vartest '$name=VECTOR("john"); $last_name=VECTOR("doe");$nick=VECTOR("Johnny");'\
 '$name." ".$last_name." also known as lil".$nick'\
  'john doe also known as lilJohnny'\
  'vartest3'



