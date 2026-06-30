# 实践1.2 基于 Bison 的语法分析

本目录在实践 1.1 的词法分析基础上，实现 C-- 的语法分析和语法树输出。

```text
exp1.2/
├── lexer.l
├── parser.y
├── ast.h
├── ast.c
├── Makefile
└── tests/
```

## 实现内容

`lexer.l` 不再直接打印 token，而是把识别出的词法单元封装为 `ASTNode`，返回给 Bison。`parser.y` 根据 C-- 文法归约生成语法树。`ast.c` 提供统一的节点创建、连接、先序打印和释放接口，为后续语义分析、符号表和中间代码生成保留结构化入口。

当前支持：

- 外部定义、全局变量、函数定义
- 基本类型 `int`、`float`
- 结构体定义和结构体类型引用
- 变量、数组、函数参数
- 复合语句、定义语句、表达式语句、`return`、`if/else`、`while`
- 赋值、逻辑、关系、算术、括号、函数调用、数组访问、结构体成员访问
- 八进制、十六进制、指数浮点数和注释处理
- 词法错误 A 与语法错误 B

对于没有词法或语法错误的输入，程序按先序遍历打印语法树。对于存在错误的输入，只打印错误信息，不打印语法树。

## 编译运行

进入本目录：

```bash
cd ~/compiler/exp1.2
```

编译：

```bash
make clean
make
```

运行测试：

```bash
make test
```

运行课程必做和选做样例：

```bash
make sample-test
```

运行自定义输入：

```bash
./cc your_source_file.cmm
```

## 错误处理

词法错误格式：

```text
Error type A at Line [行号]: [说明文字].
```

语法错误格式：

```text
Error type B at Line [行号]: [说明文字].
```

当前语法分析器包含针对常见错误的恢复规则，例如：

- 数组访问中使用逗号：`a[5,3]`
- 语句缺少分号：`i = 1 else`
- 定义缺少分号

当输入中出现词法错误时，程序会继续扫描剩余文件，尽量报告后续词法错误；同时抑制由坏 token 造成的连带语法噪声。

## 接口设计说明

本阶段没有沿用“词法阶段直接打印文本”的接口，而是定义了 `ASTNode` 作为统一结构：

```c
struct ASTNode {
    char *name;
    char *text;
    int line;
    int is_token;
    ASTNode *first_child;
    ASTNode *last_child;
    ASTNode *next_sibling;
};
```

该结构同时表示词法节点和语法节点：

- 词法节点保存 token 名称、词素文本和行号。
- 语法节点保存非终结符名称、行号和子节点链表。

这样做的目的是让后续实验可以直接在 AST 上实现：

- 符号表构建
- 类型检查
- 语义错误报告
- 中间代码生成

后续阶段不需要解析语法树的打印文本，也不需要了解 Flex/Bison 内部动作细节。

## 样例位置

课程文档中的必做样例和选做样例已经放在 `tests/` 下：

```text
required_1_lex_error.cmm
required_2_syntax_error.cmm
required_3_tree_inc.cmm
required_4_tree_struct.cmm
optional_1_base_number_ok.cmm
optional_2_base_number_error.cmm
optional_3_float_exp_ok.cmm
optional_4_float_exp_error.cmm
optional_5_comment_ok.cmm
optional_6_nested_comment_error.cmm
```

其中 `required_*` 对应必做样例，`optional_*` 对应八进制/十六进制、指数浮点数和注释的选做样例。

## 相似度控制

本实现没有直接照搬通用课程模板，而是采用自己的轻量级 AST 链式结构和 `ast_branch()` 归约辅助函数。Bison 规约动作集中调用 AST 接口，减少散落的节点拼接代码，也让后续扩展语义字段时更容易维护。
