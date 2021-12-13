#include <regex.h>

#include <gdk/gdk.h>
#include <webkit2/webkit2.h>
#include <X11/X.h>

#include "boredserf.h"
#include "filter.h"
#include "messages.h"

char *filterrulesjson;
FilterRule *filterrules;
WebKitUserContentFilter *filter;
WebKitUserContentFilterStore *filterstore;

void
filter_read(void)
{
	FilterRule *rule;
	FILE *file;
	char *buffer;
	char *field;
	char *desc;
	int buflen;
	int ret;
	int pos;

	if (NULL != filterrules)
		filter_freeall();
	if (NULL == filterrulefile)
		return;
	file = fopen(filterrulefile, "r");
	nullguard(file);

	fseek(file, 0, SEEK_END);
	buflen = ftell(file);
	buffer = malloc(buflen + 1);
	nullguard(buffer);
	buffer[buflen] = 0;
	rewind(file);

	ret = fread(buffer, 1, buflen, file);
	fclose(file);
	if (ret != buflen) {
		fprintf(stderr, "Incomplete read of rules file.\n");
		free(buffer);
		return;
	}

	rule = filterrules = malloc(sizeof(FilterRule));
	nullguard(rule);
	filter_ruleinit(rule);
	field = buffer;
	while (field) {
		switch (fieldcount(field, linelen(field))) {
		case 4: /* fall through */
			rule->ifurl = getfield(&field);
		case 3:
			rule->iftopurl = getfield(&field);
			desc = getfield(&field);
			filter_texttobits(desc, strlen(desc), &rule->p1);
			free(desc);
			desc = getfield(&field);
			filter_texttobits(desc, strlen(desc), &rule->p3);
			free(desc);
			break;
		case 0:
			rule->comment = malloc(1);
			nullguard(rule->comment);
			rule->comment[0] = 0;
			++field;
			break;
		case -1:
			field = NULL;
			break;
		default:
			rule->comment = strndup(field, linelen(field));
			field += linelen(field);
			field = nextfield(field);
			break;
		}
		rule->next = malloc(sizeof(FilterRule));
		nullguard(rule->next);
		filter_ruleinit(rule->next);
		rule->next->prev = rule;
		rule = rule->next;
	}

	rule = rule->prev;
	freeandnull(rule->next);
	free(buffer);
}

void
filter_write(void)
{
	enum { linemax = 2048 };
	FilterRule *rule = filterrules;
	FILE *output;
	char *filterrulefiletemp;
	char *ptr;
	char line[linemax];
	int hidep1;
	int hidep3;

	if (NULL == filterrules)
		return;

	nullguard(filterrulefile);
	filterrulefiletemp = malloc(strlen(filterrulefile) + 4);
	nullguard(filterrulefiletemp);
	sprintf(filterrulefiletemp, "%s%s", filterrulefile, "new");
	output = fopen(filterrulefiletemp, "w+");
	nullguard(output);

	while (NULL != rule) {
		ptr = rule->comment;
		if (NULL != ptr) {
			if (0 != rule->comment[0])
				fwrite(ptr, 1, strlen(ptr), output);
			fwrite("\n", 1, 1, output);
			rule = rule->next;
			continue;
		}

		ptr = rule->ifurl;
		if (NULL != ptr) {
			fwrite(ptr, 1, strlen(ptr), output);
			fwrite(" ", 1, 1, output);
		}

		ptr = rule->iftopurl;
		if (NULL != ptr)
			fwrite(ptr, 1, strlen(ptr), output);
		else
			fwrite("*", 1, 1, output);
		fwrite(" ", 1, 1, output);

		if (
			0 == rule->p1.allow &&
			0 == rule->p1.block &&
			0 == rule->p3.allow &&
			0 == rule->p3.block
		) {
			fwrite("1 3\n", 1, 4, output);
			rule = rule->next;
			continue;
		}

		hidep1 = rule->p1.hide;
		hidep3 = rule->p3.hide;
		if (hidep1 || hidep3) {
			rule->p1.hide = 0;
			rule->p3.hide = 0;
		}
		rule->dirtydisplay = 1;
		filter_bitstotext(rule);

		fwrite("1", 1, 1, output);
		ptr = rule->p1.display;
		if (NULL != ptr) {
			fprintf(output, "%s", ptr);
			fwrite(" ", 1, 1, output);
		}

		fwrite("3", 1, 1, output);
		ptr = rule->p3.display;
		if (NULL != ptr) {
			fprintf(output, "%s", ptr);
		}
		fwrite("\n", 1, 1, output);

		if (hidep1 || hidep3) {
			rule->p1.hide = hidep1;
			rule->p3.hide = hidep3;
			rule->dirtydisplay = 1;
		}
		rule = rule->next;
	}

	fclose(output);
	rename(filterrulefiletemp, filterrulefile);
	free(filterrulefiletemp);
}

