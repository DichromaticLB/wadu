#ifndef INCLUDE_SYNTOOLS_HPP_
#define INCLUDE_SYNTOOLS_HPP_

#define yyFlexLexer interpFlexLexer

#ifndef yyFlexLexerOnce
#include <FlexLexer.h>
#endif

#include"wadu_instrumentation.hpp"
#include<iostream>
#include<string>
#include<vector>
#include<random>
#include<string_view>
#include<fstream>

namespace wadu{
	struct expression;
	struct sequence;
	using byteVector=std::vector<uint8_t>;

	struct variant{
		variant();

		variant& operator=(const string& s);
		byteVector vec;
		expression *exp;
		uint64_t num;
	};

//	using variant=std::variant<byteVector*,expression*,uint64_t>;
	using semval=expression;


	class interpLexer:public interpFlexLexer{
	public:
		interpLexer(std::istream& i,const string&n="<anonymous>")
			: interpFlexLexer(&i),
			 name(n),
			 datasrc(i),
			 linecount(1){}
		int interplex( semval * v);

		std::string name;
		std::istream &datasrc;
		int linecount;
	};

	enum class expression_actor:uint8_t{
		u8 =sizeof(uint8_t),
		u16=sizeof(uint16_t),
		u32=sizeof(uint32_t),
		u64=sizeof(uint64_t),
		vector,
		var,
		_if,
		execlist,
		eq,
		neq,
		gt,
		lt,
		gte,
		lte,
		_while,
		dowhile,
		add,
		sub,
		mult,
		div,
		mod,
		land,
		band,
		lor,
		bor,
		_xor,
		bnot,
		lnot,
		lshift,
		rshift,
		concat,
		_register,
		edit_reg,
		_memory,
		_memory_write,
		_amemory,
		_amemory_write,
		_assignment,
		cast8,
		cast16,
		cast32,
		cast64,
		castVec,
		print,
		dump,
		createStream,
		mapFile,
		_sequence,
		_sequence_range,
		_sequence_strlist,
		_sequence_increment,
		_sequence_get,
		script,
		len,
		_break,
		slice,
		disable_breakpoint,
		enable_breakpoint,
		status_breakpoint,
		index,
		random,
		randomseq,
		memcmp,
		system,
		writestdin,
		closestdn,
		sleep,
		_signal,
		pattern,
		isdef,
		defun,
		funcall,
		steptrace,
		getc,
		request
	};
	struct debugContext;

	struct expression{
		using dataptr=const uint8_t*;
		using datalen=uint64_t;

		expression();
		expression(uint64_t number,expression_actor c);
		expression(const char* c,uint32_t len=0);
		expression& operator=(const string& s);
		pair<dataptr,datalen>  data_description() const ;

		static expression buildFromJSON(const anjson::jsonobject&o);

		expression eval(debugContext& c) const;
		bool operator==(const expression& o) const;
		bool operator>(const expression& o) const;
		bool operator>=(const expression& o) const;
		bool isVector() const;
		bool isNum() const;
		bool isSeq() const;
		uint64_t getNum() const;
		const byteVector& getVec() const;
		string asString(uint32_t dex=0) const;
		string_view asStringView(uint32_t dex=0) const;
		expression_actor actor;
		vector<variant> lt;

		void tearDown();
	};

	enum class range_type{
		invalid,
		numeric,
		stringList
	};

	struct numberRange{
		uint64_t begining,end;
	};

	struct sequence_unit{
		using numRange=numberRange;
		using numContainer=vector<uint64_t>;
		using stringContainer=vector<vector<uint8_t>>;
		using rngDev=std::mt19937;

		sequence_unit();

		numRange& range();
		numContainer& numList();
		stringContainer& strList();

		void initRange(uint64_t begin,uint64_t end);
		void initStrList();
		bool increment();
		void release();
		void randomize(rngDev&d);
		void connect(sequence_unit& s);

		range_type getType();
		uint64_t currentIndex;
	private:
		range_type type;
		void *structure;

		sequence_unit*next;
	};

