# 实践三：中间代码生成

本目录在实践二的词法、语法和语义分析基础上，实现 C-- 到课程三地址中间代码的翻译。程序接收一个输入源文件和一个输出 IR 文件路径：

```bash
./cc source.cmm output.ir
```

若输入没有词法、语法和语义错误，程序将中间代码写入输出文件；若发现错误或遇到必做范围外的结构，则不生成 IR。

## 实现范围

当前完成实践三必做部分：

- 函数定义、形参 `PARAM`、普通函数调用、递归调用和 `ARG` 逆序传参。
- 局部整型变量、局部一维整型数组和数组空间 `DEC`。
- 赋值、算术运算、关系运算、逻辑运算、取反和负号。
- `if`、`if-else`、`while`、复合语句和 `return`。
- 预定义函数 `read()` 和 `write(int)`，分别翻译为 `READ` 和 `WRITE`。
- 一维数组元素读写，使用 `&`、`*` 和偏移地址完成。

未实现选做要求 4.1 和 4.2：结构体变量、结构体参数、数组参数和高维数组不作为本阶段支持目标。遇到这些输入时，程序会给出 `Cannot translate: ...` 提示。

## 编译运行

```bash
cd ~/compiler/exp3.0
make clean
make
```

运行单个文件：

```bash
./cc test/01_sign.cmm sign.ir
```

运行全部样例：

```bash
make test
```

`make test` 会为 `test/*.cmm` 生成对应的 `test/out/*.ir`，并把生成结果打印出来。`test/out/` 是生成物，不需要提交。

## irsim 验证

先生成 IR：

```bash
cd ~/compiler/exp3.0
make
mkdir -p test/out
./cc test/01_sign.cmm test/out/01_sign.ir
./cc test/02_fact.cmm test/out/02_fact.ir
./cc test/04_while.cmm test/out/04_while.ir
```

启动课程 IR 模拟器：

```bash
cd ~/compiler
./tools/run_irsim.sh
```

在 `irsim` 界面中打开生成的 `.ir` 文件，点击运行或单步执行。遇到 `READ` 时输入一个整数：

```text
01_sign.ir 输入 5   -> 输出 1
01_sign.ir 输入 -3  -> 输出 -1
01_sign.ir 输入 0   -> 输出 0
02_fact.ir 输入 5   -> 输出 120
04_while.ir 无输入   -> 输出 10
```

## 测试样例

```text
test/
├── 01_sign.cmm       # read/write、if-else
├── 02_fact.cmm       # 递归函数和普通函数调用
├── 03_array.cmm      # 一维数组 DEC、读写和地址计算
├── 04_while.cmm      # while 循环
├── 05_call_args.cmm  # 多参数调用和 ARG 逆序
└── 06_logic.cmm      # &&、||、! 的条件翻译
```

## 接口设计

实验三仍沿用前端流水线：

```text
source.cmm -> lexer -> parser -> AST -> semantic_analyze -> ir_generate -> output.ir
```

`parser.y` 只负责命令行入口和阶段调度。`semantic.c` 在实践二基础上加入 `read`、`write` 两个预定义函数，避免实验三 I/O 样例被误判为未定义函数。

`ir.c` 直接遍历 AST 生成中间代码。内部维护一张轻量变量映射表，把源程序变量映射为 `v1`、`v2` 等 IR 名称；临时变量使用 `t1`、`t2`，跳转标号使用 `label1`、`label2`。这样中间代码不依赖源语言作用域，也避免源程序变量名和临时变量名冲突。

数组元素作为左值时先翻译成地址，再生成 `*addr := value`；数组元素作为右值时先计算地址，再生成 `tmp := *addr`。条件表达式通过 `translate_cond()` 直接生成跳转代码，普通表达式中的布尔值则生成 `0/1` 临时变量。
