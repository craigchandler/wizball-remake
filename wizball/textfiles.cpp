#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "textfiles.h"
#include "global_param_list.h"
#include "main.h"
#include "output.h"
#include "allegro.h"

#include "fortify.h"



// This file contains all the functions which pertain to the loading and returning of
// textfiles.

textfile_line *tx=NULL;
int textfile_line_count = 0;

char error_space[TEXT_LINE_SIZE];



int TEXTFILE_get_text_length (int text_tag)
{
	return (tx[text_tag].line_size - 1); // It's -1 because of the '\0' on the end.
}



void TEXTFILE_load_text (void)
{
	char file_text_line[TEXTFILE_SUPER_SIZE];
	int start,end;
	int big_start,big_end;
	int file_number;
	int line_length;

	char filename[NAME_SIZE];
	char *pointer_1;
	char *pointer_2;

	char error[TEXT_LINE_SIZE];

	int counter;

	FILE *file_pointer = NULL;

	// This goes through the appropriate GPL getting the files which contain
	// text. It then gets the first word in each line of those files and if they
	// aren't blank or a comment then it stores the tag and then skips any intervening
	// tabs and spaces then stores the line.

	GPL_list_extents ("TEXTFILES", &start, &end);

	GPL_list_extents ("TEXT", &big_start, &big_end);

	// Malloc enough room for all the text tags.

	textfile_line_count = big_end - big_start;

	tx = (textfile_line *) malloc ( textfile_line_count * sizeof(textfile_line) );

	counter = 0;

	if (start != UNSET)
	{
		// There are some textfiles! Hurrah!

		for (file_number=start; file_number<end; file_number++)
		{
			// Create a valid filename from the directory entry...
			sprintf(filename, "%s%s", GPL_what_is_word_name(file_number), GPL_what_is_list_extension("TEXTFILES") );
			append_filename(filename, "TEXTFILES", filename, sizeof (filename) );
			
			// Then open it and read it just like with the SOURCE_FILE stuff
			file_pointer = fopen (MAIN_get_project_filename(filename),"r");

			if (file_pointer != NULL)
			{
				while ( fgets ( file_text_line , TEXTFILE_SUPER_SIZE , file_pointer ) != NULL )
				{
					pointer_1 = strtok (file_text_line,"\r\n\t "); // Points to tag
					pointer_2 = strtok (NULL,"\r\n"); // Points to tag
					if (pointer_2 != NULL)
					{
						while ( (*pointer_2 == '\t') || (*pointer_2 == ' ') )
						{
							pointer_2++;
						}
					}

					// If it's not just an emtpy line or a comment...
					if ( (strlen(file_text_line)>0) && ( (file_text_line[0] != '/') && (file_text_line[1] != '/') ) && (pointer_1 != NULL) )
					{
						strcpy (tx[counter].tag , pointer_1);
						strupr (tx[counter].tag);

						tx[counter].line = NULL; // Just to be tidy

						line_length = strlen(pointer_2);

						if (line_length > 0)
						{
							tx[counter].line_size = line_length;
							tx[counter].line = (char *) malloc ( (line_length+1) * sizeof (char) );
							strcpy (tx[counter].line,pointer_2);
						}
						else
						{
							tx[counter].line_size = strlen("EMPTY LINE!");
							tx[counter].line = (char *) malloc ( (strlen("EMPTY LINE!")+1) * sizeof (char) );
							strcpy (tx[counter].line,"EMPTY LINE!");
						}

						counter++;
					}
				}
			}
			else
			{
				sprintf (error,"ERROR! TEXTFILE '%s' NOT FOUND FOR LOADING FOR DESCRIPTIVE TEXT FILES!",filename);
				OUTPUT_message (error);
			}

			fclose (file_pointer);

		}

	}

}



