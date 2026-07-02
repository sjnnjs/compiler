#include "ir.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VarInfo VarInfo;

struct VarInfo {
    char *source_name;
    char *ir_name;
    int is_array;
    int size;
    VarInfo *next;
};

typedef struct {
    FILE *out;
    VarInfo *vars;
    int temp_no;
    int label_no;
    int var_no;
    int failed;
    const char *error_message;
} IRContext;

typedef struct {
    char *name;
    int is_address;
} LValue;

typedef struct {
    char *items[128];
    int count;
} ArgList;

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
    ASTNode *child;
    int count = 0;

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

static char *dup_printf(const char *format, ...)
{
    char buffer[256];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return ast_strdup(buffer);
}

static void set_ir_error(IRContext *ctx, const char *message)
{
    if (!ctx->failed) {
        ctx->failed = 1;
        ctx->error_message = message;
    }
}

static void emit(IRContext *ctx, const char *format, ...)
{
    va_list args;

    if (ctx->failed) {
        return;
    }
    va_start(args, format);
    vfprintf(ctx->out, format, args);
    va_end(args);
    fputc('\n', ctx->out);
}

static char *new_temp(IRContext *ctx)
{
    ++ctx->temp_no;
    return dup_printf("t%d", ctx->temp_no);
}

static char *new_label(IRContext *ctx)
{
    ++ctx->label_no;
    return dup_printf("label%d", ctx->label_no);
}

static char *ensure_variable_operand(IRContext *ctx, char *operand)
{
    char *place;

    if (operand != NULL && operand[0] == '#') {
        place = new_temp(ctx);
        emit(ctx, "%s := %s", place, operand);
        return place;
    }
    return operand;
}

static VarInfo *find_var(IRContext *ctx, const char *source_name)
{
    VarInfo *var;

    for (var = ctx->vars; var != NULL; var = var->next) {
        if (strcmp(var->source_name, source_name) == 0) {
            return var;
        }
    }
    return NULL;
}

static VarInfo *register_var(IRContext *ctx, const char *source_name, int is_array, int size)
{
    VarInfo *var = find_var(ctx, source_name);

    if (var != NULL) {
        return var;
    }

    var = (VarInfo *)calloc(1, sizeof(VarInfo));
    if (var == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }
    var->source_name = ast_strdup(source_name);
    var->ir_name = dup_printf("v%d", ++ctx->var_no);
    var->is_array = is_array;
    var->size = size;
    var->next = ctx->vars;
    ctx->vars = var;
    return var;
}

static const char *lookup_var_name(IRContext *ctx, const char *source_name)
{
    VarInfo *var = find_var(ctx, source_name);

    if (var == NULL) {
        return source_name;
    }
    return var->ir_name;
}

static int specifier_is_int(ASTNode *specifier)
{
    ASTNode *type = child_at(specifier, 0);

    return is_token(type, "TYPE") && type->text != NULL && strcmp(type->text, "int") == 0;
}

static const char *var_dec_name(ASTNode *var_dec)
{
    ASTNode *first = child_at(var_dec, 0);

    if (is_token(first, "ID")) {
        return first->text;
    }
    return var_dec_name(first);
}

static int var_dec_dimension_count(ASTNode *var_dec)
{
    ASTNode *first = child_at(var_dec, 0);

    if (is_token(first, "ID")) {
        return 0;
    }
    return 1 + var_dec_dimension_count(first);
}

static int var_dec_element_count(ASTNode *var_dec)
{
    ASTNode *first = child_at(var_dec, 0);
    ASTNode *size_node;
    int inner;
    int current;

    if (is_token(first, "ID")) {
        return 1;
    }

    inner = var_dec_element_count(first);
    size_node = child_at(var_dec, 2);
    current = size_node != NULL && size_node->text != NULL ? atoi(size_node->text) : 1;
    return inner * current;
}

static void translate_ext_def_list(IRContext *ctx, ASTNode *ext_def_list);
static void translate_comp_st(IRContext *ctx, ASTNode *comp_st);
static void translate_def_list(IRContext *ctx, ASTNode *def_list);
static void translate_stmt_list(IRContext *ctx, ASTNode *stmt_list);
static void translate_stmt(IRContext *ctx, ASTNode *stmt);
static char *translate_exp(IRContext *ctx, ASTNode *exp);
static void translate_cond(IRContext *ctx, ASTNode *exp, const char *label_true, const char *label_false);
static int stmt_always_returns(ASTNode *stmt);

