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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static int deep;
uint32_t choose(uint32_t n) {
	if (deep <= 0) {	// limit the depth of recursion, does not allow the buf to overflow
		return 0;
	}
	--deep;
	return (uint32_t)random() % n;
}

char gen_rand_op() {
	long op = random() % 4;
	switch(op) {
		case 0: return '+';
		case 1: return '-';
		case 2: return '*';
		case 3: return '/';
		default: return '+';
	}
	return '+';
}

void insert_space() {
	if (random() % 2) {
		strcat(buf, " ");
	}
}

static void gen_rand_expr() {
  // buf[0] = '\0';
	
	char strnum[128] = {};
	char c[2] = {};
	switch(choose(3)) {
		case 0: 
			sprintf(strnum, "%u", (uint32_t)random()); 
			strcat(buf, strnum);
			strcat(buf, "u");	// ensure that expressions perform unsigned operations
			insert_space();
			break;
		case 1:
			strcat(buf, "(");
			insert_space();
			gen_rand_expr();
			insert_space();
			strcat(buf, ")");
			break;
		default:
			gen_rand_expr();
			*c = gen_rand_op();
			insert_space();
			strcat(buf, c);	
			insert_space();
			gen_rand_expr();
	}
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
		deep = 30;
		buf[0] = '\0';
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -Wall -Werror -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
