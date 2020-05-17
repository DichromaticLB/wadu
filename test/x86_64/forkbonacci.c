#include<unistd.h>
#include<sys/wait.h>
#include<stdio.h>
#include<stdlib.h>

int fib(int n){

        if(n<3)
                return 1;

        int p[2],pid;
        pipe(p);
        if(!(pid=fork()))
        {
                int e=fib(n-1)+fib(n-2);
                write(p[1],&e,sizeof(e));
                exit(0);
        }

        int res;
        waitpid(pid,&res,0);
        read(p[0],&res,sizeof(res));
        close(p[0]);
        close(p[1]);
        return res;

}
//1 1 2 3 5 8 13 21
int main(int argc,char **argv){
	int calculate=5;
	if(argc>1)
	{
		calculate=atoi(argv[1]);
		if(calculate>25)
			calculate=5;
	}
	printf("%d\n",fib(calculate));


}
