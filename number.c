#include "number.h"

#include <math.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// For a "digit" 0..z (or 0..Z) return decimal value 0..36
// Note: We support every radix from base 2 to base 36
int digit_value(const char c) {
  if (c>='0' && c<='9') return (int) c-48;
  if (c>='A' && c<='Z') return (int) c-'A'+10;
  if (c>='a' && c<='z') return (int) c-'a'+10;
  return -1; // Should be unreachable
}


// For a decimal value 0..36 return "digit" 0..z
// Note: We support every radix from base 2 to base 36
char value_digit(int n) {
  //printf("number:value_digit() value=%d\n", n);
  if (n>=0 && n<=9) return '0'+n;
  if (n>=10 && n<=36) return 'a'+n-10;
  return '?'; // Should be unreachable
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


// Calculate integer exponentials by simple looped multiplication
// Note: Will overflow and return 0 for large values
uint64_t power(uint64_t base, unsigned int exp) {
  uint64_t n = base;
  if (exp == 0) return 1;
  for (unsigned int i=0; i<exp-1; i++) {
    n = n * base;
    //printf("number:power() base=%" PRIu64 " exp=%u loop=%d result=%" PRIu64 "\n", base, exp, i, n);
  }
  return n;
}



// Return 1 if str contains a decimal point
// Note: String must be terminated!
int has_decimals(char* str) {
  int i = 0;
  while (str[i] != '\0') {
    if (str[i] == '.') return 1;
    i++;
  }
  return 0;
}

// Given the address of a pointer to a string we assume contains a decimal number,
// modify the pointer so it points to a string with no trailing zeroes
// If this means removing all decimals, remove the decimal point as well.
// "123.450000" => "123.45"
// "123.000000" => "123"
// Note: String must be terminated!
void trim_trailing_zeroes(char** str) {
  int oldlen = strlen(*str);
  int newlen = oldlen;
  while ((*str)[newlen-1] == '0') newlen--; // Trim until we encounter a digit other than zero
  if ((*str)[newlen-1] == '.') newlen--; // Don't leave a dangling decimal point
  // Modify the string if needed
  if (newlen < oldlen) {
    *str = realloc(*str, newlen*sizeof(char) + 1); // newlen always shorter; realloc can't fail
    (*str)[newlen] = '\0'; // Terminate
  }
}

// Given the address of a pointer to a string we assume contains a number,
// modify the pointer so it points to a string with no leading zeroes.
// Exception: If the last digit is a zero, return "0" rather than "".
// "00000123" => "123"
// "00000000" => "0"
// Note: String must be terminated!
void trim_leading_zeroes(char** str) {
  //printf("number:trim_leading_zeroes() %p->%s\n", *str, *str);
  int length = strlen(*str);
  //printf("number:trim_leading_zeroes() length=%d\n", length);
  int offset = 0;
  while ((*str)[offset] == '0') offset++; // Scan forward until we find a digit (or string terminator)
  //printf("number:trim_leading_zeroes() offset=%d\n", offset);
  if (offset > 0) {
    // We found one or more leading zero.
    // Allocate a new char* buffer, then copy the chars we want to keep
    if (offset == length) offset--;
    int newlen = length - offset;
    char* tmp = malloc(newlen*sizeof(char) + 1);
    strncpy(tmp, (*str)+offset, newlen);
    tmp[newlen] = '\0';
    // We can now dispose of the original string and let the caller use the new buffer
    free(*str);
    *str = tmp;
  }
}

// Allocates a buffer big enough to represent a floating point number as a string
// Note: Returns 6 decimals and terminates the string
int double_to_str_dec(double number, char** str) {
  int length = snprintf(NULL, 0, "%f", number);
  *str = malloc(length*sizeof(char) + 1);
  length = snprintf(*str, length, "%f", number);
  (*str)[length] = '\0'; // Terminate
  if (has_decimals(*str)) trim_trailing_zeroes(str);
  return strlen(*str); // trimming may have changed the length
}

int double_to_str(double number, char** str, int radix) {
  if (radix < 2 || radix > 36) {
    *str = ""; // Nonsense/unsupported, return empty string
    return 0;
  }
  if (radix == 10) return double_to_str_dec(number, str);

  // For all other radices, treat the number as an unsigned 64bit integer
  // This is not actually true as a double only has 52 bits of precision,
  // hence this will be the precision of our output as well.
  // On top of this comes some rather unpleasant behaviour when doing
  // division and multiplication on numbers larger than 32 bit,
  // which means this code is basically riddled with bugs and needs to be
  // rewritten using some bigint library.
  uint64_t max = 0x8000000000000-1;
  uint64_t n = (uint64_t) number & max;
  uint64_t place;
  uint64_t value;
  int bits = 52;
  // Convert number to string using a generic algorithm (slow but flexible)
  //printf("number:double_to_string() n=%I64d radix=%d\n", n, radix);
  char* buf = malloc(bits*sizeof(char) + 1);
  for (int i=0; i<bits; i++) {
    //printf("number:double_to_string() i=%d\n", i);
    place = power(radix, bits-i-1); // For radices > 2, this will overflow and return 0!
    if ((place & max) > 0) {
      //printf("number:double_to_string() n=%" PRIu64 " place=%d^%d=%" PRIu64 "\n", n, radix, bits-i-1, place);
      value = n / place;
      buf[i] = value_digit(value);
      n -= value * place;
      //printf("number:double_to_string() n=%" PRIu64 "\n", n);
    } else {
      buf[i] = '0';
    }
  }
  buf[bits] = '\0';

  trim_leading_zeroes(&buf);
  *str = buf;
  return strlen(*str);
}

