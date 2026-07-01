# 实践二：语义分析与符号表

本目录在实践 1.2 的词法/语法分析器基础上，将前端改造为语义分析器。程序接收一个 C-- 源文件路径，若没有语义错误则不输出任何内容；若存在语义错误，则按课程要求输出 `Error type N at Line X: ...`。

```text
exp2.0/
├── lexer.l
├── parser.y
├── ast.h
├── ast.c
├── semantic.h
├── semantic.c
├── Makefile
├── project2.0.md
└── test/
```

## 实现内容

当前完成必做的 1-17 类语义错误检查：

- 变量/函数未定义
- 变量/函数/结构体重复定义
- 赋值、操作数、返回值、函数实参类型不匹配
- 赋值左侧不是左值
- 非数组下标访问、非函数调用、数组下标非整数
- 非结构体成员访问、访问不存在的域
- 结构体域重复定义或初始化
- 使用未定义结构体定义变量

本实现采用实验要求中的必做假设：

- 所有变量和形参共享全局作用域，不能重名。
- 函数只能定义，不能声明。
- 结构体采用名等价。
- 函数不能嵌套定义。
- 结构体域不与变量重名，不同结构体中的域互不重名；本实现仍检查同一结构体内域名重复。

## 编译运行

```bash
cd ~/compiler/exp2.0
make clean
make
```

运行全部样例：

```bash
make test
```

运行单个文件：

```bash
./cc test/01_undefined_variable.cmm
```

## 测试样例

所有测试样例均放在 `test/` 目录下，每个样例一个独立文件：

```text
00_ok.cmm
01_undefined_variable.cmm
02_undefined_function.cmm
03_redefined_variable.cmm
04_redefined_function.cmm
05_assignment_type.cmm
06_assignment_left_value.cmm
07_operand_type.cmm
08_return_type.cmm
09_function_args.cmm
10_not_array.cmm
11_not_function.cmm
12_array_index.cmm
13_not_struct.cmm
14_no_field.cmm
15_field_redefined.cmm
16_struct_redefined.cmm
17_undefined_struct.cmm
```

`00_ok.cmm` 不应输出任何内容；其余样例分别对应错误类型 1-17。

## 接口设计

`lexer.l` 和 `parser.y` 继续作为前端黑盒，只负责从源文件构建 AST。`semantic.c` 只依赖 `ASTNode` 公开结构和 `ast_line()` 等接口，不依赖 Flex/Bison 的内部状态。

语义分析阶段内部维护三类符号表：

- 变量表：保存变量和函数形参。
- 函数表：保存函数返回类型和形参类型序列。
- 结构体表：保存结构体名和字段链表。

类型系统使用 `Type` 表示基本类型、数组类型、结构体类型、函数类型和错误类型。表达式检查会返回表达式类型，供赋值、运算、函数调用、数组访问和结构体访问继续判断。错误类型用于抑制连锁误报。
