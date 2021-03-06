/* This file is part of boredserf. */

#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gcr/gcr.h>
#include <JavaScriptCore/JavaScript.h>
#include <X11/X.h>
#include <X11/Xatom.h>

#include "arg.h"
#include "board.h"
#include "boredserf.h"
#include "common.h"
#include "filter.h"
#include "marks.h"
#include "shellish.h"

#include "config.h"

#define LENGTH(x)               (sizeof(x) / sizeof(x[0]))
#define CLEANMASK(mask)         (mask & (MODKEY|GDK_SHIFT_MASK))

ParamName loadtransient[] = {
	Certificate,
	CookiePolicies,
	DiskCache,
	DNSPrefetch,
	FileURLsCrossAccess,
	JavaScript,
	LoadImages,
	PreferredLanguages,
	ShowIndicators,
	StrictTLS,
	ParameterLast
};

ParamName loadcommitted[] = {
	//	AccessMicrophone,
	//	AccessWebcam,
	CaretBrowsing,
	DefaultCharset,
	FontSize,
	FrameFlattening,
	Geolocation,
	HideBackground,
	Inspector,
	Java,
	//	KioskMode,
	MediaManualPlay,
	RunInFullscreen,
	ScrollBars,
	SiteQuirks,
	SmoothScrolling,
	SpellChecking,
	SpellLanguages,
	Style,
	ZoomLevel,
	ParameterLast
};

ParamName loadfinished[] = {
	ParameterLast
};

char winid[64];
char togglestats[11];
char pagestats[2];
Atom atoms[AtomLast];
Window embed;
int showxid;
int cookiepolicy;
Display *dpy;
Client *clients;
GdkDevice *gdkkb;
Key *curkeytree;
char *stylefile;
const char *useragent;
char *pagefiles;
Parameter *curconfig;
int modparams[ParameterLast];
int spair[2];
char *argv0;
GString *cookie_loc;
GString *script_loc;
GString *visited_loc;
GString *marks_loc;
GString *filterrule_loc;
GString *filterdir_loc;
GString *certdir_loc;
GString *cachedir_loc;
GString *style_loc;
const GString *empty_gs;
const GString *blank_gs;

void
usage(void)
{
	die("usage: boredserf [-bBdDfFgGiIkKmMnNpPsStTvwxXyY]\n"
		"[-a cookiepolicies ] [-c cookiefile] [-C stylefile]"
		"[-e xid]\n"
		"[-r scriptfile] [-u useragent] [-z zoomlevel] [uri]\n");
}

void
setup(void)
{
	GdkDisplay *gdpy;

	if (signal(SIGHUP, sighup) == SIG_ERR)
		die("Could not install SIGHUP handler.");

	if (!(dpy = XOpenDisplay(NULL)))
		die("Could not open default display.");

	setenv("GDK_BACKEND", "x11", 0);

	/* atoms */
	atoms[AtomFind] = XInternAtom(dpy, "_BS_FIND", False);
	atoms[AtomGo] = XInternAtom(dpy, "_BS_GO", False);
	atoms[AtomUri] = XInternAtom(dpy, "_BS_URI", False);
	atoms[AtomUTF8] = XInternAtom(dpy, "UTF8_STRING", False);

	gtk_init(NULL, NULL);

	gdpy = gdk_display_get_default();

	curconfig = defconfig;

	/* dirs and files */
	if (curconfig[Ephemeral].val.i)
		cachedir = NULL;
	cookie_loc     = buildfile(cookiefile);
	script_loc     = buildfile(scriptfile);
	visited_loc    = buildfile(visitedfile);
	marks_loc      = buildfile(marksfile);
	filterrule_loc = buildfile(filterrulefile);
	filterdir_loc  = buildpath(filterdir);
	certdir_loc    = buildpath(certdir);
	cachedir_loc   = buildpath(cachedir);

	gdkkb = gdk_seat_get_keyboard(gdk_display_get_default_seat(gdpy));

	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, spair) < 0) {
		fputs("Unable to create sockets\n", stderr);
		spair[0] = spair[1] = -1;
	} else {
		GIOChannel *gchanin;
		gchanin = g_io_channel_unix_new(spair[0]);
		g_io_channel_set_encoding(gchanin, NULL, NULL);
		g_io_channel_set_flags(
			gchanin,
			g_io_channel_get_flags(gchanin) | G_IO_FLAG_NONBLOCK,
			NULL
		);
		g_io_channel_set_close_on_unref(gchanin, TRUE);
		g_io_add_watch(gchanin, G_IO_IN, readsock, NULL);
	}

	for (int i = 0; certdir_loc && i < LENGTH(certs); ++i) {
		if (!regcomp(&(certs[i].re), certs[i].regex, REG_EXTENDED)) {
			certs[i].file = g_strconcat(
				certdir_loc->str,
				"/",
				certs[i].file,
				NULL
			);
		} else {
			g_printerr(
				"Could not compile regex: %s\n",
				certs[i].regex
			);
			certs[i].regex = NULL;
		}
	}

	if (!stylefile && styledir) {
		GString *styledir_loc = buildpath(styledir);
		for (int i = 0; i < LENGTH(styles); ++i) {
			if (!styledir_loc)
				styles[i].file = NULL;
			if (!styles[i].file)
				continue;
			if (
				!regcomp(
					&(styles[i].re),
					styles[i].regex,
					REG_EXTENDED
				)
			) {
				styles[i].file = g_strconcat(
					styledir_loc->str,
					"/",
					styles[i].file,
					NULL
				);
			} else {
				g_printerr(
					"Could not compile regex: %s\n",
					styles[i].regex
				);
				styles[i].regex = NULL;
			}
		}
		if (styledir_loc)
			g_string_free(styledir_loc, TRUE);
	} else {
		style_loc = buildfile(stylefile);
	}

	for (int i = 0; i < LENGTH(uriparams); ++i) {
		if (
			regcomp(
				&(uriparams[i].re),
				uriparams[i].uri,
				REG_EXTENDED
			)
		) {
			g_printerr(
				"Could not compile regex: %s\n",
				uriparams[i].uri
			);
			uriparams[i].uri = NULL;
			continue;
		}

		/* copy default parameters with higher priority */
		for (int j = 0; j < ParameterLast; ++j) {
			if (defconfig[j].prio >= uriparams[i].config[j].prio)
				uriparams[i].config[j] = defconfig[j];
		}
	}

	pagefiles = NULL;
	empty_gs = g_string_new(NULL);
	blank_gs = g_string_new("about:blank");
}

void
sighup(int unused)
{
	Arg a = { .i = 0 };
	for (Client *c = clients; c; c = c->next)
		reload(c, &a);
}

GString*
buildfile(const char *input)
{
	FILE *file;
	GString *dirpath_gs;
	char *dirname_c;
	char *basename_c;
	char *path_c;
	struct stat st;

	if (NULL == input)
		return NULL;
	dirname_c  = g_path_get_dirname(input);
	basename_c = g_path_get_basename(input);

	dirpath_gs = buildpath(dirname_c);
	g_free(dirname_c);

	path_c = g_build_filename(dirpath_gs->str, basename_c, NULL);
	g_string_free(dirpath_gs, TRUE);
	g_free(basename_c);

	/* test if file exists, is a regular file, and  */
	if (stat(path_c, &st))
		return NULL;
	if (!S_ISREG(st.st_mode))
		return NULL;

	if (!(file = fopen(path_c, "a"))) {
		g_printerr("For %s: ", path_c);
		die("Could not open file.");
	}

	/* always */
	g_chmod(path_c, 0600);
	fclose(file);

	return g_string_new(path_c);
}

GString*
buildpath(const char *input)
{
	GString *path;
	char *rp;
	struct stat st;

	if (NULL == input)
		return NULL;

	path = g_string_new(input);
	if (g_str_has_prefix(input, "~/"))
		g_string_replace(path, "~", g_get_home_dir(), 1);

	/* if directory does not exist, create it;
	 * WebKit requires some folders to exist */
	if (stat(path->str, &st)) {
		mkdir(path->str, 0700);
		if (stat(path->str, &st))
			err("Could not create folder.", NULL);
	}
	if (!S_ISDIR(st.st_mode))
		return NULL;

	rp = realpath(path->str, NULL);
	nullguard(rp, path);
	path = g_string_assign(path, rp);
	g_free(rp);
	return path;
}

Client*
newclient(Client *rc)
{
	Client *c;

	if (!(c = calloc(1, sizeof(Client))))
		die("Failed to allocate memory for Client.");

	c->next = clients;
	clients = c;

	c->progress = 100;
	c->view = newview(c, rc ? rc->view : NULL);
	c->title = g_string_new(NULL);
	c->overtitle = g_string_new(NULL);
	c->board_flags = -1;
	c->board_input = g_string_new(NULL);
	c->board_cb = NULL;

	return c;
}

