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

#define MOVE_INITIATE_BUTTON_DEFAULT    Button1
#define MOVE_INITIATE_MODIFIERS_DEFAULT CompAltMask

#define MOVE_OPACITY_DEFAULT 100
#define MOVE_OPACITY_MIN     1
#define MOVE_OPACITY_MAX     100

#define MOVE_CONSTRAIN_Y_DEFAULT TRUE

#define MOVE_SNAPOFF_MAXIMIZED_DEFAULT TRUE

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

typedef struct _MoveDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompWindow *w;
    KeyCode    key[NUM_KEYS];
} MoveDisplay;

#define MOVE_SCREEN_OPTION_INITIATE	     0
#define MOVE_SCREEN_OPTION_OPACITY	     1
#define MOVE_SCREEN_OPTION_CONSTRAIN_Y	     2
#define MOVE_SCREEN_OPTION_SNAPOFF_MAXIMIZED 3
#define MOVE_SCREEN_OPTION_NUM		     4

typedef struct _MoveScreen {
    CompOption opt[MOVE_SCREEN_OPTION_NUM];

    PaintWindowProc paintWindow;

    int grabIndex;

    Cursor moveCursor;

    GLushort moveOpacity;

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

static CompOption *
moveGetScreenOptions (CompScreen *screen,
		      int	 *count)
{
    MOVE_SCREEN (screen);

    *count = NUM_OPTIONS (ms);
    return ms->opt;
}

static Bool
moveSetScreenOption (CompScreen      *screen,
		     char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    MOVE_SCREEN (screen);

    o = compFindOption (ms->opt, NUM_OPTIONS (ms), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case MOVE_SCREEN_OPTION_INITIATE:
	if (addScreenBinding (screen, &value->bind))
	{
	    removeScreenBinding (screen, &o->value.bind);

	    if (compSetBindingOption (o, value))
		return TRUE;
	}
	break;
    case MOVE_SCREEN_OPTION_OPACITY:
	if (compSetIntOption (o, value))
	{
	    ms->moveOpacity = (o->value.i * OPAQUE) / 100;
	    return TRUE;
	}
	break;
    case MOVE_SCREEN_OPTION_CONSTRAIN_Y:
	if (compSetBoolOption (o, value))
	    return TRUE;
	break;
    case MOVE_SCREEN_OPTION_SNAPOFF_MAXIMIZED:
	if (compSetBoolOption (o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
moveScreenInitOptions (MoveScreen *ms,
		       Display    *display)
{
    CompOption *o;

    o = &ms->opt[MOVE_SCREEN_OPTION_INITIATE];
    o->name			     = "initiate";
    o->shortDesc		     = "Initiate Window Move";
    o->longDesc			     = "Start moving window";
    o->type			     = CompOptionTypeBinding;
    o->value.bind.type		     = CompBindingTypeButton;
    o->value.bind.u.button.modifiers = MOVE_INITIATE_MODIFIERS_DEFAULT;
    o->value.bind.u.button.button    = MOVE_INITIATE_BUTTON_DEFAULT;

    o = &ms->opt[MOVE_SCREEN_OPTION_OPACITY];
    o->name	  = "opacity";
    o->shortDesc  = "Opacity";
    o->longDesc	  = "Opacity level of moving windows";
    o->type	  = CompOptionTypeInt;
    o->value.i	  = MOVE_OPACITY_DEFAULT;
    o->rest.i.min = MOVE_OPACITY_MIN;
    o->rest.i.max = MOVE_OPACITY_MAX;

    o = &ms->opt[MOVE_SCREEN_OPTION_CONSTRAIN_Y];
    o->name	  = "constrain_y";
    o->shortDesc  = "Constrain Y";
    o->longDesc	  = "Constrain Y coordinate to workspace area";
    o->type	  = CompOptionTypeBool;
    o->value.b    = MOVE_CONSTRAIN_Y_DEFAULT;

    o = &ms->opt[MOVE_SCREEN_OPTION_SNAPOFF_MAXIMIZED];
    o->name      = "snapoff_maximized";
    o->shortDesc = "Snapoff maximized windows";
    o->longDesc  = "Snapoff and auto unmaximized maximized windows "
	"when dragging";
    o->type      = CompOptionTypeBool;
    o->value.b   = MOVE_SNAPOFF_MAXIMIZED_DEFAULT;
}

static void
moveInitiate (CompWindow   *w,
	      int	   x,
	      int	   y,
	      unsigned int state)
{
    MOVE_DISPLAY (w->screen->display);
    MOVE_SCREEN (w->screen);

    if (otherScreenGrabExist (w->screen, "move", 0))
	return;

    if (md->w)
	return;

    if (w->type & (CompWindowTypeDesktopMask |
		   CompWindowTypeDockMask    |
		   CompWindowTypeFullscreenMask))
	return;

    if (w->attrib.override_redirect)
	return;

    md->w = w;

    lastPointerX = x;
    lastPointerY = y;

    ms->origState = w->state;

    ms->snapBackY = w->serverY;
    ms->snapOffY  = y;

    if (!ms->grabIndex)
	ms->grabIndex = pushScreenGrab (w->screen, ms->moveCursor, "move");

    if (ms->grabIndex)
	(w->screen->windowGrabNotify) (w, x, y, state,
				       CompWindowGrabMoveMask |
				       CompWindowGrabButtonMask);
}

static void
moveTerminate (CompDisplay *d)
{
    MOVE_DISPLAY (d);

    if (md->w)
    {
	MOVE_SCREEN (md->w->screen);

	(md->w->screen->windowUngrabNotify) (md->w);

	syncWindowPosition (md->w);

	if (ms->grabIndex)
	{
	    removeScreenGrab (md->w->screen, ms->grabIndex, NULL);
	    ms->grabIndex = 0;
	}

	md->w = 0;
    }
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
	int	   pointerDx, pointerDy, dx, dy, wx, wy;
	Bool	   move = TRUE;
	Bool	   warp = TRUE;

	MOVE_DISPLAY (s->display);

	w = md->w;

	pointerDx = xRoot - lastPointerX;
	pointerDy = yRoot - lastPointerY;

	if (w->type & CompWindowTypeFullscreenMask)
	{
	    wx = wy = dx = dy = 0;
	}
	else
	{
	    unsigned int state = w->state;
	    int		 min, max;

	    wx = dx = pointerDx;
	    wy = dy = pointerDy;

	    if (ms->opt[MOVE_SCREEN_OPTION_CONSTRAIN_Y].value.b)
	    {
		min = s->workArea.y + w->input.top;
		max = s->workArea.y + s->workArea.height;

		if (w->attrib.y + dy < min)
		    dy = min - w->attrib.y;
		else if (w->attrib.y + dy > max)
		    dy = max - w->attrib.y;

		wy = dy;
	    }

	    if (ms->opt[MOVE_SCREEN_OPTION_SNAPOFF_MAXIMIZED].value.b)
	    {
		if ((w->state & CompWindowStateMaximizedVertMask) &&
		    (w->state & CompWindowStateMaximizedHorzMask))
		{
		    warp = FALSE;

		    if (yRoot - ms->snapOffY >= SNAP_OFF)
		    {
			w->saveMask |= CWX | CWY;

			w->saveWc.x = xRoot - (w->saveWc.width >> 1);
			w->saveWc.y = yRoot + (w->input.top >> 1);

			move = FALSE;

			unmaximizeWindow (w);

			ms->snapOffY = ms->snapBackY;
		    }
		}
		else if ((ms->origState & CompWindowStateMaximizedVertMask) &&
			 (ms->origState & CompWindowStateMaximizedHorzMask))
		{
		    if (yRoot - ms->snapBackY < SNAP_BACK)
		    {
			if (!otherScreenGrabExist (s, "move", 0))
			{
			    maximizeWindow (w);

			    wy  = s->workArea.y + (w->input.top >> 1);
			    wy += w->sizeHints.height_inc >> 1;
			    wy -= lastPointerY;

			    if (wy > 0)
				wy = 0;

			    move = FALSE;
			}
		    }
		}
	    }

	    if (state & CompWindowStateMaximizedVertMask)
	    {
		min = s->workArea.y + w->input.top;
		max = s->workArea.y + s->workArea.height -
		    w->input.bottom - w->height;

		if (w->attrib.y + pointerDy < min)
		    dy = min - w->attrib.y;
		else if (w->attrib.y + pointerDy > max)
		    dy = max - w->attrib.y;

		if (warp)
		    wy = dy;
	    }

	    if (state & CompWindowStateMaximizedHorzMask)
	    {
		if (w->attrib.x > s->width || w->attrib.x + w->width < 0)
		    return;

		min = s->workArea.x + w->input.left;
		max = s->workArea.x + s->workArea.width -
		    w->input.right - w->width;

		if (w->attrib.x + pointerDx < min)
		    dx = min - w->attrib.x;
		else if (w->attrib.x + pointerDx > max)
		    dx = max - w->attrib.x;

		if (warp)
		    wx = dx;
	    }
	}

	if (move)
	    moveWindow (md->w, dx, dy, TRUE, FALSE);

	if (wx != pointerDx || wy != pointerDy)
	    warpPointer (s->display,
			 (lastPointerX + wx) - pointerX,
			 (lastPointerY + wy) - pointerY);
    }
}

static void
moveHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompScreen *s;

    MOVE_DISPLAY (d);

    switch (event->type) {
    case KeyPress:
    case KeyRelease:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    MOVE_SCREEN (s);

	    if (eventMatches (d, event, &ms->opt[MOVE_SCREEN_OPTION_INITIATE]))
	    {
		CompWindow *w;

		w = findWindowAtScreen (s, event->xkey.window);
		if (w)
		    moveInitiate (w,
				  pointerX,
				  pointerY,
				  event->xkey.state);
	    }
	    else if (eventTerminates (d, event,
				      &ms->opt[MOVE_SCREEN_OPTION_INITIATE]))
		moveTerminate (d);

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
    case ButtonPress:
    case ButtonRelease:
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s)
	{
	    CompWindow *w;

	    MOVE_SCREEN (s);

	    if (eventMatches (d, event, &ms->opt[MOVE_SCREEN_OPTION_INITIATE]))
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		    moveInitiate (w,
				  pointerX,
				  pointerY,
				  event->xbutton.state);
	    }
	    else if (eventTerminates (d, event,
				      &ms->opt[MOVE_SCREEN_OPTION_INITIATE]))
		moveTerminate (d);
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
		    int	xRoot, yRoot;

		    s = w->screen;

		    if (event->xclient.data.l[2] == WmMoveResizeMoveKeyboard)
		    {
			warpPointer (d,
				     (w->attrib.x + w->width  / 2) - pointerX,
				     (w->attrib.y + w->height / 2) - pointerY);

			moveInitiate (w, xRoot, yRoot, 0);
		    }
		    else
		    {
			unsigned int state;
			Window	     root, child;
			int	     i;

			XQueryPointer (d->display, w->screen->root,
				       &root, &child, &xRoot, &yRoot,
				       &i, &i, &state);

			/* TODO: not only button 1 */
			if (state & Button1Mask)
			{
			    moveInitiate (w,
					  event->xclient.data.l[0],
					  event->xclient.data.l[1],
					  state);

			    moveHandleMotionEvent (w->screen, xRoot, yRoot);
			}
		    }
		}
	    }
	}
	break;
    case DestroyNotify:
	if (md->w && md->w->id == event->xdestroywindow.window)
	    moveTerminate (d);
	break;
    case UnmapNotify:
	if (md->w && md->w->id == event->xunmap.window)
	    moveTerminate (d);
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

	if (md->w == w && ms->moveOpacity != OPAQUE)
	{
	    /* modify opacity of windows that are not active */
	    sAttrib = *attrib;
	    attrib  = &sAttrib;

	    sAttrib.opacity = (sAttrib.opacity * ms->moveOpacity) >> 16;
	}
    }

    UNWRAP (ms, s, paintWindow);
    status = (*s->paintWindow) (w, attrib, region, mask);
    WRAP (ms, s, paintWindow, movePaintWindow);

    return status;
}

static Bool
moveInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    MoveDisplay *md;
    int	        i;

    md = malloc (sizeof (MoveDisplay));
    if (!md)
	return FALSE;

    md->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (md->screenPrivateIndex < 0)
    {
	free (md);
	return FALSE;
    }

    md->w = 0;

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

    ms->moveOpacity = (MOVE_OPACITY_DEFAULT * OPAQUE) / 100;

    moveScreenInitOptions (ms, s->display->display);

    ms->moveCursor = XCreateFontCursor (s->display->display, XC_plus);

    addScreenBinding (s, &ms->opt[MOVE_SCREEN_OPTION_INITIATE].value.bind);

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

    free (ms);
}

static Bool
moveInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
moveFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginVTable moveVTable = {
    "move",
    "Move Window",
    "Move window",
    moveInit,
    moveFini,
    moveInitDisplay,
    moveFiniDisplay,
    moveInitScreen,
    moveFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    moveGetScreenOptions,
    moveSetScreenOption,
    NULL,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &moveVTable;
}
