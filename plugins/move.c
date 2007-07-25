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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/cursorfont.h>

#include <compiz.h>

static CompMetadata moveMetadata;

struct _MoveKeys {
    char *name;
    int  dx;
    int  dy;
} mKeys[] = {
    { "Left",  -1,  0 },
    { "Right",  1,  0 },
    { "Up",     0, -1 },
    { "Down",   0,  1 }
};

#define NUM_KEYS (sizeof (mKeys) / sizeof (mKeys[0]))

#define KEY_MOVE_INC 24

#define SNAP_BACK 20
#define SNAP_OFF  100

static int displayPrivateIndex;

#define MOVE_DISPLAY_OPTION_INITIATE	      0
#define MOVE_DISPLAY_OPTION_OPACITY	      1
#define MOVE_DISPLAY_OPTION_CONSTRAIN_Y	      2
#define MOVE_DISPLAY_OPTION_SNAPOFF_MAXIMIZED 3
#define MOVE_DISPLAY_OPTION_LAZY_POSITIONING  4
#define MOVE_DISPLAY_OPTION_NUM		      5

typedef struct _MoveDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[MOVE_DISPLAY_OPTION_NUM];

    CompWindow *w;
    int	       savedX;
    int	       savedY;
    int	       x;
    int	       y;
    Region     region;
    int        status;
    KeyCode    key[NUM_KEYS];

    GLushort moveOpacity;
} MoveDisplay;

typedef struct _MoveScreen {
    PaintWindowProc paintWindow;

    int grabIndex;

    Cursor moveCursor;

    unsigned int origState;

    int	snapOffY;
    int	snapBackY;
} MoveScreen;

