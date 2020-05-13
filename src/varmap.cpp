#include"wadu_interp.hpp"
#include"util.hpp"
#include<mutex>
wadu::sequence_unit::sequence_unit():
	currentIndex(0),
	type(range_type::invalid),
	structure(nullptr),
	next(nullptr){}

wadu::sequence_unit::numRange& wadu::sequence_unit::range(){
	return *reinterpret_cast<numRange*>(structure);
}

wadu::sequence_unit::numContainer& wadu::sequence_unit::numList(){
	return *reinterpret_cast<numContainer*>(structure);
}

wadu::sequence_unit::stringContainer& wadu::sequence_unit::strList(){
	return *reinterpret_cast<stringContainer*>(structure);
}

void wadu::sequence_unit::initRange(uint64_t begin,uint64_t end){
	structure=new numberRange();
	range()=numberRange{begin,end};
	currentIndex=begin;
	type=range_type::numeric;
}



void wadu::sequence_unit::initStrList(){
	structure=new stringContainer();
	currentIndex=0;
	type=range_type::stringList;
}

bool wadu::sequence_unit::increment(){
	switch(type){
		case range_type::numeric:
		{
			currentIndex++;
			if(currentIndex>=range().end){
				currentIndex=range().begining;
				if(next)
					return next->increment();
				else
					return false;
			}
			break;
		}
		case range_type::stringList:
		{
			currentIndex++;
			if(currentIndex>=strList().size()){
				currentIndex=0;
				if(next)
					return next->increment();
				else
					return false;
			}
			break;
		}
			break;
		default:
			throw runtime_error("Trying to increase invalid range instance");
		}
	return true;
}

void wadu::sequence_unit::release(){
	switch(type){
		case range_type::numeric:
			delete reinterpret_cast<numberRange*>(structure);
			break;
		case range_type::stringList:
			delete reinterpret_cast<stringContainer*>(structure);
			break;
		default:
			throw runtime_error("Trying to realease invalid range instance");
	}
	type=range_type::invalid;
	structure=nullptr;
}

void wadu::sequence_unit::randomize(rngDev&d){
	uint64_t min=0,max=0;
	switch(type){
		case range_type::numeric:
			min=range().begining;
			max=range().end-1;
			break;
		case range_type::stringList:
			max=strList().size()-1;
			break;
		default:
			throw runtime_error("Trying to randomize invalid range instance");
	}
	currentIndex=uniform_int_distribution<uint64_t>(min,max)(d);
}

wadu::range_type wadu::sequence_unit::getType(){
	return type;
}

void  wadu::sequence_unit::connect(sequence_unit& s){
	next=&s;
}

wadu::expression wadu::sequence::operator[](uint32_t t){
	switch(units[t].getType()){
		case range_type::numeric:
			return expression(units[t].currentIndex,expression_actor::u64);
		case range_type::stringList:
		{
			expression r(0,expression_actor::vector);
			r.lt[0].vec=static_cast<byteVector>(
					units[t].strList()[units[t].currentIndex]);
			return r;

		}
		default:
			throw runtime_error("Cant build expression from sequence");
	}
}

bool  wadu::sequence::increment(){
	if(!updated)
		dobind();

	if(!units.size())
		return false;
	return units[0].increment();
}

void wadu::sequence::randomize(sequence_unit::rngDev& d){
	for(auto&s:units)
		s.randomize(d);
}

void wadu::sequence::append(){
	units.emplace_back();
	auto s=units.size();
	if(s>1)
	{
		units[s-2].connect(units[s-1]);
	}
	updated=false;
}

wadu::sequence_unit& wadu::sequence::back(){
	return units.back();
}

void wadu::sequence::dobind(){
	for(unsigned i=0;i<size()-1;i++)
		units[i].connect(units[i+1]);
	updated=true;
}

wadu::expression& wadu::varmap::operator[](const string&k){
	varmap *ct=this;
	while(ct!=nullptr)
	{
		if(ct->variables.count(k))
			return ct->variables[k];
		else
			ct=ct->upScope;
	}

	return variables[k];
}

const wadu::expression& wadu::varmap::at(const string&k) const{
	const varmap *ct=this;
	while(ct!=nullptr)
	{
		if(ct->variables.count(k))
			return ct->variables.at(k);
		else
			ct=ct->upScope;
	}

	return variables.at(k);
}

size_t wadu::varmap::count(const string&k) const{
	const varmap *ct=this;
	while(ct!=nullptr)
	{
		if(ct->variables.count(k))
			return 1;
		else
			ct=ct->upScope;
	}
	return 0;
}

void wadu::varmap::clear(){
	variables.clear();
}

size_t wadu::sequence::size(){
	return units.size();
}

wadu::sequence::~sequence(){
	for(auto& v:units)
		v.release();
}


const wadu::expression& wadu::script(const string& filename){
	static unordered_map<string,expression> maps;
	static std::mutex m;

	bool exists;

	{
		std::lock_guard<std::mutex> mex(m);
		exists=maps.count(filename)>0;
	}

	if(!exists)
	{
		wlog(LL::TRACE)<<"Reading script: "<<filename;
		ifstream i(filename);
		if(!i.is_open())
			throw runtime_error("Could open file "+
					filename+" to load script");

		wadu_parser pp(i);
		bool success=true;

		try{

			pp.parse();
			std::lock_guard<std::mutex> mex(m);
			exists=maps.count(filename)>0;

			if(maps.count(filename))
				success=false;
			else
				maps.emplace(filename,pp.result);

		}
		catch(const exception& e){
			pp.result.tearDown();
			throw runtime_error("Script source:"+filename+"\n"+
					e.what());
		}

		if(!success)
			pp.result.tearDown();
	}
	wlog(LL::TRACE)<<"returning script: "<<filename;
	return maps.at(filename);
}
