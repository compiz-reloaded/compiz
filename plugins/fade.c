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

#include <compiz-core.h>

static CompMetadata fadeMetadata;

static int displayPrivateIndex;

typedef struct _FadeDisplay {
    int			       screenPrivateIndex;
    HandleEventProc	       handleEvent;
    MatchExpHandlerChangedProc matchExpHandlerChanged;
    int			       displayModals;
    Bool		       suppressMinimizeOpenClose;
    CompMatch		       alwaysFadeWindowMatch;
} FadeDisplay;

#define FADE_SCREEN_OPTION_FADE_MODE		   0
#define FADE_SCREEN_OPTION_FADE_SPEED		   1
#define FADE_SCREEN_OPTION_FADE_TIME		   2
#define FADE_SCREEN_OPTION_WINDOW_MATCH		   3
#define FADE_SCREEN_OPTION_VISUAL_BELL		   4
#define FADE_SCREEN_OPTION_FULLSCREEN_VISUAL_BELL  5
#define FADE_SCREEN_OPTION_MINIMIZE_OPEN_CLOSE	   6
#define FADE_SCREEN_OPTION_DIM_UNRESPONSIVE	   7
#define FADE_SCREEN_OPTION_UNRESPONSIVE_BRIGHTNESS 8
#define FADE_SCREEN_OPTION_UNRESPONSIVE_SATURATION 9
#define FADE_SCREEN_OPTION_NUM			   10

#define FADE_MODE_CONSTANTSPEED 0
#define FADE_MODE_CONSTANTTIME  1
#define FADE_MODE_MAX           FADE_MODE_CONSTANTTIME

typedef struct _FadeScreen {
    int			   windowPrivateIndex;
    int			   fadeTime;

    CompOption opt[FADE_SCREEN_OPTION_NUM];

    PreparePaintScreenProc preparePaintScreen;
    PaintWindowProc	   paintWindow;
    DamageWindowRectProc   damageWindowRect;
    FocusWindowProc	   focusWindow;
    WindowResizeNotifyProc windowResizeNotify;

    CompMatch match;
} FadeScreen;

typedef struct _FadeWindow {
    GLushort opacity;
    GLushort brightness;
    GLushort saturation;

    int dModal;

    int destroyCnt;
    int unmapCnt;

    Bool shaded;
    Bool alive;
    Bool fadeOut;

    int steps;

    int fadeTime;

    int opacityDiff;
    int brightnessDiff;
    int saturationDiff;

    GLushort targetOpacity;
    GLushort targetBrightness;
    GLushort targetSaturation;
} FadeWindow;

