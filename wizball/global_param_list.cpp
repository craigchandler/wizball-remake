#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "output.h"
#include "main.h"
#include "string_stuff.h"
#include "allegro.h"

#include "fortify.h"

static char *word_list = NULL;
int *word_list_values = NULL;
int number_of_global_parameter_lists_loaded = 0;
int total_word_list_size = 0;

char word_list_names [64][NAME_SIZE];
char word_list_extensions [64][NAME_SIZE];
int word_list_lengths [64];
int word_list_starts [64];



void GPL_destroy_lists (void)
{
	free (word_list);
	free (word_list_values);
}



char * GPL_what_is_list_name (int list_number)
{
	return &word_list_names[list_number][0];
}



char * GPL_what_is_word_name (int entry_number)
{
	// Used when printing the list of entries in a list to the screen.
	return &word_list[entry_number*NAME_SIZE];
}



int GPL_get_value_by_index (int index)
{
	return (word_list_values[index]);
}



char * GPL_what_is_list_extension (char *word_list)
{
	// Returns a pointer to the file extension used by the given word list.
	// This is for any functions which load the contents of a word list.
	int i;

	for (i=0; i<number_of_global_parameter_lists_loaded ; i++)
	{
		if (strcmp(word_list,&word_list_names[i][0]) == 0)
		{
			return &word_list_extensions[i][0];
		}
	}

	return NULL;
}



int GPL_word_list_number (char *word_list)
{
	int i;

	for (i=0; i<number_of_global_parameter_lists_loaded ; i++)
	{
		if (strcmp(word_list,&word_list_names[i][0]) == 0)
		{
			return i;
		}
	}

	return UNSET;
}



int GPL_what_is_list_number (char *word)
{
	int i;

	for (i=0; i<number_of_global_parameter_lists_loaded ; i++)
	{
		if (strcmp(word,&word_list_names[i][0]) == 0)
		{
			return i;
		}
	}

	return UNSET;
}



void GPL_list_extents (char *word_list, int *start, int *end)
{
	// Looks through the word_list names for a match then looks through that list for a match.
	int list_number;

	if (start != NULL)
	{
		*start = UNSET;
	}

	if (end != NULL)
	{
		*end = UNSET;
	}

	list_number = GPL_what_is_list_number (word_list);

	if (list_number != UNSET)
	{
		if (start != NULL)
		{
			*start = word_list_starts[list_number];
		}

		if (end != NULL)
		{
			*end = *start + word_list_lengths[list_number];
		}
	}

}



int GPL_list_size (char *word_list)
{
	int list_number;

	list_number = GPL_word_list_number (word_list);

	return word_list_lengths[list_number];
}



char * GPL_get_entry_name (char *word_list_name, int index)
{
	int start,end;

	GPL_list_extents (word_list_name, &start, &end);

	index += start;

	return &word_list[index*NAME_SIZE];
}



int GPL_get_word_index (char *word_list, char *word)
{
	int start,end;
	int i;

	GPL_list_extents (word_list, &start, &end);

	for (i=start; i<end; i++)
	{
		if (strcmp ( word , GPL_what_is_word_name (i) ) == 0)
		{
			return i;
		}
	}

	return UNSET;
}



int GPL_does_word_exist (char *word_list_name, char *word)
{
	int start,end;
	int i;

	GPL_list_extents (word_list_name, &start, &end);

	if (start == UNSET) // The word list wasn't even found...
	{
		return VERY_BROKEN_LINK;
	}

	for (i=start; i<end; i++)
	{
		if (strcmp ( word , &word_list[i*NAME_SIZE] ) == 0)
		{
			return OKAY;
		}
	}

	return BROKEN_LINK;
}



int GPL_does_parameter_exist (char *word_list, char *word)
{
	// This basically just gets a result from "EDIT_does_word_exist" but first discounts
	// the word-list "NUMBER" which is used for purely numerical values.

	if ( strcmp(word_list,"NUMBER") == 0 )
	{
		return OKAY;
	}
	else
	{
		return GPL_does_word_exist (word_list, word);
	}

}