void
filter_apply(Client *c)
{
	static const gchar *filterid = "boredserf_contentfilter";
	GBytes *json;
	gsize jsonsz;

	if (NULL == filterrulefile || NULL == filterrules)
		return;

	nullguard(c);

	if (!curconfig[ContentFilter].val.i)
		return;
	filter_updatejson();
	if (NULL == filterrulesjson)
		return;

	jsonsz = strlen(filterrulesjson);
	json = g_bytes_new(filterrulesjson, jsonsz);

	filterstore = webkit_user_content_filter_store_new(filterdir);
	webkit_user_content_filter_store_save(
		filterstore,
		filterid,
		json,
		NULL,
		filter_apply_cb,
		c
	);
}

void
filter_apply_cb(GObject *src_obj, GAsyncResult *res, gpointer data)
{
	Client *c = data;
	Arg a = { .i = 1 };
	GError *err = NULL;
	filter = webkit_user_content_filter_store_save_finish(
		filterstore,
		res,
		&err
	);
	if (err) {
		fprintf(stderr, err->message);
		return;
	}
	webkit_user_content_manager_add_filter(
		webkit_web_view_get_user_content_manager(c->view),
		filter);
	reload(c, &a);
}

void
filter_freeall(void)
{
	FilterRule *rule;
	while (NULL != filterrules) {
		rule = filterrules;
		filter_reset(rule);
		freeandnull(rule->iftopurl);
		freeandnull(rule->activeuri);
		filterrules = rule->next;
		free(rule);
	}
}

void
filter_ruleinit(FilterRule *r)
{
	nullguard(r);
	r->ifurl = r->iftopurl = r->activeuri = r->jsonpreface = NULL;
	r->comment = r->p1.jsonallow = r->p1.jsonblock = NULL;
	r->p1.display[0] = 0;
	r->p1.allow = r->p1.block = r->p1.hide = 0;
	r->p3.jsonallow = r->p3.jsonblock = NULL;
	r->p3.display[0] = 0;
	r->p3.allow = r->p3.block = r->p3.hide = 0;
	r->hidedomain = r->dirtydisplay = r->dirtyjson = 0;
	r->prev = r->next = NULL;
}

void
filter_reset(FilterRule *r)
{
	nullguard(r);
	/* retain iftopurl, activeuri, and list links */
	freeandnull(r->ifurl);
	freeandnull(r->activeuri);
	freeandnull(r->jsonpreface);
	freeandnull(r->comment);
	freeandnull(r->p1.jsonallow);
	freeandnull(r->p1.jsonblock);
	freeandnull(r->p3.jsonallow);
	freeandnull(r->p3.jsonblock);
	r->p1.display[0] = r->p3.display[0] = 0;
	r->p1.allow = r->p1.block = 0;
	r->p3.allow = r->p3.block = 0;
	r->p1.hide = r->p3.hide = r->hidedomain = 0;
	r->dirtydisplay = r->dirtyjson = 1;
}

