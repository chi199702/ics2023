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
  TK_HEXADECIMAL, TK_REGISTER,
  TK_UNEQ, TK_AND,
  TK_DEREF,
  TK_NEGATIVE,
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
  {"\\+", '+'},         // plus
  {"-", '-'},						// - sub
  {"==", TK_EQ},        // equal
  {"!=", TK_UNEQ},
  {"&&", TK_AND},
  {" +", TK_NOTYPE},    // spaces
  {"(0x|0X)([0-9]|[A-F]|[a-f])+", TK_HEXADECIMAL},
  {"[0-9]+u?", TK_DECIMAL},	// - decimal numberi,can't use $
  {"\\$\\w\\w?\\w?", TK_REGISTER},
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
          case TK_DEREF:
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
          case TK_NEGATIVE:
            tokens[nr_token].type = '-';
            tokens[nr_token++].str[0] = '-';
            break;
          case '+':
            tokens[nr_token].type = '+';
            tokens[nr_token++].str[0] = '+';
            break;
          case TK_DECIMAL:
            tokens[nr_token].type = TK_DECIMAL;
            strncpy(tokens[nr_token++].str, substr_start, substr_len);
            break;
          case TK_HEXADECIMAL:
            tokens[nr_token].type = TK_HEXADECIMAL;
            strncpy(tokens[nr_token++].str, substr_start, substr_len);
            break;
          case TK_REGISTER:
            tokens[nr_token].type = TK_REGISTER;
            strncpy(tokens[nr_token++].str, substr_start, substr_len);
            break;
          case TK_EQ:
            tokens[nr_token].type = TK_EQ;
            strncpy(tokens[nr_token++].str, substr_start, substr_len);
            break;
          case TK_UNEQ:
            tokens[nr_token].type = TK_UNEQ;
            strncpy(tokens[nr_token++].str, substr_start, substr_len);
            break;
          case TK_AND:
            tokens[nr_token].type = TK_AND;
            strncpy(tokens[nr_token++].str, substr_start, substr_len);
            break;
          default:
            Log("occur error"); 
            exit(EXIT_FAILURE);
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
static bool bracket_match(int p, int q) {
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
static bool check_parentheses(int p, int q) {
  if (tokens[p].str[0] != '(' || tokens[q].str[0] != ')') {
    return false;
  }
  return bracket_match(p + 1, q - 1);
}

static int get_priority(int operator) {
  switch(operator) {
    case TK_NEGATIVE: return 5;
    case TK_DEREF: return 4;
    case '*':
    case '/': return 3;
    case '+':
    case '-': return 2;
    case TK_EQ:
    case TK_UNEQ: return 1;
    case TK_AND:  return 0;
    default: Log("error operator");
  }
  return -1;
}

#define INIT -1

/* find the main operator  */
bool get_main_operator(int p, int q, int* idx_main_operator, int* main_operator) {
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
      return false;	// bad expression
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
  if (*idx_main_operator == -1) {
    return false;
  }
  return true;
}

bool occur_error = false;

#define BAD_EXPRESSION -1
static long eval(int p, int q) {
  if (p > q) {
    Log("bad expression, p > q");
    occur_error = true;
    return BAD_EXPRESSION;
  }else if (p == q) {
    if (tokens[p].type == TK_REGISTER) {
      bool success;
      word_t regval = isa_reg_str2val(tokens[p].str, &success);
      if (success) {
        return regval;
      }
      occur_error = true;
      Log("please check the register name: %s", tokens[p].str);
      return BAD_EXPRESSION;
    }
    if (tokens[p].type != TK_HEXADECIMAL || tokens[p].type != TK_DECIMAL) {
      occur_error = true;
      Log("No digits were found or the string is a alphanumeric : \" %s \"", tokens[p].str);
      return BAD_EXPRESSION;
    }
    int base = 10;
    char* endptr;
    long val;
    errno = 0;

    if (tokens[p].type == TK_HEXADECIMAL) {
      base = 16;
    }
    val = strtol(tokens[p].str, &endptr, base);
    if (errno != 0) {
      Log("error occur on strtol while parse the expression");
      occur_error = true;
      return BAD_EXPRESSION;
    }

    if (endptr == tokens[p].str || *endptr != '\0') {
      if (strcmp(endptr, "u")) {
        Log("No digits were found or the string is a alphanumeric : \" %s \"", tokens[p].str);
        occur_error = true;
        return BAD_EXPRESSION;
      }
    }
    return val;
  }else if (check_parentheses(p, q)) {
    return eval(p + 1, q - 1);
  }else { // bad or good expression, further judgement required
    int idx_main_operator = INIT;
    int main_operator;
    bool flag = get_main_operator(p, q, &idx_main_operator, &main_operator);
    if (occur_error || !flag) {
      occur_error = true;
      return BAD_EXPRESSION;	// bad expression
    }

    uint32_t left_result = 1, right_result;
    if (tokens[idx_main_operator].type != TK_DEREF && tokens[idx_main_operator].type != TK_NEGATIVE) {
      left_result = eval(p, idx_main_operator - 1);
      right_result = eval(idx_main_operator + 1, q);
    }else {
      right_result = eval(idx_main_operator + 1, q);
    } 
    if (occur_error) {
      occur_error = true;
      return BAD_EXPRESSION;
    }

    switch(main_operator) {
      case '+': return left_result + right_result;	
      case '-': return left_result - right_result;	
      case '*': return left_result * right_result;	
      case '/': return left_result / right_result;	
      case TK_EQ: return left_result == right_result ? 1 : 0;
      case TK_UNEQ: return left_result != right_result ? 1 : 0;
      case TK_AND: return left_result && right_result;
      case TK_DEREF: 
        word_t paddr_read(paddr_t addr, int len);
        return paddr_read(right_result, 4); 
      case TK_NEGATIVE: return (-1 * right_result); 
      default:	occur_error = true; return BAD_EXPRESSION;
    }

    return BAD_EXPRESSION;
  }
}

bool pre_signal(int i) {
  int type = tokens[i].type;
  if (type == TK_DECIMAL || type == TK_HEXADECIMAL || type == ')') {
    return false;
  }
  return true;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  for (int i = 0; i < nr_token; ++i) {  // find the operator deref '*'
    if (tokens[i].type == '*' && (i == 0 || pre_signal(i - 1)) ) {
      tokens[i].type = TK_DEREF;
    } 
  }
  for (int i = 0; i < nr_token; ++i) {  // find the operator negative '-'
    if (tokens[i].type == '-' && (i == 0 || pre_signal(i - 1)) ) {
      tokens[i].type = TK_NEGATIVE;
    } 
  }
  long compute_res = eval(0, nr_token - 1);	
  if (occur_error || compute_res < 0) {
    Log("invaild expression! please input again!");			
    *success = false;
    return 0;
  }

  *success = true;
  return compute_res;
}
