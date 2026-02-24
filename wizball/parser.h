#include "string_size_constants.h"

#ifndef _PARSER_H_
#define _PARSER_H_

// Parser header file...

#define MAX_ALIASES				(1024)

#define COMMAND_ARCHETYPE_NON_SPECIFIC			(-1)
	// This is for when a command archetype parameter is non-specific. Which it will be most of the time.
#define COMMAND_ARCHETYPE_EMPTY					(-2)
	// This is for empty commands. Ie, how they all start before I ride rough-shod over them with data.

#define VARIABLE				(-12)
	// This means it is a variable belonging to the current entity.
#define CONSTANT_VALUE			(-3)
	// This means it is a number, basically.
#define COMMAND					(-4)
	// Command
#define FLAG					(-5)
#define CARD					(-6)
#define SPRITE					(-7)
#define GRAPHIC					(-8)
#define SOUND_FX				(-9)
#define SCRIPT					(-10)
#define PATH					(-11)
#define COMPARE_OPERATION		(-95)
	// Data type.
#define MATH_OPERATION			(-96)
	// Data type.
#define MATH_COMPARATIVE		(-100)
	// Data type.
//#define IGNORE					(-97)
	// These are statements which merely aid the grammar of the language.
#define	NUMBER					(-98)
	// So does this, except this isn't a listed constant and so the string has to be evaluated instead.
#define LABEL					(-99)
	// And this is a label, which is gotten from the label list but all the same is kind of a special case.
	// The odd value of 99 is used so that when dumping a script onto the end of the big script list we
	// can find and offset any values of type -99 (LABEL).

#define INDENT_LINE						(1)
#define UNINDENT_LINE					(2)
#define STORE_EQUAL_INDENT_LINE			(4)
#define STORE_LESSER_INDENT_LINE		(8)
#define STORE_PREV_EQUAL_INDENT_LINE	(16)

#define DATA_BITMASK_IDENTITY_VARIABLE	(1) // This is when a value is contained within the variable of another entity and the "data_type" holds the variable which contains that identity. Crivens!
#define DATA_BITMASK_NEGATE_VALUE		(2) // Negates the value before returning it, for when a script has something like "X = -CONSTANT"
#define DATA_BITMASK_INVERT_VALUE		(4) // Inverts the value before returning it, for when a script has something like "IF A AND !B"

// Tokenised script structures (temporary and permanent)

typedef struct
{
	int data_type;
	int data_value;
	int data_bitmask;
} script_data;

typedef struct
{
	script_data *script_line_pointer;
	int script_line_size;
	int command_instruction_index;
	int indentation_level;
} script_line;

typedef struct
{
	int data_type;
	int data_value;
	int data_bitmask;
	int alternate_number;
	int source_word_list;
	int source_word_list_entry_1;
	int source_word_list_entry_2;
	int alias_1_index;
	int alias_2_index;
} temp_script_data;

typedef struct
{
	temp_script_data *script_line_pointer; // The argument pointer.
	int script_line_size; // How many arguments in the line.
	int indentation_level; // How indented the line is.
	int prefix_value_type; // The data's signiture, ie FOR/NEXT or SWITCH/END SWITCH or CASE/BREAK
	int indentation_data; // Includes the flags which say whether it's indented.
	int archetype_number; // Which archetype eventually contributed to this line.
} temp_script_line;

// Command syntax archetype structures...

typedef struct
{
	int word_list_index;
	int specific_list_entry;
} archetype;

typedef struct
{
	char argument_description[NAME_SIZE]; // This is just it's description so if I write an editor it knows how to describe what the next argument it wants from you is.
	int argument_type; // This is a bitflag showing what kind of argument it is, is it a changed variable? Or a parameter? Again it'll help the editor, but also the debugger.
	archetype *alternate_list;
	int alternate_list_size;
} command_syntax_argument;

typedef struct
{
	command_syntax_argument *argument_list;
	int argument_list_size; // How many arguments this command/function has.
	int prefix_value; // This is a bitmask including data about whether this statement indents or unindents a line, etc.
	int prefix_value_type; // This is the type of data pushed and popped off the stack, it makes sure you don't imbalance things by pushing a FOR and then popping a END_SWITCH or the like.
	int line_type; // Is the line a normal line, a program flow control line (ie, IF) or a redirect line (ie, GOTO).
} command_syntax_archetype;

// Text script structures... This is where loaded scripts are placed one at a time.

typedef struct
{
	char *text_word;
	int alias_1_index;
	int alias_2_index;
} text_argument;

typedef struct
{
	text_argument *text_word_list;
	int line_length;
} text_argument_line;

// Word list structure...

typedef struct
{
	char name [NAME_SIZE]; // The generic name of this list...
	int data_type; // The number which is used for data_type when a member of this list is used.
	text_argument *words; // The names of the list, each entry is NAME_SIZE in length.
	int *word_values; // The values of the list.
	int word_list_size; // How many entries there are in the list.
	bool has_extension; // Do we even have an extension?
	char extension [NAME_SIZE]; // If this is a list of files, this is the extension.
} word_list_struct;



typedef struct
{
	char name[NAME_SIZE];
	char list[MAX_LINE_SIZE];
} syntax_parameter_list_entry_struct;



bool PARSER_parse (char *filename);
void PARSER_destroy_trace_script (void);



#define TRACER_ARGUMENT_TYPE_COMMAND				(1)
#define TRACER_ARGUMENT_TYPE_PARAMETER				(2)
#define TRACER_ARGUMENT_TYPE_FIXED_VALUE			(4)
#define TRACER_ARGUMENT_TYPE_VARIABLE				(8)
#define TRACER_ARGUMENT_TYPE_LABEL					(16)
#define TRACER_ARGUMENT_TYPE_MATH_OPERATOR			(32)
#define TRACER_ARGUMENT_TYPE_OPERAND				(64)



#define TRACER_LINE_TYPE_NORMAL						(1)
#define TRACER_LINE_TYPE_PROGRAM_FLOW				(2)
#define TRACER_LINE_TYPE_REDIRECT					(4)



typedef struct
{
	char *text_word; // Dur...
	int argument_type_bitflag; // Is it a variable? Hmm? 
} tracer_text_word;

typedef struct
{
	int line_indentation; // The indentation of the line. Oh you want it, baby.
	int line_type_bitflag; // Is it a decisioney line or not? Hmm? HMM?
	int line_length; // How many arguments in the line.
	tracer_text_word *argument_list; // This is the line. Ooh.
} tracer_script_line_struct;



extern tracer_script_line_struct *tracer_script;



#endif

