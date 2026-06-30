#ifndef CMM_AST_H
#define CMM_AST_H

typedef struct ASTNode ASTNode;

struct ASTNode {
    char *name;
    char *text;
    int line;
    int is_token;
    ASTNode *first_child;
    ASTNode *last_child;
    ASTNode *next_sibling;
};

ASTNode *ast_token(const char *name, const char *text, int line);
ASTNode *ast_nonterminal(const char *name, int line);
ASTNode *ast_branch(const char *name, int child_count, ...);
ASTNode *ast_chain(ASTNode *first, ASTNode *next);
void ast_append(ASTNode *parent, ASTNode *child);
void ast_print(const ASTNode *node, int indent);
void ast_free(ASTNode *node);
int ast_line(const ASTNode *node);
char *ast_strdup(const char *text);

#endif
