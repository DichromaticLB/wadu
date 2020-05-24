%{
#include<string>
#include<cmath>
#include<fstream>
#include"syntools.hpp"
#define YYSTYPE semval
using namespace std;
%}

%require "3.0.4"
%language "c++"
%define api.value.type {semval}
%define api.namespace {wadu} 
%define parser_class_name {waduInterp}
%parse-param {wadu::interpLexer *lexer}
%parse-param {expression& result}

%code{
#undef yylex
#define yylex lexer->interplex
}

%token TOKEN STRING NUMBER 
%token CAST8 CAST16 CAST32 CAST64 CASTVEC 
%token PRINT SETREG MEMORY FILE MAPFROM SEQUENCE INCREMENT GET
%token MEMWRITE CONCAT DUMP SCRIPT BREAK SLICE EXEC DO AMEMORY AMEMWRITE
%token DISABLEBREAK ENABLEBREAK	 STATUSBREAK LEN SLEFT SRIGHT RANDOM RANDOMSEQ
%token MEMCMP SYSTEM STDIN CLOSEIN	SLEEP SIGNAL PATTERN ISDEF DEFUN STEPTRACE 
%token GETC REQUEST DETACH EXIT RESTART RESET SYSCALLNAME TOSTRING CLOSE
%token IF ELSE WHILE
%token EQ NEQ GEQ LEQ LAND LOR
  
%left ',' 
%left LAND LOR 
%left '^' '&' '|'
%left EQ NEQ
%left '<' '>' GEQ LEQ
%left '!' '~' 
%left SLEFT SRIGHT
%left '+' '-'  
%left '%' 
%left '*'
%left '/'
%left CAST8 CAST16 CAST32 CAST64 CASTVEC
%left '[' ']'
%left '(' ')'
%right ELSE
%%

input: %empty
	| input construct 		{
			result.lt.emplace_back();
			result.lt.back().exp=new expression($2);
		}

statement:  %empty
	| assignment
	| dumpable 
	| value     
	| exec
	| _return
	| write_memory
	| createSequence
	| close
	| randomize_sequence
	| createStream
	| breakpoint_control
	| write_stdin
	| close_stdin
	| sleep
	| signal
	| steptrace
	| getc
	| request

getc: GETC '(' ')' {$$.actor=expression_actor::getc;}
	
_return: BREAK value {
				$$.lt.resize(1);
				$$.actor=expression_actor::_break;
				$$.lt[0].exp=new expression($2);
}

construct: '{' statements '}' {$$=$2;}	
			| IF '(' value ')' construct {
				$$.lt.resize(2);
				$$.actor=expression_actor::_if;
				$$.lt[0].exp=new expression($3);
				$$.lt[1].exp=new expression($5);
			}
			| IF '(' value ')' construct ELSE construct {
				$$.lt.resize(3);
				$$.actor=expression_actor::_if;
				$$.lt[0].exp=new expression($3);
				$$.lt[1].exp=new expression($5);
				$$.lt[2].exp=new expression($7);
			}
			| WHILE '(' value ')' construct {
				$$.lt.resize(2);
				$$.actor=expression_actor::_while;
				$$.lt[0].exp=new expression($3);
				$$.lt[1].exp=new expression($5);
			}
			| DO construct WHILE '(' value ')' ';' {
				$$.lt.resize(2);
				$$.actor=expression_actor::dowhile;
				$$.lt[0].exp=new expression($2);
				$$.lt[1].exp=new expression($5);
			}
			| defun
			| statement ';'
			
steptrace: STEPTRACE '(' value ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::steptrace;
				$$.lt[0].exp=new expression($3);
			}


statements: %empty 
		{
			$$.actor=expression_actor::execlist;
			$$.lt.clear();
		}
	| statements construct 
		{
			$$=$1;
			$$.lt.emplace_back();
			$$.lt.back().exp=new expression($2);
		}	

