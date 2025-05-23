/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdbool.h>
#include "sdb.h"
#include <memory/vaddr.h>
#include "../../../include/common.h"
#include <regex.h>

#define TRUE 1
#define false 0

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;
  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args);

static int cmd_p(char *args);

static int cmd_info(char *args);

static int cmd_x(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si","Let the program execute N instructions in a single step and then suspend execution",cmd_si},
  { "info", "Display the info of registers & watchpoints", cmd_info },
  { "x", "Usage: x N EXPR. Scan the memory from EXPR by N bytes", cmd_x },
  { "p", "Calculate the value of the expression EXPR", cmd_p},
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
    char *cmd = strtok(str, " ");
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

static int cmd_info(char *args){
  char *arg = strtok(NULL," ");
  if(arg == NULL){
    printf("Use info r or info w \n" );
      return 0;
  }else if (!strcmp(arg,"r")){
      isa_reg_display();
  }else if (!strcmp(arg,"w")){
    //todo
  }else
    printf("Use info r or info w \n" );
  return 0;
}

static int cmd_x(char *args){
	char *arg_1 = strtok(NULL," ");
	char *arg_2 = strtok(NULL," ");
	
	int addr, N;
	regex_t reg;
	regmatch_t pmatch[1];
	if(regcomp(&reg, "[0x][0-9a-fA-F]+", REG_EXTENDED)){
		printf("Regex Compile Error!");
		return 0;
	};
	
	if(arg_1==NULL || arg_2 == NULL){
		printf("Usage: x <n/r/f> <addr>");
		return 0;
	}else if (!regexec(&reg, arg_2, 1, pmatch, 0)){
		addr = strtol(arg_2,NULL,16);
		N = strtol(arg_1,NULL,10);
		printf("addr = %x\n",addr+4*N);
		printf("value = %lu\n",vaddr_read(addr+4*N,4));
	}else{
		printf("Address must be Hex!");
	}
}

static int cmd_si(char *args){
    int N = 1;
    char* arg0 = strtok(NULL," ");
    char* arg1 = strtok(NULL," ");
    if (arg1!=NULL){
        printf("Too much args!\n");
    }else if (arg0==NULL){
      cpu_exec(N);
    }else {
        sscanf(arg0,"%d",&N);
        if (N<0||N>9){
          printf("please enter number from 1~9. default:1\n");
          return 0;
        }
        cpu_exec(N); 
    }
    return 0;
}

static int cmd_p(char *args) {
    if (args == NULL) { 
        printf("error: no expression given\n");
        return 0;
    } 
    bool success = true;
    word_t value = expr(args, &success);
    if (!success) {
        printf("error: wrong expression %s\n", args);
        return 0;
    } 
    printf("%lu\n", value);
    return 0;
}
