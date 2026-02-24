#include "string_size_constants.h"

#ifndef _HISCORES_H_
#define _HISCORES_H_

#define HISCORE_TABLE_TYPE_SCORE							(1)
#define HISCORE_TABLE_TYPE_TIMES_MINS_SECS_MILLISECS		(2)
#define HISCORE_TABLE_TYPE_TIMES_SECS_MILLISECS				(3)
#define HISCORE_TABLE_TYPE_TIMES_MINS_SECS					(4)

#define LOWEST_SCORE										(-1)

typedef struct
{
	char *name;
	int score;
} hiscore_entry;

typedef struct
{
	int unique_id;
	int table_type;
	int entries;
	int name_size;
	int score_size;
	hiscore_entry *scores;
} hiscore_table;

void HISCORES_create_hiscore_table (int unique_id, int table_type, int entries, int name_size, int score_digits);
int HISCORES_insert_score (int unique_id, char *name, int score);
void HISCORES_overwrite_name (int unique_id, int entry, char *name);
int HISCORE_get_score (int unique_id, int entry);

char * HISCORE_get_hiscore_name (int unique_id, int entry);
char * HISCORE_get_hiscore_score (int unique_id, int entry, bool pad_with_zeros);
char * HISCORE_get_hiscore_name_and_score (int unique_id, int entry, bool pad_with_zeros, bool name_first, int gap_spaces);
void HISCORES_destroy_hiscores (void);

void HISCORES_save_hiscore_table (int unique_id, int filename_text_tag);
bool HISCORES_load_hiscore_table (int filename_text_tag);

void HISCORES_append_new_hiscore (int unique_id, int score);

#endif






















