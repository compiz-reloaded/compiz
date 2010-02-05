/*
 * Copyright © 2008 Danny Baumann
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Danny Baumann not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Danny Baumann makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DANNY BAUMANN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DENNIS KASPRZYK BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Danny Baumann <dannybaumann@web.de>
 */

#include <compiz-core.h>

static CompMetadata obsMetadata;

static int displayPrivateIndex;

#define OBS_DISPLAY_OPTION_OPACITY_INCREASE_KEY	       0
#define OBS_DISPLAY_OPTION_OPACITY_INCREASE_BUTTON     1
#define OBS_DISPLAY_OPTION_OPACITY_DECREASE_KEY	       2
#define OBS_DISPLAY_OPTION_OPACITY_DECREASE_BUTTON     3
#define OBS_DISPLAY_OPTION_SATURATION_INCREASE_KEY     4
#define OBS_DISPLAY_OPTION_SATURATION_INCREASE_BUTTON  5
#define OBS_DISPLAY_OPTION_SATURATION_DECREASE_KEY     6
#define OBS_DISPLAY_OPTION_SATURATION_DECREASE_BUTTON  7
#define OBS_DISPLAY_OPTION_BRIGHTNESS_INCREASE_KEY     8
#define OBS_DISPLAY_OPTION_BRIGHTNESS_INCREASE_BUTTON  9
#define OBS_DISPLAY_OPTION_BRIGHTNESS_DECREASE_KEY     10
#define OBS_DISPLAY_OPTION_BRIGHTNESS_DECREASE_BUTTON  11
#define OBS_DISPLAY_OPTION_NUM			       12

typedef struct _ObsDisplay
{
    int screenPrivateIndex;

    HandleEventProc            handleEvent;
    MatchExpHandlerChangedProc matchExpHandlerChanged;
    MatchPropertyChangedProc   matchPropertyChanged;

    CompOption opt[OBS_DISPLAY_OPTION_NUM];
} ObsDisplay;

#define OBS_SCREEN_OPTION_OPACITY_STEP        0
#define OBS_SCREEN_OPTION_SATURATION_STEP     1
#define OBS_SCREEN_OPTION_BRIGHTNESS_STEP     2
#define OBS_SCREEN_OPTION_OPACITY_MATCHES     3
#define OBS_SCREEN_OPTION_OPACITY_VALUES      4
#define OBS_SCREEN_OPTION_SATURATION_MATCHES  5
#define OBS_SCREEN_OPTION_SATURATION_VALUES   6
#define OBS_SCREEN_OPTION_BRIGHTNESS_MATCHES  7
#define OBS_SCREEN_OPTION_BRIGHTNESS_VALUES   8
#define OBS_SCREEN_OPTION_NUM                 9

#define MODIFIER_OPACITY     0
#define MODIFIER_SATURATION  1
#define MODIFIER_BRIGHTNESS  2
#define MODIFIER_COUNT       3

typedef struct _ObsScreen
{
    int windowPrivateIndex;

    PaintWindowProc paintWindow;
    DrawWindowProc  drawWindow;

    CompOption *stepOptions[MODIFIER_COUNT];
    CompOption *matchOptions[MODIFIER_COUNT];
    CompOption *valueOptions[MODIFIER_COUNT];

    CompOption opt[OBS_SCREEN_OPTION_NUM];
} ObsScreen;

typedef struct _ObsWindow
{
    int customFactor[MODIFIER_COUNT];
    int matchFactor[MODIFIER_COUNT];

    CompTimeoutHandle updateHandle;
} ObsWindow;

#define GET_OBS_DISPLAY(d) \
    ((ObsDisplay *) (d)->base.privates[displayPrivateIndex].ptr)
#define OBS_DISPLAY(d) \
    ObsDisplay *od = GET_OBS_DISPLAY (d)

#define GET_OBS_SCREEN(s, od) \
    ((ObsScreen *) (s)->base.privates[(od)->screenPrivateIndex].ptr)
