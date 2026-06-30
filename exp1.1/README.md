# 实践1.1 学习 Flex：用 Flex 实现词法分析

本目录保存实践 1.1 的全部文件：

```text
exp1.1/
├── lexer.l
├── Makefile
├── tests/
│   ├── lex_ok.cmm
│   └── lex_error.cmm
├── lex.yy.c
└── lexer
```

## 实现内容

`lexer.l` 使用 Flex 描述 C-- 语言的词法规则，可以识别：

- 关键字：`int`、`float`、`struct`、`return`、`if`、`else`、`while`
- 标识符：`ID`
- 类型：`TYPE: int`、`TYPE: float`
- 整数：十进制、八进制、十六进制，统一按十进制输出
- 浮点数：普通小数和指数形式浮点数
- 运算符和界符：`ASSIGNOP`、`RELOP`、`PLUS`、`MINUS`、`STAR`、`DIV`、`AND`、`OR`、`NOT`、`DOT`、`SEMI`、`COMMA`、括号、方括号和花括号
- 注释：过滤 `//` 行注释和 `/* ... */` 块注释

已实现的词法错误 A：

- 未定义字符，例如 `~`
- 非法八进制数，例如 `09`
- 非法十六进制数，例如 `0x3G`
- 非法指数浮点数，例如 `1.05e`
- 未闭合的块注释

错误格式：

```text
Error type A at Line [行号]: [说明文字].
```

## 编译运行

进入本目录：

```bash
cd ~/compiler/exp1.1
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

运行自定义输入：

```bash
./lexer your_source_file.cmm
```

当前目录保留为独立的词法分析实验。语法分析部分已经在 `../exp1.2` 中实现，那里使用 Bison 接收结构化 token 并生成语法树。
