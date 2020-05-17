#include"wadu_interp.hpp"
#include<fstream>
#include<vector>
#include<sys/signal.h>
#include<sstream>
#include<iomanip>
#include<random>
#include"util.hpp"
using namespace std;
using namespace wadu;

static bool handlerBreak(wadu::debugContext& ctx){

	if(ctx.breakpoints.virt2file.count(breakPointAt(ctx.regs,
			ctx.breakpoints.breakpointSize())))
	{
		uint64_t vaddr=breakPointAt(ctx.regs,ctx.breakpoints.breakpointSize());
		uint64_t faddr=
				ctx.breakpoints.virt2file.at(vaddr);

		ctx.breakpoints.hit(ctx.regs);
		ctx.commands.at(faddr).eval(ctx);
		ctx.updates();
		ctx.breakpoints.restore_from_virtual(vaddr,ctx.regs);

		ctx.applyBreakpointChanges(faddr);

		return true;
	}

	return false;
}

static void continueExecution(wadu::debugContext& ctx,bool step=false){
	if(step)
		ctx.breakpoints.step();
	else
		ctx.breakpoints.cont();

	if(ctx.requestedSignal.size()){

		for(auto&sig:ctx.requestedSignal)
			ctx.child.signal(sig);

		ctx.requestedSignal.clear();
	}
}

static inline bool reqHandle(
		uint32_t id,
		scheduleRequest& request,
		debugContext& ctx,
		tracingContext* tc,
		stopRoot& why){

	wlog(LL::DEBUG)<<"Thread["
							<<id<<"] pid:["<<dec<<ctx.child.id<<"] Handling";

	switch(request.t){
			case scheduleRequestType::none:
				break;

			case scheduleRequestType::exit:
				tc->traceStatus.request=scheduleRequestType::exit;
				wlog(LL::TRACE)<<why<<" Thread["
						<<id<<"] pid:["<<dec<<ctx.child.id<<"] Handledr Exit";
				return true;

			case scheduleRequestType::reset:
				tc->traceStatus.request=scheduleRequestType::reset;
				tc->controlFlow.onSchedule=request.commands;
				wlog(LL::TRACE)<<why<<" Thread["<<dec<<id<<"] pid:["
					<<ctx.child.id<<"] reseting";
				return true;

			case scheduleRequestType::restart:
				tc->traceStatus.request=scheduleRequestType::restart;
				tc->controlFlow.onSchedule=request.commands;
				wlog(LL::TRACE)<<why<<" Thread["<<dec<<id<<"] pid:["
					<<ctx.child.id<<"] restarting";
				return true;

			case scheduleRequestType::resetif:
				if(request.condition.eval(ctx).getNum())
				{
					tc->traceStatus.request=scheduleRequestType::reset;
					tc->controlFlow.onSchedule=request.commands;
					wlog(LL::TRACE)<<" Thread["<<dec<<id<<"] pid:["
						<<ctx.child.id<<"] reseting";
				}
				else
					tc->traceStatus.request=scheduleRequestType::exit;
				return true;

			case scheduleRequestType::restartif:
				if(request.condition.eval(ctx).getNum())
				{
					tc->traceStatus.request=scheduleRequestType::restart;
					tc->controlFlow.onSchedule=request.commands;
					wlog(LL::TRACE)<<why<<" Thread["<<dec<<id<<"] pid:["
						<<ctx.child.id<<"] restarting";
				}
				else
					tc->traceStatus.request=scheduleRequestType::exit;
				return true;
		}

	return false;
}

static void inline  handleInterpRequest(scheduleRequest& request,debugContext& ctx){
	switch(ctx.interpRequest){
		case commandRequest::none:
			break;
		case commandRequest::detach:
			ctx.child.detach();
			request.t=scheduleRequestType::exit;
			ctx.handlingRequired=true;
			break;
		case commandRequest::exit:
			request.t=scheduleRequestType::exit;
			ctx.handlingRequired=true;
			break;
		case commandRequest::restart:
			request.t=scheduleRequestType::restart;
			ctx.handlingRequired=true;
			break;
		case commandRequest::reset:
			request.t=scheduleRequestType::reset;
			ctx.handlingRequired=true;
			break;
	}
}

static void inline handleSignalHandler(
		scheduleRequest& request,
		debugContext& ctx){

	switch(request.t){
		case scheduleRequestType::exit:
		case scheduleRequestType::none:
			request.commands.eval(ctx);
			break;
		default:
			break;
	}
}

