#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "string_stuff.h"

#include "fortify.h"

bool STRING_is_it_a_number(char *word)
{
	// Checks through the word and if it finds anything but "0"-"9", "." or "-" then returns false, otherwise true.
	unsigned int i;

	if (strlen(word) > 1)
	{
		for (i = 0; i < strlen(word); i++)
		{
			if (((word[i] < '0') || (word[i] > '9')) && (word[i] != '-') && (word[i] != '.') && (word[i] != '!'))
			{
				return false;
			}
		}
	}
	else
	{
		for (i = 0; i < strlen(word); i++)
		{
			if ((word[i] < '0') || (word[i] > '9'))
			{
				return false;
			}
		}
	}

	return true;
}

int STRING_instr_char(char *word, char finder, int start = 0)
{
	unsigned int loop;

	for (loop = start; loop < strlen(word); loop++)
	{
		if (word[loop] == finder)
			return loop;
	}

	return UNSET;
}

char *STRING_end_of_string(char *line, char *look_for)
{
	// This is like the string function strstr, except rather than pointing at the start of the found
	// string, it points at the end...

	char *pointer;

	pointer = strstr(line, look_for);

	if (pointer != NULL)
	{
		pointer += strlen(look_for);
	}

	return pointer;
}

void STRING_strip_newlines(char *line)
{
	char *nl = strpbrk(line, "\n\r");
	if (nl)
	{
		*nl = '\0';
	}
}

char *STRING_uppercase(char *s)
{
	if (!s)
		return s;

	char *p = s;

	while (*p)
	{
		*p = (char)toupper((unsigned char)*p);
		++p;
	}

	return s;
}

char *STRING_lowercase(char *s)
{
	if (!s)
		return s;

	char *p = s;

	while (*p)
	{
		*p = (char)tolower((unsigned char)*p);
		++p;
	}

	return s;
}

char *STRING_get_number_as_string(int value, int number_of_digits, bool pad_with_zeroes)
{
	static char word[32]; // I doubt we'll have numbers longer than that! In fact we *can't* have so that's peachy.
	char workspace[32];

	sprintf(workspace, "%i\0", value);
	sprintf(word, "");

	int current_length = strlen(workspace);
	int remainder;
	int counter;

	char pad_char[1];
	pad_char[0] = pad_with_zeroes ? '0' : ' ';

	if (current_length < number_of_digits)
	{
		remainder = number_of_digits - current_length;
		for (counter = 0; counter < remainder; counter++)
		{
			strcat(word, pad_char);
		}
	}
	else
	{
		number_of_digits = current_length;
	}

	strcat(word, workspace);

	return word;
}

void STRING_reverse(char *string)
{
	int i, j;
	char c;

	for (i = 0, j = strlen(string) - 1; i < j; i++, j--)
	{
		c = string[i];
		string[i] = string[j];
		string[j] = c;
	}
}

int STRING_count_chars(char *word, char finder)
{
	// Simply returns the number of "finder" in "word"

	unsigned int t;
	int counter = 0;

	for (t = 0; t < strlen(word); t++)
	{
		if (word[t] == finder)
		{
			counter++;
		}
	}

	return counter;
}

void STRING_replace_word(char *word, char *look_for, char *replace_with)
{
	char temp[MAX_LINE_SIZE];
	char *found;
	char *copy_from;
	char *copy_to;
	bool exit_flag = false;

	while (exit_flag == false)
	{

		found = strstr(word, look_for);

		if (found != NULL) // found what we want, now we need to deal with it.
		{
			strcpy(temp, ""); // blank temporary;
			copy_from = word; // point copy_from at the start of the word;
			copy_to = temp;		// point copy_from at the start of the temporary;

			while (copy_from != found) // copy everything up until the start of the string we are replacing with another.
			{
				*copy_to = *copy_from;
				copy_to++;
				copy_from++;
			}

			*copy_to = '\0';

			strcat(temp, replace_with);

			copy_from += strlen(look_for);
			copy_to += strlen(replace_with);

			strcat(copy_to, copy_from); // And plonk the rest on. Good gravy this code looks ropey to me...

			strcpy(word, temp); // And plonk it back into word.
		}
		else
		{
			exit_flag = true;
		}
	}
}

int STRING_instr_word(char *word, char *find, int start)
{
	// Searches the string "word" for the string "find" and returns it's offset if present and UNSET if not.

	int find_len = strlen(find);
	int counter;
	int i;
	bool found = false;
	int found_pos = UNSET;

	for (i = start; (i <= signed(strlen(word) - find_len)) && (found == false); i++)
	{
		counter = 0;
		while ((word[i + counter] == find[counter]) && ((i + counter) < signed(strlen(word))))
		{
			counter++;
		}
		if (counter == find_len)
		{
			found = true;
			found_pos = i;
		}
	}

	return found_pos;
}

char *STRING_checkcode(char *line)
{
	char checksum[4] = {0, 0, 0, 0};
	char byte;

	int cycle = 0;

	static char checkword[9] = {"        "};

	int c;

	for (c = 0; c < signed(strlen(line)); c++)
	{
		if (c % 2)
		{
			byte = line[c] - 32;
		}
		else
		{
			byte = line[c];
		}

		checksum[cycle] ^= byte;

		cycle++;
		cycle %= 4;
	}

	for (c = 0; c < 8; c += 2)
	{
		byte = checksum[c / 2] >> 4;
		byte &= 15;
		checkword[c] = byte + 'A';

		byte = checksum[c / 2];
		byte &= 15;
		checkword[c + 1] = 'P' - byte;
	}

	return checkword;
}

void STRING_replace_char(char *line, char look_for, char replace_with)
{
	unsigned int i;

	for (i = 0; i < strlen(line); i++)
	{
		if (line[i] == look_for)
		{
			line[i] = replace_with;
		}
	}
}