void
filter_stripper(Client *c, const char *uri)
{
	FilterRule *rule = filter_get(uri);
	nullguard(rule);
	if (0 == rule->p1.block)
		return;
	if (rule->p1.block & 1<<FilterCSS)
		filter_stripperbytype(c, "style");
	if (rule->p1.block & 1<<FilterFonts)
		filter_stripperbytype(c, "font");
	if (rule->p1.block & 1<<FilterImages) {
		filter_stripperbytype(c, "img");
		filter_stripperbytype(c, "picture");
	}
	if (rule->p1.block & 1<<FilterSVG)
		filter_stripperbytype(c, "svg");
	if (rule->p1.block & 1<<FilterMedia) {
		filter_stripperbytype(c, "source");
		filter_stripperbytype(c, "video");
	}
	if (rule->p1.block & 1<<FilterScripts) {
		filter_stripperbytype(c, "applet");
		filter_stripperbytype(c, "canvas");
		filter_stripperbytype(c, "embed");
		filter_stripperbytype(c, "object");
		filter_stripperbytype(c, "script");
	}
	if (rule->p1.block & 1<<FilterRaw) {
		filter_stripperbytype(c, "link");
	}
}

void
filter_stripperbytype(Client *c, const char *type)
{
	enum { maxlen = 2048 };
	nullguard(c);
	nullguard(type);
	char script[maxlen];
	snprintf(script, maxlen, "%s%s%s",
		"var elements = document.getElementsByTagName(\"",
		type,
		"\"); for (let i = 0;i < elements.length;++i)"
		"elements[i].innerHTML = '';"
	);
	evalscript(c, script);
}

void
filter_setresource(FilterRule *rule, int cmd, int type, int p1, int p3)
{
	int *thisrule1, *thisrule3;
	int *thatrule1, *thatrule3;

	nullguard(rule);
	if (guardwordsize(type))
		return;

	switch (cmd) {
	case SetFilterToBlock:
		thisrule1 = &rule->p1.block;
		thisrule3 = &rule->p3.block;
		thatrule1 = &rule->p1.allow;
		thatrule3 = &rule->p3.allow;
		break;
	case SetFilterToAllow:
		thisrule1 = &rule->p1.allow;
		thisrule3 = &rule->p3.allow;
		thatrule1 = &rule->p1.block;
		thatrule3 = &rule->p3.block;
		break;
	default:
		err("Unrecognized command type.\n");
		return;
	}

	if (p1 && p3) {
		/* set both to new p1 value */
		if (*thisrule1 & 1 << type) {
			*thisrule1 &= ~(1<<type);
			*thisrule3 &= ~(1<<type);
		} else {
			*thisrule1 |= 1<<type;
			*thisrule3 |= 1<<type;
			*thatrule1 &= ~(1<<type);
			*thatrule3 &= ~(1<<type);
		}
	} else if (p1) {
		if (*thisrule1 & 1 << type) {
			*thisrule1 &= ~(1<<type);
		} else {
			*thisrule1 |= 1<<type;
			*thatrule1 &= ~(1<<type);
		}
	} else if (p3) {
		if (*thisrule3 & 1 << type) {
			*thisrule3 &= ~(1<<type);
		} else {
			*thisrule3 |= 1<<type;
			*thatrule3 &= ~(1<<type);
		}
	}
	rule->dirtydisplay = rule->dirtyjson = 1;
}

void
filter_texttobits(const char *desc, int desclen, FilterResources *res)
{
	int pos = 0;
	int type;
	nullguard(desc);
	nullguard(res);
	res->block = res->allow = 0;
	if ('1' == desc[0] || '3' == desc[0])
		++pos;
	while (pos < desclen && desc[pos]) {
		switch (desc[pos++]) {
		case 'd': res->block |= 1 << FilterDocs; break;
		case 'D': res->allow |= 1 << FilterDocs; break;
		case 'c': res->block |= 1 << FilterCSS; break;
		case 'C': res->allow |= 1 << FilterCSS; break;
		case 'f': res->block |= 1 << FilterFonts; break;
		case 'F': res->allow |= 1 << FilterFonts; break;
		case 'i': res->block |= 1 << FilterImages; break;
		case 'I': res->allow |= 1 << FilterImages; break;
		case 'v': res->block |= 1 << FilterSVG; break;
		case 'V': res->allow |= 1 << FilterSVG; break;
		case 'm': res->block |= 1 << FilterMedia; break;
		case 'M': res->allow |= 1 << FilterMedia; break;
		case 's': res->block |= 1 << FilterScripts; break;
		case 'S': res->allow |= 1 << FilterScripts; break;
		case 'r': res->block |= 1 << FilterRaw; break;
		case 'R': res->allow |= 1 << FilterRaw; break;
		case 'p': res->block |= 1 << FilterPopup; break;
		case 'P': res->allow |= 1 << FilterPopup; break;
		case '1': case '3': break;
		default: return;
		}
	}
}