void TEXTFILE_save_compiled_text (void)
{
	// This function compiles all the text lines into one giant wobbly sack of data
	// and then spews it violently all over your hard disc.

	int counter;
	char line[TEXTFILE_SUPER_SIZE];

	FILE *file_pointer = fopen (MAIN_get_project_filename("cptf.txt"),"w");

	if (file_pointer != NULL)
	{
		sprintf(line,"%i\n",textfile_line_count);
		fputs(line,file_pointer);

		for (counter=0; counter<textfile_line_count; counter++)
		{
			sprintf(line,"%i\n",tx[counter].line_size);
			fputs(line,file_pointer);

			sprintf(line,"%s\n",tx[counter].line);
			fputs(line,file_pointer);
		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Cannot create 'cptf.dat'!");
		assert(0); // Tits...
	}
}



void TEXTFILE_load_compiled_text_from_datafile (void)
{
	char filename[MAX_LINE_SIZE];
	char line[MAX_LINE_SIZE];
	sprintf (filename,"%s\\data.dat#cptf.txt",MAIN_get_project_name());
	fix_filename_slashes(filename);
	PACKFILE *packfile_pointer = pack_fopen (filename,"r");

	int counter;
	
	if (packfile_pointer != NULL)
	{
		pack_fgets (line,MAX_LINE_SIZE,packfile_pointer);
		textfile_line_count = atoi(line);

		tx = (textfile_line *) malloc ( textfile_line_count * sizeof(textfile_line) );

		for (counter=0; counter<textfile_line_count; counter++)
		{
			pack_fgets (line,MAX_LINE_SIZE,packfile_pointer);
			tx[counter].line_size = atoi(line);
			tx[counter].line = (char *) malloc ((sizeof(char) * tx[counter].line_size) + 1);

			pack_fgets (tx[counter].line,TEXTFILE_SUPER_SIZE,packfile_pointer);
		}

		pack_fclose(packfile_pointer);
	}
	else
	{
		OUTPUT_message("Cannot read 'cptf.txt'!");
		assert(0); // Tits...
	}
}



void TEXTFILE_load_compiled_text (void)
{
	char line[MAX_LINE_SIZE];

	FILE *file_pointer = fopen (MAIN_get_project_filename("cptf.txt"),"r");

	int counter;
	
	if (file_pointer != NULL)
	{
		fgets (line,MAX_LINE_SIZE,file_pointer);
		textfile_line_count = atoi(line);

		tx = (textfile_line *) malloc ( textfile_line_count * sizeof(textfile_line) );

		for (counter=0; counter<textfile_line_count; counter++)
		{
			fgets (line,MAX_LINE_SIZE,file_pointer);
			tx[counter].line_size = atoi(line);
			tx[counter].line = (char *) malloc ((sizeof(char) * tx[counter].line_size) + 1);

			fgets (tx[counter].line,TEXTFILE_SUPER_SIZE,file_pointer);
		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Cannot read 'cptf.txt'!");
		assert(0); // Tits...
	}
}



void TEXTFILE_destroy (void)
{
	int l;
	
	for (l=0; l<textfile_line_count; l++)
	{
		if (tx[l].line != NULL)
		{
			free (tx[l].line);
		}
	}

	free (tx);
}



int TEXTFILE_get_index_by_tag (char *tag)
{
	int l;

	strupr (tag);

	for (l=0; l<textfile_line_count; l++)
	{
		if (strcmp(tag,tx[l].tag) == 0)
		{
			return l;
		}
	}

	return UNSET;
}



char * TEXTFILE_get_line_by_tag (char *tag)
{
	int index;

	index = TEXTFILE_get_index_by_tag (tag);

	if (index == UNSET)
	{
		sprintf (error_space,"TAG '%s' CANNOT BE FOUND!",tag);
		return error_space;
	}
	else
	{
		return tx[index].line;
	}
}



int TEXTFILE_get_line_length_by_tag (char *tag)
{
	int index;

	index = TEXTFILE_get_index_by_tag (tag);

	if (index == UNSET)
	{
		return 0;
	}
	else
	{
		return strlen (tx[index].line);
	}

}



char * TEXTFILE_get_line_by_index (int index)
{
	if (index == UNSET)
	{
		sprintf (error_space,"INVALID INDEX!");
		return error_space;
	}
	else
	{
		return tx[index].line;
	}
}



int TEXTFILE_get_line_length_by_index (int index)
{
	if (index == UNSET)
	{
		return 0;
	}
	else
	{
		return strlen (tx[index].line);
	}
}


