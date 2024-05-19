// pl/0 compiler with code generation
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "pl0.h"

void error(long n)
{
    long i;

    printf("Error=>");
    for (i = 1; i <= cc-1; i++)
    {
        printf(" ");
    }
    
    printf("|%s(%d)\n", err_msg[n], n);
    
    err++;
}

void getch()
{
    // 当前状态下，该行是否已经读尽；若读尽则重置ll和cc，并且读入新的一行
    if(cc == ll)
    {
        if(feof(infile))
        {
            printf("************************************\n");
            printf("      program incomplete\n");
            printf("************************************\n");
            exit(1);
        }
        
        ll = 0; cc = 0;
        
        printf("%5d ", cx);
        
        //不断从文件对象读取单个字符，并存放到ch，然后复制到line中，直到该行结束或文件末尾
        //line相当于行缓冲区，每次进入if条件体都会读取新的一行，line[1]为该行第一个字符
        while((!feof(infile)) && ((ch=getc(infile))!='\n'))
        {
            printf("%c", ch);
            ll = ll + 1;
            line[ll] = ch;
        }

        printf("\n");
        
        ll = ll + 1; //实际的行长度应当包括第0个字符，所以在此处对ll+1
        line[ll] = ' ';//行代码最后一个字符恒定为空格
    }

    cc = cc + 1; 
    ch = line[cc];
}

// 该函数从代码中读取一个词法单元，并未这个词法单元编码（即为全局变量sym编码）
// 这个词法单元可能是：保留字、标识符、数字、各种符号
// 该函数执行完毕后，ch是该词法单元的下一个字符
void getsym()
{
    long i, j, k;

    //越过空格和制表符，直至下一个正常符号。
    //该循环结束时，ch将成为该行内第一个非空格非\t的字符
    while(ch == ' ' || ch == '\t')
    {
        getch();
    }

    // 越过空格和\t后，即可读入真正代码字符，词法分析开始
    // 词法单元以字母开头：词法单元是保留字或标识符
    if(isalpha(ch))
    {
        k = 0;

        // 获取1个词法单元存放到a中，k为标识符长度
        // 执行结束后，ch将是该词法单元的下一个字符
        do
        {
            if(k < al)
            {
                a[k] = ch; k = k + 1;
            }

            getch();
        } while(isalpha(ch) || isdigit(ch));

        // 将a的非当前词法单元部分全部填充为空格
        if(k >= kk)
        {
            kk = k;
        }
        else
        {
            do
            {
                kk = kk-1; a[kk] = ' ';
            } while(k < kk);
        }

        // 将读取到的词法单元a拷贝到id中，因此现在id是词法单元
        strcpy(id, a);

        // 使用二分查找检查id是否为保留字
        i = 0; 
        j = norw - 1;
        do
        {
            k = i +(j-i)/2;

            if(strcmp(id, word[k]) <= 0)
            {
                j = k - 1;
            }

            if(strcmp(id, word[k]) >=0)
            {
                i = k + 1;
            }
        } while(i <= j);

        // 若找到则说明id是保留字，否则为标识符；按照结果将sym赋予相应的编码
        if(i-1 > j)
        {
            sym = wsym[k];
        }
        else
        {
            sym = ident;
            //printf("(ID:%s)\n",id);
        }
    }
    // 词法单元以数字字符开头：词法单元是一个数字常量
    else if(isdigit(ch)) 
    {
        k = 0; num = 0; 
        sym = number;//将id编码为数字

        // 将该词法单元（连续的数字字符块）转化为int类型，即将这个数字的值记录下来
        do
        {
            num = num * 10 + (ch - '0');
            k = k + 1;
            getch();
        } while(isdigit(ch));
        if(k > nmax)
        {
            error(31);
        }
    }
    // 词法单元以:开头：该词法单元非常可能是赋值符号':='
    else if(ch == ':')
    {
        getch();
        
        if(ch == '=')
        {
            sym = becomes; getch();
        }
        else
        {
            sym = nul;// 无效字符
        }
    }
    // 词法单元的其他符号编码
    else if(ch == '<')
    {
        getch();
    
        if(ch == '=')
        {
            sym = leq; getch();
        }
        else if(ch == '>')
        {
            sym=neq; getch();
        }
        else
        {
            sym = lss;
        }
    }
    else if(ch == '>')
    {
        getch();
        
        if(ch == '=')
        {
            sym=geq; getch();
        }
        else
        {
            sym=gtr;
        }
    }
    // 若不是以上任何一种词法单元，则该词法单元是一个运算符，直接编码即可
    else
    {
        sym = ssym[(unsigned char)ch]; getch();
    }
}

