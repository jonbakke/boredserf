#include <stdio.h>
#include <stdlib.h>

#include "messages.h"

void
_die(char *msg, int line)
{
	_err(msg, line);
	exit -1;
}

void
_err(char *msg, int line)
{
	fprintf(stderr, "At %d: %s\n", line, msg);
}

