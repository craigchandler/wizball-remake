#include "string_size_constants.h"

#ifndef _GLOBAL_PARAM_LIST_H_
#define _GLOBAL_PARAM_LIST_H_

int GPL_what_is_list_number (char *word);
void GPL_list_extents (char *word_list, int *start, int *end);
void GPL_add_to_global_parameter_list (char *list_name , char *new_entry_name , int new_entry_value);
void GPL_load_global_parameter_list(void);
void GPL_load_global_parameter_list_from_datfile(void);
char * GPL_what_is_list_extension (char *word_list);
int GPL_does_word_exist (char *word_list, char *word);
int GPL_does_parameter_exist (char *word_list, char *word);
int GPL_find_word_value (char *word_list, char *word);
int GPL_find_word_index (char *word_list, char *word);
char * GPL_what_is_word_name (int entry_number);
int GPL_draw_list (char *list_name , int x , int y , int width , int height , int first_entry , int mouse_x_pos, int mouse_y_pos, bool numbered=true, int y_spacing=8, int r=0, int g=0, int b=255);
int GPL_get_value_by_index (int index);
void GPL_rename_entry (char *list_name, char *old_entry_name, char *new_entry_name);
char * GPL_get_entry_name (char *word_list, int index);

void GPL_destroy_lists (void);

int GPL_how_many_lists (void);
char * GPL_what_is_list_name (int list_number);

int GPL_list_size (char *word_list);
int GPL_get_word_index (char *word_list, char *word);

#endif
