/*
 * Copyright © 2006 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Red Hat, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Søren Sandmann <sandmann@redhat.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <X11/Xatom.h>
#include <X11/Xproto.h>

#include <compiz.h>

static CompMetadata planeMetadata;

static int displayPrivateIndex;

enum
{
    PLANE_DISPLAY_OPTION_LEFT,
    PLANE_DISPLAY_OPTION_RIGHT,
    PLANE_DISPLAY_OPTION_DOWN,
    PLANE_DISPLAY_OPTION_UP,
    PLANE_DISPLAY_OPTION_TO_1,
    PLANE_DISPLAY_OPTION_TO_2,
    PLANE_DISPLAY_OPTION_TO_3,
    PLANE_DISPLAY_OPTION_TO_4,
    PLANE_DISPLAY_OPTION_TO_5,
    PLANE_DISPLAY_OPTION_TO_6,
    PLANE_DISPLAY_OPTION_TO_7,
    PLANE_DISPLAY_OPTION_TO_8,
    PLANE_DISPLAY_OPTION_TO_9,
    PLANE_DISPLAY_OPTION_TO_10,
    PLANE_DISPLAY_OPTION_TO_11,
    PLANE_DISPLAY_OPTION_TO_12,
    PLANE_N_DISPLAY_OPTIONS
};

typedef struct _PlaneDisplay {
    int			screenPrivateIndex;
    HandleEventProc	handleEvent;

    CompOption		opt[PLANE_N_DISPLAY_OPTIONS];
} PlaneDisplay;

typedef struct _PlaneScreen {
    PaintTransformedOutputProc		paintTransformedOutput;
    PreparePaintScreenProc		preparePaintScreen;
    DonePaintScreenProc			donePaintScreen;
    PaintOutputProc			paintOutput;

    WindowGrabNotifyProc		windowGrabNotify;
    WindowUngrabNotifyProc		windowUngrabNotify;

    CompTimeoutHandle			timeoutHandle;
    int					timer;

    double				cur_x;
    double				cur_y;
    double				dest_x;
    double				dest_y;
} PlaneScreen;

#define GET_PLANE_DISPLAY(d)				       \
    ((PlaneDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define PLANE_DISPLAY(d)		       \
    PlaneDisplay *pd = GET_PLANE_DISPLAY (d)

#define GET_PLANE_SCREEN(s, pd)				   \
    ((PlaneScreen *) (s)->privates[(pd)->screenPrivateIndex].ptr)

#define PLANE_SCREEN(s)						      \
    PlaneScreen *ps = GET_PLANE_SCREEN (s, GET_PLANE_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static Bool
endMove (void *data)
{
    CompScreen *screen = data;
    PLANE_SCREEN (screen);

    moveScreenViewport (screen, -ps->dest_x, -ps->dest_y, TRUE);

    ps->dest_x = 0;
    ps->dest_y = 0;

    ps->timeoutHandle = 0;
    return FALSE;
}

#define SCROLL_TIME	250

static void
computeTranslation (PlaneScreen *ps,
		    double	*x,
		    double      *y)
{
    double dx, dy;
    double elapsed = 1 - (ps->timer / (double)SCROLL_TIME);

    if (elapsed < 0.0)
	elapsed = 0.0;
    if (elapsed > 1.0)
	elapsed = 1.0;

    /* Use temporary variables to you can pass in &ps->cur_x */
    dx = (ps->dest_x - ps->cur_x) * elapsed + ps->cur_x;
    dy = (ps->dest_y - ps->cur_y) * elapsed + ps->cur_y;

    *x = dx;
    *y = dy;
}

static void
moveViewport (CompScreen *screen,
	      int	 dx,
	      int	 dy)
{
    PLANE_SCREEN (screen);

    if (dx == 0 && dy == 0)
	return;

    if (ps->timeoutHandle)
    {
	computeTranslation (ps, &ps->cur_x, &ps->cur_y);

	ps->dest_x += dx;
	ps->dest_y += dy;

	compRemoveTimeout (ps->timeoutHandle);
    }
    else
    {
	ps->cur_x = 0.0;
	ps->cur_y = 0.0;
	ps->dest_x = dx;
	ps->dest_y = dy;
    }

    if (ps->dest_x + screen->x > screen->hsize - 1)
	ps->dest_x = screen->hsize - screen->x - 1;

    if (ps->dest_x + screen->x < 0)
	ps->dest_x = -screen->x;

    if (ps->dest_y + screen->y > screen->vsize - 1)
	ps->dest_y = screen->vsize - screen->y - 1;

    if (ps->dest_y + screen->y < 0)
	ps->dest_y = -screen->y;

    ps->timer = SCROLL_TIME;
    ps->timeoutHandle = compAddTimeout (SCROLL_TIME, endMove, screen);

    damageScreen (screen);
}

