# wadu
RE &amp; binary instrumentation tool 

Wadu is a ptrace based tool for analyzing, fuzzing and reversing self contained pieces of software, originally built to execute scripts within the context of another binary.

Supports Linux(+2.4) on x86_64 or AARCH64 architecture, so far.

### Prerequisites
GNU Flex & Binson and [anjson](https://github.com/DichromaticLB/anjson)

### Installation
Wadu produces an executable whose only requirement at runtime are the c++ stdlib & [anjson](https://github.com/DichromaticLB/anjson) if built to run with shared libraries.

```
make 
```
### Usage 

Wadu works with [JSON](https://en.wikipedia.org/wiki/JSON) files.

A small example which will spawn a process and skip a few instructions whenever the breakpoint is reached

```
{
	"image":"dummy",
	"arguments":["dummy"],
	"textAt":0x600,
	"stdin":{"type":"DATA","data":"input"},
	"stdout":{"type":"PARENT_OUT"},
	"stderr":"/dev/null",
	"commands":[],
	"breakpoints":[
		{
			"address":0x817,
			"commands":[
				"$$rip=$$rip+30;"
			]
		}	
	]
}

```

### A small breakdown of each option

***
<b>image</b>

This is the binary to execute
***
<b>arguments</b>

This is the list of arguments to pass through execve
***
<b>textAt</b>

This is the offset added when calculating addresses based on the memory segments of the process. Modern binaries are loaded at random addresses alligned to page size.

When the process is loaded wadu can determine part of the virtual address from the memory maps but this offset must be provided until this software is integrated with a [Binary File Descriptor](https://en.wikipedia.org/wiki/Binary_File_Descriptor_library) library.

If the executable is not PI you can skip ignore the issue set it to zero, otherwise you generally can set it to
<b>((entry_address)&(PAGESIZE-1))</b> or just hack your way out of this issue :)
***
<b>stdin</b>

stdin redirection
***
<b>stdout</b>

stdout redirection
***
<b>stderr</b>

	
sterr redirection
***
<b>commands</b>

	
Commands to execute once the image begins execution and after each restart

***
<b>breakpoints</b>


Breakpoints and commands to execute 



### Scripting

Wadu uses a pseudo C language so you should expect some of its quirks

The language has 2 data types: unsigned integral type and vector of bytes of arbitrary length.

```
$Integer=0x414243;
$vector="ABCAB\x43";
$Integer8=U8($Integer);
```

```
PRINT("Integer: ".$Integer." vector: ".$vector." Integer(8bit):".$Integer8);
```

> Integer: 0000000000414243 vector: ABCABC Integer(8bit):43

Expect all C operators for logic and arithmetic for operations with integral values.

Integral type defaults to a 64 bit unsigned integer.

***



### Variable Assignment

Variable assignment follow the structure 

$ TOKEN '=' value


```
$somebytes= CONCAT(0x01410242.U16(2+2).'abc');
PRINT($somebytes);
```

> 42 02 41 01 00 00 00 00 
> 04 00 61 62 63




### Manipulating a process

#### For the examples we'll assume we're working with a x86_64 machine.

You can access the GP registers using the <b>$$</b> syntax

> $$ REGISTER_NAME

REGISTER_NAME should be an appropiate name for the current architecture

<b>$$rax, $$rip, $$rsp, $$ebx, $$rdx</b>,etc for x86_64 

<b>$$r1, $$w0, $$pc</b>,etc for aarch64



```
$intruction_ptr=$$rip; /*Saves the value of RIP a variable*/
$$rax=$$rdx*7 +4; /* Sets RAX to seven tmes the value  RDX plus four */
$$eax=5;   /* Sets the lower 32 bits of rax to 0x00000005, the upper 32 remain untouched*/
```


***

You can read memory with <b>MEMREAD</b> and <b>AMEMREAD</b>, both commands use the syntax


> [A]MEMREAD '(' address_to_read ',' number_of_bytes_to_read ')'



```
$top_of_stack=U64(AMEMREAD($$rsp,0x8)); //Reads the u64 pointed by stack pointer register
$data=MEMREAD(0x817,0x1000); //Reads 0x1000 bytes at the relative address 0x817
```



The difference between <b>MEMREAD</b> and <b>AMEMREAD</b> is that  <b>MEMREAD</b> will try to adjust
the values to the address where the binary it's been loaded in memory, 
[PIC executables](https://en.wikipedia.org/wiki/Position-independent_code) are loaded in
distinct addresses each time they're executed. 

Pointers stored in the process memory or registers
are already adjusted and can use <b>AMEMREAD</b> which reads from absolute addresses.


***

Analogous to <b>MEMREAD</b> and <b>AMEMREAD</b> there's also <b>MEMWRITE</b> and <b>AMEMWRITE</a> whose
syntax system is.

> [A]MEMWRITE '(' address_to_write ',' newContents [',' limit ] ')'

If the length of the contents is less than the limit no padding will be added


***
```
$$rsp=$$rsp-8;
AMEMWRITE($$rsp,U64(0xbadc00de));  /* Simulates a push instruction, analogous to PUSH $0x00000000badc00de*/

MEMWRITE(0x817,0x9090909090); /*Replaces whatever is at 0x817 by a 5 NOP instructions, assuming is executable code and the begining of an instruction*/
MEMWRITE(0x817,0x9090909090,2); /*Same as above but limit the ammount of  bytes to write, resulting in only 2 NOP instructions*/
MEMWRITE(0x610,U16(0x414243444546)); /*Replaces 2 bytes at 0x610 by 0x46 0x45*/
```

***
#### Note 

All memory operations will result in vectors of bytes regardless of you reading a valid
integral size.

<i><b> AMEMREAD($$rsp,4) </b></i> produces a 4 byte length vector and not an unsigned integer.


You can overcome this issue with casts:

> U32(AMEMREAD($$rsp,4))

***

### Printing and dumping
There's 2 commands for output,<b>PRINT</b> and <b>DUMP</b>.

Both have identical syntax but different purpose, use PRINT to produce human readable output
and DUMP to dump raw vectors of bytes, analogous to the write syscall.

> (DUMP | PRINT) '(' LIST [ ',' stream ] ')'

A list has the following syntax
LIST: value
	| LIST '.' value

If no stream is provided it defaults to stdout.


***
| COMMAND       | OUTPUT        | 
| ------------- |:-------------:| 
| PRINT(1);     | 0000000000000001 | 
| PRINT("We're currently at address:0x".$$rip );      | We're currently at address:0x0000000000000819| 
| PRINT("AAABBB"); | 41 41 41 42 42 42    | 
| PRINT(MEMREAD($$rip,2),"stderr"); | 19 08    | 
| DUMP(0x0042414141414141); | AAAAAAB |
| DUMP(AMEMREAD($$rsp,0x200).'\x00\x00',"binary"); | <Writes 512 bytes read plus 2 null bytes to stream named binary> |

***
A quirk of the PRINT command is that if it's only given a single vector to print it will generate
an hexdump instead of printing it as a C string.

| COMMAND       | OUTPUT        | 
| ------------- |:-------------:| 
| PRINT(VECTOR(0x2122));     | 22 21 00 00 00 00 00 00 | 
| PRINT("test");     | 74 65 73 74 | 
| PRINT("test"."");     | test | 

***
### Writing to files

You can use the FILE command to create additional streams.

> FILE '(' filename ',' streamName ')'

```
FILE("/tmp/output", "temp1");
DUMP(AMEMREAD($$rsp,0x200),"temp1"); /*Writes the first 0x200 bytes pointed by the RSP to the file "temp1"*/
```

***
### Reading from files

> MAPFROM '(' filename [ from ',' to ] ')'

```
$full_file=MAPFROM("some_bytes"); /* Reads the whole contents of the file named "some_bytes"  */
$part_file=MAPFROM("some_bytes",0x10,0x20); /* Reads the 16 bytes at an offset of 16 bytes from the start of file named "some_bytes" */
MEMWRITE(0x819,MAPFROM("some_bytes")); /*Replaces the contents at rel address 0x819 with the contents of file "some_bytes"*/
```

***
### Control Flow

The language has C like  criteria when testing for truthfulness, generally any non 0 value is considered
truth, boolean operation on vector type results in undefiend behavior

***
#### if

> if '(' value ')' construct \[ else construct ]
	
```
if($$rip & 0x1)
	PRINT("The current address' last bit is 1"."");

if(U8(AMEMREAD($$rsp,1)>0x0f){
	PRINT("16 or more"."");
}
else if(U8(AMEMREAD($$rsp,1)>0x0){
	PRINT("At least is not zero!"."");
}else{
	PRINT("NULL byte"."");
}
```

#### while

> while '(' value ')' construct

```
$counter=0;
while($counter<8) /*Outputs the top 8 u64 stored in the stack */
{
	PRINT("index ".$counter." value: 0x".U64(AMEMREAD($$rsp+$counter*8,8)));
	$counter=$counter+1;
}
```
***
#### do while

> do '{' construct '}' while '(' value ')' ';'

```
$cstr=0;
do { /* Pops bytes from the stack and accumulates them until $cstr is at least 0x10000*/
	$cstr=$cstr+U8(AMEMREAD($$rsp,1)));
	$$rsp=$$rsp+1;
}while($cstr<0x10000);
```
***

### Sequences

Sequences are special constructs of the language to avoid having to write complex combinatorial
logic with variables when trying to fuzz a piece of code.

***

Sequence syntax:
```
SEQUENCE '(' name ',' pattern ')'

pattern: (list | range)
	| pattern (list | range)

range: '{' value '->' value '}'

list: '{' elements '}'

elements: value
	| elements ',' value
```
***
Examples of valid sequence declarations:

```
SEQUENCE("unsigned_shorts",{0 -> 0x10000});
/*
0
1
.
.
0xffff Note that the right end of the range is open, i.e {0 -> 4} = 0,1,2,3
*/
```

```
SEQUENCE("seq1",{0 -> 0x2}{ 0x10 -> $$rax });

/*
0,0
1,0
0,1
1,1
0,2
1,2
.
.
1,$$rax -1  The rax limit is equal to the value the register held when the sequence was created.
			 
*/
```

```
SEQUENCE("Greetings",{"hello ","how are you ","greetings!, "}{"pal","friend number "}{1 -> 3});		
/**
hello pal01
how are you pal01
greetings!, pal01
hello friend number 01
how are you friend number 01
greetings!, friend number 01
hello pal02
how are you pal02
greetings!, pal02
hello friend number 02
how are you friend number 02
greetings!, friend number 02
**/
```

> The commands <b>INCREMENT</b>,<b>GET</b> and <b>RANDOMSEQ</b> manipulate sequences.
***
```
INCREMENT '(' sequence_name ')'
```
generates the next valid sequence and produces a <b>false</b> like value once it loops back to the start.

***
```
GET '(' sequence_name ',' index ')' 
```

retrieves the current's iteration value for a certain index.

***

```
 RANDOMIZE '(' sequence_name ')' 
``` 
generates random combinations but duplicates can happen.


This is quite a mouthful, lets try with an example.
## Sequence example

Lets suppose we encounter a mysterious function call at address <b>0x7c4</b>. 
said function takes 2 arguments in registers: <b>RDI, RSI</b>
an integral value and a pointer to a NULL terminated string.
It returns a pointer to a memory buffer in <b>RAX</b>.
We'll borrow a piece of memory at address <b>0x900</b> for our hijinks. 

***
commands at address 0x7bf(Instruction before our mysterious function call):
```
1)SEQUENCE("fuz",{0->0x100}{"test string 1\x00","test string 2\x00","test string 3\x00"});
2)$backup=MEMREAD(0x900,0x10); //Backup to avoid corrupting memory in case of future use
```
***
commands at address 0x7c4(Instruction where function is called):
```
3)$$rdi=GET("fuz",0); //Replace argument with ours
4)$$rsi=$$rip+(0x900 - 0x7c4); /*Relative addressing*/
5)MEMWRITE(0x900,GET("fuz",1)); //Inject string
```
***
commands at address 0x7c9(Instruction immediately after the function returns):
```
6)PRINT("arg1: ".GET("fuz",0)." arg2: ".GET("fuz",1)." result: ".AMEMREAD($$rax,0x40));
7)if(INCREMENT("fuz")) 
8)	$$rip=$$rip -5; /*Jump back to instruction call wich will trigger the previous breakpoint again with a new set of values*/
else
9)	MEMWRITE(0x900,$backup); /*Restore borrowed memory and let the process carry on*/
```
***

```
                 3
  1              4                                  6
  2              5                                  7                                                         9       
Setup -> Replace arguments & run -> print result and generate next iteration -> repeated? > Yes -> Cleanup and carry on
		 ^						          	    | 					
		 |----------------------------------------------------------------- No 
		        							    8							        
```


## Other commands

#### CONCAT
> CONCAT '(' list ')'

Concatenate the bytes from a series of values into a vector
```
$num=1;
$num2=U16(2);
$str='ababa';
$res=CONCAT($num.$num2.$str);
PRINT($res);
```
outputs:

```
01 00 00 00 00 00 00 00 
02 00 61 62 61 62 61 
```

#### CASTS

Casts reinterpret strings of bytes 
while  discarding or padding bytes as needed.
When promoting null bytes will be used to fill any missing data.
This operation is architecture dependant, particularly depends on the 
[endianess](https://en.wikipedia.org/wiki/Endianness) of the data

> valid casts: U8 U16 U32 U64 VECTOR
```
PRINT(VECTOR(U16(0x41))); /* 41 00 */
PRINT(VECTOR(U64(0))); /* 00 00 00 00 00 00 00 00 */
PRINT(U64('\x32\x21')); /* 0000000000002132  */
PRINT("0x".U64(CONCAT(U32(0x41424344).U32(0x45464748)))); /* 0x4546474841424344  */
PRINT("0x".U64(CONCAT(0x41424344.0x45464748))); /* 0x0000000041424344  */
```

#### SCRIPT

> SCRIPT '(' filename ')' ';'

The script will read from a file and evaluate the commands, if your logic is taking
more than a few lines to implement this is a good alternative to keep it readable.

```
SCRIPT("commands");
```

#### SLICE

Slice allows you extract a section from vector of bytes, left is closed range and right is open, if the begining or
end boundaries are beyond the limits of the value it might return less data than expected.
i.e [from , to) -> [1,4) -> 1,2,3

> SLICE '(' value ',' from ',' to ')'

```
$bytes=SLICE(0x41424344,1,4);
$slice=SLICE('parabole',4,20);
PRINT(U16($bytes)); /* 4243 */
PRINT($slice);  /* bole */
```

#### Breakpoint manipulation

| COMMAND       | OUTPUT        | 
| ------------- |:-------------:| 
|DISABLEBREAK();| Disable the breakpoint from which the commands are being executed | 
|DISABLEBREAK(address);|Disable the breakpoint referenced by address | 
|ENABLEBREAK();|Enable the breakpoint at from which the commands are being executed | 
|ENABLEBREAK(address);|Enable the breakpoint referenced by address | 

These addresses correspond to the values supplied in the configuration file

#### LEN

Returns the length of value

> LEN '(' value ')' 

```
PRINT(LEN("abcdef"));/*6*/
PRINT(LEN(U64(1)));  /*8*/
PRINT(LEN(U32(1)));  /*4*/
PRINT(LEN(U16(1)));  /*2*/
PRINT(LEN(U8(1)));   /*1*/
PRINT(CONCAT(LEN(U32(1)).LEN(U16(1))));  /*6*/
```


#### Indexing

Indexes contents of a variables, if the end of the value lies beyond the end boundary it will be padded with null bytes 
if the begining is the one beyond an error will ocurr.

> value '[' value [ ',' step ] ']' 

```
$number=0x0102030405060708;
$bytes[0]; /* 0x8 */
$bytes[0,3]; /* 0x8 0x7 0x6 */
$bytes[2,3]; /* 0x2 0x1 0x0 */
$bytes[1,5]; /* 0x3 0x2 0x1 0x0 0x0 */
$bytes[2,5]; /* ERROR */
```

#### RANDOM

Generate random integral types from [0 to 0xffffffffffffffff]

| COMMAND       | RANGE        | 
| ------------- |:-------------:| 
|RANDOM();| [0 to 0xffffffffffffffff] | 
|RANDOM(from,to);| [clamp the return value between [from,to) | 

#### MEMCMP

Compares memory, will never go beyond the length of the shortest value, returns true
if none of the compared bytes are different. 

> MEMCMP '(' val ',' val [ ',' max_bytes_to_compare ] ')'

```
MEMCMP(0x41424344,U16(0x4142)); /* true */
MEMCMP(0x41424344,0x414244,2); /* true */
MEMCMP(0x41424344,''); /* true */
MEMCMP('',''); /* true */
MEMCMP(0x41424344,0x414244); /* false */

```

#### SYSTEM

Spawn a subprocess and pipe its output to a vector value

> SYSTEM('program','standard_input' [',' arguments] )
arguments: value
	| value ',' arguments

```
/*  (Deleted newlines)*/
PRINT(SYSTEM('tr',VECTOR(0x410a420a430a440a),'-d','\\n')); /*44 43 42 41*/
/*  (echo back stdin)*/
PRINT(SYSTEM('cat','back at you!').''); /*back at you!*/
```

#### SLEEP

> SLEEP '(' value ')' ';'

Sleep for <value> milliseconds
```
SLEEP(1000); /*sleeps 1 second */
```

#### STDIN

Write to the write end of the STDIN pipe.

Send more data to read by the child.

> STDIN '(' value ')' ';'
 
```
STDIN(AMEMREAD($$rsp,0x20)); /* Send to the child's stdin 0x20 bytes pointed by its own RSP*/ 
```

#### CLOSEIN

By calling CLOSEIN the child will receive an EOF once it tries to read beyond the last byte of input instead
of being left waiting for more input.

```
CLOSEIN();
```

#### SIGNAL

Send a signal to the child

> SIGNAL '(' signum ')' ';'

```
SIGNAL(9); /*SIGKILL child*/
```

#### PATTERN

Create a repeating pattern
> PATTERN '(' value ',' repetitions ')'


```
PATTERN('A',10); /*AAAAAAAAAA*/
PATTERN(PATTERN('AB',2),4); /*ABABABABABABABAB*/
```

#### IFDEF

Test if a variable or function is defined.
Does not apply to sequences.

> IFDEF '(' (TOKEN|value) ')'

```
if(!ISDEF(test))
	PRINT('im undefined'.'');
$test=0;
if(ISDEF(test))
	PRINT('im defined'.'');

```
should output:

```
im undefined
im defined
```

#### STEPTRACE

Enables steping 1 instruction at a time

> STEPTRACE '(' value ')'

This should be used along a handler for SIGTRAP( signal number 5), if no breakpoint is at the address
it will attempt to call de handler for the signal number, if none is defined it will simply exit.
See https://github.com/DichromaticLB/wadu/blob/master/README.md#Signals-object)

```
STEPTRACE(1); /*enables step tracing*/
STEPTRACE(0); /*disabls step tracing*/
```

#### GETC
Calls getchar, effectively pausing further command execution until a key is pressed

> GETC '('  ')'

```
PRINT("VERY IMPORTANT MESSAGE THAT MUST NOT BE DROWNED IN A SEA OF OUTPUT"."");
GETC();
```
#### DEFUN

Function definition

> DEFUN TOKEN '(' tokenlist ')' construct

```
DEFUN fib(num) {
	if($num<2)
		1;
	else
		fib($num-1)+fib($num-2);
	}
	
DEFUN half(number) 
	U8($number/2);	
	
PRINT(U8(fib(5)).' '.half(17));

```
outputs:

```
08 08
```

#### REQUEST

> REQUEST '(' PETITION ')'

> PETITION: EXIT
	| RESTART
	| RESET
	| DETACH
	
| PETITION       | EFFECT        | 
| ------------- |:-------------:| 
|EXIT|Will simply stop the traced process and execute any onExit callbacks it's assigned  | 
|RESTART|Spawns a new process, keeps variables,unction and sequences, breakpoints are reset to their default status  | 
|RESET|Spawns a new process, keeps nothing  | 
|DETACH|Detach the process and leave it running, ideal if you just want to do some monkey patching  | 

```

REQUEST(EXIT);
REQUEST(RESTART);
REQUEST(RESET);
REQUEST(DETACH);
```

***
## Configuration Parameters

| Name       | TYPE          | DESCRIPTION |
| ------------- |:--------------|:------------:| 
|Image|String|Binary image to execute|
|Arguments| Array of strings | Command line parameters to pass to execve|
|textAt|Integral|Offset to add to memory segments |
|stdin |string| File to pipe into input|
|stdout|string|File to redirect stdout to|
|stderr|string|File to redirect stderr to|
|stdin |object|[file io](https://github.com/DichromaticLB/wadu/blob/master/README.md#file-io)|
|stdout|object|[file io](https://github.com/DichromaticLB/wadu/blob/master/README.md#file-io)|
|stderr|object|[file io](https://github.com/DichromaticLB/wadu/blob/master/README.md#file-io)|
|commands|Array of strings| Commands to execute on process start|
|breakpoints|Array ob objects|[Breakpoint object](https://github.com/DichromaticLB/wadu/blob/master/README.md#Breakpoint-array)|
|signals|Array of objects|[Signals object](https://github.com/DichromaticLB/wadu/blob/master/README.md#Signals-object)|
|onexit|object| [On Exit object](https://github.com/DichromaticLB/wadu/blob/master/README.md#Exit-object)|

***

## file io

Besides the typical file redirection there's other possible operations with the standard streams.

| Name          | example       | DESCRIPTION  | stdin        | stdout  | stderr  |
| ------------- |:--------------|:------------:|:------------:|:------------:|:------------:| 
| DATA| "stdin":{"type":"DATA","data":"piped"}| Pipe the contents of data to child's stdin| YES | NO| NO |
|FILE| "stdout":{"data":"filename","type":"FILE"} |Read/Write the the contents of stream from a file|YES|YES|YES|
|PARENT_IN|"stdin":{"type":"PARENT_IN"}|Inherit the stdin from parent|YES|NO|NO|
|PARENT_OUT|"stdout":{"type":"PARENT_OUT"}|Inherit parent stdout and redirect child to it| NO|YES|YES|
|PARENT_ERR|"stdout":{"type":"PARENT_ERR"}|Inherit parent stderr and redirect child to it| NO|YES|YES|


## Breakpoint array
The breakpoint array is composed of a series of objects each representing a possible breakpoint with a series of
commands associated to it.

| param name    | type          | description  | required     |
| ------------- |:--------------|:------------:|:------------:|
| address| integral| The address to set the breakpoint at, as it appears in binary it's loaded from | YES|
| commands| array of strings| A series of strings that will be executed sequentially if breakpoint is hit while active | YES|
| enabled| boolean| Default status of the breakpoint, by default breakpoint are enabled |NO|

***
Example of possible configuration:

```
..
.
"breakpoints":[
	{
		"address":0x817,
		"commands":[
			"PRINT('virtual address :0x'.$$rip.' physical address: 0x817');",
			"REQUEST(DETACH);"
		]
	},
	{
		"address":0x81c,
		"commands":[
			"PRINT('Didnt detach succesfully? Lets segfault happily'.'');",
			"$$rip=0x0;"
		]
	}
]
.
..
```

***

## Signals object

Whenever the child process receives a signal most of them will be propagated first to us, the default behavior in case 
of receiving a signal is to exit.

The structure of is of an array of objects, each describing the behavior for each signal number.

| param name    | type          | description  | required     |
| ------------- |:--------------|:------------:|:------------:|
| signal| integral| The signal number to be handled | YES|
| action| string| Possible actions to take, see [Actions](https://github.com/DichromaticLB/wadu/blob/master/README.md#Actions)| YES|
| commands|array of strings|Identical to its counterpart in breakpoints, a series of commands to execute on signal |NO|
| condition|array of strings|Expressions to evaluate to determine whether a certain action is to be taken, see [Actions](https://github.com/DichromaticLB/wadu/blob/master/README.md#Actions) |DEPENDS ON ACTION|

***

## Actions

Certain objects may take an action option which can trigger a variety of effects

| action name    | description  | notes        |
| ------------- |:--------------|:------------:|
|none |Executes any commands, if any, and continue execution | Commands are executed immediatly after the event triggers|
|exit| Executes any commands if any and then terminates execution|Commands are executed immediatly after the event triggers|
|restart| Restart the execution, keeping variables, functions and sequences|Commands are executed after reseting the process, before the commands array defined in [Configuration Parameters](https://github.com/DichromaticLB/wadu/blob/master/README.md#Configuration-Parameters) |
|reset| Restart the execution, nothing is kept|commands are executed after restarting the process, before the commands array defined in [Configuration Parameters](https://github.com/DichromaticLB/wadu/blob/master/README.md#Configuration-Parameters) |
|restartif| Will evaluate condition parameter and behave as restart if evaluates to true otherwise behaves like exit| Condition becomes a required parameter along action |
|resetif| Will evaluate condition parameter and behave as reset if evaluates to true otherwise behaves like exit| Condition becomes a required parameter along action |

***

## Exit object

The onexit object allows you to stop just before the process exits naturally and examine registers and memory, this behavior is enabled by default.

| param name    | type          | description  | required     |
| ------------- |:--------------|:------------:|:------------:|
| action| string| Possible actions to take, see [Actions](https://github.com/DichromaticLB/wadu/blob/master/README.md#Actions), the action "none" is invalid in this context as exit is inevitable| YES|
| commands|array of strings|Identical to its counterpart in breakpoints, a series of commands to execute on exit |NO|
| condition|array of strings|Expressions to evaluate to determine whether a certain action is to be taken, see [Actions](https://github.com/DichromaticLB/wadu/blob/master/README.md#Actions) |DEPENDS ON ACTION|


***

## Example of possible onexit and signals configuration

```
.
..
"onexit":{
	"action":"reset",
	"commands":["PRINT('Reseting. RIP: 0x'.$$rip);"]
},
"commands":[
	"if(!ISDEF(counter))",
		"$counter=0;"
	],
"signals":[{
	"signal":11,
	"commands":["$counter=U8($counter+1);"],
	"action":"restartif",
	"condition":["$counter<5;"]
}]
..
.
```

# Misc

## Segments

When a process is traced a map of its segments is created in the variable space.

| variable name | description  |
|------------- |:-------------:|
|segments| the number of segments read|
|sbegining_$i| The begining of the segment, replace $i with the index of the segment|
|send_$i| The end of the segment, replace $i with the index of the segment|
|sflags_$i| Flags showing whether the segment is readable/writable/executable, replace $i with the index of the segment|
|segment_source_$i| The name of the file the segment was loaded from, if any replace $i with the index of the segment|

