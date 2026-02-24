// This deals with loading and saving of hiscores and also adding scores to them and getting words
// from them, etc.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "main.h"
#include "hiscores.h"
#include "textfiles.h"
#include "output.h"
#include "string_stuff.h"

#include "fortify.h"

hiscore_table *hs = NULL;
int number_of_hiscore_tables = 0;



int HISCORES_get_index_from_unique_id (int unique_id)
{
	int hiscore_number;

	for (hiscore_number=0; hiscore_number<number_of_hiscore_tables; hiscore_number++)
	{
		if (hs[hiscore_number].unique_id == unique_id)
		{
			return hiscore_number;
		}
	}

	char error[MAX_LINE_SIZE];
	sprintf(error,"Invalid unique id %i for hiscore table!",unique_id);
	OUTPUT_message(error);
	assert(0);

	return UNSET;
}



int HISCORES_allocate_hiscore_table (void)
{
	if (hs == NULL)
	{
		hs = (hiscore_table *) malloc (sizeof(hiscore_table));
	}
	else
	{
		hs = (hiscore_table *) realloc (hs , sizeof(hiscore_table) * (number_of_hiscore_tables+1));
	}

	hs[number_of_hiscore_tables].scores = NULL;

	number_of_hiscore_tables++;

	return (number_of_hiscore_tables-1);
}



void HISCORES_create_hiscore_table (int unique_id, int table_type, int entries, int name_size, int score_digits)
{
	// Creates a new hiscore table with the given particulars. Oo-er.

	int index = HISCORES_allocate_hiscore_table ();

	hs[index].unique_id = unique_id;
	hs[index].entries = entries;
	hs[index].name_size = name_size;
	hs[index].score_size = score_digits;
	hs[index].table_type = table_type;

	hs[index].scores = (hiscore_entry *) malloc (sizeof (hiscore_entry) * entries);
	
	int entry;
	int char_num;

	for (entry=0; entry<entries; entry++)
	{
		hs[index].scores[entry].score = 0;
		hs[index].scores[entry].name = (char *) malloc (sizeof(char) * (name_size+1));
		for (char_num=0; char_num<=name_size; char_num++)
		{
			hs[index].scores[entry].name[char_num] = '\0';
		}
	}
}



char *HISCORE_checkcode (int unique_id, char *name, int score)
{
	char checksum[4] = {0,0,0,0};
	char byte;

	int cycle = 0;

	static char checkword[9] = {"        "};

	char line[MAX_LINE_SIZE];

	sprintf (line,"%i%s%i",unique_id,name,score);

	int c;

	for (c=0; c<signed(strlen(line)); c++)
	{
		if (c%2)
		{
			byte = line[c] - 32;
		}
		else
		{
			byte = line[c];
		}

		checksum[cycle] ^= byte;

		cycle++;
		cycle%=4;
	}

	for (c=0; c<8; c+=2)
	{
		byte = checksum[c/2] >> 4;
		byte &= 15;
		checkword[c] = byte + 'A';

		byte = checksum[c/2];
		byte &= 15;
		checkword[c+1] = 'P' - byte;
	}

	return checkword;
}



void HISCORES_save_hiscore_table (int unique_id, int filename_text_tag)
{
	// Saves a hiscore table to the filename in the text_tag given.

	char line[MAX_LINE_SIZE];

	int entry_number;
	int max_entries;

	int index = HISCORES_get_index_from_unique_id (unique_id);

	char *filename = TEXTFILE_get_line_by_index (filename_text_tag);

	FILE *fp = fopen (MAIN_get_project_filename(filename, true),"w");

	if (fp != NULL)
	{
		sprintf(line,"#HI-SCORE TABLE SIZE = %i\n",hs[index].entries);
		fputs(line,fp);

		sprintf(line,"#HI-SCORE NAME SIZE = %i\n",hs[index].name_size);
		fputs(line,fp);

		sprintf(line,"#HI-SCORE SCORE SIZE = %i\n",hs[index].score_size);
		fputs(line,fp);

		sprintf(line,"#HI-SCORE TABLE TYPE = %i\n",hs[index].table_type);
		fputs(line,fp);

		sprintf(line,"#HI-SCORE UNIQUE ID = %i\n\n",hs[index].unique_id);
		fputs(line,fp);

		fputs("#ENTRIES\n",fp);

		max_entries = hs[index].entries;

		for(entry_number=0;entry_number<max_entries;entry_number++)
		{
			sprintf(line,"\t[%s][%i][%s]\n" , hs[index].scores[entry_number].name , hs[index].scores[entry_number].score , HISCORE_checkcode(hs[index].unique_id,hs[index].scores[entry_number].name,hs[index].scores[entry_number].score) );

			fputs(line,fp);
		}

		fputs("#END OF FILE\n",fp);

		fclose (fp);
	}
	else
	{
//		OUTPUT_message("Cannot write hiscore table!");
//		assert(0);
	}

}



