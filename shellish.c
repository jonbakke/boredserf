#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "shellish.h"

char*
sh_expand(char *str)
{
	const char *command[] = {
		"/bin/sh",
		"sh", "-c", "echo", str,
		NULL
	};

	nullguard(str, NULL);
	return cmd(NULL, command);
}

char*
filetostr(char *filename)
{
	FILE *file;
	char *result;
	char *expanded_path;
	int len;

	nullguard(filename, NULL);

	if (!(file = fopen(filename, "r"))) {
		fprintf(stderr, "With %s: ", filename);
		err("Unrecognized filename.", NULL);
	}
	fseek(file, 0, SEEK_END);
	len = ftell(file);
	rewind(file);

	result = malloc(len + 1);
	result[len] = 0;
	nullguard(result, NULL);
	if (len != fread(result, 1, len, file)) {
		fclose(file);
		free(result);
		err("Failed to read file.", NULL);
	}

	fclose(file);
	return result;
}

char*
cmd(const char *input, const char *command[])
{
	enum { fdstrlen = 32 };
	char *buf;
	char *result;
	char *target;
	char intstr[fdstrlen] = { 0 };
	int cmdpipe[2] = { -1 };
	int retpipe[2] = { -1 };
	int bufsz = 4096;
	int retsz = 0;
	int bufpos;

	nullguard(command, NULL);
	nullguard(command[0], NULL);
	nullguard(command[1], NULL);

	if (0 > pipe(cmdpipe) || 0 > pipe(retpipe))
		err("Could not create pipe.", NULL);

	if (!fork()) {
		/* child process */
		close(cmdpipe[1]);
		close(retpipe[0]);
		dup2(cmdpipe[0], 0);
		dup2(retpipe[1], 1);
		snprintf(intstr, fdstrlen, "%u", cmdpipe[0]);
		setenv("BS_INPUT", intstr, 1);
		snprintf(intstr, fdstrlen, "%u", retpipe[1]);
		setenv("BS_RESULT", intstr, 1);
		execv(command[0], (char * const *)&command[1]);
	}

	close(cmdpipe[0]);
	close(retpipe[1]);

	/* write to child's stdin */
	if (NULL != input)
		write(cmdpipe[1], input, strlen(input));
	close(cmdpipe[1]);

	/* read child's stdout to buf, enlarging buf as necessary */
	buf = malloc(bufsz);
	bufpos = 0;
	while(bufsz == bufpos +
		(retsz = read(retpipe[0], &buf[bufpos], bufsz - bufpos))
	) {
		bufpos += retsz;
		bufsz *= 2;
		buf = realloc(buf, bufsz);
	};
	bufpos += retsz;
	if (0 >= bufpos) {
		free(buf);
		return NULL;
	}

	/* move result to smaller memory allocation */
	result = malloc(bufpos + 1);
	result[bufpos] = 0;
	strncpy(result, buf, bufpos);
	free(buf);
	return result;
}