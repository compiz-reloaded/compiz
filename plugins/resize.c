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

static CompMetadata resizeMetadata;

#define ResizeUpMask    (1L << 0)
#define ResizeDownMask  (1L << 1)
#define ResizeLeftMask  (1L << 2)
#define ResizeRightMask (1L << 3)

struct _ResizeKeys {
    char	 *name;
    int		 dx;
    int		 dy;
    unsigned int warpMask;
    unsigned int resizeMask;
} rKeys[] = {
    { "Left",  -1,  0, ResizeLeftMask | ResizeRightMask, ResizeLeftMask },
    { "Right",  1,  0, ResizeLeftMask | ResizeRightMask, ResizeRightMask },
    { "Up",     0, -1, ResizeUpMask | ResizeDownMask,    ResizeUpMask },
    { "Down",   0,  1, ResizeUpMask | ResizeDownMask,    ResizeDownMask }
};

#define NUM_KEYS (sizeof (rKeys) / sizeof (rKeys[0]))

#define MIN_KEY_WIDTH_INC  24
#define MIN_KEY_HEIGHT_INC 24

#define RESIZE_DISPLAY_OPTION_INITIATE 0
#define RESIZE_DISPLAY_OPTION_MODE     1
#define RESIZE_DISPLAY_OPTION_NUM      2

static int displayPrivateIndex;