// gen在存放最终汇编指令的code数组中添加一条指令，并且将cx+1（cx除初始化外只在此处变化）
void gen(enum fct x, long y, long z)
{
    if(cx > cxmax)
    {
        printf("program too long\n");
        exit(1);
    }

    code[cx].f = x; code[cx].l = y; code[cx].a = z;
    
    cx = cx + 1;
}

// 所谓的“紧急错误恢复”
void test(unsigned long s1, unsigned long s2, long n)
{
    if (!(sym & s1))
    {
        error(n);
        s1 = s1 | s2;
        
        while(!(sym & s1))
        {
            getsym();
        }
    }
}

// 将一个对象存入到标识符表中
void enter(enum object k)		
{
    //把标识符表索引+1
    tx = tx + 1;

    //记录标识符的词法单元名
    strcpy(table[tx].name, id);
    
    //记录标识符类型（指的是常量、变量、过程）
    table[tx].kind = k;
    
    switch(k)
    {
        //常量只需要知道值，变量只需要知道位置
        case constant:
            if(num > amax)
            {
                error(31);
                num = 0;
            }
            table[tx].val = num;
            break;

        case variable:
            table[tx].level = lev; table[tx].addr = dx; dx = dx + 1;
            break;

        case proc:
            table[tx].level = lev;
            break;
    }
}

// 在table中查找id词法单元，若找到则返回该词法单元的索引，否则返回0
long position(char* id)         // find identifier id in table
{
    long i;

    strcpy(table[0].name, id);
 
    i=tx;

    while(strcmp(table[i].name, id) != 0)
    {
        i = i - 1;
    }

    return i;
}

// 前置条件：getsym已读取到'const'词法单元，当前sym指向了下一个词法单元
// 执行：将整个常量声明语句读取
void constdeclaration()
{
    if (sym == ident) // 当前读的词法单元为标识符
    {
        getsym();

        if (sym == eql || sym == becomes) // 当前读的词法单元为等号或赋值号
        {
            if(sym == becomes)
            {
                error(1);
            }
            
            getsym();

            if (sym == number) // 当前读的词法单元为数字
            {
                enter(constant);
                getsym();
            }
            else
            {
                error(2);
            }
        }
        else
        {
            error(3);
        }
    }
    else
    {
        error(4);
    }
}

void vardeclaration()
{
    if(sym == ident)
    {
        enter(variable); 
        getsym();
    }
    else
    {
        error(4);
    }
}

// 该函数将一个block生成的所有汇编指令从编码转换为字符串并输出
void listcode(long cx0)         // list code generated for this block
{
    long i;

    for(i=cx0; i<=cx-1; i++)
    {
        printf("%10d%5s%3d%5d\n", i, mnemonic[code[i].f], code[i].l, code[i].a);
    }
}

void expression(unsigned long);

// 处理factor，factor结构为：Factor-> ident | number | ( Exp )
// 处理完该factor后，都会读入下一个词法单元
void factor(unsigned long fsys)
{
    long i;

    // 检验当前编码是否正确
    test(facbegsys, fsys, 24);

    // 循环条件为sym与factor开始符号编码一致
    while(sym & facbegsys)
    {
        //处理factor第一种推导（是一个标识符），则生成汇编代码，将其置于栈顶
        if(sym == ident)
        {
            i = position(id);
            
            if(i==0)
            {
                error(11);
            }
            else
            {
                switch(table[i].kind)
                {
                    case constant:
                        gen(lit, 0, table[i].val);
                        break;

                    case variable:
                        gen(lod, lev-table[i].level, table[i].addr);
                        break;

                    case proc:
                        error(21);
                        break;
                }
            }

            getsym();
        }
        // 处理factor的第二种推导，这实际上与处理常量的方法一致
        else if(sym == number)
        {
            if(num>amax)
            {
                error(31); num=0;
            }
            
            gen(lit,0,num);
            getsym();
        }
        // 处理factor的第三种推导，到此为止该推导仍需要读入Exp和一个)
        else if(sym == lparen)
        {
            getsym();
            expression(rparen|fsys);
        
            if(sym==rparen)
            {
                getsym();
            }
            else
            {
                error(22);
            }
        }

        test(fsys,lparen,23);
    }
}