bool HISCORES_load_hiscore_table (int filename_text_tag)
{
	// Saves a hiscore table to the filename in the text_tag given.

	char line[MAX_LINE_SIZE];

	char *pointer;
	char *name_pointer;
	char *checkword_pointer;
	int score;

	bool exitmainloop = false;
	bool exitsubloop = false;

	char *filename = TEXTFILE_get_line_by_index (filename_text_tag);

	FILE *fp = fopen (MAIN_get_project_filename(filename, true),"r");

	int table_size = UNSET;
	int name_size = UNSET;
	int score_size = UNSET;
	int table_type = UNSET;
	int unique_id = UNSET;

	if (fp != NULL)
	{
		while ( (fgets ( line , MAX_LINE_SIZE , fp ) != NULL) && (exitmainloop == false) )
		{

			pointer = STRING_end_of_string(line,"#HI-SCORE TABLE SIZE = ");
			if (pointer != NULL)
			{
				table_size = (atoi(pointer));
			}

			pointer = STRING_end_of_string(line,"#HI-SCORE NAME SIZE = ");
			if (pointer != NULL)
			{
				name_size = (atoi(pointer));
			}

			pointer = STRING_end_of_string(line,"#HI-SCORE SCORE SIZE = ");
			if (pointer != NULL)
			{
				score_size = (atoi(pointer));
			}

			pointer = STRING_end_of_string(line,"#HI-SCORE TABLE TYPE = ");
			if (pointer != NULL)
			{
				table_type = (atoi(pointer));
			}

			pointer = STRING_end_of_string(line,"#HI-SCORE UNIQUE ID = ");
			if (pointer != NULL)
			{
				unique_id = (atoi(pointer));
			}

			pointer = STRING_end_of_string(line,"#ENTRIES");
			if (pointer != NULL)
			{
				// We should have all the guff to make a table now... I hope...

				if ( (table_size == UNSET) || (name_size == UNSET) || (score_size == UNSET) || (table_type == UNSET) || (unique_id == UNSET) )
				{
					// Table corrupted!
					return false;
				}

				HISCORES_create_hiscore_table (unique_id,table_type,table_size,name_size,score_size);

				while ( (fgets ( line , MAX_LINE_SIZE , fp ) != NULL) && (exitsubloop == false) )
				{
					pointer = STRING_end_of_string(line,"#END OF FILE");
					if (pointer != NULL)
					{
						exitsubloop = true;
						exitmainloop = true;
					}
					else
					{
						// Hope it's a score...

						name_pointer = strtok(line,"\t[]");

						score = atoi (strtok(NULL,"\t[]"));

						checkword_pointer = strtok(NULL,"\t[]");

						if (strcmp(checkword_pointer,HISCORE_checkcode(unique_id,name_pointer,score)) == 0)
						{
							HISCORES_insert_score (unique_id,name_pointer,score);
						}
						else
						{
							HISCORES_insert_score (unique_id,name_pointer,0);
						}
					}
					
				}

			}

			pointer = STRING_end_of_string(line,"#END OF FILE");
			if (pointer != NULL)
			{
				exitmainloop = true;
			}

		}

		fclose (fp);

		return true;
	}

	return false;
}