defun: DEFUN TOKEN '(' tokenlist ')' construct {
				$$.lt.resize(3);
				$$.actor=expression_actor::defun;
				$$.lt[0].vec=$2.lt[0].vec;
				$$.lt[1].exp=new expression($4);
				$$.lt[2].exp=new expression($6);
		}

breakpoint_control:   DISABLEBREAK '(' ')' {
								$$.lt.clear();
								$$.actor=expression_actor::disable_breakpoint;
					}
					| DISABLEBREAK '(' value ')' {
						$$.lt.resize(1);
						$$.lt[0].exp=new expression($3);
						$$.actor=expression_actor::disable_breakpoint;
					}
					| ENABLEBREAK  '(' value ')' {
						$$.lt.resize(1);
						$$.lt[0].exp=new expression($3);
						$$.actor=expression_actor::enable_breakpoint;
					}
					

request:  REQUEST '(' DETACH ')' {
						$$.lt.resize(1);
						$$.actor=expression_actor::request;
						$$.lt[0].num=(uint64_t)commandRequest::detach;
		}
		| REQUEST '(' EXIT ')' {
						$$.lt.resize(1);
						$$.actor=expression_actor::request;
						$$.lt[0].num=(uint64_t)commandRequest::exit;
		}
		| REQUEST '(' RESTART ')' {
						$$.lt.resize(1);
						$$.actor=expression_actor::request;
						$$.lt[0].num=(uint64_t)commandRequest::restart;
		}	
		| REQUEST '(' RESET ')' {
						$$.lt.resize(1);
						$$.actor=expression_actor::request;
						$$.lt[0].num=(uint64_t)commandRequest::reset;
		}
					
castVal:  CAST8   '(' value ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::cast8;
				$$.lt[0].exp=new expression($3);
		}
		| CAST16  '(' value ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::cast16;
				$$.lt[0].exp=new expression($3);
		}
		| CAST32  '(' value ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::cast32;
				$$.lt[0].exp=new expression($3);
		}
		| CAST64  '(' value ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::cast64;
				$$.lt[0].exp=new expression($3);
		}
		| CASTVEC '(' value ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::castVec;
				$$.lt[0].exp=new expression($3);
				}
				
write_stdin: STDIN '(' value ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::writestdin;
				$$.lt[0].exp=new expression($3);
		}

sleep	: SLEEP '(' value ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::sleep;
				$$.lt[0].exp=new expression($3);
		}

close_stdin	: CLOSEIN '(' ')' {
				$$.actor=expression_actor::closestdn;
		}
				
createStream: FILE '(' value ',' STRING ')' {
				$$.lt.resize(2);
				$$.actor=expression_actor::createStream;
				$$.lt[0].exp=new expression($3);
				$$.lt[1].vec=$5.lt[0].vec;
		}
			
close: CLOSE '(' value ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::close;
				$$.lt[0].exp=new expression($3);
		}			
		
signal: SIGNAL '(' value ')' {
				$$.actor=expression_actor::_signal;
		        $$.lt.resize(1);
			    $$.lt[0].exp=new expression($3);
}
		
write_memory: MEMWRITE '(' value ',' value ')' {
				$$.actor=expression_actor::_memory_write;
				$$.lt.resize(2);
				$$.lt[0].exp=new expression($3);
				$$.lt[1].exp=new expression($5);
						}
			| MEMWRITE '(' value ',' value ',' value  ')' {
				$$.actor=expression_actor::_memory_write;
				$$.lt.resize(3);
				$$.lt[0].exp=new expression($3);
				$$.lt[1].exp=new expression($5);
				$$.lt[2].exp=new expression($7);
			}
			|AMEMWRITE '(' value ',' value ')' {
				$$.actor=expression_actor::_amemory_write;
				$$.lt.resize(2);
				$$.lt[0].exp=new expression($3);
				$$.lt[1].exp=new expression($5);
						}
			| AMEMWRITE '(' value ',' value ',' value  ')' {
				$$.actor=expression_actor::_amemory_write;
				$$.lt.resize(3);
				$$.lt[0].exp=new expression($3);
				$$.lt[1].exp=new expression($5);
				$$.lt[2].exp=new expression($7);
			}
		
