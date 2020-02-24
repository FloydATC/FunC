#include <stdio.h>
#include <string.h>

#include "objstring.h"

#include "index.h"
#include "memory.h"
#include "number.h"
#include "utf8.h"
#include "vm.h"


#define CHECK_ARG_IS_STRING(index) \
  if (argCount >= index+1 && !IS_STRING(args[index])) { \
    runtimeError(vm, "Argument %d must be a string, got %s.", index+1, getValueTypeString(args[index])); \
    return false; \
  }

#define CHECK_ARGS_ZERO_OR_ONE() \
  if (argCount > 1) { \
    runtimeError(vm, "Method takes 1 optional argument, got %d.", argCount); \
    return false; \
  }

#define CHECK_ARGS_ONE_OR_TWO() \
  if (argCount < 1 || argCount > 2) { \
    runtimeError(vm, "Method takes 1 or 2 arguments, got %d.", argCount); \
    return false; \
  }

#define CHECK_ARGS_ONE() \
  if (argCount != 1) { \
    runtimeError(vm, "Method takes 1 argument, got %d.", argCount); \
    return false; \
  }

#define CHECK_ARGS_RANGE(min, max) \
  if (argCount < min || argCount > max) { \
    runtimeError(vm, "Method takes %d to %d arguments, got %d.", min, max, argCount); \
    return false; \
  }



// Count how many times substr occurs in string
// If substr is empty, return -1
int count_substr(const char* string, const char* substr) {
  int count = 0;
  int sublen = strlen(substr);
  if (sublen == 0) return -1;
  char* pos = (char*) string;
  for (;;) {
    pos = strstr(pos, substr);
    if (pos == NULL) break;
    pos += sublen;
    count++;
  }
  //printf("objstring:count_substr() %s occurs %d times in %s\n", substr, count, string);
  return count;
}

// Return integer offset to the first occurrence of substr within string
// Return -1 if substr is not found
// This is analogous to .indexOf found in many scripting languages
int substr_offset(const char* string, const char* substr) {
  char* ptr = strstr(string, substr);
  return (ptr != NULL ? ptr - string : -1);
}


// Split string into want_parts at every occurrence of delim
// Warning: want_parts *must* be within range, use count_substr() for this
ObjArray* split_string(VM* vm, const char* string, const char* delim, int want_parts) {
  ObjArray* res = newArrayZeroed(vm, want_parts);
  int element_length;
  int delim_length = strlen(delim);
  int offset = 0;
  for (int i=0; i<want_parts; i++) {
    if (i < want_parts-1) {
      element_length = substr_offset(string+offset, delim);
      //printf("objstring:split_string() i=%d offset=%d element_length=%d\n", i, offset, element_length);
      res->values[i] = OBJ_VAL(copyString(vm, string+offset, element_length));
      offset += element_length;
      offset += delim_length;
    } else {
      // Last element, consume the rest of the input string
      res->values[i] = OBJ_VAL(copyString(vm, string+offset, strlen(string+i)));
    }
    //printf("objstring:split_string() i=%d element='%s'\n", i, AS_CSTRING(res->values[i]));
  }
  return res;
}


ObjArray* chars_to_array(VM* vm, const char* string, int want_parts) {
  if (want_parts == -1 || want_parts > (int) strlen(string)) want_parts = strlen(string);
  //printf("objstring:chars_to_array() string=%s want=%d\n", string, want_parts);
  ObjArray* res = newArrayZeroed(vm, want_parts);
  for (int i=0; i<want_parts; i++) {
    if (i < want_parts-1) {
      //printf("next element: %d\n", i);
      res->values[i] = OBJ_VAL(copyString(vm, string+i, 1));
    } else {
      //printf("last element: %d\n", i);
      // Last element gets remainder of the string
      int rest = strlen(string+i);
      res->values[i] = OBJ_VAL(copyString(vm, string+i, rest));
    }
    //printf("objstring:chars_to_array() i=%d element='%s'\n", i, AS_CSTRING(res->values[i]));
  }
  return res;
}


/*
// INTERNAL DEBUG FUNCTION
void dump_string(char* string, int length) {
  for (int i=0; i<length; i++) {
    printf("i=%x byte=%x char=%s\n", i, (unsigned char) string[i], (isutf(string, i) ? "true" : "false"));
  }
}
*/