int GPL_find_word_index (char *word_list, char *word)
{
	// Looks through the word_list names for a match then looks through that list for a match.
	int start,end;
	int index;
	bool found_word_list = false;

	if (strcmp(word_list,"NUMBER")==0)
	{
		// If it's not actually a word-list but rather a number, just convert it.
		return (atoi(word));
	}

	if (strcmp(word,"UNSET")==0)
	{
		return UNSET;
	}

	GPL_list_extents (word_list, &start, &end);

	if (start == UNSET)
	{
//		sprintf(error,"Word list '%s' does not exist!",word_list,word);
//		OUTPUT_message(error);
//		assert(0);
		return UNSET; // Word list not found
	}

	index = GPL_get_word_index (word_list,word);
	if (index == UNSET)
	{
//		sprintf(error,"Word list '%s' does not contain word '%s'!",word_list,word);
//		OUTPUT_message(error);
//		assert(0);
		return UNSET; // Word in word list not found
	}

	return index;

}



int GPL_find_word_value (char *word_list, char *word)
{
	int index;

	index = GPL_find_word_index ( word_list , word );

	if (index == UNSET)
	{
		return UNSET;
	}
	else
	{
		if ( strcmp (word_list,"NUMBER") == 0)
		{
			return index;
		}
		else
		{
			return word_list_values[index];
		}
	}
}



void GPL_add_to_global_parameter_list (char *list_name , char *new_entry_name , int new_entry_value)
{
	// This looks for the space in which the new word would fit into the supplied
	// list then moves everything up by one in order to make room for it and slips
	// it in. Oo-er.

	int start,end;
	int i;
	int list_number;
	char *name;

	int insert_after;

	GPL_list_extents (list_name, &start, &end);

	insert_after = start-1;

	for (i=start; i<end; i++)
	{
		name = GPL_what_is_word_name (i);
		if ( strcmp(new_entry_name,name) > 0 )
		{
			// If the word we want to insert is alphabetically less than the word in the list then update insert_after;
			insert_after = i;
			// Obviously as soon as we reach one that's equal or greater then insert_after stops updating. Peachy.
		}
	}

	// Okay, insert after now points to the last word in the list before we need to put in the new data.
	// First of all malloc more room in the list...

	word_list = (char *) realloc (word_list , (total_word_list_size + 1) * sizeof (char) * NAME_SIZE);
	word_list_values = (int *) realloc (word_list_values , (total_word_list_size + 1) * sizeof (int));

	total_word_list_size++;

	// Now move everything downwards... (because I couldn't figure out memmove - d'oh!)

//	for (i=insert_after+2 ; i<total_word_list_size ; i++)
//	{
//		word_list_values[i] = word_list_values[i-1];
//		strcpy ( &word_list[i*NAME_SIZE] , &word_list[(i-1)*NAME_SIZE] );
//	}

	for (i=total_word_list_size-1 ; i >= insert_after+2 ; i--)
	{
		word_list_values[i] = word_list_values[i-1];
		strcpy ( &word_list[i*NAME_SIZE] , &word_list[(i-1)*NAME_SIZE] );
	}

	// And plonk the new item and value in there.

	word_list_values[insert_after+1] = new_entry_value;
	strcpy ( &word_list[(insert_after+1)*NAME_SIZE] , new_entry_name );

	// Then increase the starts of every list after this one by 1.

	list_number = GPL_word_list_number (list_name);

	word_list_lengths[list_number]++; // Increase the size of this list.

	for (i=list_number+1; i<number_of_global_parameter_lists_loaded ; i++)
	{
		word_list_starts[i]++;
	}

	// And Bob's yer uncle!

}



