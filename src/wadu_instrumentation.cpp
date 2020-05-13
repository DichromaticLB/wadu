#include"wadu_interp.hpp"

#include<elf.h>
#include<bits/types/struct_iovec.h>
#include<sys/ptrace.h>
#include<string.h>
#include<functional>
#include<type_traits>
#include"util.hpp"
#include<thread>
#include<signal.h>
#include"util.hpp"
using namespace std;

wadu::breaker::breaker(process_child& c,memory_segment& m,const vector<uint8_t>& t)
	:child(c),text(m){

#ifdef __x86_64__
static uint8_t trap[]={0xCC};
#elif defined(__aarch64__)
static uint8_t trap[]={0x00,0x7d,0x20,0xd4};
#else
	static_assert(false,"Invalid Architecture");
#endif

	if(t.empty())
	{
		trapInstruction.insert(trapInstruction.end(),
				trap,
				trap+sizeof(trap));
		wlog(LL::DEBUG)<<"using default trap instruction";
	}
	else
	{
		trapInstruction=t;
		wlog(LL::DEBUG)<<"using custom trap instruction";
	}
}

wadu::word wadu::breaker::textWord(memaddr addr){
	wadu::memaddr vaddr=child.origin2real(addr);

	wadu::word original=ptrace(PTRACE_PEEKDATA,child.id,
			vaddr,0);
	//If we cross an already existing breakpoint we to recover the original contents
	for(unsigned i=1;i<sizeof(word);i++){
		if(breakpoints.count(vaddr+i)){
			word rest=breakpoints.at(vaddr+i);
			uint8_t *m=(uint8_t*)&original;
			uint8_t *t=(uint8_t*)&rest;
			memcpy(m+i,t,sizeof(word)-i);
			break;
		}
	}

	return original;
}


wadu::memaddr wadu::breaker::addBreakpoint(memaddr addr){
	wadu::memaddr vaddr=child.origin2real(addr);
	if(!isEnabledVirt(vaddr)){
		file2virt[addr]=vaddr;
		virt2file[vaddr]=addr;
		brstatus[vaddr]=true;
		wadu::word original=ptrace(PTRACE_PEEKDATA,child.id,
					vaddr,0);
		breakpoints[vaddr]=original;
		memcpy(&original,trapInstruction.data(),trapInstruction.size());
		if(ptrace(PTRACE_POKETEXT,child.id,vaddr,original)==-1)
				throw runtime_error("Failed to set breakpoint at 0x"+hexstr(addr));

		wlog(LL::DEBUG)<<"enabling breakpoint at:"<<hexstr(addr);
	}

	return vaddr;
}

void wadu::breaker::hit(){
	auto regs=getRegisters(child);
	hit(regs);
}

void  wadu::breaker::hit(userRegisters&regs){

	memaddr addr=breakPointAt(regs,trapInstruction.size());
	clear_from_virtual(addr);

#ifdef __x86_64__
	regs.user.rip=addr;
	setRegisters(child,regs);
#elif defined(__aarch64__)

#else
	static_assert(false,"Invalid Architecture");
#endif

}

void wadu::breaker::clear_from_virtual(memaddr addr){

	if(isEnabledVirt(addr))
	{
		wadu::word original=ptrace(PTRACE_PEEKDATA,child.id,addr,0);
		memcpy(&original,&breakpoints.at(addr),trapInstruction.size());

		if(ptrace(PTRACE_POKETEXT,child.id,addr,original)==-1)
				throw runtime_error("Failed to clear breakpoint at "+to_string(addr));

		brstatus[addr]=false;
		wlog(LL::DEBUG)<<"clearing breakpoint at:"<<hexstr(addr);
	}
}

void wadu::breaker::clear_all(){
	wlog(LL::DEBUG)<<"starting to clear all breakpoints";
	for(auto& v:brstatus)
		clear_from_virtual(v.first);
}

void wadu::breaker::push_all(){
	statuses.emplace_back();

	for(auto& v:brstatus)
		if(v.second)
			clear_from_virtual(v.first);
}

void wadu::breaker::restore_all(){

	if(statuses.size())
		for(auto& v:statuses.back())
			addBreakpoint(v);

	statuses.pop_back();
}