static void inline maybeSignalCallback(
		uint32_t id,
		stopRoot& why,
		scheduleRequest& request,
		tracingContext* tc,
		debugContext& ctx){

	if(tc->controlFlow.onSignal.count(why.signum())){
		wlog(LL::TRACE)<<"Thread["<<dec<<id<<"] pid:["
			<<ctx.child.id<<"] Received signal and has a handler"
				<<why;
		request=tc->controlFlow.onSignal.at(why.signum());
		ctx.handlingRequired=true;
		handleSignalHandler(request,ctx);
	}
	else{
		wlog(LL::WARNING)<<"Thread["<<dec<<id<<"] pid:["
					<<ctx.child.id<<"] Unhandled signal "
						<<why.signum()<<" Requesting exit";
		ctx.handlingRequired=true;
	}

}

static inline void handleSigtrap(
		uint32_t id,
		stopRoot& why,
		scheduleRequest& request,
		tracingContext* tc,
		debugContext& ctx){

		auto bp=breakPointAt(ctx.regs,0);
		bp=ctx.child.virtual2real(bp);
		if(why.exited())
		{
			wlog(LL::TRACE)<<"Thread["<<dec<<id<<"] pid:["
				<<ctx.child.id<<"] Received signal and is exiting "
				<<why;
			request=tc->controlFlow.onExit;
			request.commands.eval(ctx);
			ctx.child.resume();
			ctx.handlingRequired=true;

		}
		else if(why.clone())
		{
			wlog(LL::INFO)<<"Process Cloned";
			//TO-DO
		}
		else if(why.exec())
		{
			wlog(LL::INFO)<<"Process Execve";
			//TO-DO
		}
		else if(why.forked())
		{
			tc->controlFlow.onFork.parentDo.eval(ctx);
			if(tc->controlFlow.onFork.follow)
			{
				tc->manager.fork(tc,ctx.child);
				wlog(LL::INFO)<<"Process Forked source pid:"+
						to_string(ctx.child.id);
			}
			else
			{
				wlog(LL::TRACE)<<"Received trace request "<<
						" but didnt follow "<<"thread["<<dec<<id<<"] pid:["
						<<ctx.child.id<<"]";
			}
			handleInterpRequest(request,ctx);
		}
		else if(why.vforked())
		{
			//This is a trap, vfork precedes a low res execve, we're not tracing
			//Stuff in this call most likely
			wlog(LL::INFO)<<"Process VForked";
		}
		else if(!ctx.stepTrace&&handlerBreak(ctx))
		{

			wlog(LL::TRACE)<<"Thread["<<dec<<id<<"] pid:["
				<<ctx.child.id<<"] Procesing breakpoint";
			handleInterpRequest(request,ctx);
		}
		else if(ctx.stepTrace&&ctx.strace_commands.count(bp))
		{
			ctx.strace_commands.at(bp).eval(ctx);
			handleInterpRequest(request,ctx);
		}
		else
			maybeSignalCallback(id,why,request,tc,ctx);
}

static inline void handleStop(
		uint32_t id,
		stopRoot& why,
		scheduleRequest& request,
		tracingContext* tc,
		debugContext& ctx){

	switch(why.source){
		case STOP_SOURCE::STOP_SOURCE_UNCAUGHT_SIGNAL:
		{
			request=scheduleRequest();
			ctx.handlingRequired=true;
			wlog(LL::INFO)<<" Thread["<<dec<<id<<"] pid:["
				<<ctx.child.id<<"] exiting from"
				" uncaught signal: "<<why;
			break;
		}
		case STOP_SOURCE::STOP_SOURCE_EXIT:
		{
			wlog(LL::TRACE)<<"Process exited";
			request=tc->controlFlow.onExit;
			request.commands.eval(ctx);
			ctx.handlingRequired=true;
			break;
		}
		case STOP_SOURCE::STOP_SOURCE_CONTINUE:
			wlog(LL::TRACE)<<" Thread["<<dec<<id<<"] pid:["
				<<ctx.child.id<<"] continued: ";
			break;
		case STOP_SOURCE::STOP_SOURCE_INVALID:
			request=scheduleRequest();
			ctx.handlingRequired=true;
			wlog(LL::ERROR)<<" Thread["<<dec<<id<<"] pid:["
				<<ctx.child.id<<"] invalid stop source";
			break;
		case STOP_SOURCE::STOP_SOURCE_SIGNAL:
		{
			if(why.signum()==SIGTRAP)
				handleSigtrap(id,why,request,tc,ctx);
			else
				maybeSignalCallback(id,why,request,tc,ctx);
			break;
		}
	}
}

