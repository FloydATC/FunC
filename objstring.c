#include <stdio.h>
#include <string.h>

#include "objstring.h"

#include "index.h"
#include "memory.h"
#include "number.h"
#include "vm.h"

bool is_character(char* string, int index) {
  // chars matching 10xxxxxx are part of an UTF-8 encoding
  // and do not count towards the number of characters in a string
  // Note: Special characters like \n, \s and \t count as characters in this context
  //printf("objstring:is_character() index=%d, byte=%x\n", index, (unsigned char) string[index]);
  return ((unsigned char) string[index] & 192) != 128;
}


int cp_length(char* string, int index) {
  int length = 1;
  while (!is_character(string, index + length)) { length++; }
  //printf("objstring:cp_length() index=%d, length=%d\n", index, length);
  return length;
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
  printf("objstring:count_substr() %s occurs %d times in %s\n", substr, count, string);
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
  ObjArray* res = newArray(vm);
  res->values = ALLOCATE(vm, Value, want_parts);
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
    res->length++; // Increase array length carefully because copyString may trigger GC
  }
  return res;
}


ObjArray* chars_to_array(VM* vm, const char* string, int want_parts) {
  if (want_parts == -1 || want_parts > (int) strlen(string)) want_parts = strlen(string);
  //printf("objstring:chars_to_array() string=%s want=%d\n", string, want_parts);
  ObjArray* res = newArray(vm);
  res->values = ALLOCATE(vm, Value, want_parts);
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
    res->length++; // Increase array length carefully because copyString may trigger GC
  }
  return res;
}


/*
// INTERNAL DEBUG FUNCTION
void dump_string(char* string, int length) {
  for (int i=0; i<length; i++) {
    printf("i=%x byte=%x char=%s\n", i, (unsigned char) string[i], (is_character(string, i) ? "true" : "false"));
  }
}
*/


// Native C method: STRING.value()
static bool string_value(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount > 1) {
    runtimeError(vm, "Method takes 1 optional argument, got %d.", argCount);
    return false;
  }
  int radix = 10;
  if (argCount == 1) {
    radix = (int) AS_NUMBER(args[0]);
  }
  ObjString* string = AS_STRING(receiver);

  *result = NUMBER_VAL(str_to_double(string->chars, string->length, radix));
  return true;
}


// Native C method: STRING.byte_at()
static bool string_byte_at(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount != 1) {
    runtimeError(vm, "Method takes 1 argument, got %d.", argCount);
    return false;
  }
  ObjString* string = AS_STRING(receiver);

  int offset = (IS_NULL(args[0]) ? 0 : (int) AS_NUMBER(args[0]));
  offset = check_offset(offset, string->length);
  if (offset == -1) { runtimeError(vm, "Index out of range."); return false; }

  ObjString* substr = copyString(vm, string->chars + offset, 1);

  *result = OBJ_VAL(substr);
  return true;
}


// Native C method: STRING.bytes_at()
static bool string_bytes_at(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount < 1 || argCount > 2) {
    runtimeError(vm, "Method takes 1 or 2 arguments, got %d.", argCount);
    return false;
  }
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
  if (argCount != 1) {
    runtimeError(vm, "Method takes 1 argument, got %d.", argCount);
    return false;
  }
  ObjString* string = AS_STRING(receiver);

  int offset = (IS_NULL(args[0]) ? 0 : (int) AS_NUMBER(args[0]));
  offset = check_offset(offset, string->length);
  if (offset == -1) { runtimeError(vm, "Index out of range."); return false; }

  int begin = -1;
  //int bytes = 1;
  for (int i=0; i<string->length; i++) {
    // Find the beginning of the requested codepoint
    if (is_character(string->chars, i)) begin++;
    if (begin == offset) {
      // Determine byte length of this codepoint
      ObjString* substr = copyString(vm, string->chars + i, cp_length(string->chars, i));
      *result = OBJ_VAL(substr);
      return true;
    }
  }

  // If we get this far, the scan failed
  runtimeError(vm, "Invalid index.");
  return false;
}


