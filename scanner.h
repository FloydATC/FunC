#ifndef clox_scanner_h
#define clox_scanner_h

// The number and order of tokens *MUST* match
// ParseRule rules[] exactly or weird things will happen!
// (see compiler.c)
typedef enum {
  // Single-character tokens.
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET, TOKEN_COMMA, TOKEN_DOT,
  TOKEN_MINUS, TOKEN_PLUS, TOKEN_SEMICOLON, TOKEN_QUESTION,
  TOKEN_COLON, TOKEN_HASH, TOKEN_PERCENT, TOKEN_PERCENT_EQUAL,

  // One or two character tokens.
  TOKEN_TILDE, TOKEN_TILDE_EQUAL,
  TOKEN_AMP, TOKEN_AMP_EQUAL,
  TOKEN_CARET, TOKEN_CARET_EQUAL,
  TOKEN_PIPE, TOKEN_PIPE_EQUAL,
  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL, TOKEN_GREATER_GREATER,
  TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_LESS_LESS,
  TOKEN_PLUS_PLUS, TOKEN_PLUS_EQUAL,
  TOKEN_MINUS_MINUS, TOKEN_MINUS_EQUAL,
  TOKEN_STAR, TOKEN_STAR_EQUAL,
  TOKEN_SLASH, TOKEN_SLASH_EQUAL,

  // Literals.
  TOKEN_IDENTIFIER, TOKEN_STRING,
  TOKEN_BASE2_NUMBER, TOKEN_BASE8_NUMBER, TOKEN_BASE10_NUMBER, TOKEN_BASE16_NUMBER,

  // Keywords.
  TOKEN_AND, TOKEN_BREAK, TOKEN_CASE, TOKEN_CLASS, TOKEN_CONTINUE,
  TOKEN_DEFAULT, TOKEN_ELSE, TOKEN_FALSE,
  TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NULL, TOKEN_OR,
  TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_SWITCH, TOKEN_THIS,
  TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

  // Internal.
  TOKEN_ERROR,
  TOKEN_EOF
} TokenType;

typedef struct {
  const char* start;
  const char* current;
  int lineno;
  int charno;
  int fileno;
  void* vm;
  struct Scanner* parent;
} Scanner;

typedef struct {
  TokenType type;
  const char* start;
  int length;
  int fileno;
  int lineno;
  int charno;
} Token;

Scanner* initScanner(void* vm, int fileno, const char* source);
Token scanToken();

#endif

