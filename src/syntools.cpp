#include"syntools.hpp"
#include"wadu_interp.hpp"
#include<string.h>
#include<iomanip>
#include<algorithm>
#include"util.hpp"
using namespace wadu;

wadu::variant::variant(){
	num=0;
	exp=nullptr;
}

variant& wadu::variant::operator=(const string& s){
	vec.clear();
	vec.insert(vec.end(),s.begin(),s.end());
	return *this;
}

wadu::expression::expression(){
	actor=expression_actor::u8;
}

wadu::expression::expression(uint64_t data,expression_actor c){
	actor=c;
	lt.emplace_back();
	lt[0].num=data;
}

wadu::expression::expression(const char* c,uint32_t len){

	if(len==0)
		len=strlen(c);

	actor=expression_actor::vector;
	lt.emplace_back();
	lt[0].vec.insert(lt[0].vec.end(),c,c+len);
}

expression& wadu::expression::operator=(const string& s){
	lt.resize(1);
	lt[0].vec.clear();
	lt[0].vec.insert(lt[0].vec.begin(),s.begin(),s.end());
	return *this;
}

pair<wadu::expression::dataptr,wadu::expression::datalen>
		wadu::expression::data_description() const {

	pair<wadu::expression::dataptr,wadu::expression::datalen> res;

	if(isNum()){
		res.first=(dataptr)&lt[0].num;
		res.second=(uint64_t)actor;
	}
	else
	{
		res.first=(dataptr)lt[0].vec.data();
		res.second=lt[0].vec.size();
	}

	return res;
}

expression wadu::expression::buildFromJSON(const anjson::jsonobject&m){
	using namespace anjson;

	expression res;
	stringstream ss;
	bool any=false;

	if(m.getType()==type::array)
	{
		for(auto& v:m.get<jsonobject::arrayType>())
		{
			if(v.getType()!=type::string)
				throw runtime_error("cant set up condition with a non string");
			ss<<v.get<string>();
			any=true;
		}
	}
	else if(m.containsKeyType("condition",type::string))
	{
		ss<<m.get<string>();
		any=true;
	}
	else
		throw runtime_error("Cant build expression with object of type:"+m.typestr());

	if(any){
		wadu_parser pp(ss);
		try{
			pp.parse();
			res=pp.result;
		}
		catch(const runtime_error& e){
			pp.result.tearDown();
			throw e;
		}
	}

	return res;

}

bool wadu::expression::operator==(const expression& o) const{

	if(actor==expression_actor::vector&&o.actor==expression_actor::vector)
		return equal(lt[0].vec.begin(),lt[0].vec.end(),
				o.lt[0].vec.begin(),o.lt[0].vec.end());

	return lt[0].num==o.lt[0].num;

}

bool wadu::expression::operator>(const expression& o) const{
	return lt[0].num>o.lt[0].num;
}

bool wadu::expression::operator>=(const expression& o) const{
	return lt[0].num>=o.lt[0].num;
}

bool wadu::expression::isVector() const{
	return actor==expression_actor::vector;
}

bool wadu::expression::isNum() const{
	switch(actor){
		case expression_actor::u64:
		case expression_actor::u32:
		case expression_actor::u16:
		case expression_actor::u8:
			return true;
		default:
			return false;
	}
}

bool wadu::expression::isSeq() const{
	return actor==expression_actor::_sequence;
}

uint64_t wadu::expression::getNum() const{
	return lt[0].num;
}

const byteVector& wadu::expression::getVec() const{
	return lt[0].vec;
}

string wadu::expression::asString(uint32_t dex) const{
	return string ((char*)lt[dex].vec.data(),lt[dex].vec.size());
}

string_view wadu::expression::asStringView(uint32_t dex) const{
	return string_view((char*)lt[dex].vec.data(),lt[dex].vec.size());
}

expression fromRegMapping(const regMapping&m){
	switch(m.type){
		case REGISTER_TYPE::REGISTER_TYPE_8BIT:
			return expression(*(uint8_t*)m.addr,expression_actor::u8);
		case REGISTER_TYPE::REGISTER_TYPE_16BIT:
			return expression(*(uint16_t*)m.addr,expression_actor::u16);
		case REGISTER_TYPE::REGISTER_TYPE_32BIT:
			return expression(*(uint32_t*)m.addr,expression_actor::u32);
		default:
			return expression(*(uint64_t*)m.addr,expression_actor::u64);
	}
}


