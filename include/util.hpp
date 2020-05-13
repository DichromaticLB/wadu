#ifndef INCLUDE_UTIL_HPP_
#define INCLUDE_UTIL_HPP_
#include<string>
#include<sstream>
#include<iomanip>

namespace wadu{
	template<class T>
	std::string hexstr(const T& t){
		std::stringstream ss;
		ss<<std::setfill('0')<<std::setw(sizeof(T))<<std::hex<<t;
		return ss.str();
	}

	enum class LL:uint8_t{
		DEBUG=0,
		TRACE=1,
		INFO=2,
		WARNING=3,
		ERROR=4,
		CRITICAL=5,
		NONE=0xff
	};

	class wlog{
	public:
		wlog(LL e){
			level=e;
		}

		static void setLevel(LL l){
			currentLevel=l;
		}

		bool shouldLog() const{
			return level>=currentLevel;
		}

		static void setLevel(const string&s){
			static const unordered_map<string,LL> s2l={
					{"DEBUG"   ,LL::DEBUG    },
					{"TRACE"   ,LL::TRACE    },
					{"INFO"    ,LL::INFO     },
					{"WARNING" ,LL::WARNING  },
					{"ERROR"   ,LL::ERROR    },
					{"CRITICAL",LL::CRITICAL },
					{"NONE"   , LL::NONE     }
			};
			string ss=s;
			std::transform(ss.begin(), ss.end(),ss.begin(), ::toupper);
			setLevel(s2l.at(ss));
		}

		template<class T>
		wlog& operator<<(const T& t){
			if(level>=currentLevel)
				ss<<t;
			return *this;
		}

		~wlog(){
			if(level>=currentLevel)
			{
				ss<<endl;
				std::cerr<<ss.str();
			}
		}


	private:
		stringstream ss;
		LL level;
		static LL currentLevel;
	};
}

#endif
