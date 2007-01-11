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

#include <compiz.h>

#define BLUR_SPEED_DEFAULT    3.5f
#define BLUR_SPEED_MIN        0.1f
#define BLUR_SPEED_MAX       10.0f
#define BLUR_SPEED_PRECISION  0.1f

#define BLUR_FOCUS_BLUR_DEFAULT FALSE

#define BLUR_PULSE_DEFAULT FALSE

static char *winType[] = {
    N_("Toolbar"),
    N_("Menu"),
    N_("Utility"),
    N_("Normal"),
    N_("Dialog"),
    N_("ModalDialog")
};
#define N_WIN_TYPE (sizeof (winType) / sizeof (winType[0]))

static char *filterString[] = {
    N_("4xBilinear"),
    N_("12xBilinear")
};
static int  nFilterString = sizeof (filterString) / sizeof (filterString[0]);

#define BLUR_FILTER_DEFAULT (filterString[0])

typedef enum {
    BlurFilter4xBilinear,
    BlurFilter12xBilinear
} BlurFilter;

typedef struct _BlurFunction {
    struct _BlurFunction *next;

    int handle;
    int target;
    int param;
} BlurFunction;

static int displayPrivateIndex;

#define BLUR_DISPLAY_OPTION_PULSE 0
#define BLUR_DISPLAY_OPTION_NUM   1

typedef struct _BlurDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[BLUR_DISPLAY_OPTION_NUM];
} BlurDisplay;

#define BLUR_SCREEN_OPTION_BLUR_SPEED  0
#define BLUR_SCREEN_OPTION_WINDOW_TYPE 1
#define BLUR_SCREEN_OPTION_FOCUS_BLUR  2
#define BLUR_SCREEN_OPTION_FILTER      3
#define BLUR_SCREEN_OPTION_NUM	       4

typedef struct _BlurScreen {
    int	windowPrivateIndex;

    CompOption opt[BLUR_SCREEN_OPTION_NUM];

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    DrawWindowTextureProc  drawWindowTexture;

    int  wMask;
    int	 blurTime;
    Bool moreBlur;

    BlurFilter filter;

    BlurFunction *blurFunctions;
} BlurScreen;

typedef struct _BlurWindow {
    int  blur;
    Bool pulse;
} BlurWindow;

