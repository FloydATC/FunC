#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

// Printf appending into a flexible zero terminated buffer
char* vbprintf(char* buf, const char* format, va_list argp) {
  int bufsiz = 0;
  if (buf != NULL) { bufsiz = strlen(buf); }
  //printf("vbprintf() buf=%p bufsiz in=%d\n", buf, bufsiz);

  //va_list args;
  //va_start(args, format);
  int addsiz = vsnprintf(NULL, 0, format, argp) + 1;
  if (buf == NULL) {
    buf = malloc(addsiz*sizeof(char));
  } else {
    buf = (char*) realloc(buf, (bufsiz+addsiz)*sizeof(char));
  }
  vsnprintf(buf+bufsiz, addsiz, format, argp);
  //va_end (args);
  buf[bufsiz+addsiz] = '\0'; // Terminate

  //printf("vbprintf() buf=%p bufsiz out=%d\n", buf, bufsiz+addsiz);
  return buf;
}

char* bprintf(char* buf, const char *format, ...)
{
	va_list argp;
	va_start(argp, format);
	buf = vbprintf(buf, format, argp);
	va_end(argp);
	return buf;
}

