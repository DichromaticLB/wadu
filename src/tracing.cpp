#include"wadu_interp.hpp"
#include<sstream>
#include<string.h>
#include<unistd.h>
#include<signal.h>
#include"util.hpp"
using namespace wadu;
using namespace anjson;

static void createBreakmap(const optionMap&m,commandMap&break2command){
	wlog(LL::TRACE)<<"creating breakmap";
	if(m.containsKeyType("breakpoints",type::array))
	{
		for(auto& br:m["breakpoints"].get<optionMap::arrayType>())
		{

			if(!br.containsKeyType("address",type::integer))
				throw runtime_error("Breakpoint require \"address\" as number");

			if(!br.containsKeyType("commands",type::array))
				throw runtime_error("Breakpoint require \"commands\""
						" as an array of strings");

			auto addr=br["address"];
			auto commands=br["commands"];
			wlog(LL::DEBUG)<<"Creating breapoint for 0x"<<hexstr(addr);
			stringstream ss,hx;

			for(auto& command:commands.get<optionMap::arrayType>()){

				if(command.getType()!=anjson::type::string)
					throw runtime_error("Command expected to be strings,"
							"received: "+command.typestr());

				ss<<command.get<string>();
			}

			hx<<hex<<addr.get<uint64_t>();
			wadu_parser pp(ss,"parsing breakpoint:0x"+hx.str());
			pp.parse();
			break2command[addr.get<uint64_t>()]=move(pp.result);
		}

		wlog(LL::TRACE)<<break2command.size()<<" breakpoints parsed";
	}
	else
		wlog(LL::WARNING)<<"no breakpoint map in configuration";
}

static uint32_t getTraceOptions(const optionMap&m){
	using namespace anjson;
	const static unordered_map<string,traceRequest> mp={
			{"clone",traceRequest::clone},
			{"flagSyscall",traceRequest::flagSyscall},
			{"exec",traceRequest::exec},
			{"fork",traceRequest::fork},
			{"vfork",traceRequest::vfork},
			{"killonexit",traceRequest::killonexit},
			{"stopOnExit",traceRequest::stopOnExit}
	};

	uint32_t traceOptions=0;

	if(m.containsKeyType("traceOptions",type::array)){
		for(auto&v:m["traceOptions"].get<optionMap::arrayType>()){

			if(v.getType()!=type::string)
				throw runtime_error("Building traceOptions "
						"requires an array of strings");

			if(!mp.count(v.get<string>()))
				throw runtime_error("Invalid trace option: "+v.get<string>());

			traceOptions|=(uint32_t)mp.at(v.get<string>());
		}
	}
	else
		traceOptions=
			(uint32_t)traceRequest::flagSyscall |
			(uint32_t)traceRequest::killonexit;

	return traceOptions;
}

bool wadu::executingManager::done() const{
	for(auto&v:executingContexts)
		if(!v->done())
			return false;

	wlog(LL::TRACE)<<"executing manager is done";
	return true;
}

void wadu::executingManager::handleRequests(){
	for(auto&v:contextImages())
		if(v->traceStatus.request!=scheduleRequestType::none)
			v->handleRequest();
}

vector<tracingContext*>  wadu::executingManager::contextImages() {
	vector<tracingContext*>  res;

	{
		std::lock_guard< std::mutex> m(mutex);
		res.reserve(executingContexts.size());

		for(auto& v:executingContexts)
			res.push_back(v.get());

	}

	return res;
}

wadu::executingManager::executingManager(const optionMap& m){
	this->executingContexts.emplace_back(
			std::make_unique<tracingContext>(executingContexts.size(),m,*this)
	);
}

void wadu::executingManager::fork (tracingContext* tc,process_child& cd){

	wlog(LL::TRACE)<<"forking tracing context";
	tracingContextPointer wrapper;
	uint64_t cid=cd.forkPid(),tid;
	process_child process(cid);
	process.wait();

	if(process.prepareDetach()==-1)
		throw runtime_error("Failed to prepare forked process for "
				"attach "+to_string(cid)+" "+strerror(errno));

	if(process.detach()==-1)
		throw runtime_error("Failed to detach forked process for "
			"attach "+to_string(cid)+" "+strerror(errno));

	if(tc->controlFlow.onFork.newConfig.empty())
	{
		wrapper=std::make_unique<tracingContext>(
				executingContexts.size(),tc->source,*this);
	}
	else
	{
		auto config=anjson::fromFile(tc->controlFlow.onFork.newConfig);
		wrapper=std::make_unique<tracingContext>(
				executingContexts.size(),tc->source,*this);
	}

	tracingContext*child=wrapper.get();

	{
		std::lock_guard<std::mutex> guard(mutex);

		executingContexts.emplace_back(move(wrapper));
		tid=executingContexts.size()-1;

	}

	wlog(LL::INFO)<<"Process "+to_string(cd.id)+" spawns "+to_string(cid)+
			" to be traced by thread id: "<<to_string(tid);


	tc->children.emplace_back(child);
	child->handlerId=(uint32_t)handlerNames::FORK;
	child->parent=tc;
	child->copy(*tc);
	child->variables.variables["$pid"]=expression(cid,expression_actor::u64);
	child->controlFlow.onSchedule=tc->controlFlow.onFork.childDo;
	child->run();

}

