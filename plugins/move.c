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
#define MOVE_INITIATE_MODIFIERS_DEFAULT (CompPressMask | CompAltMask)

#define MOVE_TERMINATE_BUTTON_DEFAULT    Button1
#define MOVE_TERMINATE_MODIFIERS_DEFAULT CompReleaseMask

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

static int displayPrivateIndex;

typedef struct _MoveDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompWindow *w;
    KeyCode    key[NUM_KEYS];
} MoveDisplay;

#define MOVE_SCREEN_OPTION_INITIATE  0
#define MOVE_SCREEN_OPTION_TERMINATE 1
#define MOVE_SCREEN_OPTION_NUM	     2

typedef struct _MoveScreen {
    CompOption opt[MOVE_SCREEN_OPTION_NUM];

    int grabIndex;

    Cursor moveCursor;

    int prevPointerX;
    int prevPointerY;
} MoveScreen;

#define GET_MOVE_DISPLAY(d)				      \
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
    case MOVE_SCREEN_OPTION_TERMINATE:
	if (compSetBindingOption (o, value))
	    return TRUE;
	break;
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

    o = &ms->opt[MOVE_SCREEN_OPTION_TERMINATE];
    o->name			     = "terminate";
    o->shortDesc		     = "Terminate Window Move";
    o->longDesc			     = "Stop moving window";
    o->type			     = CompOptionTypeBinding;
    o->value.bind.type		     = CompBindingTypeButton;
    o->value.bind.u.button.modifiers = MOVE_TERMINATE_MODIFIERS_DEFAULT;
    o->value.bind.u.button.button    = MOVE_TERMINATE_BUTTON_DEFAULT;
}

static void
moveInitiate (CompWindow   *w,
	      int	   x,
	      int	   y,
	      unsigned int state)
{
    MOVE_DISPLAY (w->screen->display);
    MOVE_SCREEN (w->screen);

    /* some plugin has already grabbed the screen */
    if (w->screen->maxGrab)
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

    ms->prevPointerX = x;
    ms->prevPointerY = y;

    if (!ms->grabIndex)
	ms->grabIndex = pushScreenGrab (w->screen, ms->moveCursor);

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
	int	   pointerDx, pointerDy;

	MOVE_DISPLAY (s->display);

	w = md->w;

	pointerDx = xRoot - ms->prevPointerX;
	pointerDy = yRoot - ms->prevPointerY;
	ms->prevPointerX = xRoot;
	ms->prevPointerY = yRoot;

	if (w->type & CompWindowTypeFullscreenMask)
	{
	    pointerDx = pointerDy = 0;
	}
	else
	{
	    int min, max;

	    if (w->state & CompWindowStateMaximizedVertMask)
	    {
		min = s->workArea.y + w->input.top;
		max = s->workArea.y + s->workArea.height -
		    w->input.bottom - w->height;

		if (w->attrib.y + pointerDy < min)
		    pointerDy = min - w->attrib.y;
		else if (w->attrib.y + pointerDy > max)
		    pointerDy = max - w->attrib.y;
	    }

	    if (w->state & CompWindowStateMaximizedHorzMask)
	    {
		min = s->workArea.x + w->input.left;
		max = s->workArea.x + s->workArea.width -
		    w->input.right - w->width;

		if (w->attrib.x + pointerDx < min)
		    pointerDx = min - w->attrib.x;
		else if (w->attrib.x + pointerDx > max)
		    pointerDx = max - w->attrib.x;
	    }
	}

	if (pointerDx || pointerDy)
	    moveWindow (md->w, pointerDx, pointerDy, TRUE);
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

	    if (EV_KEY (&ms->opt[MOVE_SCREEN_OPTION_INITIATE], event))
	    {
		CompWindow *w;

		w = findWindowAtScreen (s, event->xkey.window);
		if (w)
		    moveInitiate (w,
				  event->xkey.x_root,
				  event->xkey.y_root,
				  event->xkey.state);
	    }

	    if (EV_KEY (&ms->opt[MOVE_SCREEN_OPTION_TERMINATE], event) ||
		(event->type	     == KeyPress &&
		 event->xkey.keycode == s->escapeKeyCode))
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

	    if (EV_BUTTON (&ms->opt[MOVE_SCREEN_OPTION_INITIATE], event))
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		    moveInitiate (w,
				  event->xbutton.x_root,
				  event->xbutton.y_root,
				  event->xbutton.state);
	    }

	    if (EV_BUTTON (&ms->opt[MOVE_SCREEN_OPTION_TERMINATE], event))
		moveTerminate (d);
	}
	break;
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	    moveHandleMotionEvent (s,
				   event->xmotion.x_root,
				   event->xmotion.y_root);
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

		    if (event->xclient.data.l[2] == WmMoveResizeMoveKeyboard)
		    {
			xRoot = w->attrib.x + w->width / 2;
			yRoot = w->attrib.y + w->height / 2;

			XWarpPointer (d->display, None, w->screen->root,
				      0, 0, 0, 0, xRoot, yRoot);

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

			/* TODO: not only button 1*/
			if (state & Button1Mask)
			{
			    moveInitiate (w,
					  event->xclient.data.l[0],
					  event->xclient.data.l[1],
					  state | CompPressMask);

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

    ms->prevPointerX = 0;
    ms->prevPointerY = 0;

    moveScreenInitOptions (ms, s->display->display);

    ms->moveCursor = XCreateFontCursor (s->display->display, XC_plus);

    addScreenBinding (s, &ms->opt[MOVE_SCREEN_OPTION_INITIATE].value.bind);

    s->privates[md->screenPrivateIndex].ptr = ms;

    return TRUE;
}

static void
moveFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    MOVE_SCREEN (s);

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
