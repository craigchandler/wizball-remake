#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "arrays.h"
#include "main.h"
#include "output.h"
#include "scripting.h"
#include "parser.h" // For the datatable struct.
#include "events.h"

#include "fortify.h"

entity_event_queue ent_event_queues[MAX_ENTITIES];

// The higher the priority of an event, the higher it's index
// in the queue, so that the freshest ones can be popped right
// off the top of the stack and we don't have to shunt down the
// rest of the queue every time we add an event or deal with a
// wraparound array (*shudder!*)



void EVENT_create_queue (int entity_id, int queue_size)
{
	// Called by a script function.

	if ( ent_event_queues[entity_id].used == true )
	{
		OUTPUT_message("Already created queue for this entity!");
		assert(0);
	}
	else
	{
		ent_event_queues[entity_id].total_queue_size = queue_size;
		ent_event_queues[entity_id].current_queue_size = 0;

		ent_event_queues[entity_id].event_types = (int *) malloc (sizeof (int) * queue_size);
		ent_event_queues[entity_id].event_entity_ids = (int *) malloc (sizeof (int) * queue_size);

		ent_event_queues[entity_id].used = true;
	}
}



void EVENT_free_queue (int entity_id)
{
	// Frees up the memory used by a queue - called automatically when
	// an entity shuffles off its mortal coil.

	if ( ent_event_queues[entity_id].used == true )
	{
		ent_event_queues[entity_id].used = false;
		free (ent_event_queues[entity_id].event_types);
		free (ent_event_queues[entity_id].event_entity_ids);
	}
}



int EVENT_nudge_queue (entity_event_queue *queue, int new_event_type)
{
	// Nudges all the events in the queue down one to make room for a
	// new event. Returns the position where the new event should be stuck
	// in.

	int e;
	int current_slot_in_position = UNSET;

	for (e=0; e<queue->total_queue_size; e++)
	{
		if (queue->event_types[e] < new_event_type)
		{
			current_slot_in_position = e;

			if (e > 0)
			{
				queue->event_types[e-1] = queue->event_types[e];
				queue->event_entity_ids[e-1] = queue->event_entity_ids[e];
			}
		}
		else
		{
			return current_slot_in_position;
		}
	}

	// If it gets here then we've gone to the head of the queue! Woot!
	return current_slot_in_position;
}



int EVENT_juggle_queue (entity_event_queue *queue, int new_event_type)
{
	// This bubble-sorts the queue to find the space where the new
	// event goes, then returns that index.

	int e;

	for (e=queue->current_queue_size-1; e>=0; e--)
	{
		if (queue->event_types[e] >= new_event_type)
		{
			// The event in the current location is of the same or
			// a higher priority, so budge it up one.

			queue->event_types[e+1] = queue->event_types[e];
			queue->event_entity_ids[e+1] = queue->event_entity_ids[e];
		}
		else
		{
			// The event in the current location is of a lower priority.

			return e+1;
			// We return +1 as the next space up has been vacated by whatever
			// we pushed up already.
		}
	}

	// If we get here, everything in the queue was higher, so we're getting
	// dumped right at the bottom.
	return 0;
}



void EVENT_add_event_to_queue (int calling_entity_id, int add_to_entity_id, int event_type)
{
	// Adds an event to the specified entity, throwing a wobbly when you
	// attempt to add one to something which hasn't got a queue set up.

	entity_event_queue *queue = &ent_event_queues[add_to_entity_id];

	int slot_in_index;

	if ( queue->used == false )
	{
		OUTPUT_message("Bork!");
		assert(0);
	}
	else
	{
		if (queue->current_queue_size == queue->total_queue_size)
		{
			// Okay, the queue is FULL, so we need to nudge it.

			slot_in_index = EVENT_nudge_queue (queue, event_type);

			if (slot_in_index != UNSET)
			{
				queue->event_entity_ids[slot_in_index] = calling_entity_id;
				queue->event_types[slot_in_index] = event_type;
			}
			else
			{
				// The event was of a lower priority than anything else in the queue.
			}
		}
		else
		{
			slot_in_index = EVENT_juggle_queue (queue, event_type);

			queue->event_entity_ids[slot_in_index] = calling_entity_id;
			queue->event_types[slot_in_index] = event_type;

			queue->current_queue_size++;
		}
	}
}



int EVENT_get_event_from_queue (int entity_id, int *poster_entity_id)
{
	// Gets the highest priority event from the queue.

	int queue_length = ent_event_queues[entity_id].current_queue_size - 1;

	if (queue_length >= 0)
	{
		int event_type = ent_event_queues[entity_id].event_types[queue_length];
		*poster_entity_id = ent_event_queues[entity_id].event_entity_ids[queue_length];

		ent_event_queues[entity_id].current_queue_size--;

		return event_type;
	}
	else
	{
		*poster_entity_id = UNSET;
		return UNSET;
	}

}



void EVENT_empty_queue (int entity_id)
{
	// Blanks all events in the current queue.

	ent_event_queues[entity_id].current_queue_size = 0;
}
