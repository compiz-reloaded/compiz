/*
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <regex.h>

#include <X11/Xatom.h>

#include <compiz-core.h>

static CompMetadata regexMetadata;

static int displayPrivateIndex;

typedef struct _RegexDisplay {
    int		     screenPrivateIndex;
    HandleEventProc  handleEvent;
    MatchInitExpProc matchInitExp;
    Atom	     roleAtom;
    Atom             visibleNameAtom;
    CompTimeoutHandle timeoutHandle;
} RegexDisplay;

typedef struct _RegexScreen {
    int	windowPrivateIndex;
} RegexScreen;

typedef struct _RegexWindow {
    char *title;
    char *role;
} RegexWindow;

#define GET_REGEX_DISPLAY(d)					   \
    ((RegexDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define REGEX_DISPLAY(d)		     \
    RegexDisplay *rd = GET_REGEX_DISPLAY (d)

#define GET_REGEX_SCREEN(s, rd)					       \
    ((RegexScreen *) (s)->base.privates[(rd)->screenPrivateIndex].ptr)

#define REGEX_SCREEN(s)							   \
    RegexScreen *rs = GET_REGEX_SCREEN (s, GET_REGEX_DISPLAY (s->display))

#define GET_REGEX_WINDOW(w, rs)					       \
    ((RegexWindow *) (w)->base.privates[(rs)->windowPrivateIndex].ptr)

#define REGEX_WINDOW(w)					       \
    RegexWindow *rw = GET_REGEX_WINDOW  (w,		       \
		      GET_REGEX_SCREEN  (w->screen,	       \
		      GET_REGEX_DISPLAY (w->screen->display)))

static void
regexMatchExpFini (CompDisplay *d,
		   CompPrivate private)
{
    regex_t *preg = (regex_t *) private.ptr;

    if (preg)
    {
	regfree (preg);
	free (preg);
    }
}

static Bool
regexMatchExpEvalTitle (CompDisplay *d,
			CompWindow  *w,
			CompPrivate private)
{
    regex_t *preg = (regex_t *) private.ptr;
    int	    status;

    REGEX_WINDOW (w);

    if (!preg)
	return FALSE;

    if (!rw->title)
	return FALSE;

    status = regexec (preg, rw->title, 0, NULL, 0);
    if (status)
	return FALSE;

    return TRUE;
}

static Bool
regexMatchExpEvalRole (CompDisplay *d,
		       CompWindow  *w,
		       CompPrivate private)
{
    regex_t *preg = (regex_t *) private.ptr;
    int	    status;

    REGEX_WINDOW (w);

    if (!preg)
	return FALSE;

    if (!rw->role)
	return FALSE;

    status = regexec (preg, rw->role, 0, NULL, 0);
    if (status)
	return FALSE;

    return TRUE;
}

static Bool
regexMatchExpEvalClass (CompDisplay *d,
			CompWindow  *w,
			CompPrivate private)
{
    regex_t *preg = (regex_t *) private.ptr;
    int	    status;

    if (!preg)
	return FALSE;

    if (!w->resClass)
	return FALSE;

    status = regexec (preg, w->resClass, 0, NULL, 0);
    if (status)
	return FALSE;

    return TRUE;
}

static Bool
regexMatchExpEvalName (CompDisplay *d,
		       CompWindow  *w,
		       CompPrivate private)
{
    regex_t *preg = (regex_t *) private.ptr;
    int	    status;

    if (!preg)
	return FALSE;

    if (!w->resName)
	return FALSE;

    status = regexec (preg, w->resName, 0, NULL, 0);
    if (status)
	return FALSE;

    return TRUE;
}

static void
regexMatchInitExp (CompDisplay  *d,
		   CompMatchExp *exp,
		   const char	*value)
{
    static struct _Prefix {
	char		     *s;
	int		     len;
	CompMatchExpEvalProc eval;
	unsigned int         flags;
    } prefix[] = {
	{ "title=", 6, regexMatchExpEvalTitle, 0 },
	{ "role=",  5, regexMatchExpEvalRole, 0  },
	{ "class=", 6, regexMatchExpEvalClass, 0 },
	{ "name=",  5, regexMatchExpEvalName, 0  },
	{ "ititle=", 7, regexMatchExpEvalTitle, REG_ICASE },
	{ "irole=",  6, regexMatchExpEvalRole, REG_ICASE  },
	{ "iclass=", 7, regexMatchExpEvalClass, REG_ICASE },
	{ "iname=",  6, regexMatchExpEvalName, REG_ICASE  },
    };
    int	i;

    REGEX_DISPLAY (d);

    for (i = 0; i < sizeof (prefix) / sizeof (prefix[0]); i++)
	if (strncmp (value, prefix[i].s, prefix[i].len) == 0)
	    break;

    if (i < sizeof (prefix) / sizeof (prefix[0]))
    {
	regex_t *preg;

	preg = malloc (sizeof (regex_t));
	if (preg)
	{
	    int status;

	    value += prefix[i].len;

	    status = regcomp (preg, value, REG_NOSUB | prefix[i].flags);
	    if (status)
	    {
		char errMsg[1024];

		regerror (status, preg, errMsg, sizeof (errMsg));

		compLogMessage ("regex", CompLogLevelWarn,
				"%s = %s", errMsg, value);

		regfree (preg);
		free (preg);
		preg = NULL;
	    }
	}

	exp->fini     = regexMatchExpFini;
	exp->eval     = prefix[i].eval;
	exp->priv.ptr = preg;
    }
    else
    {
	UNWRAP (rd, d, matchInitExp);
	(*d->matchInitExp) (d, exp, value);
	WRAP (rd, d, matchInitExp, regexMatchInitExp);
    }
}

static char *
regexGetStringProperty (CompWindow *w,
			Atom       propAtom,
			Atom       formatAtom)
{
    Atom	  type;
    unsigned long nItems;
    unsigned long bytesAfter;
    unsigned char *str = NULL;
    int		  format, result;
    char	  *retval;

    result = XGetWindowProperty (w->screen->display->display,
				 w->id, propAtom, 0, LONG_MAX,
				 FALSE, formatAtom, &type, &format, &nItems,
				 &bytesAfter, (unsigned char **) &str);

    if (result != Success)
	return NULL;

    if (type != formatAtom)
    {
	XFree (str);
	return NULL;
    }

    retval = strdup ((char *) str);

    XFree (str);

    return retval;
}

static char *
regexGetWindowTitle (CompWindow *w)
{
    CompDisplay *d = w->screen->display;
    char	*title;

    REGEX_DISPLAY (d);

    title = regexGetStringProperty (w, rd->visibleNameAtom, d->utf8StringAtom);
    if (title)
	return title;

    title = regexGetStringProperty (w, d->wmNameAtom, d->utf8StringAtom);
    if (title)
	return title;

    return regexGetStringProperty (w, XA_WM_NAME, XA_STRING);
}

static void
regexHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    REGEX_DISPLAY (d);

    UNWRAP (rd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (rd, d, handleEvent, regexHandleEvent);

    if (event->type == PropertyNotify)
    {
	CompWindow *w;

	if (event->xproperty.atom == XA_WM_NAME)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		REGEX_WINDOW (w);

		if (rw->title)
		    free (rw->title);

		rw->title = regexGetWindowTitle (w);

		(*d->matchPropertyChanged) (d, w);
	    }
	}
	if (event->xproperty.atom == rd->roleAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		REGEX_WINDOW (w);

		if (rw->role)
		    free (rw->role);

		rw->role = regexGetStringProperty (w, rd->roleAtom, XA_STRING);

		(*d->matchPropertyChanged) (d, w);
	    }
	}
	else if (event->xproperty.atom == XA_WM_CLASS)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		(*d->matchPropertyChanged) (d, w);
	}
    }
}

static Bool
regexRegisterExpHandler (void *closure)
{
    CompDisplay *display = (CompDisplay *) closure;

    (*display->matchExpHandlerChanged) (display);

    REGEX_DISPLAY (display);

    rd->timeoutHandle = 0;

    return FALSE;
}

static Bool
regexInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    RegexDisplay *rd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    rd = malloc (sizeof (RegexDisplay));
    if (!rd)
	return FALSE;

    rd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (rd->screenPrivateIndex < 0)
    {
	free (rd);
	return FALSE;
    }

    rd->roleAtom        = XInternAtom (d->display, "WM_WINDOW_ROLE", 0);
    rd->visibleNameAtom = XInternAtom (d->display, "_NET_WM_VISIBLE_NAME", 0);

    WRAP (rd, d, handleEvent, regexHandleEvent);
    WRAP (rd, d, matchInitExp, regexMatchInitExp);

    d->base.privates[displayPrivateIndex].ptr = rd;

    /* one shot timeout to which will register the expression handler
       after all screens and windows have been initialized */
    rd->timeoutHandle =
	compAddTimeout (0, 0, regexRegisterExpHandler, (void *) d);

    return TRUE;
}