createSequence: SEQUENCE '(' seqdef ')' {$$=$3;}
		
seqdef: STRING ',' {
		auto name=$1.asString();
		$$.lt.clear();
		$$.actor=expression_actor::_sequence;
		$$.lt.resize(1);
		$$.lt[0]=name;
	}
	| seqdef '{' arith '-' '>' arith '}' {
		$$=$1;
		$$.lt.emplace_back();
		expression* s=new expression();
		s->actor= expression_actor::_sequence_range;
		s->lt.resize(2);
		s->lt[0].exp=new expression($3);
		s->lt[1].exp=new expression($6);
		$$.lt.back().exp=s;
	}
	| seqdef '{' stringlist '}' {
		$$=$1;
		$$.lt.emplace_back();
		$3.actor=expression_actor::_sequence_strlist;
		$$.lt.back().exp=new expression($3);
	}
	
randomize_sequence:	RANDOMSEQ '(' value ')' {
		$$.actor=expression_actor::randomseq;
		$$.lt.resize(1);
		$$.lt[0].exp=new expression($3);
}


stringlist: value {
		expression p;
		p.lt.emplace_back();
		p.lt[0].exp=new expression($1);
		$$=p;
	
	}
	| stringlist ',' value {
		$$=$1;
		$$.lt.emplace_back();
		$$.lt.back().exp=new expression($3);
	}

tokenlist: %empty {
		$$.lt.clear();
	}
	|TOKEN {
		expression p;
		p.lt.emplace_back();
		p.lt[0].vec=$1.lt[0].vec;
		$$=p;
	
	}
	| tokenlist ',' TOKEN {
		$$=$1;
		$$.lt.emplace_back();
		$$.lt.back().vec=$3.lt[0].vec;
	}	
	
		
printableList: value {				
					expression s;
					s.actor=expression_actor::execlist;
					s.lt.emplace_back();
					s.lt[0].exp=new expression($1);
					$$=s;
				}
				|printableList '.' value{
					$$=$1;
					$$.lt.emplace_back();
					$$.lt.back().exp=new expression($3);
				}
		
dumpable: PRINT '(' printableList ',' STRING ')' {
				$$.lt.resize(2);
				$$.actor=expression_actor::print;
				$$.lt[0].exp=new expression($3);
				$$.lt[1].exp=new expression($5);
	
			}
		  |DUMP '(' printableList ',' STRING ')' {
				$$.lt.resize(2);
				$$.actor=expression_actor::dump;
				$$.lt[0].exp=new expression($3);
				$$.lt[1].exp=new expression($5);
	
			}
		  |PRINT '(' printableList ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::print;
				$$.lt[0].exp=new expression($3);
	
			}
		  |DUMP '(' printableList ')' {
				$$.lt.resize(1);
				$$.actor=expression_actor::dump;
				$$.lt[0].exp=new expression($3);
	
			}
		
assignment: '$' TOKEN '='  value {
				$$.lt.resize(2);
				$$.actor=expression_actor::_assignment;
				$$.lt[0].vec=$2.lt[0].vec;
				$$.lt[1].exp=new expression($4);
			}
			| '$' '$' TOKEN '=' value {
				$$.lt.resize(2);
				$$.actor=expression_actor::edit_reg;
				$$.lt[0].vec=$3.getVec();
				$$.lt[1].exp=new expression($5);
			}

exec: EXEC '(' value ')' {
				$$.actor=expression_actor::script;
				$$.lt.resize(1);
				$$.lt[0].exp=new expression($3);
}

value: arith

		
		