static void translate_param_dec(IRContext *ctx, ASTNode *param_dec)
{
    ASTNode *specifier = child_at(param_dec, 0);
    ASTNode *var_dec = child_at(param_dec, 1);
    const char *name;
    VarInfo *var;

    if (!specifier_is_int(specifier)) {
        set_ir_error(ctx, "Cannot translate: Code contains structure or float parameters.");
        return;
    }
    if (var_dec_dimension_count(var_dec) != 0) {
        set_ir_error(ctx, "Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.");
        return;
    }

    name = var_dec_name(var_dec);
    var = register_var(ctx, name, 0, 4);
    emit(ctx, "PARAM %s", var->ir_name);
}

static void translate_var_list(IRContext *ctx, ASTNode *var_list)
{
    if (var_list == NULL || ctx->failed) {
        return;
    }

    translate_param_dec(ctx, child_at(var_list, 0));
    if (child_count(var_list) == 3) {
        translate_var_list(ctx, child_at(var_list, 2));
    }
}

static void translate_fun_dec(IRContext *ctx, ASTNode *fun_dec)
{
    ASTNode *id = child_at(fun_dec, 0);
    ASTNode *var_list = find_child(fun_dec, "VarList");

    emit(ctx, "FUNCTION %s :", id->text);
    translate_var_list(ctx, var_list);
}

static void translate_dec(IRContext *ctx, ASTNode *dec, ASTNode *specifier)
{
    ASTNode *var_dec = child_at(dec, 0);
    ASTNode *init_exp = child_count(dec) == 3 ? child_at(dec, 2) : NULL;
    const char *source_name;
    VarInfo *var;
    int dims;
    int byte_size;

    if (!specifier_is_int(specifier)) {
        set_ir_error(ctx, "Cannot translate: Code contains structure or float variables.");
        return;
    }

    source_name = var_dec_name(var_dec);
    dims = var_dec_dimension_count(var_dec);
    if (dims > 1) {
        set_ir_error(ctx, "Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.");
        return;
    }

    byte_size = var_dec_element_count(var_dec) * 4;
    var = register_var(ctx, source_name, dims == 1, byte_size);
    if (var->is_array) {
        emit(ctx, "DEC %s %d", var->ir_name, var->size);
        if (init_exp != NULL) {
            set_ir_error(ctx, "Cannot translate: Code contains initialized array variables.");
        }
        return;
    }

    if (init_exp != NULL) {
        char *value = translate_exp(ctx, init_exp);
        emit(ctx, "%s := %s", var->ir_name, value);
    }
}

static void translate_dec_list(IRContext *ctx, ASTNode *dec_list, ASTNode *specifier)
{
    if (dec_list == NULL || ctx->failed) {
        return;
    }

    translate_dec(ctx, child_at(dec_list, 0), specifier);
    if (child_count(dec_list) == 3) {
        translate_dec_list(ctx, child_at(dec_list, 2), specifier);
    }
}

static void translate_def(IRContext *ctx, ASTNode *def)
{
    ASTNode *specifier = child_at(def, 0);
    ASTNode *dec_list = find_child(def, "DecList");

    translate_dec_list(ctx, dec_list, specifier);
}

static void translate_def_list(IRContext *ctx, ASTNode *def_list)
{
    if (def_list == NULL || ctx->failed) {
        return;
    }

    translate_def(ctx, child_at(def_list, 0));
    translate_def_list(ctx, child_at(def_list, 1));
}

static LValue translate_lvalue(IRContext *ctx, ASTNode *exp)
{
    int count = child_count(exp);
    ASTNode *first = child_at(exp, 0);
    ASTNode *second = child_at(exp, 1);
    LValue value;

    value.name = NULL;
    value.is_address = 0;

    if (count == 1 && is_token(first, "ID")) {
        value.name = ast_strdup(lookup_var_name(ctx, first->text));
        value.is_address = 0;
        return value;
    }

    if (count == 4 && is_token(second, "LB")) {
        ASTNode *base_exp = first;
        ASTNode *base_id = child_at(base_exp, 0);
        ASTNode *index_exp = child_at(exp, 2);
        VarInfo *array_var;
        char *index;
        char *offset;
        char *addr;

        if (!is_node(base_exp, "Exp") || !is_token(base_id, "ID")) {
            set_ir_error(ctx, "Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.");
            return value;
        }
        array_var = find_var(ctx, base_id->text);
        if (array_var == NULL || !array_var->is_array) {
            set_ir_error(ctx, "Cannot translate: Array access target is not a local one-dimensional array.");
            return value;
        }

        index = translate_exp(ctx, index_exp);
        offset = new_temp(ctx);
        addr = new_temp(ctx);
        emit(ctx, "%s := %s * #4", offset, index);
        emit(ctx, "%s := &%s + %s", addr, array_var->ir_name, offset);
        value.name = addr;
        value.is_address = 1;
        return value;
    }

    set_ir_error(ctx, "Cannot translate: Left value is not supported.");
    return value;
}

