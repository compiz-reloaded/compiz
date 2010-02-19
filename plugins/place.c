/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2005 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <compiz-core.h>

static CompMetadata placeMetadata;

static int displayPrivateIndex;

typedef struct _PlaceDisplay {
    int		    screenPrivateIndex;

    Atom            fullPlacementAtom;

    HandleEventProc handleEvent;
} PlaceDisplay;

#define PLACE_MODE_CASCADE  0
#define PLACE_MODE_CENTERED 1
#define PLACE_MODE_SMART    2
#define PLACE_MODE_MAXIMIZE 3
#define PLACE_MODE_RANDOM   4
#define PLACE_MODE_POINTER  5
#define PLACE_MODE_LAST     PLACE_MODE_POINTER

#define PLACE_MOMODE_CURRENT    0
#define PLACE_MOMODE_POINTER    1
#define PLACE_MOMODE_ACTIVEWIN  2
#define PLACE_MOMODE_FULLSCREEN 3
#define PLACE_MOMODE_LAST       PLACE_MOMODE_FULLSCREEN

#define PLACE_SCREEN_OPTION_WORKAROUND         0
#define PLACE_SCREEN_OPTION_MODE               1
#define PLACE_SCREEN_OPTION_MULTIOUTPUT_MODE   2
#define PLACE_SCREEN_OPTION_FORCE_PLACEMENT    3
#define PLACE_SCREEN_OPTION_POSITION_MATCHES   4
#define PLACE_SCREEN_OPTION_POSITION_X_VALUES  5
#define PLACE_SCREEN_OPTION_POSITION_Y_VALUES  6
#define PLACE_SCREEN_OPTION_POSITION_CONSTRAIN 7
#define PLACE_SCREEN_OPTION_VIEWPORT_MATCHES   8
#define PLACE_SCREEN_OPTION_VIEWPORT_X_VALUES  9
#define PLACE_SCREEN_OPTION_VIEWPORT_Y_VALUES  10
#define PLACE_SCREEN_OPTION_MODE_MATCHES       11
#define PLACE_SCREEN_OPTION_MODE_MODES         12
#define PLACE_SCREEN_OPTION_NUM                13

typedef struct _PlaceScreen {
    int	windowPrivateIndex;

    CompOption opt[PLACE_SCREEN_OPTION_NUM];

    AddSupportedAtomsProc           addSupportedAtoms;
    PlaceWindowProc                 placeWindow;
    ValidateWindowResizeRequestProc validateWindowResizeRequest;
    WindowGrabNotifyProc            windowGrabNotify;

    int               prevWidth;
    int               prevHeight;
    int               strutWindowCount;
    CompTimeoutHandle resChangeFallbackHandle;
} PlaceScreen;

typedef struct _PlaceWindow {
    Bool       savedOriginal;
    XRectangle origVpRelRect; /* saved original window rectangle with position
                                 relative to viewport */
    int        prevServerX;
    int        prevServerY;
} PlaceWindow;

