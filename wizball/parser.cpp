// Script Parser - Including the Script Parser Parser. :)

	// So, anyway here are the Parser Script Controls:

	// AUTONUMBER - this is immediately followed by the ID_TAG of the list then its token value.
	// There then follows a series of words with one on each line (at least only the first word is
	// taken notice of). Each of these words is stuck into the list in turn and assigned a value equivalent
	// to their place in the list (despite the fact this could be infered from the index, it's still necessary
	// to store it).

	// NUMBERED - this is immediately followed by the ID_TAG of the list then its token value.
	// There then follows a series of numbers followed by blank space (TAB or SPACES) then a word. One pair
	// per line. The word is stored in sequential however it's value is that which preceded it on the line.
	// Generally this is only for constants.

	// LIST_FILE - this is immediately followed by the ID_TAG of the list then its token value.
	// The ID_TAG is then used to form the basis of the filename where the list is gathered from (ie, it sticks
	// on a ".TXT"). The values are sequential and it acts like an AUTONUMBER list but just gotten from another
	// file for convenience and separation.

	// LIST_DIR - this is immediately followed by the ID_TAG of the list then its token value.
	// The directory of the same name as the ID_TAG is then opened and a list of filenames compiled with
	// sequential values. The filenames are stripped of their extensions before being stored.

	// OUTPUT_LIST - this is immediately followed by the ID_TAG of the list. The program searches through the
	// ID_TAGs it has loaded so far and then outputs all the names in the list in order.

	// COMMAND_SYNTAX_LIST - This is a list of all the different combinations of ID_TAGs which can possibly
	// make up a valid line of script. First of all it goes through the line checking that each ID_TAG is valid
	// in each line and reports those which aren't. Then it stores all that information in the command syntax
	// array as a number (ie, the number of which list each ID_TAG points to) or numbers where there are several options.

	// INTERPRET - Starts the interpreter up and begins doing all the scripts in order.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <allegro.h>

#ifdef ALLEGRO_WINDOWS
	#include <io.h>
	#include <sys\stat.h>
#endif

#include <ctype.h>
#include <math.h>
#include <allegro.h>

#include "parser.h"
#include "math_stuff.h"
#include "string_stuff.h"
#include "main.h"
#include "output.h"
#include "file_stuff.h"
#include "arrays.h"

#include "fortify.h"

char error_current_script[MAX_LINE_SIZE] = "LANGUAGE FILE";

char aliases_from[MAX_ALIASES][NAME_SIZE];
char aliases_to[MAX_ALIASES][NAME_SIZE];
int alias_count = 0;

int number_of_outputted_word_list_entries;

int debug = 1;

temp_script_line *temp_tokenised_script = NULL; // [MAX_TEMP_SCRIPT_LENGTH][MAX_ARGUMENTS];
	// This is where the script is tokenised into, before it's dumped onto the end of the BIG script.

int temp_tokenised_script_length = 0;

script_line *full_tokenised_script = NULL; // [MAX_FULL_SCRIPT_LENGTH][MAX_ARGUMENTS];
	// This is the biggy, the script file. It's this sucker which'll be squeezed out the other end of
	// the parser and saved to hard drive so that the main game can load it up. God willing. At the moment
	// it's half a meg in size, but I suspect it'll grow. Blimey.

int full_tokenised_script_length = 0;

int *script_index_table = NULL; // [MAX_SCRIPTS];
	// This contains pointers to the starts of all the scripts so that they can be referenced by number.
	// This is a list which is dumped out to a file after the scripts are dealt with so that it can be
	// used as a lookup table by the main program when finding script start lines.

int number_of_scripts_loaded = 0;

word_list_struct *word_list = NULL;
	// This is where the word lists live.

int number_of_word_lists_loaded = 0;



command_syntax_archetype *command_syntax_archetype_list = NULL;
	// This is where the archetypes go on their holidays.

int total_command_syntax_archetypes;
	// This is the number of archetypes found;

int total_scripts_loaded;
	// This is the number of scripts loaded. Duh!

int label_list_index;
	// This is set when the language parser encounters a list called "LABEL", as it's a special case.
	// ie, it is the index of the word_list which contains the labels.

int number_list_index;
	// This is set when the language parser encounters a list called "NUMBER", as it's a special case.

int ignore_list_index;
	// This is set when the language parser encounters a list called "IGNORE", as it's a special case.
	// Basically the tokens for these commands don't get put into the tokenised script.

int variable_list_index;
	// So you can only inverse the value of variables.

int script_list_index;
	// This is the list the scripts live in, so we can easily plow through them for tokenising.

int constant_list_index;
	// This is the list that constants are in.

int offset_list_index;
	// This is used for failed checks as the offset to the next valid line.

int math_list_index;
	// This is used to stop it skipping "-"'s if it's a word from the math operation list.

int compare_list_index;
	// This is used to stop it skipping "!"'s if it's a word from the compare operation list.

int math_compare_list_index;
	// This is used to stop it skipping "!"'s if it's a word from the compare operation list.

text_argument_line *script = NULL; // [MAX_LINES_PER_FILE][MAX_LINE_SIZE];
	// The place that the script descripter is plonked, and then each of the scripts in turn.

int script_length = 0;

int *matching_alternate_store = NULL;
	// Whenever we're looking to see which command syntax archetype matches we store whichever
	// alternates match, that way when we generate the tokenized version immediately afterwards
	// we're cooking on gas as we know which word lists to use.

int maximum_command_syntax_size = 0;



tracer_script_line_struct *tracer_script = NULL;
int tracer_script_length = 0;




int syntax_parameter_list_count = 0;
syntax_parameter_list_entry_struct *syntax_parameter_list = NULL;



// [C] = Command (even if it's really an ignored thing, it'll colour it nicer in the editor if I make one)
// [P] = Parameter
// [F] = Fixed value
// [V] = Variable (which will probably change in the course of the line)
// [L] = Label
// [M] = Math sign or comparative operator
// [O] = Operand

char command_syntax_archetype_identifiers[] = {"CPFVLMO"};





int PARSER_is_word_in_list ( int word_list_index , char *word , bool partial )
{
	// Simply looks through the given word list to see if there's any match. Returns UNSET
	// if it's not or index if it is.
	
	int i;
	char *pointer;

	for (i=0 ; i<word_list[word_list_index].word_list_size ; i++)
	{
		pointer = word_list[word_list_index].words[i].text_word;

		if (partial == false)
		{
			if ( strcmp ( word , word_list[word_list_index].words[i].text_word ) == 0 )
			{
				return i; // We have a winner, ladies and gentlemen!
			}
		}
		else
		{
			if ( STRING_partial_strcmp ( word , word_list[word_list_index].words[i].text_word ) == 0 )
			{
				return i; // We have a winner, ladies and gentlemen!
			}
		}
	}

	return UNSET;
}



int PARSER_is_whole_or_partial_word_in_list ( int word_list_index , char *word )
{
	// Note, so that things like sprites and sounds don't have to be referred to by the FULL filename,
	// this function will return a positive match is there is a partial match, ie "LOVE" = "LOVELY"
	// but only after it has looked unsuccesfully for a complete match.

	int result;

	// Look for whole match...
	result = PARSER_is_word_in_list ( word_list_index , word , false );

	if (result == UNSET) // Didn't find one?
	{
		// Look for partial match...
//		result = PARSER_is_word_in_list ( word_list_index , word , true );
	}

	return result;
}



void PARSER_put_values_of_word_or_words ( int word_list_index , char *word , temp_script_data *script_element )
{
	// As we should have checked everything beforehand we can safely assume that
	// all returned indexes will be valid.

	// If it's a number it's easy enough...

	script_element->data_bitmask = 0;
	int value;

	char error[MAX_LINE_SIZE];

	if ( (word_list_index == number_list_index) || (word_list_index == offset_list_index) || (label_list_index == offset_list_index) )
	{
		if (word[0]=='!')
		{
			value = ~atoi(&word[1]);
		}
		else
		{
			value = atoi(word);
		}
		script_element->data_type = word_list[word_list_index].data_type;
		script_element->data_value = value;
		script_element->source_word_list_entry_1 = value;
		script_element->source_word_list_entry_2 = UNSET;
	}
	else
	{
		// If the word is of the "word_1.word_2" type then we need to check both of 'em.

		char *word_1;
		char *word_2;

		char word_backup[NAME_SIZE];
		char *word_pointer;

		strcpy (word_backup,word); // Need to back it up as we're strtok'ing it.

		word_pointer = word_backup;

		if ( (STRING_instr_char(word_pointer,'-',0) != UNSET) && (word_list_index != math_list_index) )
		{
			// ie, if there's a "-" at the start of the word and the word isn't just the minus from the math operation list...

			word_pointer++;

			if ( (word_list_index != variable_list_index) || (word_list_index != constant_list_index) )
			{
				script_element->data_bitmask |= DATA_BITMASK_NEGATE_VALUE;
			}
			else
			{
				sprintf(error,"ILLEGAL NEGATION OF WORD '%s' OF FROM WORD LIST '%s' IN SCRIPT '%s'" , word_pointer , word_list[word_list_index].name , error_current_script);
				OUTPUT_message (error);
				assert(0);
			}
		}

		if ( (word_pointer[0]=='!') && (word_list_index != compare_list_index) && (word_list_index != math_list_index) && (word_list_index != math_compare_list_index) )
		{
			// ie, if there's a "!" at the start of the word and the word isn't just the != from the compare operation list...

			word_pointer++;

			if ( (word_list_index != variable_list_index) || (word_list_index != constant_list_index) )
			{
				script_element->data_bitmask |= DATA_BITMASK_INVERT_VALUE;
			}
			else
			{
				sprintf(error,"ILLEGAL INVERSION OF WORD '%s' OF FROM WORD LIST '%s' IN SCRIPT '%s'" , word_pointer , word_list[word_list_index].name , error_current_script);
				OUTPUT_message (error);
				assert(0);
			}
		}

		if (STRING_instr_char(word_pointer,'.',0) != -1) // Any dots in the middle? It's a word_1.word_2 one! Yay!
		{
			script_element->data_bitmask |= DATA_BITMASK_IDENTITY_VARIABLE;

			word_1 = strtok(word_pointer,".");
			word_2 = strtok(NULL,".");

			script_element->data_type = word_list[word_list_index].word_values[PARSER_is_whole_or_partial_word_in_list (word_list_index , word_1 )];
			script_element->data_value = word_list[word_list_index].word_values[PARSER_is_whole_or_partial_word_in_list (word_list_index , word_2 )];
			script_element->source_word_list_entry_1 = PARSER_is_whole_or_partial_word_in_list (word_list_index , word_1 );
			script_element->source_word_list_entry_2 = PARSER_is_whole_or_partial_word_in_list (word_list_index , word_2 );
		}
		else
		{
			script_element->data_type = word_list[word_list_index].data_type;
			script_element->data_value = word_list[word_list_index].word_values[PARSER_is_whole_or_partial_word_in_list (word_list_index , word_pointer)];
			script_element->source_word_list_entry_1 = PARSER_is_whole_or_partial_word_in_list (word_list_index , word_pointer );
			script_element->source_word_list_entry_2 = UNSET;
		}
	}

}



bool PARSER_do_word_or_words_exist_in_list ( int word_list_index , char *word , int *index_1 , int *index_2 )
{
	// First of all we can exit early for numbers...

	if ( (word_list_index == number_list_index) || (word_list_index == offset_list_index) )
	{
		return ( STRING_is_it_a_number (word) );
	}
	else
	{
		// By turns we can also exit early if we aren't looking for a number and we find one...

		if ( STRING_is_it_a_number (word) == true)
		{
			return false;
		}
	}

	// If the word is of the "word_1.word_2" type then we need to check both of 'em.

	char *word_1;
	char *word_2;

	bool non_maths_word_negated = false;
	bool non_maths_word_inverted = false;

	char error[MAX_LINE_SIZE];

	char word_backup[NAME_SIZE];
	char *word_pointer;

	strcpy (word_backup,word); // Need to back it up as we're strtok'ing it.

	word_pointer = word_backup;

	if ( (STRING_instr_char(word_pointer,'-',0) != -1) && (word_list_index != math_list_index) )
	{
		word_pointer++;
		
		non_maths_word_negated = true;
	}

	if ( (STRING_instr_char(word_pointer,'!',0) != -1) && (word_list_index != math_list_index) && (word_list_index != compare_list_index) && (word_list_index != math_compare_list_index) )
	{
		word_pointer++;
		
		non_maths_word_inverted = true;
	}

	if (STRING_instr_char(word_pointer,'.',0) != -1) // Any dots in the middle? It's a word_1.word_2 one! Yay!
	{
		word_1 = strtok(word_pointer,".");
		word_2 = strtok(NULL,".");

		if (word_2 == NULL)
		{
			sprintf(error,"WORD1.WORD2 missing WORD2 '%s' IN SCRIPT '%s'" , word_backup , error_current_script);
			OUTPUT_message(error);
			assert(0);
		}

		if ( (*index_1 = PARSER_is_whole_or_partial_word_in_list (word_list_index , word_1 ) ) == UNSET)
		{
			return false;
		}

		if ( (*index_2 = PARSER_is_whole_or_partial_word_in_list (word_list_index , word_2 ) ) == UNSET)
		{
			return false;
		}
	}
	else
	{
		if ( (*index_1 = PARSER_is_whole_or_partial_word_in_list (word_list_index , word_pointer ) ) == UNSET)
		{
			return false;
		}
	}

	// We've found a match in the list! Lets hope it wasn't illegally negated, the scamp!

	if ( (word_list_index != variable_list_index) && (word_list_index != constant_list_index) && (non_maths_word_negated) )
	{
		sprintf(error,"ILLEGAL NEGATION OF WORD '%s' OF FROM WORD LIST '%s'" , word_pointer , word_list[word_list_index].name );
		OUTPUT_message (error);
		assert(0);
	}

	if ( (word_list_index != variable_list_index) && (word_list_index != constant_list_index) && (non_maths_word_inverted) )
	{
		sprintf(error,"ILLEGAL INVERSION OF WORD '%s' OF FROM WORD LIST '%s'" , word_pointer , word_list[word_list_index].name );
		OUTPUT_message (error);
		assert(0);
	}

	return true;
}