// 处理Term，其结构为：Term->Factor{*Factor|/Factor}
// 该函数开始时，Term结构的第一个词法单元即Factor已被读入
void term(unsigned long fsys)
{
    unsigned long mulop;

    // 处理Term中第一个必然存在的Factor
    factor(fsys|times|slash);

    // 处理Term中的可选扩展，即是否有乘除因子
    while(sym==times || sym==slash)
    {
        mulop = sym; getsym();
    
        factor(fsys|times|slash);
        
        if(mulop == times)
        {
            gen(opr,0,4);
        }
        else{
            gen(opr,0,5);
        }
    }
}

// Exp分析，Exp结构为：Exp->[+|-]Term{+Term|-Term}
void expression(unsigned long fsys)
{
    unsigned long addop;

    // 处理Exp的第一种情况，即开头选定了+或-
    if(sym==plus || sym==minus)
    {
        // 先保留开头的+或-，然后读入下一个词法单元，该词法单元必为一个Term
        addop=sym;
        getsym();
       
       // 开始处理这个必然存在的Term
        term(fsys|plus|minus);
       
       // 开头可能存在的+或-：若为-则仍要生成减法操作汇编指令，若为+则无需做额外汇编指令
        if(addop==minus)
        {
            gen(opr,0,1);
        }
    }
    else
    {
        term(fsys|plus|minus);
    }

    // 处理Exp的可选扩展，即{+Term|-Term}
    while(sym==plus || sym==minus)
    {
        addop=sym; getsym();
    
        term(fsys|plus|minus);
    
        if(addop==plus)
        {
            gen(opr,0,2);
        }
        else
        {
            gen(opr,0,3);
        }
    }
}

// 处理Cond，其结构为：odd Exp | Exp RelOp Exp
void condition(unsigned long fsys)
{
    // 用于记录比较运算符的编码
    unsigned long relop;

    // 处理第一种推导：odd Exp
    if(sym==oddsym)
    {
        getsym(); 
        expression(fsys);
        gen(opr, 0, 6);
    }
    // 处理第二种推导：Exp RelOp Exp
    else
    {
        expression(fsys|eql|neq|lss|gtr|leq|geq);

        if(!(sym&(eql|neq|lss|gtr|leq|geq)))
        {
            error(20);
        }
        else
        {
            relop=sym; 
            getsym();
            expression(fsys);
            
            // 比较运算符的汇编指令生成在操作数入栈汇编指令之后
            switch(relop)
            {
                case eql:
                    gen(opr, 0, 8);
                    break;
            
                case neq:
                    gen(opr, 0, 9);
                    break;
            
                case lss:
                    gen(opr, 0, 10);
                    break;
            
                case geq:
                    gen(opr, 0, 11);
                    break;
            
                case gtr:
                    gen(opr, 0, 12);
                    break;
            
                case leq:
                    gen(opr, 0, 13);
                    break;
            }
        }
    }
}

