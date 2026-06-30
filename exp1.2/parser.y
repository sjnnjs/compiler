%{
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"

int yylex(void);
void yyrestart(FILE *input_file);
void yyerror(const char *message);

extern FILE *yyin;
extern int yylineno;
extern int lexical_error_count;
extern int yychar;

ASTNode *syntax_root = NULL;
int syntax_error_count = 0;

static int last_syntax_error_line = 0;
static int muted_syntax_reports = 0;

static void report_syntax_error(int line, const char *message)
{
    if (lexical_error_count > 0) {
        return;
    }
    if (muted_syntax_reports > 0) {
        --muted_syntax_reports;
        return;
    }
    if (line <= 0) {
        line = yylineno;
    }
    if (line == last_syntax_error_line) {
        return;
    }
    last_syntax_error_line = line;
    ++syntax_error_count;
    printf("Error type B at Line %d: %s.\n", line, message);
}

static int node_line(ASTNode *node)
{
    int line = ast_line(node);
    return line > 0 ? line : yylineno;
}

static void report_stray_comment_end(ASTNode *node)
{
    report_syntax_error(node_line(node), "Syntax error");
    muted_syntax_reports = 1;
}
%}

%union {
    ASTNode *node;
}

%token <node> ID TYPE INT FLOAT RELOP
%token <node> STRUCT RETURN IF ELSE WHILE
%token <node> ASSIGNOP AND OR PLUS MINUS STAR DIV NOT DOT
%token <node> SEMI COMMA LP RP LB RB LC RC

%type <node> Program ExtDefList ExtDef ExtDecList
%type <node> Specifier StructSpecifier OptTag Tag
%type <node> VarDec FunDec VarList ParamDec
%type <node> CompSt StmtList Stmt
%type <node> DefList Def DecList Dec
%type <node> Exp Args

%right ASSIGNOP
%left OR
%left AND
%nonassoc RELOP
%left PLUS MINUS
%left STAR DIV
%right NOT UMINUS
%left LP RP LB RB DOT
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

Program
    : ExtDefList
      { syntax_root = ast_branch("Program", 1, $1); $$ = syntax_root; }
    ;

ExtDefList
    : ExtDef ExtDefList
      { $$ = ast_branch("ExtDefList", 2, $1, $2); }
    | /* empty */
      { $$ = NULL; }
    ;

ExtDef
    : Specifier ExtDecList SEMI
      { $$ = ast_branch("ExtDef", 3, $1, $2, $3); }
    | Specifier SEMI
      { $$ = ast_branch("ExtDef", 2, $1, $2); }
    | Specifier FunDec CompSt
      { $$ = ast_branch("ExtDef", 3, $1, $2, $3); }
    | Specifier ExtDecList error
      { report_syntax_error(node_line($2), "Missing \";\""); yyerrok; $$ = NULL; }
    | Specifier FunDec error
      { report_syntax_error(node_line($2), "Syntax error"); yyerrok; $$ = NULL; }
    ;

ExtDecList
    : VarDec
      { $$ = ast_branch("ExtDecList", 1, $1); }
    | VarDec COMMA ExtDecList
      { $$ = ast_branch("ExtDecList", 3, $1, $2, $3); }
    ;

Specifier
    : TYPE
      { $$ = ast_branch("Specifier", 1, $1); }
    | StructSpecifier
      { $$ = ast_branch("Specifier", 1, $1); }
    ;

StructSpecifier
    : STRUCT OptTag LC DefList RC
      { $$ = ast_branch("StructSpecifier", 5, $1, $2, $3, $4, $5); }
    | STRUCT Tag
      { $$ = ast_branch("StructSpecifier", 2, $1, $2); }
    ;

OptTag
    : ID
      { $$ = ast_branch("OptTag", 1, $1); }
    | /* empty */
      { $$ = NULL; }
    ;

Tag
    : ID
      { $$ = ast_branch("Tag", 1, $1); }
    ;

VarDec
    : ID
      { $$ = ast_branch("VarDec", 1, $1); }
    | VarDec LB INT RB
      { $$ = ast_branch("VarDec", 4, $1, $2, $3, $4); }
    ;

FunDec
    : ID LP VarList RP
      { $$ = ast_branch("FunDec", 4, $1, $2, $3, $4); }
    | ID LP RP
      { $$ = ast_branch("FunDec", 3, $1, $2, $3); }
    ;

VarList
    : ParamDec COMMA VarList
      { $$ = ast_branch("VarList", 3, $1, $2, $3); }
    | ParamDec
      { $$ = ast_branch("VarList", 1, $1); }
    ;

ParamDec
    : Specifier VarDec
      { $$ = ast_branch("ParamDec", 2, $1, $2); }
    ;

CompSt
    : LC DefList StmtList RC
      { $$ = ast_branch("CompSt", 4, $1, $2, $3, $4); }
    ;

