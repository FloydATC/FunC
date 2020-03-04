#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

#include "error.h"
#include "file.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"


// "Constructor"
Parser* initParser(void* vm, int fileno, const char* source) {
#ifdef DEBUG_TRACE_PARSER
  printf("parser:initParser() vm=%p scanner=%p\n", vm, scanner);
#endif
  Parser* parser = ALLOCATE(vm, Parser, 1);

  parser->scanner = initScanner(vm, fileno, source);
#ifdef DEBUG_TRACE_PARSER
  printf("parser:initParser() initialized scanner=%p\n", parser->scanner);
#endif

  parser->hadError = false;
  parser->panicMode = false;

#ifdef DEBUG_TRACE_PARSER
  printf("parser:initParser() initialized parser %p\n", parser);
#endif
  return parser;
}


// "Destructor"
void destroyParser(void* vm, Parser* parser) {
#ifdef DEBUG_TRACE_PARSER
  printf("parser:destroyParser() vm=%p parser=%p\n", vm, parser);
#endif
  destroyScanner(vm, parser->scanner);
  FREE(vm, Parser, parser);
#ifdef DEBUG_TRACE_PARSER
  printf("parser:destroyParser() done\n");
#endif
}




static void errorAt(Parser* parser, Token* token, const char* message) {
  if (parser->panicMode) return;
  parser->panicMode = true;
  char* msgbuf = NULL;

  char* filename = AS_CSTRING(getFilenameByNo(parser->vm, token->fileno));
  msgbuf = bprintf(msgbuf, "%s[%d:%d] Error", filename, token->lineno, token->charno);

  if (token->type == TOKEN_EOF) {
    msgbuf = bprintf(msgbuf, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    msgbuf = bprintf(msgbuf, " at '%.*s'", token->length, token->start);
  }

  msgbuf = bprintf(msgbuf, ": %s\n", message);

  if (((VM*)parser->vm)->error_callback != NULL) { ((VM*)parser->vm)->error_callback(msgbuf); }
  printf("%s", msgbuf);

  free(msgbuf);
  parser->hadError = true;
}

void error(Parser* parser, const char* message) {
  errorAt(parser, &parser->previous, message);
}

void errorAtCurrent(Parser* parser, const char* message) {
  errorAt(parser, &parser->current, message);
}



void advance(Parser* parser) {
#ifdef DEBUG_TRACE_COMPILER
  printf("parser:advance() parser=%p\n", parser);
#endif
  parser->previous = parser->current;

  for (;;) {
    parser->current = scanToken(parser->scanner); // scanner
    if (parser->current.type != TOKEN_ERROR) break;
    errorAtCurrent(parser, parser->current.start);
  }
}

void consume(Parser* parser, TokenType type, const char* message) {
#ifdef DEBUG_TRACE_COMPILER
  printf("parser:consume() parser=%p\n", parser);
#endif
  if (parser->current.type == type) {
    advance(parser);
    return;
  }

  errorAtCurrent(parser, message);
}

// IF type matches THEN return true ELSE return false
bool check(Parser* parser, TokenType type) {
  return (parser->current.type == type);
}

// IF type matches THEN advance and return true ELSE return false
bool match(Parser* parser, TokenType type) {
  if (!check(parser, type)) return false;
  advance(parser);
  return true;
}


