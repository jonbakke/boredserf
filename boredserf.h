#ifndef BOREDSERF_H
#define BOREDSERF_H

#include <regex.h>
#include <gdk/gdkx.h>
#include <webkit2/webkit2.h>

enum { AtomFind, AtomGo, AtomUri, AtomUTF8, AtomLast };

enum {
	OnDoc   = WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT,
	OnLink  = WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK,
	OnImg   = WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE,
	OnMedia = WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA,
	OnEdit  = WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE,
	OnBar   = WEBKIT_HIT_TEST_RESULT_CONTEXT_SCROLLBAR,
	OnSel   = WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION,
	OnAny   = OnDoc | OnLink | OnImg | OnMedia | OnEdit | OnBar | OnSel,
};

typedef enum {
	AccessMicrophone,
	AccessWebcam,
	CaretBrowsing,
	Certificate,
	ContentFilter,
	CookiePolicies,
	DiskCache,
	DefaultCharset,
	DNSPrefetch,
	Ephemeral,
	FileURLsCrossAccess,
	FontSize,
	FrameFlattening,
	Geolocation,
	HideBackground,
	Inspector,
	Java,
	JavaScript,
	KioskMode,
	LoadImages,
	MediaManualPlay,
	PreferredLanguages,
	RunInFullscreen,
	ScrollBars,
	ShowIndicators,
	SiteQuirks,
	SmoothScrolling,
	SpellChecking,
	SpellLanguages,
	StrictTLS,
	Style,
	WebGL,
	ZoomLevel,
	ParameterLast
} ParamName;

typedef union {
	int i;
	float f;
	const void *v;
} Arg;

typedef struct {
	Arg val;
	int prio;
} Parameter;

typedef struct Client {
	GtkWidget *win;
	WebKitWebView *view;
	WebKitWebInspector *inspector;
	WebKitFindController *finder;
	WebKitHitTestResult *mousepos;
	GTlsCertificate *cert, *failedcert;
	GTlsCertificateFlags tlserr;
	Window xid;
	guint64 pageid;
	gulong keyhandler;
	int progress, fullscreen, https, insecure, errorpage;
	GString *title, *overtitle;
	const char *targeturi;
	const char *needle;
	int board_flags;
	GString *board_input;
	void (*board_cb)(struct Client *c);
	struct Client *next;
} Client;

typedef struct {
	guint mod;
	guint keyval;
	void (*func)(Client *c, const Arg *a);
	const Arg arg;
} Key;

typedef struct {
	unsigned int target;
	unsigned int mask;
	guint button;
	void (*func)(Client *c, const Arg *a, WebKitHitTestResult *h);
	const Arg arg;
	unsigned int stopevent;
} Button;

typedef struct {
	const char *uri;
	Parameter config[ParameterLast];
	regex_t re;
} UriParameters;

typedef struct {
	char *regex;
	char *file;
	regex_t re;
} SiteSpecific;

typedef void(*WebKit_JavaScript_cb)(GObject *s, GAsyncResult *r, gpointer d);

/* boredserf */
void usage(void);
void setup(void);
void sighup(int unused);
/* GString* in & out functions may modify input to produce output */
GString* buildfile(const char* input);
GString* buildpath(const char* input);
Client *newclient(Client *c);
void loaduri(Client *c, const Arg *a);
void updateenv(Client *c);
GString* geturi(Client *c);
void setatom(Client *c, int a, const GString *v);
const char *getatom(Client *c, int a);
void updatetitle(Client *c);
void gettogglestats(Client *c);
void getpagestats(Client *c);
void getwkjs(Client *c, char *script, WebKit_JavaScript_cb cb);
char* getwkjs_guard(GObject *source, GAsyncResult *res, gpointer data);
void savepagefile(const char *name, const char *contents);
void getpagetext(Client *c);
void getpagetext_cb(GObject *source, GAsyncResult *res, gpointer data);
void getpagehead(Client *c);
void getpagehead_cb(GObject *source, GAsyncResult *res, gpointer data);
void getpagehead_stripped(Client *c);
void getpagehead_stripped_cb(GObject *src, GAsyncResult *res, gpointer data);
void getpagebody(Client *c);
void getpagebody_cb(GObject *source, GAsyncResult *res, gpointer data);
void getpagebody_stripped(Client *c);
void getpagebody_stripped_cb(GObject *src, GAsyncResult *res, gpointer data);
void getpagetitle(Client *c);
void getpagetitle_cb(GObject *source, GAsyncResult *res, gpointer data);
void getpagelinks(Client *c);
void getpagelinks_cb(GObject *source, GAsyncResult *res, gpointer data);
void getpagescripts(Client *c);
void getpagescripts_cb(GObject *source, GAsyncResult *res, gpointer data);
void getpagestyles(Client *c);
void getpagestyles_cb(GObject *source, GAsyncResult *res, gpointer data);
void getpageimages(Client *c);
void getpageimages_cb(GObject *source, GAsyncResult *res, gpointer data);
WebKitCookieAcceptPolicy cookiepolicy_get(void);
char cookiepolicy_set(const WebKitCookieAcceptPolicy p);
void seturiparameters(Client *c, const char *uri, ParamName *params);
void setparameter(Client *c, int refresh, ParamName p, const Arg *a);
const char *getcert(const char *uri);
void setcert(Client *c);
const char *getstyle(const char *uri);
void setstyle(Client *c, const char *file);
void runscript(Client *c);
void evalscript(Client *c, const char *jsstr, ...);
void updatewinid(Client *c);
void handleplumb(Client *c, const char *uri);
void newwindow(Client *c, const Arg *a, int noembed);
void logvisit(const GString *uri);
void spawn(Client *c, const Arg *a);
void msgext(Client *c, char type, const Arg *a);
void destroyclient(Client *c);
void cleanup(void);

