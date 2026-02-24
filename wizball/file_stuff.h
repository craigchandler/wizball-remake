#ifndef _FILE_STUFF_H_
#define _FILE_STUFF_H_

void FILE_put_string_to_file (FILE *file_pointer, char *word, bool flush);
void EDIT_put_int_to_file (FILE *file_pointer, int value, bool flush);
void EDIT_put_float_to_file (FILE *file_pointer, float value, bool flush);
void FILE_reset_to_file (FILE *file_pointer);
void FILE_get_data_from_file (FILE *file_pointer, char *word, bool reset);
void FILE_reset_from_file (FILE *file_pointer);
void FILE_get_string_from_file (FILE *file_pointer, char *word);
float FILE_get_float_from_file (FILE *file_pointer);
int FILE_get_int_from_file (FILE *file_pointer);

char * FILE_open_dir (char *dirname, char *extension, bool capitalise);
char * FILE_read_dir_entry (bool capitalise);

void FILE_start_config_file (void);
void FILE_add_line_to_config_file (char *line);
void FILE_end_config_file (void);

#endif
