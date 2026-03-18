#include "string_size_constants.h"

#ifndef _TEXTFILES_H_
#define _TEXTFILES_H_



typedef struct {
	char tag[NAME_SIZE];
	int line_size;
	char *line;
} textfile_line;



void TEXTFILE_load_text (void);
void TEXTFILE_load_text_from_source_files(void);
void TEXTFILE_destroy (void);
void TEXTFILE_save_compiled_text (void);
void TEXTFILE_load_compiled_text (void);
int TEXTFILE_get_index_by_tag (char *tag);
char * TEXTFILE_get_line_by_tag (char *tag);
int TEXTFILE_get_line_length_by_tag (char *tag);
char * TEXTFILE_get_line_by_index (int index);
int TEXTFILE_get_line_length_by_index (int index);
int TEXTFILE_get_text_length (int text_tag);



#endif
