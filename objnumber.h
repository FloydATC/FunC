
#ifndef clox_objnumber_h
#define clox_objnumber_h

#include "value.h"


/*

  While most of the code dealing with the Value type is still in vm.c,
  objnumber.c contains a set of hard-coded methods that help make numbers useful
  and comfortable to work with:


*/


bool numberProperty(void* vm, Value receiver, ObjString* name);


#endif

