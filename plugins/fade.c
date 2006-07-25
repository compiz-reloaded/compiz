/*
 * Copyright © 2005 Novell, Inc.
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

#define FADE_SPEED_DEFAULT    5.0f
#define FADE_SPEED_MIN        0.1f
#define FADE_SPEED_MAX       10.0f
#define FADE_SPEED_PRECISION  0.1f

static char *winType[] = {
    N_("Dock"),
    N_("Toolbar"),
    N_("Menu"),
    N_("Utility"),
    N_("Splash"),
    N_("Normal"),
    N_("Dialog"),
    N_("ModalDialog"),
    N_("DropdownMenu"),
    N_("PopupMenu"),
    N_("Tooltip"),
    N_("Notification"),
    N_("Combo"),
    N_("Dnd"),
    N_("Unknown")
};
#define N_WIN_TYPE (sizeof (winType) / sizeof (winType[0]))

#define FADE_VISUAL_BELL_DEFAULT FALSE

#define FADE_FULLSCREEN_VISUAL_BELL_DEFAULT FALSE

static int displayPrivateIndex;

typedef struct _FadeDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
    int		    displayModals;
} FadeDisplay;

#define FADE_SCREEN_OPTION_FADE_SPEED		  0
#define FADE_SCREEN_OPTION_WINDOW_TYPE		  1
#define FADE_SCREEN_OPTION_VISUAL_BELL		  2
#define FADE_SCREEN_OPTION_FULLSCREEN_VISUAL_BELL 3
#define FADE_SCREEN_OPTION_NUM			  4

typedef struct _FadeScreen {
    int			   windowPrivateIndex;
    int			   fadeTime;
    int			   steps;

    CompOption opt[FADE_SCREEN_OPTION_NUM];

    PreparePaintScreenProc preparePaintScreen;
    PaintWindowProc	   paintWindow;
    DamageWindowRectProc   damageWindowRect;
    FocusWindowProc	   focusWindow;
    WindowResizeNotifyProc windowResizeNotify;

    int wMask;
} FadeScreen;

typedef struct _FadeWindow {
    GLushort opacity;
    GLushort brightness;
    GLushort saturation;

    int dModal;

    int destroyCnt;
    int unmapCnt;

    Bool shaded;
} FadeWindow;

#define GET_FADE_DISPLAY(d)				     \
    ((FadeDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define FADE_DISPLAY(d)			   \
    FadeDisplay *fd = GET_FADE_DISPLAY (d)

#define GET_FADE_SCREEN(s, fd)					 \
    ((FadeScreen *) (s)->privates[(fd)->screenPrivateIndex].ptr)

#define FADE_SCREEN(s)							\
    FadeScreen *fs = GET_FADE_SCREEN (s, GET_FADE_DISPLAY (s->display))

#define GET_FADE_WINDOW(w, fs)				         \
    ((FadeWindow *) (w)->privates[(fs)->windowPrivateIndex].ptr)

#define FADE_WINDOW(w)					     \
    FadeWindow *fw = GET_FADE_WINDOW  (w,		     \
		     GET_FADE_SCREEN  (w->screen,	     \
		     GET_FADE_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
fadeGetScreenOptions (CompScreen *screen,
		      int	 *count)
{
    FADE_SCREEN (screen);

    *count = NUM_OPTIONS (fs);
    return fs->opt;
}

static Bool
fadeSetScreenOption (CompScreen      *screen,
		     char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    FADE_SCREEN (screen);

    o = compFindOption (fs->opt, NUM_OPTIONS (fs), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case FADE_SCREEN_OPTION_FADE_SPEED:
	if (compSetFloatOption (o, value))
	{
	    fs->fadeTime = 1000.0f / o->value.f;
	    return TRUE;
	}
	break;
    case FADE_SCREEN_OPTION_WINDOW_TYPE:
	if (compSetOptionList (o, value))
	{
	    fs->wMask = compWindowTypeMaskFromStringList (&o->value);
	    fs->wMask &= ~CompWindowTypeDesktopMask;
	    return TRUE;
	}
	break;
    case FADE_SCREEN_OPTION_VISUAL_BELL:
    case FADE_SCREEN_OPTION_FULLSCREEN_VISUAL_BELL:
	if (compSetBoolOption (o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
fadeScreenInitOptions (FadeScreen *fs)
{
    CompOption *o;
    int	       i;

    o = &fs->opt[FADE_SCREEN_OPTION_FADE_SPEED];
    o->name		= "fade_speed";
    o->shortDesc	= N_("Fade Speed");
    o->longDesc		= N_("Window fade speed");
    o->type		= CompOptionTypeFloat;
    o->value.f		= FADE_SPEED_DEFAULT;
    o->rest.f.min	= FADE_SPEED_MIN;
    o->rest.f.max	= FADE_SPEED_MAX;
    o->rest.f.precision = FADE_SPEED_PRECISION;

    o = &fs->opt[FADE_SCREEN_OPTION_WINDOW_TYPE];
    o->name	         = "window_types";
    o->shortDesc         = N_("Window Types");
    o->longDesc	         = N_("Window types that should be fading");
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_WIN_TYPE;
    o->value.list.value  = malloc (sizeof (CompOptionValue) * N_WIN_TYPE);
    for (i = 0; i < N_WIN_TYPE; i++)
	o->value.list.value[i].s = strdup (winType[i]);
    o->rest.s.string     = windowTypeString;
    o->rest.s.nString    = nWindowTypeString;

    fs->wMask = compWindowTypeMaskFromStringList (&o->value);

    o = &fs->opt[FADE_SCREEN_OPTION_VISUAL_BELL];
    o->name	  = "visual_bell";
    o->shortDesc  = N_("Visual Bell");
    o->longDesc	  = N_("Fade effect on system beep");
    o->type	  = CompOptionTypeBool;
    o->value.b    = FADE_VISUAL_BELL_DEFAULT;

    o = &fs->opt[FADE_SCREEN_OPTION_FULLSCREEN_VISUAL_BELL];
    o->name	  = "fullscreen_visual_bell";
    o->shortDesc  = N_("Fullscreen Visual Bell");
    o->longDesc	  = N_("Fullscreen fade effect on system beep");
    o->type	  = CompOptionTypeBool;
    o->value.b    = FADE_FULLSCREEN_VISUAL_BELL_DEFAULT;
}

static void
fadePreparePaintScreen (CompScreen *s,
			int	   msSinceLastPaint)
{
    FADE_SCREEN (s);

    fs->steps = (msSinceLastPaint * OPAQUE) / fs->fadeTime;
    if (fs->steps < 12)
	fs->steps = 12;

    UNWRAP (fs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (fs, s, preparePaintScreen, fadePreparePaintScreen);
}

static void
fadeWindowStop (CompWindow *w)
{
    FADE_WINDOW (w);

    while (fw->unmapCnt)
    {
	unmapWindow (w);
	fw->unmapCnt--;
    }

    while (fw->destroyCnt)
    {
	destroyWindow (w);
	fw->destroyCnt--;
    }
}

static Bool
fadePaintWindow (CompWindow		 *w,
		 const WindowPaintAttrib *attrib,
		 Region			 region,
		 unsigned int		 mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    FADE_SCREEN (s);
    FADE_WINDOW (w);

    if (!w->screen->canDoSlightlySaturated)
	fw->saturation = attrib->saturation;

    if (fw->opacity    != attrib->opacity    ||
	fw->brightness != attrib->brightness ||
	fw->saturation != attrib->saturation)
    {
	GLint opacity;
	GLint brightness;
	GLint saturation;

	opacity = fw->opacity;
	if (attrib->opacity > fw->opacity)
	{
	    opacity = fw->opacity + fs->steps;
	    if (opacity > attrib->opacity)
		opacity = attrib->opacity;
	}
	else if (attrib->opacity < fw->opacity)
	{
	    if (w->type & CompWindowTypeUnknownMask)
		opacity = fw->opacity - (fs->steps >> 1);
	    else
		opacity = fw->opacity - fs->steps;

	    if (opacity < attrib->opacity)
		opacity = attrib->opacity;
	}

	brightness = fw->brightness;
	if (attrib->brightness > fw->brightness)
	{
	    brightness = fw->brightness + (fs->steps / 12);
	    if (brightness > attrib->brightness)
		brightness = attrib->brightness;
	}
	else if (attrib->brightness < fw->brightness)
	{
	    brightness = fw->brightness - (fs->steps / 12);
	    if (brightness < attrib->brightness)
		brightness = attrib->brightness;
	}

	saturation = fw->saturation;
	if (attrib->saturation > fw->saturation)
	{
	    saturation = fw->saturation + (fs->steps / 6);
	    if (saturation > attrib->saturation)
		saturation = attrib->saturation;
	}
	else if (attrib->saturation < fw->saturation)
	{
	    saturation = fw->saturation - (fs->steps / 6);
	    if (saturation < attrib->saturation)
		saturation = attrib->saturation;
	}

	if (opacity > 0)
	{
	    WindowPaintAttrib fAttrib = *attrib;

	    fAttrib.opacity    = opacity;
	    fAttrib.brightness = brightness;
	    fAttrib.saturation = saturation;

	    UNWRAP (fs, s, paintWindow);
	    status = (*s->paintWindow) (w, &fAttrib, region, mask);
	    WRAP (fs, s, paintWindow, fadePaintWindow);

	    if (status)
	    {
		fw->opacity    = opacity;
		fw->brightness = brightness;
		fw->saturation = saturation;

		if (opacity    != attrib->opacity    ||
		    brightness != attrib->brightness ||
		    saturation != attrib->saturation)
		    addWindowDamage (w);
	    }
	}
	else
	{
	    fw->opacity = 0;

	    fadeWindowStop (w);

	    return (mask & PAINT_WINDOW_SOLID_MASK) ? FALSE : TRUE;
	}
    }
    else
    {
	UNWRAP (fs, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, region, mask);
	WRAP (fs, s, paintWindow, fadePaintWindow);
    }

    return status;
}

static void
fadeAddDisplayModal (CompDisplay *d,
		     CompWindow  *w)
{
    FADE_DISPLAY (d);
    FADE_WINDOW (w);

    if (!(w->state & CompWindowStateDisplayModalMask))
	return;

    if (fw->dModal)
	return;

    fw->dModal = 1;

    fd->displayModals++;
    if (fd->displayModals == 1)
    {
	CompScreen *s;

	for (s = d->screens; s; s = s->next)
	{
	    for (w = s->windows; w; w = w->next)
	    {
		FADE_WINDOW (w);

		if (fw->dModal)
		    continue;

		w->paint.brightness = 0xa8a8;
		w->paint.saturation = 0;
	    }

	    damageScreen (s);
	}
    }
}

static void
fadeRemoveDisplayModal (CompDisplay *d,
			CompWindow  *w)
{
    FADE_DISPLAY (d);
    FADE_WINDOW (w);

    if (!fw->dModal)
	return;

    fw->dModal = 0;

    fd->displayModals--;
    if (fd->displayModals == 0)
    {
	CompScreen *s;

	for (s = d->screens; s; s = s->next)
	{
	    for (w = s->windows; w; w = w->next)
	    {
		FADE_WINDOW (w);

		if (fw->dModal)
		    continue;

		if (w->alive)
		{
		    w->paint.brightness = w->brightness;
		    w->paint.saturation = w->saturation;
		}
	    }

	    damageScreen (s);
	}
    }
}

static void
fadeHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompWindow *w;

    FADE_DISPLAY (d);

    switch (event->type) {
    case DestroyNotify:
	w = findWindowAtDisplay (d, event->xdestroywindow.window);
	if (w)
	{
	    FADE_SCREEN (w->screen);

	    if (w->texture.pixmap && (fs->wMask & w->type))
	    {
		FADE_WINDOW (w);

		w->paint.opacity = 0;

		if (fw->opacity == 0xffff)
		    fw->opacity = 0xfffe;

		fw->destroyCnt++;
		w->destroyRefCnt++;

		addWindowDamage (w);
	    }

	    fadeRemoveDisplayModal (d, w);
	}
	break;
    case UnmapNotify:
	w = findWindowAtDisplay (d, event->xunmap.window);
	if (w)
	{
	    FADE_SCREEN (w->screen);
	    FADE_WINDOW (w);

	    fw->shaded = w->shaded;

	    if (!fw->shaded && w->texture.pixmap && (fs->wMask & w->type))
	    {
		w->paint.opacity = 0;

		if (fw->opacity == 0xffff)
		    fw->opacity = 0xfffe;

		fw->unmapCnt++;
		w->unmapRefCnt++;

		addWindowDamage (w);
	    }

	    fadeRemoveDisplayModal (d, w);
	}
	break;
    case MapNotify:
	w = findWindowAtDisplay (d, event->xmap.window);
	if (w)
	{
	    if (!(w->type & CompWindowTypeDesktopMask))
		w->paint.opacity = getWindowProp32 (d, w->id,
						    d->winOpacityAtom,
						    OPAQUE);

	    fadeWindowStop (w);

	    if (w->state & CompWindowStateDisplayModalMask)
		fadeAddDisplayModal (d, w);
	}
	break;
    default:
	if (event->type == d->xkbEvent)
	{
	    XkbAnyEvent *xkbEvent = (XkbAnyEvent *) event;

	    if (xkbEvent->xkb_type == XkbBellNotify)
	    {
		XkbBellNotifyEvent *xkbBellEvent = (XkbBellNotifyEvent *)
		    xkbEvent;

		w = findWindowAtDisplay (d, xkbBellEvent->window);
		if (!w)
		    w = findWindowAtDisplay (d, d->activeWindow);

		if (w)
		{
		    CompScreen *s = w->screen;

		    FADE_SCREEN (s);

		    if (fs->opt[FADE_SCREEN_OPTION_VISUAL_BELL].value.b)
		    {
			int option;

			option = FADE_SCREEN_OPTION_FULLSCREEN_VISUAL_BELL;
			if (fs->opt[option].value.b)
			{
			    for (w = s->windows; w; w = w->next)
			    {
				if (w->destroyed)
				    continue;

				if (w->attrib.map_state != IsViewable)
				    continue;

				if (w->damaged)
				{
				    FADE_WINDOW (w);

				    fw->brightness = w->paint.brightness / 2;
				}
			    }

			    damageScreen (s);
			}
			else
			{
			    FADE_WINDOW (w);

			    fw->brightness = w->paint.brightness / 2;

			    addWindowDamage (w);
			}
		    }
		}
	    }
	}
	break;
    }

    UNWRAP (fd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (fd, d, handleEvent, fadeHandleEvent);

    switch (event->type) {
    case PropertyNotify:
	if (event->xproperty.atom == d->winStateAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w && w->attrib.map_state == IsViewable)
	    {
		if (w->state & CompWindowStateDisplayModalMask)
		    fadeAddDisplayModal (d, w);
		else
		    fadeRemoveDisplayModal (d, w);
	    }
	}
	break;
    }
}

static Bool
fadeDamageWindowRect (CompWindow *w,
		      Bool	 initial,
		      BoxPtr     rect)
{
    Bool status;

    FADE_SCREEN (w->screen);

    if (initial)
    {
	FADE_WINDOW (w);

	if (fw->shaded)
	{
	    fw->shaded = w->shaded;
	}
	else if ((fs->wMask & w->type) && fw->opacity == w->paint.opacity)
	{
	    fw->opacity = 0;
	}
    }

    UNWRAP (fs, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (fs, w->screen, damageWindowRect, fadeDamageWindowRect);

    return status;
}

static Bool
fadeFocusWindow (CompWindow *w)
{
    Bool status;

    FADE_SCREEN (w->screen);
    FADE_WINDOW (w);

    if (fw->destroyCnt || fw->unmapCnt)
	return FALSE;

    UNWRAP (fs, w->screen, focusWindow);
    status = (*w->screen->focusWindow) (w);
    WRAP (fs, w->screen, focusWindow, fadeFocusWindow);

    return status;
}

static void
fadeWindowResizeNotify (CompWindow *w)
{
    FADE_SCREEN (w->screen);

    if (!w->mapNum)
	fadeWindowStop (w);

    UNWRAP (fs, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w);
    WRAP (fs, w->screen, windowResizeNotify, fadeWindowResizeNotify);
}

static Bool
fadeInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    FadeDisplay *fd;

    fd = malloc (sizeof (FadeDisplay));
    if (!fd)
	return FALSE;

    fd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (fd->screenPrivateIndex < 0)
    {
	free (fd);
	return FALSE;
    }

    fd->displayModals = 0;

    WRAP (fd, d, handleEvent, fadeHandleEvent);

    d->privates[displayPrivateIndex].ptr = fd;

    return TRUE;
}

static void
fadeFiniDisplay (CompPlugin *p,
		 CompDisplay *d)
{
    FADE_DISPLAY (d);

    freeScreenPrivateIndex (d, fd->screenPrivateIndex);

    UNWRAP (fd, d, handleEvent);

    free (fd);
}

static Bool
fadeInitScreen (CompPlugin *p,
		CompScreen *s)
{
    FadeScreen *fs;

    FADE_DISPLAY (s->display);

    fs = malloc (sizeof (FadeScreen));
    if (!fs)
	return FALSE;

    fs->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (fs->windowPrivateIndex < 0)
    {
	free (fs);
	return FALSE;
    }

    fs->steps    = 0;
    fs->fadeTime = 1000.0f / FADE_SPEED_DEFAULT;

    fadeScreenInitOptions (fs);

    WRAP (fs, s, preparePaintScreen, fadePreparePaintScreen);
    WRAP (fs, s, paintWindow, fadePaintWindow);
    WRAP (fs, s, damageWindowRect, fadeDamageWindowRect);
    WRAP (fs, s, focusWindow, fadeFocusWindow);
    WRAP (fs, s, windowResizeNotify, fadeWindowResizeNotify);

    s->privates[fd->screenPrivateIndex].ptr = fs;

    return TRUE;
}

static void
fadeFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    FADE_SCREEN (s);

    freeWindowPrivateIndex (s, fs->windowPrivateIndex);

    UNWRAP (fs, s, preparePaintScreen);
    UNWRAP (fs, s, paintWindow);
    UNWRAP (fs, s, damageWindowRect);
    UNWRAP (fs, s, focusWindow);
    UNWRAP (fs, s, windowResizeNotify);

    free (fs);
}

static Bool
fadeInitWindow (CompPlugin *p,
		CompWindow *w)
{
    FadeWindow *fw;

    FADE_SCREEN (w->screen);

    fw = malloc (sizeof (FadeWindow));
    if (!fw)
	return FALSE;

    fw->opacity	   = w->paint.opacity;
    fw->brightness = w->paint.brightness;
    fw->saturation = w->paint.saturation;

    fw->dModal = 0;

    fw->destroyCnt = 0;
    fw->unmapCnt   = 0;
    fw->shaded     = w->shaded;

    w->privates[fs->windowPrivateIndex].ptr = fw;

    if (w->attrib.map_state == IsViewable)
    {
	if (w->state & CompWindowStateDisplayModalMask)
	    fadeAddDisplayModal (w->screen->display, w);
    }

    return TRUE;
}

static void
fadeFiniWindow (CompPlugin *p,
		CompWindow *w)
{
    FADE_WINDOW (w);

    fadeRemoveDisplayModal (w->screen->display, w);
    fadeWindowStop (w);

    free (fw);
}

static Bool
fadeInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
fadeFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginDep fadeDeps[] = {
    { CompPluginRuleBefore, "cube" },
    { CompPluginRuleBefore, "scale" }
};

static CompPluginVTable fadeVTable = {
    "fade",
    N_("Fading Windows"),
    N_("Fade in windows when mapped and fade out windows when unmapped"),
    fadeInit,
    fadeFini,
    fadeInitDisplay,
    fadeFiniDisplay,
    fadeInitScreen,
    fadeFiniScreen,
    fadeInitWindow,
    fadeFiniWindow,
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    fadeGetScreenOptions,
    fadeSetScreenOption,
    fadeDeps,
    sizeof (fadeDeps) / sizeof (fadeDeps[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &fadeVTable;
}