void
loaduri(Client *c, const Arg *a)
{
	GString *input;
	GString *currenturi;
	GString *url;
	FILE *visited;

	/* require some input from a->v */
	if (!a || !a->v || !((char*)a->v)[0])
		return;
	input = g_string_new(a->v);

	if (
		g_str_has_prefix(input->str, "https://") ||
		g_str_has_prefix(input->str, "http://")  ||
		g_str_has_prefix(input->str, "file://")  ||
		g_str_has_prefix(input->str, "about:")
	) {
		url = input;
	} else {
		struct stat st;
		char *path_c;
		url = g_string_new(NULL);
		if (g_str_has_prefix(input->str, "~/"))
			g_string_replace(input, "~", g_get_home_dir(), 1);

		if (
			/* stat returns 0 on success */
			!stat(input->str, &st) &&
			(path_c = realpath(input->str, NULL))
		) {
			g_string_printf(url, "file://%s", path_c);
		} else {
			path_c = mark_test(input->str);
			nullguard(path_c);
			g_string_append(url, path_c);
		}
		g_string_free(input, TRUE);
		g_free(path_c);
	}

	setatom(c, AtomUri, url);

	currenturi = geturi(c);
	if (g_string_equal(url, currenturi)) {
		reload(c, a);
	} else {
		webkit_web_view_load_uri(c->view, url->str);
		updatetitle(c);
	}

	logvisit(url);
	resetkeytree(c);
	g_string_free(url, TRUE);
	g_string_free(currenturi, TRUE);
}

void
updateenv(Client *c)
{
	static GString *uri = NULL;
	GString *currenturi = geturi(c);

	/* update once per URI */
	if (uri && currenturi && g_string_equal(uri, currenturi)) {
		g_string_free(currenturi, TRUE);
		return;
	}
	if (NULL != uri)
		g_string_free(uri, TRUE);
	uri = currenturi;

	if (NULL == pagefiles) {
		int len;
		int pos = 0;
		nullguard(cachedir_loc);
		len = cachedir_loc->len + 12;
		pagefiles = g_malloc(len);
		pagefiles[pos] = 0;
		strncat(pagefiles, cachedir_loc->str, len);
		strncat(pagefiles, "/tmp-XXXXXX", len - strlen(pagefiles));
		mkdtemp(pagefiles);
		nullguard(pagefiles);
		setenv("BS_PAGEFILES", pagefiles, 1);
	}

	getpagetext(c);
	getpagehead(c);
	getpagebody(c);
	getpagetitle(c);
	getpagelinks(c);
	getpagescripts(c);
	//getpagestyles(c);
	getpageimages(c);
	setenv("BS_URI", uri->str, 1);
	if (visited_loc && visited_loc->len)
		setenv("BS_VISITED", visited_loc->str, 1);
	filter_stripper(c, getenv("BS_URI"));
	getpagehead_stripped(c);
	getpagebody_stripped(c);
}

GString*
geturi(Client *c)
{
	const char *uri = NULL;
	if (!(uri = webkit_web_view_get_uri(c->view)))
		uri = "about:blank";
	return g_string_new(uri);
}

void
setatom(Client *c, int a, const GString *v)
{
	XChangeProperty(
		dpy,
		c->xid,
		atoms[a],
		atoms[AtomUTF8],
		8,
		PropModeReplace,
		(unsigned char *)v->str,
		v->len + 1
	);
	XSync(dpy, False);
}

const char*
getatom(Client *c, int a)
{
	static char buf[BUFSIZ];
	Atom adummy;
	int idummy;
	unsigned long ldummy;
	unsigned char *p = NULL;

	XSync(dpy, False);
	XGetWindowProperty(
		dpy,
		c->xid,
		atoms[a],
		0L,
		BUFSIZ,
		False,
		atoms[AtomUTF8],
		&adummy,
		&idummy,
		&ldummy,
		&ldummy,
		&p
	);
	if (p)
		strncpy(buf, (char *)p, LENGTH(buf) - 1);
	else
		buf[0] = '\0';
	XFree(p);

	return buf;
}

void
updatetitle(Client *c)
{
	const char *name = c->overtitle->len
		? c->overtitle->str
		: c->title->len
			? c->title->str
			: "";

	if (curconfig[ShowIndicators].val.i) {
		char *title;
		gettogglestats(c);
		getpagestats(c);

		if (c->progress != 100)
			title = g_strdup_printf(
				"[%i%%] %s:%s | %s",
				c->progress,
				togglestats,
				pagestats,
				name
			);
		else
			title = g_strdup_printf(
				"%s:%s | %s",
				togglestats,
				pagestats,
				name
			);

		gtk_window_set_title(GTK_WINDOW(c->win), title);
		g_free(title);
	} else {
		gtk_window_set_title(GTK_WINDOW(c->win), name);
	}
}

void
gettogglestats(Client *c)
{
	togglestats[0]  = cookiepolicy_set(cookiepolicy_get());
	togglestats[1]  = curconfig[CaretBrowsing].val.i ?   'C' : 'c';
	togglestats[2]  = curconfig[Geolocation].val.i ?     'G' : 'g';
	togglestats[3]  = curconfig[DiskCache].val.i ?       'D' : 'd';
	togglestats[4]  = curconfig[LoadImages].val.i ?      'I' : 'i';
	togglestats[5]  = curconfig[JavaScript].val.i ?      'S' : 's';
	togglestats[6]  = curconfig[Style].val.i ?           'M' : 'm';
	togglestats[7]  = curconfig[FrameFlattening].val.i ? 'F' : 'f';
	togglestats[8]  = curconfig[Certificate].val.i ?     'X' : 'x';
	togglestats[9]  = curconfig[StrictTLS].val.i ?       'T' : 't';
	togglestats[10] = curconfig[ContentFilter].val.i ?   'Y' : 'y';
}

void
getpagestats(Client *c)
{
	if (c->https)
		pagestats[0] = (c->tlserr || c->insecure) ?  'U' : 'T';
	else
		pagestats[0] = '-';
	pagestats[1] = '\0';
}

void
getwkjs(Client *c, char *script, WebKit_JavaScript_cb cb)
{
	nullguard(script);
	webkit_web_view_run_javascript(
		c->view,
		script,
		NULL,
		cb,
		NULL
	);
}

char*
getwkjs_guard(GObject *source, GAsyncResult *res, gpointer data)
{
	WebKitJavascriptResult *js_res;
	JSCValue *value;
	GError *error = NULL;
	char *result = NULL;

	js_res = webkit_web_view_run_javascript_finish(
		WEBKIT_WEB_VIEW(source),
		res,
		&error
	);
	if (NULL == js_res) {
		nullguard(error, NULL);
		g_printerr(
			"Error running JavaScript: %s\n",
			error->message
		);
		g_error_free(error);
		return result;
	}

	value = webkit_javascript_result_get_js_value(js_res);
	if (jsc_value_is_undefined(value) || jsc_value_is_null(value))
		return NULL;
	if (jsc_value_is_string(value)) {
		JSCException *exception;
		exception = jsc_context_get_exception(
			jsc_value_get_context(value)
		);
		if (exception) {
			g_printerr(
				"Error running WebKit JavaScript: %s\n",
				jsc_exception_get_message(exception)
			);
		} else {
			result = jsc_value_to_string(value);
		}
	} else {
		g_printerr("WebKit JavaScript returned unexpected value.\n");
	}
	webkit_javascript_result_unref(js_res);
	return result;
}

void
savepagefile(const char *name, const char *contents)
{
	char *filename;
	FILE *file;

	/* Fail silently if anything is NULL */
	if (!name || !contents || !pagefiles)
		return;

	filename = g_malloc(strlen(pagefiles) + strlen(name) + 2);
	filename[0] = 0;
	strcat(filename, pagefiles);
	strcat(filename, "/");
	strcat(filename, name);

	file = fopen(filename, "w+");
	if (NULL == file) {
		g_free(filename);
		nullguard(file);
	}
	fwrite(contents, 1, strlen(contents), file);
	fclose(file);
}

void
getpagetext(Client *c)
{
	nullguard(c);
	getwkjs(c, "document.body.innerText", getpagetext_cb);
}

void
getpagetext_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	raw = getwkjs_guard(source, res, data);
	savepagefile("page.txt", raw);
	freeandnull(raw);
}

void
getpagehead(Client *c)
{
	nullguard(c);
	getwkjs(c, "document.head.innerHTML", getpagehead_cb);
}

void
getpagehead_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	raw = getwkjs_guard(source, res, data);
	savepagefile("head.html", raw);
	freeandnull(raw);
}

void
getpagehead_stripped(Client *c)
{
	nullguard(c);
	getwkjs(c, "document.head.innerHTML", getpagehead_stripped_cb);
}

void
getpagehead_stripped_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	raw = getwkjs_guard(source, res, data);
	savepagefile("head-stripped.html", raw);
	freeandnull(raw);
}

void
getpagebody(Client *c)
{
	nullguard(c);
	getwkjs(c, "document.body.innerHTML", getpagebody_cb);
}

void
getpagebody_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	raw = getwkjs_guard(source, res, data);
	savepagefile("body.html", raw);
	freeandnull(raw);
}

void
getpagebody_stripped(Client *c)
{
	nullguard(c);
	getwkjs(c, "document.body.innerHTML", getpagebody_stripped_cb);
}