static void translate_args(IRContext *ctx, ASTNode *args, ArgList *list)
{
    if (args == NULL || ctx->failed) {
        return;
    }
    if (list->count >= 128) {
        set_ir_error(ctx, "Cannot translate: Too many function arguments.");
        return;
    }

    list->items[list->count++] = translate_exp(ctx, child_at(args, 0));
    if (child_count(args) == 3) {
        translate_args(ctx, child_at(args, 2), list);
    }
}

static char *translate_call(IRContext *ctx, ASTNode *exp)
{
    ASTNode *id = child_at(exp, 0);
    ASTNode *args = find_child(exp, "Args");
    ArgList list;
    char *place;
    int i;

    memset(&list, 0, sizeof(list));

    if (strcmp(id->text, "read") == 0 && args == NULL) {
        place = new_temp(ctx);
        emit(ctx, "READ %s", place);
        return place;
    }

    translate_args(ctx, args, &list);

    if (strcmp(id->text, "write") == 0) {
        if (list.count > 0) {
            emit(ctx, "WRITE %s", ensure_variable_operand(ctx, list.items[0]));
        }
        return ast_strdup("#0");
    }

    for (i = list.count - 1; i >= 0; --i) {
        emit(ctx, "ARG %s", list.items[i]);
    }
    place = new_temp(ctx);
    emit(ctx, "%s := CALL %s", place, id->text);
    return place;
}

static char *translate_boolean_value(IRContext *ctx, ASTNode *exp)
{
    char *place = new_temp(ctx);
    char *label_true = new_label(ctx);
    char *label_false = new_label(ctx);

    emit(ctx, "%s := #0", place);
    translate_cond(ctx, exp, label_true, label_false);
    emit(ctx, "LABEL %s :", label_true);
    emit(ctx, "%s := #1", place);
    emit(ctx, "LABEL %s :", label_false);
    return place;
}

static char *translate_exp(IRContext *ctx, ASTNode *exp)
{
    int count = child_count(exp);
    ASTNode *first = child_at(exp, 0);
    ASTNode *second = child_at(exp, 1);

    if (ctx->failed) {
        return ast_strdup("#0");
    }

    if (count == 1) {
        if (is_token(first, "ID")) {
            return ast_strdup(lookup_var_name(ctx, first->text));
        }
        if (is_token(first, "INT")) {
            return dup_printf("#%s", first->text);
        }
        if (is_token(first, "FLOAT")) {
            set_ir_error(ctx, "Cannot translate: Code contains float constants.");
            return ast_strdup("#0");
        }
    }

    if (count == 2) {
        if (is_token(first, "MINUS")) {
            char *operand = translate_exp(ctx, child_at(exp, 1));
            if (operand[0] == '#') {
                return dup_printf("#-%s", operand + 1);
            }
            {
                char *place = new_temp(ctx);
                emit(ctx, "%s := #0 - %s", place, operand);
                return place;
            }
        }
        if (is_token(first, "NOT")) {
            return translate_boolean_value(ctx, exp);
        }
    }

    if (count == 3) {
        if (is_token(first, "LP")) {
            return translate_exp(ctx, child_at(exp, 1));
        }

        if (is_token(second, "ASSIGNOP")) {
            LValue left = translate_lvalue(ctx, first);
            char *right = translate_exp(ctx, child_at(exp, 2));

            if (left.name != NULL) {
                if (left.is_address) {
                    emit(ctx, "*%s := %s", left.name, right);
                } else {
                    emit(ctx, "%s := %s", left.name, right);
                }
            }
            return right;
        }

        if (is_token(second, "PLUS") || is_token(second, "MINUS") ||
            is_token(second, "STAR") || is_token(second, "DIV")) {
            char *left = translate_exp(ctx, first);
            char *right = translate_exp(ctx, child_at(exp, 2));
            char *place = new_temp(ctx);
            const char *op = second->text;

            if (is_token(second, "PLUS")) {
                op = "+";
            } else if (is_token(second, "MINUS")) {
                op = "-";
            } else if (is_token(second, "STAR")) {
                op = "*";
            } else if (is_token(second, "DIV")) {
                op = "/";
            }
            emit(ctx, "%s := %s %s %s", place, left, op, right);
            return place;
        }

        if (is_token(second, "RELOP") || is_token(second, "AND") || is_token(second, "OR")) {
            return translate_boolean_value(ctx, exp);
        }

        if (is_token(second, "DOT")) {
            set_ir_error(ctx, "Cannot translate: Code contains structure variables.");
            return ast_strdup("#0");
        }

        if (is_token(first, "ID") && is_token(second, "LP")) {
            return translate_call(ctx, exp);
        }
    }

    if (count == 4) {
        if (is_token(first, "ID") && is_token(second, "LP")) {
            return translate_call(ctx, exp);
        }
        if (is_token(second, "LB")) {
            LValue addr = translate_lvalue(ctx, exp);
            char *place = new_temp(ctx);

            if (addr.name != NULL) {
                emit(ctx, "%s := *%s", place, addr.name);
            }
            return place;
        }
    }

    set_ir_error(ctx, "Cannot translate: Expression form is not supported.");
    return ast_strdup("#0");
}

