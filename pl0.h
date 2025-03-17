#include <stdio.h>

#define norw 15    // 保留字的数量
#define txmax 100  // 标识符表的长度
#define nmax 14    // 数字中的最大位数
#define al 10      // 标识符的最大长度
#define amax 2047  // 最大地址
#define levmax 3   // 块嵌套的最大深度
#define cxmax 2000 // 代码数组的大小

// 作用：使用32个位编码一个符号，这些编码相当于一个翻版的ASCII码。这样的编码是为了后续对某些符号进行判断。
#define nul 0x1            // 空符号
#define ident 0x2          // 标识符
#define number 0x4         // 数字

#define plus 0x8           // 加号
#define minus 0x10         // 减号
#define times 0x20         // 乘号
#define slash 0x40         // 斜杠

#define oddsym 0x80        // odd符号
#define eql 0x100          // 等号
#define neq 0x200          // 不等号
#define lss 0x400          // 小于号
#define leq 0x800          // 小于等于号
#define gtr 0x1000         // 大于号
#define geq 0x2000         // 大于等于号

#define lparen 0x4000      // 左括号
#define rparen 0x8000      // 右括号
#define comma 0x10000      // 逗号
#define semicolon 0x20000  // 分号
#define period 0x40000     // 句号
#define becomes 0x80000    // 赋值符号

#define beginsym 0x100000  // begin保留字
#define endsym 0x200000    // end保留字
#define ifsym 0x400000     // if保留字
#define thensym 0x800000   // then保留字
#define whilesym 0x1000000 // while保留字
#define dosym 0x2000000    // do保留字
#define callsym 0x4000000  // call保留字
#define constsym 0x8000000 // const保留字
#define varsym 0x10000000  // var保留字
#define procsym 0x20000000 // procedure保留字
#define elsesym 0x40000000 // else保留字
#define exitsym 0x101      // exit保留字
#define funcsym 0x105
#define retsym 0x106

// 以下的保留字尚未投入使用
#define arraysym 0x102     // array保留字
#define readsym 0x103      // read保留字
#define writesym 0x104     // read保留字

// 将对象分成常量、变量、过程
enum object
{
    constant,
    variable,
    proc,
    func
};

//     lit, // 将常数置于栈顶
//     opr, // 一组算术或逻辑运算指令
//     lod, // 将变量置于栈顶
//     sto, // 将栈顶的值赋与某变量
//     cal, // 用于过程调用的指令
//     Int, // 在数据栈中分配存贮空间
//     jmp, // 跳转
//     jpc  // 跳转
//     这里定义了操作码f
enum fct
{
    lit, opr, lod, sto, cal, Int, jmp, jpc
};

// 这里定义了一个指令的结构，它包括操作码f、层次差l、多用途a。
// long类型的变量恰好可以存储上面定义的32位的字符
typedef struct
{
    enum fct f; // 操作码
    long l;     // 层次差
    long a;     // 多用途？
} instruction;

// 报错消息定义
char* err_msg[] =
{
/*  0 */    "",
/*  1 */    "Found ':=' when expecting '='.",
/*  2 */    "There must be a number to follow '='.",
/*  3 */    "There must be an '=' to follow the identifier.",
/*  4 */    "There must be an identifier to follow 'const', 'var', or 'procedure'.",
/*  5 */    "Missing ',' or ';'.",
/*  6 */    "Incorrect procedure name.",
/*  7 */    "Statement expected.",
/*  8 */    "Follow the statement is an incorrect symbol.",
/*  9 */    "'.' expected.",
/* 10 */    "';' expected.",
/* 11 */    "Undeclared identifier.",
/* 12 */    "Illegal assignment.",
/* 13 */    "':=' expected.",
/* 14 */    "There must be an identifier to follow the 'call'.",
/* 15 */    "A constant or variable can not be called.",
/* 16 */    "'then' expected.",
/* 17 */    "';' or 'end' expected.",
/* 18 */    "'do' expected.",
/* 19 */    "Incorrect symbol.",
/* 20 */    "Relative operators expected.",
/* 21 */    "Procedure identifier can not be in an expression.",
/* 22 */    "Missing ')'.",
/* 23 */    "The symbol can not be followed by a factor.",
/* 24 */    "The symbol can not be as the beginning of an expression.",
/* 25 */    "",
/* 26 */    "",	
/* 27 */    "",
/* 28 */    "",
/* 29 */    "",
/* 30 */    "",
/* 31 */    "The number is too great.",
/* 32 */    "There are too many levels."
};

char ch;           // 存储最后读取的字符
unsigned long sym; // 存储最后读取的词法单元的编码
char id[al + 1];   // 存储最后读取的词法单元字符串
long num;          // 存储最后读取的数字
long cc;           // 字符计数器
long ll;           // 行长度
long kk, err;      // 其他计数器和错误码
long cx;           // 下一条最终汇编指令地址（指向code数组的位置）

char line[81];               // 用于存储一行文本，长度为81，通常用于读取源代码的一行
char a[al + 1];              // 用于存储标识符或其他字符串，长度为al+1，al是标识符的最大长度
instruction code[cxmax + 1]; // 存储最终汇编指令的数组
char word[norw][al + 1];     // 存储保留字
                             // 使用二维数组的原因：这是一个存放字符串（本身就是数组）的数组
unsigned long wsym[norw];    // 存储保留字的编码
unsigned long ssym[256];     // 存储运算符的编码

char mnemonic[8][3 + 1]; // 存储汇编指令操作符的字符串
unsigned long declbegsys, statbegsys, facbegsys;
// declbegsys存储声明部分可以出现的开始符号集合
// statbegsys存储语句部分可以出现的开始符号集合
// facbegsys存储因子部分可以出现的开始符号集合

// table是结构标识符数组，也即符号表
struct
{
    char name[al + 1]; // 标识符名称
    enum object kind;  // 对象类型枚举，表示变量、常量、过程等
    long val;          // 对象的值
    long level;        // 对象所处的层级
    long addr;         // 对象的地址
} table[txmax + 1];    // txmax是最大表长度

// 读取的文件名和文件对象
char infilename[80];
FILE* infile;

// the following variables for block
long dx;  // 数据分配索引，表示在当前的block中某个标识符的位置（从0开始，越新定义则索引越大）
long lev; // 当前嵌套块的深度，用于表示当前block所处的层级。最小嵌套深度为0，即不在任何显式过程内的代码部分
long tx;  // 当前符号表索引，用于表示当前标识符在符号表的位置

#define stacksize 50000
long s[stacksize]; // 数据存储区，用于解释器执行时的数据操作

// @3
#define whilevmax 3
int whilelev;
int exit_num[whilevmax];
long exit_pos[whilevmax][3] = {0};

// @4
struct
{
    char name[al + 1];
} parameter[5];
int paranum0 = 0;
long retpos[5]={0};
int retnum=0;