void wadu::registerDefaultHandler(){
	using namespace anjson;
	wadu::registerHandler([](tracingContext* tc,uint32_t id)->void{
			try{
				const optionMap& m=tc->source;

				if( !m.containsKeyType("image",type::string)||
					!m.containsKeyType("arguments",type::array)||
					!m.containsKeyType("textAt",type::integer))
					{
						wlog(LL::CRITICAL)<<("Required image, arguments and text"
								" offset to intialize trace");
						tc->traceStatus.request=scheduleRequestType::exit;
						return;
					}

				string r_image=m["image"].get<string>();
				vector<string> r_arguments;
				uint64_t r_text=m["textAt"].get<uint64_t>();

				for(auto arg:m["arguments"].get<jsonobject::arrayType>())
					if(arg.getType()==type::string)
						r_arguments.push_back(arg.get<string>());

				process_child child(r_image,r_arguments);

				try
				{
					child.run(
							m.query("\"this\"[\"stdin\"]" ),
							m.query("\"this\"[\"stdout\"]"),
							m.query("\"this\"[\"stderr\"]"));
				}
				catch (const runtime_error&e)
				{
					throw runtime_error("Failed to run process with image: "+r_image);
				}

				linux_memory_maps maps("/proc/"+to_string(child.id)+"/maps");
				child.setOptions(tc->flags.traceOptions);
				breaker b(child,maps.getFirstExec(r_image));
				child.setTextOffsets(r_text,b.text.begin);
				userRegisters r;

				debugContext ctx={
						.breakpoints=b,
						.child=child,
						.memory=maps,
						.variables=&tc->variables.variables,
						.regs=r,
						.streams=tc->io.name2stream,
						.openFiles=tc->io.openFiles,
						.sequences=tc->variables.sequences,
						.commands=tc->controlFlow.breakpoints,
						.strace_commands=tc->controlFlow.trace_breakpoints,
						.onFork=tc->controlFlow.onFork,
						.functions=tc->variables.functions,
						.stepTrace=tc->traceStatus.stepTracing,

						.requestedSignal=vector<int>(),
						.rngSrc=sequence_unit::rngDev(),
						.breakpointActions=brqueue(),
						.registersEdited=false,
						.handlingRequired=false,
						.interpRequest=commandRequest::none
					};

				ctx.stepTrace=tc->traceStatus.stepTracing;

				if(m.query("\"this\"[\"seed\"]").getType()==type::integer)
					ctx.rngSrc.seed(m["seed"].get<uint64_t>());
				else
					ctx.rngSrc=sequence_unit::rngDev(random_device()());

				ctx.mapMemoryMaps();
				ctx.miscVariables(id);

				for(auto& v:m["breakpoints"].get<jsonobject::arrayType>())
					if(!v.containsKeyType("enabled",type::booltype)
							||v["enabled"].get<bool>())
						ctx.breakpoints.addBreakpoint(v["address"].get<uint64_t>());

				tc->controlFlow.onSchedule.eval(ctx);
				tc->controlFlow.onSchedule=expression();
				tc->controlFlow.onAttach.eval(ctx);

				wlog(LL::INFO)<<"Thread["<<id<<"] "<<"Tracing pid:"<<ctx.child.id;
				continueExecution(ctx,ctx.stepTrace);

				while(true){
					scheduleRequest request; //This defaults to exit!
					auto why=child.wait();
					getRegisters(ctx.child,ctx.regs);
					handleStop(id,why,request,tc,ctx);

					if(ctx.handlingRequired){

						if(reqHandle(id,request,ctx,tc,why))
							return;

						ctx.handlingRequired=false;
					}

					ctx.updates();
					continueExecution(ctx,ctx.stepTrace);
				}
			}
			catch(const runtime_error&e){
				wlog(LL::CRITICAL)<<"Crashed thread id["<<dec<<id<<"] "<<e.what();
				tc->traceStatus.request=scheduleRequestType::exit;
			}
			catch(const exception&e){
				wlog(LL::CRITICAL)<<"Crashed thread id["<<dec<<id<<"] "<<e.what();
				tc->traceStatus.request=scheduleRequestType::exit;
			}
			catch(...){
				wlog(LL::CRITICAL)<<"Crashed thread id["<<dec<<id<<"]";
				tc->traceStatus.request=scheduleRequestType::exit;
			}
		}
	);
}