void PARSER_clear_text_script (void)
{
	int t,i;

	for (t=0; t<script_length; t++)
	{
		// Free up the individual arguments.

		for (i=0; i<script[t].line_length; i++)
		{
			if (script[t].text_word_list[i].text_word != NULL)
			{
				free(script[t].text_word_list[i].text_word);
				// Don't need to set to NULL as it'll all be deleted in a mo'...
			}
		}

		// Then free up the pointer to the list of arguments.
		if (script[t].text_word_list != NULL)
		{
			free (script[t].text_word_list);
			// Don't need to set to NULL as it'll all be deleted in a mo'...
		}
	}
	
	if (script != NULL)
	{
		free (script);
		script = NULL;
	}

	script_length = 0;
}



void PARSER_add_line_to_text_script (void)
{
	// Just gets another line of the script ready for use.

	if (script == NULL) // First line!
	{
		script_length = 0;
		script = (text_argument_line *) malloc (sizeof (text_argument_line));
		script[script_length].line_length = 0;
		script[script_length].text_word_list = NULL;
	}
	else
	{
		script_length++; // Finish off previous line by pointing to the new one.
		script = (text_argument_line *) realloc (script , ((1+script_length) * sizeof (text_argument_line)) );
		script[script_length].line_length = 0;
		script[script_length].text_word_list = NULL;
	}
}



char *PARSER_get_alias_by_index (int alias_index)
{
	return (&aliases_from[alias_index][0]);
}



char *PARSER_find_alias_section (char *word, int *alias_index)
{
	int t;
	int found_alias = UNSET;
	char word_copy[MAX_LINE_SIZE];

	static char composite_word[MAX_LINE_SIZE];

	if ( (word[0] == '-') && (word[1] != '\0') )
	{
		strcpy (composite_word,"-");
		strcpy (word_copy,&word[1]);
	}
	else
	{
		strcpy (composite_word,"");
		strcpy (word_copy,word);
	}

	if (word[0] == '!')
	{
		strcpy (composite_word,"!");
		strcpy (word_copy,&word[1]);
	}

	for (t=0; t<alias_count; t++)
	{
		if (strcmp (&aliases_from[t][0] , word_copy) == 0)
		{
			found_alias = t;
			*alias_index = found_alias;
		}
	}

	if (found_alias != UNSET)
	{
		strcat (composite_word,&aliases_to[found_alias][0]);
	}
	else
	{
		strcat (composite_word,word_copy);
	}

	return composite_word;
}



char *PARSER_find_alias(char *word, int *alias_1_index, int *alias_2_index)
{
	static char composite_word[MAX_LINE_SIZE];
	char word_copy[MAX_LINE_SIZE];
	int full_stop;

	*alias_1_index = UNSET;
	*alias_2_index = UNSET;

	if ((full_stop = STRING_instr_char(word,'.',0)) != UNSET)
	{
		strcpy (word_copy,word);
		strcpy (composite_word,"");

		word_copy[full_stop] = '\0';

		strcat (composite_word , PARSER_find_alias_section(word_copy,alias_1_index));

		strcat (composite_word , ".");

		strcat (composite_word , PARSER_find_alias_section(&word_copy[full_stop+1],alias_2_index));
	}
	else
	{
		strcpy (composite_word , PARSER_find_alias_section(word,alias_1_index) );
	}

	return composite_word;
}



void PARSER_add_word_to_text_script_line (char *word, bool parsing_actual_script)
{
	// Adds a word to the current line of the script.

	// First of all it checks for aliases, though.

	char *pointer;

	int alias_1_index;
	int alias_2_index;

	if (parsing_actual_script)
	{
		pointer = PARSER_find_alias(word,&alias_1_index,&alias_2_index);
	}
	else
	{
		pointer = word;
	}

	if ( (script[script_length].text_word_list == NULL) && (script[script_length].line_length == 0) ) // If either is true, both should be true!
	{
		script[script_length].text_word_list = (text_argument *) malloc (sizeof(text_argument));
	}
	else
	{
		script[script_length].text_word_list = (text_argument *) realloc (script[script_length].text_word_list , (script[script_length].line_length + 1) * (sizeof(text_argument)) );
	}

	script[script_length].text_word_list[script[script_length].line_length].text_word = (char *) malloc (sizeof(char) * (strlen(pointer)+1));
	
	if (parsing_actual_script)
	{
		script[script_length].text_word_list[script[script_length].line_length].alias_1_index = alias_1_index;
		script[script_length].text_word_list[script[script_length].line_length].alias_2_index = alias_2_index;
	}

	strcpy ( script[script_length].text_word_list[script[script_length].line_length].text_word , pointer );
	script[script_length].line_length++; // Point at next word in readiness.
}



void PARSER_finish_script (void)
{
	// Yeah, it's a tiny function I know but it's nicely formalised this way.

	script_length++;
}



void PARSER_clear_word_list (int word_list_index)
{
	// Really only used to clear the label list for each script that's loaded.

	int t;
	char *pointer;

	// As this routine is called by the script loader (which also loads the descriptor file)
	// we need to make sure that word_list ain't NULL as it would be before any word lists are
	// loaded. Okay?
	if (word_list != NULL)
	{
		if (word_list[word_list_index].words != NULL)
		{

			for (t=0; t<word_list[word_list_index].word_list_size; t++)
			{
				pointer = word_list[word_list_index].words[t].text_word;
				if (word_list[word_list_index].words[t].text_word != NULL)
				{
					free (word_list[word_list_index].words[t].text_word);
					// Don't bother resetting to NULL as it'll be gone in a mo'.
				}
			}

			free (word_list[word_list_index].words);
			word_list[word_list_index].words = NULL;
		}

		if (word_list[word_list_index].word_values != NULL)
		{
			free (word_list[word_list_index].word_values);
			word_list[word_list_index].word_values = NULL;
		}

		word_list[word_list_index].word_list_size = 0;
	}

}



void PARSER_add_to_word_list (int word_list_index, char *word, int word_value)
{
	// Adds a word to the given list of the given name and given value. Given.

	if ( (word_list[word_list_index].word_list_size == 0) && (word_list[word_list_index].words == NULL) && (word_list[word_list_index].word_values == NULL) )
	{
		// All three checks should pass or all three should fail, really.
		// Malloc some memory.

		word_list[word_list_index].words = (text_argument *) malloc ( sizeof (text_argument) );
		word_list[word_list_index].word_values = (int *) malloc ( sizeof (int) );
	}
	else
	{
		// Realloc some more, daddio!
		word_list[word_list_index].words = (text_argument *) realloc ( word_list[word_list_index].words , (word_list[word_list_index].word_list_size + 1) * sizeof (text_argument) );
		word_list[word_list_index].word_values = (int *) realloc ( word_list[word_list_index].word_values , (word_list[word_list_index].word_list_size + 1) * sizeof (int) );
	}

	int a = word_list[word_list_index].word_list_size;
	int b = word_list_index;
	word_list[word_list_index].words[word_list[word_list_index].word_list_size].text_word = (char *) malloc (sizeof(char) * (strlen(word)+1) );

	strcpy ( word_list[word_list_index].words[word_list[word_list_index].word_list_size].text_word , word );
	word_list[word_list_index].word_values[word_list[word_list_index].word_list_size] = word_value;

	word_list[word_list_index].word_list_size++;
}



void PARSER_split_line_into_arguments (char *line, bool parsing_actual_script)
{
	// Strtok's it's way through the line chucking out tabs and spaces then storing each word.

	char *pointer;

	// Point at first word in the line...
	pointer = strtok(line,"\t ");

	do
	{
		PARSER_add_word_to_text_script_line (pointer,parsing_actual_script);
	} while ( (pointer = strtok(NULL,"\t ")) != NULL);

}



void PARSER_compiler_directive (char *line)
{
	char line_backup[MAX_LINE_SIZE];
	char error[MAX_LINE_SIZE];
	strcpy (line_backup,line);

	char *pointer = strtok(line," \n\t");

	if (strcmp(pointer,"#DEFINE") == 0)
	{
		pointer = strtok(NULL," \n\t");

		if (pointer==NULL)
		{
			sprintf(error,"#DEFINE MISSING 2 ARGUMENTS IN SCRIPT '%s'" , error_current_script );
			OUTPUT_message (error);
			assert(0);
		}

		// Pointer now points at word we will look for...
		strcpy (&aliases_from[alias_count][0] , pointer);

		pointer = strtok(NULL," \n\t");

		if (pointer==NULL)
		{
			sprintf(error,"#DEFINE MISSING 1 ARGUMENT AFTER '%s' IN SCRIPT '%s'" ,line_backup, error_current_script );
			OUTPUT_message (error);
			assert(0);
		}

		// Pointer now points at word we will replace it with...
		strcpy (&aliases_to[alias_count][0] , pointer);

		alias_count++;
	}
}



void PARSER_read_script (char *filename, bool parsing_actual_script=false, bool remove_commas_and_brackets=false)
{
	// This reads in the script a line at a time, ignoring those lines which don't start with either a
	// letter or a "." (indicating a label). And when it finds a label it just stores it on the label
	// word list and increases its size. After that it returns the number of lines in the script.

	char error[TEXT_LINE_SIZE];

	char line[TEXT_LINE_SIZE];
	// This is what we read a line of the file into before splitting it up.

	int comment_position;
	bool in_block_comment = false;

	alias_count = 0;

	// First of all, though, we blank the script array because that's where we're gonna' be putting stuff.
	PARSER_clear_text_script ();

	// Clear the label list out.
	PARSER_clear_word_list (label_list_index);

	FILE *file_pointer = fopen (filename,"r");

	if (file_pointer == NULL)
	{
		sprintf (error,"ERROR! SCRIPT '%s' NOT FOUND FOR LOADING! YOU FLIPPIN' MUPPET!",filename);
		OUTPUT_message (error);
		assert (0);
	}

	while ( fgets ( line , TEXT_LINE_SIZE , file_pointer ) != NULL )
	{
		// Uppercase it.

		strupr (line);

		if (remove_commas_and_brackets)
		{
			// This removes the commas and brackets in the scripts
			// which don't actually *do* anything but merely serve
			// as aids to readability.
			STRING_replace_char (line,',',' ');
			STRING_replace_char (line,'(',' ');
			STRING_replace_char (line,')',' ');
		}

		// See if there are any comments on the line.


		if (in_block_comment)
		{
			if (STRING_instr_word (line , "*/" , 0 ) != UNSET)
			{
				in_block_comment = false;
			}
		}
		else
		{
			if (STRING_instr_word (line , "/*" , 0 ) != UNSET)
			{
				in_block_comment = true;
			}
		}

		comment_position = STRING_instr_word (line , "//" , 0 );

		if (comment_position != UNSET)
		{
			// And if there are, end the line just before them.
			line[comment_position] = '\0';
		}

		if (in_block_comment == false)
		{
			STRING_strip_all_disposable (line);

			if (strlen(line) > 0)
			{
				// If there's anything left of the line, see what it is...

				if (line[0] == '.') // Label!
				{
					PARSER_add_to_word_list (label_list_index, &line[1] , script_length);
				}
				else if ( (line[0] == '#') && (parsing_actual_script) )
				{
					PARSER_compiler_directive (line);
				}
				else // Line!
				{
					PARSER_add_line_to_text_script ();
					PARSER_split_line_into_arguments (line,parsing_actual_script);
				}
			}
		}

	}

	fclose (file_pointer);

}



int PARSER_find_word_list_name ( char *name )
{
	int t;

	for (t=0; t<number_of_word_lists_loaded ; t++)
	{
		if (strcmp (name , word_list[t].name) == 0)
		{
			return t;
		}
	}

	return UNSET;
}



void PARSER_check_for_special_list (char *list_name, int list_index)
{

	if (strcmp(list_name,"VARIABLE")==0) // Check for it being a VARIABLE tag.
	{
		variable_list_index = list_index;
	}
	else if (strcmp(list_name,"LABEL")==0) // Check for it being a LABEL tag.
	{
		label_list_index = list_index;
	}
	else if (strcmp(list_name,"NUMBER")==0) // Check for it being a NUMBER tag.
	{
		number_list_index = list_index;
	}
	else if (strcmp(list_name,"IGNORE")==0) // Check for it being a IGNORE tag.
	{
		ignore_list_index = list_index;
	}
	else if (strcmp(list_name,"SCRIPTS")==0) // Check for it being a SCRIPTS tag.
	{
		script_list_index = list_index;
	}
	else if (strcmp(list_name,"OFFSET")==0) // Check for it being a OFFSET tag.
	{
		offset_list_index = list_index;
	}
	else if (strcmp(list_name,"CONSTANT")==0) // Check for it being a CONSTANT tag.
	{
		constant_list_index = list_index;
	}
	else if (strcmp(list_name,"COMPARE_OPERATION")==0)
	{
		compare_list_index = list_index;
	}
	else if (strcmp(list_name,"MATH_COMPARATIVE")==0)
	{
		math_compare_list_index = list_index;
	}
	else if (strcmp(list_name,"MATH_OPERATION")==0) // Oh, you know what it's for...
	{
		math_list_index = list_index;
	}
	
}



void PARSER_create_new_word_list (void)
{
	// This just mallocs or reallocs the space needed.

	if ( (number_of_word_lists_loaded == 0) && (word_list == NULL) ) // If one's true then the other must be too! Well, almost.
	{
		word_list = (word_list_struct *) malloc (sizeof (word_list_struct) );
	}
	else
	{
		word_list = (word_list_struct *) realloc (word_list , (number_of_word_lists_loaded + 1) * sizeof (word_list_struct) );
	}

	strcpy (word_list[number_of_word_lists_loaded].name , "UNSET");
	strcpy (word_list[number_of_word_lists_loaded].extension , "UNSET");

	word_list[number_of_word_lists_loaded].has_extension = false;
	word_list[number_of_word_lists_loaded].data_type = UNSET;

	word_list[number_of_word_lists_loaded].word_values = NULL;
	word_list[number_of_word_lists_loaded].words = NULL;
	word_list[number_of_word_lists_loaded].word_list_size = 0;
}



