#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TYPE_BASIC,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_FUNCTION,
    TYPE_ERROR
} TypeKind;

typedef enum {
    BASIC_INT,
    BASIC_FLOAT
} BasicKind;

typedef struct Type Type;
typedef struct Field Field;
typedef struct Symbol Symbol;

#define TYPE_TEXT_SIZE 256

struct Type {
    TypeKind kind;
    BasicKind basic;
    Type *elem;
    int array_size;
    char *struct_name;
    Field *fields;
    Type *return_type;
    Field *params;
    int param_count;
};

struct Field {
    char *name;
    Type *type;
    int line;
    Field *next;
};

struct Symbol {
    char *name;
    Type *type;
    int line;
    Symbol *next;
};

typedef struct {
    Symbol *variables;
    Symbol *functions;
    Symbol *structures;
    int error_count;
    Type *current_return_type;
} SemanticContext;

static Type type_int_value = { TYPE_BASIC, BASIC_INT, NULL, 0, NULL, NULL, NULL, NULL, 0 };
static Type type_float_value = { TYPE_BASIC, BASIC_FLOAT, NULL, 0, NULL, NULL, NULL, NULL, 0 };
static Type type_error_value = { TYPE_ERROR, BASIC_INT, NULL, 0, NULL, NULL, NULL, NULL, 0 };

static ASTNode *child_at(ASTNode *node, int index)
{
    ASTNode *child = node ? node->first_child : NULL;
    int i;

    for (i = 0; child != NULL && i < index; ++i) {
        child = child->next_sibling;
    }
    return child;
}

static int child_count(ASTNode *node)
{
    int count = 0;
    ASTNode *child;

    for (child = node ? node->first_child : NULL; child != NULL; child = child->next_sibling) {
        ++count;
    }
    return count;
}

static int is_node(ASTNode *node, const char *name)
{
    return node != NULL && node->name != NULL && strcmp(node->name, name) == 0;
}

static int is_token(ASTNode *node, const char *name)
{
    return node != NULL && node->is_token && strcmp(node->name, name) == 0;
}

static ASTNode *find_child(ASTNode *node, const char *name)
{
    ASTNode *child;

    for (child = node ? node->first_child : NULL; child != NULL; child = child->next_sibling) {
        if (is_node(child, name) || is_token(child, name)) {
            return child;
        }
    }
    return NULL;
}

static Type *new_type(TypeKind kind)
{
    Type *type = (Type *)calloc(1, sizeof(Type));
    if (type == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    type->kind = kind;
    return type;
}

static Field *new_field(const char *name, Type *type, int line)
{
    Field *field = (Field *)calloc(1, sizeof(Field));
    if (field == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    field->name = ast_strdup(name);
    field->type = type;
    field->line = line;
    return field;
}

static Symbol *new_symbol(const char *name, Type *type, int line)
{
    Symbol *symbol = (Symbol *)calloc(1, sizeof(Symbol));
    if (symbol == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    symbol->name = ast_strdup(name);
    symbol->type = type;
    symbol->line = line;
    return symbol;
}

static Type *basic_type(const char *name)
{
    if (strcmp(name, "float") == 0) {
        return &type_float_value;
    }
    return &type_int_value;
}

static void semantic_error(SemanticContext *ctx, int type, int line, const char *message)
{
    ++ctx->error_count;
    printf("Error type %d at Line %d: %s.\n", type, line, message);
}

static void semantic_error_name(SemanticContext *ctx, int type, int line,
                                const char *prefix, const char *name)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s \"%s\"", prefix, name);
    semantic_error(ctx, type, line, buffer);
}

static void semantic_error_quoted_suffix(SemanticContext *ctx, int type, int line,
                                         const char *name, const char *suffix)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "\"%s\" %s", name, suffix);
    semantic_error(ctx, type, line, buffer);
}

static void append_text(char *buffer, size_t size, const char *text)
{
    size_t used = strlen(buffer);

    if (used + 1 < size) {
        snprintf(buffer + used, size - used, "%s", text);
    }
}

