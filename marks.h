/* This file is part of boredserf. */

/* void mark_read(BSMark **mark, int *msz)
 * Reads the file named by marks_loc (built from marksfile),
 * populating an array of BSMark of size msz at *mark. */

/* void mark_parse(char *line, BSMark *mark)
 * Reads the string at line, populating the BSMark at *mark;
 * if line is not recognized as a BSMark, mark->uri will be set NULL. */

/* char* mark_test(const char *uri)
 * If uri matches a mark, transforms it into a URI accordingly;
 * otherwise, prepends uri with 'http://' and returns it as if a URI.
 * Assumes that uri does not already have a scheme. */

#ifndef MARKS_H
#define MARKS_H

typedef struct {
	char *token;
	char *uri;
	int isquery;
} BSMark;

void  mark_read(BSMark **mark, int *msz);
void  mark_parse(char *line, BSMark *mark);
char* mark_test(const char *uri);

#endif /* MARKS_H */