#define GET_MOVE_DISPLAY(d)				     \
    ((MoveDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define MOVE_DISPLAY(d)		           \
    MoveDisplay *md = GET_MOVE_DISPLAY (d)

#define GET_MOVE_SCREEN(s, md)				         \
    ((MoveScreen *) (s)->privates[(md)->screenPrivateIndex].ptr)

#define MOVE_SCREEN(s)						        \
    MoveScreen *ms = GET_MOVE_SCREEN (s, GET_MOVE_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static Bool
moveInitiate (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int	      nOption)
{
    CompWindow *w;
    Window     xid;

    MOVE_DISPLAY (d);

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findWindowAtDisplay (d, xid);
    if (w && (w->actions & CompWindowActionMoveMask))
    {
	XRectangle   workArea;
	unsigned int mods;
	int          x, y;

	MOVE_SCREEN (w->screen);

	mods = getIntOptionNamed (option, nOption, "modifiers", 0);

	x = getIntOptionNamed (option, nOption, "x",
			       w->attrib.x + (w->width / 2));
	y = getIntOptionNamed (option, nOption, "y",
			       w->attrib.y + (w->height / 2));

	if (otherScreenGrabExist (w->screen, "move", 0))
	    return FALSE;

	if (md->w)
	    return FALSE;

	if (w->type & (CompWindowTypeDesktopMask |
		       CompWindowTypeDockMask    |
		       CompWindowTypeFullscreenMask))
	    return FALSE;

	if (w->attrib.override_redirect)
	    return FALSE;

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	if (md->region)
	{
	    XDestroyRegion (md->region);
	    md->region = NULL;
	}

	md->status = RectangleOut;

	md->savedX = w->serverX;
	md->savedY = w->serverY;

	md->x = 0;
	md->y = 0;

	lastPointerX = x;
	lastPointerY = y;

	ms->origState = w->state;

	getWorkareaForOutput (w->screen,
			      outputDeviceForWindow (w),
			      &workArea);

	ms->snapBackY = w->serverY - workArea.y;
	ms->snapOffY  = y - workArea.y;

	if (!ms->grabIndex)
	    ms->grabIndex = pushScreenGrab (w->screen, ms->moveCursor, "move");

	if (ms->grabIndex)
	{
	    md->w = w;

	    (w->screen->windowGrabNotify) (w, x, y, mods,
					   CompWindowGrabMoveMask |
					   CompWindowGrabButtonMask);

	    if (state & CompActionStateInitKey)
	    {
		int xRoot, yRoot;

		xRoot = w->attrib.x + (w->width  / 2);
		yRoot = w->attrib.y + (w->height / 2);

		warpPointer (w->screen, xRoot - pointerX, yRoot - pointerY);
	    }

	    if (md->moveOpacity != OPAQUE)
		addWindowDamage (w);
	}
    }

    return FALSE;
}

static Bool
moveTerminate (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    MOVE_DISPLAY (d);

    if (md->w)
    {
	MOVE_SCREEN (md->w->screen);

	if (state & CompActionStateCancel)
	    moveWindow (md->w,
			md->savedX - md->w->attrib.x,
			md->savedY - md->w->attrib.y,
			TRUE, FALSE);

	syncWindowPosition (md->w);

	/* update window attributes as window constraints may have
	   changed - needed e.g. if a maximized window was moved
	   to another output device */
	updateWindowAttributes (md->w, CompStackingUpdateModeNone);

	(md->w->screen->windowUngrabNotify) (md->w);

	if (ms->grabIndex)
	{
	    removeScreenGrab (md->w->screen, ms->grabIndex, NULL);
	    ms->grabIndex = 0;
	}

	if (md->moveOpacity != OPAQUE)
	    addWindowDamage (md->w);

	md->w = 0;
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

/* creates a region containing top and bottom struts. only struts that are
   outside the screen workarea are considered. */
static Region
moveGetYConstrainRegion (CompScreen *s)
{
    CompWindow *w;
    Region     region;
    REGION     r;
    XRectangle workArea;
    BoxRec     extents;
    int	       i;

    region = XCreateRegion ();
    if (!region)
	return NULL;

    r.rects    = &r.extents;
    r.numRects = r.size = 1;

    r.extents.x1 = MINSHORT;
    r.extents.y1 = 0;
    r.extents.x2 = 0;
    r.extents.y2 = s->height;

    XUnionRegion (&r, region, region);

    r.extents.x1 = s->width;
    r.extents.x2 = MAXSHORT;

    XUnionRegion (&r, region, region);

    for (i = 0; i < s->nOutputDev; i++)
    {
	XUnionRegion (&s->outputDev[i].region, region, region);

	getWorkareaForOutput (s, i, &workArea);
	extents = s->outputDev[i].region.extents;

	for (w = s->windows; w; w = w->next)
	{
	    if (!w->mapNum)
		continue;

	    if (w->struts)
	    {
		r.extents.x1 = w->struts->top.x;
		r.extents.y1 = w->struts->top.y;
		r.extents.x2 = r.extents.x1 + w->struts->top.width;
		r.extents.y2 = r.extents.y1 + w->struts->top.height;

		if (r.extents.x1 < extents.x1)
		    r.extents.x1 = extents.x1;
		if (r.extents.x2 > extents.x2)
		    r.extents.x2 = extents.x2;
		if (r.extents.y1 < extents.y1)
		    r.extents.y1 = extents.y1;
		if (r.extents.y2 > extents.y2)
		    r.extents.y2 = extents.y2;

		if (r.extents.x1 < r.extents.x2 && r.extents.y1 < r.extents.y2)
		{
		    if (r.extents.y2 <= workArea.y)
			XSubtractRegion (region, &r, region);
		}

		r.extents.x1 = w->struts->bottom.x;
		r.extents.y1 = w->struts->bottom.y;
		r.extents.x2 = r.extents.x1 + w->struts->bottom.width;
		r.extents.y2 = r.extents.y1 + w->struts->bottom.height;

		if (r.extents.x1 < extents.x1)
		    r.extents.x1 = extents.x1;
		if (r.extents.x2 > extents.x2)
		    r.extents.x2 = extents.x2;
		if (r.extents.y1 < extents.y1)
		    r.extents.y1 = extents.y1;
		if (r.extents.y2 > extents.y2)
		    r.extents.y2 = extents.y2;

		if (r.extents.x1 < r.extents.x2 && r.extents.y1 < r.extents.y2)
		{
		    if (r.extents.y1 >= (workArea.y + workArea.height))
			XSubtractRegion (region, &r, region);
		}
	    }
	}
    }

    return region;
}

static void
moveHandleMotionEvent (CompScreen *s,
		       int	  xRoot,
		       int	  yRoot)
{
    MOVE_SCREEN (s);

    if (ms->grabIndex)
    {
	CompWindow *w;
	int	   dx, dy;
	int	   wX, wY;
	int	   wWidth, wHeight;

	MOVE_DISPLAY (s->display);

	w = md->w;

	wX      = w->serverX;
	wY      = w->serverY;
	wWidth  = w->serverWidth  + w->serverBorderWidth * 2;
	wHeight = w->serverHeight + w->serverBorderWidth * 2;

	md->x += xRoot - lastPointerX;
	md->y += yRoot - lastPointerY;

	if (w->type & CompWindowTypeFullscreenMask)
	{
	    dx = dy = 0;
	}
	else
	{
	    XRectangle workArea;
	    int	       min, max;

	    dx = md->x;
	    dy = md->y;

	    getWorkareaForOutput (s,
				  outputDeviceForWindow (w),
				  &workArea);

	    if (md->opt[MOVE_DISPLAY_OPTION_CONSTRAIN_Y].value.b)
	    {
		if (!md->region)
		    md->region = moveGetYConstrainRegion (s);

		/* make sure that the top frame extents or the top row of
		   pixels are within what is currently our valid screen
		   region */
		if (md->region)
		{
		    int x, y, width, height;
		    int status;

		    x	   = wX + dx - w->input.left;
		    y	   = wY + dy - w->input.top;
		    width  = wWidth + w->input.left + w->input.right;
		    height = w->input.top ? w->input.top : 1;

		    status = XRectInRegion (md->region, x, y, width, height);

		    /* only constrain movement if previous position was valid */
		    if (md->status == RectangleIn)
		    {
			int xStatus = status;

			while (dx && xStatus != RectangleIn)
			{
			    xStatus = XRectInRegion (md->region,
						     x, y - dy,
						     width, height);

			    if (xStatus != RectangleIn)
				dx += (dx < 0) ? 1 : -1;

			    x = wX + dx - w->input.left;
			}

			while (dy && status != RectangleIn)
			{
			    status = XRectInRegion (md->region,
						    x, y,
						    width, height);

			    if (status != RectangleIn)
				dy += (dy < 0) ? 1 : -1;

			    y = wY + dy - w->input.top;
			}
		    }
		    else
		    {
			md->status = status;
		    }
		}
	    }

	    if (md->opt[MOVE_DISPLAY_OPTION_SNAPOFF_MAXIMIZED].value.b)
	    {
		if (w->state & CompWindowStateMaximizedVertMask)
		{
		    if ((yRoot - workArea.y) - ms->snapOffY >= SNAP_OFF)
		    {
			if (!otherScreenGrabExist (s, "move", 0))
			{
			    int width = w->serverWidth;

			    w->saveMask |= CWX | CWY;

			    if (w->saveMask & CWWidth)
				width = w->saveWc.width;

			    w->saveWc.x = xRoot - (width >> 1);
			    w->saveWc.y = yRoot + (w->input.top >> 1);

			    md->x = md->y = 0;

			    maximizeWindow (w, 0);

			    ms->snapOffY = ms->snapBackY;

			    return;
			}
		    }
		}
		else if (ms->origState & CompWindowStateMaximizedVertMask)
		{
		    if ((yRoot - workArea.y) - ms->snapBackY < SNAP_BACK)
		    {
			if (!otherScreenGrabExist (s, "move", 0))
			{
			    int wy;

			    /* update server position before maximizing
			       window again so that it is maximized on
			       correct output */
			    syncWindowPosition (w);

			    maximizeWindow (w, ms->origState);

			    wy  = workArea.y + (w->input.top >> 1);
			    wy += w->sizeHints.height_inc >> 1;

			    warpPointer (s, 0, wy - pointerY);

			    return;
			}
		    }
		}
	    }

	    if (w->state & CompWindowStateMaximizedVertMask)
	    {
		min = workArea.y + w->input.top;
		max = workArea.y + workArea.height - w->input.bottom - wHeight;

		if (wY + dy < min)
		    dy = min - wY;
		else if (wY + dy > max)
		    dy = max - wY;
	    }

	    if (w->state & CompWindowStateMaximizedHorzMask)
	    {
		if (wX > s->width || wX + w->width < 0)
		    return;

		if (wX + wWidth < 0)
		    return;

		min = workArea.x + w->input.left;
		max = workArea.x + workArea.width - w->input.right - wWidth;

		if (wX + dx < min)
		    dx = min - wX;
		else if (wX + dx > max)
		    dx = max - wX;
	    }
	}

	if (dx || dy)
	{
	    moveWindow (w,
			wX + dx - w->attrib.x,
			wY + dy - w->attrib.y,
			TRUE, FALSE);

	    if (md->opt[MOVE_DISPLAY_OPTION_LAZY_POSITIONING].value.b)
	    {
		/* FIXME: This form of lazy positioning is broken and should
		   be replaced asap. Current code exists just to avoid a
		   major performance regression in the 0.5.2 release. */
		w->serverX = w->attrib.x;
		w->serverY = w->attrib.y;
	    }
	    else
	    {
		syncWindowPosition (w);
	    }

	    md->x -= dx;
	    md->y -= dy;
	}
    }
}

static void
moveHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompScreen *s;

    MOVE_DISPLAY (d);

    switch (event->type) {
    case ButtonPress:
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s)
	{
	    MOVE_SCREEN (s);

	    if (ms->grabIndex)
	    {
		moveTerminate (d,
			       &md->opt[MOVE_DISPLAY_OPTION_INITIATE].value.action,
			       0, NULL, 0);
	    }
	}
	break;
    case KeyPress:
    case KeyRelease:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    MOVE_SCREEN (s);

	    if (ms->grabIndex && event->type == KeyPress)
	    {
		int i;

		for (i = 0; i < NUM_KEYS; i++)
		{
		    if (event->xkey.keycode == md->key[i])
		    {
			XWarpPointer (d->display, None, None, 0, 0, 0, 0,
				      mKeys[i].dx * KEY_MOVE_INC,
				      mKeys[i].dy * KEY_MOVE_INC);
			break;
		    }
		}
	    }
	}
	break;
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	    moveHandleMotionEvent (s, pointerX, pointerY);
	break;
    case EnterNotify:
    case LeaveNotify:
	s = findScreenAtDisplay (d, event->xcrossing.root);
	if (s)
	    moveHandleMotionEvent (s, pointerX, pointerY);
	break;
    case ClientMessage:
	if (event->xclient.message_type == d->wmMoveResizeAtom)
	{
	    CompWindow *w;

	    if (event->xclient.data.l[2] == WmMoveResizeMove ||
		event->xclient.data.l[2] == WmMoveResizeMoveKeyboard)
	    {
		w = findWindowAtDisplay (d, event->xclient.window);
		if (w)
		{
		    CompOption o[4];
		    int	       xRoot, yRoot;
		    CompAction *action =
			&md->opt[MOVE_DISPLAY_OPTION_INITIATE].value.action;

		    o[0].type    = CompOptionTypeInt;
		    o[0].name    = "window";
		    o[0].value.i = event->xclient.window;

		    if (event->xclient.data.l[2] == WmMoveResizeMoveKeyboard)
		    {
			moveInitiate (d, action,
				      CompActionStateInitKey,
				      o, 1);
		    }
		    else
		    {
			unsigned int mods;
			Window	     root, child;
			int	     i;

			XQueryPointer (d->display, w->screen->root,
				       &root, &child, &xRoot, &yRoot,
				       &i, &i, &mods);

			/* TODO: not only button 1 */
			if (mods & Button1Mask)
			{
			    o[1].type	 = CompOptionTypeInt;
			    o[1].name	 = "modifiers";
			    o[1].value.i = mods;

			    o[2].type	 = CompOptionTypeInt;
			    o[2].name	 = "x";
			    o[2].value.i = event->xclient.data.l[0];

			    o[3].type	 = CompOptionTypeInt;
			    o[3].name	 = "y";
			    o[3].value.i = event->xclient.data.l[1];

			    moveInitiate (d,
					  action,
					  CompActionStateInitButton,
					  o, 4);

			    moveHandleMotionEvent (w->screen, xRoot, yRoot);
			}
		    }
		}
	    }
	}
	break;
    case DestroyNotify:
	if (md->w && md->w->id == event->xdestroywindow.window)
	    moveTerminate (d,
			   &md->opt[MOVE_DISPLAY_OPTION_INITIATE].value.action,
			   0, NULL, 0);
	break;
    case UnmapNotify:
	if (md->w && md->w->id == event->xunmap.window)
	    moveTerminate (d,
			   &md->opt[MOVE_DISPLAY_OPTION_INITIATE].value.action,
			   0, NULL, 0);
    default:
	break;
    }

    UNWRAP (md, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (md, d, handleEvent, moveHandleEvent);
}

static Bool
movePaintWindow (CompWindow		 *w,
		 const WindowPaintAttrib *attrib,
		 const CompTransform	 *transform,
		 Region			 region,
		 unsigned int		 mask)
{
    WindowPaintAttrib sAttrib;
    CompScreen	      *s = w->screen;
    Bool	      status;

    MOVE_SCREEN (s);

    if (ms->grabIndex)
    {
	MOVE_DISPLAY (s->display);

	if (md->w == w && md->moveOpacity != OPAQUE)
	{
	    /* modify opacity of windows that are not active */
	    sAttrib = *attrib;
	    attrib  = &sAttrib;

	    sAttrib.opacity = (sAttrib.opacity * md->moveOpacity) >> 16;
	}
    }

    UNWRAP (ms, s, paintWindow);
    status = (*s->paintWindow) (w, attrib, transform, region, mask);
    WRAP (ms, s, paintWindow, movePaintWindow);

    return status;
}

static CompOption *
moveGetDisplayOptions (CompPlugin  *plugin,
		       CompDisplay *display,
		       int	   *count)
{
    MOVE_DISPLAY (display);

    *count = NUM_OPTIONS (md);
    return md->opt;
}

static Bool
moveSetDisplayOption (CompPlugin      *plugin,
		      CompDisplay     *display,
		      char	      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    MOVE_DISPLAY (display);

    o = compFindOption (md->opt, NUM_OPTIONS (md), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case MOVE_DISPLAY_OPTION_OPACITY:
	if (compSetIntOption (o, value))
	{
	    md->moveOpacity = (o->value.i * OPAQUE) / 100;
	    return TRUE;
	}
	break;
    default:
	return compSetDisplayOption (display, o, value);
    }

    return FALSE;
}

static const CompMetadataOptionInfo moveDisplayOptionInfo[] = {
    { "initiate", "action", 0, moveInitiate, moveTerminate },
    { "opacity", "int", "<min>0</min><max>100</max>", 0, 0 },
    { "constrain_y", "bool", 0, 0, 0 },
    { "snapoff_maximized", "bool", 0, 0, 0 },
    { "lazy_positioning", "bool", 0, 0, 0 }
};

static Bool
moveInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    MoveDisplay *md;
    int	        i;

    md = malloc (sizeof (MoveDisplay));
    if (!md)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &moveMetadata,
					     moveDisplayOptionInfo,
					     md->opt,
					     MOVE_DISPLAY_OPTION_NUM))
    {
	free (md);
	return FALSE;
    }

    md->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (md->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, md->opt, MOVE_DISPLAY_OPTION_NUM);
	free (md);
	return FALSE;
    }

    md->moveOpacity =
	(md->opt[MOVE_DISPLAY_OPTION_OPACITY].value.i * OPAQUE) / 100;

    md->w      = 0;
    md->region = NULL;
    md->status = RectangleOut;

    for (i = 0; i < NUM_KEYS; i++)
	md->key[i] = XKeysymToKeycode (d->display,
				       XStringToKeysym (mKeys[i].name));

    WRAP (md, d, handleEvent, moveHandleEvent);

    d->privates[displayPrivateIndex].ptr = md;

    return TRUE;
}