void
getpagebody_stripped_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	raw = getwkjs_guard(source, res, data);
	savepagefile("body-stripped.html", raw);
	freeandnull(raw);
}

void
getpagetitle(Client *c)
{
	nullguard(c);
	getwkjs(c, "document.title", getpagetitle_cb);
}

void
getpagetitle_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	raw = getwkjs_guard(source, res, data);
	savepagefile("title.txt", raw);
	freeandnull(raw);
}

void
getpagelinks(Client *c)
{
	char *script = ""
		"var list = document.links;"
		"var max = list.length;"
		"var result = \"\";"
		"var i;"
		"for (i = 0; i < max; i++) {"
		"    result = result + list[i].href + \"\\n\";"
		"}";

	nullguard(c);
	getwkjs(c, script, getpagelinks_cb);
}

void
getpagelinks_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	char *processed = NULL;
	const char *clean[] = {
		"/bin/sh", "sh", "-c", "uniq",
		NULL
	};

	raw = getwkjs_guard(source, res, data);
	processed = cmd(raw, clean);
	savepagefile("links.txt", processed);
	freeandnull(raw);
	freeandnull(processed);
}

void
getpagescripts(Client *c)
{
	char *script = ""
		"var list = document.scripts;"
		"var max = list.length;"
		"var result = \"\";"
		"var i;"
		"for (i = 0; i < max; i++) {"
		"    result = result + list[i].src + \"\\n\";"
		"}";

	nullguard(c);
	getwkjs(c, script, getpagescripts_cb);
}

void
getpagescripts_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	char *processed = NULL;
	const char *clean[] = {
		"/bin/sh", "sh", "-c", "sed /^[[:blank:]]*$/d",
		NULL
	};

	raw = getwkjs_guard(source, res, data);
	processed = cmd(raw, clean);
	savepagefile("scripts.txt", processed);
	freeandnull(raw);
	freeandnull(processed);
}

void
getpagestyles(Client *c)
{
	char *script = ""
		"var list = document.styleSheets;"
		"var max = list.length;"
		"var result = \"\";"
		"var i;"
		"for (i = 0; i < max; i++) {"
		"    if (list[i].href)"
		"        result = result + list[i].href + \"\\n\";"
		"}";

	nullguard(c);
	getwkjs(c, script, getpagestyles_cb);
}

void
getpagestyles_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	char *processed = NULL;
	const char *clean[] = {
		"/bin/sh", "sh", "-c", "sed /^[[:blank:]]*$/d",
		NULL
	};

	raw = getwkjs_guard(source, res, data);
	processed = cmd(raw, clean);
	savepagefile("styles.txt", processed);
	freeandnull(raw);
	freeandnull(processed);
}

void
getpageimages(Client *c)
{
	char *script = ""
		"var list = document.images;"
		"var max = list.length;"
		"var result = \"\";"
		"var i;"
		"for (i = 0; i < max; i++) {"
		"    result = result + list[i].src + \"\\n\";"
		"}";

	nullguard(c);
	getwkjs(c, script, getpageimages_cb);
}

void
getpageimages_cb(GObject *source, GAsyncResult *res, gpointer data)
{
	char *raw = NULL;
	char *processed = NULL;
	const char *clean[] = {
		"/bin/sh", "sh", "-c", "sed /^[[:blank:]]*$/d | uniq",
		NULL
	};

	raw = getwkjs_guard(source, res, data);
	processed = cmd(raw, clean);
	savepagefile("images.txt", processed);
	freeandnull(raw);
	freeandnull(processed);
}

WebKitCookieAcceptPolicy
cookiepolicy_get(void)
{
	switch (((char *)curconfig[CookiePolicies].val.v)[cookiepolicy]) {
	case 'a':
		return WEBKIT_COOKIE_POLICY_ACCEPT_NEVER;
	case '@':
		return WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY;
	default: /* fallthrough */
	case 'A':
		return WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS;
	}
}

char
cookiepolicy_set(const WebKitCookieAcceptPolicy p)
{
	switch (p) {
	case WEBKIT_COOKIE_POLICY_ACCEPT_NEVER:
		return 'a';
	case WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY:
		return '@';
	default: /* fallthrough */
	case WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS:
		return 'A';
	}
}

void
seturiparameters(Client *c, const char *uri, ParamName *params)
{
	Parameter *config, *uriconfig = NULL;
	int p;

	for (int i = 0; i < LENGTH(uriparams); ++i) {
		if (
			uriparams[i].uri &&
			!regexec(&(uriparams[i].re), uri, 0, NULL, 0)
		) {
			uriconfig = uriparams[i].config;
			break;
		}
	}

	curconfig = uriconfig ? uriconfig : defconfig;

	for (int i = 0; (p = params[i]) != ParameterLast; ++i) {
		switch(p) {
		default: /* FALLTHROUGH */
			if (
				!(defconfig[p].prio < curconfig[p].prio) &&
				!(defconfig[p].prio < modparams[p])
			) {
				continue;
			}
		case Certificate:
		case CookiePolicies:
		case Style:
			setparameter(c, 0, p, &curconfig[p].val);
		}
	}
}

void
setparameter(Client *c, int refresh, ParamName p, const Arg *a)
{
	GdkRGBA bgcolor = { 0 };
	WebKitSettings *s = webkit_web_view_get_settings(c->view);
	GString *uri = NULL;
	Arg passthru;

	modparams[p] = curconfig[p].prio;

	switch (p) {
	case AccessMicrophone:
		return; /* do nothing */
	case AccessWebcam:
		return; /* do nothing */
	case CaretBrowsing:
		webkit_settings_set_enable_caret_browsing(s, a->i);
		refresh = 0;
		break;
	case Certificate:
		if (a->i)
			setcert(c);
		return; /* do not update */
	case ContentFilter:
		passthru.i = FilterTogGlobal;
		filtercmd(c, &passthru);
		return;
	case CookiePolicies:
		webkit_cookie_manager_set_accept_policy(
			webkit_web_context_get_cookie_manager(
				webkit_web_view_get_context(c->view)
			),
			cookiepolicy_get());
		refresh = 0;
		break;
	case DiskCache:
		webkit_web_context_set_cache_model(
			webkit_web_view_get_context(c->view),
			a->i
			? WEBKIT_CACHE_MODEL_WEB_BROWSER
			: WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER
			);
		return; /* do not update */
	case DefaultCharset:
		webkit_settings_set_default_charset(s, a->v);
		return; /* do not update */
	case DNSPrefetch:
		webkit_settings_set_enable_dns_prefetching(s, a->i);
		return; /* do not update */
	case FileURLsCrossAccess:
		webkit_settings_set_allow_file_access_from_file_urls(
			s,
			a->i
		);
		webkit_settings_set_allow_universal_access_from_file_urls(
			s,
			a->i
		);
		return; /* do not update */
	case FontSize:
		webkit_settings_set_default_font_size(s, a->i);
		return; /* do not update */
	case FrameFlattening:
		webkit_settings_set_enable_frame_flattening(s, a->i);
		break;
	case Geolocation:
		refresh = 0;
		break;
	case HideBackground:
		if (a->i)
			webkit_web_view_set_background_color(
				c->view,
				&bgcolor
			);
		return; /* do not update */
	case Inspector:
		webkit_settings_set_enable_developer_extras(s, a->i);
		return; /* do not update */
	case Java:
		webkit_settings_set_enable_java(s, a->i);
		return; /* do not update */
	case JavaScript:
		webkit_settings_set_enable_javascript(s, a->i);
		break;
	case KioskMode:
		return; /* do nothing */
	case LoadImages:
		webkit_settings_set_auto_load_images(s, a->i);
		break;
	case MediaManualPlay:
		webkit_settings_set_media_playback_requires_user_gesture(
			s,
			a->i
		);
		break;
	case PreferredLanguages:
		return; /* do nothing */
	case RunInFullscreen:
		return; /* do nothing */
	case ScrollBars:
		/* Disabled until we write some WebKitWebExtension for
		 * manipulating the DOM directly. */
		/*
		enablescrollbars = !enablescrollbars;
		evalscript(
			c,
			"document.documentElement.style.overflow = '%s'",
			enablescrollbars ? "auto" : "hidden"
		);
		 */
		return; /* do not update */
	case ShowIndicators:
		break;
	case SmoothScrolling:
		webkit_settings_set_enable_smooth_scrolling(s, a->i);
		return; /* do not update */
	case SiteQuirks:
		webkit_settings_set_enable_site_specific_quirks(s, a->i);
		break;
	case SpellChecking:
		webkit_web_context_set_spell_checking_enabled(
			webkit_web_view_get_context(c->view),
			a->i
		);
		return; /* do not update */
	case SpellLanguages:
		return; /* do nothing */
	case StrictTLS:
		webkit_website_data_manager_set_tls_errors_policy(
			webkit_web_context_get_website_data_manager(
				webkit_web_view_get_context(c->view)
			),
			a->i
			? WEBKIT_TLS_ERRORS_POLICY_FAIL
			: WEBKIT_TLS_ERRORS_POLICY_IGNORE
		);
		break;
	case Style:
		webkit_user_content_manager_remove_all_style_sheets(
			webkit_web_view_get_user_content_manager(c->view)
		);
		if (a->i) {
			uri = geturi(c);
			setstyle(c, getstyle(uri->str));
			g_string_free(uri, TRUE);
		}
		refresh = 0;
		break;
	case WebGL:
		webkit_settings_set_enable_webgl(s, a->i);
		break;
	case ZoomLevel:
		webkit_web_view_set_zoom_level(c->view, a->f);
		return; /* do not update */
	default:
		return; /* do nothing */
	}

	updatetitle(c);
	if (refresh)
		reload(c, a);
}

