/* modifier 0 means no modifier */
int bs_useragent    = 1;  /* Append boredserf version to default WebKit user agent */
char *fulluseragent  = ""; /* Or override the whole user agent string */
char *scriptfile     = "~/.config/boredserf/script.js";
char *styledir       = "~/.config/boredserf/styles/";
char *certdir        = "~/.config/boredserf/certificates/";
char *cachedir       = "~/.config/boredserf/cache/";
char *cookiefile     = "~/.config/boredserf/cookies.txt";
char *filterrulefile = "~/.config/boredserf/filter.rules";
char *filterdir      = "~/.config/boredserf/filters/";
char *histfile       = "~/.config/boredserf/histfile";

/* Utilities for interaction */
/* dmenu as location bar (in a shell with environment variables) */
const char *selector_go[] = {
	"/bin/sh", "sh", "-c",
	"echo $BS_URI | dmenu -p Go: -w $BS_WINID",
	NULL,
};

/* Finding with dmenu (executed without shell) */
const char *selector_find_dmenu[] = {
	"/usr/bin/dmenu",
	"dmenu", "-i", "-p", "Find:", "-w", winid,
	NULL
};

/* Finding with fzf (in st in a shell with incremental previews) */
/* Note: the shell redirects input and output, so we must read and write
 *       to the file descriptors provided in $BS_INPUT and $BS_OUTPUT.
 *       Unfortunately, dash appears to fail with this redirection. */
const char *selector_find_fzf[] = {
	"/usr/local/bin/st", "st", "-e", "/usr/bin/bash", "-c",
	"<& \"$BS_INPUT\" fzf -i --preview='echo \"$BS_TEXT\" \
		| fmt --width=$FZF_PREVIEW_COLUMNS \
		| grep -i --color=always {}' "
	"| sed /^[[:blank:]]*$/d "
	">& \"$BS_RESULT\"",
	NULL
};

/* Decide which finder to use */
const char **selector_find = selector_find_fzf;


/* Webkit default features */
/* Highest priority value will be used.
 * Default parameters are priority 0
 * Per-uri parameters are priority 1
 * Command parameters are priority 2
 */
Parameter defconfig[ParameterLast] = {
	/* parameter                    Arg value       priority */
	[AccessMicrophone]    =       { { .i = 0 },     },
	[AccessWebcam]        =       { { .i = 0 },     },
	[Certificate]         =       { { .i = 0 },     },
	[CaretBrowsing]       =       { { .i = 0 },     },
	[ContentFilter]       =       { { .i = 1 },     },
	[CookiePolicies]      =       { { .v = "@Aa" }, },
	[DefaultCharset]      =       { { .v = "UTF-8" }, },
	[DiskCache]           =       { { .i = 1 },     },
	[DNSPrefetch]         =       { { .i = 0 },     },
	[Ephemeral]           =       { { .i = 0 },     },
	[FileURLsCrossAccess] =       { { .i = 0 },     },
	[FontSize]            =       { { .i = 12 },    },
	[FrameFlattening]     =       { { .i = 0 },     },
	[Geolocation]         =       { { .i = 0 },     },
	[HideBackground]      =       { { .i = 0 },     },
	[Inspector]           =       { { .i = 0 },     },
	[Java]                =       { { .i = 1 },     },
	[JavaScript]          =       { { .i = 1 },     },
	[KioskMode]           =       { { .i = 0 },     },
	[LoadImages]          =       { { .i = 1 },     },
	[MediaManualPlay]     =       { { .i = 1 },     },
	[PreferredLanguages]  =       { { .v = (char *[]){ NULL } }, },
	[RunInFullscreen]     =       { { .i = 0 },     },
	[ScrollBars]          =       { { .i = 1 },     },
	[ShowIndicators]      =       { { .i = 1 },     },
	[SiteQuirks]          =       { { .i = 1 },     },
	[SmoothScrolling]     =       { { .i = 0 },     },
	[SpellChecking]       =       { { .i = 0 },     },
	[SpellLanguages]      =       { { .v = ((char *[]){ "en_US", NULL }) }, },
	[StrictTLS]           =       { { .i = 1 },     },
	[Style]               =       { { .i = 1 },     },
	[WebGL]               =       { { .i = 0 },     },
	[ZoomLevel]           =       { { .f = 1.0 },   },
};

