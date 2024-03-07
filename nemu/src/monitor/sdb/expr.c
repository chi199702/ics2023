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

enum {
  TK_NOTYPE = 256, TK_EQ,

  /* TODO: Add more token types */
  TK_DECIMAL, TK_NEWLINE,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {"\\(", '('},						// - left bracket
  {"\\)", ')'},						// - right bracket
  {"\\*", '*'},					// - mul
  {"/", '/'},						// - div
  {" +", TK_NOTYPE},    // spaces
  {"-", '-'},						// - sub
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  {"[0-9]+u", TK_DECIMAL},	// - decimal numberi,can't use $
  {"\\\n", TK_NEWLINE},   // - '\n'
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[512] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case '(':
            tokens[nr_token].type = '(';
            tokens[nr_token++].str[0] = '(';
            break;
          case ')':
            tokens[nr_token].type = ')';
            tokens[nr_token++].str[0] = ')';
            break;
          case '*':
            tokens[nr_token].type = '*';
            tokens[nr_token++].str[0] = '*';
            break;
          case '/':
            tokens[nr_token].type = '/';
            tokens[nr_token++].str[0] = '/';
            break;
          case TK_NOTYPE:	// throw out
          case TK_NEWLINE:
            break;
          case '-':
            tokens[nr_token].type = '-';
            tokens[nr_token++].str[0] = '-';
            break;
          case '+':
            tokens[nr_token].type = '+';
            tokens[nr_token++].str[0] = '+';
            break;
          case TK_DECIMAL:
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token++].str[substr_len] = '\0';
            break;
          default:
            Log("regular expression \"==\" wait for development"); 
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

/* check if all the parentheses of the expression match */
static bool bracket_match(uint32_t p, uint32_t q) {
  /* collect all the parentheses in [p + 1, q - 1] of array tokens */
  char brackets[128];
  uint32_t len = 0;
  for (int i = p; i <= q; ++i) {
    if (tokens[i].str[0] == '(') {
      brackets[len++] = '(';	
    }else if (tokens[i].str[0] == ')') {
      brackets[len++] = ')';
    }
  }

  if (!len) {
    return true;
  }
  if (len % 2) {
    return false;
  }

  int stk[len];
  int top = 0;
  for (int i = 0; i < len; ++i) {
    if (brackets[i] == '(') {
      stk[top++] = '(';
    }else {
      if (top == 0 || stk[top - 1] != '(') {
        return false;
      }
      --top;
    }
  }
  return top == 0 ? true : false;
}

/* check that the expression is not wrapped in the outermost pair of parentheses  */
static bool check_parentheses(uint32_t p, uint32_t q) {
  if (tokens[p].str[0] != '(' || tokens[q].str[0] != ')') {
    return false;
  }
  return bracket_match(p + 1, q - 1);
}

#define LOW_PRIORITY  0
#define HIGH_PRIORITY 1
static int get_priority(int operator) {
  if (operator == '+' || operator == '-') {
    return LOW_PRIORITY;
  }
  return HIGH_PRIORITY;
}

#define BAD_EXPRESSION -1
/* find the main operator  */
int get_main_operator(uint32_t p, uint32_t q, int* idx_main_operator, char* main_operator) {
  int left_bracket = 0, right_bracket = 0;
  for (int i = p; i <= q; ++i) {
    int type = tokens[i].type;
    if (type == TK_NOTYPE || type == TK_DECIMAL || type == TK_EQ || type == TK_NEWLINE) {
      continue;
    }
    if (type == '(') {
      ++left_bracket;
      continue;
    }else if (type == ')') {
      ++right_bracket;
      continue;
    }
    if (left_bracket < right_bracket) {
      return BAD_EXPRESSION;	// bad expression
    }
    /* '+' '-' '*' '/' */
    if (left_bracket > right_bracket) {	// the operator is in parentheses
      continue;
    }
    if (*idx_main_operator == -1) {
      *idx_main_operator = i;
      *main_operator = tokens[i].str[0];
    }else {
      int pre_priority = get_priority(*main_operator);
      int now_priority = get_priority(tokens[i].str[0]);
      if (pre_priority >= now_priority) {
        *idx_main_operator = i;
        *main_operator = tokens[i].str[0];
      }
    }	
  }
  return 0;
}

#define INIT -1
static long eval(uint32_t p, uint32_t q) {
  if (p > q) {
    Log("bad expression, p > q");
    return BAD_EXPRESSION;
  }else if (p == q) {
    int base = 10;
    char* endptr;
    long val;

    errno = 0;
    val = strtol(tokens[p].str, &endptr, base);
    if (errno != 0) {
      Log("error occur on strtol while parse the expression");
      return BAD_EXPRESSION;
    }

    if (endptr == tokens[p].str || *endptr != '\0') {
      if (strcmp(endptr, "u")) {
        Log("No digits were found or the string is a alphanumeric : \" %s \"", tokens[p].str);
        return BAD_EXPRESSION;
      }
    }
    return val;
  }else if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1);
  }else { // bad or good expression, further judgement required
    int idx_main_operator = INIT;
    char main_operator;
    if (get_main_operator(p, q, &idx_main_operator, &main_operator) == BAD_EXPRESSION || idx_main_operator == INIT) {
      return BAD_EXPRESSION;	// bad expression
    }

    uint32_t left_result = (uint32_t)eval(p, idx_main_operator - 1);
    uint32_t right_result = (uint32_t)eval(idx_main_operator + 1, q);
    if (left_result == BAD_EXPRESSION || right_result == BAD_EXPRESSION) {
      return BAD_EXPRESSION;
    }

    switch(main_operator) {
      case '+': return left_result + right_result;	
      case '-': return left_result - right_result;	
      case '*': return left_result * right_result;	
      case '/': return left_result / right_result;	
      default:	return BAD_EXPRESSION;
    }

    return BAD_EXPRESSION;
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  long compute_res = eval(0, nr_token - 1);	
  if (compute_res == BAD_EXPRESSION || compute_res < 0) {
    Log("invaild expression! please input again!");			
    *success = false;
    return 0;
  }

  *success = true;
  return compute_res;
}