void wadu::tracingContext::copy(const tracingContext&tc){
	flags=tc.flags;
	variables.variables=tc.variables.variables;
	variables.functions=tc.variables.functions;
}

static  const vector<int>& defaultNoSignals(){
	static  const vector<int> dns={ SIGSTOP,SIGTRAP,SIGCHLD};
	return dns;
};

wadu::tracingContext::tracingContext(
uint32_t id,
const optionMap& m,
executingManager& parent):source(m),manager(parent){
	wlog(LL::TRACE)<<"building tracing context";
	memset(&traceStatus,0,sizeof(traceStatus));
	this->id=id;
	handlerId=(uint32_t)handlerNames::DEFAULT;
	this->parent=nullptr;
	createBreakmap(m,controlFlow.breakpoints);
	if(m.containsKeyType("trace",type::object)){
		createBreakmap(m["trace"],controlFlow.trace_breakpoints);
	}
	reset();

	if(m.containsKey("commands"))
		controlFlow.onAttach=expression::buildFromJSON(m["commands"]);

	if(m.containsKey("onexit"))
	{
		auto ox=m["onexit"];
		controlFlow.onExit=scheduleRequest(ox);
	}



	if(m.containsKeyType("signals",type::array))
		for(auto& v:m["signals"].get<optionMap::arrayType>())
		{
			if(!v.containsKeyType("signal",type::integer))
				throw runtime_error(
						"on exit signal handlers require signal number");

			controlFlow.onSignal.emplace(v["signal"].get<int64_t>(),v);
		}
	for(auto v:defaultNoSignals())
		if(!controlFlow.onSignal.count(v))
			controlFlow.onSignal[v]=scheduleRequest::donothing();



	flags.traceOptions=getTraceOptions(m);

	if(m.containsKeyType("stepTrace",type::booltype)&&
					m["stepTrace"].get<bool>())
		traceStatus.stepTracing=true;

	if(m.containsKeyType("onFork",type::object)){
		auto of=m["onFork"];

		if(of.containsKeyType("follow",type::booltype))
			controlFlow.onFork.follow=of["follow"].get<bool>();

		if(of.containsKeyType("child",type::object))
		{
			auto child=of["child"];

			if(child.containsKeyType("reconfig",type::string))
				controlFlow.onFork.newConfig=child["reconfig"].get<string>();

			if(child.containsKeyType("commands",type::array))
				controlFlow.onFork.childDo=
						expression::buildFromJSON(child["commands"]);
		}

		if(of.containsKeyType("parent",type::object))
		{
			auto parent=of["parent"];

			if(parent.containsKeyType("commands",type::array))
				controlFlow.onFork.parentDo=
						expression::buildFromJSON(parent["commands"]);
		}
	}
}

static vector<wadu::waduhandler>& handlers(){
	static vector<wadu::waduhandler> h;
	return h;
}

uint32_t wadu::registerHandler(const waduhandler&w){
	wlog(LL::TRACE)<<"registering handler";
	auto& hd=handlers();
	hd.push_back(w);
	return hd.size();
}

void wadu::tracingContext::run(){
	wlog(LL::TRACE)<<"starting tracing context";
	tracee=thread(handlers().at(handlerId),this,id);
	traceStatus.running=true;
}

bool wadu::tracingContext::done() const {
	return traceStatus.done;
}

void wadu::tracingContext::handleRequest(){
	switch(traceStatus.request){

		case scheduleRequestType::reset:
			wlog(LL::TRACE)<<"Processed reset request";
			tracee.join();
			variables.variables.clear();
			variables.sequences.clear();
			io.name2stream.clear();
			io.openFiles.clear();
			traceStatus.request=scheduleRequestType::none;
			run();
			break;
		case scheduleRequestType::restart:
			wlog(LL::TRACE)<<"Processed restart request";
			tracee.join();
			traceStatus.request=scheduleRequestType::none;
			run();
			break;
		case scheduleRequestType::exit:
		default:
			wlog(LL::TRACE)<<"Processed exit request";
			tracee.join();
			traceStatus.running=false;
			traceStatus.done=true;
			traceStatus.request=scheduleRequestType::none;
			break;

	}
}


void wadu::tracingContext::reset(){
	wlog(LL::DEBUG)<<"Reseting tracing context";
	variables.variables["threadid"]=expression(id,expression_actor::u64);
	io.name2stream["stdout"]=coutID;
	io.name2stream["stderr"]=cerrID;
}
void wadu::replicateHandler::release(){
	parentDo.tearDown();
	childDo.tearDown();
}

void wadu::tracingContext::release(){
	variables.functions.release();
	controlFlow.onAttach.tearDown();
	controlFlow.onSchedule.tearDown();
	controlFlow.onExit.release();
	for(auto&v:controlFlow.onSignal)
		v.second.release();
	for(auto&v:controlFlow.breakpoints)
		v.second.tearDown();
	for(auto&v:controlFlow.trace_breakpoints)
		v.second.tearDown();
	controlFlow.onFork.release();
}