void PARSER_finish_word_list (void)
{
	// Again, tiny function but keeps things readable.

	number_of_word_lists_loaded++;
}



void PARSER_create_empty_list (int line_number)
{
	int index;

	PARSER_create_new_word_list ();

	index = number_of_word_lists_loaded;

	strcpy ( word_list[index].name , script[line_number].text_word_list[1].text_word );
	word_list[index].data_type = atoi ( script[line_number].text_word_list[2].text_word );

	PARSER_check_for_special_list ( word_list[index].name , index );

	// We need to finish off the list we made...
	PARSER_finish_word_list ();
}



#define SOURCE_SCRIPT				(0) // Values in the language descriptor
#define SOURCE_FILE					(1) // Values in a file
#define SOURCE_DIRECTORY			(2) // List of files in directory
#define SOURCE_WORD_LIST			(3) // Loads the files in a given word list and looks at their contents
#define SOURCE_INDEX_LIST			(4)	// Blim!

int PARSER_parse_list ( int line_number, int data_source, bool autonumber=false , bool exponential_numbering=false)
{
	char error[TEXT_LINE_SIZE]; // If we're using a text file we might need to burp out an error if it ain't there...
	char file_text_line[TEXTFILE_SUPER_SIZE]; // If we're parsing from a text file...
	char filename[NAME_SIZE];
	char *pointer_1;
	char *pointer_2;
	char *dir_pointer;
	FILE *file_pointer = NULL;
	bool append;
	int index;
	int source_list_index;
	int last_value;
	int word_list_counter;
	int index_counter;

	int autonumber_counter = 0;

	if ((data_source == SOURCE_WORD_LIST) || (data_source == SOURCE_INDEX_LIST))
	{
		if (PARSER_find_word_list_name ( script[line_number].text_word_list[3].text_word ) != UNSET)
		{
			source_list_index = PARSER_find_word_list_name ( script[line_number].text_word_list[3].text_word );
		}
		else
		{
			// Fook!
			sprintf (error,"ERROR! WORD LIST '%s' NOT FOUND FOR INDEX/FILE LISTING!",script[line_number].text_word_list[3].text_word);
			OUTPUT_message (error);
		}
	}

	// Firstly see if the name of this list has been used before and if it has we just set append to true.
	if (PARSER_find_word_list_name ( script[line_number].text_word_list[1].text_word ) != UNSET)
	{
		append = true;

		// We need to find the old list...
		index = PARSER_find_word_list_name ( script[line_number].text_word_list[1].text_word );

		// Which obviously has all the other guff set up already, thankyouverymuch!
	}
	else
	{
		append = false;

		// We need to create a new list...
		PARSER_create_new_word_list ();
		index = number_of_word_lists_loaded;

		strcpy ( word_list[index].name , script[line_number].text_word_list[1].text_word );
		word_list[index].data_type = atoi ( script[line_number].text_word_list[2].text_word );

		if (data_source == SOURCE_DIRECTORY)
		{
			strcpy ( word_list[index].extension , script[line_number].text_word_list[3].text_word );
			word_list[index].has_extension = true;
		}

		// See if this is the NUMBER, LABEL, VARIABLE or IGNORE list. They are magic! ;)
		PARSER_check_for_special_list ( word_list[index].name , index );
	}

	line_number++;

	switch (data_source)
	{
	case SOURCE_SCRIPT: // -------- READING LIST FROM SCRIPT --------

		while ( strcmp ( script[line_number].text_word_list[0].text_word , "#END" ) != 0 )
		{
			if (autonumber == true)
			{
				PARSER_add_to_word_list (index, script[line_number].text_word_list[0].text_word, autonumber_counter);
			}
			else
			{
				PARSER_add_to_word_list (index, script[line_number].text_word_list[1].text_word, atoi(script[line_number].text_word_list[0].text_word) );
			}
			
			if (exponential_numbering == true)
			{
				autonumber_counter *= 2;
			}
			else
			{
				autonumber_counter++;
			}
				
			line_number++;
		}

		break;

	case SOURCE_FILE: // -------- READING LIST FROM FILE --------

		strcpy( filename , word_list[index].name );
		strcat( filename , ".TXT" );
	
		file_pointer = fopen (MAIN_get_project_filename(filename),"r");

		if (file_pointer != NULL)
		{
			while ( fgets ( file_text_line , TEXTFILE_SUPER_SIZE , file_pointer ) != NULL )
			{
				// Uppercase it.

				strupr (file_text_line);

				pointer_1 = strtok (file_text_line,"\r\n\t ");

				// If it's not just an emtpy line or a comment...
				if ( (strlen(file_text_line)>0) && ( (file_text_line[0] != '/') && (file_text_line[1] != '/') ) && (pointer_1 != NULL) )
				{
					if (autonumber == true)
					{
						PARSER_add_to_word_list (index , pointer_1 , autonumber_counter);
					}
					else
					{
						pointer_2 = strtok (NULL,"\r\n\t ");
						if (pointer_2 != NULL)
						{
							last_value = atoi(pointer_1);	
						}

						PARSER_add_to_word_list (index , pointer_2 , last_value );
						
						last_value++;
					}

					if (exponential_numbering == true)
					{
						autonumber_counter *= 2;
					}
					else
					{
						autonumber_counter++;
					}

				}
			}
		}
		else
		{
			sprintf (error,"ERROR! FILE '%s' NOT FOUND FOR LISTING!",filename);
			OUTPUT_message (error);
		}

		fclose (file_pointer);

		break;

	case SOURCE_DIRECTORY: // -------- READING LIST FROM DIRECTORY --------
		
		dir_pointer = FILE_open_dir ( MAIN_get_project_filename(word_list[index].name) , word_list[index].extension, true);

		if (dir_pointer != NULL) // If there's anything in the directory (and it exists)
		{
			do
			{
				// Copy and cut off anything past the ".";

				strcpy (filename,dir_pointer);
				strtok (filename,".");

				// Always autonumbered...

				PARSER_add_to_word_list (index , filename , autonumber_counter);
				
				if (exponential_numbering == true)
				{
					autonumber_counter *= 2;
				}
				else
				{
					autonumber_counter++;
				}

			} while ( (dir_pointer = FILE_read_dir_entry (true)) != NULL);
		}				

		break;

	case SOURCE_WORD_LIST:
		// This looks the given word list and makes a filename out of each entry before loading it
		// and reading the contents.

		for (word_list_counter=0; word_list_counter<word_list[source_list_index].word_list_size; word_list_counter++)
		{
			// Create a valid filename from the directory entry...
		  append_filename(filename, word_list[source_list_index].name, word_list[source_list_index].words[word_list_counter].text_word, sizeof(filename) );
		  strcat (filename , word_list[source_list_index].extension);
			
			// Then open it and read it just like with the SOURCE_FILE stuff
			file_pointer = fopen (MAIN_get_project_filename(filename),"r");

			if (file_pointer != NULL)
			{
				while ( fgets ( file_text_line , TEXTFILE_SUPER_SIZE , file_pointer ) != NULL )
				{
					// Uppercase it.

					strupr (file_text_line);

					pointer_1 = strtok (file_text_line,"\r\n\t ");

					// If it's not just an emtpy line or a comment...
					if ( (strlen(file_text_line)>0) && ( (file_text_line[0] != '/') && (file_text_line[1] != '/') ) && (pointer_1 != NULL) )
					{
						if (autonumber == true)
						{
							PARSER_add_to_word_list (index , pointer_1 , autonumber_counter);
						}
						else
						{
							pointer_2 = strtok (NULL,"\r\n\t ");
							if (pointer_2 != NULL)
							{
								last_value = atoi(pointer_1);	
							}

							PARSER_add_to_word_list (index , pointer_2 , last_value );
							
							last_value++;
						}

						if (exponential_numbering == true)
						{
							autonumber_counter *= 2;
						}
						else
						{
							autonumber_counter++;
						}
						
					}
				}
			}
			else
			{
				sprintf (error,"ERROR! FILE '%s' NOT FOUND FOR LISTING!",filename);
				OUTPUT_message (error);
			}

			fclose (file_pointer);

		}

		// Because we didn't pluck anything from the script...
		line_number--;

		break;

	case SOURCE_INDEX_LIST:
		// This looks the given word list and makes a filename out of each entry before loading it
		// and reading the contents, but it only adds the words after "." tags

		// As this is only used for the datatables, it also does a few more bits and bobs to do with
		// malloc'ing the memory that will be necessary to interpret the datatables later on.

		number_of_datatables = word_list[source_list_index].word_list_size;

		datatable_data = (datatable_struct *) malloc (sizeof(datatable_struct) * number_of_datatables);

		for (word_list_counter=0; word_list_counter<word_list[source_list_index].word_list_size; word_list_counter++)
		{
			// Create a valid filename from the directory entry...
			append_filename(filename , word_list[source_list_index].name, word_list[source_list_index].words[word_list_counter].text_word, sizeof(filename) );
			strcat (filename , word_list[source_list_index].extension);
			
			// Then open it and read it just like with the SOURCE_FILE stuff
			file_pointer = fopen (MAIN_get_project_filename(filename),"r");

			if (file_pointer != NULL)
			{
				autonumber_counter = 0;
				index_counter = 0;

				while ( fgets ( file_text_line , TEXTFILE_SUPER_SIZE , file_pointer ) != NULL )
				{
					// Uppercase it.

					strupr (file_text_line);

					pointer_1 = strtok (file_text_line,"\r\n\t ");

					// If it's not just an emtpy line or a comment...
					if ( (strlen(file_text_line)>0) && ( (file_text_line[0] != '/') && (file_text_line[1] != '/') ) && (pointer_1 != NULL) )
					{
						if ( pointer_1[0] == '.' )
						{
							// It's a label! Woot!
							PARSER_add_to_word_list (index , &pointer_1[1] , index_counter);
							index_counter++;
						}
						else if ( strcmp(pointer_1,"#DATA") == 0)
						{
							// It's a line of data...
							autonumber_counter++;
						}
					}
				}

				datatable_data[word_list_counter].lines = autonumber_counter;
				datatable_data[word_list_counter].line_size = UNSET;
				datatable_data[word_list_counter].index_count = index_counter; 

				datatable_data[word_list_counter].data = NULL;
				datatable_data[word_list_counter].index_list = NULL;
			}
			else
			{
				sprintf (error,"ERROR! FILE '%s' NOT FOUND FOR LISTING!",filename);
				OUTPUT_message (error);
			}

			fclose (file_pointer);
		}

		// Because we didn't pluck anything from the script...
		line_number--;
		break;

	default:
		assert(0); // Should never happen!
		break;
	}

	if (append == false)
	{
		// We need to finish off the list we made...
		PARSER_finish_word_list ();
	}

	return line_number;
}



void PARSER_clear_command_syntax_list (void)
{
	// Pretty simple, this.

	int t,i;

	for (t=0; t<total_command_syntax_archetypes ; t++)
	{
		for (i=0; i<command_syntax_archetype_list[t].argument_list_size; i++)
		{
			if (command_syntax_archetype_list[t].argument_list[i].alternate_list != NULL)
			{
				free (command_syntax_archetype_list[t].argument_list[i].alternate_list);
				// Don't NULL it, it'll be gone soon...
			}
		}

		if (command_syntax_archetype_list[t].argument_list != NULL)
		{
			free (command_syntax_archetype_list[t].argument_list);
			// Don't NULL it, it'll be gone soon...
		}
	}

	if (command_syntax_archetype_list != NULL)
	{
		free (command_syntax_archetype_list);
		command_syntax_archetype_list = NULL;
	}

	total_command_syntax_archetypes = 0;
}



void PARSER_create_new_command_syntax_archetype (void)
{
	// This creates space for a new command syntax archetype.

	if ( (total_command_syntax_archetypes == 0) && (command_syntax_archetype_list == NULL) )
	{
		command_syntax_archetype_list = (command_syntax_archetype *) malloc (sizeof(command_syntax_archetype));
	}
	else
	{
		command_syntax_archetype_list = (command_syntax_archetype *) realloc ( command_syntax_archetype_list , (total_command_syntax_archetypes + 1 ) * sizeof(command_syntax_archetype) );
	}

	command_syntax_archetype_list[total_command_syntax_archetypes].argument_list = NULL;
	command_syntax_archetype_list[total_command_syntax_archetypes].argument_list_size = 0;
	command_syntax_archetype_list[total_command_syntax_archetypes].prefix_value = UNSET;
	command_syntax_archetype_list[total_command_syntax_archetypes].prefix_value_type = UNSET;
}



void PARSER_create_new_command_syntax_archetype_argument (void)
{
	int argument_list_size;	
	argument_list_size = command_syntax_archetype_list[total_command_syntax_archetypes].argument_list_size;

	// This creates a bit of space for a new argument in the current command syntax archetype.

	if ( (argument_list_size == 0) && (command_syntax_archetype_list[total_command_syntax_archetypes].argument_list == NULL) )
	{
		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list = (command_syntax_argument *) malloc (sizeof (command_syntax_argument));
	}
	else
	{
		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list = (command_syntax_argument *) realloc ( command_syntax_archetype_list[total_command_syntax_archetypes].argument_list , (argument_list_size + 1) * sizeof (command_syntax_argument) );
	}

	if (argument_list_size > maximum_command_syntax_size)
	{
		maximum_command_syntax_size = argument_list_size;
	}

	command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list = NULL;
	command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list_size = 0;
}



void PARSER_add_argument_description (char *word)
{
	int argument_list_size;	

	argument_list_size = command_syntax_archetype_list[total_command_syntax_archetypes].argument_list_size;

	strcpy (command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].argument_description , word);
}



