/* This file is part of boredserf. */

/* boredboard is boredserf's "omnibar". It is not yet fully implemented.
 * It currently supports incremental find. The intention is to add the ability
 * to go to arbitrary URIs, to activate existing marks, and to match any
 * previously visited pages. In the long run, boredboard should also be
 * interface-independent: launch from within the browser, from the command
 * line, etc. */

/* void board(Client *c, const Arg *a)
 * Ingests keyboard shortcuts. Set a->i to an enum BoredBoard,
 * which will prioritize that type of search. */

/* gboolean board_handler(GtkWidget *w, GdkEvent *e, Client *c)
 * Handles GTK key-press events: plain typing to modify the query and
 * control-combinations that include [fgml] to toggle a type of search. */

/* void board_status(Client *c, char *input, char *match)
 * Generates titlebar status line from c->board_flags, input, and match.
 * Input is the search query. Match is the proposed action, if any. */

#ifndef BOARD_H
#define BOARD_H

#include "boredserf.h"

enum BoredBoard {
	boardtype_find,
	boardtype_goto,
	boardtype_mark,
	boardtype_local,
};

void board(Client *c, const Arg *a);
gboolean board_handler(GtkWidget *w, GdkEvent *e, Client *c);
void board_status(Client *c, GString *input, GString *match);

#endif /* BOARD_H */