int STRING_partial_strcmp(char *word_1, char *word_2)
{
	// This is just like strcmp except that it only looks at the first strlen(word_1) characters for comparison.

	unsigned int t;

	for (t = 0; (t < strlen(word_1)) && (t < strlen(word_2)); t++)
	{
		if (word_1[t] > word_2[t])
		{
			return 1;
		}
		else if (word_1[t] < word_2[t])
		{
			return -1;
		}
	}

	return 0;
}

bool STRING_is_disposable_char(char c)
{
	if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'))
	{
		return true;
	}

	return false;
}

void STRING_strip_all_disposable(char *line)
{
	// This gets rid of all preceding and trailing crapola' from a given string.

	// First of all, tidy up the end...

	while ((strlen(line) > 0) && (STRING_is_disposable_char(line[strlen(line) - 1])))
	{
		line[strlen(line) - 1] = '\0';
	}

	int counter = 0;

	while (STRING_is_disposable_char(line[counter]))
	{
		counter++;
	}

	unsigned int t;
	unsigned int length = strlen(line);

	for (t = 0; t <= (length - counter); t++)
	{
		line[t] = line[t + counter];
	}
}

bool STRING_is_char_numerical(char c)
{
	if ((c < '0') || (c > '9'))
	{
		return false;
	}
	else
	{
		return true;
	}
}

int STRING_get_word(char *line, char *word, char breaker_char)
{
	// Gets the word from the specified point to the first breaker_char or line end and stores it in the
	// the supplied array.

	int length = 0;
	bool found_characters = false;

	// Skip the preceding breaker_chars.
	while (*line == breaker_char)
	{
		*line++;
		length++;
	}

	// Copy the word into the word char array.
	while ((*line != breaker_char) && (*line != '\n') && (*line != '\0'))
	{
		*word = *line;

		*word++;
		*line++;
		length++;
		found_characters = true;
	}

	*word = '\0';

	// And pop the '\n' onto the end of it.

	if (found_characters)
	{
		return length;
	}
	else
	{
		return 0;
	}
}

int STRING_count_words(char *line)
{
	int length;
	int total_words = 0;
	int total_length = 0;
	char word[MAX_STRING_LENGTH];
	bool flag = true;

	while (flag)
	{
		length = STRING_get_word(&line[total_length], word, ' ');
		if (!length)
		{
			flag = false;
		}
		else
		{
			total_length += length;
			total_words++;
		}
	}

	return total_words;
}

void STRING_get_sub_word(char *word, char *sub_word, char endchar)
{
	// Sticks the characters from the given "word" up until the occurence of endchar into
	// "sub_word" and then NULL terminates it.

	int counter = 0;

	while (word[counter] != endchar)
	{
		sub_word[counter] = word[counter];
		counter++;
	}

	sub_word[counter] = '\0';
}

void uuencode_generic(char *dest, int game_number, int num_scores, int *scores, int *sizes)
{
	unsigned char decode[65] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+@"};
	unsigned char uu[30];

	int i, j;
	int pos_in_uu = 4; // Current position in the code
	int score_size;
	unsigned char xor_val = 0x69; // Start XOR value
	int add = 0;									// Checksum
	int real_length = 0;
	int temp_num;
	int pos_dest = 0;
	int added_bytes = 0;

	uu[0] = (time(0) >> 8) & 255;			// Time Stamp high
	uu[1] = time(0) & 255;						// Time Stamp low (use anytime/rand function)
	uu[2] = (game_number >> 8) & 255; // Game number high
	uu[3] = game_number & 255;				// Game number low

	// Lets put our scores in the code
	for (i = 0; i < num_scores; i++)
	{
		score_size = sizes[i];
		for (j = 0; j < score_size; j++)
		{
			uu[pos_in_uu] = (scores[i] >> (((score_size - 1) - j) * 8)) & 255;
			pos_in_uu++;
		}
	}

	// Start XOR-ing the code and doing checksum

	for (i = 0; i < pos_in_uu; i++)
	{
		add = add + uu[i];			 // First calc checksum
		uu[i] = uu[i] ^ xor_val; // XOR the value
		xor_val = uu[i];				 // Make the XOR value last value, so it changes
	}
	uu[pos_in_uu] = add & 255; // Checksum (not xor-ed)
	pos_in_uu++;

	real_length = pos_in_uu; // Remember the real length of the code

	// Add the "filler" ZER0 bytes at the end of the code to make it a multiple of 3 !
	if (real_length % 3 != 0)
	{
		// We need to fill, not a multiply of 3
		added_bytes = 3 - (real_length % 3);
	}

	for (i = 0; i < added_bytes; i++)
	{
		uu[real_length + i] = 0;
		pos_in_uu++;
	}
	// Do actual uuencoding here

	for (i = 0; i < pos_in_uu / 3; i++) // UUENCODE takes the bytes in chunks of 3
	{
		temp_num = (uu[i * 3] * 256 * 256) + (uu[i * 3 + 1] * 256) + uu[i * 3 + 2];
		// Calculate 24 bit value from 3 bytes
		for (j = 0; j < 4; j++)
		{
			dest[pos_dest] = decode[(temp_num >> (6 * (3 - j))) & 63]; // Do the ASCII chars from 6 bit parts
			pos_dest++;
		}
	}

	// Remove the added 'A' chars at the end ... otherwise it will just terminate the string
	dest[pos_dest - added_bytes] = 0;
}
#ifdef ALLEGRO_MACOSX
char *STRING_uppercase(char *s)
{
	char *pc = s;
	if (s)
	{
		while (*pc)
		{
			*pc = toupper(*pc);
			++pc;
		}
	}
	return s;
}
#else
#endif
