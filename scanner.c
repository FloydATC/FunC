#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
  const char* start;
  const char* current;
  int fileno;
  int lineno;
  int charno;
} Scanner;

Scanner scanner;

void initScanner(int fileno, const char* source) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:initScanner(%d, %p)\n", fileno, source);
#endif
  scanner.start = source;
  scanner.current = source;
  scanner.fileno = fileno;
  scanner.lineno = 1;
  scanner.charno = 1;
}

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
          c == '_';
}

static bool isBase2Digit(char c) {
  return c == '0' || c == '1';
}

static bool isBase8Digit(char c) {
  return c >= '0' && c <= '7';
}

static bool isBase10Digit(char c) {
  return c >= '0' && c <= '9';
}

static bool isBase16Digit(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

static bool isAtEnd() {
  return *scanner.current == '\0';
}

static char advance() {
  scanner.current++;
  scanner.charno++;
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:advance() lineno=%d, charno=%d\n", scanner.lineno, scanner.charno);
#endif // DEBUG_TRACE_SCANNER
  return scanner.current[-1];
}

static char peek() {
  return *scanner.current;
}

static char peekNext() {
  if (isAtEnd()) return '\0';
  return scanner.current[1];
}

static bool match(char expected) {
  if (isAtEnd()) return false;
  if (*scanner.current != expected) return false;

  scanner.current++;
  return true;
}


static Token makeToken(TokenType type) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:makeToken(%d)\n", type);
#endif // DEBUG_TRACE_SCANNER
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.fileno = scanner.fileno;
  token.lineno = scanner.lineno;
  token.charno = scanner.charno;

  return token;
}

static Token errorToken(const char* message) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:errorToken(%s)\n", message);
#endif // DEBUG_TRACE_SCANNER
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.fileno = scanner.fileno;
  token.lineno = scanner.lineno;
  token.charno = scanner.charno;

  return token;
}

static void skipWhitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        scanner.lineno++;
        scanner.charno = 1;
        advance();
        break;
      case '#': {
        // A directive goes intil the end of the line and has already been processed by the preprocessor
        // It should be treated like a comment from here on, i.e. completely ignored
        while (peek() != '\n' && !isAtEnd()) {
          advance();
        }
        break;
      }
      case '/':
        if (peekNext() == '/') {
          // A comment goes until the end of the line.
          while (peek() != '\n' && !isAtEnd()) advance();
        } else if (peekNext() == '*') {
          // A block comment goes until '*/'
          advance(); // Consume the slash
          advance(); // Consume the star
          // Scan for */ while ignoring everything else
          while ((peek() != '*' || peekNext() != '/') && !isAtEnd()) {
            if (peek() == '\n') { scanner.lineno++; scanner.charno = 1; }
            advance();
          }
          advance(); // Consume the star
          advance(); // Consume the slash
        } else {
          return; // This slash is not the beginning of a comment.
        }
        break;
      default:
        return;
    }
  }
}

static TokenType checkKeyword(int start, int length,
    const char* rest, TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType()
{
  switch (scanner.start[0]) {
    case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
    case 'b': return checkKeyword(1, 4, "reak", TOKEN_BREAK);
    case 'c':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': return checkKeyword(2, 2, "se", TOKEN_CASE);
          case 'l': return checkKeyword(2, 3, "ass", TOKEN_CLASS);
          case 'o': return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
        }
      }
      break;
    case 'd':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'e':
            if (scanner.current - scanner.start > 2) {
              switch (scanner.start[2]) {
                case 'b': return checkKeyword(3, 2, "ug", TOKEN_PRINT);
                case 'f': return checkKeyword(3, 4, "ault", TOKEN_DEFAULT);
              }
            }
        }
      }
    case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
          case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
          case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
        }
      }
      break;
    case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
    case 'n': return checkKeyword(1, 3, "ull", TOKEN_NULL);
    case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
    //case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
    case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
    case 's':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'u': return checkKeyword(2, 3, "per", TOKEN_SUPER);
          case 'w': return checkKeyword(2, 4, "itch", TOKEN_SWITCH);
        }
      }
    case 't':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
          case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
        }
      }
      break;
    case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
    case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier() {
  while (isAlpha(peek()) || isBase10Digit(peek())) advance();

  return makeToken(identifierType());
}