const char*
getcert(const char *uri)
{
	for (int i = 0; certdir_loc, i < LENGTH(certs); ++i) {
		if (
			certs[i].regex &&
			!regexec(&(certs[i].re), uri, 0, NULL, 0)
		) {
			return certs[i].file;
		}
	}

	return NULL;
}

void
setcert(Client *c)
{
	GTlsCertificate *cert = NULL;
	GString *uri = geturi(c);
	const char *file = getcert(uri->str);
	const char *https = "https://";

	if (!file) {
		g_string_free(uri, TRUE);
		return;
	}

	if (!(cert = g_tls_certificate_new_from_file(file, NULL))) {
		g_string_free(uri, TRUE);
		g_printerr("For %s: \n", file);
		err("Could not read certificate file.");
	}

	if (g_str_has_prefix(uri->str, https)) {
		char *host_begin;
		char *host_end;
		host_begin = uri->str + strlen(https);
		host_end = strchr(host_begin, '/');
		*host_end = 0;
		webkit_web_context_allow_tls_certificate_for_host(
			webkit_web_view_get_context(c->view),
			cert,
			host_begin
		);
	}

	g_string_free(uri, TRUE);
	g_object_unref(cert);
}

const char*
getstyle(const char *uri)
{
	if (style_loc)
		return style_loc->str;

	for (int i = 0; i < LENGTH(styles); ++i) {
		if (!styles[i].file)
			continue;
		if (
			styles[i].regex &&
			styles[i].file &&
			!regexec(&(styles[i].re), uri, 0, NULL, 0)
		) {
			return styles[i].file;
		}
	}

	return NULL;
}

void
setstyle(Client *c, const char *file)
{
	gchar *style;

	if (NULL == file)
		return;

	if (!g_file_get_contents(file, &style, NULL, NULL)) {
		g_printerr("For %s: \n", file);
		err("Could not read style file.");
	}

	webkit_user_content_manager_add_style_sheet(
		webkit_web_view_get_user_content_manager(c->view),
		webkit_user_style_sheet_new(
			style,
			WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
			WEBKIT_USER_STYLE_LEVEL_USER,
			NULL,
			NULL
		)
	);

	g_free(style);
}

void
runscript(Client *c)
{
	gchar *script;
	gsize l;

	if (!script_loc)
		return;

	if (
		script_loc &&
		g_file_get_contents(script_loc->str, &script, &l, NULL) &&
		l
	) {
		evalscript(c, "%s", script);
	}
	g_free(script);
}

void
evalscript(Client *c, const char *jsstr, ...)
{
	va_list ap;
	gchar *script;

	va_start(ap, jsstr);
	script = g_strdup_vprintf(jsstr, ap);
	va_end(ap);

	webkit_web_view_run_javascript(c->view, script, NULL, NULL, NULL);
	g_free(script);
}

void
updatewinid(Client *c)
{
	snprintf(winid, LENGTH(winid), "%lu", c->xid);
	setenv("BS_WINID", winid, 1);
}

void
handleplumb(Client *c, const char *uri)
{
	Arg a = (Arg)PLUMB(uri);
	spawn(c, &a);
}

void
newwindow(Client *c, const Arg *a, int noembed)
{
	int i = 0;
	char tmp[64];
	const char *cmd[29], *uri;
	const Arg arg = { .v = cmd };

	cmd[i++] = argv0;
	cmd[i++] = "-a";
	cmd[i++] = curconfig[CookiePolicies].val.v;
	cmd[i++] = curconfig[ScrollBars].val.i ? "-B" : "-b";
	if (cookie_loc && g_strcmp0(cookie_loc->str, "")) {
		cmd[i++] = "-c";
		cmd[i++] = cookie_loc->str;
	}
	if (style_loc && g_strcmp0(style_loc->str, "")) {
		cmd[i++] = "-C";
		cmd[i++] = style_loc->str;
	}
	cmd[i++] = curconfig[DiskCache].val.i ? "-D" : "-d";
	if (embed && !noembed) {
		cmd[i++] = "-e";
		snprintf(tmp, LENGTH(tmp), "%lu", embed);
		cmd[i++] = tmp;
	}
	cmd[i++] = curconfig[RunInFullscreen].val.i ? "-F" : "-f" ;
	cmd[i++] = curconfig[Geolocation].val.i ?     "-G" : "-g" ;
	cmd[i++] = curconfig[LoadImages].val.i ?      "-I" : "-i" ;
	cmd[i++] = curconfig[KioskMode].val.i ?       "-K" : "-k" ;
	cmd[i++] = curconfig[Style].val.i ?           "-M" : "-m" ;
	cmd[i++] = curconfig[Inspector].val.i ?       "-N" : "-n" ;
	if (script_loc && g_strcmp0(script_loc->str, "")) {
		cmd[i++] = "-r";
		cmd[i++] = script_loc->str;
	}
	cmd[i++] = curconfig[JavaScript].val.i ? "-S" : "-s";
	cmd[i++] = curconfig[StrictTLS].val.i ? "-T" : "-t";
	if (fulluseragent && g_strcmp0(fulluseragent, "")) {
		cmd[i++] = "-u";
		cmd[i++] = fulluseragent;
	}
	if (showxid)
		cmd[i++] = "-w";
	cmd[i++] = curconfig[Certificate].val.i ? "-X" : "-x" ;
	cmd[i++] = curconfig[ContentFilter].val.i ? "-Y" : "-y" ;
	/* do not keep zoom level */
	cmd[i++] = "--";
	if ((uri = a->v))
		cmd[i++] = uri;
	cmd[i] = NULL;

	spawn(c, &arg);
}

void
logvisit(const GString *uri)
{
	FILE *visited_file = NULL;

	if (NULL == visited_loc)
		return;
	nullguard(uri);

	/* TODO mutex */
	visited_file = fopen(visited_loc->str, "a+");
	if (visited_file) {
		fwrite(uri->str, 1, uri->len, visited_file);
		fwrite("\n", 1, 1, visited_file);
		fclose(visited_file);
	}
}

void
spawn(Client *c, const Arg *a)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		close(spair[0]);
		close(spair[1]);
		setsid();
		execvp(((char **)a->v)[0], (char **)a->v);
		g_printerr("%s: execvp %s\n", argv0, ((char **)a->v)[0]);
		perror(" failed");
		exit(1);
	}
}

void
destroyclient(Client *c)
{
	Client *p;

	webkit_web_view_stop_loading(c->view);
	/* Not needed, has already been called
	   gtk_widget_destroy(c->win);
	   */
	if (c->board_input)
		g_string_free(c->board_input, TRUE);
	if (c->title)
		g_string_free(c->title, TRUE);
	if (c->overtitle)
		g_string_free(c->overtitle, TRUE);

	for (p = clients; p && p->next != c; p = p->next)
		;
	if (p)
		p->next = c->next;
	else
		clients = c->next;
	g_free(c);
}

void
cleanup(void)
{
	while (clients)
		destroyclient(clients);

	if (NULL != pagefiles) {
		char *cmd_pre = "rm -rf ";
		char *command = g_malloc(
			strlen(cmd_pre) + strlen(pagefiles) + 1
		);
		command[0] = 0;
		strcat(command, cmd_pre);
		strcat(command, pagefiles);
		const char *rmrf[] = {
			"/bin/sh", "sh", "-c", command,
			NULL
		};
		cmd(NULL, rmrf);
		g_free(command);
	}

	close(spair[0]);
	close(spair[1]);
	if (cookie_loc)
		g_string_free(cookie_loc, TRUE);
	if (script_loc)
		g_string_free(script_loc, TRUE);
	if (cachedir_loc)
		g_string_free(cachedir_loc, TRUE);
	if (style_loc)
		g_string_free(style_loc, TRUE);
	XCloseDisplay(dpy);
}

