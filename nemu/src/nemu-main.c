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

#include <common.h>

void init_monitor(int, char *[]);
void am_init_monitor();
void engine_start();
int is_exit_status_bad();

/*
int main(int argc, char* argv[]) { */
  /* Initialize the monitor. */
/*
#ifdef CONFIG_TARGET_AM
  am_init_monitor();
#else
  init_monitor(argc, argv);
#endif
*/
  /* Start engine. */
/*	
  engine_start();

  return is_exit_status_bad();
}
*/

int main(int argc, char *argv[]) {
	static char* dir = "/home/chiweiming/code/ics2023/nemu/tools/gen-expr/build/input";
	FILE* f = fopen(dir, "r");
	while (1) {
		char buf[65536] = {};
		if (!fgets(buf, 65535, f)) {
			break;	
		}
		char* buf_end = buf + strlen(buf);
		char* result = strtok(buf, " ");
		if (result == NULL) { continue; }

		char* expression = result + strlen(result) + 1;

		if (expression >= buf_end) {
			continue;			
		}
		printf("%s\n", expression);	
		printf("%s\n", result);
	}
	
	return 0;
}
