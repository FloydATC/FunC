#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "file.h"


int readFile(const char* path, char** buffer) {
  printf("file:readFile() path=%s\n", path);
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("Current working dir: %s\n", cwd);
    }
    return(-74);
  }
  printf("file:readFile() open successful, seek to end\n");

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  printf("file:readFile() file size is %d bytes, rewinding\n", (int)fileSize);
  rewind(file);

  printf("file:readFile() allocating buffer\n");
  *buffer = (char*)malloc(fileSize + 1);
  if (*buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    return(-74);
  }
  printf("file:readFile() reading file\n");
  size_t bytesRead = fread(*buffer, sizeof(char), fileSize, file);
  printf("file:readFile() read %d bytes\n", (int) bytesRead);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    free(*buffer);
    *buffer = NULL;
    return(-74);
  }
  printf("file:readFile() terminating buffer at %p\n", (*buffer+bytesRead));
  (*buffer)[bytesRead] = '\0';

  fclose(file);
  printf("file:readFile() file closed\n");
  return bytesRead;
}