// 处理statement
// 其结构为：Stmt-> ident := Exp | call ident | begin Stmt {; Stmt} end
//          | if Cond then Stmt | while Cond do Stmt | ε
// 该函数开始时，已经读取Stmt的第一个词法单元
// 该函数结束时，读入Stmt的下一个词法单元
void statement(unsigned long fsys)
{
    long i,cx1,cx2;

    // 处理Stmt的第一种推导：ident := Exp
    if(sym==ident)
    {
        i=position(id);
        if(i==0)
        {
            error(11);
        }
        else if(table[i].kind!=variable)	// assignment to non-variable
        {
            error(12); i=0;
        }

        getsym();

        // 处理:=赋值符号
        if(sym==becomes)
        {
            getsym();
        }
        else
        {
            error(13);
        }
        
        // 处理Exp
        expression(fsys);
        
        if(i!=0)
        {
            // 生成最终的赋值操作汇编指令
            gen(sto,lev-table[i].level,table[i].addr);
        }
    }
    // 处理Stmt的第二种推导：call ident
    else if(sym==callsym)
    {
        getsym(); // 理应读入一个proc标识符
        if(sym!=ident)
        {
            error(14);
        }
        else
        {
            i=position(id);
            if(i==0)
            {
                error(11);
            }
            else if(table[i].kind==proc)
            {
                gen(cal,lev-table[i].level,table[i].addr);
            }
            else
            {
                error(15);
            }
            
            getsym();
        }
    }
    // 处理Stmt的第四种推导： if Cond then Stmt
    else if(sym==ifsym)
    {
        getsym(); // 理应读入一个Cond（条件表达式）

        // 处理Cond
        condition(fsys|thensym|dosym);
    
        // 处理then
        if(sym==thensym)
        {
            getsym();
        }
        else
        {
            error(16);
        }

        cx1=cx;	
        // 生成最终的条件跳转汇编指令（目前还在占位，因为尚未清楚then的后续代码长度）
        gen(jpc,0,0);
        // 处理then的后续代码
        statement(fsys | elsesym);//@2

        if (sym == elsesym) //@2
        {
            getsym();
            cx2=cx;
            code[cx1].a = cx + 1;
            gen(jmp,0,0);
            statement(fsys);
            code[cx2].a=cx;
        }
        else
        {
            code[cx1].a = cx;
        }

        // 处理完毕后，将占位的jpc的跳转地址补充
        //code[cx1].a=cx;	
    }
    // 处理Stmt第三种推导，其结构为： begin Stmt {; Stmt} end
    else if(sym==beginsym)
    {
        getsym(); // 理应读入Stmt的第一个词法单元
        // 处理Stmt
        statement(fsys|semicolon|endsym);

        // 处理可选扩展{; Stmt}
        while(sym==semicolon||(sym&statbegsys))
        {
            if(sym==semicolon)
            {
                getsym();
            }
            else
            {
                error(10);
            }
            statement(fsys|semicolon|endsym);
        }
        // 处理end
        if(sym==endsym)
        {
            getsym();
        }
        else
        {
            error(17);
        }
    }
    // 处理Stmt第五种推导，其结构为： while Cond do Stmt
    else if(sym==whilesym)
    {
        whilelev = whilelev+1;//@3
        cx1 = cx;
        getsym(); // 理应读入一个Cond
        // 处理Cond
        condition(fsys | dosym); 
        cx2=cx;	
        gen(jpc,0,0);// while的跳转占位符

        // 处理do
        if(sym==dosym)
        {
            getsym();
        }
        else
        {
            error(18);
        }

        // 处理Stmt
        statement(fsys|exitsym); 
        
        // 生成跳转回while判断的汇编指令
        gen(jmp,0,cx1);
        // 填充while判断的占位符
        code[cx2].a=cx;
        
        //@3 填充已经生成的exit跳转指令
        for (int i = 0; i < exit_num[whilelev];i++)
        {
            code[exit_pos[whilelev][i]].a=cx;
        }
        exit_num[whilelev]=0;
        whilelev = whilelev - 1; 
    }
    //@3 添加exit判断
    else if(sym==exitsym)
    {
        //printf("exit sym found!\n");
        if(fsys|whilesym)
        {
            //printf("Here is in a While!\n");

            exit_num[whilelev] = exit_num[whilelev] + 1;
            exit_pos[whilelev][exit_num[whilelev]-1]=cx;
            gen(jmp,0,0);
        }
        getsym();
    }

    test(fsys,0,19);
}