void PARSER_add_argument_type (char *word)
{
	int argument_list_size;	
	int letter_number;
	int bitvalue;

	argument_list_size = command_syntax_archetype_list[total_command_syntax_archetypes].argument_list_size;

	if (word == NULL)
	{
		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].argument_type = UNSET;
	}
	else
	{
		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].argument_type = 0;

		for (letter_number=0; letter_number<int(strlen(word)); letter_number++)
		{
			bitvalue = 1 << int (STRING_instr_char (command_syntax_archetype_identifiers,word[letter_number],0));
			command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].argument_type |= bitvalue;
		}
	}
}



void PARSER_reset_argument_type (void)
{
	int argument_list_size;	

	argument_list_size = command_syntax_archetype_list[total_command_syntax_archetypes].argument_list_size;

	command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].argument_type = 0;
}



void PARSER_add_alternate (char *word)
{
	int alternate_list_size;
	int argument_list_size;	

	argument_list_size = command_syntax_archetype_list[total_command_syntax_archetypes].argument_list_size;
	alternate_list_size = command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list_size;

	// First of all malloc the room for a new alternate;

	if ( (alternate_list_size == 0) && (command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list == NULL) )
	{
		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list = (archetype *) malloc (sizeof(archetype));
	}
	else
	{
		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list = (archetype *) realloc (command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list , (alternate_list_size + 1) * sizeof(archetype));
	}

	// Ok, now let's have a look at the word.
	
	int word_list_index;
	int word_list_specific_entry;

	char *pointer_1;
	char *pointer_2;
	
	// If it contains a ":" then we know it's a specific type.

	if (STRING_instr_char(word,':',0) != UNSET)
	{
		pointer_1 = strtok(word,":");
		pointer_2 = strtok(NULL,":");

		word_list_index = PARSER_find_word_list_name (pointer_1);

		word_list_specific_entry = PARSER_is_word_in_list ( word_list_index , pointer_2 , false );

		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list[alternate_list_size].specific_list_entry = word_list_specific_entry;
		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list[alternate_list_size].word_list_index = word_list_index;
	}
	else
	{
		word_list_index = PARSER_find_word_list_name (word);

		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list[alternate_list_size].specific_list_entry = UNSET;
		command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list[alternate_list_size].word_list_index = word_list_index;
	}

	command_syntax_archetype_list[total_command_syntax_archetypes].argument_list[argument_list_size].alternate_list_size++;

}



void PARSER_finish_command_syntax_archetype_argument (void)
{
	command_syntax_archetype_list[total_command_syntax_archetypes].argument_list_size++;
}



void PARSER_finish_command_syntax_archetype (void)
{
	total_command_syntax_archetypes++;
}



void PARSER_destroy_matching_alternate_store (void)
{
	if (matching_alternate_store != NULL)
	{
		free (matching_alternate_store);
		matching_alternate_store = NULL;
	}
}



void PARSER_create_matching_alternate_store (void)
{
	PARSER_destroy_matching_alternate_store ();
		
	int command_syntax_archetype_number;
	int max_command_syntax_archetype_length;

	max_command_syntax_archetype_length = 0;

	for (command_syntax_archetype_number = 0; command_syntax_archetype_number < total_command_syntax_archetypes ; command_syntax_archetype_number++)
	{
		if (command_syntax_archetype_list[command_syntax_archetype_number].argument_list_size > max_command_syntax_archetype_length)
		{
			max_command_syntax_archetype_length = command_syntax_archetype_list[command_syntax_archetype_number].argument_list_size;
		}
	}

	matching_alternate_store = (int *) malloc (max_command_syntax_archetype_length * sizeof (int));
}



void PARSER_add_syntax_parameter_list (int line_number)
{
	if (syntax_parameter_list == NULL)
	{
		syntax_parameter_list = (syntax_parameter_list_entry_struct *) malloc (sizeof (syntax_parameter_list_entry_struct));
	}
	else
	{
		syntax_parameter_list = (syntax_parameter_list_entry_struct *) realloc (syntax_parameter_list , sizeof (syntax_parameter_list_entry_struct) * (syntax_parameter_list_count+1) );
	}

	strcpy (syntax_parameter_list[syntax_parameter_list_count].name, script[line_number].text_word_list[1].text_word);
	strcpy (syntax_parameter_list[syntax_parameter_list_count].list, script[line_number].text_word_list[2].text_word);

	syntax_parameter_list_count++;
}



int PARSER_get_syntax_parameter_list_index (char *name)
{
	int t;

	for (t=0; t<syntax_parameter_list_count; t++)
	{
		if (strcmp (syntax_parameter_list[t].name , name) == 0)
		{
			return t;
		}
	}

	return UNSET;
}



bool PARSER_is_string_syntax_parameter_list (char *name)
{
	if (PARSER_get_syntax_parameter_list_index(name) != UNSET)
	{
		return true;
	}
	else
	{
		return false;
	}
}



char *PARSER_get_syntax_parameter_list (char *name)
{
	return (syntax_parameter_list[PARSER_get_syntax_parameter_list_index(name)].list);
}



int PARSER_create_command_syntax_list ( int line_number )
{
	int prefix_value_type,prefix_value;
	int starting_argument;
	int prefix_counter;
	int total_arguments;
	int argument_counter;
	char word_copy[TEXT_LINE_SIZE];
	char *pointer;
	int line_type;

	// First of all initialise the command_syntax_list... (although it should be blank already)

	PARSER_clear_command_syntax_list ();

	line_number++;

	// Okay, so what we do is go through the lines one by one until we get "#END" as an entry.

	while (strcmp ( script[line_number].text_word_list[0].text_word , "#END" ) != 0)
	{
		// Make some room, gentlemen...
		PARSER_create_new_command_syntax_archetype ();

		// How many words on this line?
		total_arguments = script[line_number].line_length;

		// Reset the prefix data;
		prefix_value_type = 0;
		prefix_value = 0;

		// First thing we do is see if the first word starts with a "(" as if it does we have
		// a chunk of prefix data.

		if (script[line_number].text_word_list[0].text_word[0] == '(')
		{
			prefix_counter = 1;

			line_type = TRACER_LINE_TYPE_NORMAL;

			while ( script[line_number].text_word_list[0].text_word[prefix_counter] != ')' )
			{

				if ( script[line_number].text_word_list[0].text_word[prefix_counter] == '<' )
				{
					prefix_value = prefix_value | UNINDENT_LINE;
					prefix_counter++;
					prefix_value_type = script[line_number].text_word_list[0].text_word[prefix_counter];
				}

				if ( script[line_number].text_word_list[0].text_word[prefix_counter] == '>' )
				{
					prefix_value = prefix_value | INDENT_LINE;
					prefix_counter++;
					prefix_value_type = script[line_number].text_word_list[0].text_word[prefix_counter];
				}

				if ( script[line_number].text_word_list[0].text_word[prefix_counter] == '@' )
				{
					prefix_value = prefix_value | STORE_EQUAL_INDENT_LINE;
				}

				if ( script[line_number].text_word_list[0].text_word[prefix_counter] == '?' )
				{
					prefix_value = prefix_value | STORE_LESSER_INDENT_LINE;
				}

				if ( script[line_number].text_word_list[0].text_word[prefix_counter] == '~' )
				{
					prefix_value = prefix_value | STORE_PREV_EQUAL_INDENT_LINE;
				}

				if ( script[line_number].text_word_list[0].text_word[prefix_counter] == 'Q' )
				{
					line_type = TRACER_LINE_TYPE_PROGRAM_FLOW;
				}

				if ( script[line_number].text_word_list[0].text_word[prefix_counter] == 'G' )
				{
					line_type = TRACER_LINE_TYPE_REDIRECT;
				}

				prefix_counter++;
			}

			starting_argument = 1; // Skip the first argument as it had this guff in it.
		}
		else
		{
			starting_argument = 0; // Start at the start, baby!
		}

		command_syntax_archetype_list[total_command_syntax_archetypes].prefix_value = prefix_value;
		command_syntax_archetype_list[total_command_syntax_archetypes].line_type = line_type;
		command_syntax_archetype_list[total_command_syntax_archetypes].prefix_value_type = prefix_value_type;

		for (argument_counter=starting_argument ; argument_counter<total_arguments ; argument_counter++)
		{
			// A little more room, please...
			PARSER_create_new_command_syntax_archetype_argument ();

			// Get the argument type, if any...
			strcpy (word_copy,script[line_number].text_word_list[argument_counter].text_word);

			pointer = word_copy;
			pointer = strtok ( pointer , "[]" );
			pointer = strtok ( NULL , "[]" );

			PARSER_add_argument_type (pointer);

			// Get the argument description, if any...
			pointer = word_copy;
			pointer = strtok ( pointer , "()" );
			pointer = strtok ( NULL , "()" );

			if (pointer != NULL)
			{
				PARSER_add_argument_description (pointer);
			}

			// Create the alternates...

			// Do this by first of all seeing if the current alternate is actually an
			// alternate list.
			pointer = word_copy;

			if (PARSER_is_string_syntax_parameter_list (pointer) == true)
			{
				strcpy(word_copy,PARSER_get_syntax_parameter_list(pointer));
			}

			pointer = strtok ( pointer , "|" );

			do
			{
				PARSER_add_alternate (pointer);
			} while ( ( pointer = strtok ( NULL , "|" ) ) != NULL );

			PARSER_finish_command_syntax_archetype_argument ();
		}

		line_number++;

		PARSER_finish_command_syntax_archetype ();
	}

	return line_number;

}



void PARSER_output_unused_commands_list ( int line_number )
{
	return;
	assert(0);
}



void PARSER_output_command_syntax_list ( int line_number )
{
	int t,i,j;

	char filename[NAME_SIZE];
	char line[TEXTFILE_SUPER_SIZE];
	char temp_line[TEXTFILE_SUPER_SIZE];

	int word_list_number;
	int specific_alternate;

	strcpy (filename , script[line_number].text_word_list[1].text_word);

	FILE *file_pointer = fopen (filename,"w");

	assert (file_pointer != NULL);

	for (t=0; t<total_command_syntax_archetypes; t++)
	{
		sprintf(line,"");

		for (i=0; i<command_syntax_archetype_list[t].argument_list_size; i++)
		{
			sprintf(temp_line,"");

			for (j=0; j<command_syntax_archetype_list[t].argument_list[i].alternate_list_size; j++)
			{
				word_list_number = command_syntax_archetype_list[t].argument_list[i].alternate_list[j].word_list_index;
				specific_alternate = command_syntax_archetype_list[t].argument_list[i].alternate_list[j].specific_list_entry;

				if (specific_alternate == UNSET)
				{
					strcat ( temp_line , word_list[word_list_number].name );
				}
				else
				{
					strcat ( temp_line , word_list[word_list_number].name );
					strcat ( temp_line , ":" );
					strcat ( temp_line , word_list[command_syntax_archetype_list[t].argument_list[i].alternate_list[j].word_list_index].words[specific_alternate].text_word );
				}

				if (j<command_syntax_archetype_list[t].argument_list[i].alternate_list_size - 1)
				{
					strcat (temp_line,"|");
				}
			}

			strcat (line,temp_line);
			strcat (line," ");
		}

		strcat(line,"\n");

		fputs(line,file_pointer);
	}

	fclose(file_pointer);

}



void PARSER_clear_temp_script (void)
{
	int l;

	for (l=0; l<temp_tokenised_script_length; l++)
	{
		if (temp_tokenised_script[l].script_line_pointer != NULL)
		{
			free (temp_tokenised_script[l].script_line_pointer);
		}
	}

	if (temp_tokenised_script != NULL)
	{
		free (temp_tokenised_script);
	}

	temp_tokenised_script = NULL;
	temp_tokenised_script_length = 0;
}



int PARSER_match_archetype_to_word_or_words (int archetype_number, int argument_number, char *word)
{
	// This just goes through the alternatives for the given archetype to see if the word fits into any of them.

	int index_1;
	int index_2;

	int alt_num;
	int specific;
	int arch_word_list_index;

	for (alt_num=0; alt_num<command_syntax_archetype_list[archetype_number].argument_list[argument_number].alternate_list_size ; alt_num++)
	{
		arch_word_list_index = command_syntax_archetype_list[archetype_number].argument_list[argument_number].alternate_list[alt_num].word_list_index;

		if (PARSER_do_word_or_words_exist_in_list ( arch_word_list_index , word , &index_1 , &index_2 ) == true)
		{
			// Okay, this means that "word" is in the list for this archetype, even if it's actually "word1.word2".

			specific = command_syntax_archetype_list[archetype_number].argument_list[argument_number].alternate_list[alt_num].specific_list_entry;

			if (specific != UNSET)
			{
				if (specific == index_1)
				{
					return alt_num;
				}
			}
			else
			{
				return alt_num;
			}
		}
	}

	return UNSET;
}



bool PARSER_does_archetype_match_script_line (int line_number , int archetype_number)
{
	int match_count;
	int target_match_count;
	int argument_number;

	// First of all checks whether the number of arguments in the script matches the number in the archetype.

	if ( command_syntax_archetype_list[archetype_number].argument_list_size != script[line_number].line_length )
	{
		return false; // If you got an exception here, you have an empty script you doofus!
	}

	// If we're still here then go through the script a word at a time and see if it matches the archetype.

	target_match_count = command_syntax_archetype_list[archetype_number].argument_list_size;
	match_count = 0;

	for (argument_number=0; argument_number<target_match_count ; argument_number++)
	{
		if ( ( matching_alternate_store[argument_number] = PARSER_match_archetype_to_word_or_words (archetype_number, argument_number, script[line_number].text_word_list[argument_number].text_word ) ) != UNSET )
		{
			match_count++;
		}
		else
		{
			break;
		}
	}

	if (match_count == target_match_count)
	{
		return true;
	}
	else
	{
		return false;
	}
}