void GPL_remove_from_global_parameter_list_by_name (char *list_name , char *new_entry_name)
{
	int start,end;
	int i;
	int list_number;
	char *name;

	int found_item;

	GPL_list_extents (list_name, &start, &end);

	for (i=start; i<end; i++)
	{
		name = GPL_what_is_word_name (i);
		if ( strcmp(new_entry_name,name) == 0 )
		{
			found_item = i;
		}
	}

	// Now move everything upwards...

	for (i=found_item+1 ; i<total_word_list_size ; i++)
	{
		word_list_values[i-1] = word_list_values[i];
		strcpy ( &word_list[(i-1)*NAME_SIZE] , &word_list[i*NAME_SIZE] );
	}

	// And then realloc away those extra bytes we don't need any more...

	word_list = (char *) realloc (word_list , (total_word_list_size - 1) * sizeof (char) * NAME_SIZE);
	word_list_values = (int *) realloc (word_list_values , (total_word_list_size - 1) * sizeof (int));

	total_word_list_size--;

	// Then reduce the starts of every list after this one by 1.

	list_number = GPL_word_list_number (list_name);

	word_list_lengths[list_number]--; // Decrease the size of this list.

	for (i=list_number+1; i<number_of_global_parameter_lists_loaded ; i++)
	{
		word_list_starts[i]--;
	}

	// Done and dusted, daddio!
}



void GPL_alphabetise_global_parameter_list (char *list_name)
{
	// Pass this a list name and it'll reshuffle the words in it back into order, in case
	// you've altered the name of an item in the list. Because it's only gonna' be one object
	// that's out of whack we'll use a back and forth routine so it is all in order after
	// one sweep back and forth through the deck.

	int start,end;
	char name_store[NAME_SIZE];
	int i;
	bool sorted;

	GPL_list_extents (list_name, &start, &end);

	do 
	{
		sorted = true;

		for (i=start; i<end-1; i++)
		{
			if (strcmp ( &word_list[(i)*NAME_SIZE] , &word_list[(i+1)*NAME_SIZE] ) > 0 )
			{
				strcpy ( name_store , &word_list[(i)*NAME_SIZE] );
				strcpy ( &word_list[(i)*NAME_SIZE] , &word_list[(i+1)*NAME_SIZE] );
				strcpy ( &word_list[(i+1)*NAME_SIZE] , name_store );
				sorted = false;
			}
		}

		for (i=end-1; i>=start; i++)
		{
			if (strcmp ( &word_list[(i)*NAME_SIZE] , &word_list[(i+1)*NAME_SIZE] ) > 0 )
			{
				strcpy ( name_store , &word_list[(i)*NAME_SIZE] );
				strcpy ( &word_list[(i)*NAME_SIZE] , &word_list[(i+1)*NAME_SIZE] );
				strcpy ( &word_list[(i+1)*NAME_SIZE] , name_store );
				sorted = false;
			}
		}

	} while (sorted == false);

}



bool GPL_check_for_duplicate_name (char *list_name , char *old_entry_name , char *new_entry_name)
{
	// Drop out immediately if the name ain't changed.
	if (strcmp (old_entry_name,new_entry_name) == 0)
	{
		return false;
	}

	// And then check the word list.
	if ( GPL_does_word_exist (list_name, new_entry_name) == OKAY )
	{
		return true;
	}

	return false;

}



