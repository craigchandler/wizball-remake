#include "path_utils.h"

#include <stdio.h>

void PATH_UTIL_build_relative_path(char *out_path, int out_size, const char *subdir, const char *filename)
{
	if ((out_path == 0) || (out_size <= 0))
	{
		return;
	}

	if (subdir == 0)
	{
		subdir = "";
	}
	if (filename == 0)
	{
		filename = "";
	}

	if (subdir[0] == '\0')
	{
		snprintf(out_path, out_size, "%s", filename);
	}
	else
	{
		snprintf(out_path, out_size, "%s/%s", subdir, filename);
	}
	out_path[out_size - 1] = '\0';
}