SearchEngine searchengines[] = {
	{ "d", "https://duckduckgo.com/lite/?q=%s" },
	{ "s", "https://www.startpage.com/sp/search?query=%s" },
	{ "w", "https://en.wikipedia.org/wiki/Special:Search/%s" },
};

UriParameters uriparams[] = {
	{ "(://|\\.)suckless\\.org(/|$)", {
	  [JavaScript] = { { .i = 0 }, 1 },
	}, },
};

/* default window size: width, height */
int winsize[] = { 800, 600 };

WebKitFindOptions findopts = WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE |
                                    WEBKIT_FIND_OPTIONS_WRAP_AROUND;

/* DOWNLOAD(URI, referer) */
#define DOWNLOAD(u, r) { \
        .v = (const char *[]){ "st", "-e", "/bin/sh", "-c",\
             "curl -g -L -J -O -A \"$1\" -b \"$2\" -c \"$2\"" \
             " -e \"$3\" \"$4\"; read", \
             "boredserf-download", useragent, cookiefile, r, u, NULL \
        } \
}

/* PLUMB(URI) */
/* This called when some URI which does not begin with "about:",
 * "http://" or "https://" should be opened.
 */
#define PLUMB(u) {\
        .v = (const char *[]){ "/bin/sh", "-c", \
             "xdg-open \"$0\"", u, NULL \
        } \
}

/* VIDEOPLAY(URI) */
#define VIDEOPLAY(u) {\
        .v = (const char *[]){ "/bin/sh", "-c", \
             "mpv --really-quiet \"$0\"", u, NULL \
        } \
}

/* styles */
/*
 * The iteration will stop at the first match, beginning at the beginning of
 * the list.
 */
SiteSpecific styles[] = {
	/* regexp               file in $styledir */
	{ ".*",                 "default.css" },
};

/* certificates */
/*
 * Provide custom certificate for urls
 */
SiteSpecific certs[] = {
	/* regexp               file in $certdir */
	{ "://suckless\\.org/", "suckless.org.crt" },
};

#define MODKEY GDK_CONTROL_MASK

/* hotkeys */
/*
 * If you use anything else but MODKEY and GDK_SHIFT_MASK, don't forget to
 * edit the CLEANMASK() macro.
 */

/* content filter bindings */
Key filterkeys[] = {
	{ 0,              GDK_KEY_1, filtercmd, { .i = FilterSel1Party  } },
	{ 0,              GDK_KEY_2, filtercmd, { .i = FilterTglDomDisp } },
	{ 0,              GDK_KEY_3, filtercmd, { .i = FilterSel3Party  } },
	{ 0,              GDK_KEY_a, filtercmd, { .i = FilterApply      } },
	{ 0,              GDK_KEY_c, filtercmd, { .i = FilterCSS        } },
	{ GDK_SHIFT_MASK, GDK_KEY_c, filtercmd, { .i = AllowCSS         } },
	{ 0,              GDK_KEY_d, filtercmd, { .i = FilterDocs       } },
	{ GDK_SHIFT_MASK, GDK_KEY_d, filtercmd, { .i = AllowDocs        } },
	{ 0,              GDK_KEY_f, filtercmd, { .i = FilterFonts      } },
	{ GDK_SHIFT_MASK, GDK_KEY_f, filtercmd, { .i = AllowFonts       } },
	{ 0,              GDK_KEY_i, filtercmd, { .i = FilterImages     } },
	{ GDK_SHIFT_MASK, GDK_KEY_i, filtercmd, { .i = AllowImages      } },
	{ 0,              GDK_KEY_m, filtercmd, { .i = FilterMedia      } },
	{ GDK_SHIFT_MASK, GDK_KEY_m, filtercmd, { .i = AllowMedia       } },
	{ 0,              GDK_KEY_p, filtercmd, { .i = FilterPopup      } },
	{ GDK_SHIFT_MASK, GDK_KEY_p, filtercmd, { .i = AllowPopup       } },
	{ 0,              GDK_KEY_q, filtercmd, { .i = FilterResetRule  } },
	{ 0,              GDK_KEY_r, filtercmd, { .i = FilterRaw        } },
	{ GDK_SHIFT_MASK, GDK_KEY_r, filtercmd, { .i = AllowRaw         } },
	{ 0,              GDK_KEY_s, filtercmd, { .i = FilterScripts    } },
	{ GDK_SHIFT_MASK, GDK_KEY_s, filtercmd, { .i = AllowScripts     } },
	{ 0,              GDK_KEY_t, filtercmd, { .i = FilterTogGlobal  } },
	{ 0,              GDK_KEY_v, filtercmd, { .i = FilterSVG        } },
	{ GDK_SHIFT_MASK, GDK_KEY_v, filtercmd, { .i = AllowSVG         } },
	{ 0,              GDK_KEY_w, filtercmd, { .i = FilterWrite      } },
	{ 0,         GDK_KEY_Return, filtercmd, { .i = FilterApply      } },
	{ 0,         GDK_KEY_Escape, filtercmd, { .i = FilterResetRule  } },
	{ 0, 0, NULL, { .v = NULL} }
};

