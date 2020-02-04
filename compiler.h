#ifndef clox_compiler_h
#define clox_compiler_h

#include "vm.h"

struct VM;

ObjFunction* compile(VM* vm, int fileno, const char* source, char** err);
void markCompilerRoots();

#endif