int HISCORE_get_score (int unique_id, int entry)
{
	int hiscore_number = HISCORES_get_index_from_unique_id (unique_id);

	if (entry == LOWEST_SCORE)
	{
		entry = hs[hiscore_number].entries-1;
	}

	return (hs[hiscore_number].scores[entry].score);
}



void HISCORES_overwrite_name (int unique_id, int entry, char *name)
{
	int hiscore_number = HISCORES_get_index_from_unique_id (unique_id);
	
	char name_copy[MAX_LINE_SIZE];
	strcpy(name_copy,name);

	name_copy[hs[hiscore_number].name_size] = '\0';
	// Truncate name.

	strcpy(hs[hiscore_number].scores[entry].name , name_copy);
}



int HISCORES_insert_score (int unique_id, char *name, int score)
{
	// Finds the right place to stick this into the scores and pokes it in there.

	int hiscore_number = HISCORES_get_index_from_unique_id (unique_id);

	char name_copy[MAX_LINE_SIZE];
	strcpy(name_copy,name);

	name_copy[hs[hiscore_number].name_size] = '\0';
	// Truncate name.

	int our_entry = UNSET;

	// Go from the top of the scoreboard down until our score is better
	// than that entry.

	for (our_entry=0; ( (our_entry<hs[hiscore_number].entries) && (hs[hiscore_number].scores[our_entry].score > score) ); our_entry++);

	// Now go up the table to our_entry and shuffle 'em down.

	int entry_number;

	for (entry_number = (hs[hiscore_number].entries-1) ; entry_number>our_entry ; entry_number--)
	{
		strcpy (hs[hiscore_number].scores[entry_number].name , hs[hiscore_number].scores[entry_number-1].name);
		hs[hiscore_number].scores[entry_number].score = hs[hiscore_number].scores[entry_number-1].score;
	}

	hs[hiscore_number].scores[our_entry].score = score;
	strcpy(hs[hiscore_number].scores[our_entry].name , name_copy);

	return our_entry;
}



char * HISCORE_get_hiscore_name (int unique_id, int entry)
{
	// Returns a pointer to the correct name in the hiscore table.

	int hiscore_number = HISCORES_get_index_from_unique_id (unique_id);

	if (entry < hs[hiscore_number].entries)
	{
		return (hs[hiscore_number].scores[entry].name);
	}
	else
	{
		char error[MAX_LINE_SIZE];
		sprintf(error,"Entry %i wrong, table %i has %i entries!",entry,unique_id,hs[hiscore_number].entries);
		OUTPUT_message(error);
		assert(0);
	}

	return NULL;
}



char * HISCORE_get_hiscore_score (int unique_id, int entry, bool pad_with_zeros)
{
	static char word[MAX_LINE_SIZE];
	unsigned int digits;
	int diff;
	int counter;
	static char spaces[MAX_LINE_SIZE];

	char error[MAX_LINE_SIZE];

	// Generates a string with the score in it, padded to the left with either spaces
	// or zeros so that it's the correct size.

	int hiscore_number = HISCORES_get_index_from_unique_id (unique_id);

	if (entry < hs[hiscore_number].entries)
	{
		digits = hs[hiscore_number].score_size;

		sprintf (word,"%i",hs[hiscore_number].scores[entry].score);

		if (strlen(word) < digits)
		{
			if (pad_with_zeros)
			{
				diff = digits - strlen(word);

				for (counter=0 ; counter<diff ; counter++)
				{
					spaces[counter] = '0';
				}

				spaces[diff] = '\0';
			}
			else
			{
				diff = digits - strlen(word);

				for (counter=0 ; counter<diff ; counter++)
				{
					spaces[counter] = ' ';
				}

				spaces[diff] = '\0';
			}

			strcat(spaces,word);
			strcpy(word,spaces);
		}

		return &word[0];
	}
	else
	{
		sprintf(error,"Entry %i wrong, table %i has %i entries!",entry,unique_id,hs[hiscore_number].entries);
		OUTPUT_message(error);
		assert(0);
	}

	return NULL;
}