#define GET_FADE_DISPLAY(d)					  \
    ((FadeDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define FADE_DISPLAY(d)			   \
    FadeDisplay *fd = GET_FADE_DISPLAY (d)

#define GET_FADE_SCREEN(s, fd)					      \
    ((FadeScreen *) (s)->base.privates[(fd)->screenPrivateIndex].ptr)

#define FADE_SCREEN(s)							\
    FadeScreen *fs = GET_FADE_SCREEN (s, GET_FADE_DISPLAY (s->display))

#define GET_FADE_WINDOW(w, fs)					      \
    ((FadeWindow *) (w)->base.privates[(fs)->windowPrivateIndex].ptr)

#define FADE_WINDOW(w)					     \
    FadeWindow *fw = GET_FADE_WINDOW  (w,		     \
		     GET_FADE_SCREEN  (w->screen,	     \
		     GET_FADE_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static void
fadeUpdateWindowFadeMatch (CompDisplay     *display,
			   CompOptionValue *value,
			   CompMatch       *match)
{
    matchFini (match);
    matchInit (match);
    matchAddFromString (match, "!type=desktop");
    matchAddGroup (match, MATCH_OP_AND_MASK, &value->match);
    matchUpdate (display, match);
}

static CompOption *
fadeGetScreenOptions (CompPlugin *plugin,
		      CompScreen *screen,
		      int	 *count)
{
    FADE_SCREEN (screen);

    *count = NUM_OPTIONS (fs);
    return fs->opt;
}

static Bool
fadeSetScreenOption (CompPlugin      *plugin,
		     CompScreen      *screen,
		     const char	     *name,
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
    case FADE_SCREEN_OPTION_WINDOW_MATCH:
	if (compSetMatchOption (o, value))
	{
	    fadeUpdateWindowFadeMatch (screen->display, &o->value, &fs->match);
	    return TRUE;
	}
	break;
    default:
	if (compSetOption (o, value))
	    return TRUE;
	break;
    }

    return FALSE;
}

static void
fadePreparePaintScreen (CompScreen *s,
			int	   msSinceLastPaint)
{
    CompWindow *w;
    int	       steps;

    FADE_SCREEN (s);

    switch (fs->opt[FADE_SCREEN_OPTION_FADE_MODE].value.i) {
    case FADE_MODE_CONSTANTSPEED:
	steps = (msSinceLastPaint * OPAQUE) / fs->fadeTime;
	if (steps < 12)
	    steps = 12;

	for (w = s->windows; w; w = w->next)
	{
	    FadeWindow *fw = GET_FADE_WINDOW (w, fs);
	    fw->steps    = steps;
	    fw->fadeTime = 0;
	}

	break;
    case FADE_MODE_CONSTANTTIME:
	for (w = s->windows; w; w = w->next)
	{
	    FadeWindow *fw = GET_FADE_WINDOW (w, fs);

	    if (fw->fadeTime)
	    {
		fw->steps     = 1;
		fw->fadeTime -= msSinceLastPaint;
		if (fw->fadeTime < 0)
		    fw->fadeTime = 0;
	    }
	    else
	    {
		fw->steps = 0;
	    }
	}
	
	break;
    }


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
		 const CompTransform	 *transform,
		 Region			 region,
		 unsigned int		 mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    FADE_DISPLAY (s->display);
    FADE_SCREEN (s);
    FADE_WINDOW (w);

    if (!w->screen->canDoSlightlySaturated)
	fw->saturation = attrib->saturation;

    if (!w->alive                            ||
	fw->destroyCnt			     ||
	fw->unmapCnt			     ||
	fw->opacity    != attrib->opacity    ||
	fw->brightness != attrib->brightness ||
	fw->saturation != attrib->saturation ||
	fd->displayModals)
    {
	WindowPaintAttrib fAttrib = *attrib;
	int               mode = fs->opt[FADE_SCREEN_OPTION_FADE_MODE].value.i;

	if (!w->alive && fs->opt[FADE_SCREEN_OPTION_DIM_UNRESPONSIVE].value.b)
	{
	    GLuint value;

	    value = fs->opt[FADE_SCREEN_OPTION_UNRESPONSIVE_BRIGHTNESS].value.i;
	    if (value != 100)
		fAttrib.brightness = fAttrib.brightness * value / 100;

	    value = fs->opt[FADE_SCREEN_OPTION_UNRESPONSIVE_SATURATION].value.i;
	    if (value != 100 && s->canDoSlightlySaturated)
		fAttrib.saturation = fAttrib.saturation * value / 100;
	}
	else if (fd->displayModals && !fw->dModal)
	{
	    fAttrib.brightness = 0xa8a8;
	    fAttrib.saturation = 0;
	}

	if (fw->fadeOut)
	    fAttrib.opacity = 0;

	if (mode == FADE_MODE_CONSTANTTIME)
	{
	    if (fAttrib.opacity    != fw->targetOpacity    ||
		fAttrib.brightness != fw->targetBrightness ||
		fAttrib.saturation != fw->targetSaturation)
	    {
		fw->fadeTime = fs->opt[FADE_SCREEN_OPTION_FADE_TIME].value.i;
		fw->steps    = 1;

		fw->opacityDiff    = fAttrib.opacity - fw->opacity;
		fw->brightnessDiff = fAttrib.brightness - fw->brightness;
		fw->saturationDiff = fAttrib.saturation - fw->saturation;

		fw->targetOpacity    = fAttrib.opacity;
		fw->targetBrightness = fAttrib.brightness;
		fw->targetSaturation = fAttrib.saturation;
	    }
	}

	if (fw->steps)
	{
	    GLint opacity = OPAQUE;
	    GLint brightness = BRIGHT;
	    GLint saturation = COLOR;

	    if (mode == FADE_MODE_CONSTANTSPEED)
	    {
		opacity = fw->opacity;
		if (fAttrib.opacity > fw->opacity)
		{
		    opacity = fw->opacity + fw->steps;
		    if (opacity > fAttrib.opacity)
			opacity = fAttrib.opacity;
		}
		else if (fAttrib.opacity < fw->opacity)
		{
		    opacity = fw->opacity - fw->steps;
		    if (opacity < fAttrib.opacity)
			opacity = fAttrib.opacity;
		}

		brightness = fw->brightness;
		if (fAttrib.brightness > fw->brightness)
		{
		    brightness = fw->brightness + (fw->steps / 12);
		    if (brightness > fAttrib.brightness)
			brightness = fAttrib.brightness;
		}
		else if (fAttrib.brightness < fw->brightness)
		{
		    brightness = fw->brightness - (fw->steps / 12);
		    if (brightness < fAttrib.brightness)
			brightness = fAttrib.brightness;
		}

		saturation = fw->saturation;
		if (fAttrib.saturation > fw->saturation)
		{
		    saturation = fw->saturation + (fw->steps / 6);
		    if (saturation > fAttrib.saturation)
			saturation = fAttrib.saturation;
		}
		else if (fAttrib.saturation < fw->saturation)
		{
		    saturation = fw->saturation - (fw->steps / 6);
		    if (saturation < fAttrib.saturation)
			saturation = fAttrib.saturation;
		}
	    }
	    else if (mode == FADE_MODE_CONSTANTTIME)
	    {
		int fadeTime = fs->opt[FADE_SCREEN_OPTION_FADE_TIME].value.i;

		opacity = fAttrib.opacity -
		          (fw->opacityDiff * fw->fadeTime / fadeTime);
		brightness = fAttrib.brightness -
			     (fw->brightnessDiff * fw->fadeTime / fadeTime);
		saturation = fAttrib.saturation -
			     (fw->saturationDiff * fw->fadeTime / fadeTime);
	    }

	    fw->steps = 0;

	    if (opacity > 0)
	    {
		fw->opacity    = opacity;
		fw->brightness = brightness;
		fw->saturation = saturation;

		if (opacity    != fAttrib.opacity    ||
		    brightness != fAttrib.brightness ||
		    saturation != fAttrib.saturation)
		    addWindowDamage (w);
	    }
	    else
	    {
		fw->opacity = 0;

		fadeWindowStop (w);
	    }
	}

	fAttrib.opacity	   = fw->opacity;
	fAttrib.brightness = fw->brightness;
	fAttrib.saturation = fw->saturation;

	UNWRAP (fs, s, paintWindow);
	status = (*s->paintWindow) (w, &fAttrib, transform, region, mask);
	WRAP (fs, s, paintWindow, fadePaintWindow);
    }
    else
    {
	UNWRAP (fs, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, transform, region, mask);
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
	    damageScreen (s);
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
	    damageScreen (s);
    }
}

/* Returns whether this window should be faded
 * on open and close events. */
static Bool
isFadeWinForOpenClose (CompWindow *w)
{
    FADE_DISPLAY (w->screen->display);
    FADE_SCREEN (w->screen);

    if (fs->opt[FADE_SCREEN_OPTION_MINIMIZE_OPEN_CLOSE].value.b &&
	!fd->suppressMinimizeOpenClose)
    {
	return TRUE;
    }
    return matchEval (&fd->alwaysFadeWindowMatch, w);
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

	    if (w->texture->pixmap && isFadeWinForOpenClose (w) &&
		matchEval (&fs->match, w))
	    {
		FADE_WINDOW (w);

		if (fw->opacity == 0xffff)
		    fw->opacity = 0xfffe;

		fw->destroyCnt++;
		w->destroyRefCnt++;

		fw->fadeOut = TRUE;

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

	    if (fs->opt[FADE_SCREEN_OPTION_MINIMIZE_OPEN_CLOSE].value.b &&
		!fd->suppressMinimizeOpenClose &&
		!fw->shaded && w->texture->pixmap &&
		matchEval (&fs->match, w))
	    {
		if (fw->opacity == 0xffff)
		    fw->opacity = 0xfffe;

		fw->unmapCnt++;
		w->unmapRefCnt++;

		fw->fadeOut = TRUE;

		addWindowDamage (w);
	    }

	    fadeRemoveDisplayModal (d, w);
	}
	break;
    case MapNotify:
	w = findWindowAtDisplay (d, event->xmap.window);
	if (w)
	{
	    FADE_SCREEN (w->screen);

	    if (fs->opt[FADE_SCREEN_OPTION_MINIMIZE_OPEN_CLOSE].value.b &&
		!fd->suppressMinimizeOpenClose)
	    {
		fadeWindowStop (w);
	    }
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
    case ClientMessage:
	if (event->xclient.message_type == d->wmProtocolsAtom &&
	    event->xclient.data.l[0] == d->wmPingAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.data.l[2]);
	    if (w)
	    {
		FADE_WINDOW (w);

		if (w->alive != fw->alive)
		{
		    addWindowDamage (w);
		    fw->alive = w->alive;
		}
	    }
	}
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

	fw->fadeOut = FALSE;

	if (fw->shaded)
	{
	    fw->shaded = w->shaded;
	}
	else if (matchEval (&fs->match, w))
	{
	    if (isFadeWinForOpenClose (w))
	    {
		fw->opacity       = 0;
		fw->targetOpacity = 0;
	    }
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
fadeWindowResizeNotify (CompWindow *w,
			int	   dx,
			int	   dy,
			int	   dwidth,
			int	   dheight)
{
    FADE_SCREEN (w->screen);

    if (!w->mapNum)
	fadeWindowStop (w);

    UNWRAP (fs, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w, dx, dy, dwidth, dheight);
    WRAP (fs, w->screen, windowResizeNotify, fadeWindowResizeNotify);
}

static void
fadeMatchExpHandlerChanged (CompDisplay *d)
{
    CompScreen *s;

    FADE_DISPLAY (d);

    for (s = d->screens; s; s = s->next)
	matchUpdate (d, &GET_FADE_SCREEN (s,fd)->match);

    UNWRAP (fd, d, matchExpHandlerChanged);
    (*d->matchExpHandlerChanged) (d);
    WRAP (fd, d, matchExpHandlerChanged, fadeMatchExpHandlerChanged);
}

static Bool
fadeInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    FadeDisplay *fd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

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

    fd->suppressMinimizeOpenClose = (findActivePlugin ("animation") != NULL);

    /* Always fade opening and closing of screen-dimming layer of 
       logout window and gksu. */
    matchInit (&fd->alwaysFadeWindowMatch);
    matchAddExp (&fd->alwaysFadeWindowMatch, 0, "title=gksu");
    matchAddExp (&fd->alwaysFadeWindowMatch, 0, "title=x-session-manager");
    matchAddExp (&fd->alwaysFadeWindowMatch, 0, "title=gnome-session");
    matchUpdate (d, &fd->alwaysFadeWindowMatch);

    WRAP (fd, d, handleEvent, fadeHandleEvent);
    WRAP (fd, d, matchExpHandlerChanged, fadeMatchExpHandlerChanged);

    d->base.privates[displayPrivateIndex].ptr = fd;

    return TRUE;
}

static void
fadeFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    FADE_DISPLAY (d);

    freeScreenPrivateIndex (d, fd->screenPrivateIndex);

    matchFini (&fd->alwaysFadeWindowMatch);

    UNWRAP (fd, d, handleEvent);
    UNWRAP (fd, d, matchExpHandlerChanged);

    free (fd);
}

static const CompMetadataOptionInfo fadeScreenOptionInfo[] = {
    { "fade_mode", "int", RESTOSTRING (0, FADE_MODE_MAX), 0, 0 },
    { "fade_speed", "float", "<min>0.1</min>", 0, 0 },
    { "fade_time", "int", "<min>1</min>", 0, 0 },
    { "window_match", "match", "<helper>true</helper>", 0, 0 },
    { "visual_bell", "bool", 0, 0, 0 },
    { "fullscreen_visual_bell", "bool", 0, 0, 0 },
    { "minimize_open_close", "bool", 0, 0, 0 },
    { "dim_unresponsive", "bool", 0, 0, 0 },
    { "unresponsive_brightness", "int", "<min>0</min><max>100</max>", 0, 0 },
    { "unresponsive_saturation", "int", "<min>0</min><max>100</max>", 0, 0 }
};

static Bool
fadeInitScreen (CompPlugin *p,
		CompScreen *s)
{
    FadeScreen *fs;

    FADE_DISPLAY (s->display);

    fs = malloc (sizeof (FadeScreen));
    if (!fs)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &fadeMetadata,
					    fadeScreenOptionInfo,
					    fs->opt,
					    FADE_SCREEN_OPTION_NUM))
    {
	free (fs);
	return FALSE;
    }

    fs->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (fs->windowPrivateIndex < 0)
    {
	compFiniScreenOptions (s, fs->opt, FADE_SCREEN_OPTION_NUM);
	free (fs);
	return FALSE;
    }

    fs->fadeTime = 1000.0f / fs->opt[FADE_SCREEN_OPTION_FADE_SPEED].value.f;

    matchInit (&fs->match);

    fadeUpdateWindowFadeMatch (s->display,
			       &fs->opt[FADE_SCREEN_OPTION_WINDOW_MATCH].value,
			       &fs->match);

    WRAP (fs, s, preparePaintScreen, fadePreparePaintScreen);
    WRAP (fs, s, paintWindow, fadePaintWindow);
    WRAP (fs, s, damageWindowRect, fadeDamageWindowRect);
    WRAP (fs, s, focusWindow, fadeFocusWindow);
    WRAP (fs, s, windowResizeNotify, fadeWindowResizeNotify);

    s->base.privates[fd->screenPrivateIndex].ptr = fs;

    return TRUE;
}

static void
fadeFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    FADE_SCREEN (s);

    matchFini (&fs->match);

    freeWindowPrivateIndex (s, fs->windowPrivateIndex);

    UNWRAP (fs, s, preparePaintScreen);
    UNWRAP (fs, s, paintWindow);
    UNWRAP (fs, s, damageWindowRect);
    UNWRAP (fs, s, focusWindow);
    UNWRAP (fs, s, windowResizeNotify);

    compFiniScreenOptions (s, fs->opt, FADE_SCREEN_OPTION_NUM);

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

    fw->targetOpacity    = fw->opacity;
    fw->targetBrightness = fw->brightness;
    fw->targetSaturation = fw->saturation;

    fw->opacityDiff    = 0;
    fw->brightnessDiff = 0;
    fw->saturationDiff = 0;

    fw->dModal = 0;

    fw->destroyCnt = 0;
    fw->unmapCnt   = 0;
    fw->shaded     = w->shaded;
    fw->fadeOut    = FALSE;
    fw->alive      = w->alive;

    fw->steps      = 0;
    fw->fadeTime   = 0;

    w->base.privates[fs->windowPrivateIndex].ptr = fw;

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

static CompBool
fadeInitObject (CompPlugin *p,
		CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) fadeInitDisplay,
	(InitPluginObjectProc) fadeInitScreen,
	(InitPluginObjectProc) fadeInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
fadeFiniObject (CompPlugin *p,
		CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) fadeFiniDisplay,
	(FiniPluginObjectProc) fadeFiniScreen,
	(FiniPluginObjectProc) fadeFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
fadeGetObjectOptions (CompPlugin *plugin,
		      CompObject *object,
		      int	 *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) 0, /* GetDisplayOptions */
	(GetPluginObjectOptionsProc) fadeGetScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
fadeSetObjectOption (CompPlugin      *plugin,
		     CompObject      *object,
		     const char      *name,
		     CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) 0, /* SetDisplayOption */
	(SetPluginObjectOptionProc) fadeSetScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
fadeInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&fadeMetadata, p->vTable->name, 0, 0,
					 fadeScreenOptionInfo,
					 FADE_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&fadeMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&fadeMetadata, p->vTable->name);

    return TRUE;
}

static void
fadeFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&fadeMetadata);
}

static CompMetadata *
fadeGetMetadata (CompPlugin *plugin)
{
    return &fadeMetadata;
}

static CompPluginVTable fadeVTable = {
    "fade",
    fadeGetMetadata,
    fadeInit,
    fadeFini,
    fadeInitObject,
    fadeFiniObject,
    fadeGetObjectOptions,
    fadeSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &fadeVTable;
}
