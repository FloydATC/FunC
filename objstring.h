
#ifndef clox_string_h
#define clox_string_h

#include "value.h"


/*

  While most of the code dealing with the ObjString type is still in vm.c,
  objstring.c contains a set of hard-coded methods that help make arrays useful
  and comfortable to work with:


*/


bool stringProperty(void* vm, Value receiver, ObjString* name);


#endif