#define OBS_SCREEN(s) \
    ObsScreen *os = GET_OBS_SCREEN (s, GET_OBS_DISPLAY (s->display))

#define GET_OBS_WINDOW(w, os) \
    ((ObsWindow *) (w)->base.privates[(os)->windowPrivateIndex].ptr)
#define OBS_WINDOW(w)                                      \
    ObsWindow *ow = GET_OBS_WINDOW  (w,                    \
                    GET_OBS_SCREEN  (w->screen,            \
                    GET_OBS_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static void
changePaintModifier (CompWindow *w,
		     int        modifier,
		     int        direction)
{
    int value;

    OBS_SCREEN (w->screen);
    OBS_WINDOW (w);

    if (w->attrib.override_redirect)
	return;

    if (modifier == MODIFIER_OPACITY && (w->type & CompWindowTypeDesktopMask))
	return;

    value = ow->customFactor[modifier];
    value += os->stepOptions[modifier]->value.i * direction;

    value = MIN (value, 100);
    value = MAX (value, os->stepOptions[modifier]->value.i);

    if (value != ow->customFactor[modifier])
    {
	ow->customFactor[modifier] = value;
	addWindowDamage (w);
    }
}

static void
updatePaintModifier (CompWindow *w,
		     int        modifier)
{
    int lastFactor;

    OBS_WINDOW (w);
    OBS_SCREEN (w->screen);

    lastFactor = ow->customFactor[modifier];

    if ((w->type & CompWindowTypeDesktopMask) && (modifier == MODIFIER_OPACITY))
    {
	ow->customFactor[modifier] = 100;
	ow->matchFactor[modifier]  = 100;
    }
    else
    {
	int        i, min, lastMatchFactor;
	CompOption *matches, *values;

	matches = os->matchOptions[modifier];
	values  = os->valueOptions[modifier];
	min     = MIN (matches->value.list.nValue, values->value.list.nValue);

	lastMatchFactor           = ow->matchFactor[modifier];
	ow->matchFactor[modifier] = 100;

	for (i = 0; i < min; i++)
	{
	    if (matchEval (&matches->value.list.value[i].match, w))
	    {
		ow->matchFactor[modifier] = values->value.list.value[i].i;
		break;
	    }
	}

	if (ow->customFactor[modifier] == lastMatchFactor)
	    ow->customFactor[modifier] = ow->matchFactor[modifier];
    }

    if (ow->customFactor[modifier] != lastFactor)
	addWindowDamage (w);
}

static Bool
alterPaintModifier (CompDisplay     *d,
		    CompAction      *action,
		    CompActionState state,
		    CompOption      *option,
		    int             nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);
    w   = findTopLevelWindowAtDisplay (d, xid);

    if (w)
        changePaintModifier (w, abs (action->priv.val) - 1,
			     (action->priv.val < 0) ? -1 : 1);

    return TRUE;
}

static Bool
obsPaintWindow (CompWindow              *w,
		const WindowPaintAttrib *attrib,
		const CompTransform     *transform,
		Region                  region,
		unsigned int            mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    OBS_SCREEN (s);
    OBS_WINDOW (w);

    if (ow->customFactor[MODIFIER_OPACITY] != 100)
	mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

    UNWRAP (os, s, paintWindow);
    status = (*s->paintWindow) (w, attrib, transform, region, mask);
    WRAP (os, s, paintWindow, obsPaintWindow);

    return status;
}

/* Note: Normally plugins should wrap into PaintWindow to modify opacity,
	 brightness and saturation. As some plugins bypass paintWindow when
	 they draw windows and our custom values always need to be applied,
	 we wrap into DrawWindow here */

static Bool
obsDrawWindow (CompWindow           *w,
	       const CompTransform  *transform,
	       const FragmentAttrib *attrib,
	       Region               region,
	       unsigned int         mask)
{
    CompScreen *s = w->screen;
    Bool       hasCustomFactor = FALSE;
    Bool       status;
    int        i;

    OBS_SCREEN (s);
    OBS_WINDOW (w);

    for (i = 0; i < MODIFIER_COUNT; i++)
	if (ow->customFactor[i] != 100)
	{
	    hasCustomFactor = TRUE;
	    break;
	}

    if (hasCustomFactor)
    {
	FragmentAttrib fragment = *attrib;
	int            factor;

	factor = ow->customFactor[MODIFIER_OPACITY];
	if (factor != 100)
	{
	    fragment.opacity = (int) fragment.opacity * factor / 100;
	    mask |= PAINT_WINDOW_TRANSLUCENT_MASK;
	}

	factor = ow->customFactor[MODIFIER_BRIGHTNESS];
	if (factor != 100)
	    fragment.brightness = (int) fragment.brightness * factor / 100;

	factor = ow->customFactor[MODIFIER_SATURATION];
	if (factor != 100)
	    fragment.saturation = (int) fragment.saturation * factor / 100;

	UNWRAP (os, s, drawWindow);
	status = (*s->drawWindow) (w, transform, &fragment, region, mask);
	WRAP (os, s, drawWindow, obsDrawWindow);
    }
    else
    {
	UNWRAP (os, s, drawWindow);
	status = (*s->drawWindow) (w, transform, attrib, region, mask);
	WRAP (os, s, drawWindow, obsDrawWindow);
    }

    return status;
}

static void
obsMatchExpHandlerChanged (CompDisplay *d)
{
    CompScreen *s;
    CompWindow *w;
    int        i;

    OBS_DISPLAY (d);

    UNWRAP (od, d, matchExpHandlerChanged);
    (*d->matchExpHandlerChanged) (d);
    WRAP (od, d, matchExpHandlerChanged, obsMatchExpHandlerChanged);

    /* match options are up to date after the call to matchExpHandlerChanged */
    for (s = d->screens; s; s = s->next)
	for (w = s->windows; w; w = w->next)
	    for (i = 0; i < MODIFIER_COUNT; i++)
		updatePaintModifier (w, i);
}

static void
obsMatchPropertyChanged (CompDisplay *d,
			 CompWindow  *w)
{
    int i;

    OBS_DISPLAY (d);

    for (i = 0; i < MODIFIER_COUNT; i++)
	updatePaintModifier (w, i);

    UNWRAP (od, d, matchPropertyChanged);
    (*d->matchPropertyChanged) (d, w);
    WRAP (od, d, matchPropertyChanged, obsMatchPropertyChanged);
}

static Bool
obsUpdateWindow (void *closure)
{
    CompWindow *w = (CompWindow *) closure;
    int        i;

    OBS_WINDOW (w);

    for (i = 0; i < MODIFIER_COUNT; i++)
	updatePaintModifier (w, i);

    ow->updateHandle = 0;

    return FALSE;
}

static CompOption *
obsGetDisplayOptions (CompPlugin  *p,
		      CompDisplay *display,
		      int         *count)
{
    OBS_DISPLAY (display);

    *count = NUM_OPTIONS (od);
    return od->opt;
}

static Bool
obsSetDisplayOption (CompPlugin      *p,
		     CompDisplay     *display,
		     char            *name,
		     CompOptionValue *value)
{
    CompOption *o;

    OBS_DISPLAY (display);

    o = compFindOption (od->opt, NUM_OPTIONS (od), name, NULL);
    if (!o)
        return FALSE;

    return compSetDisplayOption (display, o, value);
}

static CompOption *
obsGetScreenOptions (CompPlugin *p,
		     CompScreen *screen,
		     int        *count)
{
    OBS_SCREEN (screen);

    *count = NUM_OPTIONS (os);
    return os->opt;
}

static Bool
obsSetScreenOption (CompPlugin      *p,
		    CompScreen      *s,
		    char            *name,
		    CompOptionValue *value)
{
    CompOption *o;
    int        i;

    OBS_SCREEN (s);

    o = compFindOption (os->opt, NUM_OPTIONS (os), name, NULL);
    if (!o)
        return FALSE;

    for (i = 0; i < MODIFIER_COUNT; i++)
    {
	if (o == os->matchOptions[i])
	{
	    if (compSetOptionList (o, value))
	    {
		CompWindow *w;
		int	   j;

		for (j = 0; j < o->value.list.nValue; j++)
		    matchUpdate (s->display, &o->value.list.value[j].match);

		for (w = s->windows; w; w = w->next)
		    updatePaintModifier (w, i);

		return TRUE;
	    }
	}
	else if (o == os->valueOptions[i])
	{
	    if (compSetOptionList (o, value))
	    {
		CompWindow *w;

		for (w = s->windows; w; w = w->next)
		    updatePaintModifier (w, i);

		return TRUE;
	    }
	}
    }

    return compSetScreenOption (s, o, value);
}

static CompOption *
obsGetObjectOptions (CompPlugin *plugin,
		     CompObject *object,
		     int        *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
       (GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
       (GetPluginObjectOptionsProc) obsGetDisplayOptions,
       (GetPluginObjectOptionsProc) obsGetScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
obsSetObjectOption (CompPlugin      *plugin,
		    CompObject      *object,
		    const char      *name,
		    CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
       (SetPluginObjectOptionProc) 0, /* SetCoreOption */
       (SetPluginObjectOptionProc) obsSetDisplayOption,
       (SetPluginObjectOptionProc) obsSetScreenOption

    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static const CompMetadataOptionInfo obsDisplayOptionInfo[] = {
    { "opacity_increase_key", "key", 0, alterPaintModifier, 0 },
    { "opacity_increase_button", "button", 0, alterPaintModifier, 0 },
    { "opacity_decrease_key", "key", 0, alterPaintModifier, 0 },
    { "opacity_decrease_button", "button", 0, alterPaintModifier, 0 },
    { "saturation_increase_key", "key", 0, alterPaintModifier, 0 },
    { "saturation_increase_button", "button", 0, alterPaintModifier, 0 },
    { "saturation_decrease_key", "key", 0, alterPaintModifier, 0 },
    { "saturation_decrease_button", "button", 0, alterPaintModifier, 0 },
    { "brightness_increase_key", "key", 0, alterPaintModifier, 0 },
    { "brightness_increase_button", "button", 0, alterPaintModifier, 0 },
    { "brightness_decrease_key", "key", 0, alterPaintModifier, 0 },
    { "brightness_decrease_button", "button", 0, alterPaintModifier, 0 }
};

static CompBool
obsInitDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    ObsDisplay *od;
    int        opt;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    od = malloc (sizeof (ObsDisplay));
    if (!od)
        return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &obsMetadata,
					     obsDisplayOptionInfo,
					     od->opt,
					     OBS_DISPLAY_OPTION_NUM))
    {
	free (od);
	return FALSE;
    }

    od->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (od->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, od->opt, OBS_DISPLAY_OPTION_NUM);
        free (od);
        return FALSE;
    }

    opt = OBS_DISPLAY_OPTION_OPACITY_INCREASE_KEY;
    od->opt[opt].value.action.priv.val = MODIFIER_OPACITY + 1;
    opt = OBS_DISPLAY_OPTION_OPACITY_DECREASE_KEY;
    od->opt[opt].value.action.priv.val = -(MODIFIER_OPACITY + 1);
    opt = OBS_DISPLAY_OPTION_OPACITY_INCREASE_BUTTON;
    od->opt[opt].value.action.priv.val = MODIFIER_OPACITY + 1;
    opt = OBS_DISPLAY_OPTION_OPACITY_DECREASE_BUTTON;
    od->opt[opt].value.action.priv.val = -(MODIFIER_OPACITY + 1);

    opt = OBS_DISPLAY_OPTION_SATURATION_INCREASE_KEY;
    od->opt[opt].value.action.priv.val = MODIFIER_SATURATION + 1;
    opt = OBS_DISPLAY_OPTION_SATURATION_DECREASE_KEY;
    od->opt[opt].value.action.priv.val = -(MODIFIER_SATURATION + 1);
    opt = OBS_DISPLAY_OPTION_SATURATION_INCREASE_BUTTON;
    od->opt[opt].value.action.priv.val = MODIFIER_SATURATION + 1;
    opt = OBS_DISPLAY_OPTION_SATURATION_DECREASE_BUTTON;
    od->opt[opt].value.action.priv.val = -(MODIFIER_SATURATION + 1);

    opt = OBS_DISPLAY_OPTION_BRIGHTNESS_INCREASE_KEY;
    od->opt[opt].value.action.priv.val = MODIFIER_BRIGHTNESS + 1;
    opt = OBS_DISPLAY_OPTION_BRIGHTNESS_DECREASE_KEY;
    od->opt[opt].value.action.priv.val = -(MODIFIER_BRIGHTNESS + 1);
    opt = OBS_DISPLAY_OPTION_BRIGHTNESS_INCREASE_BUTTON;
    od->opt[opt].value.action.priv.val = MODIFIER_BRIGHTNESS + 1;
    opt = OBS_DISPLAY_OPTION_BRIGHTNESS_DECREASE_BUTTON;
    od->opt[opt].value.action.priv.val = -(MODIFIER_BRIGHTNESS + 1);

    WRAP (od, d, matchExpHandlerChanged, obsMatchExpHandlerChanged);
    WRAP (od, d, matchPropertyChanged, obsMatchPropertyChanged);

    d->base.privates[displayPrivateIndex].ptr = od;

    return TRUE;
}

