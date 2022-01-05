/* This file is part of boredserf. */

#include "board.h"

void
board(Client *c, const Arg *a)
{
	/* reset flags if c->board_flags is zero or negative */
	if (0 >= c->board_flags && a) {
		c->board_flags = 0;
		c->board_flags |= 1 << a->i;
	}

	/* handle special cases */
	if (a) {
		switch (a->i) {
		case boardtype_go_relative:
			c->board_flags &= ~(1 << a->i);
			c->board_flags |= 1 << boardtype_goto;
			if (c->board_input)
				g_string_free(c->board_input, TRUE);
			c->board_input = geturi(c);
			break;
		default:
			break;
		}
	}

	replacekeytree(c, board_handler);
	board_status(c, NULL, NULL);
}

/* return TRUE if input event is handled; FALSE to pass to another handler */
gboolean
board_handler(GtkWidget *w, GdkEvent *e, Client *c)
{
	GString *input = c->board_input;
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
				setatom(c, AtomFind, empty_gs);
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
			if (input->len > 0)
				g_string_erase(input, input->len - 1, 1);
			break;

		default:
			g_string_append_c(input, e->key.keyval);
			break;
		}
	}

	/* reset entire search */
	if (-1 == c->board_flags) {
		g_string_erase(input, 0, -1);
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
board_status(Client *c, GString *input, GString *match)
{
	enum { flaglen = 4 };
	const char active[flaglen]   = "FGML";
	const char inactive[flaglen] = "fgml";
	GString *status = g_string_new(NULL);

	/* build status string */
	for (int i = 0; i < flaglen; ++i) {
		if (c->board_flags & 1 << i)
			g_string_append_c(status, active[i]);
		else
			g_string_append_c(status, inactive[i]);
	}
	g_string_append(status, ": ");

	/* add input */
	if (input && input->len) {
		g_string_append(status, input->str);
	} else if (c->board_flags & 1 << boardtype_find) {
		const char *existing =
			webkit_find_controller_get_search_text(c->finder);
		if (existing)
			g_string_append(status, existing);
	}

	/* add match */
	if (match && match->len)
		g_string_append(status, match->str);

	/* if neither input nor match, try c->board_input */
	if (!input && !match && c->board_input)
		g_string_append(status, c->board_input->str);

	/* apply */
	gtk_window_set_title(GTK_WINDOW(c->win), status->str);
	g_string_free(status, TRUE);
}