static void
planePreparePaintScreen (CompScreen *s,
			 int	    msSinceLastPaint)
{
    PLANE_SCREEN (s);

    ps->timer -= msSinceLastPaint;

    UNWRAP (ps, s, preparePaintScreen);

    (* s->preparePaintScreen) (s, msSinceLastPaint);

    WRAP (ps, s, preparePaintScreen, planePreparePaintScreen);
}

static void
planePaintTransformedOutput (CompScreen		     *screen,
			     const ScreenPaintAttrib *sAttrib,
			     const CompTransform     *transform,
			     Region		     region,
			     CompOutput              *output,
			     unsigned int	     mask)
{
    PLANE_SCREEN (screen);

    UNWRAP (ps, screen, paintTransformedOutput);

    if (ps->timeoutHandle)
    {
	CompTransform sTransform = *transform;
	double dx, dy;
	int vx, vy;

	clearTargetOutput (screen->display, GL_COLOR_BUFFER_BIT);

	computeTranslation (ps, &dx, &dy);

	dx *= -1;
	dy *= -1;

	vx = 0;
	vy = 0;

	while (dx > 1)
	{
	    dx -= 1.0;
	    moveScreenViewport (screen, 1, 0, FALSE);
	    vx++;
	}

	while (dx < -1)
	{
	    dx += 1.0;
	    moveScreenViewport (screen, -1, 0, FALSE);
	    vx--;
	}

	while (dy > 1)
	{
	    dy -= 1.0;
	    moveScreenViewport (screen, 0, 1, FALSE);
	    vy++;
	}

	while (dy < -1)
	{
	    dy += 1.0;
	    moveScreenViewport (screen, 0, -1, FALSE);
	    vy--;
	}

	matrixTranslate (&sTransform, dx, -dy, 0.0);

	(*screen->paintTransformedOutput) (screen, sAttrib, &sTransform,
					   region, output, mask);

	if (dx > 0)
	{
	    matrixTranslate (&sTransform, -1.0, 0.0, 0.0);
	    moveScreenViewport (screen, 1, 0, FALSE);
	}
	else
	{
	    matrixTranslate (&sTransform, 1.0, 0.0, 0.0);
	    moveScreenViewport (screen, -1, 0, FALSE);
	}

	(*screen->paintTransformedOutput) (screen, sAttrib, &sTransform,
					   region, output, mask);

	if (dy > 0)
	{
	    matrixTranslate (&sTransform, 0.0, 1.0, 0.0);
	    moveScreenViewport (screen, 0, 1, FALSE);
	}
	else
	{
	    matrixTranslate (&sTransform, 0.0, -1.0, 0.0);
	    moveScreenViewport (screen, 0, -1, FALSE);
	}

	(*screen->paintTransformedOutput) (screen, sAttrib, &sTransform,
					   region, output, mask);

	if (dx > 0)
	{
	    matrixTranslate (&sTransform, 1.0, 0.0, 0.0);
	    moveScreenViewport (screen, -1, 0, FALSE);
	}
	else
	{
	    matrixTranslate (&sTransform, -1.0, 0.0, 0.0);
	    moveScreenViewport (screen, 1, 0, FALSE);
	}

	(*screen->paintTransformedOutput) (screen, sAttrib, &sTransform,
					   region, output, mask);

	if (dy > 0)
	{
	    moveScreenViewport (screen, 0, -1, FALSE);
	}
	else
	{
	    moveScreenViewport (screen, 0, 1, FALSE);
	}

	moveScreenViewport (screen, -vx, -vy, FALSE);
    }
    else
    {
	(*screen->paintTransformedOutput) (screen, sAttrib, transform,
					   region, output, mask);
    }

    WRAP (ps, screen, paintTransformedOutput, planePaintTransformedOutput);
}

static void
planeDonePaintScreen (CompScreen *s)
{
    PLANE_SCREEN (s);

    if (ps->timeoutHandle)
	damageScreen (s);

    UNWRAP (ps, s, donePaintScreen);

    (*s->donePaintScreen) (s);

    WRAP (ps, s, donePaintScreen, planeDonePaintScreen);
}

