#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "file.h"
#include "scanner.h"
#include "memory.h"


// "Constructor"
Scanner* initScanner(void* vm, int fileno, const char* source) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:initScanner(%d, %p)\n", fileno, source);
#endif
  Scanner* scanner = ALLOCATE(vm, Scanner, 1);
  scanner->start = source;
  scanner->current = source;
  scanner->lineno = 1;
  scanner->charno = 1;
  scanner->fileno = fileno;
  scanner->vm = vm;
  scanner->parent = NULL;
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:initScanner() initialized scanner %p\n", scanner);
#endif
  return scanner;
}


// "Destructor"
void destroyScanner(void* vm, Scanner* scanner) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:destroyScanner() vm=%p scanner=%p\n", vm, scanner);
#endif
  FREE(vm, Scanner, scanner);
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:destroyScanner() done\n");
#endif
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



static bool isAtEnd(Scanner* scanner) {
  return scanner->current[0] == '\0';
}

static char advance(Scanner* scanner) {
  scanner->current++;
  scanner->charno++;
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:peek() scanner=%p\n", scanner);
  printf("scanner:advance() lineno=%d, charno=%d\n", scanner->lineno, scanner->charno);
#endif // DEBUG_TRACE_SCANNER
  return scanner->current[-1];
}

static char peek(Scanner* scanner) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:peek() scanner=%p\n", scanner);
  printf("scanner:peek() lineno=%d, charno=%d\n", scanner->lineno, scanner->charno);
#endif // DEBUG_TRACE_SCANNER
  return scanner->current[0];
}

static char peekNext(Scanner* scanner) {
  if (isAtEnd(scanner)) return '\0';
  return scanner->current[1];
}

static bool match(Scanner* scanner, char expected) {
  if (isAtEnd(scanner)) return false;
  if (*scanner->current != expected) return false;

  scanner->current++;
  return true;
}


static Token makeToken(Scanner* scanner, TokenType type) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:makeToken(%d)\n", type);
#endif // DEBUG_TRACE_SCANNER
  Token token;
  token.type = type;
  token.start = scanner->start;
  token.length = (int)(scanner->current - scanner->start);
  token.fileno = scanner->fileno;
  token.lineno = scanner->lineno;
  token.charno = scanner->charno;

  return token;
}

static Token errorToken(Scanner* scanner, const char* message) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:errorToken(%s)\n", message);
#endif // DEBUG_TRACE_SCANNER
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.fileno = scanner->fileno;
  token.lineno = scanner->lineno;
  token.charno = scanner->charno;
  return token;
}

void directive(Scanner* scanner) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:directive()\n");
  printf("scanner:directive() start=%p\n", scanner->start);
  printf("scanner:directive() current=%p\n", scanner->current);
#endif
  advance(scanner);
  // Get directive name
  while (isAlpha(peek(scanner)) || isBase10Digit(peek(scanner))) advance(scanner);
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:directive() start=%p\n", scanner->start);
  printf("scanner:directive() current=%p\n", scanner->current);
  int length = scanner->current - scanner->start;
  printf("scanner:directive() directive length=%d name=%.*s\n", length, length, scanner->start);
#endif


  while (peek(scanner) != '\n' && !isAtEnd(scanner)) {
    advance(scanner);
  }
}


static void skipWhitespace(Scanner* scanner) {
  for (;;) {
    char c = peek(scanner);
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance(scanner);
        break;
      case '\n':
        scanner->lineno++;
        scanner->charno = 1;
        advance(scanner);
        break;
      case '#': {
        // Marks the beginning of a directive
        directive(scanner);
        break;
      }
      case '/':
        if (peekNext(scanner) == '/') {
          // A comment goes until the end of the line.
          while (peek(scanner) != '\n' && !isAtEnd(scanner)) advance(scanner);
        } else if (peekNext(scanner) == '*') {
          // A block comment goes until '*/'
          advance(scanner); // Consume the slash
          advance(scanner); // Consume the star
          // Scan for */ while ignoring everything else
          while ((peek(scanner) != '*' || peekNext(scanner) != '/') && !isAtEnd(scanner)) {
            if (peek(scanner) == '\n') { scanner->lineno++; scanner->charno = 1; }
            advance(scanner);
          }
          advance(scanner); // Consume the star
          advance(scanner); // Consume the slash
        } else {
          return; // This slash is not the beginning of a comment.
        }
        break;
      default:
        return;
    }
  }
}