void
filter_bitstotext(FilterRule *rule)
{
	const char allowed[FilterResourceTypes] = "DCFIVMSRP";
	const char blocked[FilterResourceTypes] = "dcfivmsrp";
	int maxlen;
	int pos1, pos3;
	int i;

	nullguard(rule);
	if (!rule->dirtydisplay)
		return;

	pos1 = pos3 = 0;
	for (i = 0; i < FilterResourceTypes; ++i) {
		if (rule->p1.allow & 1<<i)
			rule->p1.display[pos1++] = allowed[i];
		else if (rule->p1.block & 1<<i)
			rule->p1.display[pos1++] = blocked[i];

		if (rule->p3.allow & 1<<i)
			rule->p3.display[pos3++] = allowed[i];
		else if (rule->p3.block & 1<<i)
			rule->p3.display[pos3++] = blocked[i];
	}

	rule->p1.display[pos1] = 0;
	rule->p3.display[pos3] = 0;
	rule->dirtydisplay = 0;
}

FilterRule*
filter_get(const char *fordomain)
{
	enum { maxlen = 2048 };
	FilterRule *rule = filterrules;
	char shorter[maxlen];

	nullguard(fordomain, NULL);
	if (NULL == filterrules) {
		filterrules = malloc(sizeof(FilterRule));
		nullguard(filterrules, NULL);
		filter_ruleinit(filterrules);
	}

	if (NULL != strchr(fordomain, '/'))
		uritodomain(fordomain, shorter, maxlen);
	else
		strncpy(shorter, fordomain, maxlen);

	while (NULL != rule) {
		if (NULL != rule->ifurl || NULL != rule->comment) {
			if (NULL == rule->next)
				break;
			rule = rule->next;
			continue;
		}
		if (NULL == rule->iftopurl) {
			filterrules->iftopurl = strdup(shorter);
			return rule;
		}
		if (0 == strcmp(shorter, rule->iftopurl)) {
			rule->dirtydisplay = rule->dirtyjson = 1;
			return rule;
		}
		if (NULL == rule->next)
			break;
		rule = rule->next;
	}

	rule->next = malloc(sizeof(FilterRule));
	nullguard(rule->next, NULL);
	filter_ruleinit(rule->next);
	rule->next->iftopurl = strdup(shorter);
	rule->next->prev = rule;

	return rule->next;
}

void
filter_display(Client *c, FilterRule *rule)
{
	enum { maxlen = 256 };
	static char display[maxlen];
	int len = maxlen;
	nullguard(c);
	nullguard(rule);
	if (rule->dirtydisplay)
		filter_bitstotext(rule);

	display[0] = 0;

	if (NULL != rule->ifurl) {
		len -= strlen(rule->ifurl);
		if (0 >= len)
			return;
		strcat(display, rule->ifurl);
		strcat(display, " ");
	}

	if (NULL != rule->iftopurl) {
		len -= strlen(rule->iftopurl);
		if (0 >= len)
			return;
		strcat(display, rule->iftopurl);
	} else {
		--len;
		if (0 >= len)
			return;
		strcat(display, "*");
	}

	if (!rule->p1.hide) {
		len -= strlen(rule->p1.display) + 1;
		if (0 >= len)
			return;
		strcat(display, " ");
		strcat(display, "1");
		strcat(display, rule->p1.display);
	}

	if (!rule->p3.hide) {
		len -= strlen(rule->p3.display) + 1;
		if (0 >= len)
			return;
		strcat(display, " ");
		strcat(display, "3");
		strcat(display, rule->p3.display);
	}

	c->overtitle = display;
	updatetitle(c);
}