// Native C method: STRING.substr()
static bool string_substr(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount < 1 || argCount > 2) {
    runtimeError(vm, "Method takes 1 or 2 arguments, got %d.", argCount);
    return false;
  }
  ObjString* string = AS_STRING(receiver);

  int offset = (IS_NULL(args[0]) ? 0 : (int) AS_NUMBER(args[0]));
  //printf("pre check offset=%d string->length=%d\n", offset, string->length);
  offset = check_offset(offset, string->length);
  //printf("post check offset=%d\n", offset);
  if (offset == -1) { runtimeError(vm, "Offset %d out of range.", offset); return false; }

  int length = string->length - offset;
  if (argCount == 2) length = (int) AS_NUMBER(args[1]);
  length = check_length(length, offset, string->length);
  if (length == -1) { runtimeError(vm, "Length %d out of range.", length); return false; }
  if (length == 0) {
    // If calculated length is zero, return empty string
    ObjString* substr = copyString(vm, "", 0);
    *result = OBJ_VAL(substr);
    return true;
  }

  int begin = -1;
  int chars = 0;
  int bytes = 0;
  for (int i=0; i<string->length; i++) {
    // Find the beginning of the requested codepoint
    if (is_character(string->chars, i)) begin++;
    if (begin == offset) {
      while (chars < length && i+bytes < string->length) {
        bytes += cp_length(string->chars, i+bytes);
        chars++;
      }
      if (chars < length) { runtimeError(vm, "Length out of range."); return false; }

      ObjString* substr = copyString(vm, string->chars + i, bytes);
      *result = OBJ_VAL(substr);
      return true;
    }
  }

  // If we get this far, the scan failed
  runtimeError(vm, "Invalid index %d.", offset);
  return false;
}

// Native C method: STRING.split()
static bool string_split(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount < 1 || argCount > 2) {
    runtimeError(vm, "Method takes 1 or 2 arguments, got %d.", argCount);
    return false;
  }
  if (!IS_STRING(args[0])) {
    runtimeError(vm, "Argument 1 must be a string.");
    return false;
  }
  ObjString* delim = AS_STRING(args[0]);
  int want_parts = (argCount == 2 ? AS_NUMBER(args[1]) : -1); // Default (-1) is unlimited
  if (want_parts == 0) {
    // Wants zero length, gets zero length
    *result = OBJ_VAL(newArray(vm));
    return true;
  }

  ObjString* string = AS_STRING(receiver);
  printf("objstring:string_split() string=%s delim=%s want=%d\n", string->chars, delim->chars, want_parts);

  // Special case: If length of delimiter is zero, split each char
  if (strlen(delim->chars) == 0) {
    *result = OBJ_VAL(chars_to_array(vm, string->chars, want_parts));
    return true;
  }

  // Count how many times delimiter occurs in string
  int have_parts = count_substr(string->chars, delim->chars) + 1;
  printf("objstring:string_split() have=%d\n", have_parts);
  if (want_parts == -1 || want_parts > have_parts) want_parts = have_parts;



  *result = OBJ_VAL(split_string(vm, string->chars, delim->chars, want_parts));
  return true;

}


// Native C method: STRING.f()
// Allows the user manual control over %d string formatting
static bool string_format(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount != 1 || !IS_STRING(args[0])) {
    runtimeError(vm, "Method needs 1 string argument.");
    return false;
  }
  char* format = AS_CSTRING(args[0]);
  int length = snprintf(NULL, 0, format, AS_CSTRING(receiver));
  char* string = ALLOCATE(vm, char, length + 1);
  length = snprintf(string, length, format, AS_CSTRING(receiver));

  *result = OBJ_VAL(copyString(vm, string, length));
  return true;
}


bool stringProperty(void* vm, Value receiver, ObjString* name) {
  ObjString* string = AS_STRING(receiver);
  Value result;
  if (strcmp(name->chars, "bytes")==0) {
    // Return the string length in number of bytes (storage size)
    result = NUMBER_VAL(string->length);
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "chars")==0) {
    // Return the string length in number of characters (print size)
    int count = 0;
    for (int i=0; i<string->length; i++) {
      if (is_character(string->chars, i)) count++;
    }
    result = NUMBER_VAL(count);
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "value")==0) {
    // Return the numeric value of the string in the specified base
    result = OBJ_VAL(newNativeMethod(vm, receiver, string_value));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "byte_at")==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, string_byte_at));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "bytes_at")==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, string_bytes_at));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "char_at")==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, string_char_at));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "substr")==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, string_substr));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "split")==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, string_split));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "f")==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, string_format));
    pop(vm);
    push(vm, result);
    return true;
  }

  runtimeError(vm, "Undefined property '%s'.", name->chars);
  return false;
}


