#include "cstring.h"
#include <stdlib.h>
#include <string.h>


int create_string(string* str, int length)
{
	if (str->ptr)
		free_string(str);

	if (length >= 0)
	{
		str->len = length;
		str->ptr = malloc(length + 1);

		if (str->ptr != NULL)
			return length;
	} 
		
	return 0;
}


void free_string(string* str)
{
	if (str->ptr)
	{
		free(str->ptr);
		str->len = 0;
		str->ptr= NULL;
	}
}