static Bool
planePaintOutput (CompScreen		  *s,
		  const ScreenPaintAttrib *sAttrib,
		  const CompTransform	  *transform,
		  Region		  region,
		  CompOutput		  *output,
		  unsigned int		  mask)
{
    Bool status;

    PLANE_SCREEN (s);

    if (ps->timeoutHandle)
    {
	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_TRANSFORMED_MASK;
    }

    UNWRAP (ps, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (ps, s, paintOutput, planePaintOutput);

    return status;
}

static void
planeHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    CompScreen *s;

    PLANE_DISPLAY (d);

    switch (event->type) {
    case ClientMessage:
	if (event->xclient.message_type == d->winActiveAtom)
	{
	    CompWindow *w;

	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		int dx, dy;

		s = w->screen;

		/* window must be placed */
		if (!w->placed)
		    break;

		if (otherScreenGrabExist (s, "plane", "switcher", "cube", 0))
		    break;

		defaultViewportForWindow (w, &dx, &dy);
		dx -= s->x;
		dy -= s->y;

		moveViewport (s, dx, dy);
	    }
	}
	else if (event->xclient.message_type == d->desktopViewportAtom)
	{
	    int dx, dy;

	    s = findScreenAtDisplay (d, event->xclient.window);
	    if (!s)
		break;

	    if (otherScreenGrabExist (s, "plane", "switcher", "cube", 0))
		break;

	    dx = event->xclient.data.l[0] / s->width - s->x;
	    dy = event->xclient.data.l[1] / s->height - s->y;

	    if (!dx && !dy)
		break;

	    moveViewport (s, dx, dy);
	}
	break;

    default:
	break;
    }

    UNWRAP (pd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (pd, d, handleEvent, planeHandleEvent);
}

static void
planeWindowGrabNotify (CompWindow   *w,
		       int	    x,
		       int	    y,
		       unsigned int state,
		       unsigned int mask)
{
    PLANE_SCREEN (w->screen);

    UNWRAP (ps, w->screen, windowGrabNotify);
    (*w->screen->windowGrabNotify) (w, x, y, state, mask);
    WRAP (ps, w->screen, windowGrabNotify, planeWindowGrabNotify);
}

static void
planeWindowUngrabNotify (CompWindow *w)
{
    PLANE_SCREEN (w->screen);

    UNWRAP (ps, w->screen, windowUngrabNotify);
    (*w->screen->windowUngrabNotify) (w);
    WRAP (ps, w->screen, windowUngrabNotify, planeWindowUngrabNotify);
}

static CompOption *
planeGetDisplayOptions (CompPlugin  *plugin,
			CompDisplay *display,
			int	    *count)
{
    PLANE_DISPLAY (display);

    *count = NUM_OPTIONS (pd);
    return pd->opt;
}

static Bool
planeSetDisplayOption (CompPlugin      *plugin,
		       CompDisplay     *display,
		       char	       *name,
		       CompOptionValue *value)
{
    CompOption *o;

    PLANE_DISPLAY (display);

    o = compFindOption (pd->opt, NUM_OPTIONS (pd), name, NULL);
    if (!o)
	return FALSE;

    return compSetDisplayOption (display, o, value);
}

static CompScreen *
getScreen (CompDisplay *d,
	   CompOption  *option,
	   int	       n_options)
{
    XID rootWindow = getIntOptionNamed (option, n_options, "root", 0);

    return findScreenAtDisplay (d, rootWindow);
}

static Bool
planeLeft (CompDisplay		*d,
	    CompAction		*action,
	    CompActionState	state,
	    CompOption		*option,
	    int			n_options)
{
    CompScreen *screen = getScreen (d, option, n_options);

    moveViewport (screen, -1, 0);
    return FALSE;
}

static Bool
planeRight (CompDisplay	    *d,
	    CompAction	    *action,
	    CompActionState state,
	    CompOption	    *option,
	    int		    n_options)
{
    CompScreen *screen = getScreen (d, option, n_options);

    moveViewport (screen, 1, 0);
    return FALSE;
}

static Bool
planeUp (CompDisplay	 *d,
	 CompAction	 *action,
	 CompActionState state,
	 CompOption	 *option,
	 int		 n_options)
{
    CompScreen *screen = getScreen (d, option, n_options);

    moveViewport (screen, 0, -1);
    return FALSE;
}

static Bool
planeDown (CompDisplay	   *d,
	   CompAction	   *action,
	   CompActionState state,
	   CompOption	   *option,
	   int		   n_options)
{
    CompScreen *screen = getScreen (d, option, n_options);

    moveViewport (screen, 0, 1);
    return FALSE;
}

static Bool
planeTo (CompDisplay     *d,
	 CompAction      *action,
	 CompActionState state,
	 CompOption      *option,
	 int		  n_options)
{
    int i, new_x, new_y, cur_x, cur_y;
    CompScreen *screen = getScreen (d, option, n_options);
    PLANE_DISPLAY (d);

    new_x = new_y = -1;
    for (i = PLANE_DISPLAY_OPTION_TO_1; i <= PLANE_DISPLAY_OPTION_TO_12; ++i)
    {
	if (action == &pd->opt[i].value.action)
	{
	    int viewport_no = i - PLANE_DISPLAY_OPTION_TO_1;

	    new_x = viewport_no % screen->hsize;
	    new_y = viewport_no / screen->hsize;

	    break;
	}
    }

    if (new_x == -1 || new_y == -1)
	return FALSE;

    cur_x = screen->x;
    cur_y = screen->y;

    moveViewport (screen, new_x - cur_x, new_y - cur_y);

    return FALSE;
}