static void
obsFiniDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    OBS_DISPLAY (d);

    UNWRAP (od, d, matchExpHandlerChanged);
    UNWRAP (od, d, matchPropertyChanged);

    freeScreenPrivateIndex (d, od->screenPrivateIndex);
    compFiniDisplayOptions (d, od->opt, OBS_DISPLAY_OPTION_NUM);

    free (od);
}

static const CompMetadataOptionInfo obsScreenOptionInfo[] = {
    { "opacity_step", "int", 0, 0, 0 },
    { "saturation_step", "int", 0, 0, 0 },
    { "brightness_step", "int", 0, 0, 0 },
    { "opacity_matches", "list", "<type>match</type>", 0, 0 },
    { "opacity_values", "list", "<type>int</type>", 0, 0 },
    { "saturation_matches", "list", "<type>match</type>", 0, 0 },
    { "saturation_values", "list", "<type>int</type>", 0, 0 },
    { "brightness_matches", "list", "<type>match</type>", 0, 0 },
    { "brightness_values", "list", "<type>int</type>", 0, 0 }
};

static CompBool
obsInitScreen (CompPlugin *p,
	       CompScreen *s)
{
    ObsScreen  *os;
    int        mod;

    OBS_DISPLAY (s->display);

    os = malloc (sizeof (ObsScreen));
    if (!os)
        return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &obsMetadata,
					    obsScreenOptionInfo,
					    os->opt,
					    OBS_SCREEN_OPTION_NUM))
    {
	free (os);
	return FALSE;
    }

    os->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (os->windowPrivateIndex < 0)
    {
	compFiniScreenOptions (s, os->opt, OBS_SCREEN_OPTION_NUM);
        free (os);
        return FALSE;
    }

    mod = MODIFIER_OPACITY;
    os->stepOptions[mod]  = &os->opt[OBS_SCREEN_OPTION_OPACITY_STEP];
    os->matchOptions[mod] = &os->opt[OBS_SCREEN_OPTION_OPACITY_MATCHES];
    os->valueOptions[mod] = &os->opt[OBS_SCREEN_OPTION_OPACITY_VALUES];

    mod = MODIFIER_SATURATION;
    os->stepOptions[mod]  = &os->opt[OBS_SCREEN_OPTION_SATURATION_STEP];
    os->matchOptions[mod] = &os->opt[OBS_SCREEN_OPTION_SATURATION_MATCHES];
    os->valueOptions[mod] = &os->opt[OBS_SCREEN_OPTION_SATURATION_VALUES];

    mod = MODIFIER_BRIGHTNESS;
    os->stepOptions[mod]  = &os->opt[OBS_SCREEN_OPTION_BRIGHTNESS_STEP];
    os->matchOptions[mod] = &os->opt[OBS_SCREEN_OPTION_BRIGHTNESS_MATCHES];
    os->valueOptions[mod] = &os->opt[OBS_SCREEN_OPTION_BRIGHTNESS_VALUES];

    s->base.privates[od->screenPrivateIndex].ptr = os;

    WRAP (os, s, paintWindow, obsPaintWindow);
    WRAP (os, s, drawWindow, obsDrawWindow);

    return TRUE;
}