StmtList
    : Stmt StmtList
      { $$ = ast_branch("StmtList", 2, $1, $2); }
    | /* empty */
      { $$ = NULL; }
    ;

Stmt
    : Exp SEMI
      { $$ = ast_branch("Stmt", 2, $1, $2); }
    | STAR DIV
      { report_stray_comment_end($1); $$ = NULL; }
    | CompSt
      { $$ = ast_branch("Stmt", 1, $1); }
    | RETURN Exp SEMI
      { $$ = ast_branch("Stmt", 3, $1, $2, $3); }
    | IF LP Exp RP Stmt %prec LOWER_THAN_ELSE
      { $$ = ast_branch("Stmt", 5, $1, $2, $3, $4, $5); }
    | IF LP Exp RP Stmt ELSE Stmt
      { $$ = ast_branch("Stmt", 7, $1, $2, $3, $4, $5, $6, $7); }
    | WHILE LP Exp RP Stmt
      { $$ = ast_branch("Stmt", 5, $1, $2, $3, $4, $5); }
    | Exp error
      { report_syntax_error(node_line($1), "Missing \";\""); yyerrok; $$ = NULL; }
    | RETURN Exp error
      { report_syntax_error(node_line($2), "Missing \";\""); yyerrok; $$ = NULL; }
    | error SEMI
      { report_syntax_error(yylineno, "Syntax error"); yyerrok; $$ = NULL; }
    ;

DefList
    : Def DefList
      { $$ = ast_branch("DefList", 2, $1, $2); }
    | /* empty */
      { $$ = NULL; }
    ;

Def
    : Specifier DecList SEMI
      { $$ = ast_branch("Def", 3, $1, $2, $3); }
    | Specifier DecList error
      { report_syntax_error(node_line($2), "Missing \";\""); yyerrok; $$ = NULL; }
    ;

DecList
    : Dec
      { $$ = ast_branch("DecList", 1, $1); }
    | Dec COMMA DecList
      { $$ = ast_branch("DecList", 3, $1, $2, $3); }
    ;

Dec
    : VarDec
      { $$ = ast_branch("Dec", 1, $1); }
    | VarDec ASSIGNOP Exp
      { $$ = ast_branch("Dec", 3, $1, $2, $3); }
    ;

Exp
    : Exp ASSIGNOP Exp
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | Exp AND Exp
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | Exp OR Exp
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | Exp RELOP Exp
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | Exp PLUS Exp
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | Exp MINUS Exp
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | Exp STAR Exp
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | Exp DIV Exp
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | LP Exp RP
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | MINUS Exp %prec UMINUS
      { $$ = ast_branch("Exp", 2, $1, $2); }
    | NOT Exp
      { $$ = ast_branch("Exp", 2, $1, $2); }
    | ID LP Args RP
      { $$ = ast_branch("Exp", 4, $1, $2, $3, $4); }
    | ID LP RP
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | Exp LB Exp RB
      { $$ = ast_branch("Exp", 4, $1, $2, $3, $4); }
    | Exp LB Exp COMMA Exp RB
      { report_syntax_error(node_line($3), "Missing \"]\""); $$ = ast_branch("Exp", 6, $1, $2, $3, $4, $5, $6); }
    | Exp LB Exp error
      { report_syntax_error(node_line($3), "Missing \"]\""); yyerrok; $$ = NULL; }
    | Exp DOT ID
      { $$ = ast_branch("Exp", 3, $1, $2, $3); }
    | ID
      { $$ = ast_branch("Exp", 1, $1); }
    | INT
      { $$ = ast_branch("Exp", 1, $1); }
    | FLOAT
      { $$ = ast_branch("Exp", 1, $1); }
    ;

Args
    : Exp COMMA Args
      { $$ = ast_branch("Args", 3, $1, $2, $3); }
    | Exp
      { $$ = ast_branch("Args", 1, $1); }
    ;

%%

void yyerror(const char *message)
{
    (void)message;
}

int main(int argc, char **argv)
{
    FILE *input;
    int parse_result;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source-file>\n", argv[0]);
        return 1;
    }

    input = fopen(argv[1], "r");
    if (input == NULL) {
        fprintf(stderr, "Cannot open input file: %s\n", argv[1]);
        return 1;
    }

    yyin = input;
    yyrestart(input);
    parse_result = yyparse();
    if (lexical_error_count > 0) {
        while (yylex() != 0) {
        }
    }
    fclose(input);

    if (parse_result != 0 && lexical_error_count == 0 && syntax_error_count == 0) {
        report_syntax_error(yylineno, "Syntax error");
    }

    if (lexical_error_count == 0 && syntax_error_count == 0 && parse_result == 0) {
        ast_print(syntax_root, 0);
    }
    ast_free(syntax_root);

    return (lexical_error_count || syntax_error_count || parse_result) ? 2 : 0;
}
