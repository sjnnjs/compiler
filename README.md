# 编译器使用与接口设计说明

本文档记录当前实验代码的使用方式，以及后续从词法分析扩展到语法分析、语义分析、中间代码生成和目标代码生成时应保持的接口约定。后续每完成一个实验阶段，都应同步更新本文档。

## 1. 当前项目状态

当前已完成 `exp1.1` 词法分析实验和 `exp1.2` 语法分析实验。

`exp1.1` 核心文件为：

```text
exp1.1/
├── lexer.l              # Flex 词法规则源文件
├── Makefile             # 构建脚本
├── tests/
│   ├── lex_ok.cmm       # 合法词法测试
│   └── lex_error.cmm    # 词法错误测试
├── lex.yy.c             # Flex 生成文件
└── lexer                # 编译生成的词法分析程序
```

当前程序只完成词法分析。它会读取一个 C-- 源文件，逐个识别 token 并打印到标准输出；遇到词法错误时输出 `Error type A`。当前还没有接入 Bison、语法树、符号表、中间代码或目标代码模块。

`exp1.2` 核心文件为：

```text
exp1.2/
├── lexer.l              # 给 Bison 返回 token 的 Flex 词法规则
├── parser.y             # Bison 语法规则和错误恢复
├── ast.h
├── ast.c                # AST 节点创建、连接、打印和释放
├── Makefile
└── tests/
```

`exp1.2` 已经接入 Bison。对于没有词法或语法错误的输入，程序会打印语法树；对于包含词法或语法错误的输入，只打印错误信息，不打印语法树。

## 2. 环境要求

课程要求环境：

```text
Ubuntu 20.04
GCC/G++ 7.5.0
Flex 2.6.4
Bison 3.5.1
QtSpim 9.1.9
```

当前 WSL 环境已验证：

```bash
gcc --version
g++ --version
flex --version
bison --version
make --version
```

辅助工具启动方式：

```bash
cd ~/compiler
./tools/run_qtspim_9.1.9.sh qtspim_hello.s
./tools/run_irsim.sh
```

`irsim` 依赖 PyQt5。如果提示缺少 PyQt5，执行：

```bash
sudo apt install python3-pyqt5
```

## 3. 当前代码如何使用

### 3.1 词法分析实验 1.1

进入词法实验目录：

```bash
cd ~/compiler/exp1.1
```

重新构建：

```bash
make clean
make
```

运行自带测试：

```bash
make test
```

运行自定义 C-- 源文件：

```bash
./lexer your_source_file.cmm
```

### 3.2 语法分析实验 1.2

进入语法实验目录：

```bash
cd ~/compiler/exp1.2
```

重新构建：

```bash
make clean
make
```

运行自带测试：

```bash
make test
```

运行自定义 C-- 源文件：

```bash
./cc your_source_file.cmm
```

语法分析程序的输出约定：

```text
无错误：按先序遍历打印语法树
有错误：只打印 Error type A 或 Error type B，不打印语法树
```

当前输出示例：

```text
TYPE: int
ID: main
LP
RP
LC
RETURN
INT: 0
SEMI
RC
```

词法错误输出示例：

```text
Error type A at Line 3: Illegal octal number "09".
```

程序退出码约定：

```text
0  词法分析完成且没有词法错误
1  命令行参数错误或输入文件无法打开
2  词法分析完成但发现词法错误
```

## 4. 当前词法模块的行为

当前 `lexer.l` 可以识别：

- 关键字：`int`、`float`、`struct`、`return`、`if`、`else`、`while`
- 标识符：`ID`
- 整数：十进制、八进制、十六进制，统一转换为十进制值输出
- 浮点数：普通小数和指数形式
- 运算符和界符：`ASSIGNOP`、`RELOP`、`PLUS`、`MINUS`、`STAR`、`DIV`、`AND`、`OR`、`NOT`、`DOT`、`SEMI`、`COMMA`、`LP`、`RP`、`LB`、`RB`、`LC`、`RC`
- 注释：忽略 `//` 行注释和 `/* ... */` 块注释

当前已处理的词法错误：

- 未定义字符，例如 `~`
- 非法八进制数，例如 `09`
- 非法十六进制数，例如 `0x3G`
- 非法浮点数，例如 `1.05e`
- 未闭合块注释

## 5. 后续整体架构设计

编译器后续应按流水线组织：

```text
源程序
  ↓
词法分析 Lexer
  ↓ Token 流
语法分析 Parser
  ↓ AST
语义分析 Semantic Analyzer
  ↓ 带类型和符号信息的 AST
中间代码生成 IR Generator
  ↓ IR
目标代码生成 Code Generator
  ↓ MIPS 汇编或课程虚拟机代码
QtSpim / irsim / interpret 执行
```

设计重点是：相邻阶段之间通过明确的数据结构交互，而不是依赖前一阶段的打印文本。命令行打印可以保留为调试模式，但不应作为后续阶段的正式输入接口。

## 6. 阶段接口约定

### 6.1 词法分析接口

`exp1.1` 阶段是直接打印 token。`exp1.2` 已经将输出行为改造成结构化 token，并把 token 节点交给 Bison：

```c
typedef enum TokenKind {
    TOKEN_ID,
    TOKEN_TYPE,
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRUCT,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_RELOP,
    TOKEN_ASSIGNOP,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_DIV,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_DOT,
    TOKEN_SEMI,
    TOKEN_COMMA,
    TOKEN_LP,
    TOKEN_RP,
    TOKEN_LB,
    TOKEN_RB,
    TOKEN_LC,
    TOKEN_RC
} TokenKind;

typedef struct Token {
    TokenKind kind;
    int line;
    const char *lexeme;
    union {
        long int_value;
        double float_value;
    } value;
} Token;
```