static void
obsFiniScreen (CompPlugin *p,
	       CompScreen *s)
{
    OBS_SCREEN (s);

    UNWRAP (os, s, paintWindow);
    UNWRAP (os, s, drawWindow);

    damageScreen (s);

    compFiniScreenOptions (s, os->opt, OBS_SCREEN_OPTION_NUM);

    free (os);
}

static CompBool
obsInitWindow (CompPlugin *p,
	       CompWindow *w)
{
    ObsWindow *ow;
    int       i;

    OBS_SCREEN (w->screen);

    ow = malloc (sizeof (ObsWindow));
    if (!ow)
        return FALSE;

    for (i = 0; i < MODIFIER_COUNT; i++)
    {
	ow->customFactor[i] = 100;
	ow->matchFactor[i]  = 100;
    }

    /* defer initializing the factors from window matches as match evalution
       means wrapped function calls */
    ow->updateHandle = compAddTimeout (0, 0, obsUpdateWindow, w);

    w->base.privates[os->windowPrivateIndex].ptr = ow;

    return TRUE;
}

static void
obsFiniWindow (CompPlugin *p,
	       CompWindow *w)
{
    OBS_WINDOW (w);

    if (ow->updateHandle)
	compRemoveTimeout (ow->updateHandle);

    free (ow);
}

