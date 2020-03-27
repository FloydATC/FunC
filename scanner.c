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
  scanner->source = source;
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



static void copyScanner(Scanner* src, Scanner* dst) {
  dst->fileno  = src->fileno;
  dst->lineno  = src->lineno;
  dst->charno  = src->charno;
  dst->source  = src->source;
  dst->start   = src->start;
  dst->current = src->current;
  dst->parent  = src->parent;
  dst->vm      = src->vm;
}

static void swapScanners(Scanner* a, Scanner* b) {
  Scanner temp;
  copyScanner(a, &temp); // Store current scanner state
  copyScanner(b, a);
  copyScanner(&temp, b);
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


int scannerDepth(Scanner* scanner) {
  int depth = 0;
  Scanner* s = scanner;
  while (s->parent != NULL) {
    depth++;
    s = (Scanner*) s->parent;
  }
  return depth;
}

void dumpState(Scanner* scanner) {
  printf("Internal state of scanner %p\n", scanner);
  printf("  fileno    %d\n", scanner->fileno);
  printf("  lineno    %d\n", scanner->lineno);
  printf("  charno    %d\n", scanner->charno);
  printf("  source    %p (length=%d)\n", scanner->source, (int)strlen(scanner->source));
  printf("    {%.*s}\n", 40, scanner->source);
  printf("  start     %p (offset=%d)\n", scanner->start, (int)(scanner->start - scanner->source));
  printf("    {%.*s}\n", 40, scanner->start);
  printf("  current   %p (offset=%d)\n", scanner->current, (int)(scanner->current - scanner->source));
  printf("  parent    %p (depth=%d)\n", scanner->parent, scannerDepth(scanner));
  printf("  vm        %p\n", scanner->vm);
}


// This function is called when the scanner has encountered an #include directive
Token includeFile(Scanner* scanner, const char* filename, int length) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:includeFile() including filename='%.*s' length=%d, freeze parent state:\n", length, filename, length);
  dumpState(scanner);
#endif
  // The filename is not zero terminated so we need a copy
  char* filenamez = ALLOCATE(scanner->vm, char, length+1);
  strncpy(filenamez, filename, length);
  filenamez[length] = '\0';

  // Check if file has already been included
  int fileno = getFilenoByName(scanner->vm, filenamez);
  if (fileno == -1) {
    // File has not been included yet
    fileno = addFilename(scanner->vm, filenamez);
    char* source;
    int source_length = readFile(filenamez, &source);
    if (source_length < 0) return errorToken(scanner, "Error reading include file.");

    // Create a new scanner object
    Scanner* outer = initScanner(scanner->vm, fileno, source);

    // Since the Parser has a pointer to the scanner object,
    // we need to swap the internal state rather than simply use the new pointer.
    swapScanners(scanner, outer);
    scanner->parent = (struct Scanner*) outer;

    // The scanner should happily continue scanning the new file as if nothing happened
#ifdef DEBUG_TRACE_SCANNER
    printf("scanner:includeFile() starting child scanner:\n");
    dumpState(scanner);
#endif
  }
  FREE(scanner->vm, char, filenamez);

  // Return something other than an error, the actual token will be discarded
  return makeToken(scanner, TOKEN_EOF);
}


// This function is called when the scanner reaches EOF and scanner->parent is not NULL
void endOfFile(Scanner* scanner) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:endOfFile() child scanner reached EOF:\n");
  dumpState(scanner);
#endif

  // Source code buffer was allocated by readFile() and given to us so we must free it
  free((void*) scanner->source);

  // The outer scanner is stored as scanner->parent
  // Swap the contents back and then free the inner scanner object
  Scanner* inner = (Scanner*) scanner->parent;
  swapScanners(scanner, inner);
  destroyScanner(scanner->vm, inner);

  // The scanner should now resume scanning the file that contained a #include directive
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:endOfFile() resuming parent scanner:\n");
  dumpState(scanner);
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



static bool isAtEol(Scanner* scanner) {
  return scanner->current[0] == '\n' || scanner->current[0] == '\r';
}

static bool isAtEnd(Scanner* scanner) {
  return scanner->current[0] == '\0';
}

static char advance(Scanner* scanner) {
  scanner->current++;
  scanner->charno++;
#ifdef DEBUG_TRACE_SCANNER_VERBOSE
  printf("scanner:peek() scanner=%p\n", scanner);
  printf("scanner:advance() lineno=%d, charno=%d\n", scanner->lineno, scanner->charno);
#endif // DEBUG_TRACE_SCANNER
  return scanner->current[-1];
}

