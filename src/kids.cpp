#include"kids.hpp"
#include<unistd.h>
#include<exception>
#include<stdexcept>
#include<algorithm>
#include<fstream>
#include<sstream>
#include<sys/ptrace.h>
#include<sys/wait.h>
#include<string.h>
#include<regex>
#include<iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<mutex>
#include"util.hpp"
using namespace std;


const unordered_map<string,int>& wadu::process_io::fileFlags(){
	static unordered_map<string,int> f2i={
			{"O_WRONLY",O_WRONLY},
			{"O_CREAT",O_CREAT},
			{"O_APPEND",O_APPEND},
			{"O_RDONLY",O_RDONLY},
			{"O_RDWR",O_RDWR}
	};

	return f2i;
}

int wadu::process_io::duped(int original){
	static unordered_map<int,int> ex;
	static std::mutex dupedLock;
	std::lock_guard g(dupedLock);

	if(!ex.count(original))
		ex.emplace(original,dup(original));

	wlog(LL::DEBUG)<<"duping fd: "+to_string(original);

	return ex.at(original);
}

wadu::communicationPipe::~communicationPipe(){
			if(read!=-1){
				wlog(LL::DEBUG)<<"closing write end of pipe: "+to_string(read);
				close(read);
				read=-1;
			}
			if(write!=-1){
				wlog(LL::DEBUG)<<"closing write end of pipe: "+to_string(write);
				close(write);
				write=-1;
			}
		}

wadu::process_child::process_child::process_child(
		const string& s,const vector<string>& args){
	textAt=textOrigin=0;
	id=0;
	path=s;
	arguments=args;

	wlog l(LL::TRACE);
	l<<"child created: image:"+s+" args:";
	if(l.shouldLog())
		for(auto & arg:args)
			l<<arg<<" ";
}

wadu::process_child::process_child::process_child(uint64_t pid){
	id=pid;
	textAt=textOrigin=0;
}

wadu::process_io::process_io(){
	t=IOTYPE::NONE;
	flags=-1;
}

wadu::process_io::process_io(const anjson::jsonobject& o):process_io(){
	using namespace anjson;
	static const map<string,IOTYPE> s2type={
			{"NONE",      NONE,      },
			{"FILE",      FILE,      },
			{"DATA",      DATA,      },
			{"PARENT_IN", PARENT_IN, },
			{"PARENT_OUT",PARENT_OUT,},
			{"PARENT_ERR",PARENT_ERR }
	};

	if(o.getType()==type::string){
		*this=process_io(o.get<string>());
		return;
	}

	if(o.getType()==type::object){

		if(o.query("\"this\"[\"flags\"]").getType()==type::array){
			flags=0;
			for(auto& flag:o["flags"].get<jsonobject::arrayType>()){
				if(flag.getType()==type::string)
					flags|=fileFlags().at(flag.get<string>());

			}
		}

		if(o.query("\"this\"[\"type\"]").getType()==type::string){
			string mode=o.query("\"this\"[\"type\"]").get<string>();
			t=s2type.at(mode);
		}

		if(o.query("\"this\"[\"data\"]").getType()==type::string){
			string data=o.query("\"this\"[\"data\"]").get<string>();
			this->data=data;
		}

	}
}

wadu::process_io::process_io(const string& filename):process_io(){
	t=IOTYPE::FILE;
	data=filename;
}


#define PAGESIZE ((1l<<13)-1)
void wadu::process_child::setTextOffsets(uint64_t origin,uint64_t at){
	textOrigin=origin;
	textAt=origin&PAGESIZE;
	textAt|=at;

	wlog(LL::TRACE)<<"child: "<<to_string(id)<<" text base memaddr at: "<<hexstr(textAt);
}

std::string wadu::process_child::command(){
	std::stringstream ss;
	if(path.empty())
		ss<<"<unknown>";
	else
		ss<<path;

	for(auto&str:arguments)
		ss<<" "<<str;
	return ss.str();
}