#define GET_PLACE_DISPLAY(d)					   \
    ((PlaceDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define PLACE_DISPLAY(d)		     \
    PlaceDisplay *pd = GET_PLACE_DISPLAY (d)

#define GET_PLACE_SCREEN(s, pd)					       \
    ((PlaceScreen *) (s)->base.privates[(pd)->screenPrivateIndex].ptr)

#define PLACE_SCREEN(s)							   \
    PlaceScreen *ps = GET_PLACE_SCREEN (s, GET_PLACE_DISPLAY (s->display))

#define GET_PLACE_WINDOW(w, ps)					\
    ((PlaceWindow *) (w)->base.privates[(ps)->windowPrivateIndex].ptr)

#define PLACE_WINDOW(w)				         \
    PlaceWindow *pw = GET_PLACE_WINDOW  (w,		         \
		      GET_PLACE_SCREEN  (w->screen,	         \
		      GET_PLACE_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

typedef enum {
    NoPlacement = 0,
    PlaceOnly,
    ConstrainOnly,
    PlaceAndConstrain,
    PlaceOverParent,
    PlaceCenteredOnScreen
} PlacementStrategy;

/* helper macro that filters out windows irrelevant for placement */
#define IS_PLACE_RELEVANT(wi, w)                                        \
    ((w != wi) &&                                                       \
     (wi->attrib.map_state == IsViewable || wi->shaded) &&              \
     (!wi->attrib.override_redirect) &&                                 \
     (!(wi->wmType & (CompWindowTypeDockMask | CompWindowTypeDesktopMask))))

/* helper macros to get the full dimensions of a window,
   including decorations */
#define BORDER_WIDTH(w)  ((w)->input.left + (w)->input.right + \
			  2 * (w)->serverBorderWidth)
#define BORDER_HEIGHT(w) ((w)->input.top + (w)->input.bottom + \
			  2 * (w)->serverBorderWidth)

#define WIN_FULL_X(w) ((w)->serverX - (w)->input.left)
#define WIN_FULL_Y(w) ((w)->serverY - (w)->input.top)
#define WIN_FULL_W(w) ((w)->serverWidth + BORDER_WIDTH (w))
#define WIN_FULL_H(w) ((w)->serverHeight + BORDER_HEIGHT (w))

static Bool
placeMatchXYValue (CompWindow *w,
		   CompOption *matches,
		   CompOption *xValues,
		   CompOption *yValues,
		   CompOption *constrain,
		   int	      *x,
		   int	      *y,
		   Bool       *keepInWorkarea)
{
    int i, min;

    if (w->type & CompWindowTypeDesktopMask)
	return FALSE;

    min = MIN (matches->value.list.nValue, xValues->value.list.nValue);
    min = MIN (min, yValues->value.list.nValue);

    for (i = 0; i < min; i++)
    {
	if (matchEval (&matches->value.list.value[i].match, w))
	{
	    *x = xValues->value.list.value[i].i;
	    *y = yValues->value.list.value[i].i;

	    if (keepInWorkarea)
	    {
		if (constrain && constrain->value.list.nValue > i)
		    *keepInWorkarea = constrain->value.list.value[i].b;
		else
		    *keepInWorkarea = TRUE;
	    }

	    return TRUE;
	}
    }

    return FALSE;
}

static Bool
placeMatchPosition (CompWindow *w,
		    int	       *x,
		    int	       *y,
		    Bool       *keepInWorkarea)
{
    PLACE_SCREEN (w->screen);

    return placeMatchXYValue (w,
			      &ps->opt[PLACE_SCREEN_OPTION_POSITION_MATCHES],
			      &ps->opt[PLACE_SCREEN_OPTION_POSITION_X_VALUES],
			      &ps->opt[PLACE_SCREEN_OPTION_POSITION_Y_VALUES],
			      &ps->opt[PLACE_SCREEN_OPTION_POSITION_CONSTRAIN],
			      x,
			      y,
			      keepInWorkarea);
}

static Bool
placeMatchViewport (CompWindow *w,
		    int	       *x,
		    int	       *y)
{
    PLACE_SCREEN (w->screen);

    if (placeMatchXYValue (w,
			   &ps->opt[PLACE_SCREEN_OPTION_VIEWPORT_MATCHES],
			   &ps->opt[PLACE_SCREEN_OPTION_VIEWPORT_X_VALUES],
			   &ps->opt[PLACE_SCREEN_OPTION_VIEWPORT_Y_VALUES],
			   NULL,
			   x,
			   y,
			   NULL))
    {
	/* Viewport matches are given 1-based, so we need to adjust that */
	*x -= 1;
	*y -= 1;

	return TRUE;
    }

    return FALSE;
}

static void
placeWindowGrabNotify (CompWindow   *w,
		       int	     x,
		       int	     y,
		       unsigned int state,
		       unsigned int mask)
{
    CompScreen *s = w->screen;

    PLACE_SCREEN (s);
    PLACE_WINDOW (w);

    if (pw->savedOriginal)
    {
	int i;

	/* look for move or resize grab */
	for (i = 0; i < s->maxGrab; i++)
	    if (s->grabs[i].active &&
		(strcmp ("move", s->grabs[i].name) == 0 ||
		 strcmp ("resize", s->grabs[i].name) == 0))
		break;

	/* only reset savedOriginal if move or resize is active */
	if (i < s->maxGrab)
	    pw->savedOriginal = FALSE;
    }

    UNWRAP (ps, w->screen, windowGrabNotify);
    (*w->screen->windowGrabNotify) (w, x, y, state, mask);
    WRAP (ps, w->screen, windowGrabNotify, placeWindowGrabNotify);
}

static CompOption *
placeGetScreenOptions (CompPlugin *plugin,
		       CompScreen *screen,
		       int	  *count)
{
    PLACE_SCREEN (screen);

    *count = NUM_OPTIONS (ps);
    return ps->opt;
}

static Bool
placeSetScreenOption (CompPlugin      *plugin,
		      CompScreen      *screen,
		      const char      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    PLACE_SCREEN (screen);

    o = compFindOption (ps->opt, NUM_OPTIONS (ps), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case PLACE_SCREEN_OPTION_POSITION_MATCHES:
    case PLACE_SCREEN_OPTION_VIEWPORT_MATCHES:
    case PLACE_SCREEN_OPTION_MODE_MATCHES:
	if (compSetOptionList (o, value))
	{
	    int i;

	    for (i = 0; i < o->value.list.nValue; i++)
		matchUpdate (screen->display, &o->value.list.value[i].match);

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
placeSendWindowMaximizationRequest (CompWindow *w)
{
    XEvent      xev;
    CompDisplay *d = w->screen->display;

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = d->display;
    xev.xclient.format  = 32;

    xev.xclient.message_type = d->winStateAtom;
    xev.xclient.window	     = w->id;

    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = d->winStateMaximizedHorzAtom;
    xev.xclient.data.l[2] = d->winStateMaximizedVertAtom;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent (d->display, w->screen->root, FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}

static Bool
placeGetPointerPosition (CompScreen *s,
			 int        *x,
			 int        *y)
{
    Window       wDummy;
    int          iDummy;
    unsigned int uiDummy;

    /* this means a server roundtrip, which kind of sucks; thus
       this code should be removed as soon as we have software
       cursor rendering and thus have a cached pointer coordinate */
    return XQueryPointer (s->display->display, s->root,
			  &wDummy, &wDummy, x, y,
			  &iDummy, &iDummy, &uiDummy);
}

static Bool
rectangleIntersect (XRectangle *src1,
		    XRectangle *src2,
		    XRectangle *dest)
{
    int destX, destY;
    int destW, destH;

    destX = MAX (src1->x, src2->x);
    destY = MAX (src1->y, src2->y);
    destW = MIN (src1->x + src1->width, src2->x + src2->width) - destX;
    destH = MIN (src1->y + src1->height, src2->y + src2->height) - destY;

    if (destW <= 0 || destH <= 0)
    {
	dest->width  = 0;
	dest->height = 0;
	return FALSE;
    }

    dest->x = destX;
    dest->y = destY;
    dest->width = destW;
    dest->height = destH;

    return TRUE;
}

static void
getWindowExtentsRect (CompWindow *w,
		      XRectangle *rect)
{
    rect->x      = WIN_FULL_X (w);
    rect->y      = WIN_FULL_Y (w);
    rect->width  = WIN_FULL_W (w);
    rect->height = WIN_FULL_H (w);
}

static Bool
rectOverlapsWindow (XRectangle   *rect,
		    CompWindow   **windows,
		    unsigned int winCount)
{
    unsigned int i;
    XRectangle   dest;

    for (i = 0; i < winCount; i++)
    {
	CompWindow *other = windows[i];
	XRectangle otherRect;

	switch (other->type) {
	case CompWindowTypeDockMask:
	case CompWindowTypeSplashMask:
	case CompWindowTypeDesktopMask:
	case CompWindowTypeDialogMask:
	case CompWindowTypeModalDialogMask:
	case CompWindowTypeFullscreenMask:
	case CompWindowTypeUnknownMask:
	    break;
	case CompWindowTypeNormalMask:
	case CompWindowTypeUtilMask:
	case CompWindowTypeToolbarMask:
	case CompWindowTypeMenuMask:
	    getWindowExtentsRect (other, &otherRect);

	    if (rectangleIntersect (rect, &otherRect, &dest))
		return TRUE;
	    break;
	}
    }

    return FALSE;
}

static int
compareLeftmost (const void *a,
		 const void *b)
{
    CompWindow *aw = *((CompWindow **) a);
    CompWindow *bw = *((CompWindow **) b);
    int	       ax, bx;

    ax = WIN_FULL_X (aw);
    bx = WIN_FULL_X (bw);

    if (ax < bx)
	return -1;
    else if (ax > bx)
	return 1;
    else
	return 0;
}

static int
compareTopmost (const void *a,
		const void *b)
{
    CompWindow *aw = *((CompWindow **) a);
    CompWindow *bw = *((CompWindow **) b);
    int	       ay, by;

    ay = WIN_FULL_Y (aw);
    by = WIN_FULL_Y (bw);

    if (ay < by)
	return -1;
    else if (ay > by)
	return 1;
    else
	return 0;
}

static int
compareNorthWestCorner (const void *a,
			const void *b)
{
    CompWindow *aw = *((CompWindow **) a);
    CompWindow *bw = *((CompWindow **) b);
    int	       fromOriginA;
    int	       fromOriginB;
    int	       ax, ay, bx, by;

    ax = WIN_FULL_X (aw);
    ay = WIN_FULL_Y (aw);

    bx = WIN_FULL_X (bw);
    by = WIN_FULL_Y (bw);

    /* probably there's a fast good-enough-guess we could use here. */
    fromOriginA = sqrt (ax * ax + ay * ay);
    fromOriginB = sqrt (bx * bx + by * by);

    if (fromOriginA < fromOriginB)
	return -1;
    else if (fromOriginA > fromOriginB)
	return 1;
    else
	return 0;
}

static void
centerTileRectInArea (XRectangle *rect,
		      XRectangle *workArea)
{
    int fluff;

    /* The point here is to tile a window such that "extra"
     * space is equal on either side (i.e. so a full screen
     * of windows tiled this way would center the windows
     * as a group)
     */

    fluff   = (workArea->width % (rect->width + 1)) / 2;
    rect->x = workArea->x + fluff;

    fluff   = (workArea->height % (rect->height + 1)) / 3;
    rect->y = workArea->y + fluff;
}

static Bool
rectFitsInWorkarea (XRectangle *workArea,
		    XRectangle *rect)
{
    if (rect->x < workArea->x)
	return FALSE;

    if (rect->y < workArea->y)
	return FALSE;

    if (rect->x + rect->width > workArea->x + workArea->width)
	return FALSE;

    if (rect->y + rect->height > workArea->y + workArea->height)
	return FALSE;

    return TRUE;
}

/* Find the leftmost, then topmost, empty area on the workspace
 * that can contain the new window.
 *
 * Cool feature to have: if we can't fit the current window size,
 * try shrinking the window (within geometry constraints). But
 * beware windows such as Emacs with no sane minimum size, we
 * don't want to create a 1x1 Emacs.
 */
static Bool
placeCascadeFindFirstFit (CompWindow   *w,
			  CompWindow   **windows,
			  unsigned int winCount,
			  XRectangle   *workArea,
			  int          x,
			  int          y,
			  int          *newX,
			  int          *newY)
{
    /* This algorithm is limited - it just brute-force tries
     * to fit the window in a small number of locations that are aligned
     * with existing windows. It tries to place the window on
     * the bottom of each existing window, and then to the right
     * of each existing window, aligned with the left/top of the
     * existing window in each of those cases.
     */
    Bool         retval = FALSE;
    unsigned int i, allocSize = winCount * sizeof (CompWindow *);
    CompWindow   **belowSorted, **rightSorted;
    XRectangle   rect;

    belowSorted = malloc (allocSize);
    if (!belowSorted)
	return FALSE;

    rightSorted = malloc (allocSize);
    if (!rightSorted)
    {
	free (belowSorted);
	return FALSE;
    }

    /* Below each window */
    memcpy (belowSorted, windows, allocSize);
    qsort (belowSorted, winCount, sizeof (CompWindow *), compareLeftmost);
    qsort (belowSorted, winCount, sizeof (CompWindow *), compareTopmost);

    /* To the right of each window */
    memcpy (rightSorted, windows, allocSize);
    qsort (rightSorted, winCount, sizeof (CompWindow *), compareTopmost);
    qsort (rightSorted, winCount, sizeof (CompWindow *), compareLeftmost);

    getWindowExtentsRect (w, &rect);

    centerTileRectInArea (&rect, workArea);

    if (rectFitsInWorkarea (workArea, &rect) &&
	!rectOverlapsWindow (&rect, windows, winCount))
    {
	*newX = rect.x + w->input.left;
	*newY = rect.y + w->input.top;

	retval = TRUE;
    }

    if (!retval)
    {
	/* try below each window */
	for (i = 0; i < winCount && !retval; i++)
	{
	    XRectangle outerRect;

	    getWindowExtentsRect (belowSorted[i], &outerRect);

	    rect.x = outerRect.x;
	    rect.y = outerRect.y + outerRect.height;

	    if (rectFitsInWorkarea (workArea, &rect) &&
		!rectOverlapsWindow (&rect, belowSorted, winCount))
	    {
		*newX = rect.x + w->input.left;
		*newY = rect.y + w->input.top;
		retval = TRUE;
	    }
	}
    }

    if (!retval)
    {
	/* try to the right of each window */
	for (i = 0; i < winCount && !retval; i++)
	{
	    XRectangle outerRect;

	    getWindowExtentsRect (rightSorted[i], &outerRect);

	    rect.x = outerRect.x + outerRect.width;
	    rect.y = outerRect.y;

	    if (rectFitsInWorkarea (workArea, &rect) &&
		!rectOverlapsWindow (&rect, rightSorted, winCount))
	    {
		*newX = rect.x + w->input.left;
		*newY = rect.y + w->input.top;
		retval = TRUE;
	    }
	}
    }

    free (belowSorted);
    free (rightSorted);

    return retval;
}

static void
placeCascadeFindNext (CompWindow   *w,
		      CompWindow   **windows,
		      unsigned int winCount,
		      XRectangle   *workArea,
		      int          x,
		      int          y,
		      int          *newX,
		      int          *newY)
{
    CompWindow   **sorted;
    unsigned int allocSize = winCount * sizeof (CompWindow *);
    int          cascadeX, cascadeY;
    int          xThreshold, yThreshold;
    int          winWidth, winHeight;
    int          i, cascadeStage;

    sorted = malloc (allocSize);
    if (!sorted)
	return;

    memcpy (sorted, windows, allocSize);
    qsort (sorted, winCount, sizeof (CompWindow *), compareNorthWestCorner);

    /* This is a "fuzzy" cascade algorithm.
     * For each window in the list, we find where we'd cascade a
     * new window after it. If a window is already nearly at that
     * position, we move on.
     */

    /* arbitrary-ish threshold, honors user attempts to
     * manually cascade.
     */
#define CASCADE_FUZZ 15

    xThreshold = MAX (w->input.left, CASCADE_FUZZ);
    yThreshold = MAX (w->input.top, CASCADE_FUZZ);

    /* Find furthest-SE origin of all workspaces.
     * cascade_x, cascade_y are the target position
     * of NW corner of window frame.
     */

    cascadeX = MAX (0, workArea->x);
    cascadeY = MAX (0, workArea->y);

    /* Find first cascade position that's not used. */

    winWidth = WIN_FULL_W (w);
    winHeight = WIN_FULL_H (w);

    cascadeStage = 0;
    for (i = 0; i < winCount; i++)
    {
	CompWindow *wi = sorted[i];
	int	   wx, wy;

	/* we want frame position, not window position */
	wx = WIN_FULL_X (wi);
	wy = WIN_FULL_Y (wi);

	if (abs (wx - cascadeX) < xThreshold &&
	    abs (wy - cascadeY) < yThreshold)
	{
	    /* This window is "in the way", move to next cascade
	     * point. The new window frame should go at the origin
	     * of the client window we're stacking above.
	     */
	    wx = cascadeX = wi->serverX;
	    wy = cascadeY = wi->serverY;

	    /* If we go off the screen, start over with a new cascade */
	    if ((cascadeX + winWidth > workArea->x + workArea->width) ||
		(cascadeY + winHeight > workArea->y + workArea->height))
	    {
		cascadeX = MAX (0, workArea->x);
		cascadeY = MAX (0, workArea->y);

#define CASCADE_INTERVAL 50 /* space between top-left corners of cascades */

		cascadeStage += 1;
		cascadeX += CASCADE_INTERVAL * cascadeStage;

		/* start over with a new cascade translated to the right,
		 * unless we are out of space
		 */
		if (cascadeX + winWidth < workArea->x + workArea->width)
		{
		    i = 0;
		    continue;
		}
		else
		{
		    /* All out of space, this cascade_x won't work */
		    cascadeX = MAX (0, workArea->x);
		    break;
		}
	    }
	}
	else
	{
	    /* Keep searching for a further-down-the-diagonal window. */
	}
    }

    /* cascade_x and cascade_y will match the last window in the list
     * that was "in the way" (in the approximate cascade diagonal)
     */

    free (sorted);

    /* Convert coords to position of window, not position of frame. */
    *newX = cascadeX + w->input.left;
    *newY = cascadeY + w->input.top;
}

static void
placeCascade (CompWindow *w,
	      XRectangle *workArea,
	      int        *x,
	      int        *y)
{
    CompWindow   **windows;
    CompWindow   *wi;
    unsigned int count = 0;

    /* get the total window count */
    for (wi = w->screen->windows; wi; wi = wi->next)
	count++;

    windows = malloc (sizeof (CompWindow *) * count);
    if (!windows)
	return;

    /* Find windows that matter (not minimized, on same workspace
     * as placed window, may be shaded - if shaded we pretend it isn't
     * for placement purposes)
     */
    for (wi = w->screen->windows, count = 0; wi; wi = wi->next)
    {
	if (!IS_PLACE_RELEVANT (wi, w))
	    continue;

	if (wi->type & (CompWindowTypeFullscreenMask |
			CompWindowTypeUnknownMask))
	    continue;

	if (wi->serverX >= workArea->x + workArea->width  ||
	    wi->serverX + wi->serverWidth <= workArea->x  ||
	    wi->serverY >= workArea->y + workArea->height ||
	    wi->serverY + wi->serverHeight <= workArea->y)
	    continue;

	windows[count++] = wi;
    }

    if (!placeCascadeFindFirstFit (w, windows, count, workArea, *x, *y, x, y))
    {
	/* if the window wasn't placed at the origin of screen,
	 * cascade it onto the current screen
	 */
	placeCascadeFindNext (w, windows, count, workArea, *x, *y, x, y);
    }

    free (windows);
}

static void
placeCentered (CompWindow *w,
	       XRectangle *workArea,
	       int	  *x,
	       int	  *y)
{
    *x = workArea->x + (workArea->width - w->serverWidth) / 2;
    *y = workArea->y + (workArea->height - w->serverHeight) / 2;
}

static void
placeRandom (CompWindow *w,
	     XRectangle *workArea,
	     int	*x,
	     int	*y)
{
    int remainX, remainY;

    *x = workArea->x;
    *y = workArea->y;

    remainX = workArea->width - w->serverWidth;
    if (remainX > 0)
	*x += rand () % remainX;

    remainY = workArea->height - w->serverHeight;
    if (remainY > 0)
	*y += rand () % remainY;
}

static void
placePointer (CompWindow *w,
	      XRectangle *workArea,
	      int	 *x,
	      int	 *y)
{
    int xPointer, yPointer;

    if (placeGetPointerPosition (w->screen, &xPointer, &yPointer))
    {
	*x = xPointer - (w->serverWidth / 2) - w->serverBorderWidth;
	*y = yPointer - (w->serverHeight / 2) - w->serverBorderWidth;
    }
    else
    {
	/* use centered as fallback */
	placeCentered (w, workArea, x, y);
    }
}

/* overlap types */
#define NONE    0
#define H_WRONG -1
#define W_WRONG -2

static void
placeSmart (CompWindow *w,
	    XRectangle *workArea,
	    int        *x,
	    int        *y)
{
    /*
     * SmartPlacement by Cristian Tibirna (tibirna@kde.org)
     * adapted for kwm (16-19jan98) and for kwin (16Nov1999) using (with
     * permission) ideas from fvwm, authored by
     * Anthony Martin (amartin@engr.csulb.edu).
     * Xinerama supported added by Balaji Ramani (balaji@yablibli.com)
     * with ideas from xfce.
     * adapted for Compiz by Bellegarde Cedric (gnumdk(at)gmail.com)
     */
    CompWindow *wi;
    int        overlap, minOverlap = 0;
    int        xOptimal, yOptimal;
    int        possible;

    /* temp coords */
    int cxl, cxr, cyt, cyb;
    /* temp coords */
    int xl,  xr,  yt,  yb;
    /* temp holder */
    int basket;
    /* CT lame flag. Don't like it. What else would do? */
    Bool firstPass = TRUE;

    /* get the maximum allowed windows space */
    int xTmp = workArea->x;
    int yTmp = workArea->y;

    /* client gabarit */
    int cw = WIN_FULL_W (w) - 1;
    int ch = WIN_FULL_H (w) - 1;

    xOptimal = xTmp;
    yOptimal = yTmp;

    /* loop over possible positions */
    do
    {
	/* test if enough room in x and y directions */
	if (yTmp + ch > workArea->y + workArea->height && ch < workArea->height)
	    overlap = H_WRONG; /* this throws the algorithm to an exit */
	else if (xTmp + cw > workArea->x + workArea->width)
	    overlap = W_WRONG;
	else
	{
	    overlap = NONE; /* initialize */

	    cxl = xTmp;
	    cxr = xTmp + cw;
	    cyt = yTmp;
	    cyb = yTmp + ch;

	    for (wi = w->screen->windows; wi; wi = wi->next)
	    {
		if (!IS_PLACE_RELEVANT (wi, w))
		    continue;

		xl = WIN_FULL_X (wi);
		yt = WIN_FULL_Y (wi);
		xr = WIN_FULL_X (wi) + WIN_FULL_W (wi);
		yb = WIN_FULL_Y (wi) + WIN_FULL_H (wi);

		/* if windows overlap, calc the overall overlapping */
		if (cxl < xr && cxr > xl && cyt < yb && cyb > yt)
		{
		    xl = MAX (cxl, xl);
		    xr = MIN (cxr, xr);
		    yt = MAX (cyt, yt);
		    yb = MIN (cyb, yb);

		    if (wi->state & CompWindowStateAboveMask)
			overlap += 16 * (xr - xl) * (yb - yt);
		    else if (wi->state & CompWindowStateBelowMask)
			overlap += 0;
		    else
			overlap += (xr - xl) * (yb - yt);
		}
	    }
	}

	/* CT first time we get no overlap we stop */
	if (overlap == NONE)
	{
	    xOptimal = xTmp;
	    yOptimal = yTmp;
	    break;
	}

	if (firstPass)
	{
	    firstPass  = FALSE;
	    minOverlap = overlap;
	}
	/* CT save the best position and the minimum overlap up to now */
	else if (overlap >= NONE && overlap < minOverlap)
	{
	    minOverlap = overlap;
	    xOptimal = xTmp;
	    yOptimal = yTmp;
	}

	/* really need to loop? test if there's any overlap */
	if (overlap > NONE)
	{
	    possible = workArea->x + workArea->width;

	    if (possible - cw > xTmp)
		possible -= cw;

	    /* compare to the position of each client on the same desk */
	    for (wi = w->screen->windows; wi; wi = wi->next)
	    {
		if (!IS_PLACE_RELEVANT (wi, w))
		    continue;

		xl = WIN_FULL_X (wi);
		yt = WIN_FULL_Y (wi);
		xr = WIN_FULL_X (wi) + WIN_FULL_W (wi);
		yb = WIN_FULL_X (wi) + WIN_FULL_H (wi);

		/* if not enough room above or under the current
		 * client determine the first non-overlapped x position
		 */
		if (yTmp < yb && yt < ch + yTmp)
		{
		    if (xr > xTmp && possible > xr)
			possible = xr;

		    basket = xl - cw;
		    if (basket > xTmp && possible > basket)
			possible = basket;
		}
	    }
	    xTmp = possible;
	}
	/* else ==> not enough x dimension (overlap was wrong on horizontal) */
	else if (overlap == W_WRONG)
	{
	    xTmp     = workArea->x;
	    possible = workArea->y + workArea->height;

	    if (possible - ch > yTmp)
		possible -= ch;

	    /* test the position of each window on the desk */
	    for (wi = w->screen->windows; wi; wi = wi->next)
	    {
		if (!IS_PLACE_RELEVANT (wi, w))
		    continue;

		xl = WIN_FULL_X (wi);
		yt = WIN_FULL_Y (wi);
		xr = WIN_FULL_X (wi) + WIN_FULL_W (wi);
		yb = WIN_FULL_X (wi) + WIN_FULL_H (wi);

		/* if not enough room to the left or right of the current
		 * client determine the first non-overlapped y position
		 */
		if (yb > yTmp && possible > yb)
		    possible = yb;

		basket = yt - ch;
		if (basket > yTmp && possible > basket)
		    possible = basket;
	    }
	    yTmp = possible;
	}
    }
    while (overlap != NONE && overlap != H_WRONG &&
	   yTmp < workArea->y + workArea->height);

    if (ch >= workArea->height)
	yOptimal = workArea->y;

    *x = xOptimal + w->input.left;
    *y = yOptimal + w->input.top;
}

static PlacementStrategy
placeGetStrategyForWindow (CompWindow *w)
{
    CompMatch *match;

    PLACE_SCREEN (w->screen);

    if (w->type & (CompWindowTypeDockMask | CompWindowTypeDesktopMask    |
		   CompWindowTypeUtilMask | CompWindowTypeToolbarMask    |
		   CompWindowTypeMenuMask | CompWindowTypeFullscreenMask |
		   CompWindowTypeUnknownMask))
    {
	/* assume the app knows best how to place these */
	return NoPlacement;
    }

    if (w->wmType & (CompWindowTypeDockMask | CompWindowTypeDesktopMask))
    {
	/* see above */
	return NoPlacement;
    }

    match = &ps->opt[PLACE_SCREEN_OPTION_FORCE_PLACEMENT].value.match;
    if (!matchEval (match, w))
    {
	if ((w->type & CompWindowTypeNormalMask) ||
	    ps->opt[PLACE_SCREEN_OPTION_WORKAROUND].value.b)
	{
	    /* Only accept USPosition on non-normal windows if workarounds are
	     * enabled because apps claiming the user set -geometry for a
	     * dialog or dock are most likely wrong
	     */
	    if (w->sizeHints.flags & USPosition)
		return ConstrainOnly;
	}

	if (w->sizeHints.flags & PPosition)
	    return ConstrainOnly;
    }

   if (w->transientFor &&
	(w->type & (CompWindowTypeDialogMask |
		    CompWindowTypeModalDialogMask)))
    {
	CompWindow *parent = findWindowAtScreen (w->screen, w->transientFor);
	if (parent && parent->managed)
	    return PlaceOverParent;
    }

    if (w->type & (CompWindowTypeDialogMask |
		   CompWindowTypeModalDialogMask |
		   CompWindowTypeSplashMask))
    {
	return PlaceCenteredOnScreen;
    }

    return PlaceAndConstrain;
}

static CompOutput *
placeGetPlacementOutput (CompWindow        *w,
			 int               mode,
			 PlacementStrategy strategy,
			 int               x,
			 int               y)
{
    CompScreen *s = w->screen;
    int        output = -1;
    int        multiMode;

    PLACE_SCREEN (s);

    /* short cut: it makes no sense to determine a placement
       output if there is only one output */
    if (s->nOutputDev == 1)
	return &s->outputDev[0];

    switch (strategy) {
    case PlaceOverParent:
	{
	    CompWindow *parent;

	    parent = findWindowAtScreen (s, w->transientFor);
	    if (parent)
		output = outputDeviceForWindow (parent);
	}
	break;
    case ConstrainOnly:
	output = outputDeviceForGeometry (s, x, y,
					  w->serverWidth,
					  w->serverHeight,
					  w->serverBorderWidth);
	break;
    default:
	break;
    }

    if (output >= 0)
	return &s->outputDev[output];

    multiMode = ps->opt[PLACE_SCREEN_OPTION_MULTIOUTPUT_MODE].value.i;
    /* force 'output with pointer' mode for placement under pointer */
    if (mode == PLACE_MODE_POINTER)
	multiMode = PLACE_MOMODE_POINTER;

    switch (multiMode) {
    case PLACE_MOMODE_CURRENT:
	output = s->currentOutputDev;
	break;
    case PLACE_MOMODE_POINTER:
	{
	    int xPointer, yPointer;

	    if (placeGetPointerPosition (s, &xPointer, &yPointer))
		output = outputDeviceForPoint (s, xPointer, yPointer);
	}
	break;
    case PLACE_MOMODE_ACTIVEWIN:
	{
	    CompWindow *active;

	    active = findWindowAtScreen (s, s->display->activeWindow);
	    if (active)
		output = outputDeviceForWindow (active);
	}
	break;
    case PLACE_MOMODE_FULLSCREEN:
	/* only place on fullscreen output if not placing centered, as the
	   constraining will move the window away from the center otherwise */
	if (strategy != PlaceCenteredOnScreen)
	    return &s->fullscreenOutput;
	break;
    }

    if (output < 0)
	output = s->currentOutputDev;

    return &s->outputDev[output];
}

static int
placeGetPlacementMode (CompWindow *w)
{
    CompListValue *matches, *modes;
    int           i, min;

    PLACE_SCREEN (w->screen);

    matches = &ps->opt[PLACE_SCREEN_OPTION_MODE_MATCHES].value.list;
    modes   = &ps->opt[PLACE_SCREEN_OPTION_MODE_MODES].value.list;
    min     = MIN (matches->nValue, modes->nValue);

    for (i = 0; i < min; i++)
	if (matchEval (&matches->value[i].match, w))
	    return modes->value[i].i;

    return ps->opt[PLACE_SCREEN_OPTION_MODE].value.i;
}

static void
placeConstrainToWorkarea (CompWindow *w,
			  XRectangle *workArea,
			  int        *x,
			  int        *y)
{
    CompWindowExtents extents;
    int               delta;

    extents.left   = *x - w->input.left;
    extents.top    = *y - w->input.top;
    extents.right  = extents.left + WIN_FULL_W (w);
    extents.bottom = extents.top + WIN_FULL_H (w);

    delta = workArea->x + workArea->width - extents.right;
    if (delta < 0)
	extents.left += delta;

    delta = workArea->x - extents.left;
    if (delta > 0)
	extents.left  += delta;

    delta = workArea->y + workArea->height - extents.bottom;
    if (delta < 0)
	extents.top += delta;

    delta = workArea->y - extents.top;
    if (delta > 0)
	extents.top += delta;

    *x = extents.left + w->input.left;
    *y = extents.top  + w->input.top;
}

static Bool
placeDoWindowPlacement (CompWindow *w,
			int        x,
			int        y,
			int        *newX,
			int        *newY)
{
    CompScreen        *s = w->screen;
    XRectangle        workArea;
    int               targetVpX, targetVpY;
    CompOutput        *output;
    PlacementStrategy strategy;
    Bool	      keepInWorkarea;
    int               mode;

    if (placeMatchPosition (w, &x, &y, &keepInWorkarea))
    {
	strategy = keepInWorkarea ? ConstrainOnly : NoPlacement;
    }
    else
    {
	strategy = placeGetStrategyForWindow (w);
	if (strategy == NoPlacement)
	    return FALSE;
    }

    mode     = placeGetPlacementMode (w);
    output   = placeGetPlacementOutput (w, mode, strategy, x, y);
    workArea = output->workArea;

    targetVpX = w->initialViewportX;
    targetVpY = w->initialViewportY;

    if (strategy == PlaceOverParent)
    {
	CompWindow *parent;

	parent = findWindowAtScreen (s, w->transientFor);
	if (parent)
	{
	    /* center over parent horizontally */
	    x = parent->serverX + (parent->serverWidth / 2) -
		(w->serverWidth / 2);

	    /* "visually" center vertically, leaving twice as much space below
	       as on top */
	    y = parent->serverY + (parent->serverHeight - w->serverHeight) / 3;

	    /* put top of child's frame, not top of child's client */
	    y += w->input.top;

	    /* if parent is visible on current viewport, clip to work area;
	       don't constrain further otherwise */
	    if (parent->serverX < parent->screen->width   &&
		parent->serverX + parent->serverWidth > 0 &&
		parent->serverY < parent->screen->height  &&
		parent->serverY + parent->serverHeight > 0)
	    {
		defaultViewportForWindow (parent, &targetVpX, &targetVpY);
		strategy = ConstrainOnly;
	    }
	    else
	    {
		strategy = NoPlacement;
	    }
	}
    }

    if (strategy == PlaceCenteredOnScreen)
    {
	/* center window on current output device */

	x = output->region.extents.x1;
	y = output->region.extents.y1;

	x += (output->width - w->serverWidth) / 2;
	y += (output->height - w->serverHeight) / 2;

	strategy = ConstrainOnly;
    }

    workArea.x += (targetVpX - s->x) * s->width;
    workArea.y += (targetVpY - s->y) * s->height;

    if (strategy == PlaceOnly || strategy == PlaceAndConstrain)
    {
	switch (mode) {
	case PLACE_MODE_CASCADE:
	    placeCascade (w, &workArea, &x, &y);
	    break;
	case PLACE_MODE_CENTERED:
	    placeCentered (w, &workArea, &x, &y);
	    break;
	case PLACE_MODE_RANDOM:
	    placeRandom (w, &workArea, &x, &y);
	    break;
	case PLACE_MODE_POINTER:
	    placePointer (w, &workArea, &x, &y);
	    break;
	case PLACE_MODE_MAXIMIZE:
	    placeSendWindowMaximizationRequest (w);
	    break;
	case PLACE_MODE_SMART:
	    placeSmart (w, &workArea, &x, &y);
	    break;
	}

	/* When placing to the fullscreen output, constrain to one
	   output nevertheless */
	if (output->id == ~0)
	{
	    int id;

	    id = outputDeviceForGeometry (s, x, y,
					  w->serverWidth,
					  w->serverHeight,
					  w->serverBorderWidth);
	    getWorkareaForOutput (s, id, &workArea);

	    workArea.x += (targetVpX - s->x) * s->width;
	    workArea.y += (targetVpY - s->y) * s->height;
	}

	/* Maximize windows if they are too big for their work area (bit of
	 * a hack here). Assume undecorated windows probably don't intend to
	 * be maximized.
	 */
	if ((w->actions & MAXIMIZE_STATE) == MAXIMIZE_STATE &&
	    (w->mwmDecor & (MwmDecorAll | MwmDecorTitle))   &&
	    !(w->state & CompWindowStateFullscreenMask))
	{
	    if (WIN_FULL_W (w) >= workArea.width &&
		WIN_FULL_H (w) >= workArea.height)
	    {
		placeSendWindowMaximizationRequest (w);
	    }
	}
    }

    if (strategy == ConstrainOnly || strategy == PlaceAndConstrain)
	placeConstrainToWorkarea (w, &workArea, &x, &y);

    *newX = x;
    *newY = y;

    return TRUE;
}

static XRectangle
placeDoValidateWindowResizeRequest (CompWindow     *w,
				    unsigned int   *mask,
				    XWindowChanges *xwc,
				    Bool           sizeOnly,
				    Bool           clampToViewport)
{
    CompScreen *s = w->screen;
    XRectangle workArea;
    int        x, y, left, right, top, bottom;
    int        output;

    if (clampToViewport)
    {
	/* left, right, top, bottom target coordinates, clamped to viewport
	   sizes as we don't need to validate movements to other viewports;
	   we are only interested in inner-viewport movements */
	x = xwc->x % s->width;
	if ((x + xwc->width) < 0)
	    x += s->width;

	y = xwc->y % s->height;
	if ((y + xwc->height) < 0)
	    y += s->height;
    }
    else
    {
	x = xwc->x;
	y = xwc->y;
    }

    left   = x - w->input.left;
    right  = left + xwc->width + BORDER_WIDTH (w);
    top    = y - w->input.top;
    bottom = top + xwc->height + BORDER_HEIGHT (w);

    output = outputDeviceForGeometry (s,
				      xwc->x, xwc->y,
				      xwc->width, xwc->height,
				      w->serverBorderWidth);

    getWorkareaForOutput (s, output, &workArea);

    if (clampToViewport &&
	xwc->width >= workArea.width &&
	xwc->height >= workArea.height)
    {
	if ((w->actions & MAXIMIZE_STATE) == MAXIMIZE_STATE &&
	    (w->mwmDecor & (MwmDecorAll | MwmDecorTitle))   &&
	    !(w->state & CompWindowStateFullscreenMask))
	{
	    placeSendWindowMaximizationRequest (w);
	}
    }

    if ((right - left) > workArea.width)
    {
	left  = workArea.x;
	right = left + workArea.width;
    }
    else
    {
	if (left < workArea.x)
	{
	    right += workArea.x - left;
	    left  = workArea.x;
	}

	if (right > (workArea.x + workArea.width))
	{
	    left -= right - (workArea.x + workArea.width);
	    right = workArea.x + workArea.width;
	}
    }

    if ((bottom - top) > workArea.height)
    {
	top    = workArea.y;
	bottom = top + workArea.height;
    }
    else
    {
	if (top < workArea.y)
	{
	    bottom += workArea.y - top;
	    top    = workArea.y;
	}

	if (bottom > (workArea.y + workArea.height))
	{
	    top   -= bottom - (workArea.y + workArea.height);
	    bottom = workArea.y + workArea.height;
	}
    }

    /* bring left/right/top/bottom to actual window coordinates */
    left   += w->input.left;
    right  -= w->input.right + 2 * w->serverBorderWidth;
    top    += w->input.top;
    bottom -= w->input.bottom + 2 * w->serverBorderWidth;

    /* always validate position if the application changed only its size,
       as it might become partially offscreen because of that */
    if (!(*mask & (CWX | CWY)) && (*mask & (CWWidth | CWHeight)))
	sizeOnly = FALSE;

    if ((right - left) != xwc->width)
    {
	xwc->width = right - left;
	*mask      |= CWWidth;
	sizeOnly   = FALSE;
    }

    if ((bottom - top) != xwc->height)
    {
	xwc->height = bottom - top;
	*mask       |= CWHeight;
	sizeOnly    = FALSE;
    }

    if (!sizeOnly)
    {
	if (left != x)
	{
	    xwc->x += left - x;
	    *mask  |= CWX;
	}

	if (top != y)
	{
	    xwc->y += top - y;
	    *mask  |= CWY;
	}
    }

    return workArea;
}

static void
placeValidateWindowResizeRequest (CompWindow     *w,
				  unsigned int   *mask,
				  XWindowChanges *xwc,
				  unsigned int   source)
{
    CompScreen *s = w->screen;
    Bool       sizeOnly = FALSE;

    PLACE_SCREEN (s);

    UNWRAP (ps, s, validateWindowResizeRequest);
    (*s->validateWindowResizeRequest) (w, mask, xwc, source);
    WRAP (ps, s, validateWindowResizeRequest,
	  placeValidateWindowResizeRequest);

    if (*mask == 0)
	return;

    if (source == ClientTypePager)
	return;

    if (w->state & CompWindowStateFullscreenMask)
	return;

    if (w->wmType & (CompWindowTypeDockMask |
		     CompWindowTypeDesktopMask))
	return;

    /* do nothing if the window was already (at least partially) offscreen */
    if (w->serverX < 0                         ||
	w->serverX + w->serverWidth > s->width ||
	w->serverY < 0                         ||
	w->serverY + w->serverHeight > s->height)
    {
	return;
    }

    if (w->sizeHints.flags & USPosition)
    {
	/* only respect USPosition on normal windows if
	   workarounds are disabled, reason see above */
	if (ps->opt[PLACE_SCREEN_OPTION_WORKAROUND].value.b ||
	    (w->type & CompWindowTypeNormalMask))
	{
	    /* try to keep the window position intact for USPosition -
	       obviously we can't do that if we need to change the size */
	    sizeOnly = TRUE;
	}
    }

    placeDoValidateWindowResizeRequest (w, mask, xwc, sizeOnly, TRUE);
}

static unsigned int
placeAddSupportedAtoms (CompScreen   *s,
			Atom         *atoms,
			unsigned int size)
{
    unsigned int count;

    PLACE_DISPLAY (s->display);
    PLACE_SCREEN (s);

    UNWRAP (ps, s, addSupportedAtoms);
    count = (*s->addSupportedAtoms) (s, atoms, size);
    WRAP (ps, s, addSupportedAtoms, placeAddSupportedAtoms);

    if (count < size)
	atoms[count++] = pd->fullPlacementAtom;

    return count;
}

static Bool
placePlaceWindow (CompWindow *w,
		  int        x,
		  int        y,
		  int        *newX,
		  int        *newY)
{
    CompScreen *s = w->screen;
    Bool       status;

    PLACE_SCREEN (s);

    UNWRAP (ps, s, placeWindow);
    status = (*s->placeWindow) (w, x, y, newX, newY);
    WRAP (ps, s, placeWindow, placePlaceWindow);

    if (!status)
    {
	int viewportX, viewportY;

	if (!placeDoWindowPlacement (w, x, y, newX, newY))
	{
	    *newX = x;
	    *newY = y;
	}

	if (placeMatchViewport (w, &viewportX, &viewportY))
	{
	    viewportX = MAX (MIN (viewportX, s->hsize - 1), 0);
	    viewportY = MAX (MIN (viewportY, s->vsize - 1), 0);

	    x = *newX % s->width;
	    if (x < 0)
		x += s->width;
	    y = *newY % s->height;
	    if (y < 0)
		y += s->height;

	    *newX = x + (viewportX - s->x) * s->width;
	    *newY = y + (viewportY - s->y) * s->height;
	}
    }

    return TRUE;
}

static void
placeDoHandleScreenSizeChange (CompScreen *s,
			       Bool firstPass)
{
    CompWindow     *w;
    int            vpX, vpY;   /* holds x and y index of a window's target vp */
    int            shiftX, shiftY;
    XRectangle     vpRelRect;
    XRectangle     winRect;
    XRectangle     workArea;
    int            pivotX, pivotY;
    unsigned int   mask;
    XWindowChanges xwc;
    int            curVpOffsetX = s->x * s->width;
    int            curVpOffsetY = s->y * s->height;

    PLACE_SCREEN (s);

    if (firstPass)
	ps->strutWindowCount = 0;
    else
    {
	if (ps->resChangeFallbackHandle)
	{
	    compRemoveTimeout (ps->resChangeFallbackHandle);
	    ps->resChangeFallbackHandle = 0;
	}
    }

    for (w = s->windows; w; w = w->next)
    {
	if (!w->managed)
	    continue;

	PLACE_WINDOW (w);

	if (firstPass)
	{
	    /* count windows that have struts */
	    if (w->struts)
		ps->strutWindowCount++;

	    /* for maximized/fullscreen windows, keep window coords before
	       screen resize, as they are sometimes automatically changed
	       before the 2nd pass */
	    if (w->type & CompWindowTypeFullscreenMask ||
		(w->state & (CompWindowStateMaximizedVertMask |
			     CompWindowStateMaximizedHorzMask)))
	    {
		pw->prevServerX = w->serverX;
		pw->prevServerY = w->serverY;
	    }
	}

	if (w->wmType & (CompWindowTypeDockMask |
			 CompWindowTypeDesktopMask))
	    continue;

	/* Also in the first pass, we save the rectangle of those windows
	   that don't already have a saved one. So, skip those that do. */
	if (firstPass && pw->savedOriginal)
	    continue;

	winRect.x      = w->serverX;
	winRect.y      = w->serverY;
	winRect.width  = w->serverWidth;
	winRect.height = w->serverHeight;

	pivotX = winRect.x;
	pivotY = winRect.y;

	if (w->type & CompWindowTypeFullscreenMask ||
	    (w->state & (CompWindowStateMaximizedVertMask |
			 CompWindowStateMaximizedHorzMask)))
	{
	    if (w->saveMask & CWX)
		winRect.x = w->saveWc.x;
	    if (w->saveMask & CWY)
		winRect.y = w->saveWc.y;
	    if (w->saveMask & CWWidth)
		winRect.width = w->saveWc.width;
	    if (w->saveMask & CWHeight)
		winRect.height = w->saveWc.height;

	    pivotX = pw->prevServerX;
	    pivotY = pw->prevServerY;
	}

	/* calculate target vp x, y index for window's pivot point */
	vpX = pivotX / ps->prevWidth;
	if (pivotX < 0)
	    vpX -= 1;
	vpY = pivotY / ps->prevHeight;
	if (pivotY < 0)
	    vpY -= 1;

	/* if window's target vp is to the left of the leftmost viewport on that
	   row, assign its target vp column as 0 (-s->x rel. to current vp) */
	if (s->x + vpX < 0)
	    vpX = -s->x;

	/* if window's target vp is above the topmost viewport on that column,
	   assign its target vp row as 0 (-s->y rel. to current vp) */
	if (s->y + vpY < 0)
	    vpY = -s->y;

	if (pw->savedOriginal)
	{
	    /* set position/size to saved original rectangle */
	    vpRelRect = pw->origVpRelRect;

	    xwc.x = pw->origVpRelRect.x + vpX * s->width;
	    xwc.y = pw->origVpRelRect.y + vpY * s->height;
	}
	else
	{
	    /* set position/size to window's current rectangle
	       (with position relative to target viewport) */
	    vpRelRect.x = winRect.x - vpX * ps->prevWidth;
	    vpRelRect.y = winRect.y - vpY * ps->prevHeight;
	    vpRelRect.width  = winRect.width;
	    vpRelRect.height = winRect.height;

	    xwc.x = winRect.x;
	    xwc.y = winRect.y;

	    shiftX = vpX * (s->width  - ps->prevWidth);
	    shiftY = vpY * (s->height - ps->prevHeight);

	    /* if coords. relative to viewport are outside new viewport area,
	       shift window left/up so that it falls inside */
	    if (vpRelRect.x >= s->width)
		shiftX -= vpRelRect.x - (s->width - 1);
	    if (vpRelRect.y >= s->height)
		shiftY -= vpRelRect.y - (s->height - 1);

	    if (shiftX)
		xwc.x += shiftX;

	    if (shiftY)
		xwc.y += shiftY;
	}

	mask       = CWX | CWY | CWWidth | CWHeight;
	xwc.width  = vpRelRect.width;
	xwc.height = vpRelRect.height;

	/* Handle non-(0,0) current viewport by shifting by curVpOffsetX,Y,
	   and bring window to (0,0) by shifting by minus its vp offset */

	xwc.x += curVpOffsetX - (s->x + vpX) * s->width;
	xwc.y += curVpOffsetY - (s->y + vpY) * s->height;

	workArea =
	    placeDoValidateWindowResizeRequest (w, &mask, &xwc, FALSE, FALSE);

	xwc.x -= curVpOffsetX - (s->x + vpX) * s->width;
	xwc.y -= curVpOffsetY - (s->y + vpY) * s->height;

	/* Check if the new coordinates are different than current position and
	   size. If not, we can clear the corresponding mask bits. */
	if (xwc.x == winRect.x)
	    mask &= ~CWX;

	if (xwc.y == winRect.y)
	    mask &= ~CWY;

	if (xwc.width == winRect.width)
	    mask &= ~CWWidth;

	if (xwc.height == winRect.height)
	    mask &= ~CWHeight;

	if (!pw->savedOriginal)
	{
	    if (mask)
	    {
		/* save window geometry (relative to viewport) so that it
		can be restored later */
		pw->savedOriginal = TRUE;
		pw->origVpRelRect = vpRelRect;

		if (firstPass)
		{
		    /* If first pass, store updated pos. */
		    pw->origVpRelRect.x = xwc.x % s->width;
		    if (pw->origVpRelRect.x < 0)
			pw->origVpRelRect.x += s->width;
		    pw->origVpRelRect.y = xwc.y % s->height;
		    if (pw->origVpRelRect.y < 0)
			pw->origVpRelRect.y += s->height;
		}
	    }
	}
	else if (pw->origVpRelRect.x + vpX * s->width == xwc.x &&
		 pw->origVpRelRect.y + vpY * s->height == xwc.y &&
		 pw->origVpRelRect.width  == xwc.width &&
		 pw->origVpRelRect.height == xwc.height)
	{
	    /* if size and position is back to original, clear saved rect */
	    pw->savedOriginal = FALSE;
	}

	if (firstPass) /* if first pass, don't actually move the window */
	    continue;

	/* for maximized/fullscreen windows, update saved pos/size */
	if (w->type & CompWindowTypeFullscreenMask ||
	    (w->state & (CompWindowStateMaximizedVertMask |
			 CompWindowStateMaximizedHorzMask)))
	{
	    if (mask & CWX)
	    {
		w->saveWc.x = xwc.x;
		w->saveMask |= CWX;
	    }
	    if (mask & CWY)
	    {
		w->saveWc.y = xwc.y;
		w->saveMask |= CWY;
	    }
	    if (mask & CWWidth)
	    {
		w->saveWc.width = xwc.width;
		w->saveMask |= CWWidth;
	    }
	    if (mask & CWHeight)
	    {
		w->saveWc.height = xwc.height;
		w->saveMask |= CWHeight;
	    }

	    if (w->type & CompWindowTypeFullscreenMask)
	    {
		mask |= CWX | CWY | CWWidth | CWHeight;
		xwc.x = vpX * s->width;
		xwc.y = vpY * s->height;
		xwc.width  = s->width;
		xwc.height = s->height;
	    }
	    else
	    {
		if (w->state & CompWindowStateMaximizedHorzMask)
		{
		    mask |= CWX | CWWidth;
		    xwc.x = vpX * s->width + workArea.x + w->input.left;
		    xwc.width = workArea.width - BORDER_WIDTH (w);
		}
		if (w->state & CompWindowStateMaximizedVertMask)
		{
		    mask |= CWY | CWHeight;
		    xwc.y = vpY * s->height + workArea.y + w->input.top;
		    xwc.height = workArea.height - BORDER_HEIGHT (w);
		}
	    }
	}
	if (mask)
	{
	    /* actually move/resize window in directions given by mask */
	    configureXWindow (w, mask, &xwc);
	}
    }
}

static CompBool
placeHandleScreenSizeChangeFallback (void *closure)
{
    CompScreen *s = (CompScreen *) closure;

    PLACE_SCREEN (s);

    ps->resChangeFallbackHandle = 0;

    /* If countdown is not finished yet (i.e. at least one strut-window didn't
       update its struts), reset the countdown and do the 2nd pass here. */
    if (ps->strutWindowCount > 0) /* no windows with struts found */
    {
	ps->strutWindowCount = 0;
	placeDoHandleScreenSizeChange (s, FALSE);
    }

    return FALSE;
}

static void
placeHandleScreenSizeChange (CompScreen *s,
			     int        width,
			     int        height)
{
    PLACE_SCREEN (s);

    if (s->width == width && s->height == height) /* No change in screen size */
	return;

    ps->prevWidth  = s->width;
    ps->prevHeight = s->height;

    if (ps->resChangeFallbackHandle)
	compRemoveTimeout (ps->resChangeFallbackHandle);

    placeDoHandleScreenSizeChange (s, TRUE); /* 1st pass */

    if (ps->strutWindowCount == 0) /* no windows with struts found */
    {
	ps->resChangeFallbackHandle = 0;

	/* do the 2nd pass right here instead of in placeHandleEvent */
	placeDoHandleScreenSizeChange (s, FALSE);
    }
    else
    {
	/* Start a 4 second fallback timeout, just in case. */
	ps->resChangeFallbackHandle =
	    compAddTimeout (4000, 4500, placeHandleScreenSizeChangeFallback, s);
    }
}

static void
placeHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    PLACE_DISPLAY (d);

    switch (event->type) {
    case ConfigureNotify:
	{
	    CompScreen *s;

	    s = findScreenAtDisplay (d, event->xconfigure.window);
	    if (s)
		placeHandleScreenSizeChange (s,
					     event->xconfigure.width,
					     event->xconfigure.height);
	}
	break;
    case PropertyNotify:
	if (event->xproperty.atom == d->wmStrutAtom ||
	    event->xproperty.atom == d->wmStrutPartialAtom)
	{
	    CompWindow *w;

	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		PLACE_SCREEN (w->screen);

		/* Only do when handling screen size change.
		   ps->strutWindowCount is 0 at any other time */
		if (ps->strutWindowCount > 0 &&
		    updateWindowStruts (w))
		{
		    ps->strutWindowCount--;
		    updateWorkareaForScreen (w->screen);

		    /* if this was the last window with struts */
		    if (!ps->strutWindowCount)
			placeDoHandleScreenSizeChange (w->screen,
						       FALSE); /* 2nd pass */
		}
	    }
	}
	break;
    default:
	break;
    }

    UNWRAP (pd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (pd, d, handleEvent, placeHandleEvent);
}

static Bool
placeInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    PlaceDisplay *pd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    pd = malloc (sizeof (PlaceDisplay));
    if (!pd)
	return FALSE;

    pd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (pd->screenPrivateIndex < 0)
    {
	free (pd);
	return FALSE;
    }

    pd->fullPlacementAtom = XInternAtom (d->display,
					 "_NET_WM_FULL_PLACEMENT", 0);

    d->base.privates[displayPrivateIndex].ptr = pd;

    WRAP (pd, d, handleEvent, placeHandleEvent);

    return TRUE;
}

static void
placeFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    PLACE_DISPLAY (d);

    UNWRAP (pd, d, handleEvent);

    freeScreenPrivateIndex (d, pd->screenPrivateIndex);

    free (pd);
}

static const CompMetadataOptionInfo placeScreenOptionInfo[] = {
    { "workarounds", "bool", 0, 0, 0 },
    { "mode", "int", RESTOSTRING (0, PLACE_MODE_LAST), 0, 0 },
    { "multioutput_mode", "int", RESTOSTRING (0, PLACE_MOMODE_LAST), 0, 0 },
    { "force_placement_match", "match", 0, 0, 0 },
    { "position_matches", "list", "<type>match</type>", 0, 0 },
    { "position_x_values", "list", "<type>int</type>", 0, 0 },
    { "position_y_values", "list", "<type>int</type>", 0, 0 },
    { "position_constrain_workarea", "list", "<type>bool</type>", 0, 0 },
    { "viewport_matches", "list", "<type>match</type>", 0, 0 },
    { "viewport_x_values", "list",
	"<type>int</type><min>1</min><max>32</max>", 0, 0 },
    { "viewport_y_values", "list",
	"<type>int</type><min>1</min><max>32</max>", 0, 0 },
    { "mode_matches", "list", "<type>match</type>", 0, 0 },
    { "mode_modes", "list",
	"<type>int</type>" RESTOSTRING (0, PLACE_MODE_LAST), 0, 0 }
};

static Bool
placeInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    PlaceScreen *ps;

    PLACE_DISPLAY (s->display);

    ps = malloc (sizeof (PlaceScreen));
    if (!ps)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &placeMetadata,
					    placeScreenOptionInfo,
					    ps->opt,
					    PLACE_SCREEN_OPTION_NUM))
    {
	free (ps);
	return FALSE;
    }

    ps->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ps->windowPrivateIndex < 0)
    {
	compFiniScreenOptions (s, ps->opt, PLACE_SCREEN_OPTION_NUM);
	free (ps);
	return FALSE;
    }

    ps->prevWidth = s->width;
    ps->prevHeight = s->height;

    ps->strutWindowCount = 0;
    ps->resChangeFallbackHandle = 0;

    WRAP (ps, s, placeWindow, placePlaceWindow);
    WRAP (ps, s, validateWindowResizeRequest,
	  placeValidateWindowResizeRequest);
    WRAP (ps, s, addSupportedAtoms, placeAddSupportedAtoms);
    WRAP (ps, s, windowGrabNotify, placeWindowGrabNotify);

    s->base.privates[pd->screenPrivateIndex].ptr = ps;

    setSupportedWmHints (s);

    return TRUE;
}