void
filter_updatejson(void)
{
	FilterRule *rule = filterrules;
	const char actblock[] = "]},\n \"action\":{\"type\":"
		"\"block\"}}";
	const char actallow[] = "]},\n \"action\":{\"type\":"
		"\"ignore-previous-rules\"}}";
	char *j;
	int len = 2048;
	int first = 1;

	j = malloc(len);
	nullguard(j);
	j[0] = 0;

	reallocstradd(&j, &len, "[");
	while (NULL != rule) {
		filter_ruletojson(rule);
		if (NULL == rule->jsonpreface) {
			rule = rule->next;
			continue;
		}
		if (NULL != rule->p1.jsonallow) {
			if (!first)
				reallocstradd(&j, &len, ",");
			else
				first = 0;
			reallocstradd(&j, &len, rule->jsonpreface);
			reallocstradd(&j, &len, rule->p1.jsonallow);
			reallocstradd(&j, &len, actallow);
		}
		if (NULL != rule->p1.jsonblock) {
			if (!first)
				reallocstradd(&j, &len, ",");
			else
				first= 0;
			reallocstradd(&j, &len, rule->jsonpreface);
			reallocstradd(&j, &len, rule->p1.jsonblock);
			reallocstradd(&j, &len, actblock);
		}
		if (NULL != rule->p3.jsonallow) {
			if (!first)
				reallocstradd(&j, &len, ",");
			else
				first = 0;
			reallocstradd(&j, &len, rule->jsonpreface);
			reallocstradd(&j, &len, rule->p3.jsonallow);
			reallocstradd(&j, &len, actallow);
		}
		if (NULL != rule->p3.jsonblock) {
			if (!first)
				reallocstradd(&j, &len, ",");
			else
				first = 0;
			reallocstradd(&j, &len, rule->jsonpreface);
			reallocstradd(&j, &len, rule->p3.jsonblock);
			reallocstradd(&j, &len, actblock);
		}

		rule = rule->next;
	}

	if (1 == strlen(j))
		freeandnull(j);
	else
		reallocstradd(&j, &len, "]");

	if (NULL != j) {
		freeandnull(filterrulesjson);
		filterrulesjson = j;
	}
}

void
filter_ruletojson(FilterRule *rule)
{
	enum { max = 4096 };
	char preface[max];
	char *p;
	int c;

	nullguard(rule);
	rule->dirtyjson = 0;
	if (
		0 == rule->p1.allow &&
		0 == rule->p1.block &&
		0 == rule->p3.allow &&
		0 == rule->p3.block
	) {
		freeandnull(rule->jsonpreface);
		freeandnull(rule->p1.jsonallow);
		freeandnull(rule->p1.jsonblock);
		freeandnull(rule->p3.jsonallow);
		freeandnull(rule->p3.jsonblock);
		return;
	}

	if (NULL == rule->jsonpreface) {
		p = preface;
		c = max;
		stradd(&p, &c, "{\n \"trigger\":{\"url-filter\":\"");
		if (NULL != rule->ifurl)
			stradd(&p, &c, rule->ifurl);
		else
			stradd(&p, &c,".*");
		stradd(&p, &c, "\"");
		if (
			NULL != rule->iftopurl &&
			'*' != rule->iftopurl[0] &&
			0 != rule->iftopurl[1]
		) {
			stradd(&p, &c, ",\"if-top-url\":[\"");
			stradd(&p, &c, rule->iftopurl);
			stradd(&p, &c, "\"]");
		}
		stradd(&p, &c, ",\"resource-type\":[");
		p[0] = 0;
		rule->jsonpreface = strdup(preface);
	}

	filter_setresourcenames(rule->p1.allow, &rule->p1.jsonallow);
	filter_setresourcenames(rule->p1.block, &rule->p1.jsonblock);
	filter_setresourcenames(rule->p3.allow, &rule->p3.jsonallow);
	filter_setresourcenames(rule->p3.block, &rule->p3.jsonblock);
}

