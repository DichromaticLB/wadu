char _b4dc00de[0x100]  __attribute__ ((section (".text")));

char message[]="NEVER SUPPOSED TO BE WRITEN";
/*
 * This test is designed to test the behavior against a simple polymorphic program
 * remember to build with --static -nostdlib  -Wl,--omagic to enable RWX and no libc
 * */
void somecode(char* msg){

	asm("mov %rdi,%rsi");
	asm("mov $1,%rax");
	asm("mov $1,%rdi");
	asm("mov $27,%rdx");
	asm("syscall");

}

void _start()
{
	for(int i=0;i<0x100;i++)
		_b4dc00de[i]=((char*)(&somecode))[i];

	asm("mov $0,%rdi");
	asm("mov $60,%rax");
	asm("syscall");
}
