#ifndef clox_compiler_h
#define clox_compiler_h


#include "object.h"


ObjFunction* compile(void* vm, int fileno, const char* source);
void markCompilerRoots();

#endif

