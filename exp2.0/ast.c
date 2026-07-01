#include "ast.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *ast_strdup(const char *text)
{
    char *copy;
    size_t len;

    if (text == NULL) {
        return NULL;
    }
    len = strlen(text) + 1;
    copy = (char *)malloc(len);
    if (copy == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    memcpy(copy, text, len);
    return copy;
}

static ASTNode *ast_new(const char *name, const char *text, int line, int is_token)
{
    ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
    if (node == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    node->name = ast_strdup(name);
    node->text = ast_strdup(text);
    node->line = line;
    node->is_token = is_token;
    node->first_child = NULL;
    node->last_child = NULL;
    node->next_sibling = NULL;
    return node;
}

ASTNode *ast_token(const char *name, const char *text, int line)
{
    return ast_new(name, text, line, 1);
}

ASTNode *ast_nonterminal(const char *name, int line)
{
    return ast_new(name, NULL, line, 0);
}

int ast_line(const ASTNode *node)
{
    const ASTNode *child;

    if (node == NULL) {
        return 0;
    }
    if (node->line > 0) {
        return node->line;
    }
    for (child = node->first_child; child != NULL; child = child->next_sibling) {
        int line = ast_line(child);
        if (line > 0) {
            return line;
        }
    }
    return 0;
}

void ast_append(ASTNode *parent, ASTNode *child)
{
    if (parent == NULL || child == NULL) {
        return;
    }
    if (parent->first_child == NULL) {
        parent->first_child = child;
        parent->last_child = child;
    } else {
        parent->last_child->next_sibling = child;
    }
    while (parent->last_child->next_sibling != NULL) {
        parent->last_child = parent->last_child->next_sibling;
    }
}

ASTNode *ast_chain(ASTNode *first, ASTNode *next)
{
    ASTNode *tail;

    if (first == NULL) {
        return next;
    }
    tail = first;
    while (tail->next_sibling != NULL) {
        tail = tail->next_sibling;
    }
    tail->next_sibling = next;
    return first;
}

ASTNode *ast_branch(const char *name, int child_count, ...)
{
    ASTNode *node;
    ASTNode *child;
    va_list args;
    int i;
    int line = 0;

    va_start(args, child_count);
    for (i = 0; i < child_count; ++i) {
        child = va_arg(args, ASTNode *);
        if (line == 0) {
            line = ast_line(child);
        }
    }
    va_end(args);

    node = ast_nonterminal(name, line);
    va_start(args, child_count);
    for (i = 0; i < child_count; ++i) {
        ast_append(node, va_arg(args, ASTNode *));
    }
    va_end(args);
    return node;
}

static void print_indent(int indent)
{
    int i;

    for (i = 0; i < indent; ++i) {
        putchar(' ');
    }
}

void ast_print(const ASTNode *node, int indent)
{
    const ASTNode *child;

    if (node == NULL) {
        return;
    }
    print_indent(indent);
    if (node->is_token) {
        if (node->text != NULL) {
            printf("%s: %s\n", node->name, node->text);
        } else {
            printf("%s\n", node->name);
        }
    } else {
        printf("%s (%d)\n", node->name, node->line);
        for (child = node->first_child; child != NULL; child = child->next_sibling) {
            ast_print(child, indent + 2);
        }
    }
}

void ast_free(ASTNode *node)
{
    ASTNode *child;

    while (node != NULL) {
        ASTNode *next = node->next_sibling;
        child = node->first_child;
        while (child != NULL) {
            ASTNode *child_next = child->next_sibling;
            child->next_sibling = NULL;
            ast_free(child);
            child = child_next;
        }
        free(node->name);
        free(node->text);
        free(node);
        node = next;
    }
}