void GPL_load_global_parameter_list_from_datfile(void)
{
	// This loads all the word lists we could possibly want and allocates space for them to boot.

	char line[NAME_SIZE*2];
	char filename[MAX_LINE_SIZE];

	sprintf (filename,"%s\\data.dat#gpl_list_size.txt",MAIN_get_project_name());
	fix_filename_slashes(filename);

	PACKFILE *packfile_pointer = pack_fopen (filename,"r");

	if (packfile_pointer != NULL)
	{
		pack_fgets ( line , NAME_SIZE*2 , packfile_pointer );
		total_word_list_size = atoi(line);

		// Malloc some space for the word list and values...
		word_list = (char *) malloc (sizeof (char) * NAME_SIZE * total_word_list_size);
		word_list_values = (int *) malloc (sizeof (int) * total_word_list_size);

		if (word_list == NULL)
		{
			assert(0);
		}

		if (word_list_values == NULL)
		{
			assert(0);
		}

		pack_fclose(packfile_pointer);
	}
	else
	{
		OUTPUT_message("Global Parameter List Size Not Found!");
		assert(0); // AARGH!
	}

	sprintf (filename,"%s\\data.dat#global_parameter_list.txt",MAIN_get_project_name());
	fix_filename_slashes(filename);

	packfile_pointer = pack_fopen (filename,"r");

	bool exitflag = false;
	bool exitmainloop = false;

	int i;

	int total_word_counter = 0;

	char *pointer;

	if (packfile_pointer != NULL)
	{
		while ( ( pack_fgets ( line , NAME_SIZE*2 , packfile_pointer ) != NULL ) && (exitmainloop == false) )
		{
			STRING_strip_newlines (line);
			strupr(line);

			pointer = strstr(line,"#END_OF_PARAMETER_LIST");
			if (pointer != NULL) // It's the end of the world as we know it...
			{
				exitmainloop = true;
			}

			pointer = STRING_end_of_string(line,"#PARAMETER LIST NAME = ");
			if (pointer != NULL) // 
			{
				strcpy(&word_list_names[number_of_global_parameter_lists_loaded][0] , pointer);
				strcpy(&word_list_extensions[number_of_global_parameter_lists_loaded][0] , ""); // By default there is no extension unless specified
			}

			pointer = STRING_end_of_string(line,"#PARAMETER LIST EXTENSION = ");
			if (pointer != NULL) // 
			{
				strcpy(&word_list_extensions[number_of_global_parameter_lists_loaded][0] , pointer);
			}

			pointer = STRING_end_of_string(line,"#PARAMETER LIST SIZE = ");
			if (pointer != NULL) // 
			{
				word_list_lengths[number_of_global_parameter_lists_loaded] = atoi(pointer);
				
				// Only bother reading the list if there are entries in it.	
				if (word_list_lengths[number_of_global_parameter_lists_loaded] > 0)
				{
					// Then get the words one by one...

					for (i=0 ; i<word_list_lengths[number_of_global_parameter_lists_loaded] ; i++)
					{
						pack_fgets ( line , NAME_SIZE*2 , packfile_pointer );

						STRING_strip_newlines (line);
						strupr(line);

						pointer = strtok(line," =");

						strcpy ( &word_list[(i+total_word_counter)*NAME_SIZE] , line );

						pointer = strtok(NULL," =");

						word_list_values[i+total_word_counter] = atoi(pointer);
					}

				}

				word_list_starts[number_of_global_parameter_lists_loaded] = total_word_counter;

				total_word_counter += word_list_lengths[number_of_global_parameter_lists_loaded];
				number_of_global_parameter_lists_loaded++;

			}

		}

		total_word_list_size = total_word_counter; // set the global

		pack_fclose(packfile_pointer);
	}
	else
	{
		OUTPUT_message("Global Parameter List Not Found!");
		assert(0); // AARGH!
	}

}