	struct sequence{
		//NO COPYING NO MOVING AND NO FUCKING SHARING, WHERE ITS BUILT IT STAYS
		sequence& operator=(const sequence&)=delete;
		sequence (const sequence&)=delete;
		sequence& operator=(sequence&&)=delete;
		sequence (sequence&&)=delete;
		sequence():updated(false){};
		sequence(int e):updated(false){}; /*dont delete, needed for inplace cosntructor in maps :()*/
		~sequence();
		expression operator[](uint32_t t);
		bool increment();
		void randomize(sequence_unit::rngDev& d);
		void append();
		sequence_unit& back();
		size_t size();
		void dobind();


		vector<sequence_unit> units;
		bool updated=false;
	};


	struct varmap{
		varmap():upScope(nullptr){}
		expression& operator[](const string&k);
		const  expression& at(const string&k)const;
		size_t count(const string&k) const;
		void clear();

		unordered_map<string,expression> variables;
		varmap *upScope;
	};

	using seqMap=unordered_map<string,sequence>;
	using commandMap=unordered_map<memaddr,expression>;

	struct tracingContext;
	struct scheduler;
	using waduhandler=
			std::function<void(tracingContext* sc,uint32_t id)>;

	enum class handlerNames{
		DEFAULT=0,
		CLONE=1,
		EXEC=2,
		FORK=3,
		VFORK=4
	};

	void setupHandlers();
	void registerDefaultHandler();
	void registerCloneHandler();
	void registerExecHandler();
	void registerForkHandler();
	void registerForkvHandler();

	uint32_t registerHandler(const waduhandler&w);
	enum class scheduleRequestType{
		none=0,
		exit,
		reset, //This destroys variables, sequences & closes files
		restart, // This just restarts the process
		resetif,
		restartif
	};

	struct exprfunction{
		vector<string> parametersNames;
		expression code;
	};

	struct funmap {
		const exprfunction& operator[](const string&s) const;
		void put(const string&name,const expression& params,
				const expression&exec);

		unordered_map<string,exprfunction> fns;
	};

	scheduleRequestType parseScheduleRequestType(const string&s);
	struct scheduleRequest{
		scheduleRequest();
		scheduleRequest(const optionMap&m);
		scheduleRequestType t;
		expression condition;
		expression commands;
	};

	using msignal=unordered_map<int,scheduleRequest>;

	struct tracingContext;
	struct executingManager{
		void fork (int childId);
		void vfork(int childId);
		void clone(int childId);
		void execv(int childId);
		bool done() const;
		void handleRequests();

		executingManager(const optionMap& m);
		vector<tracingContext*> executingContexts;
	};

	struct tracingContext{
		using childP=tracingContext*;
		using parentP=tracingContext*;
		static const uint32_t coutID=0xffffffff;
		static const uint32_t cerrID=0xfffffffe;

		tracingContext(uint32_t id,const optionMap& m,executingManager& parent);

		void run();
		bool done() const ;
		void handleRequest();
		void reset();

		const optionMap& source;
		executingManager& manager;
		uint32_t id,handlerId;
		thread tracee;
		parentP parent;
		vector<childP> children;

		struct {
			uint32_t traceOptions;
		} flags;

		struct {
			stream_map name2stream;
			vector<ofstream> openFiles;
		} io;

		struct{
			varmap variables;
			seqMap sequences;
			funmap functions;
		} variables;

		struct {
			expression onAttach;
			expression onSchedule;
			scheduleRequest onExit;
			msignal onSignal;
			commandMap breakpoints;
		} controlFlow;

		struct {
			bool running;
			bool done;
			bool stepTracing;
			bool registersEdited;
			scheduleRequestType request;
		} traceStatus;
	};

	enum class commandRequest{
		none=0,
		detach=1,
		exit=2,
		restart=3,
		reset=4
	};


	struct debugContext
	{
		void applyBreakpointChanges(memaddr);
		void mapMemoryMaps();
		void updates();

		breaker& breakpoints;
		process_child& child;
		memory_maps& memory;
		varmap * variables;
		userRegisters& regs;
		stream_map& streams;
		vector<ofstream>& openFiles;
		seqMap& sequences;
		const commandMap& commands;
		funmap& functions;
		bool& stepTrace;

		vector<int> requestedSignal;
		sequence_unit::rngDev rngSrc;
		brqueue breakpointActions;
		bool registersEdited;
		bool handlingRequired;
		commandRequest interpRequest;
	};
	const expression& script(const string& filename);
}

#endif