/* default key bindings */
Key keys[] = {
	/* modifier              keyval          function    arg */
	{ MODKEY,                GDK_KEY_g,      i_seturi,   { 0 } },
	{ MODKEY,                GDK_KEY_f,      i_find,     { 0 } },
	{ MODKEY,                GDK_KEY_slash,  i_find,     { 0 } },
	{ 0,                     GDK_KEY_Escape, stop,       { 0 } },
	{ MODKEY,                GDK_KEY_c,      stop,       { 0 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_r,      reload,     { .i = 1 } },
	{ MODKEY,                GDK_KEY_r,      reload,     { .i = 0 } },

	{ MODKEY,                GDK_KEY_l,      navigate,   { .i = +1 } },
	{ MODKEY,                GDK_KEY_h,      navigate,   { .i = -1 } },

	/* vertical and horizontal scrolling, in viewport percentage */
	{ MODKEY,                GDK_KEY_j,      scrollv,    { .i = +10 } },
	{ MODKEY,                GDK_KEY_k,      scrollv,    { .i = -10 } },
	{ MODKEY,                GDK_KEY_space,  scrollv,    { .i = +50 } },
	{ MODKEY,                GDK_KEY_b,      scrollv,    { .i = -50 } },
	{ MODKEY,                GDK_KEY_i,      scrollh,    { .i = +10 } },
	{ MODKEY,                GDK_KEY_u,      scrollh,    { .i = -10 } },


	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_j,      zoom,       { .i = -1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_k,      zoom,       { .i = +1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_q,      zoom,       { .i = 0  } },
	{ MODKEY,                GDK_KEY_minus,  zoom,       { .i = -1 } },
	{ MODKEY,                GDK_KEY_plus,   zoom,       { .i = +1 } },

	{ MODKEY,                GDK_KEY_p,      clipboard,  { .i = 1 } },
	{ MODKEY,                GDK_KEY_y,      clipboard,  { .i = 0 } },

	{ MODKEY,                GDK_KEY_n,      find,       { .i = +1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_n,      find,       { .i = -1 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_p,      print,      { 0 } },
	{ MODKEY,                GDK_KEY_t,      showcert,   { 0 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_a,      togglecookiepolicy, { 0 } },
	{ 0,                     GDK_KEY_F11,    togglefullscreen, { 0 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_o,      toggleinspector, { 0 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_c,      toggle,     { .i = CaretBrowsing } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_f,      toggle,     { .i = FrameFlattening } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_g,      toggle,     { .i = Geolocation } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_s,      toggle,     { .i = JavaScript } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_i,      toggle,     { .i = LoadImages } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_b,      toggle,     { .i = ScrollBars } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_t,      toggle,     { .i = StrictTLS } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_m,      toggle,     { .i = Style } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_y,      setkeytree, { .v = filterkeys } },
	{ 0, 0, NULL, { .v = NULL} } /* last entry */
};
Key *defkeytree = keys;

/* button definitions */
/* target can be OnDoc, OnLink, OnImg, OnMedia, OnEdit, OnBar, OnSel, OnAny */
Button buttons[] = {
	/* target       event mask      button  function        argument        stop event */
	{ OnLink,       0,              2,      clicknewwindow, { .i = 0 },     1 },
	{ OnLink,       MODKEY,         2,      clicknewwindow, { .i = 1 },     1 },
	{ OnLink,       MODKEY,         1,      clicknewwindow, { .i = 1 },     1 },
	{ OnAny,        0,              8,      clicknavigate,  { .i = -1 },    1 },
	{ OnAny,        0,              9,      clicknavigate,  { .i = +1 },    1 },
	{ OnMedia,      MODKEY,         1,      clickexternplayer, { 0 },       1 },
};
