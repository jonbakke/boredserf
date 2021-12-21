#include <stdio.h>
#include <stdlib.h>

#include "common.h"

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

void
_ifnotnullfreeandnull(void **ptr)
{
	nullguard(ptr);
	g_free(*ptr);
	*ptr = NULL;
}