WebKitWebView*
newview(Client *c, WebKitWebView *rv)
{
	WebKitWebView *v;

	/* Webview */
	if (rv) {
		v = WEBKIT_WEB_VIEW(webkit_web_view_new_with_related_view(rv));
	} else {
		WebKitWebContext *context;
		WebKitCookieManager *cookiemanager;
		WebKitUserContentManager *contentmanager =
			webkit_user_content_manager_new();

		if (curconfig[Ephemeral].val.i) {
			context = webkit_web_context_new_ephemeral();
		} else {
			context = webkit_web_context_new_with_website_data_manager(
				webkit_website_data_manager_new(
					"base-cache-directory",
					cachedir_loc->str,
					"base-data-directory",
					cachedir_loc->str,
					NULL
				)
			);
		}

		cookiemanager = webkit_web_context_get_cookie_manager(context);
		WebKitSettings *settings = webkit_settings_new_with_settings(
			"allow-file-access-from-file-urls",
				curconfig[FileURLsCrossAccess].val.i,
			"allow-universal-access-from-file-urls",
				curconfig[FileURLsCrossAccess].val.i,
			"auto-load-images",
				curconfig[LoadImages].val.i,
			"default-charset",
				curconfig[DefaultCharset].val.v,
			"default-font-size",
				curconfig[FontSize].val.i,
			"enable-caret-browsing",
				curconfig[CaretBrowsing].val.i,
			"enable-developer-extras",
				curconfig[Inspector].val.i,
			"enable-dns-prefetching",
				curconfig[DNSPrefetch].val.i,
			"enable-frame-flattening",
				curconfig[FrameFlattening].val.i,
			"enable-html5-database",
				curconfig[DiskCache].val.i,
			"enable-html5-local-storage",
				curconfig[DiskCache].val.i,
			"enable-java",
				curconfig[Java].val.i,
			"enable-javascript",
				curconfig[JavaScript].val.i,
			"enable-site-specific-quirks",
				curconfig[SiteQuirks].val.i,
			"enable-smooth-scrolling",
				curconfig[SmoothScrolling].val.i,
			"enable-webgl",
				curconfig[WebGL].val.i,
			"media-playback-requires-user-gesture",
				curconfig[MediaManualPlay].val.i,
			NULL
		);
		/* For more interesting settings, have a look at
		 * http://webkitgtk.org/reference/webkit2gtk/stable/WebKitSettings.html */

		if (strcmp(fulluseragent, "")) {
			webkit_settings_set_user_agent(
				settings,
				fulluseragent
			);
		} else if (bs_useragent) {
			webkit_settings_set_user_agent_with_application_details(
				settings,
				"boredserf",
				VERSION
			);
		}
		useragent = webkit_settings_get_user_agent(settings);

		/* rendering process model, can be a shared unique one
		 * or one for each view */
		webkit_web_context_set_process_model(
			context,
			WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES
		);

		/* TLS */
		webkit_website_data_manager_set_tls_errors_policy(
			webkit_web_context_get_website_data_manager(context),
			curconfig[StrictTLS].val.i
			? WEBKIT_TLS_ERRORS_POLICY_FAIL
			: WEBKIT_TLS_ERRORS_POLICY_IGNORE
		);

		/* disk cache */
		webkit_web_context_set_cache_model(
			context,
			curconfig[DiskCache].val.i
			? WEBKIT_CACHE_MODEL_WEB_BROWSER
			: WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER
		);

		/* Currently only works with text file to be compatible with
		 * curl */
		if (cookie_loc && !curconfig[Ephemeral].val.i) {
			webkit_cookie_manager_set_persistent_storage(
				cookiemanager,
				cookie_loc->str,
				WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT
			);
		}

		/* cookie policy */
		webkit_cookie_manager_set_accept_policy(
			cookiemanager,
			cookiepolicy_get()
		);

		/* languages */
		webkit_web_context_set_preferred_languages(
			context,
			curconfig[PreferredLanguages].val.v
		);
		webkit_web_context_set_spell_checking_languages(
			context,
			curconfig[SpellLanguages].val.v
		);
		webkit_web_context_set_spell_checking_enabled(
			context,
			curconfig[SpellChecking].val.i
		);

		g_signal_connect(
			G_OBJECT(context),
			"download-started",
			G_CALLBACK(downloadstarted),
			c
		);
		g_signal_connect(
			G_OBJECT(context),
			"initialize-web-extensions",
			G_CALLBACK(initwebextensions),
			c
		);

		v = g_object_new(
			WEBKIT_TYPE_WEB_VIEW,
			"settings",
			settings,
			"user-content-manager",
			contentmanager,
			"web-context",
			context,
			NULL
		);
	}

	g_signal_connect(
		G_OBJECT(v),
		"notify::estimated-load-progress",
		G_CALLBACK(progresschanged),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"notify::title",
		G_CALLBACK(titlechanged),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"button-release-event",
		G_CALLBACK(buttonreleased),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"close",
		G_CALLBACK(closeview),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"create",
		G_CALLBACK(createview),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"decide-policy",
		G_CALLBACK(decidepolicy),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"insecure-content-detected",
		G_CALLBACK(insecurecontent),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"load-failed-with-tls-errors",
		G_CALLBACK(loadfailedtls),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"load-changed",
		G_CALLBACK(loadchanged),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"mouse-target-changed",
		G_CALLBACK(mousetargetchanged),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"permission-request",
		G_CALLBACK(permissionrequested),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"ready-to-show",
		G_CALLBACK(showview),
		c
	);
	g_signal_connect(
		G_OBJECT(v),
		"web-process-terminated",
		G_CALLBACK(webprocessterminated),
		c
	);

	return v;
}

gboolean
readsock(GIOChannel *s, GIOCondition ioc, gpointer unused)
{
	static char msg[MSGBUFSZ];
	GError *gerr = NULL;
	gsize msgsz;

	if (
		G_IO_STATUS_NORMAL != g_io_channel_read_chars(
			s,
			msg,
			sizeof(msg),
			&msgsz,
			&gerr
		)
	){
		if (gerr) {
			g_printerr(
				"boredserf: error reading socket: %s\n",
				gerr->message
			);
			g_error_free(gerr);
		}
		return TRUE;
	}

	if (msgsz < 2) {
		g_printerr("boredserf: message too short: %d\n", msgsz);
		return TRUE;
	}

	return TRUE;
}

void
initwebextensions(WebKitWebContext *wc, Client *c)
{
	GVariant *gv;

	if (spair[1] < 0)
		return;

	gv = g_variant_new("i", spair[1]);

	webkit_web_context_set_web_extensions_initialization_user_data(
		wc,
		gv
	);
	webkit_web_context_set_web_extensions_directory(wc, WEBEXTDIR);
}

GtkWidget*
createview(WebKitWebView *v, WebKitNavigationAction *a, Client *c)
{
	switch (webkit_navigation_action_get_navigation_type(a)) {
	case WEBKIT_NAVIGATION_TYPE_OTHER:
		/*
		 * popup windows of type ???other??? are almost always triggered
		 * by user gesture, so inverse the logic here
		 */
		/* instead of this, compare destination uri to mouse-over uri
		 * for validating window */
		if (webkit_navigation_action_is_user_gesture(a))
			break;
		/* else fallthrough */

	case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_RELOAD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED:
		return GTK_WIDGET(newclient(c)->view);

	default:
		break;
	}

	return NULL;
}

gboolean
buttonreleased(GtkWidget *w, GdkEvent *e, Client *c)
{
	WebKitHitTestResultContext element =
		webkit_hit_test_result_get_context(c->mousepos);

	for (int i = 0; i < LENGTH(buttons); ++i) {
		if (
			element & buttons[i].target &&
			e->button.button == buttons[i].button &&
			CLEANMASK(e->button.state) == CLEANMASK(buttons[i].mask) &&
			buttons[i].func
		) {
			buttons[i].func(c, &buttons[i].arg, c->mousepos);
			return buttons[i].stopevent;
		}
	}

	return FALSE;
}

GdkFilterReturn
processx(GdkXEvent *e, GdkEvent *event, gpointer d)
{
	Client *c = (Client *)d;
	XPropertyEvent *ev;
	Arg a;

	if (((XEvent *)e)->type == PropertyNotify) {
		ev = &((XEvent *)e)->xproperty;
		if (ev->state == PropertyNewValue) {
			if (ev->atom == atoms[AtomFind]) {
				find(c, NULL);

				return GDK_FILTER_REMOVE;
			} else if (ev->atom == atoms[AtomGo]) {
				a.v = getatom(c, AtomGo);
				loaduri(c, &a);

				return GDK_FILTER_REMOVE;
			}
		}
	}
	return GDK_FILTER_CONTINUE;
}

gboolean
winevent(GtkWidget *w, GdkEvent *e, Client *c)
{
	Key key;

	switch (e->type) {
	case GDK_ENTER_NOTIFY:
		if (c->targeturi)
			c->overtitle = g_string_assign(
				c->overtitle,
				c->targeturi
			);
		else
			g_string_erase(c->overtitle, 0, -1);
		updatetitle(c);
		break;
	case GDK_KEY_PRESS:
		key.mod = CLEANMASK(e->key.state);
		key.keyval = gdk_keyval_to_lower(e->key.keyval);
		return runkey(key,c);
	case GDK_LEAVE_NOTIFY:
		g_string_erase(c->overtitle, 0, -1);
		updatetitle(c);
		break;
	case GDK_WINDOW_STATE:
		if (
			e->window_state.changed_mask ==
			GDK_WINDOW_STATE_FULLSCREEN
		) {
			c->fullscreen = e->window_state.new_window_state &
				GDK_WINDOW_STATE_FULLSCREEN;
		}
		break;
	default:
		break;
	}

	return FALSE;
}

