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

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char* exp;  // expression
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
void info_w() {
  if (!head) {
    printf("No watchpoints\n");
    return;
  }
  printf("Num\tDisp\tEnb\tAddress\t\t\tWhat\n");
  WP* cursor = head;
  while (cursor) {
    printf("%d\t-\t-\t-\t\t\t%s\n", cursor -> NO, cursor -> exp);
    cursor = cursor -> next;
  }
}
static WP* new_wp(bool* success) {
  WP* ptr = NULL;
  if (free_) {
    ptr = free_;
    free_ = free_ -> next; 
    ptr -> next = NULL;
    *success = true;
    return ptr;
  }
  *success = false;
  return ptr;
}

static void free_wp(WP* wp) {
  if (!free_) {
    free_ = wp;
    wp -> next = NULL;
    return;
  }

  WP* cursor = free_;
  while (cursor -> next != NULL) {
    cursor = cursor -> next;
  }
  cursor -> next = wp;
  wp -> next = NULL;
  free(wp -> exp);
}

static int wp_count = 0;
void add_wp(char* exp, bool* success) {
  WP* new_wp_ = new_wp(success);
  if (*success == false) {
    Log("the number of watch points has exceeded the limit");
    return;
  }
  new_wp_ -> NO = ++wp_count;
  new_wp_ -> exp = (char*)malloc(strlen(exp) + 1);
  strcpy(new_wp_ -> exp, exp);
  
  if (!head) {
    head = new_wp_;
    *success = true;
    return;
  }
  WP* cursor = head;
  while (cursor -> next != NULL) {
    cursor = cursor -> next;
  }
  cursor -> next = new_wp_;
  *success = true;
}

void del_wp(int NO, bool* success) {
  if (NO < 0 || !head) {
    *success = false;
    Log("no watch points or the NO of watch point incorrect");
    return;
  }
  WP* cursor = head;
  if (cursor -> NO == NO) {
    *success = true;
    head = head -> next;
    free_wp(cursor);
    return;
  }
  for (; cursor -> next; cursor = cursor -> next) {
    if (cursor -> next -> NO == NO) {
      WP* tmp = cursor -> next;
      cursor -> next = cursor -> next -> next;      
      free_wp(tmp);
      *success = true;
      return;
    }
  }
  *success = false;
}