void wadu::setupHandlers(){
	registerDefaultHandler();
	registerCloneHandler();
	registerExecHandler();
	registerForkHandler();
	registerForkvHandler();
}

void wadu::registerForkHandler(){
	using namespace anjson;
	wadu::registerHandler([](tracingContext* tc,uint32_t id)->void{
		try{
			const optionMap& m=tc->source;

			if( !m.containsKeyType("image",type::string)||
				!m.containsKeyType("textAt",type::integer))
				{
					wlog(LL::CRITICAL)<<("Required image and text"
							" offset to intialize trace of forked process");
					tc->traceStatus.request=scheduleRequestType::exit;
					return;
				}

			string r_image=m["image"].get<string>();
			uint64_t r_text=m["textAt"].get<uint64_t>();
			process_child child(
				tc->variables.variables.at("$pid").getNum());
			if(child.attach()==-1){
				throw runtime_error("Failed to attach to pid:"
						+to_string(child.id));
			}

			child.waitForkReady();
			child.setOptions(tc->flags.traceOptions);
			linux_memory_maps maps("/proc/"+to_string(child.id)+"/maps");
			breaker b(child,maps.getFirstExec(r_image));
			child.setTextOffsets(r_text,b.text.begin);
			userRegisters r;

			debugContext ctx={
					.breakpoints=b,
					.child=child,
					.memory=maps,
					.variables=&tc->variables.variables,
					.regs=r,
					.streams=tc->io.name2stream,
					.openFiles=tc->io.openFiles,
					.sequences=tc->variables.sequences,
					.commands=tc->controlFlow.breakpoints,
					.strace_commands=tc->controlFlow.trace_breakpoints,
					.onFork=tc->controlFlow.onFork,
					.functions=tc->variables.functions,
					.stepTrace=tc->traceStatus.stepTracing,

					.requestedSignal=vector<int>(),
					.rngSrc=sequence_unit::rngDev(),
					.breakpointActions=brqueue(),
					.registersEdited=false,
					.handlingRequired=false,
					.interpRequest=commandRequest::none
				};

			ctx.stepTrace=tc->traceStatus.stepTracing;

			if(m.query("\"this\"[\"seed\"]").getType()==type::integer)
				ctx.rngSrc.seed(m["seed"].get<uint64_t>());
			else
				ctx.rngSrc=sequence_unit::rngDev(random_device()());

			ctx.mapMemoryMaps();
			ctx.miscVariables(id);

			for(auto& v:m["breakpoints"].get<jsonobject::arrayType>())
				if(!v.containsKeyType("enabled",type::booltype)
						||v["enabled"].get<bool>())
					ctx.breakpoints.addBreakpoint(v["address"].get<uint64_t>());

			tc->controlFlow.onSchedule.eval(ctx);
			tc->controlFlow.onSchedule=expression();
			tc->controlFlow.onAttach.eval(ctx);

			wlog(LL::INFO)<<"Thread["<<id<<"] "<<"Tracing pid:"<<ctx.child.id;
			continueExecution(ctx,ctx.stepTrace);//

			while(true){
				scheduleRequest request; //This defaults to exit!
				auto why=child.wait();
				getRegisters(ctx.child,ctx.regs);
				handleStop(id,why,request,tc,ctx);

				if(ctx.handlingRequired){

					if(reqHandle(id,request,ctx,tc,why))
						return;

					ctx.handlingRequired=false;
				}

				ctx.updates();
				continueExecution(ctx,ctx.stepTrace);
			}
		}
		catch(const runtime_error&e){
			wlog(LL::CRITICAL)<<"Crashed thread id["<<dec<<id<<"] "<<e.what();
			tc->traceStatus.request=scheduleRequestType::exit;
		}
		catch(const exception&e){
			wlog(LL::CRITICAL)<<"Crashed thread id["<<dec<<id<<"] "<<e.what();
			tc->traceStatus.request=scheduleRequestType::exit;
		}
		catch(...){
			wlog(LL::CRITICAL)<<"Crashed thread id["<<dec<<id<<"]";
			tc->traceStatus.request=scheduleRequestType::exit;
		}
	});
}
void wadu::registerCloneHandler(){
	using namespace anjson;
	wadu::registerHandler([](tracingContext* tc,uint32_t id)->void{});
}
void wadu::registerExecHandler(){
	using namespace anjson;
	wadu::registerHandler([](tracingContext* tc,uint32_t id)->void{});
}
void wadu::registerForkvHandler(){
	using namespace anjson;
	wadu::registerHandler([](tracingContext* tc,uint32_t id)->void{});
}
