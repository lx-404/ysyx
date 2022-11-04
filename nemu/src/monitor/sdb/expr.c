/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <string.h>

/*把不同类型的token用不同的编码表示*/
enum {
  TK_NOTYPE = 256,  //空格
  TK_EQ,  //识别双等号，暂时不需要
  TK_NUM, //识别数字0~9
  HEX,
  REG,
  TK_UEQ
   /*加减乘除四则运算不需要，43~47*/
  /*括号也不需要，都可以使用ascii表示，40，41*/
  /* TODO: Add more token types */

};



static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  {"0x[0-9A-Fa-f]+", HEX}, //16进制数字
  {"\\$[0-9a-z]+", REG},//寄存器
  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"\\-", '-'},
  {"\\*", '*'},
  {"\\/", '/'},
  {"\\(", '('},
  {"\\)", ')'},
  {"[0-9]+", TK_NUM},
  {"==", TK_EQ},        // equal
  {"!=", TK_UEQ},
};

#define NR_REGEX ARRLEN(rules)  //得到规则的个数
#define MAXSIZE_NUM  100
static regex_t re[NR_REGEX] = {};  //一种特定的结构体数据类型，专门用于存放编译后的正则表达式。

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
 /*初始化的时候定义好的正则表达式*/
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED); 
     
    //编译正则表达式  第二个参数用于指向正则表达式的指针没第三个参数一般使用REG_EXTENDED，执行成功返回0
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);  
      /*执行regcomp 或者regexec 产生错误的时候，就可以调用这个函数而返回一个包含错误信息的字符串
        1：返回错误代号，2：regcomp编译好的正则表达式，3.存放错误信息的字符串内存空间，4.指明错误信息的空间大小
      */
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;


/*栈操作*/
typedef struct
{
  char data[MAXSIZE_NUM];
  int top;
}Stack;
//初始化栈
void InitStack(Stack **s)
{
  *s = (Stack *) malloc (sizeof (Stack));
  (*s)->top = -1;
}

// 入栈
bool Push (Stack *s, char e)
{
  if(s->top == MAXSIZE_NUM - 1)
    return false;
  s->top ++;
  s->data[s->top] = e;
  return true;
}
//出栈
bool Pop(Stack **s , char *x)
{
  if((*s)->top == -1)
    return false;
  *x = (*s)->data[(*s)->top];
  (*s)->top --;
  return true;
}
//得到栈顶元素
bool GetTop(Stack **s, char *x)
{
  if((*s)->top == -1)
    return false;
  *x = (*s)->data[(*s)->top];
  return true;
}

//判断是否为空
bool StackEmpty(Stack **s)
{
  if((*s)->top == -1)
    return true;
  return false;
}


/*__attribute__((used))：该函数属性通知编译器在目标文件中保留一个静态函数/变量，即使它没有被引用*/
static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;
int count;  //记录有几个符号和数字
char postexp[MAXSIZE_NUM]; //存放后缀表达式