void wadu::breaker::clear_from_segment(memaddr addr){
	clear_from_virtual(file2virt.at(addr));
}

void wadu::breaker::appyAction(
		const breakpointAction& act){

	switch(act.action)
	{

		case breakpointAction::action::clear_address:
		case breakpointAction::action::clear_this:
		{
			clear_from_segment(act.target);
			break;
		}
		//No sense to enable a breakpoint we're currently at
		case breakpointAction::action::enable_this:
		case breakpointAction::action::enable_address:
		{
			if(!isEnabledSeg(act.target))
			{
				auto regs=getRegisters(child);
				restore_from_segment(act.target,regs);
			}
			break;
		}
	}
}

void wadu::breaker::appyAction(
		const breakpointAction& act,
		const userRegisters&regs){

	switch(act.action)
	{

		case breakpointAction::action::clear_address:
		case breakpointAction::action::clear_this:
		{
			clear_from_segment(act.target);
			break;
		}
		//No sense to enable a breakpoint we're currently at
		case breakpointAction::action::enable_this:
		case breakpointAction::action::enable_address:
		{
			addBreakpoint(act.target);
			break;
		}
	}
}

bool wadu::breaker::isEnabledVirt(memaddr m) const{
	return  brstatus.count(m)&&brstatus.at(m);
}

bool wadu::breaker::isEnabledSeg(memaddr m) const{
	return isEnabledSeg(file2virt.at(m));
}

void wadu::breaker::restore_from_virtual(memaddr addr,const userRegisters&regs){

	if(!isEnabledVirt(addr))
	{
		wadu::word original=ptrace(PTRACE_PEEKDATA,child.id,addr,0);

	#ifdef __x86_64__
		if(regs.user.rip==addr)
	#elif defined(__aarch64__)
		if(regs.user.pc==addr)
	#else
		static_assert(false,"Invalid Architecture");
	#endif
		{
			step();
			auto trap=child.wait();
			if(trap.source!=STOP_SOURCE::STOP_SOURCE_SIGNAL||trap.signum()!=SIGTRAP)
					throw runtime_error("Failed waiting to clear breakpoint: "+hexstr(addr));

		}

		memcpy(&original,trapInstruction.data(),trapInstruction.size());

		if(ptrace(PTRACE_POKETEXT,child.id,addr,original)==-1)
			throw runtime_error("Failed to restore breakpoint at "+hexstr(addr));

		brstatus[addr]=true;
		wlog(LL::DEBUG)<<"enabling breakpoint at:"<<hexstr(addr);
	}
}

void wadu::breaker::restore_from_segment(memaddr addr,const userRegisters&regs){
	restore_from_virtual(file2virt.at(addr),regs);
}

void wadu::breaker::restore_from_virtual(memaddr addr){
	auto regs=getRegisters(child);
	restore_from_virtual(addr,regs);
}

void wadu::breaker::restore_from_segment(memaddr addr){
	auto regs=getRegisters(child);
	restore_from_segment(addr,regs);
}


void  wadu::breaker::cont(){
	ptrace(PTRACE_CONT,child.id,0,0);

}

void wadu::breaker::step(){
	ptrace(PTRACE_SINGLESTEP,child.id,0,0);
}



wadu::word wadu::breaker::readRel(uint64_t addr){
	return ptrace(PTRACE_PEEKDATA,child.id,
			child.origin2real(addr),0);
}

void wadu::breaker::writeRel(uint64_t addr,wadu::word d){
	if(ptrace(PTRACE_POKEDATA,child.id,
			child.origin2real(addr),d))
		throw runtime_error("Failed to write to "+to_string(addr));
}

uint32_t wadu::breaker::breakpointSize() const{
	return trapInstruction.size();
}

wadu::userRegisters wadu::getRegisters(process_child& child){
	wadu::userRegisters r;
	getRegisters(child,r);
	return r;
}

int wadu::getRegisters(process_child& child,userRegisters& r){

#ifdef __x86_64__
	return ptrace(PTRACE_GETREGS,child.id,0,&r.user);
#elif defined(__aarch64__)
	struct iovec v={
			.iov_base=&r.user,
			.iov_len=sizeof(r.user)
	};

	return ptrace(PTRACE_GETREGSET,child.id,NT_PRSTATUS,&v);
#else
	static_assert(false,"Invalid Architecture");
#endif
}

