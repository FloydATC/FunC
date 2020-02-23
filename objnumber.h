
#ifndef clox_objnumber_h
#define clox_objnumber_h

#include "value.h"


/*

  While most of the code dealing with the Value type is still in vm.c,
  objnumber.c contains a set of hard-coded methods that help make numbers useful
  and comfortable to work with:

  0.floor          return floor value (e.g. 1.2 => 1.0)
  0.ceil           return ceiling value (e.g. 1.2 => 2.0)
  0.abs            return absolute value
  0.sqrt           return square root
  0.base(a)        return string representation in base 2..36 inclusive
  0.f(a)           return custom formatted FLOAT as from printf(a, ...)

*/


bool getNumberProperty(void* vm, Value receiver, ObjString* name, Value* property);
bool pushNumberProperty(void* vm, Value receiver, ObjString* name);
//bool numberProperty(void* vm, Value receiver, ObjString* name);


#endif