static TokenType checkKeyword(Scanner* scanner, int start, int length,
    const char* rest, TokenType type) {
  if (scanner->current - scanner->start == start + length &&
      memcmp(scanner->start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner* scanner)
{
  switch (scanner->start[0]) {
    case 'a': return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND);
    case 'b': return checkKeyword(scanner, 1, 4, "reak", TOKEN_BREAK);
    case 'c':
      if (scanner->current - scanner->start > 1) {
        switch (scanner->start[1]) {
          case 'a': return checkKeyword(scanner, 2, 2, "se", TOKEN_CASE);
          case 'l': return checkKeyword(scanner, 2, 3, "ass", TOKEN_CLASS);
          case 'o': return checkKeyword(scanner, 2, 6, "ntinue", TOKEN_CONTINUE);
        }
      }
      break;
    case 'd':
      if (scanner->current - scanner->start > 1) {
        switch (scanner->start[1]) {
          case 'e':
            if (scanner->current - scanner->start > 2) {
              switch (scanner->start[2]) {
                case 'b': return checkKeyword(scanner, 3, 2, "ug", TOKEN_PRINT);
                case 'f': return checkKeyword(scanner, 3, 4, "ault", TOKEN_DEFAULT);
              }
            }
        }
      }
    case 'e': return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
    case 'f':
      if (scanner->current - scanner->start > 1) {
        switch (scanner->start[1]) {
          case 'a': return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
          case 'o': return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
          case 'u': return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN);
        }
      }
      break;
    case 'i': return checkKeyword(scanner, 1, 1, "f", TOKEN_IF);
    case 'n': return checkKeyword(scanner, 1, 3, "ull", TOKEN_NULL);
    case 'o': return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
    //case 'p': return checkKeyword(scanner, 1, 4, "rint", TOKEN_PRINT);
    case 'r': return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
    case 's':
      if (scanner->current - scanner->start > 1) {
        switch (scanner->start[1]) {
          case 'u': return checkKeyword(scanner, 2, 3, "per", TOKEN_SUPER);
          case 'w': return checkKeyword(scanner, 2, 4, "itch", TOKEN_SWITCH);
        }
      }
    case 't':
      if (scanner->current - scanner->start > 1) {
        switch (scanner->start[1]) {
          case 'h': return checkKeyword(scanner, 2, 2, "is", TOKEN_THIS);
          case 'r': return checkKeyword(scanner, 2, 2, "ue", TOKEN_TRUE);
        }
      }
      break;
    case 'v': return checkKeyword(scanner, 1, 2, "ar", TOKEN_VAR);
    case 'w': return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner* scanner) {
  while (isAlpha(peek(scanner)) || isBase10Digit(peek(scanner))) advance(scanner);

  return makeToken(scanner, identifierType(scanner));
}

static Token number(Scanner* scanner, char current) {
  //printf("number() peek=%c next=%c\n", peek(), peekNext());
  if (current== '0' && peek(scanner) == 'b') {
    //printf("number() binary notation\n");
    advance(scanner); // Consume the 'b'
    // Binary
    // ...
    while (isBase2Digit(peek(scanner))) advance(scanner);
    return makeToken(scanner, TOKEN_BASE2_NUMBER);
  }
  if (current== '0' && isBase8Digit(peek(scanner))) {
    //printf("number() octal notation\n");
    // Octal
    // ...
    while (isBase8Digit(peek(scanner))) advance(scanner);
    return makeToken(scanner, TOKEN_BASE8_NUMBER);
  }
  if (current== '0' && peek(scanner) == 'x') {
    //printf("number() hexadecimal notation\n");
    advance(scanner); // Consume the 'x'
    // Hexadecimal
    while (isBase16Digit(peek(scanner))) advance(scanner);
    return makeToken(scanner, TOKEN_BASE16_NUMBER);
  }
  //printf("number() decimal notation\n");

  while (isBase10Digit(peek(scanner))) advance(scanner);

  // Look for a fractional part.
  if (peek(scanner) == '.' && isBase10Digit(peekNext(scanner))) {
    // Consume the ".".
    advance(scanner);

    while (isBase10Digit(peek(scanner))) advance(scanner);
  }

  return makeToken(scanner, TOKEN_BASE10_NUMBER);
}

static Token string(Scanner* scanner) {
  while (peek(scanner) != '"' && !isAtEnd(scanner)) {
    if (peek(scanner) == '\n') { scanner->lineno++; scanner->charno = 1; }
    if (peek(scanner) == '\\' && peekNext(scanner) == '\\') {
      // Double backslashes will be interpreted as a single backslash later
      // Skip them so they don't interfere with the next test
      advance(scanner);
      advance(scanner);
      continue;
    }
    if (peek(scanner) == '\\' && peekNext(scanner) == '"') {
      // Escaped double quotes must not terminate the string
      advance(scanner);
      advance(scanner);
      continue;
    }
    // All other escape sequences should now pass through without causing problems
    advance(scanner);
  }

  if (isAtEnd(scanner)) return errorToken(scanner, "Unterminated string.");

  // The closing quote.
  advance(scanner);
  return makeToken(scanner, TOKEN_STRING);
}


Token scanToken(Scanner* scanner) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:scanToken() scanner=%p\n", scanner);
#endif

  skipWhitespace(scanner);
  scanner->start = scanner->current;

  // TODO: Check for outer scanner
  if (isAtEnd(scanner)) return makeToken(scanner, TOKEN_EOF);

  char c = advance(scanner);

  if (isAlpha(c)) return identifier(scanner);
  if (isBase10Digit(c)) return number(scanner, c); // All radii start with a base 10 digit

  switch (c) {
    //case '#': return makeToken(TOKEN_HASH);
    case '(': return makeToken(scanner, TOKEN_LEFT_PAREN);
    case ')': return makeToken(scanner, TOKEN_RIGHT_PAREN);
    case '{': return makeToken(scanner, TOKEN_LEFT_BRACE);
    case '}': return makeToken(scanner, TOKEN_RIGHT_BRACE);
    case '[': return makeToken(scanner, TOKEN_LEFT_BRACKET);
    case ']': return makeToken(scanner, TOKEN_RIGHT_BRACKET);
    case ';': return makeToken(scanner, TOKEN_SEMICOLON);
    case ',': return makeToken(scanner, TOKEN_COMMA);
    case '.': return makeToken(scanner, TOKEN_DOT);
    case '-': {
      if (match(scanner, '-')) return makeToken(scanner, TOKEN_MINUS_MINUS);
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_MINUS_EQUAL);
      return makeToken(scanner, TOKEN_MINUS);
    }
    case '+': {
      if (match(scanner, '+')) return makeToken(scanner, TOKEN_PLUS_PLUS);
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_PLUS_EQUAL);
      return makeToken(scanner, TOKEN_PLUS);
    }
    case '/': {
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_SLASH_EQUAL);
      return makeToken(scanner, TOKEN_SLASH);
    }
    case '*': {
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_STAR_EQUAL);
      return makeToken(scanner, TOKEN_STAR);
    }
    case '~': {
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_TILDE_EQUAL);
      return makeToken(scanner, TOKEN_TILDE);
    }
    case '^': {
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_CARET_EQUAL);
      return makeToken(scanner, TOKEN_CARET);
    }
    case '&': {
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_AMP_EQUAL);
      return makeToken(scanner, TOKEN_AMP);
    }
    case '|': {
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_PIPE_EQUAL);
      return makeToken(scanner, TOKEN_PIPE);
    }
    case '%': {
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_PERCENT_EQUAL);
      return makeToken(scanner, TOKEN_PERCENT);
    }
    case '?': return makeToken(scanner, TOKEN_QUESTION);
    case ':': return makeToken(scanner, TOKEN_COLON);
    case '!':
      return makeToken(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=':
      return makeToken(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': {
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_LESS_EQUAL);
      if (match(scanner, '<')) return makeToken(scanner, TOKEN_LESS_LESS);
      return makeToken(scanner, TOKEN_LESS);
    }
    case '>': {
      if (match(scanner, '=')) return makeToken(scanner, TOKEN_GREATER_EQUAL);
      if (match(scanner, '>')) return makeToken(scanner, TOKEN_GREATER_GREATER);
      return makeToken(scanner, TOKEN_GREATER);
    }
    case '"': return string(scanner);
  }

  return errorToken(scanner, "Unexpected character.");
}
