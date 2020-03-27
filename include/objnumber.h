
#ifndef clox_objnumber_h
#define clox_objnumber_h

#include "value.h"


/*

  While most of the code dealing with the Value type is still in vm.c,
  objnumber.c contains a set of hard-coded methods that help make numbers useful
  and comfortable to work with:

  0.base(a)        return string representation in base 2..36 inclusive
  0.str            shorthand for .base(10); return number as string
  0.char           return utf8 character as string

  0.atan2(x)       y.atan2(x) returns the arctangent of y, x
  0.pow(y)         x.pow(y) returns x to the power of y
  0.fmod(y)        x.fmod(y) returns the remainder of x divided by y
  0.hypot(y)       x.hypot(y) returns the hypotenuse of x,y = sqrt(x*x + y*y)

  // The following are thin wrappers around the corresponding C math functions
  0.floor          return floor value (e.g. 1.2 => 1.0)
  0.ceil           return ceiling value (e.g. 1.2 => 2.0)
  0.abs            return absolute value
  0.sqrt           return square root
  0.sin
  0.cos
  0.tan
  0.asin
  0.acos
  0.atan
  0.sinh
  0.cosh
  0.tanh
  0.asinh
  0.acosh
  0.atanh
  0.exp
  0.log
  0.log10
  0.cbrt

*/


bool getNumberProperty(void* vm, Value receiver, ObjString* name, Value* property);
bool pushNumberProperty(void* vm, Value receiver, ObjString* name);
//bool numberProperty(void* vm, Value receiver, ObjString* name);


#endif