char * HISCORE_get_hiscore_name_and_score (int unique_id, int entry, bool pad_with_zeros, bool name_first, int gap_spaces)
{
	static char word[MAX_LINE_SIZE];
	int counter;
	static char spaces[MAX_LINE_SIZE];

	char *name_pointer;
	char *score_pointer;

	name_pointer = HISCORE_get_hiscore_name (unique_id, entry);
	score_pointer = HISCORE_get_hiscore_score (unique_id, entry, pad_with_zeros);

	if ( (score_pointer == NULL) && (name_pointer == NULL) )
	{
		return NULL;
	}

	// Generates a string with the name and score in it, padded as above and in the order specified
	// by the boolean "name_first". Also inserts a number of spaces between the two parts as requested.

	for (counter=0; counter<gap_spaces; counter++)
	{
		spaces[counter] = ' ';
	}

	spaces[counter] = '\0';

	if (name_first)
	{
		sprintf(word,"%s%s%s",name_pointer,spaces,score_pointer);
	}
	else
	{
		sprintf(word,"%s%s%s",score_pointer,spaces,name_pointer);
	}

	return (&word[0]);
}



void HISCORES_destroy_hiscores (void)
{
	// Goes through each one freeing the tables there-in and then the main holder itself.

	if (hs != NULL)
	{
		int hiscore_number;
		int entry_number;

		for (hiscore_number=0; hiscore_number<number_of_hiscore_tables; hiscore_number++)
		{
			if (hs[hiscore_number].scores != NULL)
			{
				for (entry_number=0; entry_number<hs[hiscore_number].entries; entry_number++)
				{
					if (hs[hiscore_number].scores[entry_number].name != NULL)
					{
						free (hs[hiscore_number].scores[entry_number].name);
					}
				}

				free (hs[hiscore_number].scores);
			}
		}

		free (hs);
	}
}



void HISCORES_check_for_and_replace_upload_code_file (void)
{
	FILE *file_pointer = fopen (MAIN_get_project_filename("hiscore_upload_codes.txt", true),"r");

	if (file_pointer == NULL)
	{
		file_pointer = fopen (MAIN_get_project_filename("hiscore_upload_codes.txt", true),"w");

		if (file_pointer != NULL)
		{
			fputs ("Hiscore Upload Code List\n------------------------\n\n",file_pointer);
			fputs ("This contains a chronological list of all the hiscores you've ever achieved,\n",file_pointer);
			fputs ("so the newest ones will always be at the bottom.\n\n",file_pointer);
			fputs ("To use a code, simply copy everything between the []'s from the appropriate\n",file_pointer);
			fputs ("line and then paste it into the box on our webpage (http://retrospec.sgn.net/).\n\n",file_pointer);

			fclose(file_pointer);
		}
		else
		{
			assert(0);
		}
	}
	else
	{
		fclose(file_pointer);
	}
}



void HISCORES_append_new_hiscore (int unique_id, int score)
{
	int hiscore_number = HISCORES_get_index_from_unique_id (unique_id);

	HISCORES_check_for_and_replace_upload_code_file();

	char line[MAX_LINE_SIZE];
	char date[MAX_LINE_SIZE];
	char uuencoded_line[MAX_LINE_SIZE];

	int scores[1];
	scores[0] = score;
	int sizes[1];
	sizes[0] = 4;

	uuencode_generic (&uuencoded_line[0], RETRENGINE_RETROSPEC_HISCORE_GAME_ID, 1, &scores[0], &sizes[0]);

	time_t rawtime;
	struct tm *timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	sprintf (date,asctime (timeinfo));
	STRING_strip_all_disposable (date);

	sprintf (line , "Hi-score %i scored on %s = [%s]\n", score , date , &uuencoded_line[0]);

	FILE *file_pointer = fopen (MAIN_get_project_filename("hiscore_upload_codes.txt", true),"a");

	if (file_pointer != NULL)
	{
		fputs(line,file_pointer);
		fclose(file_pointer);
	}
	else
	{
		assert(0);
	}
}