static Token number(char current) {
  //printf("number() peek=%c next=%c\n", peek(), peekNext());
  if (current== '0' && peek() == 'b') {
    //printf("number() binary notation\n");
    advance(); // Consume the 'b'
    // Binary
    // ...
    while (isBase2Digit(peek())) advance();
    return makeToken(TOKEN_BASE2_NUMBER);
  }
  if (current== '0' && isBase8Digit(peek())) {
    //printf("number() octal notation\n");
    // Octal
    // ...
    while (isBase8Digit(peek())) advance();
    return makeToken(TOKEN_BASE8_NUMBER);
  }
  if (current== '0' && peek() == 'x') {
    //printf("number() hexadecimal notation\n");
    advance(); // Consume the 'x'
    // Hexadecimal
    // ...
    while (isBase16Digit(peek())) advance();
    return makeToken(TOKEN_BASE16_NUMBER);
  }
  //printf("number() decimal notation\n");

  while (isBase10Digit(peek())) advance();

  // Look for a fractional part.
  if (peek() == '.' && isBase10Digit(peekNext())) {
    // Consume the ".".
    advance();

    while (isBase10Digit(peek())) advance();
  }

  return makeToken(TOKEN_BASE10_NUMBER);
}

static Token string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') { scanner.lineno++; scanner.charno = 1; }
    if (peek() == '\\' && peekNext() == '\\') {
      // Double backslashes will be interpreted as a single backslash later
      // Skip them so they don't interfere with the next test
      advance();
      advance();
      continue;
    }
    if (peek() == '\\' && peekNext() == '"') {
      // Escaped double quotes must not terminate the string
      advance();
      advance();
      continue;
    }
    // All other escape sequences should now pass through without causing problems
    advance();
  }

  if (isAtEnd()) return errorToken("Unterminated string.");

  // The closing quote.
  advance();
  return makeToken(TOKEN_STRING);
}

Token scanToken() {
  skipWhitespace();
  scanner.start = scanner.current;

  if (isAtEnd()) return makeToken(TOKEN_EOF);

  char c = advance();

  if (isAlpha(c)) return identifier();
  if (isBase10Digit(c)) return number(c); // All radii start with a base 10 digit

  switch (c) {
    //case '#': return makeToken(TOKEN_HASH);
    case '(': return makeToken(TOKEN_LEFT_PAREN);
    case ')': return makeToken(TOKEN_RIGHT_PAREN);
    case '{': return makeToken(TOKEN_LEFT_BRACE);
    case '}': return makeToken(TOKEN_RIGHT_BRACE);
    case '[': return makeToken(TOKEN_LEFT_BRACKET);
    case ']': return makeToken(TOKEN_RIGHT_BRACKET);
    case ';': return makeToken(TOKEN_SEMICOLON);
    case ',': return makeToken(TOKEN_COMMA);
    case '.': return makeToken(TOKEN_DOT);
    case '-': {
      if (match('-')) return makeToken(TOKEN_MINUS_MINUS);
      if (match('=')) return makeToken(TOKEN_MINUS_EQUAL);
      return makeToken(TOKEN_MINUS);
    }
    case '+': {
      if (match('+')) return makeToken(TOKEN_PLUS_PLUS);
      if (match('=')) return makeToken(TOKEN_PLUS_EQUAL);
      return makeToken(TOKEN_PLUS);
    }
    case '/': {
      if (match('=')) return makeToken(TOKEN_SLASH_EQUAL);
      return makeToken(TOKEN_SLASH);
    }
    case '*': {
      if (match('=')) return makeToken(TOKEN_STAR_EQUAL);
      return makeToken(TOKEN_STAR);
    }
    case '?': return makeToken(TOKEN_QUESTION);
    case ':': return makeToken(TOKEN_COLON);
    case '!':
      return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
      return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': {
      if (match('=')) return makeToken(TOKEN_LESS_EQUAL);
      if (match('<')) return makeToken(TOKEN_LESS_LESS);
      return makeToken(TOKEN_LESS);
    }
    case '>': {
      if (match('=')) return makeToken(TOKEN_GREATER_EQUAL);
      if (match('>')) return makeToken(TOKEN_GREATER_GREATER);
      return makeToken(TOKEN_GREATER);
    }
    case '~': return makeToken(TOKEN_TILDE);
    case '^': return makeToken(TOKEN_CARET);
    case '&': return makeToken(TOKEN_AMPERSAND);
    case '|': return makeToken(TOKEN_PIPE);
    case '"': return string();
  }

  return errorToken("Unexpected character.");
}