void
filter_setresourcenames(int types, char **names)
{
	enum { max = 256 };
	const char resource[FilterResourceTypes][16] = {
		"\"document\"",
		"\"style-sheet\"",
		"\"font\"",
		"\"image\"",
		"\"svg-document\"",
		"\"media\"",
		"\"script\"",
		"\"raw\"",
		"\"popup\""
	};
	char text[max];
	char *p = text;
	int c = max;
	int first = 1;
	int i;

	nullguard(names);
	if (NULL != *names) {
		free(*names);
		*names = NULL;
	}

	if (!types)
		return;

	for (i = 0; i < FilterResourceTypes; ++i) {
		if (! (types & 1<<i))
			continue;
		if (!first)
			stradd(&p, &c, ",");
		else
			first = 0;
		stradd(&p, &c, resource[i]);
	}
	p[0] = 0;

	*names = strdup(text);
}

void
_ifnotnullfreeandnull(void **ptr)
{
	nullguard(ptr);
	if (NULL == *ptr)
		return;
	free(*ptr);
	*ptr = NULL;
}

void
uritodomain(const char *uri, char *domain, int maxdomlen)
{
	int offset = 0;
	int len = 0;
	int start = 0;
	int end = 0;

	nullguard(domain);
	if (NULL == uri) {
		strncpy(domain, "*", maxdomlen);
		return;
	}
	len = strlen(uri);
	if (0 > len) {
		fprintf(stderr, "%s could not get URI length.\n", __func__);
		return;
	}

	while (offset < len && ':' != uri[offset++]);
	while (offset < len && '/' == uri[offset++]);
	if (offset >= len) {
		fprintf(stderr, "%s found malformed URI.\n", __func__);
		return;
	}

	start = offset - 1;
	end = strchr(uri + start, '/') - uri;
	if (0 >= end || end <= start) {
		fprintf(stderr, "%s failed to get substring.\n", __func__);
		return;
	}

	len = end - start;
	if (len <= maxdomlen) {
		strncpy(domain, uri + start, len);
		if (len < maxdomlen)
			domain[len] = 0;
	} else {
		fprintf(stderr, "Domain exceeds max length for: %s\n", uri);
	}

	return;
}

int
lentodelim(const char *in, const char *delims, int delimsz)
{
	char testnull = 0;
	const char *test;
	int count = 0;
	int testsz;
	int i;

	nullguard(in, -1);
	if (NULL == delims || 0 == delimsz) {
		test = &testnull;
		testsz = 1;
	} else {
		test = delims;
		testsz = delimsz;
	}
	if (0 >= testsz)
		return 0;

	while (1) {
		for (i = 0; i < testsz; ++i) {
			if (in[count] == test[i])
				return count;
		}
		++count;
	}
	return count;
}

int
linelen(const char *in)
{
	const char delims[] = "\n";
	return lentodelim(in, delims, 2);
}

int
fieldlen(const char *in)
{
	const char delims[] = " \t\n";
	return lentodelim(in, delims, 4);
}

int
fieldcount(const char *in, int linelen)
{
	int isblank = 1;
	int fields = 0;
	int pos = 0;
	int i;

	nullguard(in, -1);

	if ('#' == in[0])
		return 0;
	while (pos < linelen) {
		switch (in[pos++]) {
		case 0:
			return fields ? fields : -1;
		case '\n':
			return fields;
		case ' ':  /* fall through */
		case '\t':
			isblank = 1;
			continue;
		default:
			if (!isblank)
				continue;
			isblank = 0;
			++fields;
			continue;
		}
	}
	return fields;
}

char*
nextfield(char *in)
{
	int pos = 0;
	int isblank = 0;
	if (NULL == in)
		return NULL;
	while (1) {
		switch (in[pos]) {
		case 0: /* fall through */
			return NULL;
		case ' ':  /* fall through */
		case '\t':
			isblank = 1;
			break;
		case '\n':
			if (in[++pos])
				return in + pos;
			else
				return NULL;
			break;
		default:
			if (isblank)
				return in + pos;
			break;
		}
		++pos;
	}
	return NULL;
}