int PARSER_find_matching_archetype_for_line (int line_number)
{
	int archetype_number;
	char temp_text[MAX_LINE_SIZE];

	for (archetype_number=0; archetype_number<total_command_syntax_archetypes; archetype_number++)
	{
		if ( PARSER_does_archetype_match_script_line (line_number , archetype_number) )
		{
			return archetype_number;
		}
	}

	char error[MAX_LINE_SIZE];

	int argument;

	sprintf(error,"ERROR = '");

	for (argument=0; argument<script[line_number].line_length; argument++)
	{
		strcat (error,script[line_number].text_word_list[argument].text_word);
		strcat (error," ");
	}

	sprintf(temp_text,"'\nIN SCRIPT '%s'",error_current_script);
	strcat(error,temp_text);
	OUTPUT_message(error);
	assert(0);

	return UNSET; // WOAH! This shouldn't happen!
}



void PARSER_create_temp_script_line (int prefix_value_type , int indentation_data , int archetype_number)
{
	if ( (temp_tokenised_script_length==0) && (temp_tokenised_script==NULL) )
	{
		temp_tokenised_script = (temp_script_line *) malloc (sizeof(temp_script_line));
	}
	else
	{
		temp_tokenised_script = (temp_script_line *) realloc ( temp_tokenised_script , (temp_tokenised_script_length+1) * sizeof(temp_script_line) );
	}

	temp_tokenised_script[temp_tokenised_script_length].archetype_number = archetype_number;
	temp_tokenised_script[temp_tokenised_script_length].prefix_value_type = prefix_value_type;
	temp_tokenised_script[temp_tokenised_script_length].indentation_data = indentation_data;
	temp_tokenised_script[temp_tokenised_script_length].script_line_pointer = NULL;
	temp_tokenised_script[temp_tokenised_script_length].script_line_size = 0;
}



void PARSER_add_temp_script_line_argument (int word_list_index , char *word , int matching_alternate , int line_number=UNSET , int alias_1_index=UNSET, int alias_2_index=UNSET)
{
	// Makes space in the temporary script for an argument and then stores it. Aw, bless.

	if (line_number == UNSET)
	{
		line_number = temp_tokenised_script_length;
		// If it's unset then we assume that we dump the command on the end of the last line.
	}

	int script_line_size = temp_tokenised_script[line_number].script_line_size;

	if (temp_tokenised_script[line_number].script_line_pointer == NULL)
	{
		temp_tokenised_script[line_number].script_line_pointer = (temp_script_data *) malloc (sizeof(temp_script_data));
	}
	else
	{
		temp_tokenised_script[line_number].script_line_pointer = (temp_script_data *) realloc ( temp_tokenised_script[line_number].script_line_pointer , (temp_tokenised_script[line_number].script_line_size + 1) * sizeof(temp_script_data));
	}

	PARSER_put_values_of_word_or_words ( word_list_index , word , &temp_tokenised_script[line_number].script_line_pointer[script_line_size] );
	temp_tokenised_script[line_number].script_line_pointer[script_line_size].alternate_number = matching_alternate;
	temp_tokenised_script[line_number].script_line_pointer[script_line_size].source_word_list = word_list_index;
	temp_tokenised_script[line_number].script_line_pointer[script_line_size].alias_1_index = alias_1_index;
	temp_tokenised_script[line_number].script_line_pointer[script_line_size].alias_2_index = alias_2_index;

	temp_tokenised_script[line_number].script_line_size++;
}



void PARSER_finish_temp_script_line (void)
{
	temp_tokenised_script_length++;
}



void PARSER_store_script_line (int line_number , int archetype_number , int indentation_data , int prefix_value_type )
{
	// Okay, we know this is the matching archetype for this line and so now we just
	// go through it an argument at a time, using the stored matching alternate.

	int argument_number;
	int matching_alternate;
	int word_list_index;

	PARSER_create_temp_script_line ( prefix_value_type , indentation_data , archetype_number );

	for (argument_number=0; argument_number<command_syntax_archetype_list[archetype_number].argument_list_size ; argument_number++)
	{
		matching_alternate = matching_alternate_store[argument_number];
		word_list_index = command_syntax_archetype_list[archetype_number].argument_list[argument_number].alternate_list[matching_alternate].word_list_index;

		if ( (word_list_index != ignore_list_index) || (debug == 1) ) // We don't store fluff unless debugging mode is on.
		{
			PARSER_add_temp_script_line_argument ( word_list_index , script[line_number].text_word_list[argument_number].text_word , matching_alternate , UNSET , script[line_number].text_word_list[argument_number].alias_1_index , script[line_number].text_word_list[argument_number].alias_2_index );
		}
	}

	PARSER_finish_temp_script_line ();
}



int PARSER_process_script (void)
{
	PARSER_clear_temp_script ();

	int indentation;
	int prefix_value_type;
	int archetype_number;
	int line_number;
	char error[MAX_LINE_SIZE];

	for (line_number=0; line_number<=script_length; line_number++)
	{
		if (line_number==41)

		{
			int a=0;
		}
		archetype_number = PARSER_find_matching_archetype_for_line (line_number);

		prefix_value_type = command_syntax_archetype_list[archetype_number].prefix_value_type;

		PARSER_store_script_line ( line_number , archetype_number , command_syntax_archetype_list[archetype_number].prefix_value , prefix_value_type );
	}

	int stack[256]; // This is as deep as indentation goes, matey!
	int stack_pointer = 0;
	int error_occurred = UNSET;

	indentation = 0;

	// Okay so first we just go through the lines checking that PUSHED and POPPED stack data matches up.

	for (line_number=0; line_number<temp_tokenised_script_length; line_number++)
	{
		if (temp_tokenised_script[line_number].indentation_data & UNINDENT_LINE)
		{
			if (stack[stack_pointer] != temp_tokenised_script[line_number].prefix_value_type)
			{
				error_occurred = line_number;
			}
			else
			{
				stack_pointer--;
			}
			indentation--;
		}

		temp_tokenised_script[line_number].indentation_level = indentation;

		if (stack_pointer < 0)
		{
			sprintf(error,"STACK POINTER MISMATCH IN SCRIPT '%s'",error_current_script);
			OUTPUT_message(error);
			assert(0); // Aw... crap!
		}

		if (temp_tokenised_script[line_number].indentation_data & INDENT_LINE)
		{
			stack_pointer++;
			stack[stack_pointer] = temp_tokenised_script[line_number].prefix_value_type;	
			indentation++;
		}
	}

	int other_line_number;
	char number_as_string[NAME_SIZE];

	for (line_number=0; line_number<temp_tokenised_script_length; line_number++)
	{
		other_line_number = line_number;

		if (temp_tokenised_script[line_number].indentation_data & STORE_EQUAL_INDENT_LINE)
		{
			do
			{
				other_line_number++;

				if (other_line_number == temp_tokenised_script_length)
				{
					sprintf(error,"COULD NOT FIND MATCHING INDENTATION IN SCRIPT '%s'",error_current_script);
					OUTPUT_message(error);
					assert(0); // FOOOOOOOOOK! What happened there then?
				}
			}
			while (temp_tokenised_script[line_number].indentation_level != temp_tokenised_script[other_line_number].indentation_level);

			sprintf (number_as_string,"%d",other_line_number);
			
			// Dump the offset to the next line of equal indentation after the end of the current line.
			PARSER_add_temp_script_line_argument (offset_list_index , number_as_string , UNSET , line_number);
		}
		else if (temp_tokenised_script[line_number].indentation_data & STORE_LESSER_INDENT_LINE)
		{
			do
			{
				other_line_number++;

				if (other_line_number == temp_tokenised_script_length)
				{
					sprintf(error,"COULD NOT FIND LESSER INDENTATION IN SCRIPT '%s'",error_current_script);
					OUTPUT_message(error);
					assert(0); // FOOOOOOOOOK! What happened there then?
				}
			}
			while (temp_tokenised_script[line_number].indentation_level - 1 != temp_tokenised_script[other_line_number].indentation_level);

			sprintf (number_as_string,"%d",other_line_number);
			
			// Dump the offset to the next line of lesser indentation after the end of the current line.
			PARSER_add_temp_script_line_argument (offset_list_index , number_as_string , UNSET , line_number);
		}
		else if (temp_tokenised_script[line_number].indentation_data & STORE_PREV_EQUAL_INDENT_LINE)
		{
			do
			{
				other_line_number--;

				if (other_line_number < 0)
				{
					sprintf(error,"COULD NOT FIND PREVIOUS MATCHING INDENTATION IN SCRIPT '%s'",error_current_script);
					OUTPUT_message(error);
					assert(0); // FOOOOOOOOOK! What happened there then?
				}
			}
			while (temp_tokenised_script[line_number].indentation_level != temp_tokenised_script[other_line_number].indentation_level);

			sprintf (number_as_string,"%d",other_line_number);
			
			// Dump the offset to the next line of equal indentation after the end of the current line.
			PARSER_add_temp_script_line_argument (offset_list_index , number_as_string , UNSET , line_number);
		}
	}

	return error_occurred;
}



void PARSER_delete_debug_files (void)
{
	remove (MAIN_get_project_filename("processed_scripts_test.txt"));
}



void PARSER_output_processed_script_back_as_text (int script_number)
{
	// Now as a public service we also spew out the tokenised script as text in the hope that is at least
	// vaguely matches what we stuck in there. It crapping well better had...

	// It also (and at no extra cost to your good self, sir!) makes the tracer script at the same time
	// if it has been requested. Wooters, eh?

	int argument_number;
	int source_word_list;
	int source_word_list_entry_1;
	int source_word_list_entry_2;
	char tokenised_line_text[TEXT_LINE_SIZE];
	char word[TEXT_LINE_SIZE]; // Using TEXT_LINE_SIZE not NAME_SIZE due to compound words blah.blah getting too long once bracketted defines are in there.
	int t;
	int line_number;
	int indentation;
	int archetype_number;
	char error[MAX_LINE_SIZE];
	bool skip_this_word_for_tracer_script;
	bool nullify_type_bitflag;

	static int cumulative_line = 0;

	FILE *file_pointer = fopen (MAIN_get_project_filename("processed_scripts_test.txt", true) , "a");

	if (file_pointer == NULL)
	{
		sprintf(error,"CANNOT OUTPUT SCRIPT '%s' TO PROCESSED SCRIPT FILE",error_current_script);
		OUTPUT_message(error);
		assert (0); // Arse!
	}

	sprintf (tokenised_line_text , "%s\n\n" , word_list[script_list_index].words[script_number].text_word);
	fputs (tokenised_line_text,file_pointer);

	for (line_number=0; line_number<temp_tokenised_script_length; line_number++)
	{

		#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER
			// First of all, create space for a new line!
			if (tracer_script == NULL)
			{
				tracer_script = (tracer_script_line_struct *) malloc (sizeof (tracer_script_line_struct));
			}
			else
			{
				tracer_script = (tracer_script_line_struct *) realloc (tracer_script , (tracer_script_length+1) * sizeof (tracer_script_line_struct));
			}
		#endif

		indentation = temp_tokenised_script[line_number].indentation_level;
		archetype_number = temp_tokenised_script[line_number].archetype_number;

		#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER
			tracer_script[tracer_script_length].line_type_bitflag = command_syntax_archetype_list[archetype_number].line_type;
			tracer_script[tracer_script_length].line_indentation = indentation;

			tracer_script[tracer_script_length].line_length = temp_tokenised_script[line_number].script_line_size;
			tracer_script[tracer_script_length].argument_list = (tracer_text_word *) malloc (sizeof(tracer_text_word) * tracer_script[tracer_script_length].line_length);
		#endif

		sprintf (tokenised_line_text,"(%5d) %3d:\t",cumulative_line,line_number);

		cumulative_line++;

		for (t=0; t<indentation; t++)
		{
			strcat(tokenised_line_text,"\t");
		}

		for (argument_number=0; argument_number<temp_tokenised_script[line_number].script_line_size ;argument_number++)
		{
			strcpy (word,"");
			
			source_word_list = temp_tokenised_script[line_number].script_line_pointer[argument_number].source_word_list;

			source_word_list_entry_1 = temp_tokenised_script[line_number].script_line_pointer[argument_number].source_word_list_entry_1;
			source_word_list_entry_2 = temp_tokenised_script[line_number].script_line_pointer[argument_number].source_word_list_entry_2;

			nullify_type_bitflag = false;

			if (source_word_list == label_list_index)
			{
				// It's a label, so just make a number and stick an L in front of it.

				sprintf (word,"%s",word_list[label_list_index].words[source_word_list_entry_1].text_word);
//				sprintf (word,"L%d",source_word_list_entry_1);
				skip_this_word_for_tracer_script = false;
			}
			else if (source_word_list == offset_list_index)
			{
				// It's a label, so just make a number and stick an L in front of it.
				sprintf (word,"(ELSE GOTO LINE %d)",source_word_list_entry_1+1);
				skip_this_word_for_tracer_script = true;
			}
			else if (source_word_list == number_list_index)
			{
				// It's a number, so just do a number. (unless it's a define)
				
				if (temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_1_index != UNSET)
				{
					sprintf (word,"%s", PARSER_get_alias_by_index(temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_1_index));
				}
				else
				{
					sprintf (word,"%d",source_word_list_entry_1);
					nullify_type_bitflag = true;
				}

				skip_this_word_for_tracer_script = false;
			}
			else
			{
				// Otherwise it's a word from a list, so use that why don't you?
				if (source_word_list_entry_2 != UNSET)
				{
					// It's a compound word, not just one!

					if (temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_1_index != UNSET)
					{
						if (temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_2_index != UNSET)
						{
							sprintf (word,"[%s]%s.[%s]%s", word_list[source_word_list].words[source_word_list_entry_1].text_word , PARSER_get_alias_by_index(temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_1_index), word_list[source_word_list].words[source_word_list_entry_2].text_word,  PARSER_get_alias_by_index(temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_2_index) );

						}
						else
						{
							sprintf (word,"[%s]%s.%s", word_list[source_word_list].words[source_word_list_entry_1].text_word , PARSER_get_alias_by_index(temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_1_index), word_list[source_word_list].words[source_word_list_entry_2].text_word );
						}
					}
					else
					{
						if (temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_2_index != UNSET)
						{
							sprintf (word,"%s.[%s]%s", word_list[source_word_list].words[source_word_list_entry_1].text_word , word_list[source_word_list].words[source_word_list_entry_2].text_word , PARSER_get_alias_by_index(temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_2_index));

						}
						else
						{
							sprintf (word,"%s.%s", word_list[source_word_list].words[source_word_list_entry_1].text_word , word_list[source_word_list].words[source_word_list_entry_2].text_word );
						}
					}
				}
				else
				{	
					// This isn't, though. It's just a word. Sad and lonely.

					if (temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_1_index != UNSET)
					{
						sprintf (word,"[%s]%s", word_list[source_word_list].words[source_word_list_entry_1].text_word , PARSER_get_alias_by_index(temp_tokenised_script[line_number].script_line_pointer[argument_number].alias_1_index) );
					}
					else
					{
						sprintf (word,"%s", word_list[source_word_list].words[source_word_list_entry_1].text_word );
					}
				}
				skip_this_word_for_tracer_script = false;
			}

			#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER
				if (skip_this_word_for_tracer_script)
				{
					tracer_script[tracer_script_length].argument_list[argument_number].text_word = (char *) malloc (strlen("")+1 * sizeof(char));
					strcpy (tracer_script[tracer_script_length].argument_list[argument_number].text_word,"");
					tracer_script[tracer_script_length].argument_list[argument_number].argument_type_bitflag = 0;
				}
				else
				{
					tracer_script[tracer_script_length].argument_list[argument_number].text_word = (char *) malloc (strlen(word)+1 * sizeof(char));
					strcpy (tracer_script[tracer_script_length].argument_list[argument_number].text_word,word);
					if (nullify_type_bitflag)
					{
						tracer_script[tracer_script_length].argument_list[argument_number].argument_type_bitflag = 0; // So we don't see it in brackets afterwards. Blerk!
					}
					else
					{
						tracer_script[tracer_script_length].argument_list[argument_number].argument_type_bitflag = command_syntax_archetype_list[archetype_number].argument_list[argument_number].argument_type;
					}
				}
			#endif

			strcat (word," ");

			strcat (tokenised_line_text,word);
		}

		strcat (tokenised_line_text," // ");

		for (argument_number=0; argument_number<temp_tokenised_script[line_number].script_line_size ;argument_number++)
		{
			sprintf( word , "(B%i : T%i : V%i)" , temp_tokenised_script[line_number].script_line_pointer[argument_number].data_bitmask , temp_tokenised_script[line_number].script_line_pointer[argument_number].data_type , temp_tokenised_script[line_number].script_line_pointer[argument_number].data_value );
			
			strcat (tokenised_line_text,word);
		}

		strcat (tokenised_line_text,"\n");

		fputs (tokenised_line_text,file_pointer);
	
		#ifdef RETRENGINE_DEBUG_VERSION_SCRIPT_TRACER
			tracer_script_length++;
		#endif
	}

	sprintf (tokenised_line_text,"\n\n\n");
	fputs(tokenised_line_text,file_pointer);

	fclose(file_pointer);

}



