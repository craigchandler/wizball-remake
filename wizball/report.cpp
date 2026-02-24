#include "report.h"
#include "fortify.h"



void REPORT_start (void)
{
	// This clears out the current report freeing any memory used up.
}

void REPORT_add_line (char *line)
{
	// This adds the line to the report, duh!
}

void REPORT_copy (char *pointer)
{
	// This takes a pointer, frees any memory allocated to it if there is some then allocates enough to copy the report over and does so.
}

void REPORT_output (char *filename)
{
	// This writes the report out to the specified file.
}



