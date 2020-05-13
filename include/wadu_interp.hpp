#ifndef INCLUDE_WADU_INTERP_HPP_
#define INCLUDE_WADU_INTERP_HPP_

#include"syntools.hpp"
#include"interp.hpp"


namespace wadu{

	class wadu_parser{
	public:
		wadu_parser(istream&i);
		wadu_parser(istream&i,const string& name);
		void parse();
		void enableDebug();
		void disableDebug();

		expression result;


		interpLexer lexer;
		waduInterp parser;

	};

}



#endif