// Native C method: STRING.value()
static bool string_value(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_RANGE(0,2);

  ObjString* string = AS_STRING(receiver);

  int radix = 10;
  if (argCount >= 1) {
    radix = (int) AS_NUMBER(args[0]);
  }
  int maxlen = string->length;
  if (argCount >= 2) {
    maxlen = (int) AS_NUMBER(args[1]);
  }
  if (maxlen > string->length) {
    runtimeError(vm, "Length %d is out of range.", maxlen);
    return false;
  }

  *result = NUMBER_VAL(str_to_double(string->chars, maxlen, radix));
  return true;
}


// Native C method: STRING.byte_at()
static bool string_byte_at(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();

  ObjString* string = AS_STRING(receiver);

  int offset = (IS_NULL(args[0]) ? 0 : (int) AS_NUMBER(args[0]));
  offset = check_offset(offset, string->length);
  if (offset == -1) { runtimeError(vm, "Index out of range."); return false; }

  *result = OBJ_VAL(copyString(vm, string->chars + offset, 1));
  return true;
}


// Native C method: STRING.bytes_at()
static bool string_bytes_at(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE_OR_TWO();

  ObjString* string = AS_STRING(receiver);

  int offset = (IS_NULL(args[0]) ? 0 : (int) AS_NUMBER(args[0]));
  offset = check_offset(offset, string->length);
  if (offset == -1) { runtimeError(vm, "Offset out of range."); return false; }

  int length = string->length - offset;
  if (argCount == 2) length = (int) AS_NUMBER(args[1]);
  length = check_length(length, offset, string->length);
  if (length == -1) { runtimeError(vm, "Length out of range."); return false; }

  ObjString* substr = copyString(vm, string->chars + offset, length);

  *result = OBJ_VAL(substr);
  return true;
}


// Native C method: STRING.char_at()
static bool string_char_at(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();

  ObjString* string = AS_STRING(receiver);

  int offset = (IS_NULL(args[0]) ? 0 : (int) AS_NUMBER(args[0]));
  offset = check_offset(offset, string->length);
  if (offset == -1) { runtimeError(vm, "Index out of range."); return false; }

  int byte_offset = u8_offset(string->chars, offset);
  int byte_length = u8_seqlen(string->chars+byte_offset);
  *result = OBJ_VAL(copyString(vm, string->chars+byte_offset, byte_length));
  return true;
}


// Native C method: STRING.substr()
static bool string_substr(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE_OR_TWO();

  ObjString* string = AS_STRING(receiver);
  int codepoints = u8_strlen(string->chars);

  int want_offset = (IS_NULL(args[0]) ? 0 : (int) AS_NUMBER(args[0]));
  int offset = check_offset(want_offset, codepoints);
  if (offset == -1) { runtimeError(vm, "Offset %d out of range.", want_offset); return false; }

  int want_length = codepoints - offset;
  if (argCount == 2) want_length = (int) AS_NUMBER(args[1]);
  int length = check_length(want_length, offset, codepoints);
  if (length == -1) { runtimeError(vm, "Length %d out of range.", want_length); return false; }
  if (length == 0) {
    // If calculated length is zero, return empty string
    *result = OBJ_VAL(copyString(vm, "", 0));
    return true;
  }

  int byte_offset = u8_offset(string->chars, offset);
  int byte_length = u8_offset(string->chars+byte_offset, length);
  *result = OBJ_VAL(copyString(vm, string->chars+byte_offset, byte_length));
  return true;

}

// Native C method: STRING.split()
static bool string_split(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE_OR_TWO();
  CHECK_ARG_IS_STRING(0);

  ObjString* delim = AS_STRING(args[0]);
  int want_parts = (argCount == 2 ? AS_NUMBER(args[1]) : -1); // Default (-1) is unlimited
  if (want_parts == 0) {
    // Wants zero length, gets zero length
    *result = OBJ_VAL(newArray(vm));
    return true;
  }

  ObjString* string = AS_STRING(receiver);
  //printf("objstring:string_split() string=%s delim=%s want=%d\n", string->chars, delim->chars, want_parts);

  // Special case: If length of delimiter is zero, split each char
  if (strlen(delim->chars) == 0) {
    *result = OBJ_VAL(chars_to_array(vm, string->chars, want_parts));
    return true;
  }

  // Count how many times delimiter occurs in string
  int have_parts = count_substr(string->chars, delim->chars) + 1;
  //printf("objstring:string_split() have=%d\n", have_parts);
  if (want_parts == -1 || want_parts > have_parts) want_parts = have_parts;

  *result = OBJ_VAL(split_string(vm, string->chars, delim->chars, want_parts));
  return true;

}