#define GET_BLUR_DISPLAY(d)				     \
    ((BlurDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define BLUR_DISPLAY(d)			   \
    BlurDisplay *bd = GET_BLUR_DISPLAY (d)

#define GET_BLUR_SCREEN(s, bd)					 \
    ((BlurScreen *) (s)->privates[(bd)->screenPrivateIndex].ptr)

#define BLUR_SCREEN(s)							\
    BlurScreen *bs = GET_BLUR_SCREEN (s, GET_BLUR_DISPLAY (s->display))

#define GET_BLUR_WINDOW(w, bs)					 \
    ((BlurWindow *) (w)->privates[(bs)->windowPrivateIndex].ptr)

#define BLUR_WINDOW(w)					     \
    BlurWindow *bw = GET_BLUR_WINDOW  (w,		     \
		     GET_BLUR_SCREEN  (w->screen,	     \
		     GET_BLUR_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static BlurFilter
blurFilterFromString (CompOptionValue *value)
{
    if (strcasecmp (value->s, "12xbilinear") == 0)
	return BlurFilter12xBilinear;

    return BlurFilter4xBilinear;
}

static void
blurDestroyFragmentFunctions (CompScreen *s)
{
    BlurFunction *function, *next;

    BLUR_SCREEN (s);

    function = bs->blurFunctions;
    while (function)
    {
	destroyFragmentFunction (s, function->handle);

	next = function->next;
	free (function);
	function = next;
    }

    bs->blurFunctions = NULL;
}

static CompOption *
blurGetScreenOptions (CompScreen *screen,
		      int	 *count)
{
    BLUR_SCREEN (screen);

    *count = NUM_OPTIONS (bs);
    return bs->opt;
}

static Bool
blurSetScreenOption (CompScreen      *screen,
		     char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    BLUR_SCREEN (screen);

    o = compFindOption (bs->opt, NUM_OPTIONS (bs), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case BLUR_SCREEN_OPTION_BLUR_SPEED:
	if (compSetFloatOption (o, value))
	{
	    bs->blurTime = 1000.0f / o->value.f;
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_WINDOW_TYPE:
	if (compSetOptionList (o, value))
	{
	    bs->wMask = compWindowTypeMaskFromStringList (&o->value);
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_FOCUS_BLUR:
	if (compSetBoolOption (o, value))
	{
	    bs->moreBlur = TRUE;
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_FILTER:
	if (compSetStringOption (o, value))
	{
	    bs->filter = blurFilterFromString (&o->value);
	    bs->moreBlur = TRUE;
	    blurDestroyFragmentFunctions (screen);
	    damageScreen (screen);
	    return TRUE;
	}
    default:
	break;
    }

    return FALSE;
}

static void
blurScreenInitOptions (BlurScreen *bs)
{
    CompOption *o;
    int	       i;

    o = &bs->opt[BLUR_SCREEN_OPTION_BLUR_SPEED];
    o->name		= "blur_speed";
    o->shortDesc	= N_("Blur Speed");
    o->longDesc		= N_("Window blur speed");
    o->type		= CompOptionTypeFloat;
    o->value.f		= BLUR_SPEED_DEFAULT;
    o->rest.f.min	= BLUR_SPEED_MIN;
    o->rest.f.max	= BLUR_SPEED_MAX;
    o->rest.f.precision = BLUR_SPEED_PRECISION;

    o = &bs->opt[BLUR_SCREEN_OPTION_WINDOW_TYPE];
    o->name	         = "window_types";
    o->shortDesc         = N_("Window Types");
    o->longDesc	         = N_("Window types that should be blurred");
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_WIN_TYPE;
    o->value.list.value  = malloc (sizeof (CompOptionValue) * N_WIN_TYPE);
    for (i = 0; i < N_WIN_TYPE; i++)
	o->value.list.value[i].s = strdup (winType[i]);
    o->rest.s.string     = windowTypeString;
    o->rest.s.nString    = nWindowTypeString;

    bs->wMask = compWindowTypeMaskFromStringList (&o->value);

    o = &bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR];
    o->name	 = "focus_blur";
    o->shortDesc = N_("Focus Blur");
    o->longDesc	 = N_("Blur windows that doesn't have focus");
    o->type	 = CompOptionTypeBool;
    o->value.b   = BLUR_FOCUS_BLUR_DEFAULT;

    o = &bs->opt[BLUR_SCREEN_OPTION_FILTER];
    o->name	         = "filter";
    o->shortDesc         = N_("Blur Filter");
    o->longDesc	         = N_("Filter method used for blurring");
    o->type	         = CompOptionTypeString;
    o->value.s		 = strdup (BLUR_FILTER_DEFAULT);
    o->rest.s.string     = filterString;
    o->rest.s.nString    = nFilterString;

    bs->filter = blurFilterFromString (&o->value);
}

static void
blurPreparePaintScreen (CompScreen *s,
			int	   msSinceLastPaint)
{
    BLUR_SCREEN (s);

    if (bs->moreBlur)
    {
	CompWindow  *w;
	int	    steps;
	Bool        focus = bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR].value.b;

	steps = (msSinceLastPaint * 0xffff) / bs->blurTime;
	if (steps < 12)
	    steps = 12;

	bs->moreBlur = FALSE;

	for (w = s->windows; w; w = w->next)
	{
	    if (bs->wMask & w->type)
	    {
		BLUR_WINDOW (w);

		if (!bw->pulse && (!focus || w->id == s->display->activeWindow))
		{
		    if (bw->blur)
		    {
			bw->blur -= steps;
			if (bw->blur > 0)
			    bs->moreBlur = TRUE;
			else
			    bw->blur = 0;
		    }
		}
		else
		{
		    if (bw->blur < 0xffff)
		    {
			if (bw->pulse)
			{
			    bw->blur += steps * 2;

			    if (bw->blur >= 0xffff)
			    {
				bw->blur = 0xffff - 1;
				bw->pulse = FALSE;
			    }

			    bs->moreBlur = TRUE;
			}
			else
			{
			    bw->blur += steps;
			    if (bw->blur < 0xffff)
				bs->moreBlur = TRUE;
			    else
				bw->blur = 0xffff;
			}
		    }
		}
	    }
	}
    }

    UNWRAP (bs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (bs, s, preparePaintScreen, blurPreparePaintScreen);
}

static void
blurDonePaintScreen (CompScreen *s)
{
    BLUR_SCREEN (s);

    if (bs->moreBlur)
    {
	CompWindow *w;

	for (w = s->windows; w; w = w->next)
	{
	    if (bs->wMask & w->type)
	    {
		BLUR_WINDOW (w);

		if (bw->blur > 0 && bw->blur < 0xffff)
		    addWindowDamage (w);
	    }
	}
    }

    UNWRAP (bs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (bs, s, donePaintScreen, blurDonePaintScreen);
}

static int
getBlurFragmentFunction (CompScreen  *s,
			 CompTexture *texture,
			 int	     param)
{
    BlurFunction     *function;
    CompFunctionData *data;
    int		     target;

    BLUR_SCREEN (s);

    if (texture->target == GL_TEXTURE_2D)
	target = COMP_FETCH_TARGET_2D;
    else
	target = COMP_FETCH_TARGET_RECT;

    for (function = bs->blurFunctions; function; function = function->next)
	if (function->param == param && function->target == target)
	    return function->handle;

    data = createFunctionData ();
    if (data)
    {
	static char *temp[] = { "offset0", "offset1", "sum" };
	int	    i, handle = 0;
	char	    str[1024];
	Bool	    ok = TRUE;

	for (i = 0; i < sizeof (temp) / sizeof (temp[0]); i++)
	    ok &= addTempHeaderOpToFunctionData (data, temp[i]);

	snprintf (str, 1024,
		  "MUL offset0, program.env[%d].xyzw, { 1.0, 1.0, 0.0, 0.0 };"
		  "MUL offset1, program.env[%d].zwww, { 1.0, 1.0, 0.0, 0.0 };",
		  param, param);

	ok &= addDataOpToFunctionData (data, str);

	switch (bs->filter) {
	case BlurFilter4xBilinear:
	    ok &= addFetchOpToFunctionData (data, "output", "offset0", target);
	    ok &= addDataOpToFunctionData (data, "MUL sum, output, 0.25;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset0", target);
	    ok &= addDataOpToFunctionData (data, "MAD sum, output, 0.25, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "offset1", target);
	    ok &= addDataOpToFunctionData (data, "MAD sum, output, 0.25, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset1", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD output, output, 0.25, sum;");
	    break;
	case BlurFilter12xBilinear:
	    ok &= addFetchOpToFunctionData (data, "output", "offset0", target);
	    ok &= addDataOpToFunctionData (data, "MUL sum, output, 0.125;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset0", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD sum, output, 0.125, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "offset1", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD sum, output, 0.125, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset1", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD sum, output, 0.125, sum;");

	    snprintf (str, 1024,
		      "MUL offset0, program.env[%d].xyzw, "
		      "{ 3.0, 3.0, 0.0, 0.0 };"
		      "MUL offset1, program.env[%d].zwww, "
		      "{ 3.0, 3.0, 0.0, 0.0 };",
		      param, param);

	    ok &= addDataOpToFunctionData (data, str);
	    ok &= addFetchOpToFunctionData (data, "output", "offset0", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD sum, output, 0.05, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset0", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD sum, output, 0.05, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "offset1", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD sum, output, 0.05, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset1", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD sum, output, 0.05, sum;");

	    snprintf (str, 1024,
		      "MUL offset0, offset0, { 1.0, 0.0, 0.0, 0.0 };"
		      "MUL offset1, offset1, { 0.0, 1.0, 0.0, 0.0 };");

	    ok &= addDataOpToFunctionData (data, str);
	    ok &= addFetchOpToFunctionData (data, "output", "offset0", target);
	    ok &= addDataOpToFunctionData (data, "MAD sum, output, 0.1, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset0", target);
	    ok &= addDataOpToFunctionData (data, "MAD sum, output, 0.1, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "offset1", target);
	    ok &= addDataOpToFunctionData (data, "MAD sum, output, 0.1, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset1", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD output, output, 0.1, sum;");
	    break;
	}

	if (!ok)
	{
	    destroyFunctionData (data);
	    return 0;
	}

	function = malloc (sizeof (BlurFunction));
	if (function)
	{
	    handle = createFragmentFunction (s, "blur", data);

	    function->handle = handle;
	    function->target = target;
	    function->param  = param;

	    function->next = bs->blurFunctions;
	    bs->blurFunctions = function;
	}

	destroyFunctionData (data);

	return handle;
    }

    return 0;
}

static void
blurDrawWindowTexture (CompWindow	       *w,
		       CompTexture	       *texture,
		       const WindowPaintAttrib *attrib,
		       const FragmentAttrib    *fAttrib,
		       unsigned int	       mask)
{
    BLUR_SCREEN (w->screen);
    BLUR_WINDOW (w);

    if (bw->blur)
    {
	FragmentAttrib fa = *fAttrib;
	int	       param, function;

	param = allocFragmentParameter (&fa);

	function = getBlurFragmentFunction (w->screen, texture, param);
	if (function)
	{
	    GLfloat dx, dy;

	    addFragmentFunction (&fa, function);

	    dx = ((texture->matrix.xx / 2.1f) * bw->blur) / 65535.0f;
	    dy = ((texture->matrix.yy / 2.1f) * bw->blur) / 65535.0f;

	    (*w->screen->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB, param,
						 dx, dy, dx, -dy);
	}

	/* bi-linear filtering is required */
	mask |= PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK;

	UNWRAP (bs, w->screen, drawWindowTexture);
	(*w->screen->drawWindowTexture) (w, texture, attrib, &fa, mask);
	WRAP (bs, w->screen, drawWindowTexture, blurDrawWindowTexture);
    }
    else
    {
	UNWRAP (bs, w->screen, drawWindowTexture);
	(*w->screen->drawWindowTexture) (w, texture, attrib, fAttrib, mask);
	WRAP (bs, w->screen, drawWindowTexture, blurDrawWindowTexture);
    }
}

static void
blurHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    Window activeWindow = 0;

    BLUR_DISPLAY (d);

    if (event->type == PropertyNotify &&
	event->xproperty.atom == d->winActiveAtom)
	activeWindow = d->activeWindow;

    UNWRAP (bd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (bd, d, handleEvent, blurHandleEvent);

    if (event->type == PropertyNotify		  &&
	event->xproperty.atom == d->winActiveAtom &&
	d->activeWindow != activeWindow)
    {
	CompWindow *w;

	w = findWindowAtDisplay (d, activeWindow);
	if (w)
	{
	    BLUR_SCREEN (w->screen);

	    if (bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR].value.b)
	    {
		addWindowDamage (w);
		bs->moreBlur = TRUE;
	    }
	}

	w = findWindowAtDisplay (d, d->activeWindow);
	if (w)
	{
	    BLUR_SCREEN (w->screen);

	    if (bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR].value.b)
	    {
		addWindowDamage (w);
		bs->moreBlur = TRUE;
	    }
	}
    }
}

static CompOption *
blurGetDisplayOptions (CompDisplay *display,
		       int	   *count)
{
    BLUR_DISPLAY (display);

    *count = NUM_OPTIONS (bd);
    return bd->opt;
}

static Bool
blurSetDisplayOption (CompDisplay     *display,
		      char	      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    BLUR_DISPLAY (display);

    o = compFindOption (bd->opt, NUM_OPTIONS (bd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case BLUR_DISPLAY_OPTION_PULSE:
	if (setDisplayAction (display, o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static Bool
blurPulse (CompDisplay     *d,
	   CompAction      *action,
	   CompActionState state,
	   CompOption      *option,
	   int	           nOption)
{
    CompWindow *w;
    int	       xid;

    xid = getIntOptionNamed (option, nOption, "window", d->activeWindow);

    w = findWindowAtDisplay (d, xid);
    if (w)
    {
	BLUR_SCREEN (w->screen);
	BLUR_WINDOW (w);

	bw->pulse    = TRUE;
	bs->moreBlur = TRUE;

	addWindowDamage (w);
    }

    return FALSE;
}

static void
blurDisplayInitOptions (BlurDisplay *bd)
{
    CompOption *o;

    o = &bd->opt[BLUR_DISPLAY_OPTION_PULSE];
    o->name		      = "pulse";
    o->shortDesc	      = N_("Pulse");
    o->longDesc		      = N_("Pulse effect");
    o->type		      = CompOptionTypeAction;
    o->value.action.initiate  = blurPulse;
    o->value.action.terminate = 0;
    o->value.action.bell      = BLUR_PULSE_DEFAULT;
    o->value.action.edgeMask  = 0;
    o->value.action.type      = CompBindingTypeNone;
    o->value.action.state     = CompActionStateInitBell;
}

static Bool
blurInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    BlurDisplay *bd;

    bd = malloc (sizeof (BlurDisplay));
    if (!bd)
	return FALSE;

    bd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (bd->screenPrivateIndex < 0)
    {
	free (bd);
	return FALSE;
    }

    WRAP (bd, d, handleEvent, blurHandleEvent);

    blurDisplayInitOptions (bd);

    d->privates[displayPrivateIndex].ptr = bd;

    return TRUE;
}

static void
blurFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    BLUR_DISPLAY (d);

    freeScreenPrivateIndex (d, bd->screenPrivateIndex);

    UNWRAP (bd, d, handleEvent);

    free (bd);
}

static Bool
blurInitScreen (CompPlugin *p,
		CompScreen *s)
{
    BlurScreen *bs;

    BLUR_DISPLAY (s->display);

    bs = malloc (sizeof (BlurScreen));
    if (!bs)
	return FALSE;

    bs->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (bs->windowPrivateIndex < 0)
    {
	free (bs);
	return FALSE;
    }

    bs->blurFunctions = NULL;
    bs->blurTime      = 1000.0f / BLUR_SPEED_DEFAULT;
    bs->moreBlur      = FALSE;

    blurScreenInitOptions (bs);

    WRAP (bs, s, preparePaintScreen, blurPreparePaintScreen);
    WRAP (bs, s, donePaintScreen, blurDonePaintScreen);
    WRAP (bs, s, drawWindowTexture, blurDrawWindowTexture);

    s->privates[bd->screenPrivateIndex].ptr = bs;

    return TRUE;
}

static void
blurFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    BLUR_SCREEN (s);

    blurDestroyFragmentFunctions (s);
    damageScreen (s);

    freeWindowPrivateIndex (s, bs->windowPrivateIndex);

    UNWRAP (bs, s, preparePaintScreen);
    UNWRAP (bs, s, donePaintScreen);
    UNWRAP (bs, s, drawWindowTexture);

    free (bs);
}

static Bool
blurInitWindow (CompPlugin *p,
		CompWindow *w)
{
    BlurWindow *bw;

    BLUR_SCREEN (w->screen);

    bw = malloc (sizeof (BlurWindow));
    if (!bw)
	return FALSE;

    bw->blur  = 0;
    bw->pulse = FALSE;

    w->privates[bs->windowPrivateIndex].ptr = bw;

    return TRUE;
}

static void
blurFiniWindow (CompPlugin *p,
		CompWindow *w)
{
    BLUR_WINDOW (w);

    free (bw);
}

static Bool
blurInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
blurFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
blurGetVersion (CompPlugin *plugin,
		int	   version)
{
    return ABIVERSION;
}

static CompPluginVTable blurVTable = {
    "blur",
    N_("Blur Windows"),
    N_("Blur windows"),
    blurGetVersion,
    blurInit,
    blurFini,
    blurInitDisplay,
    blurFiniDisplay,
    blurInitScreen,
    blurFiniScreen,
    blurInitWindow,
    blurFiniWindow,
    blurGetDisplayOptions,
    blurSetDisplayOption,
    blurGetScreenOptions,
    blurSetScreenOption,
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &blurVTable;
}