int wadu::setRegisters(const process_child& child,const userRegisters& r){
#ifdef __x86_64__
	return ptrace(PTRACE_SETREGS,child.id,0,&r.user);
#elif defined(__aarch64__)
	struct iovec v={
			.iov_base=(void*)&r.user,
			.iov_len=sizeof(r.user)
	};

	return ptrace(PTRACE_SETREGSET,child.id,NT_PRSTATUS,&v);
#else
	static_assert(false,"Invalid Architecture");
#endif

}

void wadu::writeMemory(process_child& child,uint64_t addr,const void*data,uint64_t count){
	uint32_t i=0;
	for(;i+sizeof(wadu::word)<count;i+=sizeof(wadu::word)){
		ptrace(PTRACE_POKEDATA,child.id,addr+i,*(wadu::word*)(((uint8_t*)data)+i));
	}

	wadu::word w=ptrace(PTRACE_PEEKDATA,child.id,addr+i,0);
	memcpy(&w,((uint8_t*)data)+i,count-i);
	ptrace(PTRACE_POKEDATA,child.id,addr+i,w);
	wlog(LL::DEBUG)<<"writing memory 0x"<<hexstr(addr)<<" count:"<<count;
}


wadu::memaddr wadu::breakPointAt(const userRegisters&r,uint32_t trapSize){
#ifdef __x86_64__
	return r.user.rip-trapSize;
#elif defined(__aarch64__)
	return r.user.pc;
#else
	static_assert(false,"Invalid Architecture");
#endif
}

wadu::dataVector wadu::readMemory(process_child& child,uint64_t addr,uint64_t count){
	wadu::dataVector res(count,0);
	uint32_t i=0;
	wadu::word w;
	for(;i+sizeof(wadu::word)<count;i+=sizeof(wadu::word)){
		w=ptrace(PTRACE_PEEKDATA,child.id,addr+i,0);
		memcpy(res.data()+i,&w,sizeof(wadu::word));
	}
	w=ptrace(PTRACE_PEEKDATA,child.id,addr+i,0);
	memcpy(res.data()+i,&w,count-i);
	wlog(LL::DEBUG)<<"reading memory 0x"<<hexstr(addr)<<" count:"<<count;
	return res;
}




wadu::scheduleRequestType wadu::parseScheduleRequestType(const string&s){
	if(s=="none")
		return scheduleRequestType::none;
	else if(s=="exit")
			return scheduleRequestType::exit;
	else if(s=="reset")
			return scheduleRequestType::reset;
	else if(s=="restart")
			return scheduleRequestType::restart;
	else if(s=="resetif")
			return scheduleRequestType::resetif;
	else if(s=="restartif")
			return scheduleRequestType::restartif;

	throw runtime_error("Couldnt parse schedule Request type from: "+s);

}

const wadu::exprfunction& wadu::funmap::operator[](const string&s) const{
	if(!fns.count(s))
		throw runtime_error("Unknown function: "+s);
	return fns.at(s);
}

void wadu::funmap::put(
		const string&name,
		const expression& params,
		const expression&exec){

	wlog(LL::DEBUG)<<"Function craeted: "<<name<<" "<<params.lt.size()<<" parameters";

	if(fns.count(name))
		throw runtime_error("Redefinition of function: "+name);

	auto& fn=fns[name];
	for(unsigned i=0;i<params.lt.size();i++)
		fn.parametersNames.emplace_back(params.asString(i));
	fn.code=exec;

}

wadu::scheduleRequest::scheduleRequest(){
	t=scheduleRequestType::exit;
}

wadu::scheduleRequest::scheduleRequest(const optionMap&m):scheduleRequest(){
	using namespace anjson;

	if(m.containsKeyType("action",type::string))
	{
		this->t=parseScheduleRequestType(m["action"].get<string>());
	}

	if(m.containsKey("condition"))
	{
		condition=expression::buildFromJSON(m["condition"]);
		if((t==scheduleRequestType::resetif||t==scheduleRequestType::restartif)&&
				!condition.lt.size())
			throw runtime_error("empty condition with a resetif/restartif clause");

	}

	if(m.containsKey("commands"))
	{
		commands=expression::buildFromJSON(m["commands"]);
	}
}