void PARSER_destroy_trace_script (void)
{
	if (tracer_script)
	{
		int line_number;
		int argument_number;

		for (line_number=0; line_number<tracer_script_length; line_number++)
		{
			for (argument_number=0; argument_number<tracer_script[line_number].line_length; argument_number++)
			{
				free (tracer_script[line_number].argument_list[argument_number].text_word);
			}

			free (tracer_script[line_number].argument_list);
		}

		free (tracer_script);
	}
}



void PARSER_create_full_script_line ( int indentation_level )
{
	if ( (full_tokenised_script_length==0) && (temp_tokenised_script==NULL) )
	{
		full_tokenised_script = (script_line *) malloc (sizeof(script_line));
	}
	else
	{
		full_tokenised_script = (script_line *) realloc ( full_tokenised_script , (full_tokenised_script_length+1) * sizeof(script_line) );
	}

	full_tokenised_script[full_tokenised_script_length].script_line_pointer = NULL;
	full_tokenised_script[full_tokenised_script_length].script_line_size = 0;
	full_tokenised_script[full_tokenised_script_length].indentation_level = indentation_level;
}



void PARSER_add_full_script_line_argument ( int data_bitmask , int data_type , int data_value )
{
	// Makes space in the full script for an argument and then stores it. Aw, bless.

	int line_number = full_tokenised_script_length;

	int script_line_size = full_tokenised_script[line_number].script_line_size;

	if (full_tokenised_script[line_number].script_line_pointer == NULL)
	{
		full_tokenised_script[line_number].script_line_pointer = (script_data *) malloc (sizeof(script_data));
	}
	else
	{
		full_tokenised_script[line_number].script_line_pointer = (script_data *) realloc ( full_tokenised_script[line_number].script_line_pointer , (full_tokenised_script[line_number].script_line_size + 1) * sizeof(script_data));
	}

	full_tokenised_script[line_number].script_line_pointer[script_line_size].data_bitmask = data_bitmask;
	full_tokenised_script[line_number].script_line_pointer[script_line_size].data_type = data_type;
	full_tokenised_script[line_number].script_line_pointer[script_line_size].data_value = data_value;

	full_tokenised_script[line_number].script_line_size++;
}



void PARSER_finish_full_script_line (void)
{
	full_tokenised_script_length++;
}



void PARSER_offset_labels ( int line_number , int argument_number , int line_number_offset )
{
	int source_word_list = temp_tokenised_script[line_number].script_line_pointer[argument_number].source_word_list;

	if ( (source_word_list == offset_list_index) || (source_word_list == label_list_index) )
	{
		if (temp_tokenised_script[line_number].script_line_pointer[argument_number].data_value == 347)
		{
			int a=0;
		}
		temp_tokenised_script[line_number].script_line_pointer[argument_number].data_value += line_number_offset;
	}
}



void PARSER_add_script_details_to_table (void)
{
	// This expands the script_list_index and then adds the details of the latest script to it.
	// It should be called just after a script is loaded and processed and just before it's shoved
	// over into the full script.

	if (script_index_table == NULL)
	{
		script_index_table = (int *) malloc ( sizeof (int) );
		number_of_scripts_loaded = 0;
	}
	else
	{
		script_index_table = (int *) realloc ( script_index_table , (number_of_scripts_loaded+1) * sizeof (int) );
	}

	script_index_table[number_of_scripts_loaded] = full_tokenised_script_length;

	number_of_scripts_loaded++;
}



void PARSER_append_temp_script_to_real_script ( int script_number )
{
	int line_number_offset;
	int line_number;
	int argument_number;
	int max_argument_number;
	int max_line_number;

	line_number_offset = full_tokenised_script_length;

	max_line_number = temp_tokenised_script_length;

	// First of all find all the 

	for (line_number = 0; line_number<max_line_number; line_number++)
	{
		PARSER_create_full_script_line (temp_tokenised_script[line_number].indentation_level);

		max_argument_number = temp_tokenised_script[line_number].script_line_size;

		for (argument_number=0; argument_number<max_argument_number; argument_number++)
		{
			PARSER_offset_labels ( line_number , argument_number , line_number_offset );
			PARSER_add_full_script_line_argument ( temp_tokenised_script[line_number].script_line_pointer[argument_number].data_bitmask , temp_tokenised_script[line_number].script_line_pointer[argument_number].data_type , temp_tokenised_script[line_number].script_line_pointer[argument_number].data_value );
		}

		PARSER_finish_full_script_line ();
	}
}



bool PARSER_check_file_exists (char *filename)
{
	FILE *file_pointer = fopen (filename,"r");
	
	if (file_pointer == NULL)
	{
		return false;
	}
	else
	{
		fclose (file_pointer);
		return true;
	}
}



void PARSER_replace_unsuitable_chars ( char *word )
{
	// This is to replace those chars in the names which would be unsuitable if used in enums or defines.

	STRING_replace_word ( word , "!>" , "NOT_GREATER_THAN" );
	STRING_replace_word ( word , "!<" , "NOT_LESS_THAN" );
	STRING_replace_word ( word , "?" , "QUESTION_MARK" );
	STRING_replace_word ( word , "++" , "INCREASE" );
	STRING_replace_word ( word , ">%" , "ADAPT_BY_PERCENTAGE" );
	STRING_replace_word ( word , "++" , "INCREASE" );
	STRING_replace_word ( word , "--" , "DECREASE" );
	STRING_replace_word ( word , "!||" , "NOT_LOGICAL_OR" );
	STRING_replace_word ( word , "!&&" , "NOT_LOGICAL_AND" );
	STRING_replace_word ( word , "!^^" , "NOT_LOGICAL_XOR" );
	STRING_replace_word ( word , "!|" , "NOT_BITWISE_OR" );
	STRING_replace_word ( word , "!&" , "NOT_BITWISE_AND" );
	STRING_replace_word ( word , "!^" , "NOT_BITWISE_XOR" );
	STRING_replace_word ( word , "||" , "LOGICAL_OR" );
	STRING_replace_word ( word , "&&" , "LOGICAL_AND" );
	STRING_replace_word ( word , "<<" , "LEFT_SHIFT" );
	STRING_replace_word ( word , ">>" , "RIGHT_SHIFT" );
	STRING_replace_word ( word , "<=" , "LESS_THAN_OR_EQUAL" );
	STRING_replace_word ( word , ">=" , "GREATER_THAN_OR_EQUAL" );
	STRING_replace_word ( word , "==" , "EQUAL" );
	STRING_replace_word ( word , "!=" , "UNEQUAL" );
	STRING_replace_word ( word , "<>" , "UNEQUAL" );
	STRING_replace_word ( word , "!" , "NOT" );
	STRING_replace_word ( word , "=" , "EQUAL" );
	STRING_replace_word ( word , ">>" , "SHIFT_RIGHT" );
	STRING_replace_word ( word , "<<" , "SHIFT_LEFT" );
	STRING_replace_word ( word , ">" , "GREATER_THAN" );
	STRING_replace_word ( word , "<" , "LESS_THAN" );
	STRING_replace_word ( word , "+" , "PLUS" );
	STRING_replace_word ( word , "-" , "MINUS" );
	STRING_replace_word ( word , "/" , "DIVIDE" );
	STRING_replace_word ( word , "*" , "MULTIPLY" );
	STRING_replace_word ( word , "|" , "BITWISE_OR" );
	STRING_replace_word ( word , "&" , "BITWISE_AND" );
	STRING_replace_word ( word , "^" , "BITWISE_XOR" );
	STRING_replace_word ( word , "%" , "PERCENTAGE" );
}



void PARSER_output_enums ( int line_number , int split )
{
	bool replace_chars;
	bool use_prefix;
	int word_list_number;
	char prefix[NAME_SIZE];
	char word[NAME_SIZE*2];
	char temp[NAME_SIZE*2];
	int t;

	if (strcmp ( script[line_number].text_word_list[3].text_word , "REPLACE_YES" ) == 0)
	{
		replace_chars = true;
	}
	else
	{
		replace_chars = false;
	}

	word_list_number = PARSER_find_word_list_name ( script[line_number].text_word_list[1].text_word );

	strcpy ( prefix , script[line_number].text_word_list[2].text_word );

	if (strcmp (prefix,"NONE") == 0)
	{
		use_prefix = false;
	}
	else
	{
		use_prefix = true;
	}

	FILE *file_pointer = fopen ("scripting_enums.h","a");

	if (file_pointer == NULL)
	{
		OUTPUT_message("CANNOT OUTPUT ENUMS! AS BLOODY USUAL...");
		assert(0);
	}

	fputs ( "enum { " , file_pointer );

	for (t=0; t<word_list[word_list_number].word_list_size; t++)
	{
		if (use_prefix == true)
		{
			strcpy (word,prefix);
		}
		else
		{
			strcpy (word,"");
		}

		strcat ( word , word_list[word_list_number].words[t].text_word );

		if (replace_chars == true)
		{
			PARSER_replace_unsuitable_chars ( word );
		}

		if (t % split == 0)
		{
			sprintf ( temp , " = %i",t);
			strcat ( word , temp );
		}

		strcat ( word , " " );

		if ( (t % split == split-1) || (t == word_list[word_list_number].word_list_size - 1) )
		{
			strcat ( word , "};\n" );
			if (t < word_list[word_list_number].word_list_size - 1)
			{
				strcat ( word , "enum { " );
			}
		}
		else if (t < word_list[word_list_number].word_list_size - 1)
		{
			strcat ( word , ", " );
		}

		fputs (word , file_pointer);
	}

	fputs ("\n",file_pointer);

	fclose (file_pointer);

}



void PARSER_output_cases ( int line_number )
{
	bool replace_chars;
	bool use_prefix;
	int word_list_number;
	char prefix[NAME_SIZE];
	char word[NAME_SIZE*2];
	int t;

	if (strcmp ( script[line_number].text_word_list[3].text_word , "REPLACE_YES" ) == 0)
	{
		replace_chars = true;
	}
	else
	{
		replace_chars = false;
	}

	word_list_number = PARSER_find_word_list_name ( script[line_number].text_word_list[1].text_word );

	strcpy ( prefix , script[line_number].text_word_list[2].text_word );

	if (strcmp (prefix,"NONE") == 0)
	{
		use_prefix = false;
	}
	else
	{
		use_prefix = true;
	}

	FILE *file_pointer = fopen ("scripting_cases.h","a");

	if (file_pointer == NULL)
	{
		OUTPUT_message("CANNOT OUTPUT CASES!");
		assert(0);
	}

	fputs ( "switch ()\n{\n\n" , file_pointer );

	for (t=0; t<word_list[word_list_number].word_list_size; t++)
	{
		strcpy (word,"case ");

		if (use_prefix == true)
		{
			strcat (word,prefix);
		}

		strcat ( word , word_list[word_list_number].words[t].text_word );

		if (replace_chars == true)
		{
			PARSER_replace_unsuitable_chars ( word );
		}

		strcat ( word , ":\n\tbreak;\n\n" );

		fputs (word , file_pointer);
	}

	fputs ( "default:\n\tbreak;\n\n}\n\n" , file_pointer );

	fclose (file_pointer);
}



