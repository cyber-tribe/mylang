#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* 入力プログラム */
char *user_input;

/* トークンの種類 */
typedef enum{
  TK_RESERVED,  /* 記号 */
  TK_NUM,  /* 整数トークン */
  TK_EOF,  /* 入力の終わりを表すトークン */
} TokenKind;

typedef struct Token Token;

/* トークン型 */
struct Token
{
  TokenKind kind;  /* トークンの型 */
  Token *next;  /* 次の入力トークン */
  int val;  /* kindがTK_NUMの場合の値 */
  char *str;  /* トークン文字列 */
  int len;  /* トークンの長さ */
};

/* 現在注目しているトークン */
Token *token;

/* 抽象構文木のノードの種類 */
typedef enum{
  ND_ADD,  /* + */
  ND_SUB,  /* - */
  ND_MUL,  /* * */
  ND_DIV,  /* / */
  ND_EQ,  /* == */
  ND_NE,  /* != */
  ND_LT,  /* < */
  ND_LE,  /* <= */
  ND_NUM,  /* 整数 */
} NodeKind;

typedef struct Node Node;

/* 抽象構文木のノードの型 */
struct Node{
  NodeKind kind;  /* ノードの型 */
  Node *lhs;  /* 左の子 */
  Node *rhs;  /* 右の子 */
  int val;  /* kindがND_NUMの場合のみ使う */
};

/*関数の宣言*/

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
bool consume(char *op);
void expect(char *op);
int expect_number(void);
bool at_eof(void);
Token *new_token(TokenKind kind, Token *cur, char *str, int len);
bool startswith(char *p, char *q);
Token *tokenize();
Node *new_node(NodeKind kind);
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs);
Node *new_node_num(int val);
Node *expr();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();
void gen(Node *node);

/************/


int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "引数の個数が正しくありません\n");
    return 1;
  }
  /* error用 */
  user_input = argv[1];
  /* トークナイズする. */
  token = tokenize();
  Node *node = expr();

  /* アセンブリの前半部分を出力する. */
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  /* 抽象構文木を下りながらコードを生成する. */
  gen(node);

  /* スタックトップに式全体の値が残っているはずなので */
  /* それをRAXにロードして関数からの返り値とする. */
  printf("  pop rax\n");
  printf("  ret\n");
  return 0;
}

/* 関数の定義 */

/* エラーを報告するための関数 */
/* printfと同じ引数を取る */
void error(char *fmt, ...){
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

/* エラー箇所を報告する. */
void error_at(char *loc, char *fmt, ...){
  va_list ap;
  va_start(ap ,fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, "");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

/* 次のトークンが期待している記号のときには, トークンを１つ読み進めて */
/* tureを返す. それ以外の場合にはfalseを返す. */
bool consume(char *op){
  if(token->kind != TK_RESERVED ||
    token->len != strlen(op) ||
    memcmp(token->str, op, token->len)){
    
    return false;
  
  }
  token = token->next;
  return true;
}

/* 次のトークンが期待している記号のときには, トークンを１つ読み進める. */
/* それ以外の場合にはエラーを報告する. */
void expect(char *op){
  if(token->kind != TK_RESERVED ||
    token->len != strlen(op) ||
    memcmp(token->str, op, token->len)){

    error_at(token->str,"'%c'ではありません", op);

  }
  token = token->next;
}

/* 次のトークンが数値の場合, トークンを１つ読み進めてその数値を返す. */
/* それ以外の場合にはエラーを報告する. */
int expect_number(void){
  if(token->kind != TK_NUM){
    error_at(token->str, "数ではありません");
  }
  int val = token->val;
  token = token->next;
  return val;
}

/* 次のトークンがEOFかどうか調べる. */
bool at_eof(void){
  return token->kind == TK_EOF;
}

/* 新しいトークンを作成してcurにつなげる. */
Token *new_token(TokenKind kind, Token *cur, char *str, int len){
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

bool startswith(char *p, char *q){
  return memcmp(p, q, strlen(q)) == 0;
}

/* 入力文字列pをトークナイズしてそれを返す. */
Token *tokenize(){
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while(*p){
    /* 空白文字を飛ばす. */
    if(isspace(*p)){
      p++;
      continue;
    }

    /* 複数文字の場合 */
    if(startswith(p, "==") || startswith(p, "!=") ||
      startswith(p, "<=") || startswith(p, ">=")){
        cur = new_token(TK_RESERVED, cur, p, 2);
        p += 2;
        continue;
      }
    /* １文字の場合 */
    if(strchr("+-*/()<>", *p)){
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }
    /* 数字の場合 */
    if(isdigit(*p)){
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    error_at(p,"未知のトークンです.");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}



/* 二項演算子の場合 */
Node *new_node(NodeKind kind){
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs){
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

/* 数値の場合 */
Node *new_node_num(int val){
  Node *node = calloc(1,sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

Node *expr(){
  return equality();
}

Node *equality(){
  Node *node = relational();

  for(;;){
    if(consume("==")){
      node = new_binary(ND_EQ, node, relational());
    }else if(consume("!=")){
      node = new_binary(ND_NE, node, relational());
    }else{
      return node;
    }
  }
}

Node *relational(){
  Node *node = add();

  for(;;){
    if(consume("<")){
      node = new_binary(ND_LT, node, add());
    }else if(consume("<=")){
      node = new_binary(ND_LE, node, add());
    }else if(consume(">")){
      node = new_binary(ND_LT, add(), node);
    }else if(consume(">=")){
      node = new_binary(ND_LE, add(), node);
    }else{
      return node;
    }
  }
}

Node *add(){
  Node *node = mul();

  for(;;){
    if(consume("+")){
      node = new_binary(ND_ADD, node, mul());
    }else if(consume("-")){
      node = new_binary(ND_SUB, node, mul());
    }else{
      return node;
    }
  }
}

Node *mul(){
  Node *node = unary();

  for(;;){
    if(consume("*")){
      node = new_binary(ND_MUL, node, unary());
    }else if(consume("/")){
      node = new_binary(ND_DIV, node, unary());
    }else{
      return node;
    }
  }
}

Node *unary(){
  if(consume("+")){
    return primary();
  }
  if(consume("-")){
    /* x = 0 - x */
    return new_binary(ND_SUB, new_node_num(0), primary());
  }

  return primary();
}

Node *primary(){
  /* 次のトークンが"("なら, "(" expr ")"である. */
  if(consume("(")){
    Node *node = expr();
    expect(")");
    return node;
  }
  /* そうでなければ数値である. */
  return new_node_num(expect_number());
}



/* 仮想スタックマシン */
void gen(Node *node) {
  if (node->kind == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
  case ND_ADD:
    printf("  add rax, rdi\n");
    break;
  case ND_SUB:
    printf("  sub rax, rdi\n");
    break;
  case ND_MUL:
    printf("  imul rax, rdi\n");
    break;
  case ND_DIV:
    printf("  cqo\n");
    printf("  idiv rdi\n");
    break;
  case ND_EQ:
    printf("  cmp rax, rdi\n");
    printf("  sete al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_NE:
    printf("  cmp rax, rdi\n");
    printf("  setne al\n");
    printf(" movzb rax, al\n");
    break;
  case ND_LT:
    printf("  cmp rax, rdi\n");
    printf("  setl al\n");
    printf("  movzb rax, al\n");
    break;
  case ND_LE:
    printf("  cmp rax, rdi\n");
    printf("  setle al\n");
    printf("  movzb rax, al\n");
    break;
  }

  printf("  push rax\n");
}

/* ********** */