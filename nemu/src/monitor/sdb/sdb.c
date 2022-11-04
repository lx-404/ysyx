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
#include <utils.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

static int is_batch_mode = false;

void init_regex();
word_t expr(char *e, bool *success);
word_t vaddr_read(vaddr_t addr, int len);
void isa_reg_display();
void init_wp_pool();
void isa_reg_mon(char *reg);
/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");
  /*
  printf("line_read = %s ,*line_read = %d\n", line_read, *line_read);
  line_read 是输出该空间存放的值（某个值的地址），*line_read是输出该空间存放的地址所在的值
  */
  if (line_read && *line_read) {  //类似于缓存上次输入的命令，
    add_history(line_read);
   // printf("line_read_old = %s\n", line_read);

  }
  //printf("line_read_new = %s\n",line_read);
  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_info(char *args) {
  char *arg = strtok(NULL, " ");
  if(*arg == 'r')
    isa_reg_display();
  else if(*arg == 'w')
    {  
      arg = strtok(NULL, " ");
      //printf("%s\n",arg);
      isa_reg_mon(arg);
    }
  else
    printf("error \n");
  return 0;
}

static int cmd_p(char * args) {
  char *arg = strtok(NULL, "\n");
    word_t res;
  /*其实定义的char *arg是两个含义，一个是定义了char类型的arg变量，还有一个是定义了该变量的指针*arg
  strtok返回的是char类型的字串，如果没有匹配就返回NULL，*arg是char类型的arg存放的地方*/
  while(arg != NULL)
{ 
  bool success = false;
  res = expr(arg, &success);
  arg = strtok(NULL, " ");
}
printf("res = %ld\n", res);
  return 0;
}

static int cmd_test(char * args) {
  printf("this is a test fun!\n");
  int a = 2;
  int *p = &a;
  printf("a = %d\n", a);    //a = 2
  printf("&a = %p\n", &a);  //&a = 0x7ffdd273eb2c
  printf("*p = %d\n", *p);  //*p = 2
  printf("p = %p\n", p);    //p = 0x7ffdc42630cc

  printf("&p = %p\n", &p);  //&p = 0x7ffeb61be330


  char *arg = strtok(NULL, " ");
  while(arg != NULL)
  {
    printf("arg == %s\n", arg);
    printf("*arg == %d\n", *arg);
    printf("&arg == %p\n", arg);
    arg = strtok(NULL," ");
  }
  return 0;
}

static int cmd_scan(char *args) {
    char *arg1 = strtok(NULL," ");
    char *arg2 = strtok(NULL," ");
    vaddr_t addr;
    int n;
    sscanf(arg1, "%d", &n);
    sscanf(arg2, "%lx", &addr);
    printf("n = %d\n", n);
    printf("addr = %lx\n",addr);
    for(int i = 0; i < n; i ++)
    {
      printf(" x = 0x%08lx\n",vaddr_read(addr,4));
      addr += 4;
    }
    return 0;
}

static int cmd_si (char *args) {
      char *arg = strtok(NULL, " ");
      // printf("arg = %s\n", arg);
      if(arg == NULL)
        cpu_exec(1);
      else {
        int temp = *arg - '0';
        cpu_exec(temp);
      }
    return 0;
}

static int cmd_q(char *args) {
   nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  {"si", "单步执行", cmd_si},
  {"info", "查看寄存器状态",cmd_info},
  {"scan", "扫描内存",cmd_scan},
  {"p","正则表达式求解",cmd_p},
  {"test","用于自己测试",cmd_test}
  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }
  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);
    /* extract the first token as the command */
    char *cmd = strtok(str, " ");  //打印命令
    //printf("cmd = %s\n",cmd);
    if (cmd == NULL) { continue; }
    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        /*这里handler是用于匹配成功，他会返回0*/
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
