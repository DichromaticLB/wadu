#include<unistd.h>
#include<stdio.h>
#include<string.h>


int doread(){
	switch(getchar())
	{
		case 'a':
			puts("the first letter of the alphabet");
			break;
		case 'A':
			puts("the first letter of the alphabet, but bigger");
			break;
		case '0':
			puts("the first digit");
			break;
		case '9':
			puts("largest digit");
			break;
		case 'F':
			puts("is this a digit or a number? Last sentence may be a lie");
			break;
		case 'X':
			puts("marks the spot");
			break;
		default:
			puts("Im sorry i dont understand");
			return 0;
	}
	return 420;
}

char indata[][5]={"1234","4567","89ab","cdef"};

char* strings[]={
		"string number 1",
		"string number 2",
		"string number 3",
		"mysterious string 4",
		indata[3]
};

int access_strings(unsigned parameter){
	if(parameter>2)
	{
		puts("Trying to overflow? NO");
		return -1;
	}
	puts(strings[parameter]);

	return 420;
}

int neverCalled(){
	puts("This message was never supposed to be printed");
	return (unsigned long)"this string shouldnt be returned";
}

int main()
{
	access_strings(2);
	access_strings(1);
	while(doread());
	return 0;
}