char*
getfield(char **in)
{
	char *result;
	int len = 0;
	nullguard(in, NULL);
	nullguard(*in, NULL);
	len = fieldlen(*in);
	result = strndup(*in, len ? len : 1);
	*in = nextfield(*in);
	return result;
}

void
filtercmd(Client *c, const Arg *a)
{
	enum { maxdomlen = 1024 };
	static char olddomain[maxdomlen] = { 0 };
	char curdomain[maxdomlen] = { 0 };
	static FilterRule *rule;
	static int editing1p = 1;
	static int editing3p = 1;
	int *thisparty = NULL;
	int *thatparty = NULL;
	Arg passthru = { .i = 1 };

	nullguard(c);
	nullguard(filterrules);

	uritodomain(webkit_web_view_get_uri(c->view), curdomain, maxdomlen);
	if (0 == olddomain[0]) {
		strcpy(olddomain, curdomain);
		rule = filter_get(curdomain);
	} else if (0 != strcmp(curdomain, olddomain)) {
		strncpy(olddomain, curdomain, maxdomlen);
		rule = filter_get(curdomain);
	}
	nullguard(rule);

	switch (a->i) {
	case FilterDocs:    /* fall through */
	case FilterCSS:     /* fall through */
	case FilterFonts:   /* fall through */
	case FilterImages:  /* fall through */
	case FilterSVG:     /* fall through */
	case FilterMedia:   /* fall through */
	case FilterScripts: /* fall through */
	case FilterRaw:     /* fall through */
	case FilterPopup:
		filter_setresource(
			rule,
			SetFilterToBlock,
			a->i,
			editing1p,
			editing3p
		);
		filter_display(c, rule);
		break;

	case AllowDocs:    /* fall through */
	case AllowCSS:     /* fall through */
	case AllowFonts:   /* fall through */
	case AllowImages:  /* fall through */
	case AllowSVG:     /* fall through */
	case AllowMedia:   /* fall through */
	case AllowScripts: /* fall through */
	case AllowRaw:     /* fall through */
	case AllowPopup:
		filter_setresource(
			rule,
			SetFilterToAllow,
			a->i - AllowDocs,
			editing1p,
			editing3p
		);
		filter_display(c, rule);
		break;

	case FilterSel1Party: /* fall through */
		thisparty = &editing1p;
		thatparty = &editing3p;
	case FilterSel3Party:
		if (NULL == thisparty) {
			thisparty = &editing3p;
			thatparty = &editing1p;
		}
		if (0 == *thisparty)
			*thisparty = 1;
		else
			*thatparty = 0;
		rule->p1.hide = editing1p ? 0 : 1;
		rule->p3.hide = editing3p ? 0 : 1;
		rule->dirtydisplay = 1;
		filter_display(c, rule);
		break;

	case FilterDispRule:
		filter_display(c, rule);
		break;
	case FilterTglDomDisp:
		rule->hidedomain = rule->hidedomain ? 0 : 1;
		rule->dirtydisplay = 1;
		filter_display(c, rule);
		break;
	case FilterApply:
		filter_apply(c);
		break;
	case FilterWrite:
		filter_write();
		break;
	case FilterResetRule:
		filter_reset(rule);
		filter_display(c, rule);
		break;
	case FilterTogGlobal:
		if (curconfig[ContentFilter].val.i) {
			webkit_user_content_manager_remove_all_filters(
				webkit_web_view_get_user_content_manager(
					c->view)
			);
			curconfig[ContentFilter].val.i = 0;
			reload(c, &passthru);
		} else {
			curconfig[ContentFilter].val.i = 1;
			filter_apply(c);
		}
		break;
	default:
		break;
	}
}

int
guardwordsize(int modify)
{
#ifdef __WORDSIZE
	if (modify >= __WORDSIZE) {
		fprintf(stderr, "modify bits exceed word size; "
			"check the enum FilterCommand value passed "
			"to filtercmd()\n");
		return 1;
	}
#endif /* __WORDSIZE */
	return 0;
}