static bool make_token(char *e) {
  int position = 0;
  int i;
  char tok[32];
  char x;
  /*regmatch_t有两个成员变量，rm_so 存放匹配文本串在目标串中的开始位置，rm_eo 存放结束位置*/
  regmatch_t pmatch;
  Stack *Optr;
  InitStack(&Optr);
  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      /*匹配正则表达式regexec，识别成功返回0
      1. 编译好的正则表达式，2.目标文本串，3.regmatch_t的长度，4.存放匹配文本串的位置信息，5.0就行
      */
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        // 把字符串逐个识别成token，存到pmatch
        char *substr_start = e + position;
        // 把token对应的起始字符串地址存入substr_start
        int substr_len = pmatch.rm_eo;
        memset(tok,0,sizeof tok);
        if(rules[i].token_type == TK_NUM)
        {
          int j = 0;
          for(int i = position; i < (position+substr_len); i ++)
            {
              tok[j] = e[i];
              j ++;
            }
            //printf("tok = %s\n", tok);

        }
        // 把token长度存入substr_len
        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;
        if(substr_len >= 23 && rules[i].token_type == 258)
        {
          printf("buffer overflow!\n");
          return 0;
        }
        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        //printf("rules[i].token_type = %d\n",rules[i].token_type);
        switch (rules[i].token_type) {
          case '(' : {
            tokens->type = '('; 
            //printf("%c\n",tokens->type);
            strcpy(tokens->str,substr_start); 
            Push(Optr, '(');  //如果是左括号直接入栈
            break;

            }
          case ')' : {
            tokens->type = ')';  
            //printf("%c\n",tokens->type);
            strcpy(tokens->str,substr_start); 
            Pop(&Optr, &x);  //把栈顶元素出栈
            while(x != '(')  //如果出的元素不是(,那么久把他放入到postexp中
            {
              postexp[count++] = x;
              Pop(&Optr, &x);
            }
            break;
            }

          case '+' : {
            tokens->type = '+'; 
            //printf("%c\n",tokens->type);
            strcpy(tokens->str,substr_start); 
            while (!StackEmpty(&Optr)) //如果栈不为空
            {
              GetTop(&Optr, &x); //得到栈顶元素，由于 + - 的优先级只比 ( 大，所有只要栈顶字符不为 ( 就一直出栈；反之，则将 + - 入栈。
              if(x == '(')
                break;
              else
              {
                postexp[count++] = x;
                Pop(&Optr,&x);
              }
            }
            //入栈
            Push(Optr, '+');
            break;
            }
            
          case '-' : {
            tokens->type = '-';  
            //printf("%c\n",tokens->type);
            strcpy(tokens->str,substr_start); 
            while (!StackEmpty(&Optr)) //如果栈不为空
            {
              GetTop(&Optr, &x); //得到栈顶元素，由于 + - 的优先级只比 ( 大，所有只要栈顶字符不为 ( 就一直出栈；反之，则将 + - 入栈。
              if(x == '(')
                break;
              else
              {
                postexp[count++] = x;
                Pop(&Optr,&x);
              }
            }
            //入栈
            Push(Optr, '-');
            break;
            }

          case '*' : {
            tokens->type = '*';  
            //printf("%c\n",tokens->type);
            strcpy(tokens->str,substr_start); 
            // * / 优先级比 * / ^ ! 小，所有如果栈顶运算符是它们，就出栈；反之就将 * / 入栈
            while(!StackEmpty(&Optr))
            {
              GetTop(&Optr, &x);
              if(x == '/' || x == '*' || x == '^' || x == '!')// * / 的优先级仅仅低于它前面的 * /，高于前面的 + -，所以要将前面的 * / 弹出栈；+ - 保留，因为新的 * / 会放在栈低，优先级高。
              {
                  postexp[count++] = x;
                  Pop(&Optr, &x);
              }
              else
                break;  //其他情况（ + - 左括号 ）退出
            }
            Push(Optr, '*');
            break;
            }

          case '/' : {
            tokens->type = '/';  
            //printf("%c\n",tokens->type);
            strcpy(tokens->str,substr_start); 
                        while(!StackEmpty(&Optr))
            {
              GetTop(&Optr, &x);
              if(x == '/' || x == '*' || x == '^' || x == '!')// * / 的优先级仅仅低于它前面的 * /，高于前面的 + -，所以要将前面的 * / 弹出栈；+ - 保留，因为新的 * / 会放在栈低，优先级高。
              {
                  postexp[count++] = x;
                  Pop(&Optr, &x);
              }
              else
                break;  //其他情况（ + - 左括号 ）退出
            }
            Push(Optr, '*');
            break;
            }

          case TK_NUM: {
            tokens->type = TK_NUM;
            strcpy(tokens->str,tok);  
            //printf("tokens->str = %s\n",tokens->str); 
            for(int k = 0; tok[k] != '\0'; k++ )
            {
              postexp[count++] = tok[k];
            }
            postexp[count++] = '#';
            //printf("postexp = %s\n", postexp);
            memset(tok,0,sizeof tok);
            break;
            }
          //default : printf("error");break;
        }

        break;
      }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      printf("请重新输入表达式！\n");
      return false;
    }
  }

}
    while(!StackEmpty(&Optr))
    {
      Pop(&Optr,&x);
      postexp[count++] = x;
    }
    postexp[count] = '\0';

  for(int n = 0; postexp[n] != '\0'; n ++)
  {
    printf("%c",postexp[n]);
  }

  printf("\n");
  count = 0;

    return true;
}

typedef struct
{
	double data[MAXSIZE_NUM];
	int top;
}Stack_num;

void InitStack_num(Stack_num **s)
{
	*s = (Stack_num *)malloc(sizeof(Stack_num));
	(*s)->top = -1;
}

bool Push_num(Stack_num **s, double e)
{
	if ((*s)->top == MAXSIZE_NUM - 1)
		return false;
	(*s)->top++;
	(*s)->data[(*s)->top] = e;
	return true;
}

bool Pop_num(Stack_num **s, double *e)
{
	if ((*s)->top == -1)
		return false;
	*e = (*s)->data[(*s)->top];
	(*s)->top--;
	return true;
}

bool Pop_num_res(Stack_num **s, word_t *e)
{
	if ((*s)->top == -1)
		return false;
	*e = (*s)->data[(*s)->top];
	(*s)->top--;
	return true;
}

word_t expr(char *e, bool *success) {

  if (!make_token(e)) {
    *success = false;
    return 0;
  }
    Stack_num *num;
    double a, b;
    double c;
    double d;
    word_t result;
    InitStack_num(&num);

    for(int m = 0; postexp[m] != '\0'; m ++)
    {
      switch(postexp[m])
      {
        case '+':
          Pop_num(&num, &a);
          Pop_num(&num, &b);

          c = a + b;
          Push_num(&num, c);
          break;

        case '-':
          Pop_num(&num, &a);
          Pop_num(&num, &b);

          c = b - a;
          Push_num(&num, c);
          break;

        case '*':
          Pop_num(&num, &a);
          Pop_num(&num, &b);

          c = b * a;
          Push_num(&num,c);
          break;

        case '/':
          Pop_num(&num, &a);
          Pop_num(&num, &b);

          if(a != 0)
          {
            c = b / a;
            Push_num(&num, c);  
          }
          else
          {
            printf("error!\n");
            exit(0);
          }
          break;

        case '#':
          break;

        default:
          d = 0;
          if(postexp[m] >= '0' && postexp[m] <= '9')
          {
            d = 10 * d + (postexp[m] - '0');
          }  
          Push_num(&num, d);
          break;
      }
    }
    Pop_num_res(&num, &result);
    *success = true;
  return result;
}