void
showview(WebKitWebView *ignored, Client *c)
{
	GdkRGBA bgcolor = { 0 };
	GdkWindow *gwin;

	nullguard(c);

	c->finder = webkit_web_view_get_find_controller(c->view);
	c->inspector = webkit_web_view_get_inspector(c->view);

	c->pageid = webkit_web_view_get_page_id(c->view);
	createwindow(c);

	gtk_container_add(GTK_CONTAINER(c->win), GTK_WIDGET(c->view));
	gtk_widget_show_all(c->win);
	gtk_widget_grab_focus(GTK_WIDGET(c->view));

	gwin = gtk_widget_get_window(GTK_WIDGET(c->win));
	c->xid = gdk_x11_window_get_xid(gwin);
	updatewinid(c);
	if (showxid) {
		gdk_display_sync(gtk_widget_get_display(c->win));
		puts(winid);
		fflush(stdout);
	}

	if (curconfig[HideBackground].val.i)
		webkit_web_view_set_background_color(c->view, &bgcolor);

	if (!curconfig[KioskMode].val.i) {
		gdk_window_set_events(gwin, GDK_ALL_EVENTS_MASK);
		gdk_window_add_filter(gwin, processx, c);
	}

	if (curconfig[RunInFullscreen].val.i)
		togglefullscreen(c, NULL);

	if (curconfig[ZoomLevel].val.f != 1.0)
		webkit_web_view_set_zoom_level(c->view,
			curconfig[ZoomLevel].val.f);

	setatom(c, AtomFind, empty_gs);
	setatom(c, AtomUri, blank_gs);
}

void
createwindow(Client *c)
{
	GtkWidget *w;

	if (embed) {
		w = gtk_plug_new(embed);
	} else {
		char *wmstr = g_strdup_printf(
			"%s[%"PRIu64"]",
			"boredserf",
			c->pageid
		);
		w = gtk_window_new(GTK_WINDOW_TOPLEVEL);

		gtk_window_set_role(GTK_WINDOW(w), wmstr);
		g_free(wmstr);

		gtk_window_set_default_size(
			GTK_WINDOW(w),
			winsize[0],
			winsize[1]
		);
	}

	c->win = w;

	g_signal_connect(
		G_OBJECT(w),
		"destroy",
		G_CALLBACK(destroywin),
		c
	);
	g_signal_connect(
		G_OBJECT(w),
		"enter-notify-event",
		G_CALLBACK(winevent),
		c
	);
	g_signal_connect(
		G_OBJECT(w),
		"leave-notify-event",
		G_CALLBACK(winevent),
		c
	);
	g_signal_connect(
		G_OBJECT(w),
		"window-state-event",
		G_CALLBACK(winevent),
		c
	);

	replacekeytree(c, winevent);
}

gboolean
loadfailedtls(
	WebKitWebView *v,
	gchar *uri,
	GTlsCertificate *cert,
	GTlsCertificateFlags err,
	Client *c
)
{
	GString *errmsg = g_string_new(NULL);
	gchar *html, *pem;

	c->failedcert = g_object_ref(cert);
	c->tlserr = err;
	c->errorpage = 1;

	if (err & G_TLS_CERTIFICATE_UNKNOWN_CA)
		g_string_append(
			errmsg,
			"The signing certificate authority is not known."
			"<br>"
		);
	if (err & G_TLS_CERTIFICATE_BAD_IDENTITY)
		g_string_append(
			errmsg,
			"The certificate does not match the expected "
			"identity of the site that it was retrieved from."
			"<br>"
		);
	if (err & G_TLS_CERTIFICATE_NOT_ACTIVATED)
		g_string_append(
			errmsg,
			"The certificate's activation time is still in the "
			"future."
			"<br>"
		);
	if (err & G_TLS_CERTIFICATE_EXPIRED)
		g_string_append(
			errmsg,
			"The certificate has expired."
			"<br>"
		);
	if (err & G_TLS_CERTIFICATE_REVOKED)
		g_string_append(
			errmsg,
			"The certificate has been revoked according to the "
			"GTlsConnection's certificate revocation list."
			"<br>"
		);
	if (err & G_TLS_CERTIFICATE_INSECURE)
		g_string_append(
			errmsg,
			"The certificate's algorithm is considered insecure."
			"<br>"
		);
	if (err & G_TLS_CERTIFICATE_GENERIC_ERROR)
		g_string_append(
			errmsg,
			"Some error occurred validating the certificate."
			"<br>"
		);

	g_object_get(cert, "certificate-pem", &pem, NULL);
	html = g_strdup_printf(
		"<p>Could not validate TLS for ???%s???<br>%s</p><p>You can "
		"inspect the following certificate with Ctrl-t (default "
		"keybinding).</p><p><pre>%s</pre></p>",
		uri,
		errmsg->str,
		pem
	);
	g_free(pem);
	g_string_free(errmsg, TRUE);

	webkit_web_view_load_alternate_html(c->view, html, uri, NULL);
	g_free(html);

	return TRUE;
}

void
loadchanged(WebKitWebView *v, WebKitLoadEvent e, Client *c)
{
	const GString *uri = geturi(c);

	switch (e) {
	case WEBKIT_LOAD_STARTED:
		setatom(c, AtomUri, uri);
		g_string_assign(c->title, uri->str);
		c->https = c->insecure = 0;
		seturiparameters(c, uri->str, loadtransient);
		if (c->errorpage)
			c->errorpage = 0;
		else
			g_clear_object(&c->failedcert);
		break;
	case WEBKIT_LOAD_REDIRECTED:
		setatom(c, AtomUri, uri);
		g_string_assign(c->title, uri->str);
		seturiparameters(c, uri->str, loadtransient);
		break;
	case WEBKIT_LOAD_COMMITTED:
		setatom(c, AtomUri, uri);
		g_string_assign(c->title, uri->str);
		seturiparameters(c, uri->str, loadcommitted);
		c->https = webkit_web_view_get_tls_info(
			c->view,
			&c->cert,
			&c->tlserr
		);
		break;
	case WEBKIT_LOAD_FINISHED:
		seturiparameters(c, uri->str, loadfinished);
		/* Disabled until we write some WebKitWebExtension for
		 * manipulating the DOM directly. */
		/*
		evalscript(
			c,
			"document.documentElement.style.overflow = '%s'",
			enablescrollbars ? "auto" : "hidden"
		);
		 */
		runscript(c);
		updateenv(c);
		break;
	}
	updatetitle(c);
}

void
progresschanged(WebKitWebView *v, GParamSpec *ps, Client *c)
{
	c->progress =
		webkit_web_view_get_estimated_load_progress(c->view) * 100;
	updatetitle(c);
}

void
titlechanged(WebKitWebView *view, GParamSpec *ps, Client *c)
{
	g_string_assign(c->title, webkit_web_view_get_title(c->view));
	updatetitle(c);
}

void
mousetargetchanged(WebKitWebView *v, WebKitHitTestResult *h, guint modifiers,
	Client *c)
{
	WebKitHitTestResultContext hc = webkit_hit_test_result_get_context(h);

	/* Keep the hit test to know where is the pointer on the next click */
	c->mousepos = h;

	if (hc & OnLink)
		c->targeturi = webkit_hit_test_result_get_link_uri(h);
	else if (hc & OnImg)
		c->targeturi = webkit_hit_test_result_get_image_uri(h);
	else if (hc & OnMedia)
		c->targeturi = webkit_hit_test_result_get_media_uri(h);
	else
		c->targeturi = NULL;

	if (c->targeturi)
		g_string_assign(c->overtitle, c->targeturi);
	else
		g_string_erase(c->overtitle, 0, -1);
	updatetitle(c);
}

gboolean
permissionrequested(WebKitWebView *v, WebKitPermissionRequest *r, Client *c)
{
	ParamName param = ParameterLast;

	if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(r)) {
		param = Geolocation;
	} else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(r)) {
		if (
			webkit_user_media_permission_is_for_audio_device(
				WEBKIT_USER_MEDIA_PERMISSION_REQUEST(r))
		) {
			param = AccessMicrophone;
		} else if (
			webkit_user_media_permission_is_for_video_device(
				WEBKIT_USER_MEDIA_PERMISSION_REQUEST(r))
		) {
			param = AccessWebcam;
		}
	} else {
		return FALSE;
	}

	if (curconfig[param].val.i)
		webkit_permission_request_allow(r);
	else
		webkit_permission_request_deny(r);

	return TRUE;
}

gboolean
decidepolicy(WebKitWebView *v, WebKitPolicyDecision *d,
	WebKitPolicyDecisionType dt, Client *c)
{
	switch (dt) {
	case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
		decidenavigation(d, c);
		break;
	case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
		decidenewwindow(d, c);
		break;
	case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
		decideresource(d, c);
		break;
	default:
		webkit_policy_decision_ignore(d);
		break;
	}
	return TRUE;
}

