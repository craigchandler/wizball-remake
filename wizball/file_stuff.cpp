#include "version.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "main.h"
#include "string_size_constants.h"
#include "output.h"

#include "fortify.h"

#if !defined(_WIN32)
#include <dirent.h>
#include <strings.h> // for strcasecmp on Linux/macOS
#else
#include <io.h>
#include <sys/stat.h>
static _finddata_t finder;
#endif
#include "string_stuff.h"

static char file_pattern[NAME_SIZE];

#if !defined(_WIN32)
static DIR *g_dir = NULL;
static char g_dirname[NAME_SIZE];
static char g_entry_name[NAME_SIZE];
#endif

char caCurrentDir[NAME_SIZE];
long hFiles;

FILE *config_file = NULL;

void FILE_start_config_file(void)
{
	if (config_file == NULL)
	{
		config_file = fopen(MAIN_get_project_filename("config.txt", true), "w");
	}
	else
	{
		OUTPUT_message("Trying to open 'config.txt' repeatedly");
		assert(0);
	}
}

void FILE_add_line_to_config_file(char *line)
{
	if (config_file != NULL)
	{
		fputs(line, config_file);
		fputc('\n', config_file);
	}
	else
	{
		OUTPUT_message("Trying to write to 'config.txt' before opening it.");
	}
}

void FILE_end_config_file(void)
{
	if (config_file != NULL)
	{
		fputs("#END OF FILE\n", config_file);
		fclose(config_file);
		config_file = NULL;
	}
	else
	{
		OUTPUT_message("Trying to close 'config.txt' without opening it.");
	}
}

void FILE_put_string_to_file(FILE *file_pointer, char *word, bool flush)
{
	// This attempts to add the parsed word to a static char within the function but
	// if it will make it longer than a predefined size it dumps the line out to a
	// file anyway.

	// If flush is true then it will send the line to a file anyway.

	static char line[TEXT_LINE_SIZE];
	static bool first_time = true;

	if (first_time == true)
	{
		first_time = false;
		line[0] = '\0';
	}

	if (strlen(word) > 0)
	{
		// Check that we've actually passed some data and aren't just called the routine to flush the remainder out to the file.

		if (strlen(word) + strlen(line) + 2 >= TEXT_LINE_SIZE) // The +2 is for the comma and the newline
		{
			// It ain't gonna' fit onto the line then flush it out to the file...
			strcat(line, "\n");
			fputs(line, file_pointer);
			// And then just put the word onto the line on it's own.
			strcpy(line, word);
		}
		else if (strlen(line) == 0)
		{
			// There's nothing on the line yet so don't append a comma and word, just stick the word on the line.
			strcpy(line, word);
		}
		else
		{
			// Just add the word to the line and a lovely comma to boot.
			strcat(line, ",");
			strcat(line, word);
		}
	}

	if (flush == true)
	{
		// This is the last bit of data for this section so just flush it out.
		strcat(line, "\n");
		fputs(line, file_pointer);
		line[0] = '\0';
	}
}

void EDIT_put_int_to_file(FILE *file_pointer, int value, bool flush)
{
	static char word[TEXT_LINE_SIZE];
	snprintf(word, sizeof(word), "%d", value);
	FILE_put_string_to_file(file_pointer, word, flush);
}

void EDIT_put_float_to_file(FILE *file_pointer, float value, bool flush)
{
	static char word[TEXT_LINE_SIZE];
	snprintf(word, sizeof(word), "%6.6f", value);
	FILE_put_string_to_file(file_pointer, word, flush);
}

void FILE_reset_to_file(FILE *file_pointer)
{
	FILE_put_string_to_file(file_pointer, "", true);
}

void FILE_get_data_from_file(FILE *file_pointer, char *word, size_t word_size, bool reset)
{
	// This grabs a line from the given file and then gradually digests it until all the values
	// from it have been used up, whereupon it gets another value. Lubbly.

	static char line[TEXT_LINE_SIZE];
	static char *pointer = NULL;

	if (reset == false)
	{
		if (pointer == NULL)
		{
			// Get some more data from the file...
			fgets(line, TEXT_LINE_SIZE, file_pointer);
			// And set the pointer to the first number in it...
			pointer = strtok(line, ",\r\t\n");
		}
		if (pointer != NULL)
			strncpy(word, pointer, word_size - 1), word[word_size - 1] = '\0';
		else
		{
			if (word_size > 0)
				word[0] = '\0';
		}

		pointer = strtok(NULL, ",\r\t\n");
	}
	else
	{
		pointer = NULL;
	}
}

