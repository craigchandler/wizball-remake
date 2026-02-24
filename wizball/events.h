#ifndef _EVENTS_H_
#define _EVENTS_H_

typedef struct
{
	bool used;
	int current_queue_size;
	int total_queue_size;
	int *event_types;
	int *event_entity_ids;
} entity_event_queue;



void EVENT_create_queue (int entity_id, int queue_size);
void EVENT_free_queue (int entity_id);
void EVENT_add_event_to_queue (int calling_entity_id, int add_to_entity_id, int event_type);
int EVENT_get_event_from_queue (int entity_id, int *poster_entity_id);
void EVENT_empty_queue (int entity_id);



#endif