void
decidenavigation(WebKitPolicyDecision *d, Client *c)
{
	WebKitNavigationAction *a =
		webkit_navigation_policy_decision_get_navigation_action(
			WEBKIT_NAVIGATION_POLICY_DECISION(d));

	switch (webkit_navigation_action_get_navigation_type(a)) {
	case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_RELOAD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_OTHER: /* fallthrough */
	default:
		/* Do not navigate to links with a "_blank" target (popup) */
		if (
			webkit_navigation_policy_decision_get_frame_name(
				WEBKIT_NAVIGATION_POLICY_DECISION(d))
		) {
			webkit_policy_decision_ignore(d);
		} else {
			/* Filter out navigation to different domain ? */
			/* get action???urirequest, copy and load in new
			 * window+view on Ctrl+Click ? */
			webkit_policy_decision_use(d);
		}
		break;
	}
}

void
decidenewwindow(WebKitPolicyDecision *d, Client *c)
{
	Arg arg;
	WebKitNavigationAction *a =
		webkit_navigation_policy_decision_get_navigation_action(
			WEBKIT_NAVIGATION_POLICY_DECISION(d)
		);

	switch (webkit_navigation_action_get_navigation_type(a)) {
	case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_RELOAD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED:
		/* Filter domains here */
		/* If the value of ???mouse-button??? is not 0, then the navigation
		 * was triggered by a mouse event.
		 * test for link clicked but no button ? */
		arg.v = webkit_uri_request_get_uri(
			webkit_navigation_action_get_request(a)
		);
		newwindow(c, &arg, 0);
		break;
	case WEBKIT_NAVIGATION_TYPE_OTHER: /* fallthrough */
	default:
		break;
	}

	webkit_policy_decision_ignore(d);
}

void
decideresource(WebKitPolicyDecision *d, Client *c)
{
	WebKitResponsePolicyDecision *r = WEBKIT_RESPONSE_POLICY_DECISION(d);
	WebKitURIResponse *res =
		webkit_response_policy_decision_get_response(r);
	const GString *visited =
		g_string_new(webkit_uri_response_get_uri(res));

	if (g_str_has_suffix(visited->str, "/favicon.ico")) {
		webkit_policy_decision_ignore(d);
		return;
	}

	if (
		!g_str_has_prefix(visited->str, "https://") &&
		!g_str_has_prefix(visited->str, "http://") &&
		!g_str_has_prefix(visited->str, "file://") &&
		!g_str_has_prefix(visited->str, "about:") &&
		!g_str_has_prefix(visited->str, "data:") &&
		!g_str_has_prefix(visited->str, "blob:") &&
		0 < visited->len
	) {
		for (int i = 0; i < visited->len; i++) {
			if (!g_ascii_isprint(visited->str[i])) {
				handleplumb(c, visited->str);
				webkit_policy_decision_ignore(d);
				return;
			}
		}
	}

	if (webkit_response_policy_decision_is_mime_type_supported(r)) {
		logvisit(visited);
		webkit_policy_decision_use(d);
	} else {
		webkit_policy_decision_ignore(d);
		download(c, res);
	}
}

void
insecurecontent(WebKitWebView *v, WebKitInsecureContentEvent e, Client *c)
{
	c->insecure = 1;
}

void
downloadstarted(WebKitWebContext *wc, WebKitDownload *d, Client *c)
{
	g_signal_connect(
		G_OBJECT(d),
		"notify::response",
		G_CALLBACK(responsereceived),
		c
	);
}

void
responsereceived(WebKitDownload *d, GParamSpec *ps, Client *c)
{
	download(c, webkit_download_get_response(d));
	webkit_download_cancel(d);
}

void
download(Client *c, WebKitURIResponse *r)
{
	GString *uri = geturi(c);
	Arg a = (Arg)DOWNLOAD(webkit_uri_response_get_uri(r), uri->str);
	spawn(c, &a);
	g_string_free(uri, TRUE);
}

void
webprocessterminated(
	WebKitWebView *v,
	WebKitWebProcessTerminationReason r,
	Client *c
)
{
	g_printerr("web process terminated: %s\n",
		r == WEBKIT_WEB_PROCESS_CRASHED
		? "crashed"
		: "no memory"
	);
	closeview(v, c);
}

void
closeview(WebKitWebView *v, Client *c)
{
	gtk_widget_destroy(c->win);
}

void
destroywin(GtkWidget* w, Client *c)
{
	destroyclient(c);
	if (!clients)
		gtk_main_quit();
}

gboolean
runkey(Key key, Client *c)
{
	/* ignore standard modifiers as literal key presses */
	if (GDK_KEY_Shift_L <= key.keyval && GDK_KEY_Hyper_R >= key.keyval)
		return FALSE;
	if (curconfig[KioskMode].val.i)
		return FALSE;
	if (curkeytree == NULL)
		resetkeytree(c);
	for (int i = 0; curkeytree[i].func; ++i) {
		if (
			key.keyval != curkeytree[i].keyval ||
			key.mod != curkeytree[i].mod
		) {
			continue;
		}
		updatewinid(c);
		curkeytree[i].func(c, &(curkeytree[i].arg));
		return TRUE;
	}
	if (key.keyval && curkeytree != defkeytree) {
		resetkeytree(c);
		return runkey(key, c);
	}
	return FALSE;
}

void
resetkeytree(Client *c)
{
	/* keytree cleanup */
	if (curkeytree == filterkeys) {
		g_string_erase(c->overtitle, 0, -1);
		updatetitle(c);
	}

	curkeytree = defkeytree;
}

void
setkeytree(Client *c, const Arg *a)
{
	const Arg filterkeysarg = { .i = FilterDispRule };

	if (a->v && curkeytree != (Key*)a->v) {
		resetkeytree(c);
		curkeytree = (Key*)a->v;
	}

	/* keytree initialization */
	if (curkeytree == filterkeys)
		filtercmd(c, &filterkeysarg);
}

void
replacekeytree(Client *c, void *cb)
{
	enum { defsz = 8 };
	static gulong *ids = NULL;
	static int idsz = 0;
	static int idpos = 0;

	if (NULL == ids || idpos >= idsz) {
		if (defsz > idsz)
			idsz = defsz;
		else
			idsz *= 2;
		ids = realloc(ids, idsz * sizeof(gulong));
		if (!idpos)
			ids[0] = 0;
		for (int i = idpos + 1; i < idsz; ++i)
			ids[i] = 0;
	}

	if (NULL == cb) {
		if (0 < idpos) {
			g_signal_handler_disconnect(
				G_OBJECT(c->win),
				c->keyhandler
			);
			c->keyhandler = ids[--idpos];
			g_signal_handler_unblock(
				G_OBJECT(c->win),
				c->keyhandler
			);
		}
		return;
	}

	if (ids[idpos])
		g_signal_handler_block(G_OBJECT(c->win), ids[idpos]);
	ids[++idpos] = g_signal_connect(
		G_OBJECT(c->win),
		"key-press-event",
		G_CALLBACK(cb),
		c
	);
	c->keyhandler = ids[idpos];
}

/* returns malloc'd string */
/*
gchar*
parseuri(const gchar *uri) {
	for (guint i = 0; i < LENGTH(searchengines); i++) {
		if (
			searchengines[i].token == NULL ||
			searchengines[i].uri == NULL ||
			*(uri + strlen(searchengines[i].token)) != ' '
		) {
			continue;
		}
		if (g_str_has_prefix(uri, searchengines[i].token)) {
			return g_strdup_printf(
				searchengines[i].uri,
				uri + strlen(searchengines[i].token) + 1
			);
		}
	}

	return g_strdup_printf("http://%s", uri);
}
*/

void
pasteuri(GtkClipboard *clipboard, const char *text, gpointer d)
{
	Arg a = {.v = text };
	if (text)
		loaduri((Client *) d, &a);
}

void
i_seturi(Client *c, const Arg *a)
{
	char *result_c;
	int len;

	nullguard(c);
	updatewinid(c);

	result_c = cmd(NULL, selector_go);
	if (NULL != result_c) {
		GString *result_gs = g_string_new(result_c);
		setatom(c, AtomGo, result_gs);
		g_free(result_c);
		g_string_free(result_gs, TRUE);
	}
}

void
i_find(Client *c, const Arg *a)
{
	char *result = NULL;

	nullguard(c);
	updatewinid(c);

	if ((result = cmd(NULL, selector_find))) {
		GString *result_gs = g_string_new(result);
		setatom(c, AtomFind, result_gs);
		freeandnull(result);
		g_string_free(result_gs, TRUE);
	}
}

void
reload(Client *c, const Arg *a)
{
	if (a->i)
		webkit_web_view_reload_bypass_cache(c->view);
	else
		webkit_web_view_reload(c->view);
}

void
print(Client *c, const Arg *a)
{
	webkit_print_operation_run_dialog(
		webkit_print_operation_new(c->view),
		GTK_WINDOW(c->win)
	);
}

void
showcert(Client *c, const Arg *a)
{
	GTlsCertificate *cert = c->failedcert ? c->failedcert : c->cert;
	GcrCertificate *gcrt;
	GByteArray *crt;
	GtkWidget *win;
	GcrCertificateWidget *wcert;

	if (!cert)
		return;

	g_object_get(cert, "certificate", &crt, NULL);
	gcrt = gcr_simple_certificate_new(crt->data, crt->len);
	g_byte_array_unref(crt);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	wcert = gcr_certificate_widget_new(gcrt);
	g_object_unref(gcrt);

	gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(wcert));
	gtk_widget_show_all(win);
}

