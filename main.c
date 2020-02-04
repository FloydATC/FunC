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
      char* err = NULL;
      InterpretResult result = interpret(vm, code, &err);
      if (err != NULL) {
        printf(err);
        //printf("main:repl() free(%p) // error buffer\n", (void*) err);
        //free(err);
        err = NULL;
      }
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
    int ll = strlen(line);
    if (code == NULL) {
      temp = malloc(ll + 1);
      strncpy(temp, line, ll + 1);
    } else {
      int cl = strlen(code);
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

/*
static char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}
*/

static void runFile(VM* vm, const char* path) {
  char* source = readFile(path);
  char* err;
  InterpretResult result = interpret(vm, source, &err);
  if (err != NULL) {
    printf(err);
    //printf("main:repl() free(%p) // error buffer\n", (void*) err);
    //free(err);
    err = NULL;
  }
  while (result == INTERPRET_COMPILED || result == INTERPRET_RUNNING) {
    result = run(vm);
  }
  free(source);

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