void FILE_reset_from_file(FILE *file_pointer)
{
	FILE_get_data_from_file(file_pointer, (char *)"", 0, true);
}

void FILE_get_string_from_file(FILE *file_pointer, char *word, size_t word_size)
{
	FILE_get_data_from_file(file_pointer, word, word_size, false);
}

float FILE_get_float_from_file(FILE *file_pointer)
{
	static char word[TEXT_LINE_SIZE];
	float value;

	FILE_get_data_from_file(file_pointer, word, sizeof(word), false);

	value = float(atof(word));

	return value;
}

int FILE_get_int_from_file(FILE *file_pointer)
{
	static char word[TEXT_LINE_SIZE];
	int value;

	FILE_get_data_from_file(file_pointer, word, sizeof(word), false);

	value = atoi(word);

	return value;
}

void lowercase_last_path_components(const char *in_path, char *out_path, size_t out_size, int components)
{
	size_t i;
	size_t len;
	int component_count = 0;
	size_t start = 0;

	if ((out_size == 0) || (in_path == NULL) || (out_path == NULL))
	{
		return;
	}

	strncpy(out_path, in_path, out_size - 1);
	out_path[out_size - 1] = '\0';

	len = strlen(out_path);
	for (i = len; i > 0; i--)
	{
		if ((out_path[i - 1] == '/') || (out_path[i - 1] == '\\'))
		{
			component_count++;
			if (component_count == components)
			{
				start = i;
				break;
			}
		}
	}

	if (component_count < components)
	{
		start = 0;
	}

	for (i = start; i < len; i++)
	{
		out_path[i] = (char)tolower((unsigned char)out_path[i]);
	}
}

#if !defined(_WIN32)

static bool match_ext(const char *filename, const char *ext)
{
	if (!filename || !ext)
		return false;

	int flen = (int)strlen(filename);
	int elen = (int)strlen(ext);

	if (flen == 0 || elen == 0)
		return false;

	// If ext does not start with '.', treat it as if it does.
	bool ext_has_dot = (ext[0] == '.');
	int required_len = ext_has_dot ? elen : (elen + 1);

	if (required_len > flen)
		return false;

	const char *file_ext = filename + flen - required_len;

	// If ext did not include '.', ensure filename actually has one.
	if (!ext_has_dot)
	{
		if (file_ext[0] != '.')
			return false;

		// Compare after the dot
		return strcasecmp(file_ext + 1, ext) == 0;
	}
	else
	{
		return strcasecmp(file_ext, ext) == 0;
	}
}

static bool is_dot_or_dotdot(const char *name)
{
	return (strcmp(name, ".") == 0) || (strcmp(name, "..") == 0);
}

static const char *next_matching_entry(bool capitalise)
{
	if (!g_dir)
		return NULL;

	struct dirent *ent = NULL;
	while ((ent = readdir(g_dir)) != NULL)
	{
		const char *name = ent->d_name;
		if (is_dot_or_dotdot(name))
			continue;

		if (!match_ext(name, file_pattern))
			continue;

		// Copy into stable buffer (dirent->d_name storage is reused by readdir)
		strncpy(g_entry_name, name, sizeof(g_entry_name) - 1);
		g_entry_name[sizeof(g_entry_name) - 1] = '\0';

		if (capitalise)
			STRING_uppercase(g_entry_name);

		return g_entry_name;
	}

	return NULL;
}

char *FILE_open_dir(char *dirname, char *extension, bool capitalise)
{
	// Close any previous enumeration
	if (g_dir)
	{
		closedir(g_dir);
		g_dir = NULL;
	}

	if (!dirname)
		dirname = (char *)"";
	if (!extension)
		extension = (char *)"";

	strncpy(file_pattern, extension, sizeof(file_pattern) - 1);
	file_pattern[sizeof(file_pattern) - 1] = '\0';

	// Store current directory for possible debugging/use
	strncpy(g_dirname, dirname, sizeof(g_dirname) - 1);
	g_dirname[sizeof(g_dirname) - 1] = '\0';

	// Try open requested directory ("" means current dir)
	const char *open_dir = (dirname[0] != '\0') ? dirname : ".";
	g_dir = opendir(open_dir);

	// Case fallback: try lowercasing last path component if open fails
	if (!g_dir && dirname[0] != '\0')
	{
		char fallback_dir[NAME_SIZE];
		lowercase_last_path_components(dirname, fallback_dir, sizeof(fallback_dir), 1);
		if (strcmp(fallback_dir, dirname) != 0)
			g_dir = opendir(fallback_dir);
	}

	if (!g_dir)
		return NULL;

	const char *found = next_matching_entry(capitalise);
	return found ? g_entry_name : NULL;
}