void
clipboard(Client *c, const Arg *a)
{
	GString *uri = geturi(c);
	if (a->i) { /* load clipboard uri */
		gtk_clipboard_request_text(
			gtk_clipboard_get(
				GDK_SELECTION_PRIMARY
			),
			pasteuri,
			c
		);
	} else { /* copy uri */
		gtk_clipboard_set_text(
			gtk_clipboard_get(
				GDK_SELECTION_PRIMARY
			),
			c->targeturi
			? c->targeturi
			: uri->str,
			-1
		);
	}
	g_string_free(uri, TRUE);
}

void
zoom(Client *c, const Arg *a)
{
	if (a->i > 0)
		webkit_web_view_set_zoom_level(
			c->view,
			curconfig[ZoomLevel].val.f + 0.1
		);
	else if (a->i < 0)
		webkit_web_view_set_zoom_level(
			c->view,
			curconfig[ZoomLevel].val.f - 0.1
		);
	else
		webkit_web_view_set_zoom_level(
			c->view,
			1.0
		);

	curconfig[ZoomLevel].val.f = webkit_web_view_get_zoom_level(c->view);
}

void
msgext(Client *c, char type, const Arg *a)
{
	static char msg[MSGBUFSZ];
	int ret;

	if (spair[0] < 0)
		return;

	if (
		(ret = snprintf(
				msg,
				sizeof(msg),
				"%c%c%c",
				c->pageid,
				type,
				a->i
		)) &&
		sizeof(msg) <= ret
	) {
		g_printerr("boredserf: message too long: %d\n", ret);
		return;
	}

	if (send(spair[0], msg, ret, 0) != ret) {
		g_printerr(
			"boredserf: error sending: %u%c%d (%d)\n",
			c->pageid,
			type,
			a->i,
			ret
		);
	}
}

void
scrollv(Client *c, const Arg *a)
{
	msgext(c, 'v', a);
}

void
scrollh(Client *c, const Arg *a)
{
	msgext(c, 'h', a);
}

void
navigate(Client *c, const Arg *a)
{
	if (a->i < 0)
		webkit_web_view_go_back(c->view);
	else if (a->i > 0)
		webkit_web_view_go_forward(c->view);
}

void
stop(Client *c, const Arg *a)
{
	webkit_web_view_stop_loading(c->view);
}

void
toggle(Client *c, const Arg *a)
{
	curconfig[a->i].val.i ^= 1;
	setparameter(c, 1, (ParamName)a->i, &curconfig[a->i].val);
}

void
togglefullscreen(Client *c, const Arg *a)
{
	/* toggling value is handled in winevent() */
	if (c->fullscreen)
		gtk_window_unfullscreen(GTK_WINDOW(c->win));
	else
		gtk_window_fullscreen(GTK_WINDOW(c->win));
}

void
togglecookiepolicy(Client *c, const Arg *a)
{
	++cookiepolicy;
	cookiepolicy %= strlen(curconfig[CookiePolicies].val.v);

	setparameter(c, 0, CookiePolicies, NULL);
}

void
toggleinspector(Client *c, const Arg *a)
{
	if (webkit_web_inspector_is_attached(c->inspector))
		webkit_web_inspector_close(c->inspector);
	else if (curconfig[Inspector].val.i)
		webkit_web_inspector_show(c->inspector);
}

void
find(Client *c, const Arg *a)
{
	if (a && a->i) {
		if (a->i > 0)
			webkit_find_controller_search_next(c->finder);
		else
			webkit_find_controller_search_previous(c->finder);
	} else {
		const char *s = getatom(c, AtomFind);
		const char *f =
			webkit_find_controller_get_search_text(c->finder);

		if (g_strcmp0(f, s) == 0) { /* reset search */
			webkit_find_controller_search(
				c->finder,
				"",
				findopts,
				G_MAXUINT
			);
		}

		webkit_find_controller_search(
			c->finder,
			s,
			findopts,
			G_MAXUINT
		);

		if (strcmp(s, "") == 0)
			webkit_find_controller_search_finish(c->finder);
	}
}

void
clicknavigate(Client *c, const Arg *a, WebKitHitTestResult *h)
{
	navigate(c, a);
}

void
clicknewwindow(Client *c, const Arg *a, WebKitHitTestResult *h)
{
	Arg arg = { .v = webkit_hit_test_result_get_link_uri(h) };
	newwindow(c, &arg, a->i);
}

void
clickexternplayer(Client *c, const Arg *a, WebKitHitTestResult *h)
{
	Arg arg = (Arg)VIDEOPLAY(webkit_hit_test_result_get_media_uri(h));
	spawn(c, &arg);
}

int
main(int argc, char *argv[])
{
	Arg arg;
	Client *c;

	memset(&arg, 0, sizeof(arg));

	/* command line args */
	ARGBEGIN {
	case 'a':
		defconfig[CookiePolicies].val.v = EARGF(usage());
		defconfig[CookiePolicies].prio = 2;
		break;
	case 'b':
		defconfig[ScrollBars].val.i = 0;
		defconfig[ScrollBars].prio = 2;
		break;
	case 'B':
		defconfig[ScrollBars].val.i = 1;
		defconfig[ScrollBars].prio = 2;
		break;
	case 'c':
		cookiefile = EARGF(usage());
		break;
	case 'C':
		stylefile = EARGF(usage());
		break;
	case 'd':
		defconfig[DiskCache].val.i = 0;
		defconfig[DiskCache].prio = 2;
		break;
	case 'D':
		defconfig[DiskCache].val.i = 1;
		defconfig[DiskCache].prio = 2;
		break;
	case 'e':
		embed = strtol(EARGF(usage()), NULL, 0);
		break;
	case 'f':
		defconfig[RunInFullscreen].val.i = 0;
		defconfig[RunInFullscreen].prio = 2;
		break;
	case 'F':
		defconfig[RunInFullscreen].val.i = 1;
		defconfig[RunInFullscreen].prio = 2;
		break;
	case 'g':
		defconfig[Geolocation].val.i = 0;
		defconfig[Geolocation].prio = 2;
		break;
	case 'G':
		defconfig[Geolocation].val.i = 1;
		defconfig[Geolocation].prio = 2;
		break;
	case 'i':
		defconfig[LoadImages].val.i = 0;
		defconfig[LoadImages].prio = 2;
		break;
	case 'I':
		defconfig[LoadImages].val.i = 1;
		defconfig[LoadImages].prio = 2;
		break;
	case 'k':
		defconfig[KioskMode].val.i = 0;
		defconfig[KioskMode].prio = 2;
		break;
	case 'K':
		defconfig[KioskMode].val.i = 1;
		defconfig[KioskMode].prio = 2;
		break;
	case 'm':
		defconfig[Style].val.i = 0;
		defconfig[Style].prio = 2;
		break;
	case 'M':
		defconfig[Style].val.i = 1;
		defconfig[Style].prio = 2;
		break;
	case 'n':
		defconfig[Inspector].val.i = 0;
		defconfig[Inspector].prio = 2;
		break;
	case 'N':
		defconfig[Inspector].val.i = 1;
		defconfig[Inspector].prio = 2;
		break;
	case 'r':
		scriptfile = EARGF(usage());
		break;
	case 's':
		defconfig[JavaScript].val.i = 0;
		defconfig[JavaScript].prio = 2;
		break;
	case 'S':
		defconfig[JavaScript].val.i = 1;
		defconfig[JavaScript].prio = 2;
		break;
	case 't':
		defconfig[StrictTLS].val.i = 0;
		defconfig[StrictTLS].prio = 2;
		break;
	case 'T':
		defconfig[StrictTLS].val.i = 1;
		defconfig[StrictTLS].prio = 2;
		break;
	case 'u':
		fulluseragent = EARGF(usage());
		break;
	case 'v':
		die("boredserf-"VERSION", see LICENSE for ?? details\n");
		break;
	case 'w':
		showxid = 1;
		break;
	case 'x':
		defconfig[Certificate].val.i = 0;
		defconfig[Certificate].prio = 2;
		break;
	case 'X':
		defconfig[Certificate].val.i = 1;
		defconfig[Certificate].prio = 2;
		break;
	case 'y':
		defconfig[ContentFilter].val.i = 0;
		defconfig[ContentFilter].prio = 2;
		break;
	case 'Y':
		defconfig[ContentFilter].val.i = 1;
		defconfig[ContentFilter].prio = 2;
		break;
	case 'z':
		defconfig[ZoomLevel].val.f = strtof(EARGF(usage()), NULL);
		defconfig[ZoomLevel].prio = 2;
		break;
	default:
		usage();
	} ARGEND;
	if (argc > 0)
		arg.v = argv[0];
	else
		arg.v = "about:blank";

	setup();
	atexit(cleanup);

	c = newclient(NULL);
	filter_read();
	filter_apply(c);
	showview(NULL, c);

	loaduri(c, &arg);
	updatetitle(c);

	gtk_main();

	return 0;
}