arith: NUMBER
	| STRING
	| MEMORY '(' arith ',' arith ')' { //Address count
							$$.actor=expression_actor::_memory;
							$$.lt.resize(2);
							$$.lt[0].exp=new expression($3);
							$$.lt[1].exp=new expression($5);
	}

	| AMEMORY '(' arith ',' arith ')' { //Address count
							$$.actor=expression_actor::_amemory;
							$$.lt.resize(2);
							$$.lt[0].exp=new expression($3);
							$$.lt[1].exp=new expression($5);
	}
	| SCRIPT  '(' value  ')' {
							$$.actor=expression_actor::script;
							$$.lt.resize(1);
							$$.lt[0].exp=new expression($3);
	}
	| MAPFROM '(' STRING ',' arith ',' arith ')' {
							$$.actor=expression_actor::mapFile;
							$$.lt.clear();
							$$.lt.resize(3);
							$$.lt[0].vec=$3.lt[0].vec;
							$$.lt[1].exp=new expression($5);
							$$.lt[2].exp=new expression($7);
	}
	| MAPFROM '(' STRING ')' {
							$$.actor=expression_actor::mapFile;
							$$.lt.clear();
							$$.lt.resize(1);
							$$.lt[0].vec=$3.lt[0].vec;
	}
	| SLICE '(' value ','  value ',' value  ')'  {
							$$.actor=expression_actor::slice;
							$$.lt.resize(3);
							$$.lt[0].exp=new expression($3);
							$$.lt[1].exp=new expression($5);
							$$.lt[2].exp=new expression($7);
	}
	| CONCAT '(' printableList ')' {
		                    $$.actor=expression_actor::concat;
	                        $$.lt.clear();
		                    $$.lt.resize(1);
			                $$.lt[0].exp=new expression($3);
			              }  
	| castVal
	| LEN '(' arith ')'		{
		                    $$.actor=expression_actor::len;
		                    $$.lt.resize(1);
			                $$.lt[0].exp=new expression($3);
			              }       
	| STATUSBREAK '(' value ')'  {
		                    $$.actor=expression_actor::status_breakpoint;
	                        $$.lt.clear();
		                    $$.lt.resize(1);
			                $$.lt[0].exp=new expression($3);
   							 }  
   	| TOSTRING '(' value ')'  {
      $$.actor=expression_actor::tostring;
      $$.lt.clear();
      $$.lt.resize(1);
      $$.lt[0].exp=new expression($3);
   							 }  
	| TOKEN '(' stringlist ')' {
							$$.lt.clear();
							$$.lt.resize(2);
							$$.actor= expression_actor::funcall;
							$$.lt[0].vec=$1.lt[0].vec;  
							$$.lt[1].exp=new expression($3);  
	}  
	| TOKEN '('  ')' {
							$$.lt.clear();
							$$.lt.resize(2);
							$$.actor= expression_actor::funcall;
							$$.lt[0].vec=$1.lt[0].vec;  
							$$.lt[1].exp=new expression();  
	}                 
	| MEMCMP '(' arith ',' arith ')' {
							$$.lt.clear();
							$$.lt.resize(2);
							$$.actor= expression_actor::memcmp;
							$$.lt[0].exp=new expression($3);  
							$$.lt[1].exp=new expression($5);  
	
						}
	| MEMCMP '(' arith ',' arith ',' arith ')' {
							$$.lt.clear();
							$$.lt.resize(3);
							$$.actor= expression_actor::memcmp;
							$$.lt[0].exp=new expression($3);  
							$$.lt[1].exp=new expression($5);  
							$$.lt[2].exp=new expression($7);  
					}						              
	| RANDOM '(' ')'	{
							$$.actor=expression_actor::random;
							$$.lt.clear();
						}
	| SYSCALLNAME '(' value ')'	{
							$$.actor=expression_actor::sysname;
							$$.lt.resize(1);
							$$.lt[0].exp=new expression($3);
						}			
	| RANDOM '(' value ',' value ')' {
							$$.actor=expression_actor::random;
							$$.lt.resize(2);
							$$.lt[0].exp=new expression($3);
							$$.lt[1].exp=new expression($5);
						}
	| SYSTEM '(' value ',' value ',' stringlist ')'	{
							$$.actor=expression_actor::system;
							$$.lt.resize(3);
							$$.lt[0].exp=new expression($3);
							$$.lt[1].exp=new expression($5);
							$$.lt[2].exp=new expression($7);
	}
	| SYSTEM '(' value ',' value ')'	{
							$$.actor=expression_actor::system;
							$$.lt.resize(3);
							$$.lt[0].exp=new expression($3);
							$$.lt[1].exp=new expression($5);
							$$.lt[2].exp=new expression();
	}
	| PATTERN '(' value ',' value ')' {
							$$.actor=expression_actor::pattern;
							$$.lt.resize(2);
							$$.lt[0].exp=new expression($3);
							$$.lt[1].exp=new expression($5);
						}
	| ISDEF '(' value ')' {
							$$.actor=expression_actor::isdef;
							$$.lt.resize(1);
							$$.lt[0].exp=new expression($3);
	}
	| ISDEF '(' TOKEN ')' {
							$$.actor=expression_actor::isdef;
							$$.lt.resize(1);
							$$.lt[0].exp=new expression($3);
	}					
	| '$' '$' TOKEN    {

							$$.actor=expression_actor::_register;
							$$.lt.resize(1);
							$$.lt[0].vec=$3.lt[0].vec;
						}
	| '$' TOKEN 	   {
							$$.actor=expression_actor::var;
							$$.lt.resize(1);
							$$.lt[0].vec=$2.lt[0].vec;
						}
	| arith '+' arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::add;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						}
	| arith '-' arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::sub;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						}
	| arith '*' arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::mult;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						}
	| arith '/' arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::div;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						}
	| arith '&' arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::band;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						}
	| arith '^' arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::_xor;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						}					
	| arith '|' arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::bor;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						}
	| arith LAND arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::land;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						}
	| arith LOR arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::lor;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						 }
	| arith '[' arith ']' {
							$$.lt.resize(3);
							$$.actor= expression_actor::index;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
							$$.lt[2].exp=new expression(1,expression_actor::u8);  
						}
	| arith '[' arith ',' arith ']' {
						$$.lt.resize(3);
						$$.actor= expression_actor::index;
						$$.lt[0].exp=new expression($1);  
						$$.lt[1].exp=new expression($3);  
						$$.lt[2].exp=new expression($5);  
					}															
	| '!' arith    { 
							$$.lt.resize(2);
							$$.actor= expression_actor::lnot;
							$$.lt[0].exp=new expression($2);  
  
						}
	|  '~' arith        { 
							$$.lt.resize(1);
							$$.actor= expression_actor::bnot;
							$$.lt[0].exp=new expression($2);  

						}																							
	| arith '%' arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::mod;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
					}					
	| arith SLEFT arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::lshift;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						   }
	| arith SRIGHT arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::rshift;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						   }
						   
	| arith EQ arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::eq;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						   }
					   
	| arith NEQ arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::neq;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						   }						   
	| arith '>'  arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::gt;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						   }
	| arith '<' arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::lt;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						   }
	| arith GEQ  arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::gte;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						   }
	| arith LEQ arith  { 
							$$.lt.resize(2);
							$$.actor= expression_actor::lte;
							$$.lt[0].exp=new expression($1);  
							$$.lt[1].exp=new expression($3);  
						   }
	| INCREMENT '(' STRING ')'	{
							$$=$3;
							$$.actor= 
								expression_actor::_sequence_increment;
 
	}
	| GET '(' STRING ',' arith ')' {
							$$=$3;
							$$.lt.resize(2);
							$$.actor= 
								expression_actor::_sequence_get;
							$$.lt[1].exp=new expression($5);  
	
	}					   
	| '(' arith ')'		   {$$=$2;}


		
%%

void wadu::waduInterp::error(const string& s){
	cerr<<s<<" line:"<<to_string(lexer->linecount)
		<<" sourcename:"<<lexer->name<<endl;
}