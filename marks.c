#include <glib.h>
#include <stdio.h>

#include "boredserf.h"
#include "common.h"
#include "marks.h"

enum { linemax = 1024 };
enum { markszdef = 32 };

/* read marks from disk;
/* malloc's each mark and string(s) for each mark; however, should only
 * be called once and remains useful for duration of runtime. */
void
mark_read(BSMark **mark_out, int *msz_out)
{
	BSMark *mark = NULL;
	int msz = 0;
	int mpos = 0;
	FILE *file;
	int fsz = 0;
	int fpos = 0;
	GArray *lineend = NULL;
	int ch = 0;
	char *line = NULL;
	int lnum = 0;
	int lstart = 0;
	int lend = 0;
	int lsz = 0;
	int lpos = 0;

	/* open file */
	file = fopen(marks_loc->str, "r");
	nullguard(file);
	fseek(file, 0, SEEK_END);
	fsz = ftell(file);

	/* get line count and line-ending positions
	 * allocate memory for one BSMark per line */
	fpos = 0;
	rewind(file);
	lineend = g_array_new(FALSE, FALSE, sizeof(int));
	while (fpos < fsz) {
		ch = fgetc(file);
		if ('\n' == ch || '\r' == ch || 0 == ch || EOF == ch)
			g_array_append_vals(lineend, &fpos, 1);
		++fpos;
	}
	msz = lineend->len;
	mark = g_malloc(sizeof(BSMark) * msz);

	/* for each line */
	fpos = 0;
	rewind(file);
	while (lnum < lineend->len) {
		/* copy next line */
		lpos = 0;
		lstart = 0 < lnum ? ((int*)lineend->data)[lnum - 1] : 0;
		lend = ((int*)lineend->data)[lnum];
		lsz = lend - lstart;
		if (NULL != line)
			g_free(line);
		line = g_malloc(lsz + 1);
		if(lsz != fread(line, 1, lsz, file))
			err("Could not read marks file.");
		line[lsz] = 0;
		++lnum;

		/* get BSMark for this line */
		mark_parse(line, &mark[mpos]);
		if (NULL != mark[mpos].uri)
			++mpos;
	}
	*mark_out = mark;
	*msz_out = mpos;
}

void
mark_parse(char *line, BSMark *mark)
{
	GScanner *scan = NULL;
	char *allowed_mark =
		"abcdefghijklmnopqrstuvwxyz"
		"~1!2@3#4$5%6^7&8*9(0)-_=+[{]},<.>;/\\|?"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char *skip_mark = ": \t\n\r";
	char *allowed_uri =
		"abcdefghijklmnopqrstuvwxyz"
		"~1!2@3#4$5%6^7&8*9(0)-_=+[{]},<.>;:/\\|?"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char *skip_uri = " \t\n\r";
	char *field1 = NULL;
	char *field2 = NULL;
	char *field3 = NULL;
	char *markquery = NULL;

	nullguard(line);
	nullguard(mark);

	mark->uri = NULL;

	scan = g_scanner_new(NULL);
	g_scanner_input_text(scan, line, strlen(line));

	scan->config->cset_skip_characters = skip_mark;
	scan->config->cset_identifier_first = allowed_mark;
	scan->config->cset_identifier_nth = allowed_mark;
	scan->config->scan_identifier_1char = FALSE;

	if (G_TOKEN_IDENTIFIER == g_scanner_get_next_token(scan))
		field1 = strdup(scan->value.v_identifier);
	else
		goto markparse_cleanup;

	scan->config->scan_identifier_1char = TRUE;
	if (G_TOKEN_IDENTIFIER == g_scanner_get_next_token(scan))
		field2 = strdup(scan->value.v_identifier);
	else
		goto markparse_cleanup;

	scan->config->cset_skip_characters = skip_uri;
	if (':' != g_scanner_get_next_token(scan))
		goto markparse_cleanup;

	scan->config->cset_identifier_first = allowed_uri;
	scan->config->cset_identifier_nth = allowed_uri;

	if (G_TOKEN_IDENTIFIER == g_scanner_get_next_token(scan))
		field3 = strdup(scan->value.v_identifier);
	else
		goto markparse_cleanup;

	if (field1 && 0 == strcmp("mark", field1)) {
		g_free(field1);
		mark->token = field2;
		mark->uri = field3;

		markquery = mark->token + strlen(mark->token) - 1;
		if ('?' == *markquery) {
			*markquery = 0;
			mark->isquery = 1;
		} else {
			mark->isquery = 0;
		}
		return;
	}

markparse_cleanup:
	freeandnull(field1);
	freeandnull(field2);
	freeandnull(field3);
}

char*
mark_test(const char *uri)
{
	static BSMark *mark = NULL;
	static int msz = 0;
	char maybetoken[linemax];
	char *result = NULL;
	int ressz;
	int i;
	int c;

	if (NULL == marks_loc || NULL == uri)
		return NULL;

	if (NULL == mark)
		mark_read(&mark, &msz);

	/* search through marks for matches */
	for (i = 0; i < msz; ++i) {
		if (NULL == mark[i].token)
			continue;
		for (c = 0; uri[c]; ++c) {
			maybetoken[c] = uri[c];
			if (
				' ' == maybetoken[c] ||
				'\t' == maybetoken[c]
			) {
				maybetoken[c] = 0;
				++c;
			}
			if (c == linemax - 1)
				break;
		}
		maybetoken[c] = 0;
		if (
			strlen(mark[i].token) == strlen(maybetoken) &&
			0 == strcmp(mark[i].token, maybetoken)
		) {
			if (mark[i].isquery) {
				ressz = strlen(uri) + strlen(mark[i].uri);
				result = g_malloc(ressz);
				snprintf(
					result,
					ressz,
					mark[i].uri,
					uri + strlen(maybetoken) + 1
				);
				return result;
			} else {
				return strdup(mark[i].uri);
			}
		}
	}

	/* duplicate string to ensure returned value can be free'd */
	/* add http:// to each uri (note: loaduri() tests for this first */
	result = g_malloc(strlen(uri) + 8);
	result[0] = 0;
	strcat(result, "http://");
	strcat(result, uri);
}