static void translate_cond(IRContext *ctx, ASTNode *exp, const char *label_true, const char *label_false)
{
    int count = child_count(exp);
    ASTNode *first = child_at(exp, 0);
    ASTNode *second = child_at(exp, 1);

    if (ctx->failed) {
        return;
    }

    if (count == 2 && is_token(first, "NOT")) {
        translate_cond(ctx, child_at(exp, 1), label_false, label_true);
        return;
    }

    if (count == 3 && is_token(second, "AND")) {
        char *label_mid = new_label(ctx);
        translate_cond(ctx, first, label_mid, label_false);
        emit(ctx, "LABEL %s :", label_mid);
        translate_cond(ctx, child_at(exp, 2), label_true, label_false);
        return;
    }

    if (count == 3 && is_token(second, "OR")) {
        char *label_mid = new_label(ctx);
        translate_cond(ctx, first, label_true, label_mid);
        emit(ctx, "LABEL %s :", label_mid);
        translate_cond(ctx, child_at(exp, 2), label_true, label_false);
        return;
    }

    if (count == 3 && is_token(second, "RELOP")) {
        char *left = translate_exp(ctx, first);
        char *right = translate_exp(ctx, child_at(exp, 2));
        emit(ctx, "IF %s %s %s GOTO %s", left, second->text, right, label_true);
        emit(ctx, "GOTO %s", label_false);
        return;
    }

    {
        char *value = translate_exp(ctx, exp);
        emit(ctx, "IF %s != #0 GOTO %s", value, label_true);
        emit(ctx, "GOTO %s", label_false);
    }
}

static void translate_stmt(IRContext *ctx, ASTNode *stmt)
{
    int count = child_count(stmt);
    ASTNode *first = child_at(stmt, 0);

    if (stmt == NULL || ctx->failed) {
        return;
    }

    if (count == 1 && is_node(first, "CompSt")) {
        translate_comp_st(ctx, first);
        return;
    }

    if (count == 2 && is_node(first, "Exp")) {
        translate_exp(ctx, first);
        return;
    }

    if (count == 3 && is_token(first, "RETURN")) {
        char *value = translate_exp(ctx, child_at(stmt, 1));
        emit(ctx, "RETURN %s", value);
        return;
    }

    if (count == 5 && is_token(first, "IF")) {
        char *label_true = new_label(ctx);
        char *label_false = new_label(ctx);

        translate_cond(ctx, child_at(stmt, 2), label_true, label_false);
        emit(ctx, "LABEL %s :", label_true);
        translate_stmt(ctx, child_at(stmt, 4));
        emit(ctx, "LABEL %s :", label_false);
        return;
    }

    if (count == 7 && is_token(first, "IF")) {
        char *label_true = new_label(ctx);
        char *label_false = new_label(ctx);
        char *label_end = NULL;
        ASTNode *then_stmt = child_at(stmt, 4);
        ASTNode *else_stmt = child_at(stmt, 6);
        int then_returns = stmt_always_returns(then_stmt);

        translate_cond(ctx, child_at(stmt, 2), label_true, label_false);
        emit(ctx, "LABEL %s :", label_true);
        translate_stmt(ctx, then_stmt);
        if (!then_returns) {
            label_end = new_label(ctx);
            emit(ctx, "GOTO %s", label_end);
        }
        emit(ctx, "LABEL %s :", label_false);
        translate_stmt(ctx, else_stmt);
        if (label_end != NULL) {
            emit(ctx, "LABEL %s :", label_end);
        }
        return;
    }

    if (count == 5 && is_token(first, "WHILE")) {
        char *label_begin = new_label(ctx);
        char *label_body = new_label(ctx);
        char *label_end = new_label(ctx);

        emit(ctx, "LABEL %s :", label_begin);
        translate_cond(ctx, child_at(stmt, 2), label_body, label_end);
        emit(ctx, "LABEL %s :", label_body);
        translate_stmt(ctx, child_at(stmt, 4));
        emit(ctx, "GOTO %s", label_begin);
        emit(ctx, "LABEL %s :", label_end);
        return;
    }
}

