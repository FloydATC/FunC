#ifndef clox_array_h
#define clox_array_h

#include "value.h"


/*

  While most of the code dealing with the ObjArray type is still in vm.c,
  objarray.c contains a set of hard-coded methods that help make arrays useful
  and comfortable to work with:

  [].length                return length of array as a number value
  [].push(v1, v2, vN)      append value(s) to array, return new array
  [v1, v2, vN].pop()       remove last value from array, return value
  [].unshift(v1, v2, vN)   prepend value(s) to array, return new array
  [v1, v2, vN].shift()     remove first value from array, return value
  [].size(N)               extend or truncate table to specified size
  [].fill(V)               set all elements in array to specified value
  [].join(S)               return String with all elements joined with String
  [].flat()                return a new array with all elements from nested array

*/


bool arrayProperty(void* vm, Value receiver, ObjString* name);


#endif
