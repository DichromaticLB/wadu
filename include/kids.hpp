#ifndef INCLUDE_KIDS_HPP_
#define INCLUDE_KIDS_HPP_
#include<cstdint>
#include<string>
#include<vector>
#include<sys/user.h>
#include<unordered_map>
#include<anjson/anjson.hpp>

namespace wadu{

	using namespace std;
	using word=long int;
	using memaddr=uint64_t;
	using stream_map=std::unordered_map<string,uint32_t>;
	stream_map& getStreams();


	struct linux_memory_segment{
		memaddr begin,end;
		uint64_t offset,inode;
		bool r,w,x,p;
		string device,file;
	};

	struct linux_memory_maps{
		linux_memory_maps()=default;
		linux_memory_maps(const string& file);
		vector<linux_memory_segment> segments;
		linux_memory_segment& getFirstExec(const string& s);
	};

	enum class STOP_SOURCE{
		STOP_SOURCE_INVALID,
		STOP_SOURCE_SIGNAL,
		STOP_SOURCE_UNCAUGHT_SIGNAL,
		STOP_SOURCE_EXIT,
		STOP_SOURCE_CONTINUE //no 1cc???
	};

	struct stopRoot{
		STOP_SOURCE source;
		int status;
		int sigData;

		int signum() const;
		bool syscall() const;
		bool exec() const;
		bool clone() const;
		bool forked() const;
		bool vforked() const;
		bool exited() const;

	};

	struct process_io{
		enum IOTYPE{
			NONE,
			FILE,
			DATA,
			PARENT_IN,
			PARENT_OUT,
			PARENT_ERR
		};

		process_io();
		process_io(const anjson::jsonobject& o);
		process_io(const string& filename);

		static const unordered_map<string,int>& fileFlags();
		static int duped(int original);

		IOTYPE t;
		string data;
		int flags;
	};

	struct communicationPipe{
		~communicationPipe();
		int write=-1;
		int read=-1;
	};

	enum class traceRequest:uint32_t{
		clone=0x1,
		flagSyscall=0x2,
		exec=0x4,
		fork=0x8,
		vfork=0x10,
		killonexit=0x20,
		stopOnExit=0x40

	};

	struct process_child{
		using userData=struct user;

		process_child(const string& s,const vector<string>& args);
		process_child(uint64_t pid);
		void run(
				const process_io&in=process_io(),
				const process_io& out=process_io(),
				const process_io& err=process_io());
		void setTextOffsets(memaddr origin,memaddr at);
		stopRoot wait();
		int resume();
		int detach();

		void signal(int num);
		void processSleep(uint64_t millis) const;
		void stdinCloseWrite();
		void stdinCloseRead();
		void stdinWrite(const uint8_t* data,uint64_t len) const;

		void setOptions(uint32_t i);

		uint64_t forkPid();
		uint64_t vforkPid();
		uint64_t clonePid();


		memaddr origin2real(memaddr mem);
		string command();
		uint64_t id;
		string path;
		communicationPipe in,out,err; //This needs to be released manually!
		vector<string> arguments;

	private:
		memaddr textOrigin,textAt;
	};
	using byteVector=vector<uint8_t>;

	byteVector spawnProcess(
			const string& file,
			const byteVector& stdin,
			const vector<const char*>& args);
	ostream& operator<<(ostream&o,const struct user_regs_struct& u);
	ostream& operator<<(ostream&o,const linux_memory_maps& m);
	ostream& operator<<(ostream&o,const linux_memory_segment& m);
	ostream& operator<<(ostream&o,const stopRoot& s);
}



#endif