wadu::regMapping::regMapping():type(REGISTER_TYPE::REGISTER_TYPE_8BIT){
	addr=nullptr;
}

wadu::regMapping::regMapping(const REGISTER_TYPE& r,void*ad){
	type=r;
	addr=ad;
}

wadu::userRegisters::userRegisters(){
	buildMap();
}

wadu::userRegisters::userRegisters(const userRegisters& other){
	user=other.user;
	buildMap();
}

wadu::userRegisters::userRegisters(userRegisters&& other){
	user=other.user;
	buildMap();
}

wadu::userRegisters& wadu::userRegisters::operator=(wadu::userRegisters&& other){
	user=other.user;
	buildMap();
	return *this;
}

wadu::userRegisters& wadu::userRegisters::operator=(const wadu::userRegisters& other){
	user=other.user;
	buildMap();
	return *this;
}

void wadu::userRegisters::buildMap(){
#define SET64(_REG,_64,_32,_16,_8H,_8L) 	do{ \
	this->mp[_64]=regMapping(REGISTER_TYPE::REGISTER_TYPE_64BIT, &user._REG);\
	this->mp[_32]=regMapping(REGISTER_TYPE::REGISTER_TYPE_32BIT, &user._REG);\
	this->mp[_16]=regMapping(REGISTER_TYPE::REGISTER_TYPE_16BIT, &user._REG);\
	this->mp[_8H]=regMapping(REGISTER_TYPE::REGISTER_TYPE_8BIT, ((char*)(&user._REG))+1);\
	this->mp[_8L]=regMapping(REGISTER_TYPE::REGISTER_TYPE_8BIT, &user._REG);\
}while(0);
#define SETS64(REG,_64) this->mp[REG]=\
		regMapping(REGISTER_TYPE::REGISTER_TYPE_64BIT, &user._64)
#define SETS32(REG,_32) this->mp[REG]=\
		regMapping(REGISTER_TYPE::REGISTER_TYPE_32BIT, &user._32)
#ifdef __x86_64__
	SET64(rax,"rax","eax","ax","ah","al");
	SET64(rbx,"rbx","ebx","bx","bh","bl");
	SET64(rcx,"rcx","ecx","cx","ch","cl");
	SET64(rdx,"rdx","edx","dx","dh","dl");
	SET64(rsi,"rsi","esi","si","sih","sil");
	SET64(rdi,"rdi","edi","di","dih","dil");
	SET64(rsp,"rsp","esp","sp","sph","spl");
	SET64(rbp,"rbp","ebp","bp","bph","bpl");
	SET64(r8 ,"r8", "r8d","r8d","r8h","r8l");
	SET64(r9 ,"r9", "r9d","r9d","r9h","r9l");
	SET64(r10,"r10","r10d","r10w","r10h","r10l");
	SET64(r11,"r11","r11d","r11w","r11h","r11l");
	SET64(r12,"r12","r12d","r12w","r12h","r12l");
	SET64(r13,"r13","r13d","r13w","r13h","r13l");
	SET64(r14,"r14","r14d","r14w","r14h","r14l");
	SET64(r15,"r15","r15d","r15w","r15h","r15l");
	SET64(rip,"rip","eip","ip","iph","ipl");
	SETS64("cs",cs);
	SETS64("eflags",eflags);
	SETS64("ss",ss);
	SETS64("fs_base",fs_base);
	SETS64("gs_base",gs_base);
	SETS64("ds",ds);
	SETS64("es",es);
	SETS64("fs",fs);
	SETS64("gs",gs);