typedef struct _ResizeDisplay {
    CompOption opt[RESIZE_DISPLAY_OPTION_NUM];

    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompWindow	      *w;
    XWindowAttributes savedAttrib;
    int		      releaseButton;
    unsigned int      mask;
    int		      width;
    int		      height;
    int		      ucWidth;	/* unconstrained width */
    int		      ucHeight;	/* unconstrained height */
    KeyCode	      key[NUM_KEYS];
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
    Cursor middleCursor;
    Cursor cursor[NUM_KEYS];
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
    if (w && (w->actions & CompWindowActionResizeMask))
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

	/* Initiate the resize in the direction suggested by the
	 * quarter of the window the mouse is in, eg drag in top left
	 * will resize up and to the left.  Keyboard resize starts out
	 * with the cursor in the middle of the window and then starts
	 * resizing the edge corresponding to the next key press. */
	if (state & CompActionStateInitKey)
	{
	    mask = 0;
	}
	else if (!mask)
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

	rd->w	        = w;
	rd->mask        = mask;
	rd->ucWidth     = w->attrib.width;
	rd->ucHeight    = w->attrib.height;
	rd->width       = w->attrib.width;
	rd->height      = w->attrib.height;
	rd->savedAttrib = w->attrib;

	lastPointerX = x;
	lastPointerY = y;

	if (!rs->grabIndex)
	{
	    Cursor cursor;

	    if (state & CompActionStateInitKey)
	    {
		cursor = rs->middleCursor;
	    }
	    else if (mask & ResizeLeftMask)
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

		warpPointer (w->screen, xRoot - pointerX, yRoot - pointerY);
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

	if (state & CompActionStateCancel)
	{
	    XWindowChanges xwc;

	    sendSyncRequest (rd->w);

	    xwc.x      = rd->savedAttrib.x;
	    xwc.y      = rd->savedAttrib.y;
	    xwc.width  = rd->savedAttrib.width;
	    xwc.height = rd->savedAttrib.height;

	    configureXWindow (rd->w,
			      CWX | CWY | CWWidth | CWHeight,
			      &xwc);
	}
	else
	{
	    syncWindowPosition (rd->w);
	}

	if (rs->grabIndex)
	{
	    removeScreenGrab (rd->w->screen, rs->grabIndex, NULL);
	    rs->grabIndex = 0;
	}

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
	rd->height = rd->w->serverHeight;

    if (rd->w->state & CompWindowStateMaximizedHorzMask)
	rd->width = rd->w->serverWidth;

    if (rd->width  == rd->w->serverWidth &&
	rd->height == rd->w->serverHeight)
	return;

    if (rd->w->syncWait)
	return;

    if (constrainNewWindowSize (rd->w,
				rd->width, rd->height,
				&width, &height))
    {
	XWindowChanges xwc;

	xwc.x = rd->w->serverX;
	if (rd->mask & ResizeLeftMask)
	    xwc.x -= width - rd->w->serverWidth;

	xwc.y = rd->w->serverY;
	if (rd->mask & ResizeUpMask)
	    xwc.y -= height - rd->w->serverHeight;

	xwc.width  = width;
	xwc.height = height;

	sendSyncRequest (rd->w);

	configureXWindow (rd->w,
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
resizeHandleKeyEvent (CompScreen *s,
		      KeyCode	 keycode)
{
    RESIZE_SCREEN (s);
    RESIZE_DISPLAY (s->display);

    if (rs->grabIndex && rd->w)
    {
	CompWindow *w;
	int i, widthInc, heightInc;

	w = rd->w;

	widthInc  = w->sizeHints.width_inc;
	heightInc = w->sizeHints.height_inc;

	if (widthInc < MIN_KEY_WIDTH_INC)
	    widthInc = MIN_KEY_WIDTH_INC;

	if (heightInc < MIN_KEY_HEIGHT_INC)
	    heightInc = MIN_KEY_HEIGHT_INC;

	for (i = 0; i < NUM_KEYS; i++)
	{
	    if (keycode != rd->key[i])
	      continue;

	    if (rd->mask & rKeys[i].warpMask)
	    {
		XWarpPointer (s->display->display, None, None, 0, 0, 0, 0,
			      rKeys[i].dx * widthInc,
			      rKeys[i].dy * heightInc);
	    }
	    else
	    {
		int x, y, left, top, width, height;

		left   = w->attrib.x - w->input.left;
		top    = w->attrib.y - w->input.top;
		width  = w->input.left + w->attrib.width  + w->input.right;
		height = w->input.top  + w->attrib.height + w->input.bottom;

		x = left + width  * (rKeys[i].dx + 1) / 2;
		y = top  + height * (rKeys[i].dy + 1) / 2;

		warpPointer (s, x - pointerX, y - pointerY);
		rd->mask = rKeys[i].resizeMask;
		updateScreenGrab (s, rs->grabIndex, rs->cursor[i]);
	    }
	    break;
	}
    }
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

	    w = rd->ucWidth;
	    h = rd->ucHeight;

	    if (rd->mask & ResizeLeftMask)
		w -= pointerDx;
	    else if (rd->mask & ResizeRightMask)
		w += pointerDx;

	    if (rd->mask & ResizeUpMask)
		h -= pointerDy;
	    else if (rd->mask & ResizeDownMask)
		h += pointerDy;

	    rd->ucWidth  = w;
	    rd->ucHeight = h;

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
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	    resizeHandleKeyEvent (s, event->xkey.keycode);
	break;
    case KeyRelease:
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
    default:
	break;
    }

    UNWRAP (rd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (rd, d, handleEvent, resizeHandleEvent);

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
}

static CompOption *
resizeGetDisplayOptions (CompPlugin  *plugin,
			 CompDisplay *display,
			 int	     *count)
{
    RESIZE_DISPLAY (display);

    *count = NUM_OPTIONS (rd);
    return rd->opt;
}

static Bool
resizeSetDisplayOption (CompPlugin      *plugin,
			CompDisplay     *display,
			char	        *name,
			CompOptionValue *value)
{
    CompOption *o;

    RESIZE_DISPLAY (display);

    o = compFindOption (rd->opt, NUM_OPTIONS (rd), name, NULL);
    if (!o)
	return FALSE;

    return compSetDisplayOption (display, o, value);
}

static const CompMetadataOptionInfo resizeDisplayOptionInfo[] = {
    { "initiate", "action", 0, resizeInitiate, resizeTerminate },
    { "mode", "string", 0, 0, 0 }
};

static Bool
resizeInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    ResizeDisplay *rd;
    int	          i;

    rd = malloc (sizeof (ResizeDisplay));
    if (!rd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &resizeMetadata,
					     resizeDisplayOptionInfo,
					     rd->opt,
					     RESIZE_DISPLAY_OPTION_NUM))
    {
	free (rd);
	return FALSE;
    }

    rd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (rd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, rd->opt, RESIZE_DISPLAY_OPTION_NUM);
	free (rd);
	return FALSE;
    }

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
    rs->middleCursor    = XCreateFontCursor (s->display->display, XC_fleur);

    rs->cursor[0] = rs->leftCursor;
    rs->cursor[1] = rs->rightCursor;
    rs->cursor[2] = rs->upCursor;
    rs->cursor[3] = rs->downCursor;

    addScreenAction (s, &rd->opt[RESIZE_DISPLAY_OPTION_INITIATE].value.action);

    s->privates[rd->screenPrivateIndex].ptr = rs;

    return TRUE;
}

static void
resizeFiniScreen (CompPlugin *p,
		  CompScreen *s)
{
    RESIZE_SCREEN (s);

    if (rs->leftCursor)
	XFreeCursor (s->display->display, rs->leftCursor);
    if (rs->rightCursor)
	XFreeCursor (s->display->display, rs->rightCursor);
    if (rs->upCursor)
	XFreeCursor (s->display->display, rs->upCursor);
    if (rs->downCursor)
	XFreeCursor (s->display->display, rs->downCursor);
    if (rs->middleCursor)
	XFreeCursor (s->display->display, rs->middleCursor);
    if (rs->upLeftCursor)
	XFreeCursor (s->display->display, rs->upLeftCursor);
    if (rs->upRightCursor)
	XFreeCursor (s->display->display, rs->upRightCursor);
    if (rs->downLeftCursor)
	XFreeCursor (s->display->display, rs->downLeftCursor);
    if (rs->downRightCursor)
	XFreeCursor (s->display->display, rs->downRightCursor);

    free (rs);
}

static Bool
resizeInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&resizeMetadata,
					 p->vTable->name,
					 resizeDisplayOptionInfo,
					 RESIZE_DISPLAY_OPTION_NUM,
					 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&resizeMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&resizeMetadata, p->vTable->name);

    return TRUE;
}

static void
resizeFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);

    compFiniMetadata (&resizeMetadata);
}

static int
resizeGetVersion (CompPlugin *plugin,
		  int	     version)
{
    return ABIVERSION;
}

static CompMetadata *
resizeGetMetadata (CompPlugin *plugin)
{
    return &resizeMetadata;
}

CompPluginVTable resizeVTable = {
    "resize",
    N_("Resize Window"),
    N_("Resize window"),
    resizeGetVersion,
    resizeGetMetadata,
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
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &resizeVTable;
}