static void append_type_text(Type *type, char *buffer, size_t size)
{
    if (type == NULL) {
        append_text(buffer, size, "unknown");
        return;
    }

    switch (type->kind) {
    case TYPE_BASIC:
        append_text(buffer, size, type->basic == BASIC_FLOAT ? "float" : "int");
        break;
    case TYPE_ARRAY:
        append_type_text(type->elem, buffer, size);
        append_text(buffer, size, "[]");
        break;
    case TYPE_STRUCT:
        append_text(buffer, size, "struct ");
        append_text(buffer, size, type->struct_name != NULL ? type->struct_name : "<anonymous>");
        break;
    case TYPE_FUNCTION:
        append_text(buffer, size, "function");
        break;
    case TYPE_ERROR:
        append_text(buffer, size, "error");
        break;
    }
}

static void append_field_type_list(Field *fields, char *buffer, size_t size)
{
    Field *field;

    append_text(buffer, size, "(");
    for (field = fields; field != NULL; field = field->next) {
        if (field != fields) {
            append_text(buffer, size, ", ");
        }
        append_type_text(field->type, buffer, size);
    }
    append_text(buffer, size, ")");
}

static void report_function_argument_error(SemanticContext *ctx, int line,
                                           const char *name, Type *function_type,
                                           const char *actual_args)
{
    char params[TYPE_TEXT_SIZE] = "";
    char message[TYPE_TEXT_SIZE * 2] = "";

    append_field_type_list(function_type->params, params, sizeof(params));
    snprintf(message, sizeof(message),
             "Function \"%s%s\" is not applicable for arguments \"%s\"",
             name, params, actual_args);
    semantic_error(ctx, 9, line, message);
}

static void start_arg_text(char *buffer, size_t size)
{
    snprintf(buffer, size, "(");
}

static void append_arg_text(char *buffer, size_t size, Type *type, int index)
{
    if (index > 0) {
        append_text(buffer, size, ", ");
    }
    append_type_text(type, buffer, size);
}

static void finish_arg_text(char *buffer, size_t size)
{
    append_text(buffer, size, ")");
}

static Symbol *find_symbol(Symbol *head, const char *name)
{
    Symbol *symbol;

    for (symbol = head; symbol != NULL; symbol = symbol->next) {
        if (strcmp(symbol->name, name) == 0) {
            return symbol;
        }
    }
    return NULL;
}

static void add_symbol(Symbol **head, Symbol *symbol)
{
    symbol->next = *head;
    *head = symbol;
}

