#include "string_size_constants.h"

#ifndef _STRING_STUFF_H_
#define _STRING_STUFF_H_

#define MAX_STRING_LENGTH				(512)

bool STRING_is_it_a_number ( char *word );
int STRING_instr_char (char *word , char finder, int start);
int STRING_instr_word(char *word , char *find , int start=0);
int STRING_get_word (char *line, char *word, char breaker_char);
int STRING_count_words ( char *line );
void STRING_replace_char (char *line, char look_for , char replace_with);
void STRING_replace_word (char *word ,char *look_for , char *replace_with);
void STRING_get_sub_word (char *word, char *sub_word, char endchar);

char * STRING_end_of_string (char *line, char *look_for);
void STRING_strip_newlines (char *line);
char *STRING_uppercase(char* s);
char *STRING_lowercase(char* s);
void STRING_strip_all_disposable (char *line);

int STRING_partial_strcmp (char *word_1, char *word_2);

int STRING_count_chars(char *word, char finder);

bool STRING_is_char_numerical (char c);

char * STRING_get_number_as_string (int value, int number_of_digits, bool pad_with_zeroes);

char *STRING_checkcode (char *line);

void uuencode_generic (char *dest, int game_number, int num_scores, int *scores, int *sizes);

#ifdef ALLEGRO_MACOSX
	/* This function is not POSIX so define it here... */
	char* STRING_uppercase(char*);
#else
#endif
#endif
