#include"wadu_interp.hpp"


wadu::wadu_parser::wadu_parser(istream&i):
	result(),lexer(i),parser(&lexer,result){
	result.lt.clear();
	result.actor=expression_actor::execlist;
}

wadu::wadu_parser::wadu_parser(istream&i,const string& name):wadu_parser(i){
	lexer.name=name;
}

void wadu::wadu_parser::parse(){
	parser.parse();
}

void wadu::wadu_parser::enableDebug(){
#ifdef SYNTAXDEBUGGING
	lexer.set_debug(5);
	parser.debug_level(5);
#endif
}

void wadu::wadu_parser::disableDebug(){
#ifdef SYNTAXDEBUGGING
	lexer.set_debug(0);
	parser.set_debug_level(0);
#endif
}
