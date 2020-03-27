#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "version.h"
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "error.h"
#include "file.h"
#include "vm.h"


// http://craftinginterpreters.com
// C:\Users\floyd\Documents\C\FunC\craftinginterpreters.com\index.html

// Chapter 21: Global variables

char* code = NULL;
char* temp = NULL;
static void repl(VM* vm) {
  char line[1024]; // Note: Line too long == CRASH


  for (;;) {
    printf("> ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break; // Read error from STDIN
    }

    if (strncmp(line, "\n", 1) == 0 || strncmp(line, "\r\n", 1) == 0) {
      // Empty line from STDIN
      if (code == NULL) continue;
      InterpretResult result = interpret(vm, code, "");
      while (result == INTERPRET_COMPILED || result == INTERPRET_RUNNING) {
        //printf("== timeslice begin\n");
        result = run(vm);
      }
      //printf("main:repl() free(%p) // code\n", (void*) code);
      free(code);
      code = NULL;
      continue;
    }

    if (strncmp(line, "exit\n", 5) == 0 || strncmp(line, "exit\r\n", 5) == 0)
      break; // Got "exit" command from STDIN

    // Add line to code (+1 = include "\0")
    size_t ll = strlen(line);
    if (code == NULL) {
      temp = malloc(ll + 1);
      strncpy(temp, line, ll + 1);
    } else {
      size_t cl = strlen(code);
      temp = (char *) malloc(cl + ll + 1);
      strncpy(temp, code, cl + 1);
      strncat(temp, line, ll + 1);
      //printf("main:repl() free(%p) // code\n", (void*) code);
      free(code);
      code = NULL; // Just for verbosity
    }
    code = temp;
    temp = NULL; // Again, just for verbosity

  }
}


static void runFile(VM* vm, const char* path) {
  InterpretResult result;
  char* source;
  int bytes = readFile(path, &source);
  if (bytes > 0) {
    result = interpret(vm, source, path);
    while (result == INTERPRET_COMPILED || result == INTERPRET_RUNNING) {
      result = run(vm);
    }
    free(source);
  } else {
    result = INTERPRET_COMPILE_ERROR;
  }

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

void print_version() {
  printf("FunC v%ld.%ld.%ld.%ld %s",MAJOR,MINOR,BUILD,REVISION,STATUS); //C example
}

int main(int argc, const char* argv[]) {
  VM* vm = initVM();

  if (argc == 1) {
    print_version();
    printf(" (interactive mode)\n");
    repl(vm);
  } else if (argc == 2) {
    runFile(vm, argv[1]);
  } else {
    print_version();
    printf("\n");
    fprintf(stderr, "Usage: func [path]\n");
    exit(64);
  }

  freeVM(vm);

  return 0;
}

