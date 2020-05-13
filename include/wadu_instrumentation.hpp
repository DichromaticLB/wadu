#ifndef INCLUDE_WADU_INSTRUMENTATION_HPP_
#define INCLUDE_WADU_INSTRUMENTATION_HPP_

#include"kids.hpp"
#include<map>
#include<iostream>
#include<thread>
#include<vector>
#include<map>
#include<functional>
#include<anjson/anjson.hpp>
#include<sys/ptrace.h>

namespace wadu{

	using breakmap=map<memaddr,wadu::word>;
	using addrmap=map<memaddr,memaddr>;
	using breakStatus=map<memaddr,bool>;

	using optionMap=anjson::jsonobject;
	using dataVector=std::vector<uint8_t>;
	using breakStack=vector<vector<memaddr>>;

#ifdef __x86_64__
	using memory_maps=linux_memory_maps;
	using memory_segment=linux_memory_segment;
#elif defined(__aarch64__)
	using memory_maps=linux_memory_maps;
	using memory_segment=linux_memory_segment;
#else
	static_assert(false,"Invalid Architecture");
#endif

	struct breaker;
	struct userRegisters;

	struct breakpointAction{
		enum class action{
			clear_this,
			enable_this,
			clear_address,
			enable_address
		};
		action action;
		memaddr target;
	};
	using brqueue=vector<breakpointAction>;
	struct breaker{

		breaker(process_child& child,memory_segment& m,
				const vector<uint8_t>& trap={});
		wadu::memaddr addBreakpoint(memaddr addr);
		wadu::word 	  textWord(memaddr addr);
		// clears breakpoint and sets rip to the proper value
		void hit();
		void hit(userRegisters&r);
		// restores breakpoint,
		//if we're at the address that would trigger it again steps once
		void restore_from_virtual(memaddr addr);
		void restore_from_segment(memaddr addr);
		void restore_from_virtual(memaddr addr,const userRegisters&r);
		void restore_from_segment(memaddr addr,const userRegisters&r);
		//Clears the breakpoint
		void clear_from_virtual(memaddr addr);
		void clear_from_segment(memaddr addr);
		void clear_all();
		void push_all();
		void restore_all();
		void appyAction(const breakpointAction& act);
		void appyAction(const breakpointAction& act,
				const userRegisters&regs);

		//From address where it's been loaded in memory
		bool isEnabledVirt(memaddr m) const;
		//From file address
		bool isEnabledSeg(memaddr m) const;

		void cont();
		void step();

		word readRel(uint64_t addr);
		void writeRel(uint64_t addr,word d);
		uint32_t breakpointSize() const;

		process_child& child;
		memory_segment &text;
		//Mapping the original contents of text rewritten with breakpoints
		breakmap breakpoints;
		//Mapping file segments to memory segments
		addrmap file2virt;
		//Mapping memory segments to file
		addrmap virt2file;
		//Status of current breakpoints
		breakStatus brstatus;

		breakStack statuses;

		vector<uint8_t> trapInstruction;
	};


	enum class REGISTER_TYPE{
		REGISTER_TYPE_8BIT,
		REGISTER_TYPE_16BIT,
		REGISTER_TYPE_32BIT,
		REGISTER_TYPE_64BIT
	};

	struct regMapping{
		regMapping();
		regMapping(const REGISTER_TYPE& r,void*ad);
		REGISTER_TYPE type;
		void*addr;
	};

	using registerMemoryMap=unordered_map<string,regMapping>;

	struct userRegisters{
		userRegisters();
		userRegisters(const userRegisters& other);
		userRegisters& operator=(const userRegisters& other);
		userRegisters(userRegisters&& other);
		userRegisters& operator=(userRegisters&& other);
		void buildMap();

		struct user_regs_struct user;
		registerMemoryMap mp;
	};


	userRegisters getRegisters(process_child& child);
	int getRegisters(process_child& child,userRegisters& r);
	int setRegisters(const process_child& child,const userRegisters& r);

	memaddr breakPointAt(const userRegisters&r,uint32_t trapSize);
	void writeMemory(process_child& child,
			uint64_t addr,const void*data,uint64_t count);

	dataVector readMemory(process_child& child,uint64_t addr,uint64_t count);

	std::ostream& operator<<(std::ostream&o,const userRegisters& u);
	std::ostream& operator<<(std::ostream&o,const dataVector& u);
}



#endif