// Native C method: STRING.ltrim()
static bool string_ltrim(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ZERO_OR_ONE();
  CHECK_ARG_IS_STRING(0);

  ObjString* string = AS_STRING(receiver);
  ObjString* unwanted;
  if (argCount == 1) {
    unwanted = AS_STRING(args[0]);
  } else {
    unwanted = copyString(vm, " ", 1); // Default is space character
  }
  int offset = 0;
  while (offset < string->length && strchr(unwanted->chars, string->chars[offset])!=NULL) offset++;
  if (offset > 0) {
    *result = OBJ_VAL(copyString(vm, string->chars+offset, string->length-offset));
  } else {
    *result = receiver;
  }
  return true;
}



// Native C method: STRING.rtrim()
static bool string_rtrim(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ZERO_OR_ONE();
  CHECK_ARG_IS_STRING(0);

  ObjString* string = AS_STRING(receiver);
  ObjString* unwanted;

  if (argCount == 1) {
    unwanted = AS_STRING(args[0]);
  } else {
    unwanted = copyString(vm, " ", 1); // Default is space character
  }

  int newlen = string->length;
  while (newlen > 0 && strchr(unwanted->chars, string->chars[newlen-1])!=NULL) newlen--;
  if (newlen < string->length) {
    *result = OBJ_VAL(copyString(vm, string->chars, newlen));
  } else {
    *result = receiver;
  }
  return true;
}


#define METHOD(fn_name, fn_call) \
  if (strcmp(name->chars, fn_name)==0) { \
    *property = OBJ_VAL(newNativeMethod(vm, receiver, name, fn_call)); \
    return true; \
  }


bool getStringProperty(void* vm, Value receiver, ObjString* name, Value* property) {
  ObjString* string = AS_STRING(receiver);

  // Properties
  if (strcmp(name->chars, "bytes")==0) {
    // Return the string length in number of bytes (storage size)
    *property = NUMBER_VAL(string->length);
    return true;
  }
  if (strcmp(name->chars, "chars")==0) {
    // Return the string length in number of characters (print size)
    int count = 0;
    for (int i=0; i<string->length; i++) {
      if (isutf(string->chars[i])) count++;
    }
    *property = NUMBER_VAL(count);
    return true;
  }
  if (strcmp(name->chars, "code")==0) {
    // Return the first codepoint value of the string, -1 if string is empty
    int bufsiz = 2;
    uint32_t codepoint[bufsiz];
    //printf("objstring:stringProperty() get codepoint of '%s' (%d bytes)\n", string->chars, string->length);
    u8_toucs(codepoint, bufsiz, string->chars, string->length);
    //printf("objstring:stringProperty() result=%d\n", codepoint[0]);
    *property = NUMBER_VAL((double) codepoint[0]);
    return true;
  }
  if (strcmp(name->chars, "num")==0) {
    *property = NUMBER_VAL(str_to_double(string->chars, string->length, 10));
    return true;
  }

  METHOD("value",    string_value);
  METHOD("byte_at",  string_byte_at);
  METHOD("bytes_at", string_bytes_at);
  METHOD("char_at",  string_char_at);
  METHOD("substr",   string_substr);
  METHOD("split",    string_split);
  METHOD("ltrim",    string_ltrim);
  METHOD("rtrim",    string_rtrim);

  runtimeError(vm, "String has no '%s'.", name->chars);
  return false;
}


// Get a string property, pop receiver and push the property
bool pushStringProperty(void* vm, Value receiver, ObjString* name) {
  Value method;
  if (!getStringProperty(vm, receiver, name, &method)) return false;
  pop(vm); // receiver
  push(vm, method);
  return true;
}



#undef METHOD

#undef CHECK_ARG_IS_STRING
#undef CHECK_ARGS_ZERO_OR_ONE
#undef CHECK_ARGS_ONE_OR_TWO
#undef CHECK_ARGS_ONE
#undef CHECK_ARGS_RANGE

