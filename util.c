#include "util.h"
#include <string.h>
#include <glib.h>

gchar* 
remove_char(
		const gchar* str, 
		gchar c
)
{
	int lng = strlen(str);

	int amount = 0;
	for (int i = 0; i < lng; i++)
	{
		if (str[i] == c)
			amount += 1;
	}

	if (!amount)
		return g_strdup(str);


	if (lng == amount)
		return NULL;

	gchar* r = g_new0(gchar, lng - amount);

	for (int i = 0, j = 0; i < lng; i++)
	{
		if (str[i] != c)
			r[j++] = str[i];
	}

	return r;
}