当前 `exp1.2/lexer.l` 中，`yylex()` 返回 token 类型，并通过 `yylval.node` 传递 `ASTNode *`。行号保存在节点的 `line` 字段中。`exp1.1` 中的 `print_token()`、`print_text_token()` 等函数只保留在词法实验中，正式语法分析流程不依赖打印文本。

### 6.2 语法分析接口

语法分析阶段输入 token 流，输出抽象语法树 AST。当前 `exp1.2` 已经落地该接口。

当前 AST 节点定义为：

```c
typedef struct ASTNode {
    char *name;
    char *text;
    int line;
    int is_token;
    struct ASTNode *first_child;
    struct ASTNode *last_child;
    struct ASTNode *next_sibling;
} ASTNode;
```

该链式结构刻意不把子节点固定成数组，后续做语义分析时可以顺序遍历，也可以局部插入或重组节点。

当前语法分析阶段负责：

- 根据文法构建 AST
- 报告语法错误 `Error type B`
- 在出现语法错误时尽量恢复，继续发现后续错误

语法分析阶段不应直接做完整类型检查，也不应直接生成最终代码。它的输出应是后续语义分析可以遍历的 AST。

### 6.3 语义分析接口

语义分析阶段输入 AST，输出语义检查结果，并维护符号表、类型表等信息。

建议核心结构：

```c
typedef struct Type Type;
typedef struct Symbol Symbol;
typedef struct SymbolTable SymbolTable;
```

语义分析阶段负责：

- 变量、函数、结构体定义检查
- 作用域管理
- 类型匹配检查
- 左值检查
- 函数参数检查
- 数组、结构体成员访问检查

语义错误应按课程要求输出对应错误类型。只要语义分析存在错误，后续中间代码和目标代码生成通常应停止，避免基于错误 AST 继续生成误导性结果。

### 6.4 中间代码接口

中间代码生成阶段输入“通过语义检查的 AST”，输出 IR。

建议使用三地址码或课程要求的中间表示：

```c
typedef enum IROp {
    IR_LABEL,
    IR_FUNCTION,
    IR_ASSIGN,
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_GOTO,
    IR_IF_GOTO,
    IR_RETURN,
    IR_DEC,
    IR_ARG,
    IR_CALL,
    IR_PARAM,
    IR_READ,
    IR_WRITE
} IROp;

typedef struct IRInstruction {
    IROp op;
    char *result;
    char *arg1;
    char *arg2;
    char *relop;
} IRInstruction;
```

IR 生成阶段不应依赖源代码文本格式，而应只依赖 AST、类型信息和符号表查询接口。

### 6.5 目标代码生成接口

目标代码生成阶段输入 IR，输出目标代码。根据课程要求，可能输出：

- MIPS32 汇编：用于 QtSpim
- `.pld`、`.lst`、`.lab`：用于课程提供的 `irsim` 或 `interpret`

建议接口形式：

```text
IRInstruction list
  ↓
CodeGenerator
  ↓
target.s 或 target.pld/target.lst/target.lab
```

目标代码生成阶段应只关心 IR，不应回头访问原始 token 或直接遍历未经约束的 AST。这样可以让前端和后端相对独立。

## 7. 黑盒状态下的接口设计原则

后续每个阶段都应把前一阶段视为黑盒，只依赖公开接口：

- 语法分析只依赖 token 类型、语义值和行号，不依赖词法分析内部正则表达式。
- 语义分析只依赖 AST 节点和符号表接口，不依赖 Bison 规约动作的内部实现。
- IR 生成只依赖通过语义检查后的 AST、类型信息和符号查询，不依赖源代码文本。
- 目标代码生成只依赖 IR，不依赖语法树细节。

这样做的好处是：

- 方便单独测试每个阶段
- 方便替换某一阶段实现
- 错误定位更清晰
- 后续实验扩展不会把所有代码缠在一起

## 8. 后续更新记录

每完成一个实验阶段，应在这里补充：

```text
日期：
阶段：
新增文件：
构建命令：
运行命令：
输入接口：
输出接口：
错误处理：
测试结果：
遗留问题：
```

当前记录：

```text
日期：2026-06-30
阶段：实践 1.1 词法分析
新增文件：exp1.1/lexer.l、exp1.1/Makefile、exp1.1/tests/*
构建命令：cd ~/compiler/exp1.1 && make clean && make
运行命令：./lexer tests/lex_ok.cmm
输入接口：C-- 源文件路径
输出接口：标准输出 token 文本；错误输出为 Error type A
错误处理：非法字符、非法数字、未闭合块注释
测试结果：make test 通过
遗留问题：当前 token 仅打印为文本，尚未封装为供 Bison 使用的结构化接口
```

```text
日期：2026-06-30
阶段：实践 1.2 语法分析
新增文件：exp1.2/lexer.l、exp1.2/parser.y、exp1.2/ast.h、exp1.2/ast.c、exp1.2/Makefile、exp1.2/tests/*
构建命令：cd ~/compiler/exp1.2 && make clean && make
运行命令：./cc tests/syntax_ok.cmm
输入接口：C-- 源文件路径
输出接口：无错误时输出先序语法树；有错误时输出 Error type A/B
错误处理：词法错误 A；语法错误 B；数组访问缺右中括号、语句/定义缺分号等恢复规则
测试结果：make test 通过
遗留问题：语法树已经可供语义分析遍历，但符号表、类型系统和语义错误检查尚未实现
```
