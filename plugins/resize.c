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

#define ResizeUpMask    (1L << 0)
#define ResizeDownMask  (1L << 1)
#define ResizeLeftMask  (1L << 2)
#define ResizeRightMask (1L << 3)

#define RESIZE_INITIATE_BUTTON_DEFAULT    Button2
#define RESIZE_INITIATE_BUTTON_MODIFIERS_DEFAULT CompAltMask

#define RESIZE_INITIATE_KEY_DEFAULT           "F8"
#define RESIZE_INITIATE_KEY_MODIFIERS_DEFAULT CompAltMask

struct _ResizeKeys {
    char *name;
    int  dx;
    int  dy;
} rKeys[] = {
    { "Left",  -1,  0 },
    { "Right",  1,  0 },
    { "Up",     0, -1 },
    { "Down",   0,  1 }
};

#define NUM_KEYS (sizeof (rKeys) / sizeof (rKeys[0]))

#define MIN_KEY_WIDTH_INC  24
#define MIN_KEY_HEIGHT_INC 24

#define RESIZE_DISPLAY_OPTION_INITIATE 0
#define RESIZE_DISPLAY_OPTION_NUM      1

static int displayPrivateIndex;

typedef struct _ResizeDisplay {
    CompOption opt[RESIZE_DISPLAY_OPTION_NUM];

    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompWindow	 *w;
    int		 releaseButton;
    unsigned int mask;
    int		 width;
    int		 height;
    KeyCode      key[NUM_KEYS];
} ResizeDisplay;

typedef struct _ResizeScreen {
    int grabIndex;

    Cursor leftCursor;
    Cursor rightCursor;
    Cursor upCursor;
    Cursor upLeftCursor;
    Cursor upRightCursor;
    Cursor downCursor;
    Cursor downLeftCursor;
    Cursor downRightCursor;
} ResizeScreen;

