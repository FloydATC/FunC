
#include "common.h"
#include "scanner.h"

typedef struct Parser {
  bool hadError;
  bool panicMode;
  Token current;
  Token previous;
  Scanner* scanner;
  void* vm;
} Parser;


Parser* initParser(void* vm, int fileno, const char* source);
void destroyParser(void* vm, Parser* parser);

void error(Parser* parser, const char* message);
void errorAtCurrent(Parser* parser, const char* message);

void advance(Parser* parser);
void consume(Parser* parser, TokenType type, const char* message);
bool check(Parser* parser, TokenType type);
bool match(Parser* parser, TokenType type);