static void
placeFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    PLACE_SCREEN (s);

    if (ps->resChangeFallbackHandle)
	compRemoveTimeout (ps->resChangeFallbackHandle);

    UNWRAP (ps, s, placeWindow);
    UNWRAP (ps, s, validateWindowResizeRequest);
    UNWRAP (ps, s, addSupportedAtoms);
    UNWRAP (ps, s, windowGrabNotify);

    setSupportedWmHints (s);

    compFiniScreenOptions (s, ps->opt, PLACE_SCREEN_OPTION_NUM);

    free (ps);
}

static Bool
placeInitWindow (CompPlugin *p,
		 CompWindow *w)
{
    PlaceWindow *pw;

    PLACE_SCREEN (w->screen);

    pw = malloc (sizeof (PlaceWindow));
    if (!pw)
	return FALSE;

    pw->savedOriginal = FALSE;

    w->base.privates[ps->windowPrivateIndex].ptr = pw;

    return TRUE;
}

static void
placeFiniWindow (CompPlugin *p,
		 CompWindow *w)
{
    PLACE_WINDOW (w);

    free (pw);
}

static CompBool
placeInitObject (CompPlugin *p,
		 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) placeInitDisplay,
	(InitPluginObjectProc) placeInitScreen,
	(InitPluginObjectProc) placeInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
placeFiniObject (CompPlugin *p,
		 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) placeFiniDisplay,
	(FiniPluginObjectProc) placeFiniScreen,
	(FiniPluginObjectProc) placeFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
placeGetObjectOptions (CompPlugin *plugin,
		       CompObject *object,
		       int	  *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) 0, /* GetDisplayOptions */
	(GetPluginObjectOptionsProc) placeGetScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
placeSetObjectOption (CompPlugin      *plugin,
		      CompObject      *object,
		      const char      *name,
		      CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) 0, /* SetDisplayOption */
	(SetPluginObjectOptionProc) placeSetScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
placeInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&placeMetadata,
					 p->vTable->name, 0, 0,
					 placeScreenOptionInfo,
					 PLACE_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&placeMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&placeMetadata, p->vTable->name);

    return TRUE;
}

static void
placeFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&placeMetadata);
}

static CompMetadata *
placeGetMetadata (CompPlugin *plugin)
{
    return &placeMetadata;
}

static CompPluginVTable placeVTable = {
    "place",
    placeGetMetadata,
    placeInit,
    placeFini,
    placeInitObject,
    placeFiniObject,
    placeGetObjectOptions,
    placeSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &placeVTable;
}