static inline uint64_t operate(uint64_t a,uint64_t b,char op,expression_actor c){
	switch(op){
	case '+':
	{
		switch(c){
		case expression_actor::u8:
			return (uint8_t)a+(uint8_t)b;
		case expression_actor::u16:
			return (uint16_t)a+(uint16_t)b;
		case expression_actor::u32:
			return (uint32_t)a+(uint32_t)b;
		case expression_actor::u64:
			return (uint64_t)a+(uint64_t)b;
		default:
			throw runtime_error("Coldnt operate with op "+op);
		}
	}
	case '-':
	{
		switch(c){
		case expression_actor::u8:
			return (uint8_t)a-(uint8_t)b;
		case expression_actor::u16:
			return (uint16_t)a-(uint16_t)b;
		case expression_actor::u32:
			return (uint32_t)a-(uint32_t)b;
		case expression_actor::u64:
			return (uint64_t)a-(uint64_t)b;
		default:
			throw runtime_error("Coldnt operate with op "+op);
		}
	}
	case '*':
	{
		switch(c){
		case expression_actor::u8:
			return (uint8_t)a*(uint8_t)b;
		case expression_actor::u16:
			return (uint16_t)a*(uint16_t)b;
		case expression_actor::u32:
			return (uint32_t)a*(uint32_t)b;
		case expression_actor::u64:
			return (uint64_t)a*(uint64_t)b;
		default:
			throw runtime_error("Coldnt operate with op "+op);
		}
	}
	case '/':
	{
		switch(c){
		case expression_actor::u8:
			return (uint8_t)a/(uint8_t)b;
		case expression_actor::u16:
			return (uint16_t)a/(uint16_t)b;
		case expression_actor::u32:
			return (uint32_t)a/(uint32_t)b;
		case expression_actor::u64:
			return (uint64_t)a/(uint64_t)b;
		default:
			throw runtime_error("Coldnt operate with op "+op);
		}
	}
	case '%':
	{
		switch(c){
		case expression_actor::u8:
			return (uint8_t)a%(uint8_t)b;
		case expression_actor::u16:
			return (uint16_t)a%(uint16_t)b;
		case expression_actor::u32:
			return (uint32_t)a%(uint32_t)b;
		case expression_actor::u64:
			return (uint64_t)a%(uint64_t)b;
		default:
			throw runtime_error("Coldnt operate with op "+op);
		}
	}
	case 'l':
	{
		switch(c){
		case expression_actor::u8:
			return (uint8_t)a<<(uint8_t)b;
		case expression_actor::u16:
			return (uint16_t)a<<(uint16_t)b;
		case expression_actor::u32:
			return (uint32_t)a<<(uint32_t)b;
		case expression_actor::u64:
			return (uint64_t)a<<(uint64_t)b;
		default:
			throw runtime_error("Coldnt operate with op "+op);
		}
	}
	case 'r':
	{
		switch(c){
		case expression_actor::u8:
			return (uint8_t)a>>(uint8_t)b;
		case expression_actor::u16:
			return (uint16_t)a>>(uint16_t)b;
		case expression_actor::u32:
			return (uint32_t)a>>(uint32_t)b;
		case expression_actor::u64:
			return (uint64_t)a>>(uint64_t)b;
		default:
			throw runtime_error("Coldnt operate with op "+op);
		}
	}
	case '^':
	{
		switch(c){
		case expression_actor::u8:
			return (uint8_t)a^(uint8_t)b;
		case expression_actor::u16:
			return (uint16_t)a^(uint16_t)b;
		case expression_actor::u32:
			return (uint32_t)a^(uint32_t)b;
		case expression_actor::u64:
			return (uint64_t)a^(uint64_t)b;
		default:
			throw runtime_error("Coldnt operate with op "+op);
		}
		break;
	}
	default:
		throw runtime_error("Coldnt operate with actor "+to_string((int)c));
	}

}