static void
moveFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    MOVE_DISPLAY (d);

    freeScreenPrivateIndex (d, md->screenPrivateIndex);

    UNWRAP (md, d, handleEvent);

    compFiniDisplayOptions (d, md->opt, MOVE_DISPLAY_OPTION_NUM);

    free (md);
}

static Bool
moveInitScreen (CompPlugin *p,
		CompScreen *s)
{
    MoveScreen *ms;

    MOVE_DISPLAY (s->display);

    ms = malloc (sizeof (MoveScreen));
    if (!ms)
	return FALSE;

    ms->grabIndex = 0;

    ms->moveCursor = XCreateFontCursor (s->display->display, XC_fleur);

    WRAP (ms, s, paintWindow, movePaintWindow);

    s->privates[md->screenPrivateIndex].ptr = ms;

    return TRUE;
}

static void
moveFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    MOVE_SCREEN (s);

    UNWRAP (ms, s, paintWindow);

    if (ms->moveCursor)
	XFreeCursor (s->display->display, ms->moveCursor);

    free (ms);
}

static Bool
moveInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&moveMetadata,
					 p->vTable->name,
					 moveDisplayOptionInfo,
					 MOVE_DISPLAY_OPTION_NUM,
					 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&moveMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&moveMetadata, p->vTable->name);

    return TRUE;
}

static void
moveFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&moveMetadata);
}

static int
moveGetVersion (CompPlugin *plugin,
		int	   version)
{
    return ABIVERSION;
}

static CompMetadata *
moveGetMetadata (CompPlugin *plugin)
{
    return &moveMetadata;
}

CompPluginVTable moveVTable = {
    "move",
    moveGetVersion,
    moveGetMetadata,
    moveInit,
    moveFini,
    moveInitDisplay,
    moveFiniDisplay,
    moveInitScreen,
    moveFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    moveGetDisplayOptions,
    moveSetDisplayOption,
    0, /* GetScreenOptions */
    0  /* SetScreenOption */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &moveVTable;
}