static CompBool
obsInitObject (CompPlugin *p,
	       CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
       (InitPluginObjectProc) 0, /* InitCore */
       (InitPluginObjectProc) obsInitDisplay,
       (InitPluginObjectProc) obsInitScreen,
       (InitPluginObjectProc) obsInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));

}

static void
obsFiniObject (CompPlugin *p,
	       CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
       (FiniPluginObjectProc) 0, /* FiniCore */
       (FiniPluginObjectProc) obsFiniDisplay,
       (FiniPluginObjectProc) obsFiniScreen,
       (FiniPluginObjectProc) obsFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompBool
obsInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&obsMetadata,
					 p->vTable->name,
					 obsDisplayOptionInfo,
					 OBS_DISPLAY_OPTION_NUM,
					 obsScreenOptionInfo,
					 OBS_SCREEN_OPTION_NUM))
    {
	return FALSE;
    }

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&obsMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&obsMetadata, p->vTable->name);

    return TRUE;
}

static void
obsFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);

    compFiniMetadata (&obsMetadata);
}

static CompMetadata *
obsGetMetadata (CompPlugin *plugin)
{
    return &obsMetadata;
}

CompPluginVTable obsVTable = {
    "obs",
    obsGetMetadata,
    obsInit,
    obsFini,
    obsInitObject,
    obsFiniObject,
    obsGetObjectOptions,
    obsSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &obsVTable;
}