void PARSER_alphabetise_list ( int line_number )
{
	// This simply regigs the list so it's alphabetical. In some cases you don't want to swap around
	// the values associated with each number during this as it isn't important but in others you want
	// to maintain the values (ie, for those lists which were generated with numbers assigned to each
	// word in the list rather than automatic numbering).

	int word_list_number;
	int list_size;
	bool articles_swapped;
	int temp_value;
	bool swap_values;
	int t;

	char *temp_word_pointer;

	word_list_number = PARSER_find_word_list_name ( script[line_number].text_word_list[1].text_word );
	list_size = word_list[word_list_number].word_list_size;

	if (strcmp (script[line_number].text_word_list[2].text_word , "SWAP_VALUES_YES") == 0)
	{
		swap_values = true;
	}
	else
	{
		swap_values = false;
	}

	do
	{
		articles_swapped = false;

		for (t=0; t<list_size - 1; t++)
		{
			if (strcmp ( word_list[word_list_number].words[t].text_word , word_list[word_list_number].words[t+1].text_word ) > 0 )
			{
				temp_word_pointer = word_list[word_list_number].words[t].text_word;
				word_list[word_list_number].words[t].text_word = word_list[word_list_number].words[t+1].text_word;
				word_list[word_list_number].words[t+1].text_word = temp_word_pointer;

//				strcpy ( word , word_list[word_list_number].words[t].text_word );
//				strcpy ( word_list[word_list_number].words[t].text_word , word_list[word_list_number].words[t+1].text_word );
//				strcpy ( word_list[word_list_number].words[t+1].text_word , word );

				if (swap_values == true)
				{
					// This'll only really be used for constants and crap like that.
					temp_value = word_list[word_list_number].word_values[t];
					word_list[word_list_number].word_values[t] = word_list[word_list_number].word_values[t+1];
					word_list[word_list_number].word_values[t+1] = temp_value;
				}

				articles_swapped = true;
			}
		}

	} while (articles_swapped);

}



void PARSER_start_global_parameter_list (void)
{
	remove (MAIN_get_project_filename("global_parameter_list.txt"));
	remove (MAIN_get_project_filename("gpl_list_size.txt"));
	number_of_outputted_word_list_entries = 0;
}



void PARSER_add_to_global_parameter_list (int line_number)
{
	char list_name[NAME_SIZE];
	char temp_char[NAME_SIZE*2];
	int word_list_number;
	int list_size;
	int t;

	strcpy (list_name, script[line_number].text_word_list[1].text_word);

	word_list_number = PARSER_find_word_list_name (list_name);

	list_size = word_list[word_list_number].word_list_size;

	number_of_outputted_word_list_entries += list_size;

	FILE *file_pointer = fopen (MAIN_get_project_filename("global_parameter_list.txt"),"a");

	if (file_pointer != NULL)
	{
		sprintf(temp_char,"#PARAMETER LIST NAME = %s\n",list_name);

		fputs(temp_char,file_pointer);

		if (word_list[word_list_number].has_extension == true) // Only output extension if there is one...
		{
			sprintf(temp_char,"#PARAMETER LIST EXTENSION = %s\n" , word_list[word_list_number].extension);
			fputs(temp_char,file_pointer);
		}

/*
		for (t=0; t<list_size; t++)
		{
			if (strcmp(word_list[word_list_number].words[t].text_word,"DUMMY_TAG")!=0)
			{
				real_size++;
			}
		}
*/
		sprintf(temp_char,"#PARAMETER LIST SIZE = %d\n",list_size);
		fputs(temp_char,file_pointer);

		for (t=0; t<list_size; t++)
		{
//			if (strcmp(word_list[word_list_number].words[t].text_word,"DUMMY_TAG")!=0)
//			{
				sprintf ( temp_char , "%s = %d\n" , word_list[word_list_number].words[t].text_word , word_list[word_list_number].word_values[t] );
				fputs(temp_char,file_pointer);
//			}
		}

		fputs("\n",file_pointer);
		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not append to Global Parameter List!");
		assert(0); // WHAT THE HECK?!
	}

}



void PARSER_end_global_parameter_list (void)
{
	FILE *file_pointer = fopen (MAIN_get_project_filename("global_parameter_list.txt"),"a");

	if (file_pointer != NULL)
	{
		fputs("#END_OF_PARAMETER_LIST\n",file_pointer);
		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not append an end to Global Parameter List.");
		assert(0); // WHAT THE HECK?!
	}

	char text[NAME_SIZE];

	sprintf (text,"%i\n",number_of_outputted_word_list_entries);
	
	file_pointer = fopen (MAIN_get_project_filename("gpl_list_size.txt"),"w");
	
	if (file_pointer != NULL)
	{
		fputs(text,file_pointer);
		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not output GPL list size.");
		assert(0); // WHAT THE HECK?!
	}
}



void PARSER_start_c_define_file (void)
{
	remove ("scripting_enums.h");
	remove ("scripting_cases.h");
}



int PARSER_output_instructions (int *pointer)
{
	// This function will go through the tokenised script an instruction at a time and put them
	// into the given structure. At the same time it counts them and returns the total amount.

	// If a NULL pointer is passed in then it'll just do the counting.

	int counter;
	int line_number;
	int argument_number;
	int line_length;

	counter = 0;

	for (line_number = 0; line_number < full_tokenised_script_length ; line_number++)
	{
		line_length = full_tokenised_script[line_number].script_line_size;

		for (argument_number = 0; argument_number<line_length ; argument_number++)
		{
			counter++;
			if (pointer != NULL)
			{
				*pointer = full_tokenised_script[line_number].script_line_pointer[argument_number].data_type;
				pointer++;
				*pointer = full_tokenised_script[line_number].script_line_pointer[argument_number].data_value;
				pointer++;
				*pointer = full_tokenised_script[line_number].script_line_pointer[argument_number].data_bitmask;
				pointer++;
			}

		}

	}

	return counter;

}



void PARSER_save_full_script (void)
{
	// Okay, this takes all the scripts and compiles them into a honkingly huge file which can be blurted
	// out into a file in a couple of fwrites for speed and to stop thrashing the HDD.

	int *big_data;
	int *pointer;

	int t;

	int total_size;
	int script_index_table_size; // This is the list of starts of each line in the full script
	int line_length_table_size; // This is the list of how big each line in the script is (so it can be all malloc'd at once)
	int line_indentation_table_size; // This is the same size as above. Oh yeah.
	int data_table_size; // This is all the actual script data itself, a fairly large chunk of change.
	int details_array[5];

	// All of these are multipled by sizeof(int)

	script_index_table_size = number_of_scripts_loaded;
	line_length_table_size = full_tokenised_script_length;
	line_indentation_table_size = full_tokenised_script_length;
	data_table_size = PARSER_output_instructions (NULL);

	total_size = script_index_table_size + line_length_table_size + line_indentation_table_size + (data_table_size * 3); // It's *3 because each instruction is 3 ints.

	details_array[0] = total_size;
	details_array[1] = script_index_table_size;
	details_array[2] = line_length_table_size;
	details_array[3] = line_indentation_table_size;
	details_array[4] = data_table_size;

	big_data = (int *) malloc ( total_size * sizeof(int) );

	if (big_data == NULL)
	{
		OUTPUT_message("Could not allocate 'big_data'. Arse!");
		assert(0);
	}

	pointer = big_data;

	for (t=0 ; t<script_index_table_size ; t++)
	{
		*pointer = script_index_table[t];
		pointer++;
	}
	
	for (t=0 ; t<line_length_table_size ; t++)
	{
		*pointer = full_tokenised_script[t].script_line_size;
		pointer++;
	}

	for (t=0 ; t<line_length_table_size ; t++)
	{
		*pointer = full_tokenised_script[t].indentation_level;
		pointer++;
	}

	PARSER_output_instructions (pointer);

	// There, that should fit snuggly in there.

	FILE *file_pointer = fopen (MAIN_get_project_filename("scriptfile.tsl"),"wb");

	if (file_pointer != NULL)
	{
		fwrite (details_array , 5 * sizeof(int) , 1 , file_pointer);
		fwrite (big_data , total_size * sizeof(int) , 1 , file_pointer);

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not output 'scriptfile.tsl'. What gives?");
		assert(0);
	}

	free (big_data);

}



/*

void PARSER_load_full_script (void)
{
	int details_array[4];
	int *big_data;

	int total_size;
	int script_index_table_size; // This is the list of starts of each line in the full script
	int line_length_table_size; // This is the list of how big each line in the script is (so it can be all malloc'd at once)
	int data_table_size; // This is all the actual script data itself, a fairly large chunk of change.	

	FILE *file_pointer = fopen (MAIN_get_project_filename("scriptfile.tsl"),"rb");

	if (file_pointer != NULL)
	{
		fread (details_array , 4 * sizeof(int) , 1 , file_pointer);

		total_size = details_array[0];
		script_index_table_size = details_array[1];
		line_length_table_size = details_array[2];
		data_table_size = details_array[3];

		big_data = (int *) malloc ( total_size * sizeof(int) );

		fread (big_data , total_size * sizeof(int) , 1 , file_pointer);

		fclose(file_pointer);
	}
	else
	{
		OUTPUT_message("Could not open 'scriptfile.tsl'. Where is it, matey?");
		assert(0);
	}

	int t;
	int line_counter;
	int argument_counter;
	int data_counter;

	// First of all reconstruct the script index table...

	script_index_table = (int *) malloc ( script_index_table_size * sizeof (int) );

	// And transfer it across...

	for (t=0; t<script_index_table_size; t++)
	{
		script_index_table[t] = big_data[t];
	}

	// Then the full tokenised script space...

	full_tokenised_script = (script_line *) malloc ( line_length_table_size * sizeof(script_line) );

	// Now put in the lengths of each line...

	for (t=0; t<line_length_table_size; t++)
	{
		full_tokenised_script[t].script_line_size = big_data[t+script_index_table_size]; // Because the data starts after the script_index_table.
	}

	// And malloc the space for them...

	for (t=0; t<line_length_table_size; t++)
	{
		full_tokenised_script[t].script_line_pointer = (script_data *) malloc ( full_tokenised_script[t].script_line_size * sizeof(script_data) );
	}

	// And copy across the instructions.

	line_counter = 0;
	argument_counter = 0;
	data_counter = script_index_table_size + line_length_table_size; // ie, the start of the actual instruction data...

	while (data_counter < total_size)
	{
		full_tokenised_script[line_counter].script_line_pointer[argument_counter].data_type = big_data[data_counter];
		full_tokenised_script[line_counter].script_line_pointer[argument_counter].data_value = big_data[data_counter+1];
		full_tokenised_script[line_counter].script_line_pointer[argument_counter].data_bitmask = big_data[data_counter+2];
		data_counter += 3;

		argument_counter++;
		if (argument_counter == full_tokenised_script[line_counter].script_line_size)
		{
			argument_counter = 0;
			line_counter++;
		}
	}

	// And then free up the temporary data.

	free (big_data);

}

*/



void PARSER_save_datatables (int line_number)
{
	// This calculates the necessary memory needed for the datatables, mallocs a HUGE blimping chunk of it
	// and writes all the necessary values to it and then squirts it out to disc.

	// Oh, and then it frees it.

	int datatable_number;
	int index_number;
	int data_offset;

	int header_size = (number_of_datatables * 3);

	// That's for all the numbers in the base structs plus a dinky little header saying how many of them there are.

	int data_size = 0;

	for (datatable_number=0; datatable_number<number_of_datatables; datatable_number++)
	{
		data_size += datatable_data[datatable_number].index_count;
		data_size += datatable_data[datatable_number].line_size * datatable_data[datatable_number].lines;
	}

	int total_size = header_size + data_size + 2; // +1 is for size of the data.

	int *data_chunk = (int *) malloc (total_size * sizeof(int));

	int *pointer = data_chunk;

	*pointer++ = (total_size-1);
	*pointer++ = number_of_datatables;

	for (datatable_number=0; datatable_number<number_of_datatables; datatable_number++)
	{
		*pointer++ = datatable_data[datatable_number].line_size;
		*pointer++ = datatable_data[datatable_number].lines;
		*pointer++ = datatable_data[datatable_number].index_count;

		for (index_number=0; index_number<datatable_data[datatable_number].index_count; index_number++ )
		{
			*pointer++ = datatable_data[datatable_number].index_list[index_number];
		}

		for (data_offset=0 ; data_offset < datatable_data[datatable_number].line_size * datatable_data[datatable_number].lines ; data_offset++ )
		{
			*pointer++ = datatable_data[datatable_number].data[data_offset];
		}
	}

	// And now we save it, assuming the above code actually worked...

	FILE *fp = fopen(MAIN_get_project_filename("datatable.dat"),"wb");

	if (fp != NULL)
	{
		fwrite (data_chunk,total_size*sizeof(int),1,fp);

		fclose(fp);
	}
	else
	{
		assert(0);
	}

	// And now, the time has come, for us to face, the final curtain...

	free (data_chunk);

	for (datatable_number=0; datatable_number<number_of_datatables; datatable_number++)
	{
		free (datatable_data[datatable_number].data);
		free (datatable_data[datatable_number].index_list);
	}

	free (datatable_data);

}



