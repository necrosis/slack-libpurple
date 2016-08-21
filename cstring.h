#ifndef _CSTRING_H
#define _CSTRING_H
#include <stdlib.h>

typedef struct _string {
  char *ptr;
  size_t len;
} string;


#define NULL_STRING { .ptr = NULL, .len = 0 }

int create_string(string* str, int length);
void free_string(string* str);
#endif