void GPL_load_global_parameter_list(void)
{
	// This loads all the word lists we could possibly want and allocates space for them to boot.

	char line[NAME_SIZE*2];

	FILE *file_pointer = fopen (MAIN_get_project_filename("gpl_list_size.txt"),"r");

	if (file_pointer != NULL)
	{
		fgets ( line , NAME_SIZE*2 , file_pointer );
		total_word_list_size = atoi(line);

		// Malloc some space for the word list and values...
		word_list = (char *) malloc (sizeof (char) * NAME_SIZE * total_word_list_size);
		word_list_values = (int *) malloc (sizeof (int) * total_word_list_size);

		if (word_list == NULL)
		{
			assert(0);
		}

		if (word_list_values == NULL)
		{
			assert(0);
		}

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Global Parameter List Size Not Found!");
		assert(0); // AARGH!
	}

	file_pointer = fopen (MAIN_get_project_filename("global_parameter_list.txt"),"r");

	bool exitflag = false;
	bool exitmainloop = false;

	int i;

	int total_word_counter = 0;

	char *pointer;

	if (file_pointer != NULL)
	{
		while ( ( fgets ( line , NAME_SIZE*2 , file_pointer ) != NULL ) && (exitmainloop == false) )
		{
			STRING_strip_newlines (line);
			strupr(line);

			pointer = strstr(line,"#END_OF_PARAMETER_LIST");
			if (pointer != NULL) // It's the end of the world as we know it...
			{
				exitmainloop = true;
			}

			pointer = STRING_end_of_string(line,"#PARAMETER LIST NAME = ");
			if (pointer != NULL) // 
			{
				strcpy(&word_list_names[number_of_global_parameter_lists_loaded][0] , pointer);
				strcpy(&word_list_extensions[number_of_global_parameter_lists_loaded][0] , ""); // By default there is no extension unless specified
			}

			pointer = STRING_end_of_string(line,"#PARAMETER LIST EXTENSION = ");
			if (pointer != NULL) // 
			{
				strcpy(&word_list_extensions[number_of_global_parameter_lists_loaded][0] , pointer);
			}

			pointer = STRING_end_of_string(line,"#PARAMETER LIST SIZE = ");
			if (pointer != NULL) // 
			{
				word_list_lengths[number_of_global_parameter_lists_loaded] = atoi(pointer);
				
				// Only bother reading the list if there are entries in it.	
				if (word_list_lengths[number_of_global_parameter_lists_loaded] > 0)
				{
					// Then get the words one by one...

					for (i=0 ; i<word_list_lengths[number_of_global_parameter_lists_loaded] ; i++)
					{
						fgets ( line , NAME_SIZE*2 , file_pointer );

						STRING_strip_newlines (line);
						strupr(line);

						pointer = strtok(line," =");

						strcpy ( &word_list[(i+total_word_counter)*NAME_SIZE] , line );

						pointer = strtok(NULL," =");

						word_list_values[i+total_word_counter] = atoi(pointer);
					}

				}

				word_list_starts[number_of_global_parameter_lists_loaded] = total_word_counter;

				total_word_counter += word_list_lengths[number_of_global_parameter_lists_loaded];
				number_of_global_parameter_lists_loaded++;

			}

		}

		total_word_list_size = total_word_counter; // set the global

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Global Parameter List Not Found!");
		assert(0); // AARGH!
	}

}



int GPL_draw_list (char *list_name , int x , int y , int width , int height , int first_entry , int mouse_x_pos, int mouse_y_pos, bool numbered, int y_spacing, int r, int g, int b)
{
	int start,end;
	int i;
	char word[NAME_SIZE];
	int highlighted;
	int counter;

	int number_of_chars;
	int number_of_lines;

	number_of_chars = width/8;
	number_of_lines = (height/y_spacing);

	highlighted = UNSET;
	counter = 0;

	GPL_list_extents (list_name, &start, &end);

	for (i=start+first_entry ; (i<end) && (i<start+first_entry+number_of_lines) ; i++)
	{
		if (numbered)
		{
			sprintf( word , "%3i" , i-start );
			STRING_replace_char (word, ' ' , '0');
			strcat( word , " : " );
			strcat( word , GPL_what_is_word_name (i) );
		}
		else
		{
			strcpy( word , GPL_what_is_word_name (i) );
		}

		if (strlen(word) > unsigned(number_of_chars))
		{
			word[number_of_chars] = '\0';
		}

		if ( (mouse_y_pos > y+(counter*y_spacing)) && (mouse_y_pos < y+(counter*y_spacing)+8) && (mouse_x_pos > x) && (mouse_x_pos < x + width) )
		{
			OUTPUT_text (x,y+(counter*y_spacing),word,255,255,255);
			highlighted = i-start;
		}
		else
		{
			OUTPUT_text (x,y+(counter*y_spacing),word,r,g,b);
		}

		counter++;
	}

	return highlighted;
}



void GPL_rename_entry (char *list_name, char *old_entry_name, char *new_entry_name)
{
	int index;

	index = GPL_get_word_index (list_name, old_entry_name);

	strcpy (GPL_what_is_word_name (index),new_entry_name);
}



int GPL_how_many_lists (void)
{
	return number_of_global_parameter_lists_loaded;
}