static char peek(Scanner* scanner) {
#ifdef DEBUG_TRACE_SCANNER_VERBOSE
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
      case '/':
        if (peekNext(scanner) == '/') {
          // A comment goes until the end of the line.
          while (!isAtEol(scanner) && !isAtEnd(scanner)) advance(scanner);
        } else if (peekNext(scanner) == '*') {
          // A block comment goes until '*/'
          advance(scanner); // Consume the slash
          advance(scanner); // Consume the star
          // Scan for */ while ignoring everything else
          while ((peek(scanner) != '*' || peekNext(scanner) != '/') && !isAtEnd(scanner)) {
            if (isAtEol(scanner)) { scanner->lineno++; scanner->charno = 1; }
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


Token includeDirective(Scanner* scanner) {
  skipWhitespace(scanner);
  // Directive must be followed by a filename in doublequotes, newline is forbidden
  if (peek(scanner) == '"') {
    advance(scanner); // consume initial doublequotes
    scanner->start = scanner->current;
    while (peek(scanner) != '"' && !isAtEol(scanner) && !isAtEnd(scanner)) advance(scanner);
    if (isAtEol(scanner)) return errorToken(scanner, "Unterminated string.");
    if (isAtEnd(scanner)) return errorToken(scanner, "Unexpected end of file.");
    const char* fname_begin = scanner->start;
    int fname_length = (int)(scanner->current - scanner->start);
    advance(scanner); // consume terminating doublequotes
    scanner->start = scanner->current;
    skipWhitespace(scanner);
    scanner->start = scanner->current;
    return includeFile(scanner, fname_begin, fname_length);
  } else {
    return errorToken(scanner, "Expected '\"' after #include.");
  }
}

Token directive(Scanner* scanner) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:directive()\n");
  printf("scanner:directive() start=%p\n", scanner->start);
  printf("scanner:directive() current=%p\n", scanner->current);
#endif
  advance(scanner);
  // Get directive name
  while (isAlpha(peek(scanner)) || isBase10Digit(peek(scanner))) advance(scanner);
  int directive_length = (int)(scanner->current - scanner->start);

#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:directive() start=%p\n", scanner->start);
  printf("scanner:directive() current=%p\n", scanner->current);
  printf("scanner:directive() directive length=%d name=%.*s\n", directive_length, directive_length, scanner->start);
#endif

  if (strncmp("#include", scanner->start, directive_length) == 0) {
    return includeDirective(scanner);
  }

  return errorToken(scanner, "Unknown directive.");
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
    case 'e':
      if (scanner->current - scanner->start > 1) {
        switch (scanner->start[1]) {
          case 'l': return checkKeyword(scanner, 2, 2, "se", TOKEN_ELSE);
          case 'x': return checkKeyword(scanner, 2, 2, "it", TOKEN_EXIT);
        }
      }
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
    if (isAtEol(scanner)) { scanner->lineno++; scanner->charno = 1; }
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


bool handleEof(Scanner* scanner) {
  if (isAtEnd(scanner)) {
#ifdef DEBUG_TRACE_SCANNER
    printf("scanner:scanToken() scanner=%p reached EOF\n", scanner);
    dumpState(scanner);
#endif
    if (scanner->parent == NULL) {
#ifdef DEBUG_TRACE_SCANNER
      printf("scanner:scanToken() scanner=%p reached EOF of top level file\n", scanner);
#endif
      return true;
    } else {
#ifdef DEBUG_TRACE_SCANNER
      printf("scanner:scanToken() scanner=%p reached EOF of included file\n", scanner);
#endif
      endOfFile(scanner); // Note: scanner->parent gets copied to scanner object
      skipWhitespace(scanner);
      scanner->start = scanner->current;
    }
  }
  return false;
}

Token scanToken(Scanner* scanner) {
#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:scanToken() scanner=%p\n", scanner);
#endif

  skipWhitespace(scanner);
  scanner->start = scanner->current;

  // Checking for preprocessor directives and EOF is closely tied together
  while (isAtEnd(scanner) || peek(scanner) == '#') {
    if (handleEof(scanner)) return makeToken(scanner, TOKEN_EOF);
    if (peek(scanner) == '#') {
      Token result = directive(scanner); // Note: Changes state of scanner object
      if (result.type == TOKEN_ERROR) return result;
      skipWhitespace(scanner);
      scanner->start = scanner->current;
    }
  }

  char c = advance(scanner);

  if (isAlpha(c)) return identifier(scanner);
  if (isBase10Digit(c)) return number(scanner, c); // All radii start with a base 10 digit


  switch (c) {
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

//#ifdef DEBUG_TRACE_SCANNER
  printf("scanner:scanToken() scanner=%p character=%d '%c'\n", scanner, c, c);
//#endif
  return errorToken(scanner, "Unexpected character.");
}