#elif defined(__aarch64__)
	SETS64("sp"    ,sp      );
	SETS64("pc"    ,pc      );
	SETS64("pstate",pstate  );

	SETS64( "r0"   ,regs[0 ]);
	SETS64( "r1"   ,regs[1 ]);
	SETS64( "r2"   ,regs[2 ]);
	SETS64( "r3"   ,regs[3 ]);
	SETS64( "r4"   ,regs[4 ]);
	SETS64( "r5"   ,regs[5 ]);
	SETS64( "r6"   ,regs[6 ]);
	SETS64( "r7"   ,regs[7 ]);
	SETS64( "r8"   ,regs[8 ]);
	SETS64( "r9"   ,regs[9 ]);
	SETS64( "r10"  ,regs[10]);
	SETS64( "r11"  ,regs[11]);
	SETS64( "r12"  ,regs[12]);
	SETS64( "r13"  ,regs[13]);
	SETS64( "r14"  ,regs[14]);
	SETS64( "r15"  ,regs[15]);
	SETS64( "r16"  ,regs[16]);
	SETS64( "r17"  ,regs[17]);
	SETS64( "r18"  ,regs[18]);
	SETS64( "r19"  ,regs[19]);
	SETS64( "r20"  ,regs[20]);
	SETS64( "r21"  ,regs[21]);
	SETS64( "r22"  ,regs[22]);
	SETS64( "r23"  ,regs[23]);
	SETS64( "r24"  ,regs[24]);
	SETS64( "r25"  ,regs[25]);
	SETS64( "r26"  ,regs[26]);
	SETS64( "r27"  ,regs[27]);
	SETS64( "r28"  ,regs[28]);
	SETS64( "r29"  ,regs[29]);
	SETS64( "r30"  ,regs[30]);
	SETS64( "r31"  ,regs[31]);

	SETS64( "x0"   ,regs[0 ]);
	SETS64( "x1"   ,regs[1 ]);
	SETS64( "x2"   ,regs[2 ]);
	SETS64( "x3"   ,regs[3 ]);
	SETS64( "x4"   ,regs[4 ]);
	SETS64( "x5"   ,regs[5 ]);
	SETS64( "x6"   ,regs[6 ]);
	SETS64( "x7"   ,regs[7 ]);
	SETS64( "x8"   ,regs[8 ]);
	SETS64( "x9"   ,regs[9 ]);
	SETS64( "x10"  ,regs[10]);
	SETS64( "x11"  ,regs[11]);
	SETS64( "x12"  ,regs[12]);
	SETS64( "x13"  ,regs[13]);
	SETS64( "x14"  ,regs[14]);
	SETS64( "x15"  ,regs[15]);
	SETS64( "x16"  ,regs[16]);
	SETS64( "x17"  ,regs[17]);
	SETS64( "x18"  ,regs[18]);
	SETS64( "x19"  ,regs[19]);
	SETS64( "x20"  ,regs[20]);
	SETS64( "x21"  ,regs[21]);
	SETS64( "x22"  ,regs[22]);
	SETS64( "x23"  ,regs[23]);
	SETS64( "x24"  ,regs[24]);
	SETS64( "x25"  ,regs[25]);
	SETS64( "x26"  ,regs[26]);
	SETS64( "x27"  ,regs[27]);
	SETS64( "x28"  ,regs[28]);
	SETS64( "x29"  ,regs[29]);
	SETS64( "x30"  ,regs[30]);
	SETS64( "x31"  ,regs[31]);

	SETS32( "w0"   ,regs[0 ]);
	SETS32( "w1"   ,regs[1 ]);
	SETS32( "w2"   ,regs[2 ]);
	SETS32( "w3"   ,regs[3 ]);
	SETS32( "w4"   ,regs[4 ]);
	SETS32( "w5"   ,regs[5 ]);
	SETS32( "w6"   ,regs[6 ]);
	SETS32( "w7"   ,regs[7 ]);
	SETS32( "w8"   ,regs[8 ]);
	SETS32( "w9"   ,regs[9 ]);
	SETS32( "w10"  ,regs[10]);
	SETS32( "w11"  ,regs[11]);
	SETS32( "w12"  ,regs[12]);
	SETS32( "w13"  ,regs[13]);
	SETS32( "w14"  ,regs[14]);
	SETS32( "w15"  ,regs[15]);
	SETS32( "w16"  ,regs[16]);
	SETS32( "w17"  ,regs[17]);
	SETS32( "w18"  ,regs[18]);
	SETS32( "w19"  ,regs[19]);
	SETS32( "w20"  ,regs[20]);
	SETS32( "w21"  ,regs[21]);
	SETS32( "w22"  ,regs[22]);
	SETS32( "w23"  ,regs[23]);
	SETS32( "w24"  ,regs[24]);
	SETS32( "w25"  ,regs[25]);
	SETS32( "w26"  ,regs[26]);
	SETS32( "w27"  ,regs[27]);
	SETS32( "w28"  ,regs[28]);
	SETS32( "w29"  ,regs[29]);
	SETS32( "w30"  ,regs[30]);
	SETS32( "w31"  ,regs[31]);