static void
regexFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    REGEX_DISPLAY (d);

    freeScreenPrivateIndex (d, rd->screenPrivateIndex);

    if (rd->timeoutHandle)
	compRemoveTimeout (rd->timeoutHandle);

    UNWRAP (rd, d, handleEvent);
    UNWRAP (rd, d, matchInitExp);

    if (d->base.parent)
	(*d->matchExpHandlerChanged) (d);

    free (rd);
}

static Bool
regexInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    RegexScreen *rs;

    REGEX_DISPLAY (s->display);

    rs = malloc (sizeof (RegexScreen));
    if (!rs)
	return FALSE;

    rs->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (rs->windowPrivateIndex < 0)
    {
	free (rs);
	return FALSE;
    }

    s->base.privates[rd->screenPrivateIndex].ptr = rs;

    return TRUE;
}

static void
regexFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    REGEX_SCREEN (s);

    freeWindowPrivateIndex (s, rs->windowPrivateIndex);

    free (rs);
}

static Bool
regexInitWindow (CompPlugin *p,
		 CompWindow *w)
{
    RegexWindow *rw;

    REGEX_DISPLAY (w->screen->display);
    REGEX_SCREEN (w->screen);

    rw = malloc (sizeof (RegexWindow));
    if (!rw)
	return FALSE;

    rw->title = regexGetWindowTitle (w);
    rw->role  = regexGetStringProperty (w, rd->roleAtom, XA_STRING);

    w->base.privates[rs->windowPrivateIndex].ptr = rw;

    return TRUE;
}

static void
regexFiniWindow (CompPlugin *p,
		 CompWindow *w)
{
    REGEX_WINDOW (w);

    if (rw->title)
	free (rw->title);

    if (rw->role)
	free (rw->role);

    free (rw);
}

static CompBool
regexInitObject (CompPlugin *p,
		 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) regexInitDisplay,
	(InitPluginObjectProc) regexInitScreen,
	(InitPluginObjectProc) regexInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
regexFiniObject (CompPlugin *p,
		 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) regexFiniDisplay,
	(FiniPluginObjectProc) regexFiniScreen,
	(FiniPluginObjectProc) regexFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
regexInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&regexMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&regexMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&regexMetadata, p->vTable->name);

    return TRUE;
}

static void
regexFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&regexMetadata);
}

static CompMetadata *
regexGetMetadata (CompPlugin *plugin)
{
    return &regexMetadata;
}

static CompPluginVTable regexVTable = {
    "regex",
    regexGetMetadata,
    regexInit,
    regexFini,
    regexInitObject,
    regexFiniObject,
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &regexVTable;
}
