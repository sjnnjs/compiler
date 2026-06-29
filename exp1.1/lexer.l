%option noyywrap yylineno

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lexical_error = 0;
static int comment_start_line = 1;

static void print_token(const char *name)
{
    printf("%s\n", name);
}

static void print_text_token(const char *name, const char *text)
{
    printf("%s: %s\n", name, text);
}

static void print_int_token(const char *text, int base)
{
    long value = strtol(text, NULL, base);
    printf("INT: %ld\n", value);
}

static void print_float_token(const char *text)
{
    double value = strtod(text, NULL);
    printf("FLOAT: %f\n", value);
}

static void report_error(const char *message, const char *text)
{
    lexical_error = 1;
    if (text == NULL) {
        printf("Error type A at Line %d: %s.\n", yylineno, message);
    } else {
        printf("Error type A at Line %d: %s \"%s\".\n", yylineno, message, text);
    }
}
%}

DIGIT       [0-9]
LETTER      [_A-Za-z]
ID          {LETTER}({LETTER}|{DIGIT})*
DECINT      0|[1-9]{DIGIT}*
OCTINT      0[0-7]+
HEXINT      0[xX][0-9A-Fa-f]+
BAD_OCT     0[0-7]*[89]{DIGIT}*
BAD_HEX     0[xX][0-9A-Fa-f]*[G-Zg-z_]({LETTER}|{DIGIT})*
FLOAT1      ({DIGIT}+\.{DIGIT}*|{DIGIT}*\.{DIGIT}+)([eE][+-]?{DIGIT}+)?
FLOAT2      {DIGIT}+[eE][+-]?{DIGIT}+
BAD_FLOAT1  ({DIGIT}+\.{DIGIT}*|{DIGIT}*\.{DIGIT}+)[eE][+-]?
BAD_FLOAT2  {DIGIT}+[eE][+-]?

%x COMMENT

%%

"//".*                         ;
"/*"                           { comment_start_line = yylineno; BEGIN(COMMENT); }
<COMMENT>"*/"                  { BEGIN(INITIAL); }
<COMMENT>\n                    ;
<COMMENT>.                     ;
<COMMENT><<EOF>>               {
                                    lexical_error = 1;
                                    printf("Error type A at Line %d: Unterminated comment.\n",
                                           comment_start_line);
                                    yyterminate();
                                }

[ \t\r]+                       ;
\n                             ;

"int"|"float"                  { print_text_token("TYPE", yytext); }
"struct"                       { print_token("STRUCT"); }
"return"                       { print_token("RETURN"); }
"if"                           { print_token("IF"); }
"else"                         { print_token("ELSE"); }
"while"                        { print_token("WHILE"); }

{BAD_HEX}                      { report_error("Illegal hexadecimal number", yytext); }
{BAD_OCT}                      { report_error("Illegal octal number", yytext); }
{BAD_FLOAT1}|{BAD_FLOAT2}       { report_error("Illegal floating point number", yytext); }
{HEXINT}                       { print_int_token(yytext, 16); }
{OCTINT}                       { print_int_token(yytext, 8); }
{FLOAT1}|{FLOAT2}              { print_float_token(yytext); }
{DECINT}                       { print_int_token(yytext, 10); }
{ID}                           { print_text_token("ID", yytext); }

"=="|"!="|"<="|">="|"<"|">"    { print_text_token("RELOP", yytext); }
"="                            { print_token("ASSIGNOP"); }
"&&"                           { print_token("AND"); }
"||"                           { print_token("OR"); }
"+"                            { print_token("PLUS"); }
"-"                            { print_token("MINUS"); }
"*"                            { print_token("STAR"); }
"/"                            { print_token("DIV"); }
"!"                            { print_token("NOT"); }
"."                            { print_token("DOT"); }
";"                            { print_token("SEMI"); }
","                            { print_token("COMMA"); }
"("                            { print_token("LP"); }
")"                            { print_token("RP"); }
"["                            { print_token("LB"); }
"]"                            { print_token("RB"); }
"{"                            { print_token("LC"); }
"}"                            { print_token("RC"); }

.                              { report_error("Mysterious character", yytext); }

%%

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source-file>\n", argv[0]);
        return 1;
    }

    yyin = fopen(argv[1], "r");
    if (yyin == NULL) {
        fprintf(stderr, "Cannot open input file: %s\n", argv[1]);
        return 1;
    }

    yylex();
    fclose(yyin);
    return lexical_error ? 2 : 0;
}
