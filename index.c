#include "index.h"


int check_offset(const int input, const int maxlen) {
  int offset = input;
  if (offset < 0) offset = maxlen + offset; // Negative offset = count from right hand side
  if (offset < 0) return -1; // Still negative? Return -1 to signal "invalid"
  if (offset >= maxlen) return -1; // Beyond end? Return -1 to signal "invalid"
  return offset;
}


int check_length(const int input, const int offset, const int maxlen) {
  int length = input;
  if (length < 0) length = maxlen - offset + length; // Negative length = reduce actual length
  if (length < 0) return -1; // Still negative? Return -1 to signal "invalid"
  if (length > maxlen - offset) return -1; // Beyond end? Return -1 to signal "invalid"
  return length;
}
