#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"

// Printf appending into a flexible zero terminated buffer
char* vbprintf(char* buf, const char* format, va_list argp) {
  int bufsiz = 0;
  if (buf != NULL) { bufsiz = strlen(buf); }
  printf("error:vbprintf() buf=%p bufsiz in=%d\n", buf, bufsiz);

  //va_list args;
  //va_start(args, format);
  int addsiz = vsnprintf(NULL, 0, format, argp);
  if (buf == NULL) {
    buf = malloc((addsiz+1)*sizeof(char));
  } else {
    buf = (char*) realloc(buf, (bufsiz+addsiz+1)*sizeof(char));
  }
  vsnprintf(buf+bufsiz, addsiz, format, argp);
  //va_end (args);
  buf[bufsiz+addsiz] = '\0'; // Terminate

  printf("error:vbprintf() buf=%p bufsiz out=%d\n", buf, bufsiz+addsiz);
  return buf;
}

char* bprintf(char* buf, const char* format, ...)
{
  printf("error:bprintf() buf=%p format=%s\n", buf, format);
	va_list argp;
	va_start(argp, format);
	buf = vbprintf(buf, format, argp);
	va_end(argp);
  printf("error:bprintf() returning buf=%p\n", buf);
	return buf;
}

