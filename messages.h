/* This file is part of boredserf. */

/* void die(char *msg)
 * Sends msg to standard error and exits the program.
 * die() is a macro that uses the _die() function. */

/* void err(char *msg, ...)
 * Sends msg to standard error, then returns with a value of ...; e.g., in a
 * void function, err("Oops") is sufficient, but in a function that returns a
 * pointer, use err("Oops", NULL).
 * err() is a macro that uses the _err() function. */

/* void nullguard(void *ptr, ...) 
 * If ptr is NULL, sends a generic warning to standard error and returns with a
 * value of ..., as with err().
 * nullguard() is a macro. */

/* Note: the macros wrap commands in a do {} while(0) loop to allow them to be
 * used as functions in `if () macro(); else {}` construtions. If the macro
 * expansion ended with a ; then the result would have two semicolons after the
 * if, meaning the statement preceding the else would be a no-op instead of an
 * if, and the else would therefore be illegal. */

#ifndef MESSAGES_H
#define MESSAGES_H

#define die(msg) \
	do { \
		_die(msg, __LINE__); \
	} while (0)

#define err(msg, ...) \
	do { \
		_err(msg, __LINE__); \
		return __VA_ARGS__; \
	} while (0)

#define nullguard(x, ...) \
	do {\
		if (NULL == x) {\
			fprintf( \
				stderr, \
				"Unexpected null at line %d.\n", \
				__LINE__ \
			); \
			return __VA_ARGS__; \
		} \
	} while(0)

void _die(char *msg, int line);
void _err(char *msg, int line);

#endif /* MESSAGES_H */