void wadu::process_child::run(const process_io&in,const process_io&out,const process_io&err){

	int iflags=O_RDONLY;
	int oflags=O_WRONLY|O_CREAT;
	int eflags=O_WRONLY|O_CREAT;

	wlog(LL::INFO)<<"begining child execution: "<<path;

	if(in.flags!=-1)
		iflags=in.flags;

	if(out.flags!=-1)
		oflags=out.flags;

	if(err.flags!=-1)
		eflags=err.flags;

	int idup=process_io::duped(STDIN_FILENO);
	int odup=process_io::duped(STDOUT_FILENO);
	int edup=process_io::duped(STDERR_FILENO);
	
	int ipipefiledes[2];

	if(in.t==process_io::DATA){
		if(pipe(ipipefiledes)==-1){
			throw runtime_error("Failed to setup pipes for stdin communication");
		}
		this->in.read =ipipefiledes[0];
		this->in.write=ipipefiledes[1];
		write(this->in.write,in.data.data(),in.data.size());
		wlog(LL::TRACE)<<"setup child input to be data: "<<in.data;
	}

	int pid=fork();

	if(pid==0)
	{

		switch(in.t)
		{
			case process_io::FILE:
			{
				int ifileno=open(in.data.c_str(),iflags,S_IWUSR|S_IRUSR);
				if(ifileno==-1)
				{
					string errmsg="Failed to open "+in.data+" for io as stdin\n";
					write(edup,errmsg.c_str(),errmsg.size());
					exit(-1);
				}
				dup2(ifileno,STDIN_FILENO);
				wlog(LL::DEBUG)<<"successfuly piped file to stdin "<<in.data;
				break;
			}
			case process_io::DATA:
			{
				dup2(this->in.read,STDIN_FILENO);
				stdinCloseRead();
				stdinCloseWrite();
				break;
			}
			case process_io::PARENT_IN:
			{
				dup2(idup,STDIN_FILENO);
				wlog(LL::DEBUG)<<"parent in -> child in";
				break;
			}
			case process_io::NONE:
				break;
			default:
			{
				string errmsg="Invalid mode used for stdin in child process\n";
				write(edup,errmsg.c_str(),errmsg.size());
				exit(-1);
			}
		}

		switch(out.t)
		{
			case process_io::FILE:
			{
				int ofileno=open(out.data.c_str(),oflags,S_IWUSR|S_IRUSR);
				if(ofileno==-1)
				{
					string errmsg="Failed to open "+out.data+" for io as stdout\n";
					write(edup,errmsg.c_str(),errmsg.size());
					exit(-1);
				}
				dup2(ofileno,STDOUT_FILENO);
				wlog(LL::DEBUG)<<"successfuly piped stdout to file "<<in.data;
				break;
			}

			case process_io::PARENT_OUT:
			{
				dup2(odup,STDOUT_FILENO);
				wlog(LL::DEBUG)<<"child out -> parent out";
				break;
			}
			case process_io::PARENT_ERR:
			{
				dup2(edup,STDOUT_FILENO);
				wlog(LL::DEBUG)<<"child out -> parent err";
				break;
			}
			case process_io::NONE:
				break;
			default:
			{
				string errmsg="Invalid mode used for stdout in child process\n";
				write(edup,errmsg.c_str(),errmsg.size());
				exit(-1);
			}
		}

		switch(err.t)
		{
			case process_io::FILE:
			{
				int efileno=open(err.data.c_str(),eflags,S_IWUSR|S_IRUSR);
				if(efileno==-1)
				{
					string errmsg="Failed to open "+out.data+" for io as stderr\n";
					write(edup,errmsg.c_str(),errmsg.size());
					exit(-1);
				}
				dup2(efileno,STDERR_FILENO);
				wlog(LL::DEBUG)<<"successfuly piped stderr to file "<<in.data;
				break;
			}

			case process_io::PARENT_OUT:
			{
				dup2(odup,STDERR_FILENO);
				wlog(LL::DEBUG)<<"child err -> parent out";
				break;
			}
			case process_io::PARENT_ERR:
			{
				dup2(edup,STDERR_FILENO);
				wlog(LL::DEBUG)<<"child err -> parent err";
				break;
			}
			case process_io::NONE:
				break;
			default:
			{
				string errmsg="Invalid mode used for stderr in child process\n";
				write(edup,errmsg.c_str(),errmsg.size());
				exit(-1);
			}
		}

		ptrace(PTRACE_TRACEME,0,0,0);
		vector<const char*> ag(arguments.size()+1,nullptr);

		std::transform(arguments.begin(),arguments.end(),ag.begin(),
				[](const string& s)->const char*{
			return s.c_str();
		});

		if(execve(path.c_str(),(char **)ag.data(),0)==-1)
		{
			string errmsg="Failed to run image "+path+"\n";
			write(edup,errmsg.c_str(),errmsg.size());
			exit(-1);
		}
	}
	else if(pid==-1){
		throw runtime_error("Failed to run "+command());
	}
	else
	{
		if(in.t==process_io::DATA){
			wlog(LL::DEBUG)<<"closing unused read end of pipe";
			stdinCloseRead();
		}

		id=pid;
		wlog(LL::TRACE)<<"new child pid is:"<<id;
		auto w=wait();
		if(w.source!=STOP_SOURCE::STOP_SOURCE_SIGNAL||w.signum()!=SIGTRAP)
			throw runtime_error("Error, didnt receive sigtrap while "
					"waiting child process to execute image:"+path);

		wlog(LL::TRACE)<<"child is ready";

	}
}