char *FILE_read_dir_entry(bool capitalise)
{
	const char *found = next_matching_entry(capitalise);
	if (found)
		return g_entry_name;

	if (g_dir)
	{
		closedir(g_dir);
		g_dir = NULL;
	}
	return NULL;
}

#else

char *FILE_open_dir(char *dirname, char *extension, bool capitalise)
{
	if (strcmp(dirname, "") != 0)
	{
		snprintf(, sizeof(), "%s\\*%s", dirname, extension);
	}
	else
	{
		snprintf(, sizeof(), "*%s", extension);
	}

	hFiles = _findfirst(caCurrentDir, &finder);

	if (hFiles != -1)
	{
		STRING_uppercase(finder.name);
		return (&finder.name[0]);
		;
	}
	else
	{
		return NULL;
	}
}

char *FILE_read_dir_entry(bool capitalise)
{
	int result;

	do
	{
		result = _findnext(hFiles, &finder);
	} while ((strcmp(".", finder.name) == 0) || (strcmp("..", finder.name) == 0));

	if (result != -1)
	{
		if (capitalise)
		{
			STRING_uppercase(finder.name);
		}

		return (&finder.name[0]);
	}
	else
	{
		return NULL;
	}
}

#endif

char *FILE_append_filename(char *dest,
													 const char *path,
													 const char *filename,
													 int size)
{
	if (!dest || size <= 0)
		return dest;

	dest[0] = '\0';

	if (!path)
		path = "";
	if (!filename)
		filename = "";

	if (path[0] == '\0')
	{
		snprintf(dest, (size_t)size, "%s", filename);
		dest[size - 1] = '\0';
		return dest;
	}

	snprintf(dest, (size_t)size, "%s", path);
	dest[size - 1] = '\0';

	size_t len = strlen(dest);
	if (len > 0)
	{
		char last = dest[len - 1];
		if (last != '/' && last != '\\')
		{
#if defined(_WIN32)
			strncat(dest, "\\", (size_t)size - 1 - len);
#else
			strncat(dest, "/", (size_t)size - 1 - len);
#endif
		}
	}

	while (*filename == '/' || *filename == '\\')
		filename++;
	strncat(dest, filename, (size_t)size - 1 - strlen(dest));
	dest[size - 1] = '\0';

	return dest;
}

void FILE_fix_filename_slashes(char *path)
{
	if (!path)
		return;

#if defined(_WIN32)
	const char native = '\\';
	const char other = '/';
#else
	const char native = '/';
	const char other = '\\';
#endif

	while (*path)
	{
		if (*path == other)
			*path = native;
		++path;
	}
}

void FILE_put_backslash(char *path, size_t size)
{
	if (!path || size == 0)
		return;

	size_t len = strlen(path);
	if (len == 0)
		return;

	char last = path[len - 1];

	if (last == '/' || last == '\\')
		return;

	if (len + 1 >= size)
		return; // no space to append

#if defined(_WIN32)
	path[len] = '\\';
#else
	path[len] = '/';
#endif

	path[len + 1] = '\0';
}

FILE* FILE_open_case_fallback(const char* filename, const char* mode)
{
    if (!filename || !mode)
        return NULL;

    FILE* file_pointer = fopen(filename, mode);
    if (file_pointer)
        return file_pointer;

#if !defined(_WIN32)
    // Try lowercasing the last 1 path component (filename)
    char lower_filename[TEXT_LINE_SIZE];

    lowercase_last_path_components(filename, lower_filename, sizeof(lower_filename), 1);
    if (strcmp(lower_filename, filename) != 0)
    {
        file_pointer = fopen(lower_filename, mode);
        if (file_pointer)
            return file_pointer;
    }

    // Try lowercasing the last 2 path components (dir + filename)
    lowercase_last_path_components(filename, lower_filename, sizeof(lower_filename), 2);
    if (strcmp(lower_filename, filename) != 0)
    {
        file_pointer = fopen(lower_filename, mode);
        if (file_pointer)
            return file_pointer;
    }
#endif

    return NULL;
}

FILE* FILE_open_read_case_fallback(const char* filename)
{
    return FILE_open_case_fallback(filename, "r");
}

FILE* FILE_open_project_case_fallback(const char* relative_filename, const char* mode)
{
    if (!relative_filename)
        return NULL;

    return FILE_open_case_fallback(MAIN_get_project_filename((char*)relative_filename), mode);
}

FILE* FILE_open_project_read_case_fallback(const char* relative_filename)
{
    return FILE_open_project_case_fallback(relative_filename, "r");
}