#define GET_RESIZE_DISPLAY(d)				       \
    ((ResizeDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define RESIZE_DISPLAY(d)		       \
    ResizeDisplay *rd = GET_RESIZE_DISPLAY (d)

#define GET_RESIZE_SCREEN(s, rd)				   \
    ((ResizeScreen *) (s)->privates[(rd)->screenPrivateIndex].ptr)

#define RESIZE_SCREEN(s)						      \
    ResizeScreen *rs = GET_RESIZE_SCREEN (s, GET_RESIZE_DISPLAY (s->display))

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static Bool
resizeInitiate (CompDisplay     *d,
		CompAction      *action,
		CompActionState state,
		CompOption      *option,
		int	        nOption)
{
    CompWindow *w;
    Window     xid;

    RESIZE_DISPLAY (d);

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findWindowAtDisplay (d, xid);
    if (w)
    {
	unsigned int mods;
	unsigned int mask;
	int          x, y;
	int	     button;

	RESIZE_SCREEN (w->screen);

	mods = getIntOptionNamed (option, nOption, "modifiers", 0);

	x = getIntOptionNamed (option, nOption, "x",
			       w->attrib.x + (w->width / 2));
	y = getIntOptionNamed (option, nOption, "y",
			       w->attrib.y + (w->height / 2));

	button = getIntOptionNamed (option, nOption, "button", -1);

	mask = getIntOptionNamed (option, nOption, "direction", 0);

	/* Initiate the resize in the direction suggested by the quarter
	 * of the window the mouse is in, eg drag in top left will resize
	 * up and to the left. */
	if (!mask)
	{
	    mask |= ((x - w->attrib.x) < (w->width / 2)) ?
		ResizeLeftMask : ResizeRightMask;

	    mask |= ((y - w->attrib.y) < (w->height / 2)) ?
		ResizeUpMask : ResizeDownMask;
	}

	if (otherScreenGrabExist (w->screen, "resize", 0))
	    return FALSE;

	if (rd->w)
	    return FALSE;

	if (w->type & (CompWindowTypeDesktopMask |
		       CompWindowTypeDockMask	 |
		       CompWindowTypeFullscreenMask))
	    return FALSE;

	if (w->attrib.override_redirect)
	    return FALSE;

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	if (w->shaded)
	    mask &= ~(ResizeUpMask | ResizeDownMask);

	rd->w	   = w;
	rd->mask   = mask;
	rd->width  = w->attrib.width;
	rd->height = w->attrib.height;

	lastPointerX = x;
	lastPointerY = y;

	if (!rs->grabIndex)
	{
	    Cursor cursor;

	    if (mask & ResizeLeftMask)
	    {
		if (mask & ResizeDownMask)
		    cursor = rs->downLeftCursor;
		else if (mask & ResizeUpMask)
		    cursor = rs->upLeftCursor;
		else
		    cursor = rs->leftCursor;
	    }
	    else if (mask & ResizeRightMask)
	    {
		if (mask & ResizeDownMask)
		    cursor = rs->downRightCursor;
		else if (mask & ResizeUpMask)
		    cursor = rs->upRightCursor;
		else
		    cursor = rs->rightCursor;
	    }
	    else if (mask & ResizeUpMask)
	    {
		cursor = rs->upCursor;
	    }
	    else
	    {
		cursor = rs->downCursor;
	    }

	    rs->grabIndex = pushScreenGrab (w->screen, cursor, "resize");
	}

	if (rs->grabIndex)
	{
	    rd->releaseButton = button;

	    (w->screen->windowGrabNotify) (w, x, y, state,
					   CompWindowGrabResizeMask |
					   CompWindowGrabButtonMask);

	    if (state & CompActionStateInitKey)
	    {
		int xRoot, yRoot;

		xRoot = w->attrib.x + (w->width  / 2);
		yRoot = w->attrib.y + (w->height / 2);

		warpPointer (d, xRoot - pointerX, yRoot - pointerY);
	    }
	}
    }

    return FALSE;
}

static Bool
resizeTerminate (CompDisplay	 *d,
		 CompAction	 *action,
		 CompActionState state,
		 CompOption	 *option,
		 int		 nOption)
{
    RESIZE_DISPLAY (d);

    if (rd->w)
    {
	RESIZE_SCREEN (rd->w->screen);

	(rd->w->screen->windowUngrabNotify) (rd->w);

	if (rs->grabIndex)
	{
	    removeScreenGrab (rd->w->screen, rs->grabIndex, NULL);
	    rs->grabIndex = 0;
	}

	syncWindowPosition (rd->w);

	rd->w		  = 0;
	rd->releaseButton = 0;
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

static void
resizeUpdateWindowSize (CompDisplay *d)
{
    int width, height;

    RESIZE_DISPLAY (d);

    if (!rd->w)
	return;

    if (rd->w->state & CompWindowStateMaximizedVertMask)
	rd->height = rd->w->attrib.height;

    if (rd->w->state & CompWindowStateMaximizedHorzMask)
	rd->width = rd->w->attrib.width;

    if (rd->width  == rd->w->attrib.width &&
	rd->height == rd->w->attrib.height)
	return;

    if (rd->w->syncWait)
	return;

    if (constrainNewWindowSize (rd->w,
				rd->width, rd->height,
				&width, &height))
    {
	XWindowChanges xwc;

	xwc.x = rd->w->attrib.x;
	if (rd->mask & ResizeLeftMask)
	    xwc.x -= width - rd->w->attrib.width;

	xwc.y = rd->w->attrib.y;
	if (rd->mask & ResizeUpMask)
	    xwc.y -= height - rd->w->attrib.height;

	xwc.width  = width;
	xwc.height = height;

	sendSyncRequest (rd->w);

	XConfigureWindow (d->display, rd->w->id,
			  CWX | CWY | CWWidth | CWHeight,
			  &xwc);
    }
}

static void
resizeConstrainMinMax (CompWindow *w,
		       int        width,
		       int        height,
		       int        *newWidth,
		       int        *newHeight)
{
    const XSizeHints *hints = &w->sizeHints;
    int		     min_width = 0;
    int		     min_height = 0;
    int		     max_width = MAXSHORT;
    int		     max_height = MAXSHORT;

    if ((hints->flags & PBaseSize) && (hints->flags & PMinSize))
    {
	min_width = hints->min_width;
	min_height = hints->min_height;
    }
    else if (hints->flags & PBaseSize)
    {
	min_width = hints->base_width;
	min_height = hints->base_height;
    }
    else if (hints->flags & PMinSize)
    {
	min_width = hints->min_width;
	min_height = hints->min_height;
    }

    if (hints->flags & PMaxSize)
    {
	max_width = hints->max_width;
	max_height = hints->max_height;
    }

#define CLAMP(v, min, max) ((v) <= (min) ? (min) : (v) >= (max) ? (max) : (v))

    /* clamp width and height to min and max values */
    width  = CLAMP (width, min_width, max_width);
    height = CLAMP (height, min_height, max_height);

#undef CLAMP

    *newWidth  = width;
    *newHeight = height;
}

static void
resizeHandleMotionEvent (CompScreen *s,
			 int	    xRoot,
			 int	    yRoot)
{
    RESIZE_SCREEN (s);

    if (rs->grabIndex)
    {
	int pointerDx, pointerDy;

	RESIZE_DISPLAY (s->display);

	pointerDx = xRoot - lastPointerX;
	pointerDy = yRoot - lastPointerY;

	if (pointerDx || pointerDy)
	{
	    int w, h;

	    w = rd->width;
	    h = rd->height;

	    if (rd->mask & ResizeLeftMask)
		w -= pointerDx;
	    else if (rd->mask & ResizeRightMask)
		w += pointerDx;

	    if (rd->mask & ResizeUpMask)
		h -= pointerDy;
	    else if (rd->mask & ResizeDownMask)
		h += pointerDy;

	    resizeConstrainMinMax (rd->w, w, h, &w, &h);

	    rd->width  = w;
	    rd->height = h;

	    resizeUpdateWindowSize (s->display);
	}
    }
}

static void
resizeHandleEvent (CompDisplay *d,
		   XEvent      *event)
{
    CompScreen *s;

    RESIZE_DISPLAY (d);

    switch (event->type) {
    case KeyPress:
    case KeyRelease:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    RESIZE_SCREEN (s);

	    if (rs->grabIndex && rd->w && event->type == KeyPress)
	    {
		int i, widthInc, heightInc;

		widthInc  = rd->w->sizeHints.width_inc;
		heightInc = rd->w->sizeHints.height_inc;

		if (widthInc < MIN_KEY_WIDTH_INC)
		    widthInc = MIN_KEY_WIDTH_INC;

		if (heightInc < MIN_KEY_HEIGHT_INC)
		    heightInc = MIN_KEY_HEIGHT_INC;

		for (i = 0; i < NUM_KEYS; i++)
		{
		    if (event->xkey.keycode == rd->key[i])
		    {
			XWarpPointer (d->display, None, None, 0, 0, 0, 0,
				      rKeys[i].dx * widthInc,
				      rKeys[i].dy * heightInc);
			break;
		    }
		}
	    }
	}
	break;
    case ButtonRelease: {
	CompAction *action =
	    &rd->opt[RESIZE_DISPLAY_OPTION_INITIATE].value.action;

	if (action->state & CompActionStateTermButton)
	{
	    if (event->type == ButtonRelease &&
		(rd->releaseButton     == -1 ||
		 event->xbutton.button == rd->releaseButton))
	    {
		resizeTerminate (d,
				 action,
				 CompActionStateTermButton,
				 NULL,
				 0);
	    }
	}
    } break;
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	    resizeHandleMotionEvent (s, pointerX, pointerY);
	break;
    case EnterNotify:
    case LeaveNotify:
	s = findScreenAtDisplay (d, event->xcrossing.root);
	if (s)
	    resizeHandleMotionEvent (s, pointerX, pointerY);
	break;
    case ClientMessage:
	if (event->xclient.message_type == d->wmMoveResizeAtom)
	{
	    CompWindow *w;

	    if (event->xclient.data.l[2] <= WmMoveResizeSizeLeft ||
		event->xclient.data.l[2] == WmMoveResizeSizeKeyboard)
	    {
		w = findWindowAtDisplay (d, event->xclient.window);
		if (w)
		{
		    CompOption o[6];
		    CompAction *action =
			&rd->opt[RESIZE_DISPLAY_OPTION_INITIATE].value.action;

		    o[0].type    = CompOptionTypeInt;
		    o[0].name    = "window";
		    o[0].value.i = event->xclient.window;

		    if (event->xclient.data.l[2] == WmMoveResizeSizeKeyboard)
		    {
			o[1].type    = CompOptionTypeInt;
			o[1].name    = "button";
			o[1].value.i = 0;

			resizeInitiate (d, action,
					CompActionStateInitKey,
					o, 2);
		    }
		    else
		    {
			static unsigned int mask[] = {
			    ResizeUpMask | ResizeLeftMask,
			    ResizeUpMask,
			    ResizeUpMask | ResizeRightMask,
			    ResizeRightMask,
			    ResizeDownMask | ResizeRightMask,
			    ResizeDownMask,
			    ResizeDownMask | ResizeLeftMask,
			    ResizeLeftMask,
			};
			unsigned int mods;
			Window	     root, child;
			int	     xRoot, yRoot, i;

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

			    o[4].type	 = CompOptionTypeInt;
			    o[4].name	 = "direction";
			    o[4].value.i = mask[event->xclient.data.l[2]];

			    o[5].type	 = CompOptionTypeInt;
			    o[5].name	 = "button";
			    o[5].value.i = event->xclient.data.l[3] ?
				event->xclient.data.l[3] : -1;

			    resizeInitiate (d,
					    action,
					    CompActionStateInitButton,
					    o, 6);

			    resizeHandleMotionEvent (w->screen, xRoot, yRoot);
			}
		    }
		}
	    }
	}
	break;
    case DestroyNotify:
	if (rd->w && rd->w->id == event->xdestroywindow.window)
	{
	    CompAction *action =
		&rd->opt[RESIZE_DISPLAY_OPTION_INITIATE].value.action;

	    resizeTerminate (d, action, 0, NULL, 0);
	}
	break;
    case UnmapNotify:
	if (rd->w && rd->w->id == event->xunmap.window)
	{
	    CompAction *action =
		&rd->opt[RESIZE_DISPLAY_OPTION_INITIATE].value.action;

	    resizeTerminate (d, action, 0, NULL, 0);
	}
	break;
    default:
	if (event->type == d->syncEvent + XSyncAlarmNotify)
	{
	    if (rd->w)
	    {
		XSyncAlarmNotifyEvent *sa;

		sa = (XSyncAlarmNotifyEvent *) event;

		if (rd->w->syncAlarm == sa->alarm)
		    resizeUpdateWindowSize (d);
	    }
	}
	break;
    }

    UNWRAP (rd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (rd, d, handleEvent, resizeHandleEvent);
}

static CompOption *
resizeGetDisplayOptions (CompDisplay *display,
			 int	     *count)
{
    RESIZE_DISPLAY (display);

    *count = NUM_OPTIONS (rd);
    return rd->opt;
}

static Bool
resizeSetDisplayOption (CompDisplay     *display,
			char	        *name,
			CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    RESIZE_DISPLAY (display);

    o = compFindOption (rd->opt, NUM_OPTIONS (rd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case RESIZE_DISPLAY_OPTION_INITIATE:
	if (setDisplayAction (display, o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
resizeDisplayInitOptions (ResizeDisplay *rd,
			  Display       *display)
{
    CompOption *o;

    o = &rd->opt[RESIZE_DISPLAY_OPTION_INITIATE];
    o->name			     = "initiate";
    o->shortDesc		     = N_("Initiate Window Resize");
    o->longDesc			     = N_("Start resizing window");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = resizeInitiate;
    o->value.action.terminate	     = resizeTerminate;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.state	     = CompActionStateInitButton;
    o->value.action.button.modifiers = RESIZE_INITIATE_BUTTON_MODIFIERS_DEFAULT;
    o->value.action.button.button    = RESIZE_INITIATE_BUTTON_DEFAULT;
    o->value.action.type	    |= CompBindingTypeKey;
    o->value.action.state	    |= CompActionStateInitKey;
    o->value.action.key.modifiers    = RESIZE_INITIATE_KEY_MODIFIERS_DEFAULT;
    o->value.action.key.keycode      =
	XKeysymToKeycode (display,
			  XStringToKeysym (RESIZE_INITIATE_KEY_DEFAULT));
}

static Bool
resizeInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    ResizeDisplay *rd;
    int	          i;

    rd = malloc (sizeof (ResizeDisplay));
    if (!rd)
	return FALSE;

    rd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (rd->screenPrivateIndex < 0)
    {
	free (rd);
	return FALSE;
    }

    resizeDisplayInitOptions (rd, d->display);

    rd->w = 0;

    rd->releaseButton = 0;

    for (i = 0; i < NUM_KEYS; i++)
	rd->key[i] = XKeysymToKeycode (d->display,
				       XStringToKeysym (rKeys[i].name));

    WRAP (rd, d, handleEvent, resizeHandleEvent);

    d->privates[displayPrivateIndex].ptr = rd;

    return TRUE;
}

static void
resizeFiniDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    RESIZE_DISPLAY (d);

    freeScreenPrivateIndex (d, rd->screenPrivateIndex);

    UNWRAP (rd, d, handleEvent);

    free (rd);
}

static Bool
resizeInitScreen (CompPlugin *p,
		  CompScreen *s)
{
    ResizeScreen *rs;

    RESIZE_DISPLAY (s->display);

    rs = malloc (sizeof (ResizeScreen));
    if (!rs)
	return FALSE;

    rs->grabIndex = 0;

    rs->leftCursor	= XCreateFontCursor (s->display->display, XC_left_side);
    rs->rightCursor	= XCreateFontCursor (s->display->display, XC_right_side);
    rs->upCursor	= XCreateFontCursor (s->display->display,
					     XC_top_side);
    rs->upLeftCursor	= XCreateFontCursor (s->display->display,
					     XC_top_left_corner);
    rs->upRightCursor	= XCreateFontCursor (s->display->display,
					     XC_top_right_corner);
    rs->downCursor	= XCreateFontCursor (s->display->display,
					     XC_bottom_side);
    rs->downLeftCursor	= XCreateFontCursor (s->display->display,
					     XC_bottom_left_corner);
    rs->downRightCursor = XCreateFontCursor (s->display->display,
					     XC_bottom_right_corner);

    addScreenAction (s, &rd->opt[RESIZE_DISPLAY_OPTION_INITIATE].value.action);

    s->privates[rd->screenPrivateIndex].ptr = rs;

    return TRUE;
}

static void
resizeFiniScreen (CompPlugin *p,
		  CompScreen *s)
{
    RESIZE_SCREEN (s);

    free (rs);
}

static Bool
resizeInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
resizeFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginVTable resizeVTable = {
    "resize",
    N_("Resize Window"),
    N_("Resize window"),
    resizeInit,
    resizeFini,
    resizeInitDisplay,
    resizeFiniDisplay,
    resizeInitScreen,
    resizeFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    resizeGetDisplayOptions,
    resizeSetDisplayOption,
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    NULL,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &resizeVTable;
}
