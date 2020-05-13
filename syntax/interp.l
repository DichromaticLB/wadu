%{
#include<string>
#include<iostream>
#include"syntools.hpp"
#include"interp.hpp"
using namespace std;
#define YYSTYPE semval
#define YY_DECL int wadu::interpLexer::interplex(semval* yylval)

static unsigned char fastHex(char a,char b){
	static char table[]={
		0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,0,10,11,12,13,14,15,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		10,11,12,13,14,15
	};
	
	return table[a-'0']*16+table[b-'0'];	
}

%}

%option noyywrap
%option nounput
%option c++
%x STRINGS STRINGSSQ COMMENTS

%%

"/*"	       {BEGIN(COMMENTS);}

<COMMENTS>{
"*/" {BEGIN(INITIAL);}
.   {}
}

U64        {return waduInterp::token_type::CAST64;}
U32        {return waduInterp::token_type::CAST32;}
U16        {return waduInterp::token_type::CAST16;}
U8         {return waduInterp::token_type::CAST8;}
VECTOR     {return waduInterp::token_type::CASTVEC;}
PRINT      {return waduInterp::token_type::PRINT;}
DUMP       {return waduInterp::token_type::DUMP;}
MEMREAD     {return waduInterp::token_type::MEMORY;}
MEMWRITE   {return waduInterp::token_type::MEMWRITE;}
AMEMREAD   {return waduInterp::token_type::AMEMORY;}
AMEMWRITE  {return waduInterp::token_type::AMEMWRITE;}
FILE       {return waduInterp::token_type::FILE;}
MAPFROM    {return waduInterp::token_type::MAPFROM;}
SEQUENCE   {return waduInterp::token_type::SEQUENCE;}
INCREMENT  {return waduInterp::token_type::INCREMENT;}
GET		   {return waduInterp::token_type::GET;}
CONCAT	   {return waduInterp::token_type::CONCAT;}
SCRIPT     {return waduInterp::token_type::SCRIPT;}
BREAK      {return waduInterp::token_type::BREAK;}
SLICE      {return waduInterp::token_type::SLICE;}
DISABLEBREAK  {return waduInterp::token_type::DISABLEBREAK;}
ENABLEBREAK	  {return waduInterp::token_type::ENABLEBREAK;}
STATUSBREAK   {return waduInterp::token_type::STATUSBREAK;}
LEN			{return waduInterp::token_type::LEN;}
RANDOM		{return waduInterp::token_type::RANDOM;}
RANDOMSEQ	{return waduInterp::token_type::RANDOMSEQ;}
MEMCMP		{return waduInterp::token_type::MEMCMP;}
SYSTEM		{return waduInterp::token_type::SYSTEM;}
SLEEP		{return waduInterp::token_type::SLEEP;}
STDIN		{return waduInterp::token_type::STDIN;}
CLOSEIN		{return waduInterp::token_type::CLOSEIN;}
SIGNAL 		{return waduInterp::token_type::SIGNAL;}
PATTERN		{return waduInterp::token_type::PATTERN;}
ISDEF		{return waduInterp::token_type::ISDEF;}
DEFUN		{return waduInterp::token_type::DEFUN;}
STEPTRACE	{return waduInterp::token_type::STEPTRACE;}
GETC		{return waduInterp::token_type::GETC;}
REQUEST     {return waduInterp::token_type::REQUEST ;}
DETACH      {return waduInterp::token_type::DETACH  ;}
EXIT        {return waduInterp::token_type::EXIT    ;}
RESTART     {return waduInterp::token_type::RESTART ;}
RESET       {return waduInterp::token_type::RESET   ;}

if     {return waduInterp::token_type::IF;} 
else   {return waduInterp::token_type::ELSE;}
while   {return waduInterp::token_type::WHILE;}
do 		   {return waduInterp::token_type::DO;}

==	   {return waduInterp::token_type::EQ;}
!=	   {return waduInterp::token_type::NEQ;} 

"&&" {return waduInterp::token_type::LAND;} 
"||" {return waduInterp::token_type::LOR;} 


">="	   {return waduInterp::token_type::GEQ;} 
"<="	   {return waduInterp::token_type::LEQ;} 

"<<"     {return waduInterp::token_type::SLEFT;}
">>"     {return waduInterp::token_type::SRIGHT;}

[a-zA-Z_][a-zA-Z_0-9]* { 
	*yylval=expression(yytext,yyleng); 
	return waduInterp::token_type::TOKEN;
	}


0|([1-9][0-9]*) {
	*yylval=expression(strtoull(yytext,0,0),expression_actor::u64);
	return waduInterp::token_type::NUMBER;
}

0x([A-Fa-f0-9]+) {
	*yylval=expression(strtoull(yytext,0,16),expression_actor::u64);
	return waduInterp::token_type::NUMBER;
}


\" {BEGIN(STRINGS);*yylval=expression(0,expression_actor::vector); }
\' {BEGIN(STRINGSSQ);*yylval=expression(0,expression_actor::vector); }


\n { linecount++;}
[ \t\r] {}

<STRINGS>{

\\\" {yylval->lt[0].vec.push_back('"' );}
\\\\ {yylval->lt[0].vec.push_back('\\');}
\\b  {yylval->lt[0].vec.push_back('\b');}
\\f  {yylval->lt[0].vec.push_back('\f');}
\\n  {yylval->lt[0].vec.push_back('\n');}
\\r  {yylval->lt[0].vec.push_back('\r');}
\\t  {yylval->lt[0].vec.push_back('\t');}
\\x[a-fA-F0-9]{2} {yylval->lt[0].vec.push_back(fastHex(yytext[2],yytext[3]));}

\"   {BEGIN(INITIAL); return waduInterp::token_type::STRING;}	

. { yylval->lt[0].vec.push_back(yytext[0]); }
}

<STRINGSSQ>{

\\' {yylval->lt[0].vec.push_back('\'' );}
\\\\ {yylval->lt[0].vec.push_back('\\');}
\\b  {yylval->lt[0].vec.push_back('\b');}
\\f  {yylval->lt[0].vec.push_back('\f');}
\\n  {yylval->lt[0].vec.push_back('\n');}
\\r  {yylval->lt[0].vec.push_back('\r');}
\\t  {yylval->lt[0].vec.push_back('\t');}
\\x[a-fA-F0-9]{2} {yylval->lt[0].vec.push_back(fastHex(yytext[2],yytext[3]));}

\'   {BEGIN(INITIAL); return waduInterp::token_type::STRING;}	

. { yylval->lt[0].vec.push_back(yytext[0]); }
}


. {return yytext[0];}

%%


/*Dummy*/
int interpFlexLexer::yylex(){
	return 0;
}