istream& operator>>(istream&i,wadu::linux_memory_segment& s){
	string stuff;
	i>>stuff;
	auto sep=stuff.find('-');
	if(sep==string::npos)
		throw runtime_error("Failed to read map from line "+stuff);
	string begin(stuff.c_str(),sep);
	string end(stuff.c_str()+sep+1,stuff.size()-sep);

	s.begin=strtoull(begin.c_str(),0,16);
	s.end=strtoull(end.c_str(),0,16);

	i>>stuff;
	if(!i.good()||stuff.size()!=4)
		throw runtime_error("Failed to read flags for segment ");

	s.r=stuff[0]=='r';
	s.w=stuff[1]=='w';
	s.x=stuff[2]=='x';
	s.p=stuff[3]=='p';

	i>>stuff;

	if(!i.good())
		throw runtime_error("Failed to read file offset");

	s.offset=strtoull(stuff.c_str(),0,0);

	i>>stuff;
	if(!i.good())
		throw runtime_error("Failed to read device");

	s.device=stuff;

	i>>stuff;
	if(!i.good())
		throw runtime_error("Failed to read inode");

	s.inode=strtoull(stuff.c_str(),0,0);

	i>>s.file;
	wadu::wlog(wadu::LL::DEBUG)<<"Segment read "<<s;
	return i;
}

#define SYSCALLMASK 0x80
#define MAXSIGNALMASK 0x1F

#define CLONED(status)  (status>>8 == (SIGTRAP | (PTRACE_EVENT_CLONE<<8)))
#define EXECD(status)   (status>>8 == (SIGTRAP | (PTRACE_EVENT_EXEC<<8)))
#define FORKED(status)  (status>>8 == (SIGTRAP | (PTRACE_EVENT_FORK<<8)))
#define VFORKED(status) (status>>8 == (SIGTRAP | (PTRACE_EVENT_VFORK<<8)))
#define PTRACEXITED(status) (status>>8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8)))

int wadu::stopRoot::signum() const {
	return sigData&MAXSIGNALMASK;
}

bool wadu::stopRoot::syscall() const {
	return sigData&SYSCALLMASK;
}

bool wadu::stopRoot::clone() const {
	return CLONED(status);
}

bool wadu::stopRoot::exec() const {
	return EXECD(status);
}

bool wadu::stopRoot::forked() const {
	return FORKED(status);
}

bool wadu::stopRoot::vforked() const{
	return VFORKED(status);
}

bool wadu::stopRoot::exited() const{
	return PTRACEXITED(status);
}

wadu::linux_memory_maps::linux_memory_maps(const string& file){
	std::ifstream i(file);
	wadu::wlog(wadu::LL::TRACE)<<"Reading segments from: "<<file;
	if(!i.is_open())
		throw runtime_error("Failed to create maps from "
				+file+", unable to open file");

	string line;
	std::getline(i,line);

	while(i.good())
	{
		segments.emplace_back();
		stringstream ss(line);
		ss>>segments.back();
		std::getline(i,line);
	}
}

wadu::linux_memory_segment& wadu::linux_memory_maps::getFirstExec(const string& s){
	for(auto& segment:segments){
		if(segment.x&& segment.file.find(s)!=string::npos)
			return segment;
	}

	throw runtime_error("Failed to find executable segment for file:"+s);
}



wadu::stopRoot wadu::process_child::wait(){

	wadu::wlog(wadu::LL::DEBUG)<<"waiting for child "<<id;

	stopRoot res={STOP_SOURCE::STOP_SOURCE_INVALID,0,0};
	int &status=res.status;

	if(waitpid(id,&status,0)==-1)
		throw runtime_error("Error waiting for pid "+to_string(id));

	if(WIFEXITED(status))
		res.source=STOP_SOURCE::STOP_SOURCE_EXIT;
	else if(WIFSIGNALED(status))
	{
		res.source=STOP_SOURCE::STOP_SOURCE_UNCAUGHT_SIGNAL;
		res.sigData=WTERMSIG(status);

	}
	else if(WIFSTOPPED(status)){
		res.source=STOP_SOURCE::STOP_SOURCE_SIGNAL;
		res.sigData=WSTOPSIG(status);
	}
	else if(WIFCONTINUED(status))
		res.source=STOP_SOURCE::STOP_SOURCE_CONTINUE;

	wadu::wlog(wadu::LL::DEBUG)<<"finish waiting, reason: "<<res;

	return res;
}

int wadu::process_child::resume(){
	return ptrace(PTRACE_CONT,id,0,0);
}

int wadu::process_child::detach(){
	return ptrace(PTRACE_DETACH,id,0,0);
}

void wadu::process_child::signal(int num){
	kill(id,num);
}

void wadu::process_child::processSleep(uint64_t millis) const{
	usleep(millis*1000);
}

