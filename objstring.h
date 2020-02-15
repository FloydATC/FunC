
#ifndef clox_string_h
#define clox_string_h

#include "value.h"


/*

  While most of the code dealing with the ObjString type is still in vm.c,
  objstring.c contains a set of hard-coded methods that help make strings useful
  and comfortable to work with:

  "".bytes          return number of bytes in string
  "".chars          return number of utf8 code points in string
  "".byte_at(a)     return byte at offset a
  "".char_at(a)     return utf8 code point number a
  "".bytes_at(a,b)  return b bytes starting at offset a (negative values = count from end)
  "".substr(a,b)    return b codepoints starting at number a (negative values = count from end)
  "".value(a)       return number value of string in base 2..36 inclusive (default=10 if not specified)
  "".split(a)       return array containing string parts using a as delimiter (individual BYTES if "")
  "".f(a)           return custom formatted string as from printf(a, ...)

*/


bool stringProperty(void* vm, Value receiver, ObjString* name);


#endif
