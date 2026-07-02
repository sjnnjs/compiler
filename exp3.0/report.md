# 实践三报告：中间代码生成

## 一、目的与任务

本次实验的目标是把前两次实验得到的词法、语法和语义分析器继续扩展为翻译器，在输入程序通过检查后生成课程规定格式的中间代码。中间代码采用三地址码形式，每行一条语句，包含 `FUNCTION`、`PARAM`、`LABEL`、`GOTO`、`IF ... GOTO`、`RETURN`、`DEC`、`ARG`、`CALL`、`READ`、`WRITE` 以及赋值和算术运算。

当前实现面向实践三必做假设：输入程序不包含词法、语法和语义错误；变量为整型或一维整型数组；函数参数为普通整型变量；没有全局变量使用；函数不会返回数组或结构体。结构体变量、结构体参数、数组参数和高维数组属于选做范围，本实现会主动给出无法翻译提示。

## 二、过程

实现时继续沿用实践二的前端结构。Flex 负责识别 token，Bison 构建 AST，语义分析阶段先检查程序并维护变量、函数和结构体符号表。为了适配实践三样例，语义分析初始化时预置了 `read()` 和 `write(int)` 两个函数，其中 `read` 返回 `int`，`write` 接收一个 `int` 参数并返回 `int`。这样 I/O 函数既能通过语义检查，又能在中间代码生成阶段被翻译成专门的 `READ` 和 `WRITE` 指令。

中间代码生成器位于 `ir.c`。它不读取语法树打印文本，而是直接遍历 AST。生成器内部维护三类编号：源程序变量映射为 `v1`、`v2` 等，表达式中间结果映射为 `t1`、`t2` 等，控制流标号映射为 `label1`、`label2` 等。变量映射表同时记录数组变量的长度和空间大小，因此遇到局部一维数组定义时可以生成 `DEC v size`。

表达式翻译采用按需生成临时变量的方式。普通变量和立即数可直接作为操作数；复杂算术表达式生成新的临时变量保存结果。赋值左侧单独翻译为左值：普通变量直接赋值，一维数组元素先计算 `&array + index * 4` 得到地址，再根据读写方向生成 `*addr := value` 或 `tmp := *addr`。

条件表达式使用 `translate_cond()` 直接生成跳转代码。关系表达式生成 `IF x relop y GOTO label`，逻辑与、逻辑或和取反通过短路控制流连接多个标号。若布尔表达式出现在普通表达式位置，则先生成一个初始化为 `0` 的临时变量，在真分支中改为 `1`。

函数调用先按源程序顺序翻译实参表达式，再按课程要求逆序输出 `ARG`。普通函数调用生成 `t := CALL func`；`read()` 生成 `READ t`；`write(x)` 生成 `WRITE x`，若参数是立即数则先放入临时变量，避免输出文件中出现虚拟机可能不接受的 `WRITE #n`。

## 三、结果与分析

实验三目录提供 6 个独立测试样例：

```text
01_sign.cmm       read/write 和 if-else
02_fact.cmm       递归函数调用
03_array.cmm      一维数组声明、读写和地址计算
04_while.cmm      while 循环
05_call_args.cmm  多参数调用和 ARG 逆序
06_logic.cmm      &&、||、! 的条件翻译
```

编译和测试命令为：

```bash
cd ~/compiler/exp3.0
make clean
make
make test
```

测试时程序会为每个 `test/*.cmm` 生成对应的 `test/out/*.ir`。例如，阶乘样例会生成如下关键中间代码片段：

```text
FUNCTION fact :
PARAM v1
IF v1 == #1 GOTO label1
GOTO label2
LABEL label1 :
RETURN v1
LABEL label2 :
t1 := v1 - #1
ARG t1
t2 := CALL fact
t3 := v1 * t2
RETURN t3
```

数组样例会生成 `DEC` 申请连续空间，并通过地址加偏移访问元素：

```text
DEC v1 12
t1 := #0 * #4
t2 := &v1 + t1
*t2 := #1
```

除检查生成文本外，还可以用课程提供的 `irsim` 打开生成的 `.ir` 文件进行运行验证。验证步骤如下：

```bash
cd ~/compiler/exp3.0
make
mkdir -p test/out
./cc test/01_sign.cmm test/out/01_sign.ir
./cc test/02_fact.cmm test/out/02_fact.ir
cd ~/compiler
./tools/run_irsim.sh
```

在 `irsim` 界面中打开 `test/out/01_sign.ir` 或 `test/out/02_fact.ir` 后，可以直接运行或单步执行。遇到 `READ` 指令时，`irsim` 会提示输入整数。`01_sign.ir` 可分别输入 `5`、`-3`、`0`，预期输出分别为 `1`、`-1`、`0`；`02_fact.ir` 可输入 `5`，预期输出为 `120`。不含 `read()` 的 `04_while.ir` 可直接运行，预期输出为 `10`。

从结果看，当前中间代码生成器已经覆盖必做部分的核心语句和表达式，并保持了从 AST 到 IR 的明确接口。后续如果继续实现选做要求，可以扩展变量映射表中的类型信息，加入结构体字段偏移和多维数组维度信息；若进入目标代码生成阶段，也可以直接以当前 IR 文件作为输入。