static Field *find_field(Field *head, const char *name)
{
    Field *field;

    for (field = head; field != NULL; field = field->next) {
        if (strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static int same_type(Type *left, Type *right)
{
    if (left == NULL || right == NULL) {
        return 0;
    }
    if (left->kind == TYPE_ERROR || right->kind == TYPE_ERROR) {
        return 1;
    }
    if (left->kind != right->kind) {
        return 0;
    }
    switch (left->kind) {
    case TYPE_BASIC:
        return left->basic == right->basic;
    case TYPE_ARRAY:
        return same_type(left->elem, right->elem);
    case TYPE_STRUCT:
        if (left->struct_name == NULL || right->struct_name == NULL) {
            return left == right;
        }
        return strcmp(left->struct_name, right->struct_name) == 0;
    case TYPE_FUNCTION:
        return 0;
    case TYPE_ERROR:
        return 1;
    }
    return 0;
}

static int is_numeric(Type *type)
{
    return type != NULL && (type->kind == TYPE_BASIC || type->kind == TYPE_ERROR);
}

static int is_int(Type *type)
{
    return type != NULL &&
           (type->kind == TYPE_ERROR || (type->kind == TYPE_BASIC && type->basic == BASIC_INT));
}

static const char *var_dec_name(ASTNode *var_dec)
{
    ASTNode *first;

    if (var_dec == NULL) {
        return NULL;
    }
    first = child_at(var_dec, 0);
    if (is_token(first, "ID")) {
        return first->text;
    }
    return var_dec_name(first);
}

static int var_dec_line(ASTNode *var_dec)
{
    ASTNode *first = child_at(var_dec, 0);

    if (is_token(first, "ID")) {
        return first->line;
    }
    return var_dec_line(first);
}

static Type *apply_var_dec(Type *base, ASTNode *var_dec)
{
    ASTNode *first = child_at(var_dec, 0);
    Type *array_type;
    ASTNode *size_node;

    if (is_token(first, "ID")) {
        return base;
    }

    array_type = new_type(TYPE_ARRAY);
    array_type->elem = apply_var_dec(base, first);
    size_node = child_at(var_dec, 2);
    array_type->array_size = size_node && size_node->text ? atoi(size_node->text) : 0;
    return array_type;
}

static Type *analyze_specifier(SemanticContext *ctx, ASTNode *specifier);
static void analyze_def_list(SemanticContext *ctx, ASTNode *def_list, int in_struct, Field **fields);
static void analyze_stmt_list(SemanticContext *ctx, ASTNode *stmt_list);
static Type *analyze_exp(SemanticContext *ctx, ASTNode *exp);

static Type *make_struct_type(const char *name, Field *fields)
{
    Type *type = new_type(TYPE_STRUCT);
    type->struct_name = name ? ast_strdup(name) : NULL;
    type->fields = fields;
    return type;
}

static Type *analyze_struct_specifier(SemanticContext *ctx, ASTNode *struct_spec)
{
    ASTNode *second = child_at(struct_spec, 1);
    ASTNode *def_list;
    const char *name = NULL;
    Field *fields = NULL;
    Type *struct_type;
    Symbol *existing;
    int line = ast_line(struct_spec);

    if (is_node(second, "Tag")) {
        ASTNode *id = child_at(second, 0);
        existing = find_symbol(ctx->structures, id->text);
        if (existing == NULL) {
            semantic_error_name(ctx, 17, id->line, "Undefined structure", id->text);
            return &type_error_value;
        }
        return existing->type;
    }

    if (is_node(second, "OptTag")) {
        ASTNode *id = child_at(second, 0);
        if (id != NULL) {
            name = id->text;
            line = id->line;
        }
    }

    if (name != NULL &&
        (find_symbol(ctx->structures, name) != NULL || find_symbol(ctx->variables, name) != NULL)) {
        semantic_error_name(ctx, 16, line, "Duplicated name", name);
    }

    def_list = find_child(struct_spec, "DefList");
    analyze_def_list(ctx, def_list, 1, &fields);
    struct_type = make_struct_type(name, fields);
    if (name != NULL && find_symbol(ctx->structures, name) == NULL) {
        add_symbol(&ctx->structures, new_symbol(name, struct_type, line));
    }
    return struct_type;
}

static Type *analyze_specifier(SemanticContext *ctx, ASTNode *specifier)
{
    ASTNode *child = child_at(specifier, 0);

    if (is_token(child, "TYPE")) {
        return basic_type(child->text);
    }
    return analyze_struct_specifier(ctx, child);
}

static void add_variable(SemanticContext *ctx, const char *name, Type *type, int line)
{
    if (find_symbol(ctx->variables, name) != NULL || find_symbol(ctx->structures, name) != NULL) {
        semantic_error_name(ctx, 3, line, "Redefined variable", name);
        return;
    }
    add_symbol(&ctx->variables, new_symbol(name, type, line));
}

static void append_field(Field **head, Field *field)
{
    Field *tail;

    if (*head == NULL) {
        *head = field;
        return;
    }
    tail = *head;
    while (tail->next != NULL) {
        tail = tail->next;
    }
    tail->next = field;
}

static void analyze_dec(SemanticContext *ctx, ASTNode *dec, Type *base, int in_struct, Field **fields)
{
    ASTNode *var_dec = child_at(dec, 0);
    ASTNode *assign = child_at(dec, 1);
    const char *name = var_dec_name(var_dec);
    int line = var_dec_line(var_dec);
    Type *type = apply_var_dec(base, var_dec);

    if (in_struct) {
        if (find_field(*fields, name) != NULL) {
            semantic_error_name(ctx, 15, line, "Redefined field", name);
        } else if (assign != NULL) {
            semantic_error(ctx, 15, line, "Initialized field in structure");
            append_field(fields, new_field(name, type, line));
        } else {
            append_field(fields, new_field(name, type, line));
        }
    } else {
        add_variable(ctx, name, type, line);
        if (assign != NULL) {
            Type *right = analyze_exp(ctx, child_at(dec, 2));
            if (!same_type(type, right)) {
                semantic_error(ctx, 5, ast_line(assign), "Type mismatched for assignment");
            }
        }
    }
}

static void analyze_dec_list(SemanticContext *ctx, ASTNode *dec_list, Type *base,
                             int in_struct, Field **fields)
{
    if (dec_list == NULL) {
        return;
    }
    analyze_dec(ctx, child_at(dec_list, 0), base, in_struct, fields);
    if (child_count(dec_list) == 3) {
        analyze_dec_list(ctx, child_at(dec_list, 2), base, in_struct, fields);
    }
}

static void analyze_def(SemanticContext *ctx, ASTNode *def, int in_struct, Field **fields)
{
    Type *base = analyze_specifier(ctx, child_at(def, 0));
    analyze_dec_list(ctx, child_at(def, 1), base, in_struct, fields);
}

static void analyze_def_list(SemanticContext *ctx, ASTNode *def_list, int in_struct, Field **fields)
{
    if (def_list == NULL) {
        return;
    }
    analyze_def(ctx, child_at(def_list, 0), in_struct, fields);
    analyze_def_list(ctx, child_at(def_list, 1), in_struct, fields);
}

static Field *analyze_param_dec(SemanticContext *ctx, ASTNode *param_dec)
{
    Type *base = analyze_specifier(ctx, child_at(param_dec, 0));
    ASTNode *var_dec = child_at(param_dec, 1);
    const char *name = var_dec_name(var_dec);
    int line = var_dec_line(var_dec);
    Type *type = apply_var_dec(base, var_dec);

    add_variable(ctx, name, type, line);
    return new_field(name, type, line);
}

static void append_params_from_var_list(SemanticContext *ctx, ASTNode *var_list,
                                        Field **params, int *count)
{
    Field *param;

    if (var_list == NULL) {
        return;
    }
    param = analyze_param_dec(ctx, child_at(var_list, 0));
    append_field(params, param);
    ++*count;
    if (child_count(var_list) == 3) {
        append_params_from_var_list(ctx, child_at(var_list, 2), params, count);
    }
}

static Type *analyze_fun_dec(SemanticContext *ctx, ASTNode *fun_dec, Type *return_type)
{
    ASTNode *id = child_at(fun_dec, 0);
    Type *func_type = new_type(TYPE_FUNCTION);
    Symbol *existing = find_symbol(ctx->functions, id->text);

    if (existing != NULL) {
        semantic_error_name(ctx, 4, id->line, "Redefined function", id->text);
    }

    func_type->return_type = return_type;
    if (child_count(fun_dec) == 4) {
        append_params_from_var_list(ctx, child_at(fun_dec, 2),
                                    &func_type->params, &func_type->param_count);
    }
    if (existing == NULL) {
        add_symbol(&ctx->functions, new_symbol(id->text, func_type, id->line));
    }
    return func_type;
}

static int is_left_value(ASTNode *exp)
{
    if (child_count(exp) == 1 && is_token(child_at(exp, 0), "ID")) {
        return 1;
    }
    if (child_count(exp) == 4 && is_token(child_at(exp, 1), "LB")) {
        return 1;
    }
    if (child_count(exp) == 3 && is_token(child_at(exp, 1), "DOT")) {
        return 1;
    }
    return 0;
}

static Type *analyze_args(SemanticContext *ctx, ASTNode *args, Field *params,
                          int *arg_count, int *mismatch,
                          char *arg_text, size_t arg_text_size)
{
    Type *arg_type;
    Field *next_param = params;

    if (args == NULL) {
        if (params != NULL) {
            *mismatch = 1;
        }
        return NULL;
    }

    arg_type = analyze_exp(ctx, child_at(args, 0));
    append_arg_text(arg_text, arg_text_size, arg_type, *arg_count);
    ++*arg_count;
    if (next_param == NULL || !same_type(arg_type, next_param->type)) {
        *mismatch = 1;
    } else {
        next_param = next_param->next;
    }

    if (child_count(args) == 3) {
        analyze_args(ctx, child_at(args, 2), next_param, arg_count, mismatch,
                     arg_text, arg_text_size);
    } else if (next_param != NULL) {
        *mismatch = 1;
    }
    return arg_type;
}

static Type *analyze_binary_numeric(SemanticContext *ctx, ASTNode *exp, int require_int)
{
    Type *left = analyze_exp(ctx, child_at(exp, 0));
    Type *right = analyze_exp(ctx, child_at(exp, 2));
    int line = ast_line(child_at(exp, 1));

    if (require_int) {
        if (left->kind == TYPE_ERROR || right->kind == TYPE_ERROR) {
            return &type_error_value;
        }
        if (!is_int(left) || !is_int(right)) {
            semantic_error(ctx, 7, line, "Type mismatched for operands");
            return &type_error_value;
        }
        return &type_int_value;
    }

    if (left->kind == TYPE_ERROR || right->kind == TYPE_ERROR) {
        return &type_error_value;
    }
    if (!is_numeric(left) || !is_numeric(right) || !same_type(left, right)) {
        semantic_error(ctx, 7, line, "Type mismatched for operands");
        return &type_error_value;
    }
    return left;
}

static Type *analyze_exp(SemanticContext *ctx, ASTNode *exp)
{
    int count = child_count(exp);
    ASTNode *first = child_at(exp, 0);
    ASTNode *second = child_at(exp, 1);

    if (count == 1) {
        if (is_token(first, "ID")) {
            Symbol *var = find_symbol(ctx->variables, first->text);
            if (var == NULL) {
                semantic_error_name(ctx, 1, first->line, "Undefined variable", first->text);
                return &type_error_value;
            }
            return var->type;
        }
        if (is_token(first, "INT")) {
            return &type_int_value;
        }
        if (is_token(first, "FLOAT")) {
            return &type_float_value;
        }
    }

    if (count == 2) {
        Type *operand = analyze_exp(ctx, child_at(exp, 1));
        if (is_token(first, "NOT")) {
            if (!is_int(operand)) {
                semantic_error(ctx, 7, first->line, "Type mismatched for operands");
                return &type_error_value;
            }
            return &type_int_value;
        }
        if (!is_numeric(operand)) {
            semantic_error(ctx, 7, first->line, "Type mismatched for operands");
            return &type_error_value;
        }
        return operand;
    }

    if (count == 3) {
        if (is_token(first, "LP")) {
            return analyze_exp(ctx, child_at(exp, 1));
        }
        if (is_token(second, "ASSIGNOP")) {
            Type *left = analyze_exp(ctx, first);
            Type *right = analyze_exp(ctx, child_at(exp, 2));
            if (!is_left_value(first)) {
                semantic_error(ctx, 6, ast_line(first), "The left-hand side of an assignment must be a variable");
            } else if (!same_type(left, right)) {
                semantic_error(ctx, 5, second->line, "Type mismatched for assignment");
            }
            return left;
        }
        if (is_token(second, "DOT")) {
            Type *base = analyze_exp(ctx, first);
            ASTNode *field_id = child_at(exp, 2);
            Field *field;
            if (base->kind != TYPE_STRUCT) {
                semantic_error(ctx, 13, second->line, "Illegal use of \".\"");
                return &type_error_value;
            }
            field = find_field(base->fields, field_id->text);
            if (field == NULL) {
                semantic_error_name(ctx, 14, field_id->line, "Non-existent field", field_id->text);
                return &type_error_value;
            }
            return field->type;
        }
        if (is_token(second, "PLUS") || is_token(second, "MINUS") ||
            is_token(second, "STAR") || is_token(second, "DIV")) {
            return analyze_binary_numeric(ctx, exp, 0);
        }
        if (is_token(second, "RELOP")) {
            Type *rel = analyze_binary_numeric(ctx, exp, 0);
            return rel->kind == TYPE_ERROR ? rel : &type_int_value;
        }
        if (is_token(second, "AND") || is_token(second, "OR")) {
            return analyze_binary_numeric(ctx, exp, 1);
        }
        if (is_token(first, "ID") && is_token(child_at(exp, 1), "LP")) {
            Symbol *func = find_symbol(ctx->functions, first->text);
            Symbol *var = find_symbol(ctx->variables, first->text);
            if (func == NULL) {
                if (var != NULL) {
                    semantic_error_quoted_suffix(ctx, 11, first->line, first->text, "is not a function");
                } else {
                    semantic_error_name(ctx, 2, first->line, "Undefined function", first->text);
                }
                return &type_error_value;
            }
            if (func->type->param_count != 0) {
                report_function_argument_error(ctx, first->line, first->text, func->type, "()");
            }
            return func->type->return_type;
        }
    }

    if (count == 4) {
        if (is_token(first, "ID") && is_token(second, "LP")) {
            Symbol *func = find_symbol(ctx->functions, first->text);
            Symbol *var = find_symbol(ctx->variables, first->text);
            int arg_count = 0;
            int mismatch = 0;
            char actual_args[TYPE_TEXT_SIZE] = "";
            start_arg_text(actual_args, sizeof(actual_args));
            if (func == NULL) {
                if (var != NULL) {
                    semantic_error_quoted_suffix(ctx, 11, first->line, first->text, "is not a function");
                } else {
                    semantic_error_name(ctx, 2, first->line, "Undefined function", first->text);
                }
                analyze_args(ctx, child_at(exp, 2), NULL, &arg_count, &mismatch,
                             actual_args, sizeof(actual_args));
                return &type_error_value;
            }
            analyze_args(ctx, child_at(exp, 2), func->type->params, &arg_count, &mismatch,
                         actual_args, sizeof(actual_args));
            finish_arg_text(actual_args, sizeof(actual_args));
            if (mismatch || arg_count != func->type->param_count) {
                report_function_argument_error(ctx, first->line, first->text,
                                               func->type, actual_args);
            }
            return func->type->return_type;
        }
        if (is_token(second, "LB")) {
            Type *base = analyze_exp(ctx, first);
            Type *index = analyze_exp(ctx, child_at(exp, 2));
            if (base->kind != TYPE_ARRAY) {
                const char *name = NULL;
                ASTNode *id_exp = child_at(first, 0);
                if (is_token(id_exp, "ID")) {
                    name = id_exp->text;
                }
                if (name) {
                    semantic_error_quoted_suffix(ctx, 10, second->line, name, "is not an array");
                } else {
                    semantic_error(ctx, 10, second->line, "Not an array");
                }
                return &type_error_value;
            }
            if (!is_int(index)) {
                semantic_error(ctx, 12, ast_line(child_at(exp, 2)), "Array index is not an integer");
            }
            return base->elem;
        }
    }

    return &type_error_value;
}

static void analyze_stmt(SemanticContext *ctx, ASTNode *stmt)
{
    ASTNode *first = child_at(stmt, 0);
    int count = child_count(stmt);

    if (first == NULL) {
        return;
    }
    if (is_node(first, "Exp")) {
        analyze_exp(ctx, first);
    } else if (is_node(first, "CompSt")) {
        analyze_def_list(ctx, find_child(first, "DefList"), 0, NULL);
        analyze_stmt_list(ctx, find_child(first, "StmtList"));
    } else if (is_token(first, "RETURN")) {
        Type *ret = analyze_exp(ctx, child_at(stmt, 1));
        if (!same_type(ret, ctx->current_return_type)) {
            semantic_error(ctx, 8, first->line, "Type mismatched for return");
        }
    } else if (is_token(first, "IF")) {
        Type *cond = analyze_exp(ctx, child_at(stmt, 2));
        if (!is_int(cond)) {
            semantic_error(ctx, 7, ast_line(child_at(stmt, 2)), "Type mismatched for operands");
        }
        analyze_stmt(ctx, child_at(stmt, 4));
        if (count == 7) {
            analyze_stmt(ctx, child_at(stmt, 6));
        }
    } else if (is_token(first, "WHILE")) {
        Type *cond = analyze_exp(ctx, child_at(stmt, 2));
        if (!is_int(cond)) {
            semantic_error(ctx, 7, ast_line(child_at(stmt, 2)), "Type mismatched for operands");
        }
        analyze_stmt(ctx, child_at(stmt, 4));
    }
}

static void analyze_stmt_list(SemanticContext *ctx, ASTNode *stmt_list)
{
    if (stmt_list == NULL) {
        return;
    }
    analyze_stmt(ctx, child_at(stmt_list, 0));
    analyze_stmt_list(ctx, child_at(stmt_list, 1));
}

static void analyze_comp_st(SemanticContext *ctx, ASTNode *comp_st)
{
    analyze_def_list(ctx, find_child(comp_st, "DefList"), 0, NULL);
    analyze_stmt_list(ctx, find_child(comp_st, "StmtList"));
}

static void analyze_ext_dec_list(SemanticContext *ctx, ASTNode *ext_dec_list, Type *base)
{
    ASTNode *var_dec;
    const char *name;
    Type *type;
    int line;

    if (ext_dec_list == NULL) {
        return;
    }
    var_dec = child_at(ext_dec_list, 0);
    name = var_dec_name(var_dec);
    line = var_dec_line(var_dec);
    type = apply_var_dec(base, var_dec);
    add_variable(ctx, name, type, line);
    if (child_count(ext_dec_list) == 3) {
        analyze_ext_dec_list(ctx, child_at(ext_dec_list, 2), base);
    }
}

static void analyze_ext_def(SemanticContext *ctx, ASTNode *ext_def)
{
    Type *base = analyze_specifier(ctx, child_at(ext_def, 0));
    ASTNode *ext_dec_list = find_child(ext_def, "ExtDecList");
    ASTNode *fun_dec = find_child(ext_def, "FunDec");

    if (ext_dec_list != NULL) {
        analyze_ext_dec_list(ctx, ext_dec_list, base);
    } else if (fun_dec != NULL) {
        Type *old_return = ctx->current_return_type;
        analyze_fun_dec(ctx, fun_dec, base);
        ctx->current_return_type = base;
        analyze_comp_st(ctx, find_child(ext_def, "CompSt"));
        ctx->current_return_type = old_return;
    }
}

static void analyze_ext_def_list(SemanticContext *ctx, ASTNode *ext_def_list)
{
    if (ext_def_list == NULL) {
        return;
    }
    analyze_ext_def(ctx, child_at(ext_def_list, 0));
    analyze_ext_def_list(ctx, child_at(ext_def_list, 1));
}

static Type *new_function_type(Type *return_type, Field *params, int param_count)
{
    Type *type = new_type(TYPE_FUNCTION);

    type->return_type = return_type;
    type->params = params;
    type->param_count = param_count;
    return type;
}

static void install_builtin_functions(SemanticContext *ctx)
{
    Type *read_type = new_function_type(&type_int_value, NULL, 0);
    Field *write_param = new_field("value", &type_int_value, 0);
    Type *write_type = new_function_type(&type_int_value, write_param, 1);

    add_symbol(&ctx->functions, new_symbol("read", read_type, 0));
    add_symbol(&ctx->functions, new_symbol("write", write_type, 0));
}

int semantic_analyze(ASTNode *root)
{
    SemanticContext ctx;

    memset(&ctx, 0, sizeof(ctx));
    install_builtin_functions(&ctx);
    if (root != NULL) {
        analyze_ext_def_list(&ctx, child_at(root, 0));
    }
    return ctx.error_count;
}