int PARSER_evaluate_data_argument (char *file_text_line, char *pointer_2, int unused_value)
{
	bool negate;
	int colon_position;
	char *list_name;
	char *list_entry;
	int list_name_index;
	int list_entry_index;
	char word_copy[TEXT_LINE_SIZE];
	char error[TEXT_LINE_SIZE]; // If we're using a text file we might need to burp out an error if it ain't there...
	char grabbed_word[TEXT_LINE_SIZE];

	int value;

	if (pointer_2 != NULL)
	{
		// Okay, first find out if it's got a minus on the front...

		if (pointer_2[0] == '-')
		{
			negate = true;
			pointer_2++;
		}
		else
		{
			negate = false;
		}

		// Check to see if it's got a ':' in it.

		if ((colon_position = STRING_instr_char(pointer_2,':',0)) != UNSET)
		{
			strcpy(word_copy,pointer_2);

			word_copy[colon_position] = '\0';

			list_name = word_copy;
			list_entry = &word_copy[colon_position+1];

			list_name_index = PARSER_find_word_list_name(list_name);

			if (list_name_index == UNSET)
			{
				sprintf(error,"Unrecognised data list name '%s' in line '%s'",list_name,file_text_line);
				OUTPUT_message(error);
				assert(0);
			}
			else
			{
				if (STRING_instr_char(list_entry,'|',0) != UNSET)
				{
					value = 0;

					// It's a series of OR'd values! Lawks! How exciting...

					char *argument_pointer = list_entry-1;
					int i;

					do
					{
						argument_pointer++;

						sprintf(grabbed_word,"");
						i=0;

						while ((argument_pointer[0] != '|') && (argument_pointer[0] != '\0'))
						{
							grabbed_word[i] = argument_pointer[0];
							i++;
							argument_pointer++;
						}

						grabbed_word[i] = '\0';

						list_entry_index = PARSER_is_word_in_list ( list_name_index , grabbed_word, false );

						if (list_entry_index == UNSET)
						{
							sprintf(error,"Unrecognised data list entry '%s' in line '%s'",grabbed_word,file_text_line);
							OUTPUT_message(error);
							assert(0);
						}
						else
						{
							value |= word_list[list_name_index].word_values[list_entry_index];
						}

					} while(argument_pointer[0] != '\0');

					if (negate)
					{
						value *= -1;
					}

				}
				else
				{
					list_entry_index = PARSER_is_word_in_list ( list_name_index , list_entry, false );

					if (list_entry_index == UNSET)
					{
						sprintf(error,"Unrecognised data list entry '%s' in line '%s'",list_entry,file_text_line);
						OUTPUT_message(error);
						assert(0);
					}
					else
					{
						value = word_list[list_name_index].word_values[list_entry_index];

						if (negate)
						{
							value *= -1;
						}
					}
				}
			}

		}
		else if (STRING_is_it_a_number(pointer_2) == true)
		{
			value = atoi(pointer_2);

			if (negate)
			{
				value *= -1;
			}
		}
		else
		{
			// What the hell is it then?!
			sprintf(error,"Unrecognised data entry '%s' in line '%s'",pointer_2,file_text_line);
			OUTPUT_message(error);
			assert(0);
		}
	}
	else
	{
		// Empty argument - tsk!
		value = unused_value;
	}

	return value;
}



void PARSER_parse_datatables ( int line_number )
{
	// As these are reliant upon having all the word lists in memory it's best to process them at this stage.

	// They are then saved out as purely numerical integer data in one large unsightly blob. Much like myself.

	int source_list_index = 0;


	char error[TEXT_LINE_SIZE]; // If we're using a text file we might need to burp out an error if it ain't there...
	char file_text_line[TEXT_LINE_SIZE]; // If we're parsing from a text file...
	char filename[NAME_SIZE];
	char *pointer_1,*pointer_2;
	FILE *file_pointer = NULL;
	int datatable_number;
	int argument_number;
//	bool negate;
	char line_copy[TEXT_LINE_SIZE];
//	char word_copy[TEXT_LINE_SIZE];
//	char *list_name;
//	char *list_entry;
//	int list_name_index;
//	int list_entry_index;
//	int colon_position;

	int dataline_number;
	int index_counter;

	int unused_value = UNSET;

//	datatable_data[word_list_counter].lines = autonumber_counter;
//	datatable_data[datatable_number].line_size = 0;

//	datatable_data[word_list_counter].data = NULL;
//	datatable_data[word_list_counter].index_list = NULL;

	source_list_index = PARSER_find_word_list_name ( script[line_number].text_word_list[1].text_word );

	for (datatable_number=0; datatable_number<number_of_datatables; datatable_number++)
	{
		// Create a valid filename from the directory entry...
		append_filename (filename , word_list[source_list_index].name, word_list[source_list_index].words[datatable_number].text_word, sizeof(filename) );
		strcat (filename , word_list[source_list_index].extension);
		
		// Then open it and read it just like with the SOURCE_FILE stuff
		file_pointer = fopen (MAIN_get_project_filename(filename),"r");

		if (file_pointer != NULL)
		{
			dataline_number = 0;
			index_counter = 0;

			while ( fgets ( file_text_line , TEXT_LINE_SIZE , file_pointer ) != NULL )
			{
				// Uppercase it.

				strcpy(line_copy,file_text_line);

				strupr (line_copy);

				pointer_1 = strtok (line_copy,",\r\n\t ");

				// If it's not just an emtpy line or a comment...
				if ( (strlen(line_copy)>0) && ( (line_copy[0] != '/') && (line_copy[1] != '/') ) && (pointer_1 != NULL) )
				{
					if ( pointer_1[0] == '.' )
					{
						// It's a label! Evil evil label! So we need to store this number in the index list.

						if (datatable_data[datatable_number].line_size != UNSET)
						{
							datatable_data[datatable_number].index_list[index_counter] = dataline_number;

							index_counter++;
						}
						else
						{
							OUTPUT_message("Attempting to read datatable data before line size has been set!");
							assert(0);
						}
					}
					else if ( strcmp(pointer_1,"#DATA") == 0)
					{
						// It's a line of data... Hurrah! So we're gonna' read it in...

						if (datatable_data[datatable_number].line_size != UNSET)
						{
							for (argument_number=0; argument_number<datatable_data[datatable_number].line_size; argument_number++)
							{
								pointer_2 = strtok (NULL,",\r\n\t ");

								datatable_data[datatable_number].data[(dataline_number * datatable_data[datatable_number].line_size) + argument_number] = PARSER_evaluate_data_argument (file_text_line,pointer_2,unused_value);
							}

							dataline_number++;
						}
						else
						{
							OUTPUT_message("Attempting to read datatable data before line size has been set!");
							assert(0);
						}
					}
					else if ( strcmp(pointer_1,"#SIZE") == 0)
					{
						// It's the size indicator. Read it then use it to malloc all the lovely space.

						pointer_2 = strtok (NULL,"\r\n\t ");

						// That should be the size...

						datatable_data[datatable_number].line_size = PARSER_evaluate_data_argument (file_text_line,pointer_2,unused_value);
						
						// And now malloc the space for the index list and main data.

						datatable_data[datatable_number].index_list = (int *) malloc (datatable_data[datatable_number].index_count * sizeof(int));
						datatable_data[datatable_number].data = (int *) malloc (datatable_data[datatable_number].line_size * datatable_data[datatable_number].lines * sizeof(int));
					}
					else if ( strcmp(pointer_1,"#BLANK") == 0)
					{
						// It's the unused value indicator. Read it then use it whenever we are missing values off the end of a line.

						pointer_2 = strtok (NULL,"\r\n\t ");

						// That should be the size...

						unused_value = PARSER_evaluate_data_argument (file_text_line,pointer_2,unused_value);
					}
				}
			}
		}
		else
		{
			sprintf (error,"ERROR! FILE '%s' NOT FOUND FOR LISTING!",filename);
			OUTPUT_message (error);
		}

		fclose (file_pointer);
	}


}



bool PARSER_parse ( char *filename )
{
	// And this is the main function, it's called with a filename for the Script Descripter File which is
	// then loaded and dealt with.

	// For ease of access, we'll dump it into a honking great character array so we can happily bugger
	// about with bits of it.

	PARSER_delete_debug_files ();

	if ( PARSER_check_file_exists (filename) == false ) // checks that the command syntax file is present.
	{
		return false; // If it isn't then just jump out of the parser. By this means we'll stop the release versions looking for the script files.
	}

	int line_number;
	bool loop_flag;
	bool output_debug_stuff;

	char dirname[NAME_SIZE];
	char extension[NAME_SIZE];
	char compiled_script_filename[NAME_SIZE];

	PARSER_read_script (filename); // Doesn't need to have project added to it because it's the same for all projects and so in the root directory.

	line_number = 0;

	loop_flag = true;
	output_debug_stuff = false;

	while (loop_flag == true)
	{

		if (script[line_number].text_word_list[0].text_word[0] == '#')
		{
			// It's a command!
			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#AUTONUMBER" ) == 0 )
			{
				// It's the autonumbered list command! Hurrah!
				line_number =  PARSER_parse_list ( line_number, SOURCE_SCRIPT , true );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#NUMBERED" ) == 0 )
			{
				// It's the numbered list command! Hurrah!
				line_number =  PARSER_parse_list ( line_number, SOURCE_SCRIPT , false );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#BOOLEAN_AUTONUMBERED" ) == 0 )
			{
				// It's the boolean autonumbered list command! Hurrah!
				line_number =  PARSER_parse_list ( line_number, SOURCE_SCRIPT , true , true );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#LIST_FILE" ) == 0 )
			{
				// It's the list file command! Hurrah!
				line_number =  PARSER_parse_list ( line_number, SOURCE_FILE , true );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#LIST_NUMBERED_FILE" ) == 0 )
			{
				// It's the list numbered file command! Hurrah!
				line_number =  PARSER_parse_list ( line_number, SOURCE_FILE , false );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#LIST_DIR" ) == 0 )
			{
				// It's the list directory command! Hurrah!
				line_number =  PARSER_parse_list ( line_number, SOURCE_DIRECTORY , true );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#LIST_LIST" ) == 0 )
			{
				// It's the list directory command! Hurrah!
				line_number =  PARSER_parse_list ( line_number, SOURCE_WORD_LIST , true );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#LIST_INDEX_LIST" ) == 0 )
			{
				// It's the list directory command! Hurrah!
				line_number =  PARSER_parse_list ( line_number, SOURCE_INDEX_LIST , true );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#START_GLOBAL_PARAMETER_LIST" ) == 0 )
			{
				// This basically just deletes the large parameter list file.
				PARSER_start_global_parameter_list();
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#ADD_TO_GLOBAL_PARAMETER_LIST" ) == 0 )
			{
				// This outputs the given word list to the global parameter list, headed by its name.
				PARSER_add_to_global_parameter_list ( line_number );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#END_GLOBAL_PARAMETER_LIST" ) == 0 )
			{
				// This just outputs a terminator line to the global paramater list for good measure.
				PARSER_end_global_parameter_list();
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#ALPHABETISE" ) == 0 )
			{
				// This sorts an already loaded word-list alphabetically. How twee.
				PARSER_alphabetise_list ( line_number );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#COMMAND_SYNTAX_LIST" ) == 0 )
			{
				// It's that jolly important CSL.
				line_number = PARSER_create_command_syntax_list ( line_number );
				PARSER_create_matching_alternate_store ();
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#OUTPUT_COMMAND_SYNTAX_LIST" ) == 0 )
			{
				// Dump out the stuff as we understand it so the user can check for discrepancies.
				if (output_debug_information)
				{
					PARSER_output_command_syntax_list ( line_number );
				}
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#OUTPUT_UNUSED_COMMANDS_LIST" ) == 0 )
			{
				// Dump out a list of members of the COMMAND list which are not current found anywhere in the archetype list.
				if (output_debug_information)
				{
					PARSER_output_unused_commands_list ( line_number );
				}
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#INTERPRET_DIR" ) == 0 )
			{
				strcpy ( dirname , script[line_number].text_word_list[1].text_word );
				strcpy ( extension , script[line_number].text_word_list[2].text_word );
				strcpy ( compiled_script_filename , script[line_number].text_word_list[3].text_word );
				loop_flag = false;
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#START_C_DEFINE_FILE" ) == 0 )
			{
				if (output_debug_information)
				{
					PARSER_start_c_define_file();
				}
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#OUTPUT_ENUMS" ) == 0 )
			{
				// This is just to make it easier when it comes to writing the interpreter as it'll output
				// a handy text file containing all the enums for inclusion in the header file of the interpreter.
				// It only outputs those lists which have been compiled so far.
				if (output_debug_information)
				{
					PARSER_output_enums (line_number,10);
				}
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#OUTPUT_CASES" ) == 0 )
			{
				// This is just to make it easier when it comes to writing the interpreter as it'll output
				// a handy text file containing all the enums for inclusion in the header file of the interpreter.
				// It only outputs those lists which have been compiled so far.
				if (output_debug_information)
				{
					PARSER_output_cases (line_number);
				}
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#INTERPRET_DATATABLES" ) == 0 )
			{
				PARSER_parse_datatables ( line_number );
				PARSER_save_datatables ( line_number );
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#CREATE_EMTPY_LIST" ) == 0 )
			{
				PARSER_create_empty_list (line_number);
			}

			if ( strcmp ( script[line_number].text_word_list[0].text_word , "#COMMAND_SYNTAX_PARAMETER_LIST" ) == 0 )
			{
				PARSER_add_syntax_parameter_list (line_number);
			}

		}

		line_number++;
	}

	// Righty, time to whizz through those scripts and convert them from text to tokenised and then
	// add them onto the full tokenised script table.

	int script_number;
	int max_script_number;
	char script_filename[NAME_SIZE];

	max_script_number = word_list[script_list_index].word_list_size;

	for (script_number = 0; script_number<max_script_number ; script_number++)
	{
		sprintf (script_filename , "%s\\%s%s" , word_list[script_list_index].name , word_list[script_list_index].words[script_number].text_word , word_list[script_list_index].extension );
		fix_filename_slashes(script_filename);
		strcpy (error_current_script,script_filename);
		PARSER_read_script ( MAIN_get_project_filename (script_filename) , true , true);
		PARSER_process_script ();
		PARSER_output_processed_script_back_as_text (script_number);
		PARSER_add_script_details_to_table ();
		PARSER_append_temp_script_to_real_script ( script_number );
	}

	// Okay, now we save everything to one honking great file.

	PARSER_save_full_script ();

	// And now we free up any memory that might be tied up.

	PARSER_destroy_matching_alternate_store ();

	if (syntax_parameter_list != NULL)
	{
		free (syntax_parameter_list);
		syntax_parameter_list = NULL;
		syntax_parameter_list_count = 0;
	}

	// And that's about that. Lawks.

	return false; // All okay, mum!

}


