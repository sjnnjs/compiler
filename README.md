# 编译器使用与接口设计说明

本文档记录当前实验代码的使用方式，以及后续从词法分析扩展到语法分析、语义分析、中间代码生成和目标代码生成时应保持的接口约定。后续每完成一个实验阶段，都应同步更新本文档。

## 1. 当前项目状态

当前已完成 `exp1.1` 词法分析实验、`exp1.2` 语法分析实验、`exp2.0` 语义分析实验和 `exp3.0` 中间代码生成实验。

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

`exp2.0` 核心文件为：

```text
exp2.0/
├── lexer.l              # 复用结构化 token 前端
├── parser.y             # 构建 AST，成功后调用语义分析
├── ast.h / ast.c
├── semantic.h / semantic.c
├── Makefile
└── test/
```

`exp2.0` 在语法分析成功后进行语义分析。无语义错误时不输出任何内容；有语义错误时输出 `Error type 1` 至 `Error type 17`。

`exp3.0` 核心文件为：

```text
exp3.0/
├── lexer.l              # 复用结构化 token 前端
├── parser.y             # 构建 AST，调度语义检查和 IR 生成
├── ast.h / ast.c
├── semantic.h / semantic.c
├── ir.h / ir.c          # 中间代码生成器
├── Makefile
└── test/
```

`exp3.0` 在语义分析通过后生成课程表 4.6 格式的中间代码。程序输入为 C-- 源文件路径和 IR 输出文件路径。

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

### 3.3 语义分析实验 2.0

进入语义分析目录：

```bash
cd ~/compiler/exp2.0
```

重新构建：

```bash
make clean
make
```

运行全部样例：

```bash
make test
```

运行自定义 C-- 源文件：

```bash
./cc your_source_file.cmm
```

语义分析程序的输出约定：

```text
无语义错误：不输出任何内容
有语义错误：逐行输出 Error type N at Line X: ...
```

### 3.4 中间代码生成实验 3.0

进入中间代码生成目录：

```bash
cd ~/compiler/exp3.0
```

重新构建：

```bash
make clean
make
```

运行全部样例：

```bash
make test
```

运行自定义 C-- 源文件并输出 IR：

```bash
./cc your_source_file.cmm output.ir
```

中间代码生成程序的输出约定：

```text
无错误：将中间代码写入指定输出文件
有词法/语法/语义错误：输出对应错误，不生成 IR
遇到必做范围外结构：输出 Cannot translate: ...
```

词法分析输出示例：

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
0  当前阶段成功完成
1  命令行参数错误，或输入/输出文件无法打开
2  发现词法、语法、语义错误，或遇到当前实验范围外的不可翻译结构
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

语义分析阶段输入 AST，输出语义检查结果，并维护符号表、类型表等信息。当前 `exp2.0` 已经落地该接口。

当前核心结构按职责拆分为：

```c
typedef struct Type Type;
typedef struct Symbol Symbol;
typedef struct Field Field;
```

其中：

- `Type` 表示基本类型、数组类型、结构体类型、函数类型和错误类型。
- `Symbol` 用于变量表、函数表和结构体表。
- `Field` 用于结构体字段链表和函数形参链表。

当前语义分析阶段负责：

- 变量、函数、结构体定义检查
- 全局作用域下的变量和形参重名检查
- 类型匹配检查
- 左值检查
- 函数参数检查
- 数组、结构体成员访问检查

语义错误按课程要求输出对应错误类型。只要语义分析存在错误，后续中间代码和目标代码生成应停止，避免基于错误 AST 继续生成误导性结果。

`exp3.0` 在语义分析初始化时额外预置 `read()` 和 `write(int)` 两个函数，供中间代码阶段直接翻译为 `READ` 和 `WRITE`。

### 6.4 中间代码接口

中间代码生成阶段输入“通过语义检查的 AST”，输出 IR。当前 `exp3.0` 已经落地该接口。

当前对外入口为：

```c
int ir_generate(ASTNode *root, const char *output_path);
```

`ir_generate()` 直接遍历 AST 并将 IR 写入文件。当前实现内部维护变量映射表，把源程序变量映射为 `v1`、`v2` 等 IR 名称；表达式中间结果使用 `t1`、`t2`；控制流标号使用 `label1`、`label2`。这样可以避免源程序变量名和临时变量名冲突。

当前 IR 生成阶段负责：

- 生成 `FUNCTION`、`PARAM`、`RETURN`
- 生成赋值和四则运算
- 生成 `LABEL`、`GOTO`、`IF x relop y GOTO label`
- 翻译 `if`、`if-else`、`while` 和短路逻辑
- 翻译 `ARG`、`CALL`
- 将预定义 `read()`、`write(int)` 翻译为 `READ`、`WRITE`
- 为一维局部数组生成 `DEC`，并用 `&`、`*` 和偏移完成元素访问

IR 生成阶段不依赖源代码文本格式，也不依赖前序阶段的打印结果。只要语义分析发现错误，IR 生成就不会继续执行。

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

```text
日期：2026-07-01
阶段：实践 2.0 语义分析
新增文件：exp2.0/semantic.h、exp2.0/semantic.c、exp2.0/README.md、exp2.0/report.md、exp2.0/test/*、注意事项.md
构建命令：cd ~/compiler/exp2.0 && make clean && make
运行命令：./cc test/01_undefined_variable.cmm
输入接口：语法正确的 C-- 源文件路径
输出接口：无语义错误时不输出；有语义错误时输出 Error type 1-17
错误处理：未定义、重复定义、类型不匹配、非法操作符、函数实参、数组和结构体访问错误等 17 类必做语义错误
测试结果：make test 覆盖 00_ok 和 1-17 类错误样例
遗留问题：暂未实现函数声明、嵌套作用域和结构等价三个选做要求
```

```text
日期：2026-07-02
阶段：实践 3.0 中间代码生成
新增文件：exp3.0/ir.h、exp3.0/ir.c、exp3.0/README.md、exp3.0/report.md、exp3.0/test/*
构建命令：cd ~/compiler/exp3.0 && make clean && make
运行命令：./cc test/01_sign.cmm test/out/01_sign.ir
输入接口：C-- 源文件路径和 IR 输出文件路径
输出接口：课程表 4.6 格式的中间代码文件
错误处理：词法/语法/语义错误时停止；结构体、数组参数、高维数组等选做范围外结构输出 Cannot translate
测试结果：make test 覆盖 read/write、递归调用、一维数组、while、多参数调用和逻辑表达式
遗留问题：暂未实现结构体变量、结构体参数、数组参数和高维数组等选做要求
```