static const CompMetadataOptionInfo planeDisplayOptionInfo[] = {
    { "plane_left", "action", 0, planeLeft, 0 },
    { "plane_right", "action", 0, planeRight, 0 },
    { "plane_down", "action", 0, planeDown, 0 },
    { "plane_up", "action", 0, planeUp, 0 },
    { "plane_to_1", "action", 0, planeTo, 0 },
    { "plane_to_2", "action", 0, planeTo, 0 },
    { "plane_to_3", "action", 0, planeTo, 0 },
    { "plane_to_4", "action", 0, planeTo, 0 },
    { "plane_to_5", "action", 0, planeTo, 0 },
    { "plane_to_6", "action", 0, planeTo, 0 },
    { "plane_to_7", "action", 0, planeTo, 0 },
    { "plane_to_8", "action", 0, planeTo, 0 },
    { "plane_to_9", "action", 0, planeTo, 0 },
    { "plane_to_10", "action", 0, planeTo, 0 },
    { "plane_to_11", "action", 0, planeTo, 0 },
    { "plane_to_12", "action", 0, planeTo, 0 }
};

static Bool
planeInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    PlaneDisplay *pd;

    pd = malloc (sizeof (PlaneDisplay));
    if (!pd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &planeMetadata,
					     planeDisplayOptionInfo,
					     pd->opt,
					     PLANE_N_DISPLAY_OPTIONS))
    {
	free (pd);
	return FALSE;
    }

    pd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (pd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, pd->opt, PLANE_N_DISPLAY_OPTIONS);
	free (pd);
	return FALSE;
    }

    WRAP (pd, d, handleEvent, planeHandleEvent);

    d->privates[displayPrivateIndex].ptr = pd;

    return TRUE;
}

static void
planeFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    PLANE_DISPLAY (d);

    freeScreenPrivateIndex (d, pd->screenPrivateIndex);

    UNWRAP (pd, d, handleEvent);

    compFiniDisplayOptions (d, pd->opt, PLANE_N_DISPLAY_OPTIONS);

    free (pd);
}

static Bool
planeInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    PlaneScreen *ps;

    PLANE_DISPLAY (s->display);

    ps = malloc (sizeof (PlaneScreen));
    if (!ps)
	return FALSE;

    ps->timeoutHandle = 0;

    WRAP (ps, s, paintTransformedOutput, planePaintTransformedOutput);
    WRAP (ps, s, preparePaintScreen, planePreparePaintScreen);
    WRAP (ps, s, donePaintScreen, planeDonePaintScreen);
    WRAP (ps, s, paintOutput, planePaintOutput);
    WRAP (ps, s, windowGrabNotify, planeWindowGrabNotify);
    WRAP (ps, s, windowUngrabNotify, planeWindowUngrabNotify);

    s->privates[pd->screenPrivateIndex].ptr = ps;

    return TRUE;
}

static void
planeFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    PLANE_SCREEN (s);

    UNWRAP (ps, s, paintTransformedOutput);
    UNWRAP (ps, s, preparePaintScreen);
    UNWRAP (ps, s, donePaintScreen);
    UNWRAP (ps, s, paintOutput);
    UNWRAP (ps, s, windowGrabNotify);
    UNWRAP (ps, s, windowUngrabNotify);

    free (ps);
}

static Bool
planeInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&planeMetadata,
					 p->vTable->name,
					 planeDisplayOptionInfo,
					 PLANE_N_DISPLAY_OPTIONS,
					 NULL, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&planeMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&planeMetadata, p->vTable->name);

    return TRUE;
}

static void
planeFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&planeMetadata);
}

static int
planeGetVersion (CompPlugin *plugin,
		 int	    version)
{
    return ABIVERSION;
}

static CompMetadata *
planeGetMetadata (CompPlugin *plugin)
{
    return &planeMetadata;
}

CompPluginVTable planeVTable = {
    "plane",
    planeGetVersion,
    planeGetMetadata,
    planeInit,
    planeFini,
    planeInitDisplay,
    planeFiniDisplay,
    planeInitScreen,
    planeFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    planeGetDisplayOptions,
    planeSetDisplayOption,
    NULL, /* planeGetScreenOptions, */
    NULL  /* planeSetScreenOption, */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &planeVTable;
}
