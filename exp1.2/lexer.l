%option noyywrap yylineno

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "parser.tab.h"

int lexical_error_count = 0;
static int comment_start_line = 1;

static char *format_int(const char *text, int base)
{
    char buffer[64];
    long value = strtol(text, NULL, base);
    snprintf(buffer, sizeof(buffer), "%ld", value);
    return ast_strdup(buffer);
}

static char *format_float(const char *text)
{
    char buffer[128];
    double value = strtod(text, NULL);
    snprintf(buffer, sizeof(buffer), "%f", value);
    return ast_strdup(buffer);
}

static int emit_plain(int token, const char *name)
{
    yylval.node = ast_token(name, NULL, yylineno);
    return token;
}

static int emit_text(int token, const char *name, const char *text)
{
    yylval.node = ast_token(name, text, yylineno);
    return token;
}

static int emit_owned_text(int token, const char *name, char *text)
{
    yylval.node = ast_token(name, text, yylineno);
    free(text);
    return token;
}

static void report_lexical_error(const char *message, const char *text)
{
    ++lexical_error_count;
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
                                    ++lexical_error_count;
                                    printf("Error type A at Line %d: Unterminated comment.\n",
                                           comment_start_line);
                                    yyterminate();
                                }

[ \t\r]+                       ;
\n                             ;

"int"|"float"                  { return emit_text(TYPE, "TYPE", yytext); }
"struct"                       { return emit_plain(STRUCT, "STRUCT"); }
"return"                       { return emit_plain(RETURN, "RETURN"); }
"if"                           { return emit_plain(IF, "IF"); }
"else"                         { return emit_plain(ELSE, "ELSE"); }
"while"                        { return emit_plain(WHILE, "WHILE"); }

{BAD_HEX}                      { report_lexical_error("Illegal hexadecimal number", yytext); }
{BAD_OCT}                      { report_lexical_error("Illegal octal number", yytext); }
{BAD_FLOAT1}|{BAD_FLOAT2}       { report_lexical_error("Illegal floating point number", yytext); }
{HEXINT}                       { return emit_owned_text(INT, "INT", format_int(yytext, 16)); }
{OCTINT}                       { return emit_owned_text(INT, "INT", format_int(yytext, 8)); }
{FLOAT1}|{FLOAT2}              { return emit_owned_text(FLOAT, "FLOAT", format_float(yytext)); }
{DECINT}                       { return emit_owned_text(INT, "INT", format_int(yytext, 10)); }
{ID}                           { return emit_text(ID, "ID", yytext); }

"=="|"!="|"<="|">="|"<"|">"    { return emit_text(RELOP, "RELOP", yytext); }
"="                            { return emit_plain(ASSIGNOP, "ASSIGNOP"); }
"&&"                           { return emit_plain(AND, "AND"); }
"||"                           { return emit_plain(OR, "OR"); }
"+"                            { return emit_plain(PLUS, "PLUS"); }
"-"                            { return emit_plain(MINUS, "MINUS"); }
"*"                            { return emit_plain(STAR, "STAR"); }
"/"                            { return emit_plain(DIV, "DIV"); }
"!"                            { return emit_plain(NOT, "NOT"); }
"."                            { return emit_plain(DOT, "DOT"); }
";"                            { return emit_plain(SEMI, "SEMI"); }
","                            { return emit_plain(COMMA, "COMMA"); }
"("                            { return emit_plain(LP, "LP"); }
")"                            { return emit_plain(RP, "RP"); }
"["                            { return emit_plain(LB, "LB"); }
"]"                            { return emit_plain(RB, "RB"); }
"{"                            { return emit_plain(LC, "LC"); }
"}"                            { return emit_plain(RC, "RC"); }

.                              { report_lexical_error("Mysterious character", yytext); }

%%