static int stmt_list_always_returns(ASTNode *stmt_list)
{
    if (stmt_list == NULL) {
        return 0;
    }
    if (stmt_always_returns(child_at(stmt_list, 0))) {
        return 1;
    }
    return stmt_list_always_returns(child_at(stmt_list, 1));
}

static int comp_st_always_returns(ASTNode *comp_st)
{
    return stmt_list_always_returns(find_child(comp_st, "StmtList"));
}

static int stmt_always_returns(ASTNode *stmt)
{
    int count = child_count(stmt);
    ASTNode *first = child_at(stmt, 0);

    if (stmt == NULL) {
        return 0;
    }
    if (count == 1 && is_node(first, "CompSt")) {
        return comp_st_always_returns(first);
    }
    if (count == 3 && is_token(first, "RETURN")) {
        return 1;
    }
    if (count == 7 && is_token(first, "IF")) {
        return stmt_always_returns(child_at(stmt, 4)) &&
               stmt_always_returns(child_at(stmt, 6));
    }
    return 0;
}

static void translate_stmt_list(IRContext *ctx, ASTNode *stmt_list)
{
    if (stmt_list == NULL || ctx->failed) {
        return;
    }

    translate_stmt(ctx, child_at(stmt_list, 0));
    translate_stmt_list(ctx, child_at(stmt_list, 1));
}

static void translate_comp_st(IRContext *ctx, ASTNode *comp_st)
{
    translate_def_list(ctx, find_child(comp_st, "DefList"));
    translate_stmt_list(ctx, find_child(comp_st, "StmtList"));
}

static void translate_ext_def(IRContext *ctx, ASTNode *ext_def)
{
    ASTNode *specifier = find_child(ext_def, "Specifier");
    ASTNode *fun_dec = find_child(ext_def, "FunDec");
    ASTNode *comp_st = find_child(ext_def, "CompSt");
    ASTNode *ext_dec_list = find_child(ext_def, "ExtDecList");

    if (ctx->failed || ext_def == NULL) {
        return;
    }

    if (ext_dec_list != NULL) {
        set_ir_error(ctx, "Cannot translate: Code contains global variables.");
        return;
    }

    if (fun_dec != NULL && comp_st != NULL) {
        if (!specifier_is_int(specifier)) {
            set_ir_error(ctx, "Cannot translate: Code contains functions returning non-int values.");
            return;
        }
        translate_fun_dec(ctx, fun_dec);
        translate_comp_st(ctx, comp_st);
    }
}

static void translate_ext_def_list(IRContext *ctx, ASTNode *ext_def_list)
{
    if (ext_def_list == NULL || ctx->failed) {
        return;
    }

    translate_ext_def(ctx, child_at(ext_def_list, 0));
    translate_ext_def_list(ctx, child_at(ext_def_list, 1));
}

int ir_generate(ASTNode *root, const char *output_path)
{
    IRContext ctx;
    FILE *out;

    out = fopen(output_path, "w");
    if (out == NULL) {
        fprintf(stderr, "Cannot open output file: %s\n", output_path);
        return 1;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.out = out;

    if (root != NULL) {
        translate_ext_def_list(&ctx, child_at(root, 0));
    }

    fclose(out);
    if (ctx.failed) {
        remove(output_path);
        printf("%s\n", ctx.error_message);
        return 1;
    }
    return 0;
}