#else
	static_assert(false,"Invalid Architecture");
#endif
}


std::ostream& wadu::operator<<(std::ostream&o,const userRegisters& uu){
#define PRINT_REG(X,Y) o<<setfill(' ')<<setw(9)\
	<<Y<<": 0x"<<setfill('0')<<setw(16)<<std::hex<<u.X<<std::endl

	auto u=uu.user;

#ifdef __x86_64__

	o<<endl;
	PRINT_REG(rax,"rax");
	PRINT_REG(rbx,"rbx");
	PRINT_REG(rcx,"rcx");
	PRINT_REG(rdx,"rdx");
	PRINT_REG(rdi,"rdi");
	PRINT_REG(rsi,"rsi");

	PRINT_REG(r8,"r8");
	PRINT_REG(r9,"r9");
	PRINT_REG(r10,"r10");
	PRINT_REG(r11,"r11");
	PRINT_REG(r12,"r12");
	PRINT_REG(r13,"r13");
	PRINT_REG(r14,"r14");
	PRINT_REG(r15,"r15");

	PRINT_REG(rbp,"rbp");
	PRINT_REG(rsp,"rsp");
	PRINT_REG(rip,"rip");

	PRINT_REG(orig_rax,"orig_rax");

	PRINT_REG(cs,"cs");
	PRINT_REG(eflags,"eflags");
	PRINT_REG(ss,"ss");
	PRINT_REG(fs_base,"fs_base");
	PRINT_REG(gs_base,"gs_base");
	PRINT_REG(ds,"ds");
	PRINT_REG(es,"es");
	PRINT_REG(fs,"fs");
	PRINT_REG(gs,"gs");
#elif defined(__aarch64__)
	PRINT_REG(sp,"sp");
	PRINT_REG(pc,"pc");
	PRINT_REG(pstate,"pstate");
	PRINT_REG(regs[0 ], "r0");
	PRINT_REG(regs[1 ], "r1");
	PRINT_REG(regs[2 ], "r2");
	PRINT_REG(regs[3 ], "r3");
	PRINT_REG(regs[4 ], "r4");
	PRINT_REG(regs[5 ], "r5");
	PRINT_REG(regs[6 ], "r6");
	PRINT_REG(regs[7 ], "r7");
	PRINT_REG(regs[8 ], "r8");
	PRINT_REG(regs[9 ], "r9");
	PRINT_REG(regs[10], "r10");
	PRINT_REG(regs[11], "r11");
	PRINT_REG(regs[12], "r12");
	PRINT_REG(regs[13], "r13");
	PRINT_REG(regs[14], "r14");
	PRINT_REG(regs[15], "r15");
	PRINT_REG(regs[16], "r16");
	PRINT_REG(regs[17], "r17");
	PRINT_REG(regs[18], "r18");
	PRINT_REG(regs[19], "r19");
	PRINT_REG(regs[20], "r20");
	PRINT_REG(regs[21], "r21");
	PRINT_REG(regs[22], "r22");
	PRINT_REG(regs[23], "r23");
	PRINT_REG(regs[24], "r24");
	PRINT_REG(regs[25], "r25");
	PRINT_REG(regs[26], "r26");
	PRINT_REG(regs[27], "r27");
	PRINT_REG(regs[28], "r28");
	PRINT_REG(regs[29], "r29");
	PRINT_REG(regs[30], "r30");
	PRINT_REG(regs[31], "r31");

#else
	static_assert(false,"Invalid Architecture");
#endif

	return o;
}

std::ostream& wadu::operator<<(std::ostream&o,const dataVector& u){
	unsigned i=0;
	o<<setfill('0');
	for(auto& hx:u)
	{
		i++;
		o<<setw(2)<<hex<<(unsigned)hx;
		if(i%8)
			o<<" ";
		else
			o<<endl;
	}

	return o;
}
