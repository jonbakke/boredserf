#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "messages.h"
#include "shellish.h"

char*
sh_expand(char *str)
{
	char *result;
	char *command;
	int len;

	nullguard(str, NULL);

	command = malloc(strlen(str) + 6);
	command[0] = 0;
	strcat(command, "echo ");
	strcat(command, str);

	result = cmd(NULL, command);
	free(command);
	return result;
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
cmd(char *input, char *command)
{
	char *cmd_array[5] = { "/bin/sh", "sh", "-c", command, NULL };
	char *result;
	int len = 0;

	nullguard(command, NULL);

	result = cmd_abs(input, cmd_array);
	len = strlen(result);
	if ('\n' == result[len - 1])
		result[len - 1] = 0;
	return result;
}

char*
cmd_abs(char *input, char **cmd_array)
{
	char *buf;
	char *result;
	char *target;
	int cmdpipe[2] = { -1 };
	int retpipe[2] = { -1 };
	int bufsz = 4096;
	int retsz = 0;
	int bufpos;

	nullguard(cmd_array, NULL);
	nullguard(cmd_array[0], NULL);
	nullguard(cmd_array[1], NULL);
	nullguard(cmd_array[2], NULL);

	if (0 > pipe(cmdpipe) || 0 > pipe(retpipe))
		err("Could not create pipe.", NULL);

	if (!fork()) {
		/* child process */
		close(cmdpipe[1]);
		close(retpipe[0]);
		dup2(cmdpipe[0], 0);
		dup2(retpipe[1], 1);
		close(cmdpipe[0]);
		close(retpipe[1]);
		close(2);
		execv(cmd_array[0], &cmd_array[1]);
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
