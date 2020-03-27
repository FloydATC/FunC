
#include "value.h"

int readFile(const char* path, char** buffer);

// Experimental include support
int addFilename(void* vm, const char* filename);
int getFilenoByName(void* vm, const char* filename);
Value getFilenameByNo(void* vm, int id);
