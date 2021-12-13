/* This file is part of boredserf. */

#ifndef FILTER_H
#define FILTER_H

typedef enum {
	FilterDocs = 0,
	FilterCSS,
	FilterFonts,
	FilterImages,
	FilterSVG,
	FilterMedia,
	FilterScripts,
	FilterRaw,
	FilterPopup,
	FilterResourceTypes,
	FilterSel1Party,
	FilterSel3Party,
	FilterDispRule,
	FilterTglDomDisp,
	FilterApply,
	FilterWrite,
	FilterResetRule,
	FilterTogGlobal,
} FilterCommand;

typedef struct {
	char display[FilterResourceTypes];
	char *jsonallow;
	char *jsonblock;
	unsigned int allow;
	unsigned int block;
	unsigned int hide;
} FilterResources;

typedef struct _FilterRule {
	char *ifurl;
	char *iftopurl;
	char *activeuri;
	char *jsonpreface;
	char *comment;
	FilterResources p1;
	FilterResources p3;
	unsigned int hidedomain;
	unsigned int dirtydisplay;
	unsigned int dirtyjson;
	struct _FilterRule *prev;
	struct _FilterRule *next;
} FilterRule;

extern char *filterrulefile;
extern char *filterdir;
extern char *filterrulesjson;
extern FilterRule *filterrules;
extern WebKitUserContentFilter *filter;
extern WebKitUserContentFilterStore *filterstore;

void filter_read(void);
void filter_write(void);
void filter_apply(Client *c);
void filter_apply_cb(GObject *src, GAsyncResult *res, gpointer data);
void filter_freeall(void);
void filter_ruleinit(FilterRule *rule);
void filter_reset(FilterRule *rule);
void filter_stripper(Client *c, const char *uri);
void filter_stripperbytype(Client *c, const char *type);
void filter_rulecycle(FilterRule *rule, int modify, int p1, int p3);
void filter_texttobits(const char *desc, int len, FilterResources *r);
void filter_bitstotext(FilterRule *rule);
int  filter_isactive(FilterResources *party, int type);
void filter_setresource(FilterRule *rule, int modify, int p1, int p3);
void filter_setresourcenames(int types, char **names);
void filter_cycleresource(FilterResources *res, int type);
void filter_display(Client *c, FilterRule *rule);
void filter_updatejson(void);
void filter_ruletojson(FilterRule *rule);
FilterRule* filter_get(const char *fordomain);
void filtercmd(Client *c, const Arg *a);

/* utilities */
#define freeandnull(x) _ifnotnullfreeandnull((void*)&x)
void _ifnotnullfreeandnull(void **var);
void uritodomain(const char *uri, char *domain, int maxdomlen);
int lentodelim(const char *in, const char *delims, int delimsz);
int linelen(const char *in);
int fieldlen(const char *in);
int fieldcount(const char *in, int linelen);
char* nextfield(char *in);
char* getfield(char **in);
int stradd(char **base, int *remain, const char *addition);
void reallocstradd(char **base, int *maxlen, const char *addition);

#endif /* FILTER_H */