void wadu::process_child::stdinCloseWrite(){
	if(in.write!=-1){
		close(in.write);
		in.write=-1;
	}
	else
		throw runtime_error("Invalid stdin close, stdin isnt reading from a fileno");
}

void wadu::process_child::stdinCloseRead(){
	if(in.read!=-1){
		close(in.read);
		in.read=-1;
	}
	else
		throw runtime_error("Invalid stdin close, stdin being written from a fileno");
}

void wadu::process_child::stdinWrite(const uint8_t* data,uint64_t len) const{
	if(in.write!=-1){
		write(in.write,data,len);
	}
	else
		throw runtime_error("Invalid stdin write, stdin isnt reading from a fileno");
}

void wadu::process_child::setOptions(uint32_t i){
	word flags=0;

	if(i&(word)traceRequest::clone)
		flags|=PTRACE_O_TRACECLONE;
	if(i&(word)traceRequest::flagSyscall)
		flags|=PTRACE_O_TRACESYSGOOD;
	if(i&(word)traceRequest::exec)
		flags|=PTRACE_O_TRACEEXEC;
	if(i&(word)traceRequest::fork)
		flags|=PTRACE_O_TRACEFORK;
	if(i&(word)traceRequest::vfork)
		flags|=PTRACE_O_TRACEVFORK;
	if(i&(word)traceRequest::killonexit)
		flags|=PTRACE_O_EXITKILL;
	if(i&(word)traceRequest::stopOnExit)
		flags|=PTRACE_O_TRACEEXIT;

	ptrace(PTRACE_SETOPTIONS,id,0,flags);
	wadu::wlog(wadu::LL::DEBUG)<<"ptrace options set: "<<flags;
}


wadu::memaddr wadu::process_child::origin2real(memaddr mem){
	return textAt+mem-textOrigin;
}


ostream& wadu::operator<<(ostream&o,const wadu::linux_memory_segment& v){
	o<<hex<<v.begin<<"-"<<hex<<v.end<<" r:"<<v.r
			<<" w:"<<v.w<<" x:"<<v.x<<" p:"<<v.p
			<<" "<<v.file;

	return o;
}

ostream& wadu::operator<<(ostream&o,const wadu::stopRoot& s){
	switch(s.source){
	case STOP_SOURCE::STOP_SOURCE_CONTINUE:
		o<<"Continuing";
		break;
	case STOP_SOURCE::STOP_SOURCE_INVALID:
		o<<"[INVALID]";
		break;
	case STOP_SOURCE::STOP_SOURCE_SIGNAL:
		o<<"Stopped due to signal "<<strsignal(s.signum());
		break;
	case STOP_SOURCE::STOP_SOURCE_UNCAUGHT_SIGNAL:
		o<<"Exit from uncaught signal "<<strsignal(s.signum());
		break;
	case STOP_SOURCE::STOP_SOURCE_EXIT:
		o<<"Process exited ";
		break;
	}
	return o;
}

ostream& wadu::operator<<(ostream&o,const linux_memory_maps& m){
	for(auto& v:m.segments)
		o<<v<<endl;

	return o;
}

wadu::byteVector wadu::spawnProcess(
		const string& file,
		const byteVector& stdin,
		const vector<const char*>& args){

	wadu::wlog(wadu::LL::DEBUG)<<"spawing subprocess from interpreter "<<file;

	byteVector res;
	int stdi[2]={-1,-1};
	int stdo[2]={-1,-1};

	string error;
	int pid=-1;
	if(pipe(stdi)==-1||pipe(stdo)==-1)
	{
		error="Failed to create pipes for process";
		goto cleanup;
	}

	write(stdi[1],stdin.data(),stdin.size());
	close(stdi[1]);
	stdi[1]=-1;

	pid=fork();
	if(pid==0)
	{

		dup2(stdi[0],STDIN_FILENO);
		dup2(stdo[1],STDOUT_FILENO);
		if(execvp(file.c_str(),(char**)args.data())==-1)
			exit(-1);

	}
	else if(pid==-1)
	{
		error="Failed to fork process";
		goto cleanup;
	}
	else
	{
		int status,bytesread;
		while(waitpid(pid,&status,0)!=-1);
		close(stdo[1]);
		stdo[1]=-1;

		uint8_t buffer[256];
		while((bytesread=read(stdo[0],buffer,256))>0){
			res.insert(res.end(),buffer,buffer+bytesread);
		}

	}

cleanup:

	if(stdi[0]!=-1)
		close(stdi[0]);

	if(stdi[1]!=-1)
		 (stdi[1]);

	if(stdo[0]!=-1)
		close(stdo[0]);

	if(stdo[1]!=-1)
		close(stdo[1]);

	if(!error.empty())
		throw runtime_error(error);

	wadu::wlog l(wadu::LL::DEBUG);
	if(l.shouldLog()){
		for(auto c:res)
			l<<(char)c;
	}

	return res;
}