expression wadu::expression::eval(debugContext&c) const{
	switch(actor){
		case expression_actor::u64:
		case expression_actor::u32:
		case expression_actor::u16:
		case expression_actor::u8:
		case expression_actor::vector:
			return *this;
		case expression_actor::execlist:
		{
			if(lt.empty())
				return expression();

			for(unsigned i=0;i<lt.size()-1;i++)
				if(lt[i].exp->actor==expression_actor::_break)
					return lt[i].exp->eval(c);
				else
					lt[i].exp->eval(c);

			return lt.back().exp->eval(c);
			break;
		}
		case expression_actor::_break:
		{
			return lt[0].exp->eval(c);
		}
		case expression_actor::var:
		{
			auto key=asString();

			if(!c.variables->count(key))
				throw runtime_error("Failed to access variable: "+key);

			return c.variables->at(asString());
		}
		case expression_actor::_if:
		{
			if(lt[0].exp->eval(c).lt[0].num)
				return lt[1].exp->eval(c);
			else if(lt.size()>2)
				return lt[2].exp->eval(c);
			break;
		}
		case expression_actor::eq:
		{
			expression res(0,expression_actor::u8);
			res.lt[0].num=lt[0].exp->eval(c)==lt[1].exp->eval(c);
			return res;
		}
		case expression_actor::neq:
		{
			expression res(0,expression_actor::u8);
			res.lt[0].num=!(lt[0].exp->eval(c) == lt[1].exp->eval(c));
			return res;
		}
		case expression_actor::gt:
		{
			expression res(0,expression_actor::u8);
			res.lt[0].num=lt[0].exp->eval(c) > lt[1].exp->eval(c);
			return res;
		}
		case expression_actor::gte:
		{
			expression res(0,expression_actor::u8);
			res.lt[0].num=lt[0].exp->eval(c) >= lt[1].exp->eval(c);
			return res;
		}
		case expression_actor::lt:
		{
			expression res(0,expression_actor::u8);
			res.lt[0].num=lt[1].exp->eval(c) > lt[0].exp->eval(c);
			return res;
		}
		case expression_actor::lte:
		{
			expression res(0,expression_actor::u8);
			res.lt[0].num=lt[1].exp->eval(c) >= lt[0].exp->eval(c);
			return res;
		}
		case expression_actor::_while:
		{
			while(lt[0].exp->eval(c).lt[0].num)
				lt[1].exp->eval(c);
			break;
		}
		case expression_actor::dowhile:
		{
			do{
				lt[0].exp->eval(c);
			}while(lt[1].exp->eval(c).getNum());

			break;
		}
		case expression_actor::add:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not add using vector types");

			return expression(operate(l.getNum(),r.getNum(),'+',max(l.actor,r.actor))
					,max(l.actor,r.actor));
		}
		case expression_actor::sub:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not substract using vector types");

			return expression(operate(l.getNum(),r.getNum(),'-',max(l.actor,r.actor))
					,max(l.actor,r.actor));
		}
		case expression_actor::mult:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not multiply using vector types");

			return expression(operate(l.getNum(),r.getNum(),'*',max(l.actor,r.actor))
					,max(l.actor,r.actor));
		}
		case expression_actor::div:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not divide using vector types");

			return expression(operate(l.getNum(),r.getNum(),'/',max(l.actor,r.actor))
					,max(l.actor,r.actor));
		}
		case expression_actor::mod:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not mod using vector types");

			return expression(operate(l.getNum(),r.getNum(),'%',max(l.actor,r.actor))
					,max(l.actor,r.actor));
		}
		case expression_actor::land:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not do logic and using vector types");

			return expression(l.getNum()&&r.getNum(),max(l.actor,r.actor));
		}
		case expression_actor::band:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not do binary and using vector types");

			return expression(l.getNum()&r.getNum(),max(l.actor,r.actor));
		}
		case expression_actor::lor:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not do logic or using vector types");

			return expression(l.getNum()||r.getNum(),max(l.actor,r.actor));
		}
		case expression_actor::bor:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not do binary or using vector types");

			return expression(l.getNum()|r.getNum(),max(l.actor,r.actor));
		}
		case expression_actor::_xor:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not do binary xor using vector types");

			return expression(operate(l.getNum(),r.getNum(),'^',max(l.actor,r.actor))
					,max(l.actor,r.actor));
		}
		case expression_actor::lshift:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not do binary lshift using vector types");

			return expression(operate(l.getNum(),r.getNum(),'l',max(l.actor,r.actor))
					,max(l.actor,r.actor));
		}
		case expression_actor::bnot:
		{
			auto l=lt[0].exp->eval(c);

			if(l.isNum())
				l.lt[0].num=~l.lt[0].num;
			else
				for(auto& byte:l.lt[0].vec)
					byte=~byte;

			return l;
		}
		case expression_actor::lnot:
		{
			auto l=lt[0].exp->eval(c);

			if(l.isNum())
				l.lt[0].num=!l.lt[0].num;
			else
				throw runtime_error("Cant logical not a vector type");

			return l;
		}

		case expression_actor::rshift:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);

			if(!l.isNum()||!r.isNum())
				throw runtime_error("Can not do binary rshift using vector types");

			return expression(operate(l.getNum(),r.getNum(),'r',max(l.actor,r.actor))
					,max(l.actor,r.actor));
		}
		case expression_actor::_register:
		{
			auto key=asString();

			if(!c.regs.mp.count(key))
				throw runtime_error("Unknown register: "+key);

			auto reg=c.regs.mp.at(key);
			return fromRegMapping(reg);
		}
		case expression_actor::edit_reg:
		{
			auto rval=lt[1].exp->eval(c);
			c.registersEdited=true;

			auto key=asString();

			if(!c.regs.mp.count(key))
				throw runtime_error("Unknown register: "+key);

			auto reg=c.regs.mp.at(key);

			char * data=nullptr;
			size_t lim=sizeof(uint64_t);
			if(rval.isNum())
				data=(char*)&rval.lt[0].num;
			else
			{
				data=(char*)rval.lt[0].vec.data();
				lim=rval.getVec().size();
			}
			switch(reg.type){
				case REGISTER_TYPE::REGISTER_TYPE_8BIT:
					memcpy(reg.addr,data,min(sizeof(uint8_t),lim));
					break;
				case REGISTER_TYPE::REGISTER_TYPE_16BIT:
					memcpy(reg.addr,data,min(sizeof(uint16_t),lim));
					break;
				case REGISTER_TYPE::REGISTER_TYPE_32BIT:
					memcpy(reg.addr,data,min(sizeof(uint32_t),lim));
					break;
				case REGISTER_TYPE::REGISTER_TYPE_64BIT:
					memcpy(reg.addr,data,min(sizeof(uint64_t),lim));
					break;

			}
			break;
		}
		case expression_actor::_amemory:
		case expression_actor::_memory:
		{
			bool abs=actor==expression_actor::_amemory;
			auto address=lt[0].exp->eval(c);
			auto count=lt[1].exp->eval(c);

			if(address.isVector()||count.isVector())
				throw runtime_error("Required address and count as numbers to access read memory");

			expression res;
			res.actor=expression_actor::vector;
			res.lt.emplace_back();
			res.lt[0].vec=readMemory(
					c.child,abs?
					address.getNum()
					:
					c.child.real2virtual(address.getNum()),
					count.getNum());
			return res;
		}
		case expression_actor::_amemory_write:
		case expression_actor::_memory_write:
		{
			bool abs=actor==expression_actor::_amemory_write;
			auto address=lt[0].exp->eval(c);
			auto source=lt[1].exp->eval(c);
			uint64_t lim=0;
			const uint8_t *bytes=nullptr;
			switch(source.actor){
			      case expression_actor::u8:
			      case expression_actor::u16:
			      case expression_actor::u32:
			      case expression_actor::u64:
			    	  lim=(uint64_t)source.actor;
			    	  bytes=(uint8_t*)&source.lt[0].num;
			    	  break;
			      case expression_actor::vector:
			    	  lim=source.getVec().size();
			    	  bytes=source.getVec().data();
			    	  break;
			      default:
			    	  throw runtime_error("Cant get bytes to write to memory");
			}

			if(lt.size()>2)
				lim=min(lt[2].exp->eval(c).getNum(),lim);

			if(!address.isNum())
				throw runtime_error("Address for memory writing must be a number");

			writeMemory(c.child,abs?
					address.getNum()
					:
					c.child.real2virtual(address.getNum()),bytes,lim);

			break;
		}
		case expression_actor::_assignment:
		{
			auto val=lt[1].exp->eval(c);
			(*c.variables)[asString()]=val;
			return val;
		}
		case expression_actor::cast8:{
			auto res=lt[0].exp->eval(c);
			if(res.isNum()){
				res.lt[0].num&=0xff;
			}
			else{
				if(res.lt[0].vec.size())
					res.lt[0].num=res.lt[0].vec[0];
				else
					res.lt[0].num=0;
			}
			res.actor=expression_actor::u8;
			return res;
		}
		case expression_actor::cast16:{
			auto res=lt[0].exp->eval(c);
			if(res.isNum()){
				res.lt[0].num&=0xffff;
			}
			else{
				res.lt[0].num=0;
				memcpy(
						&res.lt[0].num,
						res.lt[0].vec.data(),
						min(res.lt[0].vec.size(),sizeof(uint16_t)));
			}
			res.actor=expression_actor::u16;
			return res;
		}
		case expression_actor::cast32:{
			auto res=lt[0].exp->eval(c);
			if(res.isNum()){
				res.lt[0].num&=0xffffffff;
			}
			else{
				res.lt[0].num=0;
				memcpy(
						&res.lt[0].num,
						res.lt[0].vec.data(),
						min(res.lt[0].vec.size(),sizeof(uint32_t)));
			}
			res.actor=expression_actor::u32;
			return res;
		}
		case expression_actor::cast64:{
			auto res=lt[0].exp->eval(c);
			if(res.isNum()){
				res.lt[0].num&=0xffffffffffffffffl;
			}
			else{
				res.lt[0].num=0;
				memcpy(
						&res.lt[0].num,
						res.lt[0].vec.data(),
						min(res.lt[0].vec.size(),sizeof(uint64_t)));
			}
			res.actor=expression_actor::u64;
			return res;
		}
		case expression_actor::castVec:{
			auto res=lt[0].exp->eval(c);
			expression ret(0,expression_actor::vector);
			auto& vc=ret.lt[0].vec;
			switch(res.actor){
				case expression_actor::u8:
					vc.insert(vc.end(),
							(uint8_t*)&res.lt[0].num,
							((uint8_t*)&res.lt[0].num)+sizeof(uint8_t));
					break;
				case expression_actor::u16:
					vc.insert(vc.end(),
							(uint8_t*)&res.lt[0].num,
							((uint8_t*)&res.lt[0].num)+sizeof(uint16_t));
					break;

				case expression_actor::u32:
					vc.insert(vc.end(),
							(uint8_t*)&res.lt[0].num,
							((uint8_t*)&res.lt[0].num)+sizeof(uint32_t));
					break;

				case expression_actor::u64:
					vc.insert(vc.end(),
							(uint8_t*)&res.lt[0].num,
							((uint8_t*)&res.lt[0].num)+sizeof(uint64_t));
					break;
				default:
					return res;

			}

			return ret;
		}
		case expression_actor::print:
		{
			bool useStrings=lt[0].exp->lt.size()>1;
			string key=lt.size()>1?(lt[1].exp->eval(c).asString()):"stdout";
			uint32_t dex=c.streams.at(key);
			ostream* d;
			if(dex==tracingContext::coutID)
				d=&cout;
			else if(dex==tracingContext::cerrID)
				d=&cerr;
			else
				d=(ostream*)&c.openFiles.at(dex);

			for(auto&v:lt[0].exp->lt){
				auto val=v.exp->eval(c);
				switch(val.actor){
					case expression_actor::u8:
						(*d)<<setfill('0')
							<<setw(sizeof(uint8_t)*2)<<hex<<(int)val.getNum();
						break;
					case expression_actor::u16:
						(*d)<<setfill('0')
						<<setw(sizeof(uint16_t)*2)<<hex<<(uint16_t)val.getNum();
						break;
					case expression_actor::u32:
						(*d)<<setfill('0')
						<<setw(sizeof(uint32_t)*2)<<hex<<(uint32_t)val.getNum();
						break;
					case expression_actor::u64:
						(*d)<<setfill('0')
						<<setw(sizeof(uint64_t)*2)<<hex<<(uint64_t)val.getNum();
						break;
					case expression_actor::vector:
					{
						if(useStrings)
							(*d)<<val.asStringView();
						else
						{
							int i=1;
							for(auto& c:val.getVec())
							{
								(*d)<<setfill('0')<<setw(sizeof(uint8_t)*2)
										<<hex<<(int)c<<" ";
								if(!(i++%8))
									(*d)<<endl;
							}
						}
						break;
					}
					default:
						throw runtime_error("Cant dump invalid type "+(char)actor);
					}
			}
			(*d)<<endl;
			break;
		}
		case expression_actor::dump:
		{
			string key=lt.size()>1?(lt[1].exp->eval(c).asString()):"stdout";
			uint32_t dex=c.streams.at(key);
			ostream* d;
			if(dex==tracingContext::coutID)
				d=&cout;
			else if(dex==tracingContext::cerrID)
				d=&cerr;
			else
				d=(ostream*)&c.openFiles.at(dex);

			for(auto&v:lt[0].exp->lt){
				auto val=v.exp->eval(c);
				switch(val.actor){
					case expression_actor::u8:
					case expression_actor::u16:
					case expression_actor::u32:
					case expression_actor::u64:
					{
						d->write((const char*)&val.lt[0].num,(int)val.actor);
						break;
					}
					case expression_actor::vector:
					{
						d->write((const char*)val.getVec().data(),val.getVec().size());
						break;
					}
					default:
						throw runtime_error("Cant dump invalid type "+(char)actor);
					}
			}
			break;
		}
		case expression_actor::createStream:
		{
			auto fnale=lt[0].exp->eval(c);

			if(!fnale.isVector())
				throw runtime_error("Cant open file with a non string as param");
			auto filename=fnale.asString();

			c.openFiles.emplace_back(filename);
			if(!c.openFiles.back().is_open())
			{
				c.openFiles.pop_back();
				throw runtime_error("Could open file for ostream: "+filename);
			}
			c.streams[asString(1)]=c.openFiles.size()-1;
			break;
		}
		case expression_actor::mapFile:
		{
			expression res;
			res.actor=expression_actor::vector;
			uint64_t from=0,to=numeric_limits<uint64_t>::max();

			if(lt.size()==3){
				from=lt[1].exp->eval(c).getNum();
				to=lt[2].exp->eval(c).getNum();
			}

			ifstream in(asString(),ios::binary|ios::ate);
			if(!in.is_open())
				throw runtime_error("Couldnt open file for mapping "+asString());

			uint64_t lim=std::min(to,(uint64_t)in.tellg());
			if(from<lim){
				res.lt.emplace_back();
				res.lt[0].vec.resize(lim-from);
				in.seekg(from);
				in.read((char*)res.lt[0].vec.data(),lim-from);
			}
			return res;
			break;
		}
		case expression_actor::_sequence:
		{
			string k=asString();
			c.sequences.emplace(k,1);
			auto& at=c.sequences.at(k);
			for(unsigned i=1;i<lt.size();i++){
				switch(lt[i].exp->actor){
					case expression_actor::_sequence_range:
					{
						at.append();
						at.back().initRange(
								lt[i].exp->lt[0].exp->eval(c).getNum(),
								lt[i].exp->lt[1].exp->eval(c).getNum());
						break;
					}
					case expression_actor::_sequence_strlist:
					{
						at.append();
						at.back().initStrList();
						auto &l=at.back().strList();
						l.reserve(lt[i].exp->lt.size());
						for(auto& ex:lt[i].exp->lt){

							l.emplace_back();
							auto val=ex.exp->eval(c);

							switch(val.actor){
								case expression_actor::u64:
								{
									char* b=(char*)&val.lt[0].num;
									l.back().insert(
											l.back().begin(),
											b,
											b+sizeof(uint64_t));
									break;
								}
								case expression_actor::u32:
								{
									char* b=(char*)&val.lt[0].num;
									l.back().insert(
											l.back().begin(),
											b,
											b+sizeof(uint32_t));
									break;
								}
								case expression_actor::u16:
								{
									char* b=(char*)&val.lt[0].num;
									l.back().insert(
											l.back().begin(),
											b,
											b+sizeof(uint16_t));
									break;
								}
								case expression_actor::u8:
								{
									char* b=(char*)&val.lt[0].num;
									l.back().insert(
											l.back().begin(),
											b,
											b+sizeof(uint8_t));
									break;
								}
								case expression_actor::vector:
								{
									l.back().insert(
											l.back().begin(),
											val.getVec().begin(),
											val.getVec().end());
									break;
								}
								default:
								{
									throw runtime_error("Invalid value "
											"for sequence strlist");
									break;
								}
							}
						}
						break;
					}

					default:
					{
						throw runtime_error(
								"Couldn't build sequence,"
								" invalid expression actor");
					}
				}
			}

			break;
		}
		case expression_actor::_sequence_range:
		case expression_actor::_sequence_strlist:
			throw runtime_error("Evaluation sequence atoms individually");

		case expression_actor::_sequence_increment:
		{
			return expression(
					c.sequences.at(
							asString()).increment()?1:0,
							expression_actor::u64);
			break;
		}
		case expression_actor::_sequence_get:
		{
			auto& seq=c.sequences[asString()];
			auto index=lt[1].exp->eval(c);
			if(!index.isNum())
				throw runtime_error("Cant use non-number to index sequence");
			return seq[index.getNum()];

			break;
		}
		case expression_actor::concat:{
			expression res(0,expression_actor::vector);
			auto &vec=res.lt[0].vec;
			for(auto& v:lt[0].exp->lt){
				auto val=v.exp->eval(c);
				switch(val.actor){
					case expression_actor::u8:
					case expression_actor::u16:
					case expression_actor::u32:
					case expression_actor::u64:
					{
						uint8_t *pt=(uint8_t*)(&val.lt[0].num);
						vec.insert(
								vec.end(),
								pt,
								pt+(unsigned)val.actor);
						break;
					}
					case expression_actor::vector:
					{
						vec.insert(
								vec.end(),
								val.getVec().begin(),
								val.getVec().end()
						);
						break;
					}
					default:
						throw runtime_error("Invalid type received for concatenation");

				}
			}
			return res;
		}
		case expression_actor::script:{
			auto filename=lt[0].exp->eval(c);
			if(filename.isNum())
				throw runtime_error("Cant load script from number");

			return wadu::script(filename.asString()).eval(c);
		}
		case expression_actor::slice:{
			auto data=lt[0].exp->eval(c);
			auto from=lt[1].exp->eval(c);
			auto to  =lt[2].exp->eval(c);

			if(!from.isNum()|| !to.isNum())
				throw runtime_error("Cant slice using non numbers as delimiters");

			uint64_t left=from.getNum(),right=to.getNum();
			uint8_t *pt_to_data=nullptr;
			switch(data.actor){
				case expression_actor::u8:
				case expression_actor::u16:
				case expression_actor::u32:
				case expression_actor::u64:
					pt_to_data=(uint8_t*)&data.lt[0].num;
					right=min(right,(uint64_t)data.actor);
					break;
				case expression_actor::vector:
					pt_to_data=data.lt[0].vec.data();
					right=min(right,data.getVec().size());
					break;
				default:
					throw runtime_error("Invalid type to slice data from");
			}

			expression res(0,expression_actor::vector);
			if(left<right){
				res.lt[0].vec.insert(
						res.lt[0].vec.begin(),
						pt_to_data+left,
						pt_to_data+right
						);
			}

			return res;
		}
		case expression_actor::disable_breakpoint:
		{
			if(lt.size()>0)
			{
				c.breakpointActions.emplace_back();
				c.breakpointActions.back().action=
						breakpointAction::action::clear_address;
				c.breakpointActions.back().target=lt[0].exp->eval(c).getNum();
			}
			else
			{
				c.breakpointActions.emplace_back();
				c.breakpointActions.back().action=
						breakpointAction::action::clear_this;
			}

			break;
		}
		case expression_actor::enable_breakpoint:
		{
			c.breakpointActions.emplace_back();
			c.breakpointActions.back().action=
					breakpointAction::action::enable_address;
			c.breakpointActions.back().target=lt[0].exp->eval(c).getNum();
			break;
		}
		case expression_actor::status_breakpoint:
		{
			auto addr=lt[0].exp->eval(c);
			if(!addr.isNum())
				throw runtime_error("Cant query breakpoint status using vector");

			return expression(
					c.breakpoints.isEnabledSeg(addr.getNum())?1:0,
					expression_actor::u8);

			break;
		}

		case expression_actor::index:
		{
			auto data= lt[0].exp->eval(c);
			auto index=lt[1].exp->eval(c);
			auto size= lt[2].exp->eval(c);


			if(!index.isNum()||!size.isNum())
				throw runtime_error("Cant index bytes with a non number");

			uint64_t b=index.getNum(),e=b+1,s=size.getNum();
			b*=s;
			e*=s;


			auto vl=data.data_description();
			e=min(e,vl.second);

			if(b>=vl.second||e<b)
				throw runtime_error("Overflow indexing data. Max:"
						+to_string(vl.second/s+(vl.second%s)?1:0)
						+" request: "+to_string(index.getNum()));

			expression res(0,expression_actor::u8);
			switch(s){
				case 1:
				{
					 res.actor=expression_actor::u8;
					 res.lt[0].num=vl.first[b];
					 break;
				}
				case 2:
				case 4:
				case 8:
				{
					 res.actor=(expression_actor)s;
					 memcpy(&res.lt[0].num,vl.first+b,e-b);
					 break;
				}
				default:
				{
					res.actor=expression_actor::vector;
					res.lt[0].vec.resize(s,0);
					res.lt[0].vec.insert(res.lt[0].vec.end(),
							vl.first+b,
							vl.first+e
					);
					break;
				}
			}

			return res;

			break;
		}
		case expression_actor::len:
		{
			auto data=lt[0].exp->eval(c);

			return expression(data.isNum()?
						((uint64_t)data.actor)
					:
						data.getVec().size(),
					expression_actor::u64);

			break;
		}
		case expression_actor::randomseq:
		{
			auto data=lt[0].exp->eval(c);
			c.sequences.at(data.asString()).randomize(c.rngSrc);
			break;
		}
		case expression_actor::random:
		{
			if(lt.empty())
			{
				return expression(
						uniform_int_distribution<uint64_t>()(c.rngSrc),
						expression_actor::u64);
			}
			else
			{
				auto l=lt[0].exp->eval(c);
				auto r=lt[1].exp->eval(c);
				if(!l.isNum()|!r.isNum())
					throw runtime_error("Cant clamp random limits with non numbers");
				return expression(
					uniform_int_distribution<uint64_t>(l.getNum(),r.getNum())(c.rngSrc),
					expression_actor::u64);

			}
			break;
		}
		case expression_actor::memcmp:
		{
			auto l=lt[0].exp->eval(c);
			auto r=lt[1].exp->eval(c);
			auto left_dat=l.data_description();
			auto right_dat=r.data_description();
			auto clamp=min(left_dat.second,right_dat.second);
			if(lt.size()>2)
			{
				auto lim=lt[2].exp->eval(c);
				if(!lim.isNum())
					throw runtime_error("Cant use non number to limit"
							" memory comparation");
				clamp=min(lim.getNum(),clamp);
			}

			return expression(
					memcmp(left_dat.first,right_dat.first,clamp)?0:1,
					expression_actor::u8);
			break;
		}
		case expression_actor::system:
		{
			auto exec=  lt[0].exp->eval(c).asString();
			auto stdin= lt[1].exp->eval(c);
			vector<string> args;
			vector<const char*> cargs;
			args.push_back(exec);

			for(auto& v:lt[2].exp->lt)
				args.emplace_back(v.exp->eval(c).asString());


			for(auto&v:args)
				cargs.push_back(v.c_str());

			cargs.push_back(nullptr);

			auto res=spawnProcess(exec,stdin.lt[0].vec,cargs);
			expression ret(0,expression_actor::vector);
			ret.lt[0].vec.insert(
					ret.lt[0].vec.end(),
					res.begin(),
					res.end());
			return ret;
			break;
		}
		case expression_actor::sleep:
		{
			auto time=lt[0].exp->eval(c);
			if(!time.isNum())
				throw runtime_error("Trying to sleep a non-number value");
			c.child.processSleep(time.getNum());
			break;
		}
		case expression_actor::closestdn:
		{
			c.child.stdinCloseWrite();
			break;
		}
		case expression_actor::writestdin:
		{
			auto v=lt[0].exp->eval(c);
			auto desc=v.data_description();
			c.child.stdinWrite(desc.first,desc.second);
			break;
		}
		case expression_actor::_signal:
		{
			c.requestedSignal.push_back(lt[0].exp->eval(c).getNum());
			break;
		}
		case expression_actor::pattern:
		{
			auto pattern=  lt[0].exp->eval(c);
			auto repeated= lt[1].exp->eval(c);

			if(!repeated.isNum())
				throw runtime_error("second value to pattern must"
						" be a number with the total of repetitions");

			auto data=pattern.data_description();
			expression res(0,expression_actor::vector);

			res.lt[0].vec.reserve(data.second*repeated.getNum());
			for(uint64_t i=0;i<repeated.getNum();i++)
				res.lt[0].vec.insert(
							res.lt[0].vec.end(),
							data.first,
							data.first+data.second);
			return res;
		}
		case expression_actor::isdef:
		{
			auto key=  lt[0].exp->eval(c).asString();
			bool res=c.variables->count(key)||c.functions.fns.count(key);
			return expression(res,expression_actor::u8);
		}
		case expression_actor::defun:
		{
			c.functions.put(asString(),*lt[1].exp,*lt[2].exp);
			break;
		}
		case expression_actor::funcall:
		{
			const auto& fn=c.functions[asString()];

			if(fn.parametersNames.size()!=lt[1].exp->lt.size())
				throw runtime_error("Failed to call function "+asString()+
						"wrong number of arguments expected:"+
						to_string(fn.parametersNames.size())+
						" got:"+to_string(lt[1].exp->lt.size()));

			varmap p;

			for(unsigned i=0;i<fn.parametersNames.size();i++)
				p[fn.parametersNames[i]]=lt[1].exp->lt[i].exp->eval(c);

			p.upScope=c.variables;

			try{
				c.variables=&p;
				expression res=fn.code.eval(c);
				c.variables=p.upScope;
				return res;
			}
			catch(const runtime_error &e){
				c.variables=p.upScope;
				throw e;
			}
			catch(...)
			{
				c.variables=p.upScope;
				throw;
			}
			break;
		}
		case expression_actor::steptrace:
		{
			auto steptrace=  lt[0].exp->eval(c);
			c.stepTrace=steptrace.getNum();
			break;
		}
		case expression_actor::getc:
		{
			getchar();
			break;
		}
		case expression_actor::request:
		{
			c.interpRequest=(commandRequest)getNum();
			break;
		}
	}

	return expression();
}