// 处理block，其结构为：Block->[ConstDecl] [VarDecl][ProcDecl] Stmt
void block(unsigned long fsys)  
{

    long tx0; // 初始表索引
    long cx0; // 初始代码索引
    long tx1; // 在处理嵌套block前保存当前表索引
    long dx1; // 在处理嵌套block前保存数据分配索引

    dx=3;// 分配3个单元供运行期间存放静态链SL、动态链DL、返回地址RA
    tx0=tx;
    // 该block的词法单元名目前已经置入table[tx]位置，在这里只是将block的地址进行补充
    // block的地址与变量栈的地址不同，这里应当使用cx也即最终生成汇编指令地址
    table[tx].addr = cx; // table[tx].addr的值指向gen(jmp,0,0);占位编译指令地址
    gen(jmp,0,0);

    if(lev>levmax)
    {
        error(32);
    }
    
    do
    {
        // do内部首先处理这个块的常量声明部分和变量声明部分，然后循环处理过程部分

        // 常量声明部分，则将该常量声明部分读取到一个分号semicolon为止
        // 一边读取，一边将这些常量词法单元名放到table中
        // （这在constdeclaration中调用enter来实现）
        // 处理可选扩展ConstDecl
        if(sym==constsym)
        {
            getsym();
        
            do
            {
                constdeclaration();
                while(sym==comma)
                {
                    getsym(); constdeclaration();
                }
                
                if(sym==semicolon)
                {
                    getsym();
                }
                else
                {
                    error(5);
                }
            } while(sym==ident);
        }

        // 处理可选扩展VarDecl
        if(sym==varsym)
        {
            getsym();
            do
            {
                vardeclaration();
                while(sym==comma)
                {
                    getsym(); vardeclaration();
                }
                
                if(sym==semicolon)
                {
                    getsym();
                }
                else
                {
                    error(5);
                }
            } while(sym==ident);
        }

        // 循环处理过程声明部分，并在该循环体中进行嵌套深度的计算
        //  处理可选扩展ProcDecl
        while(sym==procsym)
        {
            getsym();
            if(sym==ident)
            {
                enter(proc); getsym();
            }
            else
            {
                error(4);
            }
            
            if(sym==semicolon)
            {
                getsym();
            }
            else
            {
                error(5);
            }
            
            //嵌套深度+1，保存当前标识符表索引以及当前数据分配索引。然后分析子代码块
            lev=lev+1; tx1=tx; dx1=dx;
            block(fsys|semicolon);
            lev=lev-1; tx=tx1; dx=dx1;
            
            if(sym==semicolon)
            {
                getsym();
                test(statbegsys|ident|procsym,fsys,6);
            }
            else
            {
                error(5);
            }
        }
        
        test(statbegsys|ident,declbegsys,7);
    } while(sym&declbegsys);//当读到的sym处在declarement开始符号集中时，继续执行block判断

    // 找到本Block开头占位的跳转指令，并设置其多用途（跳转目标地址）为当前cx（也就是Stmt部分的第一句汇编指令）
    // 该语句执行后，开头占位的jmp指令将会指向gen(Int,0,dx);所生成的汇编指令
    code[table[tx0].addr].a=cx;
    // 给过程词法单元的地址属性附上地址（注意，初始的大Block没有名字，只有开始语句的地址）
    table[tx0].addr=cx;		// start addr of code 
    cx0=cx; 
    gen(Int,0,dx);// 根据当前的变量dx开辟栈区
    // 处理必然存在的Stmt
    statement(fsys|semicolon|endsym);
    gen(opr,0,0); // return
    test(fsys,0,8);
    listcode(cx0);
}

// 
long base(long b, long l)
{
    long b1;

    b1=b;

    while (l>0)         // find base l levels down
    {
        b1=s[b1]; l=l-1;
    }

    return b1;
}

void interpret()
{
    long p,b,t;		// program-, base-, topstack-registers
    instruction i;	// instruction register

    printf("start PL/0\n");
    t=0; b=1; p=0;
    s[1]=0; s[2]=0; s[3]=0;
    
    do
    {
        i=code[p]; p=p+1;
    
        switch(i.f)
        {
            case lit:
                t=t+1; s[t]=i.a;
                break;
        
            case opr:
                switch(i.a) 	// operator
                {
                    case 0:	// return
                        t=b-1; p=s[t+3]; b=s[t+2];
                        break;
                 
                    case 1:
                        s[t]=-s[t];
                        break;
                 
                    case 2:
                        t=t-1; s[t]=s[t]+s[t+1];
                        break;
                 
                    case 3:
                        t=t-1; s[t]=s[t]-s[t+1];
                        break;
                 
                    case 4:
                        t=t-1; s[t]=s[t]*s[t+1];
                        break;
                 
                    case 5:
                        t=t-1; s[t]=s[t]/s[t+1];
                        break;
                 
                    case 6:
                        s[t]=s[t]%2;
                        break;
                 
                    case 8:
                        t=t-1; s[t]=(s[t]==s[t+1]);
                        break;
                 
                    case 9:
                        t=t-1; s[t]=(s[t]!=s[t+1]);
                        break;
                 
                    case 10:
                        t=t-1; s[t]=(s[t]<s[t+1]);
                        break;
                 
                    case 11:
                        t=t-1; s[t]=(s[t]>=s[t+1]);
                        break;
                 
                    case 12:
                        t=t-1; s[t]=(s[t]>s[t+1]);
                        break;
                 
                    case 13:
                        t=t-1; s[t]=(s[t]<=s[t+1]);
                }
                break;
            
            case lod:
                t=t+1; s[t]=s[base(b,i.l)+i.a];
                break;
            
            case sto:
                s[base(b,i.l)+i.a]=s[t]; printf("%10d\n", s[t]); t=t-1;
                break;
            
            case cal:		// generate new block mark
                s[t+1]=base(b,i.l); s[t+2]=b; s[t+3]=p;
                b=t+1; p=i.a;
                break;
            
            case Int:
                t=t+i.a;
                break;
            
            case jmp:
                p=i.a;
                break;
            
            case jpc:
                if(s[t]==0)
                {
                    p=i.a;
                }
                t=t-1;
        }
    } while(p!=0);
    printf("end PL/0\n");
}

