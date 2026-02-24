#include "version.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <allegro.h>
#include <ctype.h>
#include <assert.h>

#include "main.h"
#include "string_size_constants.h"
#include "output.h"

#include "fortify.h"

#ifdef ALLEGRO_MACOSX
	struct al_ffblk finder;
	char file_pattern[NAME_SIZE];
#else
	#include <io.h>
	#include <sys\stat.h>

	_finddata_t finder;
#endif

	

char caCurrentDir[NAME_SIZE];
long hFiles;

FILE *config_file = NULL;



void FILE_start_config_file (void)
{
	if (config_file == NULL)
	{
		config_file = fopen(MAIN_get_project_filename("config.txt", true) , "w");
	}
	else
	{
		OUTPUT_message("Trying to open 'config.txt' repeatedly");
		assert(0);
	}
}



void FILE_add_line_to_config_file (char *line)
{
	if (config_file != NULL)
	{
		fputs(line,config_file);
		fputc('\n',config_file);
	}
	else
	{
		OUTPUT_message("Trying to write to 'config.txt' before opening it.");
	}
}



void FILE_end_config_file (void)
{
	if (config_file != NULL)
	{
		fputs("#END OF FILE\n",config_file);
		fclose(config_file);
		config_file = NULL;
	}
	else
	{
		OUTPUT_message("Trying to close 'config.txt' without opening it.");
	}
}



void FILE_put_string_to_file (FILE *file_pointer, char *word, bool flush)
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
		sprintf(line,"");
	}
	
	if (strlen(word) > 0)
	{
		// Check that we've actually passed some data and aren't just called the routine to flush the remainder out to the file.

		if (strlen(word) + strlen(line) + 2 >= TEXT_LINE_SIZE) // The +2 is for the comma and the newline
		{
			// It ain't gonna' fit onto the line then flush it out to the file...
			strcat(line,"\n");
			fputs(line,file_pointer);
			// And then just put the word onto the line on it's own.
			strcpy(line,word);
		}
		else if (strlen(line) == 0)
		{
			// There's nothing on the line yet so don't append a comma and word, just stick the word on the line.
			strcpy(line,word);
		}
		else
		{
			// Just add the word to the line and a lovely comma to boot.
			strcat(line,",");
			strcat(line,word);
		}
	}

	if (flush == true)
	{
		// This is the last bit of data for this section so just flush it out.
		strcat(line,"\n");
		fputs(line,file_pointer);
		sprintf(line,"");
	}

}



void EDIT_put_int_to_file (FILE *file_pointer, int value, bool flush)
{
	static char word[TEXT_LINE_SIZE];
	sprintf(word,"%d",value);
	FILE_put_string_to_file (file_pointer, word, flush);
}



void EDIT_put_float_to_file (FILE *file_pointer, float value, bool flush)
{
	static char word[TEXT_LINE_SIZE];
	sprintf(word,"%6.6f",value);
	FILE_put_string_to_file (file_pointer, word, flush);
}



void FILE_reset_to_file (FILE *file_pointer)
{
	FILE_put_string_to_file (file_pointer, "", true);
}



void FILE_get_data_from_file (FILE *file_pointer, char *word, bool reset)
{
	// This grabs a line from the given file and then gradually digests it until all the values
	// from it have been used up, whereupon it gets another value. Lubbly.

	static char line[TEXT_LINE_SIZE];
	static char *pointer = NULL;

	if (reset == false)
	{
		if ( pointer == NULL )
		{
			// Get some more data from the file...
			fgets(line,TEXT_LINE_SIZE,file_pointer);
			// And set the pointer to the first number in it...
			pointer = strtok(line,",\r\t\n");
		}
		if (pointer!=NULL)
		strcpy (word,pointer);
		else {
		strcpy(word, "");
		
		}

		pointer = strtok(NULL,",\r\t\n");
	}
	else
	{
		pointer = NULL;
	}
}



void FILE_reset_from_file (FILE *file_pointer)
{
	FILE_get_data_from_file (file_pointer, "", true);
}



void FILE_get_string_from_file (FILE *file_pointer, char *word)
{
	FILE_get_data_from_file (file_pointer, word , false);
}



float FILE_get_float_from_file (FILE *file_pointer)
{
	static char word[TEXT_LINE_SIZE];
	float value;

	FILE_get_data_from_file (file_pointer, word , false);

	value = float(atof(word));

	return value;
}



int FILE_get_int_from_file (FILE *file_pointer)
{
	static char word[TEXT_LINE_SIZE];
	int value;

	FILE_get_data_from_file (file_pointer, word, false);

	value = atoi(word);

	return value;
}


#ifdef ALLEGRO_MACOSX



static bool match_ext(const char* filename, const char* ext) 
{
  int flen = strlen(filename);
  int elen = strlen(ext);
  if (elen > flen) return false;
  return strcasecmp(filename + flen - elen, ext) == 0;
}



char * FILE_open_dir (char *dirname, char *extension, bool capitalise)
{
	if (strcmp(dirname,"")!=0)
	{
		append_filename(caCurrentDir, dirname, "*.*", sizeof(caCurrentDir));
	}
	else
	{
		sprintf(caCurrentDir, "*.*");
	}
	strcpy(file_pattern, extension);
	int found = al_findfirst(caCurrentDir, &finder, FA_ARCH);
	while (found == 0) 
	  {
	    if (match_ext(finder.name, file_pattern))
	      {
		if (capitalise) strupr (finder.name);
		return finder.name;
	      }
	    found = al_findnext(&finder);
	  }
	
	al_findclose(&finder);
	return NULL;
}



char * FILE_read_dir_entry (bool capitalise)
{
	int found = al_findnext(&finder);
	while (found == 0)
          {
            if (match_ext(finder.name, file_pattern))
              {
                if (capitalise) strupr (finder.name);
                return finder.name;
              }
            found = al_findnext(&finder);
          }

        al_findclose(&finder);
        return NULL;
}

#else

char * FILE_open_dir (char *dirname, char *extension, bool capitalise)
{
	if (strcmp(dirname,"")!=0)
	{
		sprintf(caCurrentDir, "%s\\*%s", dirname, extension);
	}
	else
	{
		sprintf(caCurrentDir, "*%s", extension);
	}

	hFiles = _findfirst(caCurrentDir, &finder);

	if (hFiles != -1)
	{
		strupr (finder.name);
		return (&finder.name[0]);;
	}
	else
	{
		return NULL;
	}
}



char * FILE_read_dir_entry (bool capitalise)
{
	int result;

	do
	{
		result = _findnext(hFiles, &finder);
	} while ( (strcmp(".", finder.name) == 0) || (strcmp("..", finder.name) == 0) );

	if (result != -1)
	{
		if (capitalise)
		{
			strupr (finder.name);
		}

		return (&finder.name[0]);
	}
	else
	{
		return NULL;
	}

}

#endif