void wadu::debugContext::applyBreakpointChanges(memaddr faddr){
	wadu::wlog(wadu::LL::DEBUG)<<"Applying breakpoint changes ";
	for(auto& br:breakpointActions)
	{
		if(br.action==breakpointAction::action::clear_this)
			br.target=faddr;

		breakpoints.appyAction(br,regs);
	}
	breakpointActions.clear();
}

void wadu::debugContext::mapMemoryMaps(){
	uint32_t index=0;
	varmap& variables=*this->variables;
	variables.variables.emplace(
			"segments",
			expression(memory.segments.size(),expression_actor::u64));

	for(auto& seg:memory.segments){

		variables.variables.emplace("sbegining_"+to_string(index),
				expression(seg.begin,expression_actor::u64));

		variables.variables.emplace("send_"+to_string(index),
				expression(seg.end,expression_actor::u64));

		string flags="---";
		if(seg.r)
			flags[0]='r';
		if(seg.w)
			flags[1]='w';
		if(seg.x)
			flags[2]='x';

		variables.variables.emplace("sflags_"+to_string(index),
				expression(0,expression_actor::vector));

		variables["sflags_"+to_string(index)].lt[0]=flags;
		variables.variables.emplace("segment_source_"+to_string(index),
				expression(0,expression_actor::vector)
		);
		variables["segment_source_"+to_string(index)]=seg.file;

		index++;
	}
}

void wadu::debugContext::miscVariables(uint64_t threadId){
	(*variables)["pid"]=expression(child.id,expression_actor::u64);
	(*variables)["thread"]=expression(threadId,expression_actor::u64);
}

void wadu::debugContext::updates(){
	if(registersEdited){
		registersEdited=false;
		setRegisters(child,regs);
	}
}

void wadu::expression::tearDown(){
	for(auto& var:lt){
		if(var.exp)
		{
			var.exp->tearDown();
			delete var.exp;
			var.exp=nullptr;
		}
	}

}
