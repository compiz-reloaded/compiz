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

#define _GNU_SOURCE /* for asprintf */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <X11/Xatom.h>
#include <X11/Xproto.h>

#include <compiz.h>

static int displayPrivateIndex;

#define PLANE_LEFT_KEY_DEFAULT        "Left"
#define PLANE_LEFT_MODIFIERS_DEFAULT  (ControlMask | CompAltMask)

#define PLANE_RIGHT_KEY_DEFAULT       "Right"
#define PLANE_RIGHT_MODIFIERS_DEFAULT (ControlMask | CompAltMask)

#define PLANE_UP_KEY_DEFAULT          "Up"
#define PLANE_UP_MODIFIERS_DEFAULT    (ControlMask | CompAltMask)

#define PLANE_DOWN_KEY_DEFAULT        "Down"
#define PLANE_DOWN_MODIFIERS_DEFAULT  (ControlMask | CompAltMask)

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
    PaintTransformedScreenProc		paintTransformedScreen;
    PreparePaintScreenProc		preparePaintScreen;
    DonePaintScreenProc			donePaintScreen;
    PaintScreenProc			paintScreen;

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
planePaintTransformedScreen (CompScreen		     *screen,
			     const ScreenPaintAttrib *sAttrib,
			     const CompTransform     *transform,
			     Region		     region,
			     int                     output,
			     unsigned int	     mask)
{
    PLANE_SCREEN (screen);

    UNWRAP (ps, screen, paintTransformedScreen);

    if (ps->timeoutHandle)
    {
	CompTransform sTransform = *transform;
	double dx, dy, tx, ty;
	int vx, vy;

	clearTargetOutput (screen->display, GL_COLOR_BUFFER_BIT);

	computeTranslation (ps, &dx, &dy);

	dx *= -1;
	dy *= -1;

	tx = dy;
	ty = dy;
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

	(*screen->paintTransformedScreen) (screen, sAttrib, &sTransform,
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

	(*screen->paintTransformedScreen) (screen, sAttrib, &sTransform,
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

	(*screen->paintTransformedScreen) (screen, sAttrib, &sTransform,
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

	(*screen->paintTransformedScreen) (screen, sAttrib, &sTransform,
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
	(*screen->paintTransformedScreen) (screen, sAttrib, transform,
					   region, output, mask);
    }

    WRAP (ps, screen, paintTransformedScreen, planePaintTransformedScreen);
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
planePaintScreen (CompScreen		  *s,
		  const ScreenPaintAttrib *sAttrib,
		  const CompTransform	  *transform,
		  Region		  region,
		  int			  output,
		  unsigned int		  mask)
{
    Bool status;

    PLANE_SCREEN (s);

    if (ps->timeoutHandle)
    {
	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_TRANSFORMED_MASK;
    }

    UNWRAP (ps, s, paintScreen);
    status = (*s->paintScreen) (s, sAttrib, transform, region, output, mask);
    WRAP (ps, s, paintScreen, planePaintScreen);

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
planeGetDisplayOptions (CompDisplay *display,
			int	    *count)
{
    PLANE_DISPLAY (display);

    *count = NUM_OPTIONS (pd);
    return pd->opt;
}

static Bool
planeSetDisplayOption (CompDisplay     *display,
		       char	       *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    PLANE_DISPLAY (display);

    o = compFindOption (pd->opt, NUM_OPTIONS (pd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case PLANE_DISPLAY_OPTION_LEFT:
    case PLANE_DISPLAY_OPTION_RIGHT:
    case PLANE_DISPLAY_OPTION_UP:
    case PLANE_DISPLAY_OPTION_DOWN:

    case PLANE_DISPLAY_OPTION_TO_1:
    case PLANE_DISPLAY_OPTION_TO_2:
    case PLANE_DISPLAY_OPTION_TO_3:
    case PLANE_DISPLAY_OPTION_TO_4:
    case PLANE_DISPLAY_OPTION_TO_5:
    case PLANE_DISPLAY_OPTION_TO_6:
    case PLANE_DISPLAY_OPTION_TO_7:
    case PLANE_DISPLAY_OPTION_TO_8:
    case PLANE_DISPLAY_OPTION_TO_9:
    case PLANE_DISPLAY_OPTION_TO_10:
    case PLANE_DISPLAY_OPTION_TO_11:
    case PLANE_DISPLAY_OPTION_TO_12:
	if (setDisplayAction (display, o, value))
	    return TRUE;
	break;
    default:
	break;
    }

    return FALSE;
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

static void
planeDisplayInitOptions (PlaneDisplay *pd,
			 Display      *display)
{
    CompOption *o;
    char *str;

    o = &pd->opt[PLANE_DISPLAY_OPTION_LEFT];
    o->name			  = "plane_left";
    o->shortDesc		  = N_("Plane Left");
    o->longDesc			  = N_("Plane left");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = planeLeft;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = PLANE_LEFT_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (PLANE_LEFT_KEY_DEFAULT));

    o = &pd->opt[PLANE_DISPLAY_OPTION_RIGHT];
    o->name			  = "plane_right";
    o->shortDesc		  = N_("Plane Right");
    o->longDesc			  = N_("Plane right");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = planeRight;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = PLANE_RIGHT_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (PLANE_RIGHT_KEY_DEFAULT));

    o = &pd->opt[PLANE_DISPLAY_OPTION_DOWN];
    o->name			  = "plane_down";
    o->shortDesc		  = N_("Plane Down");
    o->longDesc			  = N_("Plane down");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = planeDown;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = PLANE_DOWN_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (PLANE_DOWN_KEY_DEFAULT));

    o = &pd->opt[PLANE_DISPLAY_OPTION_UP];
    o->name			  = "plane_up";
    o->shortDesc		  = N_("Plane Up");
    o->longDesc			  = N_("Plane up");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = planeUp;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = PLANE_UP_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (PLANE_UP_KEY_DEFAULT));

#define PLANE_TO_SHORT        N_("Plane To Face %d")
#define PLANE_TO_LONG         N_("Plane to face %d")
#define PLANE_TO_WINDOW_SHORT N_("Plane To Face %d with Window")
#define PLANE_TO_WINDOW_LONG  N_("Plane to face %d and bring active " \
				  "window along")

#define PLANE_TO_OPTION(n)						 \
    o = &pd->opt[PLANE_DISPLAY_OPTION_TO_ ## n];			 \
    o->name			  = "plane_to_" #n;			 \
    asprintf (&str, PLANE_TO_SHORT, n);				 \
    o->shortDesc		  = str;				 \
    asprintf (&str, PLANE_TO_LONG, n);					 \
    o->longDesc			  = str;				 \
    o->type			  = CompOptionTypeAction;		 \
    o->value.action.initiate	  = planeTo;				 \
    o->value.action.terminate	  = 0;					 \
    o->value.action.bell	  = FALSE;				 \
    o->value.action.edgeMask	  = 0;					 \
    o->value.action.state	  = CompActionStateInitKey;		 \
    o->value.action.state	 |= CompActionStateInitButton;		 \
    o->value.action.type	  = CompBindingTypeNone;

    PLANE_TO_OPTION (1);
    PLANE_TO_OPTION (2);
    PLANE_TO_OPTION (3);
    PLANE_TO_OPTION (4);
    PLANE_TO_OPTION (5);
    PLANE_TO_OPTION (6);
    PLANE_TO_OPTION (7);
    PLANE_TO_OPTION (8);
    PLANE_TO_OPTION (9);
    PLANE_TO_OPTION (10);
    PLANE_TO_OPTION (11);
    PLANE_TO_OPTION (12);
}

static Bool
planeInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    PlaneDisplay *pd;

    pd = malloc (sizeof (PlaneDisplay));
    if (!pd)
	return FALSE;

    pd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (pd->screenPrivateIndex < 0)
    {
	free (pd);
	return FALSE;
    }

    planeDisplayInitOptions (pd, d->display);

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

    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_LEFT].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_RIGHT].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_DOWN].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_UP].value.action);

    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_1].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_2].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_3].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_4].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_5].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_6].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_7].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_8].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_9].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_10].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_11].value.action);
    addScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_12].value.action);

    WRAP (ps, s, paintTransformedScreen, planePaintTransformedScreen);
    WRAP (ps, s, preparePaintScreen, planePreparePaintScreen);
    WRAP (ps, s, donePaintScreen, planeDonePaintScreen);
    WRAP (ps, s, paintScreen, planePaintScreen);
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
    PLANE_DISPLAY (s->display);

    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_LEFT].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_RIGHT].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_DOWN].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_UP].value.action);

    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_1].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_2].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_3].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_4].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_5].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_6].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_7].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_8].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_9].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_10].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_11].value.action);
    removeScreenAction (s, &pd->opt[PLANE_DISPLAY_OPTION_TO_12].value.action);

    UNWRAP (ps, s, paintTransformedScreen);
    UNWRAP (ps, s, preparePaintScreen);
    UNWRAP (ps, s, donePaintScreen);
    UNWRAP (ps, s, paintScreen);
    UNWRAP (ps, s, windowGrabNotify);
    UNWRAP (ps, s, windowUngrabNotify);

    free (ps);
}

static Bool
planeInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
planeFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
planeGetVersion (CompPlugin *plugin,
		 int	    version)
{
    return ABIVERSION;
}

CompPluginDep planeDeps[] = {
    { CompPluginRuleBefore, "scale" },
    { CompPluginRuleBefore, "switcher" }
};

CompPluginFeature planeFeatures[] = {
    { "largedesktop" }
};

CompPluginVTable planeVTable = {
    "plane",
    N_("Desktop Plane"),
    N_("Place windows on a plane"),
    planeGetVersion,
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
    NULL, /* planeSetScreenOption, */
    planeDeps,
    sizeof (planeDeps) / sizeof (planeDeps[0]),
    planeFeatures,
    sizeof (planeFeatures) / sizeof (planeFeatures[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &planeVTable;
}
