/* This file is part of boredserf. */

#include "board.h"

enum { maxlen = 512 };

void
board(Client *c, const Arg *a)
{
	if (0 > c->board_flags && a) {
		c->board_flags = 0;
		c->board_flags |= 1 << a->i;
	}
	replacekeytree(c, board_handler);
	board_status(c, NULL, NULL);
}

/* return TRUE if input event is handled; FALSE to pass to another handler */
gboolean
board_handler(GtkWidget *w, GdkEvent *e, Client *c)
{
	static char status[maxlen];
	static char input[maxlen];
	static int pos = 0;

	if (GDK_KEY_PRESS != e->type)
		return FALSE;
	if (e->key.is_modifier)
		return FALSE;

	/* handle flags */
	if (e->key.state & GDK_CONTROL_MASK) {
		int modflag = -1;
		switch (e->key.keyval) {
		case 'f':
			modflag = boardtype_find;
			if (c->board_flags & 1 << modflag) {
				webkit_find_controller_search_finish(
					c->finder
				);
			}
			break;
		case 'g':
			modflag = boardtype_goto;
			break;
		case 'm':
			modflag = boardtype_mark;
			break;
		case 'l':
			modflag = boardtype_local;
			break;
		default:
			return FALSE;
		}

		if (0 <= modflag) {
			if (c->board_flags & 1 << modflag)
				c->board_flags &= ~(1 << modflag);
			else
				c->board_flags |= 1 << modflag;
		}

	} else { /* not control-key */

		/* build search string */
		switch (e->key.keyval) {
		case GDK_KEY_Escape: /* fall through */
			if (c->board_flags & 1 << boardtype_find) {
				setatom(c, AtomFind, "");
				webkit_find_controller_search_finish(
					c->finder
				);
			}
			c->board_flags = -1;
			break;

		case GDK_KEY_Return:
			/* exit board interface */
			c->overtitle = c->targeturi;
			updatetitle(c);

			/* goto resets upon commit */
			if (c->board_flags & 1 << boardtype_goto) {
				setatom(c, AtomGo, input);
				c->board_flags = -1;
				break;
			}
			replacekeytree(c, NULL);
			return TRUE;

		case GDK_KEY_BackSpace: /* fall through */
		case GDK_KEY_Delete:
			if (pos > 0)
				input[--pos] = 0;
			break;

		default:
			if (pos == maxlen - 1) {
				fprintf(stderr, "Search string is too long.");
				return TRUE;
			}
			input[pos++] = e->key.keyval;
			input[pos] = 0;
			break;
		}
	}

	/* reset entire search */
	if (-1 == c->board_flags) {
		pos = 0;
		input[pos] = 0;
		replacekeytree(c, NULL);
		c->overtitle = c->targeturi;
		updatetitle(c);
		return TRUE;
	}

	/* implement */
	board_status(c, input, NULL);
	if (c->board_flags & 1 << boardtype_find)
		setatom(c, AtomFind, input);
	return TRUE;
}

void
board_status(Client *c, char *input, char *match)
{
	const char active[]   = "FGML";
	const char inactive[] = "fgml";
	char status[maxlen];
	const char *existing;
	int i;

	/* build status string */
	for (i = 0; i < strlen(active); ++i) {
		if (c->board_flags & 1 << i)
			status[i] = active[i];
		else
			status[i] = inactive[i];
	}
	status[i] = 0;
	strncat(status, ": ", maxlen - strlen(status));
	if (input) {
		strncat(status, input, maxlen - strlen(status));
	} else if (c->board_flags & 1 << boardtype_find) {
		existing = webkit_find_controller_get_search_text(c->finder);
		if (existing)
			strncat(status, existing, maxlen - strlen(existing));
	}
	if (match)
		strncat(status, match, maxlen - strlen(status));
	gtk_window_set_title(GTK_WINDOW(c->win), status);
}
