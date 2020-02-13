#include "number.h"

#include <stdio.h>

int digit_value(const char c) {
  if (c>='0' && c<='9') return (int) c-48;
  if (c>='A' && c<='Z') return (int) c-'A'+10;
  if (c>='a' && c<='z') return (int) c-'a'+10;
  return -1;
}

double str_to_fraction(const char* str, int maxlen, int radix) {
  double result = 0;
  double factor = 1;
  for (int i=0; i<maxlen; i++) {
    int n = digit_value(str[i]);
    if (n>=0 && n<radix) {
      factor /= radix;
      result += n * factor;
      continue;
    }
    break; // Invalid character encountered
  }
  return result;
}

double str_to_double(const char* str, int maxlen, int radix) {
  if (radix < 2 || radix > 36) return 0; // Nonsense/unsupported
  char negative = 0;
  double result = 0;
  for (int i=0; i<maxlen; i++) {
    if (i == 0 && str[i]=='-') { negative = 1; continue; }
    int n = digit_value(str[i]);
    //printf("number:str_to_double() digit = %d\n", n);
    if (n>=0 && n<radix) {
      result *= radix;
      result += n;
      continue;
    }
    if (str[i]=='.') result += str_to_fraction(str+i+1, maxlen-i, radix);
    break; // Invalid character encountered
  }
  return (negative==1 ? result * -1 : result);
}

int double_to_str(double number, int maxlen, char** buf, int radix) {
  int length = 0;
  *buf = NULL;
  return length;
}
