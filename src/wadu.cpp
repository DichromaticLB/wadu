#include"wadu_interp.hpp"
#include<anjson/anjson.hpp>
#include<unistd.h>
#include<cstring>
#include<sstream>
#include<algorithm>
#include"util.hpp"
const std::string defaultds="input.json";

void help(){
	using namespace std;
	cout<<"wadu - reverse engineering and binary instrumentation tool"<<endl;
	cout<<"	usage: wadu [-f filename] [-e expression] [-h]"<<endl;
	cout<<"	by default tries to read its configuration from "<<defaultds<<endl;
	cout<<"Version: "<<MAJORNUM<<"."<<MINORNUM<<endl;
	exit(0);
}

void dummy(std::istream& i){
	using namespace wadu;
	process_child dummychild("dummy",{});
	linux_memory_maps dummymaps;
	memory_segment dummymemseg;
	userRegisters dummyregs;
	breaker dummybreaker(dummychild,dummymemseg);
	stream_map dummymap;
	commandMap dummycm;
	varmap vm;
	vector<ofstream> o;
	seqMap s;
	funmap f;
	bool st=false;
	wlog(LL::INFO)<<"Evaluating dummy stream";

	debugContext dummyctx={
			.breakpoints=dummybreaker,
			.child=dummychild,
			.memory=dummymaps,
			.variables=&vm,
			.regs=dummyregs,
			.streams=dummymap,
			.openFiles=o,
			.sequences=s,
			.commands=dummycm,
			.functions=f,
			.stepTrace=st,

			.requestedSignal=vector<int>(),
			.rngSrc=sequence_unit::rngDev(),
			.breakpointActions=brqueue(),
			.registersEdited=false,
			.handlingRequired=false,
			.interpRequest=commandRequest::none

		};

	dummyctx.streams["stdout"]=tracingContext::coutID;
	dummyctx.streams["stderr"]=tracingContext::cerrID;
	dummyctx.rngSrc.seed(time(0));
	wadu_parser p(i);

	try{
		p.parse();
		p.result.eval(dummyctx);
	}catch(const runtime_error& e)
	{
		wlog(LL::CRITICAL)<<"Fatal error while evaluating dummy, "<<e.what();
	}
	catch(...){
		wlog(LL::CRITICAL)<<"Fatal error while evaluating dummy";
	}
}
wadu::LL wadu::wlog::currentLevel=wadu::LL::ERROR;

int main(int argc,char **argv) {
		using namespace wadu;
		using namespace std;

		string dataSource=defaultds;
		stringstream evals;
		int option;
		bool usecin=false,exp=false;
		while ((option= getopt(argc, argv, "f:e:hl:")) != -1) {
			switch(option){
				case 'f':
					if(!strcmp("-",optarg))
						usecin=true;
					else
						dataSource=optarg;
					break;
				case 'e':
					evals<<optarg;
					exp=true;
					break;
				case 'h':
					help();
					break;
				case 'l':
				{
					if(isdigit(optarg[0]))
						wadu::wlog::setLevel((wadu::LL)atoi(optarg));
					else
						wadu::wlog::setLevel(optarg);
				}
					break;
				default:
					help();
					break;
			}
		}

		if(exp){
			dummy(evals);
			return 0;
		}

		setupHandlers();

		anjson::jsonobject o;
		try{
			if(usecin)
			{
				o=anjson::fromStream(cin);
				wlog(LL::TRACE)<<"Reading configuration from stdin";
			}
			else
			{
				o=anjson::fromFile(dataSource);
				wlog(LL::TRACE)<<"Reading configuration from "+dataSource;
			}
			if(o.getType()!=anjson::type::object)
			{
				wlog(LL::CRITICAL)<<"Couldn't read a valid configuration, exiting";
				exit(-1);
			}

			executingManager m(o);
			m.executingContexts[0]->run();
			while(!m.done())
			{
				m.handleRequests();
				usleep(1000*100);
			}

		}
		catch(const runtime_error& e)
		{
			wlog(LL::CRITICAL)<<e.what();
			return -1;
		}

		return 0;
}