/* GTK/WebKit */
WebKitWebView *newview(Client *c, WebKitWebView *rv);
void initwebextensions(WebKitWebContext *wc, Client *c);
GtkWidget *createview(WebKitWebView *v, WebKitNavigationAction *a,
                      Client *c);
gboolean buttonreleased(GtkWidget *w, GdkEvent *e, Client *c);
GdkFilterReturn processx(GdkXEvent *xevent, GdkEvent *event,
                         gpointer d);
gboolean winevent(GtkWidget *w, GdkEvent *e, Client *c);
gboolean readsock(GIOChannel *s, GIOCondition ioc, gpointer unused);
void showview(WebKitWebView *ignored, Client *c);
void createwindow(Client *c);
gboolean loadfailedtls(WebKitWebView *v, gchar *uri,
                       GTlsCertificate *cert,
                       GTlsCertificateFlags err, Client *c);
void loadchanged(WebKitWebView *v, WebKitLoadEvent e, Client *c);
void progresschanged(WebKitWebView *v, GParamSpec *ps, Client *c);
void titlechanged(WebKitWebView *view, GParamSpec *ps, Client *c);
void mousetargetchanged(WebKitWebView *v, WebKitHitTestResult *h,
                        guint modifiers, Client *c);
gboolean permissionrequested(WebKitWebView *v,
                             WebKitPermissionRequest *r, Client *c);
gboolean decidepolicy(WebKitWebView *v, WebKitPolicyDecision *d,
                      WebKitPolicyDecisionType dt, Client *c);
void decidenavigation(WebKitPolicyDecision *d, Client *c);
void decidenewwindow(WebKitPolicyDecision *d, Client *c);
void decideresource(WebKitPolicyDecision *d, Client *c);
void insecurecontent(WebKitWebView *v, WebKitInsecureContentEvent e,
                     Client *c);
void downloadstarted(WebKitWebContext *wc, WebKitDownload *d,
                     Client *c);
void responsereceived(WebKitDownload *d, GParamSpec *ps, Client *c);
void download(Client *c, WebKitURIResponse *r);
void webprocessterminated(WebKitWebView *v,
                          WebKitWebProcessTerminationReason r,
                          Client *c);
void closeview(WebKitWebView *v, Client *c);
void destroywin(GtkWidget* w, Client *c);
char* testmarks(const char *uri);
gchar* parseuri(const gchar *uri);

/* Hotkeys */
/* interactive */
void i_seturi(Client *c, const Arg *a);
void i_find(Client *c, const Arg *a);
/* one-shot */
void pasteuri(GtkClipboard *clipboard, const char *text, gpointer d);
void reload(Client *c, const Arg *a);
void print(Client *c, const Arg *a);
void showcert(Client *c, const Arg *a);
void clipboard(Client *c, const Arg *a);
void zoom(Client *c, const Arg *a);
void scrollv(Client *c, const Arg *a);
void scrollh(Client *c, const Arg *a);
void navigate(Client *c, const Arg *a);
void stop(Client *c, const Arg *a);
void togglefullscreen(Client *c, const Arg *a);
void togglecookiepolicy(Client *c, const Arg *a);
void toggleinspector(Client *c, const Arg *a);
/* utility */
gboolean runkey(Key key, Client *c);
void resetkeytree(Client *c);
void setkeytree(Client *c, const Arg *a);
void replacekeytree(Client *c, void *cb);
void find(Client *c, const Arg *a);
void toggle(Client *c, const Arg *a);

/* Buttons */
void clicknavigate(Client *c, const Arg *a, WebKitHitTestResult *h);
void clicknewwindow(Client *c, const Arg *a, WebKitHitTestResult *h);
void clickexternplayer(Client *c, const Arg *a, WebKitHitTestResult *h);

extern char winid[64];
extern char togglestats[11];
extern char pagestats[2];
extern char *pagetext;
extern Atom atoms[AtomLast];
extern Window embed;
extern int showxid;
extern int cookiepolicy;
extern Display *dpy;
extern Client *clients;
extern GdkDevice *gdkkb;
extern Key *curkeytree;
extern char *stylefile;
extern const char *useragent;
extern char *pagefiles;
extern Parameter *curconfig;
extern int modparams[ParameterLast];
extern int spair[2];
extern char *argv0;
extern GString *cookie_loc;
extern GString *script_loc;
extern GString *visited_loc;
extern GString *marks_loc;
extern GString *filterrule_loc;
extern GString *filterdir_loc;
extern GString *certdir_loc;
extern GString *cachedir_loc;
extern GString *style_loc;
extern const GString *empty_gs;
extern const GString *blank_gs;

extern ParamName loadtransient[];
extern ParamName loadcommitted[];
extern ParamName loadfinished[];

#endif /* BOREDSERF_H */
