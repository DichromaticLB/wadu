# wadu
RE &amp; binary instrumentation tool 

Wadu is a ptrace based tool for analyzing, fuzzing and reversing self contained pieces of software, originally built to execute scripts within the context of another binary in execution to gain insight over it.

Supports Linux(+2.4) with x86_64 or AARCH64 architecture

### Prerequisites
GNU Flex & Binson and [anjson](https://github.com/DichromaticLB/anjson)

### Installation
Wadu produces an executable whose only requirement at runtime are the c++ stdlib & [anjson](https://github.com/DichromaticLB/anjson) if built to run with shared libraries.

```
make 
```
### Usage 

Wadu works with [JSON](https://en.wikipedia.org/wiki/JSON) files.

A small example which will spawn a process and skip a few instructions whenever a certain instruction is reached

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

### A small breakdown on each option

```
"image":
```  
This is the binary to execute
```
"arguments":
```
This is the list of arguments to pass through execve
```
"textAt":
```
This is the offset of the text segment from the page size, this software doesnt have
Binary file format parsing and will expect the first executable memory segment from the 
loaded from "image" to be the reference point for all breakpoints and non absolute memory operations
```
"stdin":
```
stdin redirection
```
"stdout":
```
stdout redirection
```
"stderr":
```
sterr redirection
```
"commands":
```
Commands to execute once the image begins execution and after each restart
```
"breakpoints":
```
Breakpoints and commands to execute 


#### io redirection

The standard input, output and error of the process can be manipulated with the options mentioned above

"<standard stream>": mode
  mode: "string" // Referes to the file to read/write 
        | '{' "type" ':' "<mode>"  [, options ] '}'

```
"stdin":{"type":"DATA","data":"SOME TEXT"}
```
Will pipe the string "SOME TEXT" when the child process reads from stdin, the pipe is not closed by default to allow additional input, this option is only available for stdin.
```
"stdin":{"type":"FILE","data":"input.txt"}
"stdin":"input.txt"
```
In both cases will pipe the content of input.txt to the child, as in the shell command <b> cat input.txt | executable  </b>

> "stdin":{"type":"PARENT_IN"}

Will inherit the parent stdin.

> "stdout":{"type":"PARENT_OUT"}

Will inherit the parent stdout and write standard output to it.

> "stderr":{"type":"PARENT_ERR"}

Will inherit the parent stderr and write standard error to it.

### Scripting


