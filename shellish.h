/* This file is part of boredserf. */

/* char* sh_expand(char *str)
 * Passes str to /bin/sh for expansion, returning the result.
 * Result is malloc'd. */

/* char* filetostr(char *filename)
 * Reads contents of filename into returned string;
 * relative paths (e.g., ~/file) must first be expanded with sh_expand().
 * Result is malloc'd. */

/* char* cmd(char *input, char *command)
 * Runs command as argument after '/bin/sh -c';
 * input is piped to the command's standard input.
 * Returns the command's standard output.
 * Result is malloc'd. */

/* char* cmd_abs(char *input, char **cmd_array)
 * As cmd(), but uses execv(3) for efficiency and flexibility.
 * See cmd() for example of cmd_array implementation.
 * Result is malloc'd. */

#ifndef SHELLISH_H
#define SHELLISH_H

extern char winid[];
extern char **environ;

char* sh_expand(char *str);
char* filetostr(char *filename);
char* cmd(const char *input, const char *command[]);

#endif /* SHELLISH_H */