int main()
{
    long i;

    //把操作符码数组全部置为nul符号的码
    for(i=0; i<256; i++)
    {
        ssym[i]=nul;
    }
    
    //初始化保留字的字符串
    strcpy(word[0], "begin     ");  // 0
    strcpy(word[1], "call      ");  // 1
    strcpy(word[2], "const     ");  // 2
    strcpy(word[3], "do        ");  // 3
    strcpy(word[4], "else      ");  // 4
    strcpy(word[5], "end       ");  // 5
    strcpy(word[6], "exit      ");  // 6
    strcpy(word[7], "if        ");  // 7
    strcpy(word[8], "odd       ");  // 8
    strcpy(word[9], "procedure ");  // 9
    strcpy(word[10], "then      "); // 10
    strcpy(word[11], "var       "); // 11
    strcpy(word[12], "while     "); // 12

    // 初始化保留字编码数组
    // 实际上，word和wsym的索引相一致，这个索引间接实现了从保留字字符串到保留字编码的映射
    wsym[0] = beginsym;  // b
    wsym[1] = callsym;   // c
    wsym[2] = constsym;  // c
    wsym[3] = dosym;     // d
    wsym[4] = elsesym;   // e
    wsym[5] = endsym;    // e
    wsym[6] = exitsym;   // e
    wsym[7] = ifsym;     // i
    wsym[8] = oddsym;    // o
    wsym[9] = procsym;   // p
    wsym[10] = thensym;  // t
    wsym[11] = varsym;   // v
    wsym[12] = whilesym; // w

    //初始化运算符编码数组，并且实际上建立了从运算符字符到运算符编码的映射
    ssym['+']=plus;
    ssym['-']=minus;
    ssym['*']=times;
    ssym['/']=slash;
    ssym['(']=lparen;
    ssym[')']=rparen;
    ssym['=']=eql;
    ssym[',']=comma;
    ssym['.']=period;
    ssym[';']=semicolon;

    // 初始化操作符字符数组，实际上实现了从操作符枚举类型（其实也是其对应的数值）到操作符字符串的映射
    strcpy(mnemonic[lit],"LIT");
    strcpy(mnemonic[opr],"OPR");
    strcpy(mnemonic[lod],"LOD");
    strcpy(mnemonic[sto],"STO");
    strcpy(mnemonic[cal],"CAL");
    strcpy(mnemonic[Int],"INT");
    strcpy(mnemonic[jmp],"JMP");
    strcpy(mnemonic[jpc],"JPC");

    // @3 初始化while_nest
    whilelev=-1;
    for (int i = 0; i < whilevmax; i++)
    {
        exit_num[i] = 0;
        //printf("exit_num: %d\n", exit_num[i]);
    }

    // 实现了声明部分、语句部分、因子部分编码，这个编码似乎是开始符号集的编码相或的结果？
    declbegsys=constsym|varsym|procsym;
    statbegsys = beginsym | callsym | ifsym | whilesym;
    facbegsys=ident|number|lparen;

    // 编译过程正式开始，这里输入需要编译的pl0文件
    printf("please input source program file name: ");
    scanf("%s",infilename);
    printf("\n");
  
    if((infile=fopen(infilename,"r"))==NULL)
    {
        printf("File %s can't be opened.\n", infilename);
        exit(1);
    }

    err=0;
    cc=0; cx=0; ll=0; ch=' '; kk=al; getsym();
    lev=0; tx=0;
    block(declbegsys|statbegsys|period);
    
    if(sym!=period)
    {
        error(9);
    }
    
    if(err==0)
    {
       // interpret();
    }
    else
    {
        printf("errors in PL/0 program\n");
    }
    
    fclose(infile);

    return (0);
}
