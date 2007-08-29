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
#include <math.h>
#include <sys/types.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>

#include <compiz-core.h>

#define ZOOMED_WINDOW_MASK (1 << 0)
#define NORMAL_WINDOW_MASK (1 << 1)

static CompMetadata switchMetadata;

static int displayPrivateIndex;

#define SWITCH_DISPLAY_OPTION_NEXT_BUTTON          0
#define SWITCH_DISPLAY_OPTION_NEXT_KEY	           1
#define SWITCH_DISPLAY_OPTION_PREV_BUTTON	   2
#define SWITCH_DISPLAY_OPTION_PREV_KEY	           3
#define SWITCH_DISPLAY_OPTION_NEXT_ALL_BUTTON	   4
#define SWITCH_DISPLAY_OPTION_NEXT_ALL_KEY	   5
#define SWITCH_DISPLAY_OPTION_PREV_ALL_BUTTON	   6
#define SWITCH_DISPLAY_OPTION_PREV_ALL_KEY	   7
#define SWITCH_DISPLAY_OPTION_NEXT_NO_POPUP_BUTTON 8
#define SWITCH_DISPLAY_OPTION_NEXT_NO_POPUP_KEY    9
#define SWITCH_DISPLAY_OPTION_PREV_NO_POPUP_BUTTON 10
#define SWITCH_DISPLAY_OPTION_PREV_NO_POPUP_KEY    11
#define SWITCH_DISPLAY_OPTION_NUM	           12

typedef struct _SwitchDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[SWITCH_DISPLAY_OPTION_NUM];

    Atom selectWinAtom;
} SwitchDisplay;

#define SWITCH_SCREEN_OPTION_SPEED	  0
#define SWITCH_SCREEN_OPTION_TIMESTEP	  1
#define SWITCH_SCREEN_OPTION_WINDOW_MATCH 2
#define SWITCH_SCREEN_OPTION_MIPMAP	  3
#define SWITCH_SCREEN_OPTION_SATURATION	  4
#define SWITCH_SCREEN_OPTION_BRIGHTNESS	  5
#define SWITCH_SCREEN_OPTION_OPACITY	  6
#define SWITCH_SCREEN_OPTION_BRINGTOFRONT 7
#define SWITCH_SCREEN_OPTION_ZOOM	  8
#define SWITCH_SCREEN_OPTION_ICON	  9
#define SWITCH_SCREEN_OPTION_MINIMIZED	  10
#define SWITCH_SCREEN_OPTION_AUTO_ROTATE  11
#define SWITCH_SCREEN_OPTION_NUM	  12

typedef struct _SwitchScreen {
    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintOutputProc	   paintOutput;
    PaintWindowProc        paintWindow;
    PaintBackgroundProc    paintBackground;
    DamageWindowRectProc   damageWindowRect;

    CompOption opt[SWITCH_SCREEN_OPTION_NUM];

    Window popupWindow;

    Window	 selectedWindow;
    Window	 zoomedWindow;
    unsigned int lastActiveNum;

    float zoom;

    int grabIndex;

    Bool switching;
    Bool zooming;
    int  zoomMask;

    int moreAdjust;

    GLfloat mVelocity;
    GLfloat tVelocity;
    GLfloat sVelocity;

    CompWindow **windows;
    int        windowsSize;
    int        nWindows;

    int pos;
    int move;

    float translate;
    float sTranslate;

    GLushort saturation;
    GLushort brightness;
    GLushort opacity;

    Bool allWindows;
} SwitchScreen;

#define MwmHintsDecorations (1L << 1)

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
} MwmHints;

#define SELECT_WIN_PROP "_SWITCH_SELECT_WINDOW"

#define WIDTH  212
#define HEIGHT 192
#define SPACE  10

#define SWITCH_ZOOM 0.1f

#define BOX_WIDTH 3

#define ICON_SIZE 64

static float _boxVertices[] =
{
    -(WIDTH >> 1), 0,
    -(WIDTH >> 1), BOX_WIDTH,
     (WIDTH >> 1), BOX_WIDTH,
     (WIDTH >> 1), 0,

    -(WIDTH >> 1),	       BOX_WIDTH,
    -(WIDTH >> 1),	       HEIGHT - BOX_WIDTH,
    -(WIDTH >> 1) + BOX_WIDTH, HEIGHT - BOX_WIDTH,
    -(WIDTH >> 1) + BOX_WIDTH, 0,

     (WIDTH >> 1) - BOX_WIDTH, BOX_WIDTH,
     (WIDTH >> 1) - BOX_WIDTH, HEIGHT - BOX_WIDTH,
     (WIDTH >> 1),	       HEIGHT - BOX_WIDTH,
     (WIDTH >> 1),	       0,

    -(WIDTH >> 1), HEIGHT - BOX_WIDTH,
    -(WIDTH >> 1), HEIGHT,
     (WIDTH >> 1), HEIGHT,
     (WIDTH >> 1), HEIGHT - BOX_WIDTH
};

#define WINDOW_WIDTH(count) (WIDTH * (count) + (SPACE << 1))
#define WINDOW_HEIGHT (HEIGHT + (SPACE << 1))

#define GET_SWITCH_DISPLAY(d)				       \
    ((SwitchDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define SWITCH_DISPLAY(d)		       \
    SwitchDisplay *sd = GET_SWITCH_DISPLAY (d)

#define GET_SWITCH_SCREEN(s, sd)				   \
    ((SwitchScreen *) (s)->privates[(sd)->screenPrivateIndex].ptr)

#define SWITCH_SCREEN(s)						      \
    SwitchScreen *ss = GET_SWITCH_SCREEN (s, GET_SWITCH_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
switchGetScreenOptions (CompPlugin *plugin,
			CompScreen *screen,
			int	   *count)
{
    SWITCH_SCREEN (screen);

    *count = NUM_OPTIONS (ss);
    return ss->opt;
}

static Bool
switchSetScreenOption (CompPlugin      *plugin,
		       CompScreen      *screen,
		       const char      *name,
		       CompOptionValue *value)
{
    CompOption  *o;
    int	        index;

    SWITCH_SCREEN (screen);

    o = compFindOption (ss->opt, NUM_OPTIONS (ss), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case SWITCH_SCREEN_OPTION_SATURATION:
	if (compSetIntOption (o, value))
	{
	    ss->saturation = (COLOR * o->value.i) / 100;
	    return TRUE;
	}
	break;
    case SWITCH_SCREEN_OPTION_BRIGHTNESS:
	if (compSetIntOption (o, value))
	{
	    ss->brightness = (0xffff * o->value.i) / 100;
	    return TRUE;
	}
	break;
    case SWITCH_SCREEN_OPTION_OPACITY:
	if (compSetIntOption (o, value))
	{
	    ss->opacity = (OPAQUE * o->value.i) / 100;
	    return TRUE;
	}
	break;
    case SWITCH_SCREEN_OPTION_ZOOM:
	if (compSetFloatOption (o, value))
	{
	    if (o->value.f < 0.05f)
	    {
		ss->zooming = FALSE;
		ss->zoom    = 0.0f;
	    }
	    else
	    {
		ss->zooming = TRUE;
		ss->zoom    = o->value.f / 30.0f;
	    }

	    return TRUE;
	}
	break;
    default:
	return compSetScreenOption (screen, o, value);
    }

    return FALSE;
}

static void
setSelectedWindowHint (CompScreen *s)
{
    SWITCH_DISPLAY (s->display);
    SWITCH_SCREEN (s);

    XChangeProperty (s->display->display, ss->popupWindow, sd->selectWinAtom,
		     XA_WINDOW, 32, PropModeReplace,
		     (unsigned char *) &ss->selectedWindow, 1);
}

static Bool
isSwitchWin (CompWindow *w)
{
    SWITCH_SCREEN (w->screen);

    if (!w->mapNum || w->attrib.map_state != IsViewable)
    {
	if (ss->opt[SWITCH_SCREEN_OPTION_MINIMIZED].value.b)
	{
	    if (!w->minimized && !w->inShowDesktopMode && !w->shaded)
		return FALSE;
	}
	else
	{
	    return FALSE;
	}
    }

    if (w->attrib.override_redirect)
	return FALSE;

    if (w->wmType & (CompWindowTypeDockMask | CompWindowTypeDesktopMask))
	return FALSE;

    if (w->state & CompWindowStateSkipTaskbarMask)
	return FALSE;

    if (!ss->allWindows)
    {
	if (!w->mapNum || w->attrib.map_state != IsViewable)
	{
	    if (w->serverX + w->width  <= 0    ||
		w->serverY + w->height <= 0    ||
		w->serverX >= w->screen->width ||
		w->serverY >= w->screen->height)
		return FALSE;
	}
	else
	{
	    if (!(*w->screen->focusWindow) (w))
		return FALSE;
	}
    }

    if (!matchEval (&ss->opt[SWITCH_SCREEN_OPTION_WINDOW_MATCH].value.match, w))
	return FALSE;

    return TRUE;
}

static void
switchActivateEvent (CompScreen *s,
		     Bool	activating)
{
    CompOption o[2];

    o[0].type = CompOptionTypeInt;
    o[0].name = "root";
    o[0].value.i = s->root;

    o[1].type = CompOptionTypeBool;
    o[1].name = "active";
    o[1].value.b = activating;

    (*s->display->handleCompizEvent) (s->display, "switcher", "activate", o, 2);
}

static int
compareWindows (const void *elem1,
		const void *elem2)
{
    CompWindow *w1 = *((CompWindow **) elem1);
    CompWindow *w2 = *((CompWindow **) elem2);

    if (w1->mapNum && !w2->mapNum)
	return -1;

    if (w2->mapNum && !w1->mapNum)
	return 1;

    return w2->activeNum - w1->activeNum;
}

static void
switchAddWindowToList (CompScreen *s,
		       CompWindow *w)
{
    SWITCH_SCREEN (s);

    if (ss->windowsSize <= ss->nWindows)
    {
	ss->windows = realloc (ss->windows,
			       sizeof (CompWindow *) * (ss->nWindows + 32));
	if (!ss->windows)
	    return;

	ss->windowsSize = ss->nWindows + 32;
    }

    ss->windows[ss->nWindows++] = w;
}

static void
switchUpdateWindowList (CompScreen *s,
			int	   count)
{
    int x, y;

    SWITCH_SCREEN (s);

    if (count > 1)
    {
	count -= (count + 1) & 1;
	if (count < 3)
	    count = 3;
    }

    ss->pos  = ((count >> 1) - ss->nWindows) * WIDTH;
    ss->move = 0;

    ss->selectedWindow = ss->windows[0]->id;

    x = s->outputDev[s->currentOutputDev].region.extents.x1 +
	s->outputDev[s->currentOutputDev].width / 2;
    y = s->outputDev[s->currentOutputDev].region.extents.y1 +
	s->outputDev[s->currentOutputDev].height / 2;

    if (ss->popupWindow)
	XMoveResizeWindow (s->display->display, ss->popupWindow,
			   x - WINDOW_WIDTH (count) / 2,
			   y - WINDOW_HEIGHT / 2,
			   WINDOW_WIDTH (count),
			   WINDOW_HEIGHT);
}

static void
switchCreateWindowList (CompScreen *s,
			int	   count)
{
    CompWindow *w;

    SWITCH_SCREEN (s);

    ss->nWindows = 0;

    for (w = s->windows; w; w = w->next)
    {
	if (isSwitchWin (w))
	    switchAddWindowToList (s, w);
    }

    qsort (ss->windows, ss->nWindows, sizeof (CompWindow *), compareWindows);

    if (ss->nWindows == 2)
    {
	switchAddWindowToList (s, ss->windows[0]);
	switchAddWindowToList (s, ss->windows[1]);
    }

    switchUpdateWindowList (s, count);
}

static void
switchToWindow (CompScreen *s,
		Bool	   toNext)
{
    CompWindow *w;
    int	       cur;

    SWITCH_SCREEN (s);

    if (!ss->grabIndex)
	return;

    for (cur = 0; cur < ss->nWindows; cur++)
    {
	if (ss->windows[cur]->id == ss->selectedWindow)
	    break;
    }

    if (cur == ss->nWindows)
	return;

    if (toNext)
	w = ss->windows[(cur + 1) % ss->nWindows];
    else
	w = ss->windows[(cur + ss->nWindows - 1) % ss->nWindows];

    if (w)
    {
	Window old = ss->selectedWindow;

	if (ss->allWindows && ss->opt[SWITCH_SCREEN_OPTION_AUTO_ROTATE].value.b)
	{
	    XEvent xev;
	    int	   x, y;

	    defaultViewportForWindow (w, &x, &y);

	    xev.xclient.type = ClientMessage;
	    xev.xclient.display = s->display->display;
	    xev.xclient.format = 32;

	    xev.xclient.message_type = s->display->desktopViewportAtom;
	    xev.xclient.window = s->root;

	    xev.xclient.data.l[0] = x * s->width;
	    xev.xclient.data.l[1] = y * s->height;
	    xev.xclient.data.l[2] = 0;
	    xev.xclient.data.l[3] = 0;
	    xev.xclient.data.l[4] = 0;

	    XSendEvent (s->display->display, s->root, FALSE,
			SubstructureRedirectMask | SubstructureNotifyMask,
			&xev);
	}

	ss->lastActiveNum  = w->activeNum;
	ss->selectedWindow = w->id;

	if (!ss->zoomedWindow)
	    ss->zoomedWindow = ss->selectedWindow;

	if (old != w->id)
	{
	    if (toNext)
		ss->move -= WIDTH;
	    else
		ss->move += WIDTH;

	    ss->moreAdjust = 1;
	}

	if (ss->popupWindow)
	{
	    CompWindow *popup;

	    popup = findWindowAtScreen (s, ss->popupWindow);
	    if (popup)
		addWindowDamage (popup);

	    setSelectedWindowHint (s);
	}

	addWindowDamage (w);

	if (old)
	{
	    w = findWindowAtScreen (s, old);
	    if (w)
		addWindowDamage (w);
	}
    }
}

static int
switchCountWindows (CompScreen *s)
{
    CompWindow *w;
    int	       count = 0;

    for (w = s->windows; w && count < 5; w = w->next)
	if (isSwitchWin (w))
	    count++;

    if (count == 5 && s->width <= WINDOW_WIDTH (5))
	count = 3;

    return count;
}

static Visual *
findArgbVisual (Display *dpy, int scr)
{
    XVisualInfo		*xvi;
    XVisualInfo		template;
    int			nvi;
    int			i;
    XRenderPictFormat	*format;
    Visual		*visual;

    template.screen = scr;
    template.depth  = 32;
    template.class  = TrueColor;

    xvi = XGetVisualInfo (dpy,
			  VisualScreenMask |
			  VisualDepthMask  |
			  VisualClassMask,
			  &template,
			  &nvi);
    if (!xvi)
	return 0;

    visual = 0;
    for (i = 0; i < nvi; i++)
    {
	format = XRenderFindVisualFormat (dpy, xvi[i].visual);
	if (format->type == PictTypeDirect && format->direct.alphaMask)
	{
	    visual = xvi[i].visual;
	    break;
	}
    }

    XFree (xvi);

    return visual;
}

static void
switchInitiate (CompScreen *s,
		Bool	   allWindows,
		Bool	   showPopup)
{
    int count;

    SWITCH_SCREEN (s);

    if (otherScreenGrabExist (s, "switcher", "scale", "cube", 0))
	return;

    ss->allWindows = allWindows;

    count = switchCountWindows (s);
    if (count < 1)
	return;

    if (!ss->popupWindow && showPopup)
    {
	Display		     *dpy = s->display->display;
	XSizeHints	     xsh;
	XWMHints	     xwmh;
	Atom		     state[4];
	int		     nState = 0;
	XSetWindowAttributes attr;
	Visual		     *visual;

	visual = findArgbVisual (dpy, s->screenNum);
	if (!visual)
	    return;

	if (count > 1)
	{
	    count -= (count + 1) & 1;
	    if (count < 3)
		count = 3;
	}

	xsh.flags  = PSize | PPosition;
	xsh.width  = WINDOW_WIDTH (count);
	xsh.height = WINDOW_HEIGHT;

	xwmh.flags = InputHint;
	xwmh.input = 0;

	attr.background_pixel = 0;
	attr.border_pixel     = 0;
	attr.colormap	      = XCreateColormap (dpy, s->root, visual,
						 AllocNone);

	ss->popupWindow =
	    XCreateWindow (dpy, s->root,
			   s->width  / 2 - xsh.width / 2,
			   s->height / 2 - xsh.height / 2,
			   xsh.width, xsh.height, 0,
			   32, InputOutput, visual,
			   CWBackPixel | CWBorderPixel | CWColormap, &attr);

	XSetWMProperties (dpy, ss->popupWindow, NULL, NULL,
			  programArgv, programArgc,
			  &xsh, &xwmh, NULL);

	state[nState++] = s->display->winStateAboveAtom;
	state[nState++] = s->display->winStateStickyAtom;
	state[nState++] = s->display->winStateSkipTaskbarAtom;
	state[nState++] = s->display->winStateSkipPagerAtom;

	XChangeProperty (dpy, ss->popupWindow,
			 s->display->winStateAtom,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) state, nState);

	XChangeProperty (dpy, ss->popupWindow,
			 s->display->winTypeAtom,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) &s->display->winTypeUtilAtom, 1);

	setWindowProp (s->display, ss->popupWindow,
		       s->display->winDesktopAtom,
		       0xffffffff);
    }

    if (!ss->grabIndex)
	ss->grabIndex = pushScreenGrab (s, s->invisibleCursor, "switcher");

    if (ss->grabIndex)
    {
	if (!ss->switching)
	{
	    ss->lastActiveNum = s->activeNum;

	    switchCreateWindowList (s, count);

	    ss->sTranslate = ss->zoom;

	    if (ss->popupWindow && showPopup)
	    {
		CompWindow *w;

		w = findWindowAtScreen (s, ss->popupWindow);
		if (w && (w->state & CompWindowStateHiddenMask))
		{
		    w->hidden = FALSE;
		    showWindow (w);
		}
		else
		{
		    XMapWindow (s->display->display, ss->popupWindow);
		}
	    }

	    setSelectedWindowHint (s);

	    switchActivateEvent (s, TRUE);
	}

	damageScreen (s);

	ss->switching  = TRUE;
	ss->moreAdjust = 1;
    }
}

static Bool
switchTerminate (CompDisplay     *d,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int	         nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	SWITCH_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (ss->grabIndex)
	{
	    CompWindow *w;

	    if (ss->popupWindow)
	    {
		w = findWindowAtScreen (s, ss->popupWindow);
		if (w && w->managed && w->mapNum)
		{
		    w->hidden = TRUE;
		    hideWindow (w);
		}
		else
		{
		    XUnmapWindow (s->display->display, ss->popupWindow);
		}
	    }

	    ss->switching = FALSE;

	    if (state && ss->selectedWindow)
	    {
		w = findWindowAtScreen (s, ss->selectedWindow);
		if (w)
		    sendWindowActivationRequest (w->screen, w->id);
	    }

	    removeScreenGrab (s, ss->grabIndex, 0);
	    ss->grabIndex = 0;

	    if (!ss->zooming)
	    {
		ss->selectedWindow = None;
		ss->zoomedWindow   = None;

		switchActivateEvent (s, FALSE);
	    }
	    else
	    {
		ss->moreAdjust = 1;
	    }

	    ss->lastActiveNum = 0;

	    damageScreen (s);
	}
    }

    if (action)
	action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

static Bool
switchNext (CompDisplay     *d,
	    CompAction      *action,
	    CompActionState state,
	    CompOption      *option,
	    int	            nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SWITCH_SCREEN (s);

	if (!ss->switching)
	{
	    switchInitiate (s, FALSE, TRUE);

	    if (state & CompActionStateInitKey)
		action->state |= CompActionStateTermKey;

	    if (state & CompActionStateInitButton)
		action->state |= CompActionStateTermButton;

	    if (state & CompActionStateInitEdge)
		action->state |= CompActionStateTermEdge;

	}

	switchToWindow (s, TRUE);
    }

    return FALSE;
}

static Bool
switchPrev (CompDisplay     *d,
	    CompAction      *action,
	    CompActionState state,
	    CompOption      *option,
	    int	            nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SWITCH_SCREEN (s);

	if (!ss->switching)
	{
	    switchInitiate (s, FALSE, TRUE);

	    if (state & CompActionStateInitKey)
		action->state |= CompActionStateTermKey;

	    if (state & CompActionStateInitButton)
		action->state |= CompActionStateTermButton;

	    if (state & CompActionStateInitEdge)
		action->state |= CompActionStateTermEdge;
	}

	switchToWindow (s, FALSE);
    }

    return FALSE;
}

static Bool
switchNextAll (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SWITCH_SCREEN (s);

	if (!ss->switching)
	{
	    switchInitiate (s, TRUE, TRUE);

	    if (state & CompActionStateInitKey)
		action->state |= CompActionStateTermKey;

	    if (state & CompActionStateInitButton)
		action->state |= CompActionStateTermButton;

	    if (state & CompActionStateInitEdge)
		action->state |= CompActionStateTermEdge;
	}

	switchToWindow (s, TRUE);
    }

    return FALSE;
}

static Bool
switchPrevAll (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SWITCH_SCREEN (s);

	if (!ss->switching)
	{
	    switchInitiate (s, TRUE, TRUE);

	    if (state & CompActionStateInitKey)
		action->state |= CompActionStateTermKey;

	    if (state & CompActionStateInitButton)
		action->state |= CompActionStateTermButton;

	    if (state & CompActionStateInitEdge)
		action->state |= CompActionStateTermEdge;
	}

	switchToWindow (s, FALSE);
    }

    return FALSE;
}

static Bool
switchNextNoPopup (CompDisplay     *d,
		   CompAction      *action,
		   CompActionState state,
		   CompOption      *option,
		   int	           nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SWITCH_SCREEN (s);

	if (!ss->switching)
	{
	    switchInitiate (s, FALSE, FALSE);

	    if (state & CompActionStateInitKey)
		action->state |= CompActionStateTermKey;

	    if (state & CompActionStateInitButton)
		action->state |= CompActionStateTermButton;

	    if (state & CompActionStateInitEdge)
		action->state |= CompActionStateTermEdge;

	}

	switchToWindow (s, TRUE);
    }

    return FALSE;
}

static Bool
switchPrevNoPopup (CompDisplay     *d,
		   CompAction      *action,
		   CompActionState state,
		   CompOption      *option,
		   int	           nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SWITCH_SCREEN (s);

	if (!ss->switching)
	{
	    switchInitiate (s, FALSE, FALSE);

	    if (state & CompActionStateInitKey)
		action->state |= CompActionStateTermKey;

	    if (state & CompActionStateInitButton)
		action->state |= CompActionStateTermButton;

	    if (state & CompActionStateInitEdge)
		action->state |= CompActionStateTermEdge;
	}

	switchToWindow (s, FALSE);
    }

    return FALSE;
}

static void
switchWindowRemove (CompDisplay *d,
		    Window	id)
{
    CompWindow *w;

    w = findWindowAtDisplay (d, id);
    if (w)
    {
	Bool   inList = FALSE;
	int    count, j, i = 0;
	Window selected, old;

	SWITCH_SCREEN (w->screen);

	if (isSwitchWin (w))
	    return;

	old = selected = ss->selectedWindow;

	while (i < ss->nWindows)
	{
	    if (ss->windows[i] == w)
	    {
		inList = TRUE;

		if (w->id == selected)
		{
		    if (i < ss->nWindows)
			selected = ss->windows[i + 1]->id;
		    else
			selected = ss->windows[0]->id;
		}

		ss->nWindows--;
		for (j = i; j < ss->nWindows; j++)
		    ss->windows[j] = ss->windows[j + 1];
	    }
	    else
	    {
		i++;
	    }
	}

	if (!inList)
	    return;

	count = ss->nWindows;

	if (ss->nWindows == 2)
	{
	    if (ss->windows[0] == ss->windows[1])
	    {
		ss->nWindows--;
		count = 1;
	    }
	    else
	    {
		switchAddWindowToList (w->screen, ss->windows[0]);
		switchAddWindowToList (w->screen, ss->windows[1]);
	    }
	}

	if (ss->nWindows == 0)
	{
	    CompOption o;

	    o.type    = CompOptionTypeInt;
	    o.name    = "root";
	    o.value.i = w->screen->root;

	    switchTerminate (d, NULL, 0, &o, 1);
	    return;
	}

	if (!ss->grabIndex)
	    return;

	switchUpdateWindowList (w->screen, count);

	for (i = 0; i < ss->nWindows; i++)
	{
	    ss->selectedWindow = ss->windows[i]->id;

	    if (ss->selectedWindow == selected)
		break;

	    ss->pos -= WIDTH;
	    if (ss->pos < -ss->nWindows * WIDTH)
		ss->pos += ss->nWindows * WIDTH;
	}

	if (ss->popupWindow)
	{
	    CompWindow *popup;

	    popup = findWindowAtScreen (w->screen, ss->popupWindow);
	    if (popup)
		addWindowDamage (popup);

	    setSelectedWindowHint (w->screen);
	}

	if (old != ss->selectedWindow)
	{
	    addWindowDamage (w);

	    w = findWindowAtScreen (w->screen, old);
	    if (w)
		addWindowDamage (w);

	    ss->moreAdjust = 1;
	}
    }
}

static void
switchHandleEvent (CompDisplay *d,
		   XEvent      *event)
{
    SWITCH_DISPLAY (d);

    UNWRAP (sd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (sd, d, handleEvent, switchHandleEvent);

    switch (event->type) {
    case UnmapNotify:
	switchWindowRemove (d, event->xunmap.window);
	break;
    case DestroyNotify:
	switchWindowRemove (d, event->xdestroywindow.window);
    default:
	break;
    }
}

static int
adjustSwitchVelocity (CompScreen *s)
{
    float dx, adjust, amount;

    SWITCH_SCREEN (s);

    dx = ss->move;

    adjust = dx * 0.15f;
    amount = fabs (dx) * 1.5f;
    if (amount < 0.2f)
	amount = 0.2f;
    else if (amount > 2.0f)
	amount = 2.0f;

    ss->mVelocity = (amount * ss->mVelocity + adjust) / (amount + 1.0f);

    if (ss->zooming)
    {
	float dt, ds;

	if (ss->switching)
	    dt = ss->zoom - ss->translate;
	else
	    dt = 0.0f - ss->translate;

	adjust = dt * 0.15f;
	amount = fabs (dt) * 1.5f;
	if (amount < 0.2f)
	    amount = 0.2f;
	else if (amount > 2.0f)
	    amount = 2.0f;

	ss->tVelocity = (amount * ss->tVelocity + adjust) / (amount + 1.0f);

	if (ss->selectedWindow == ss->zoomedWindow)
	    ds = ss->zoom - ss->sTranslate;
	else
	    ds = 0.0f - ss->sTranslate;

	adjust = ds * 0.5f;
	amount = fabs (ds) * 5.0f;
	if (amount < 1.0f)
	    amount = 1.0f;
	else if (amount > 6.0f)
	    amount = 6.0f;

	ss->sVelocity = (amount * ss->sVelocity + adjust) / (amount + 1.0f);

	if (ss->selectedWindow == ss->zoomedWindow)
	{
	    if (fabs (dx) < 0.1f   && fabs (ss->mVelocity) < 0.2f   &&
		fabs (dt) < 0.001f && fabs (ss->tVelocity) < 0.001f &&
		fabs (ds) < 0.001f && fabs (ss->sVelocity) < 0.001f)
	    {
		ss->mVelocity = ss->tVelocity = ss->sVelocity = 0.0f;
		return 0;
	    }
	}
    }
    else
    {
	if (fabs (dx) < 0.1f  && fabs (ss->mVelocity) < 0.2f)
	{
	    ss->mVelocity = 0.0f;
	    return 0;
	}
    }

    return 1;
}

static void
switchPreparePaintScreen (CompScreen *s,
			  int	     msSinceLastPaint)
{
    SWITCH_SCREEN (s);

    if (ss->moreAdjust)
    {
	int   steps, m;
	float amount, chunk;

	amount = msSinceLastPaint * 0.05f *
	    ss->opt[SWITCH_SCREEN_OPTION_SPEED].value.f;
	steps  = amount /
	    (0.5f * ss->opt[SWITCH_SCREEN_OPTION_TIMESTEP].value.f);
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    ss->moreAdjust = adjustSwitchVelocity (s);
	    if (!ss->moreAdjust)
	    {
		ss->pos += ss->move;
		ss->move = 0;

		if (ss->zooming)
		{
		    if (ss->switching)
		    {
			ss->translate  = ss->zoom;
			ss->sTranslate = ss->zoom;
		    }
		    else
		    {
			ss->translate  = 0.0f;
			ss->sTranslate = ss->zoom;

			ss->selectedWindow = None;
			ss->zoomedWindow   = None;

			if (ss->grabIndex)
			{
			    removeScreenGrab (s, ss->grabIndex, 0);
			    ss->grabIndex = 0;
			}

			switchActivateEvent (s, FALSE);
		    }
		}
		break;
	    }

	    m = ss->mVelocity * chunk;
	    if (!m)
	    {
		if (ss->mVelocity)
		    m = (ss->move > 0) ? 1 : -1;
	    }

	    ss->move -= m;
	    ss->pos  += m;
	    if (ss->pos < -ss->nWindows * WIDTH)
		ss->pos += ss->nWindows * WIDTH;
	    else if (ss->pos > 0)
		ss->pos -= ss->nWindows * WIDTH;

	    ss->translate  += ss->tVelocity * chunk;
	    ss->sTranslate += ss->sVelocity * chunk;

	    if (ss->selectedWindow != ss->zoomedWindow)
	    {
		if (ss->sTranslate < 0.01f)
		    ss->zoomedWindow = ss->selectedWindow;
	    }
	}
    }

    UNWRAP (ss, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (ss, s, preparePaintScreen, switchPreparePaintScreen);
}

static Bool
switchPaintOutput (CompScreen		   *s,
		   const ScreenPaintAttrib *sAttrib,
		   const CompTransform	   *transform,
		   Region		   region,
		   CompOutput		   *output,
		   unsigned int		   mask)
{
    Bool status;

    SWITCH_SCREEN (s);

    ss->zoomMask = ZOOMED_WINDOW_MASK | NORMAL_WINDOW_MASK;

    if (ss->grabIndex || (ss->zooming && ss->translate > 0.001f))
    {
	ScreenPaintAttrib sa = *sAttrib;
	CompWindow	  *zoomed;
	CompWindow	  *switcher;
	Window		  zoomedAbove = None;
	Bool		  saveDestroyed = FALSE;

	if (ss->zooming)
	{
	    mask &= ~PAINT_SCREEN_REGION_MASK;
	    mask |= PAINT_SCREEN_TRANSFORMED_MASK | PAINT_SCREEN_CLEAR_MASK;

	    sa.zCamera -= ss->translate;

	    ss->zoomMask = NORMAL_WINDOW_MASK;
	}

	switcher = findWindowAtScreen (s, ss->popupWindow);
	if (switcher)
	{
	    saveDestroyed = switcher->destroyed;
	    switcher->destroyed = TRUE;
	}

	if (ss->opt[SWITCH_SCREEN_OPTION_BRINGTOFRONT].value.b)
	{
	    zoomed = findWindowAtScreen (s, ss->zoomedWindow);
	    if (zoomed)
	    {
		CompWindow *w;

		for (w = zoomed->prev; w && w->id <= 1; w = w->prev);
		zoomedAbove = (w) ? w->id : None;

		unhookWindowFromScreen (s, zoomed);
		insertWindowIntoScreen (s, zoomed, s->reverseWindows->id);
	    }
	}
	else
	{
	    zoomed = NULL;
	}

	UNWRAP (ss, s, paintOutput);
	status = (*s->paintOutput) (s, &sa, transform, region, output, mask);
	WRAP (ss, s, paintOutput, switchPaintOutput);

	if (ss->zooming)
	{
	    mask &= ~PAINT_SCREEN_CLEAR_MASK;

	    ss->zoomMask = ZOOMED_WINDOW_MASK;

	    sa.zCamera += MIN (ss->sTranslate, ss->translate);

	    UNWRAP (ss, s, paintOutput);
	    status = (*s->paintOutput) (s, &sa, transform, region, output,
					mask);
	    WRAP (ss, s, paintOutput, switchPaintOutput);
	}

	if (zoomed)
	{
	    unhookWindowFromScreen (s, zoomed);
	    insertWindowIntoScreen (s, zoomed, zoomedAbove);
	}

	if (switcher)
	{
	    CompTransform sTransform = *transform;

	    switcher->destroyed = saveDestroyed;

	    transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &sTransform);

	    glPushMatrix ();
	    glLoadMatrixf (sTransform.m);

	    if (!switcher->destroyed			 &&
		switcher->attrib.map_state == IsViewable &&
		switcher->damaged)
	    {
		(*s->paintWindow) (switcher, &switcher->paint, &sTransform,
				   &infiniteRegion, 0);
	    }

	    glPopMatrix ();
	}
    }
    else
    {
	UNWRAP (ss, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, transform, region, output,
				    mask);
	WRAP (ss, s, paintOutput, switchPaintOutput);
    }

    return status;
}

static void
switchDonePaintScreen (CompScreen *s)
{
    SWITCH_SCREEN (s);

    if ((ss->grabIndex || ss->zooming) && ss->moreAdjust)
    {
	if (ss->zooming)
	{
	    damageScreen (s);
	}
	else
	{
	    CompWindow *w;

	    w = findWindowAtScreen (s, ss->popupWindow);
	    if (w)
		addWindowDamage (w);
	}
    }

    UNWRAP (ss, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ss, s, donePaintScreen, switchDonePaintScreen);
}

static void
switchPaintThumb (CompWindow		  *w,
		  const WindowPaintAttrib *attrib,
		  const CompTransform	  *transform,
		  unsigned int		  mask,
		  int			  x,
		  int			  y,
		  int			  x1,
		  int			  x2)
{
    WindowPaintAttrib sAttrib = *attrib;
    int		      wx, wy;
    float	      width, height;
    CompIcon	      *icon = NULL;

    mask |= PAINT_WINDOW_TRANSFORMED_MASK;

    if (w->mapNum)
    {
	if (!w->texture->pixmap && !w->bindFailed)
	    bindWindow (w);
    }

    if (w->texture->pixmap)
    {
	AddWindowGeometryProc oldAddWindowGeometry;
	FragmentAttrib	      fragment;
	CompTransform	      wTransform = *transform;
	int		      ww, wh;

	SWITCH_SCREEN (w->screen);

	width  = WIDTH  - (SPACE << 1);
	height = HEIGHT - (SPACE << 1);

	ww = w->width  + w->input.left + w->input.right;
	wh = w->height + w->input.top  + w->input.bottom;

	if (ww > width)
	    sAttrib.xScale = width / ww;
	else
	    sAttrib.xScale = 1.0f;

	if (wh > height)
	    sAttrib.yScale = height / wh;
	else
	    sAttrib.yScale = 1.0f;

	if (sAttrib.xScale < sAttrib.yScale)
	    sAttrib.yScale = sAttrib.xScale;
	else
	    sAttrib.xScale = sAttrib.yScale;

	width  = ww * sAttrib.xScale;
	height = wh * sAttrib.yScale;

	wx = x + SPACE + ((WIDTH  - (SPACE << 1)) - width)  / 2;
	wy = y + SPACE + ((HEIGHT - (SPACE << 1)) - height) / 2;

	sAttrib.xTranslate = wx - w->attrib.x + w->input.left * sAttrib.xScale;
	sAttrib.yTranslate = wy - w->attrib.y + w->input.top  * sAttrib.yScale;

	initFragmentAttrib (&fragment, &sAttrib);

	if (w->alpha || fragment.opacity != OPAQUE)
	    mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

	matrixTranslate (&wTransform, w->attrib.x, w->attrib.y, 0.0f);
	matrixScale (&wTransform, sAttrib.xScale, sAttrib.yScale, 1.0f);
	matrixTranslate (&wTransform,
			 sAttrib.xTranslate / sAttrib.xScale - w->attrib.x,
			 sAttrib.yTranslate / sAttrib.yScale - w->attrib.y,
			 0.0f);

	glPushMatrix ();
	glLoadMatrixf (wTransform.m);

	/* XXX: replacing the addWindowGeometry function like this is
	   very ugly but necessary until the vertex stage has been made
	   fully pluggable. */
	oldAddWindowGeometry = w->screen->addWindowGeometry;
	w->screen->addWindowGeometry = addWindowGeometry;
	(w->screen->drawWindow) (w, &wTransform, &fragment, &infiniteRegion,
				 mask);
	w->screen->addWindowGeometry = oldAddWindowGeometry;

	glPopMatrix ();

	if (ss->opt[SWITCH_SCREEN_OPTION_ICON].value.b)
	{
	    icon = getWindowIcon (w, ICON_SIZE, ICON_SIZE);
	    if (icon)
	    {
		sAttrib.xScale = sAttrib.yScale = 1.0f;

		wx = x + WIDTH  - icon->width  - SPACE;
		wy = y + HEIGHT - icon->height - SPACE;
	    }
	}
    }
    else
    {
	width  = WIDTH  - (WIDTH  >> 2);
	height = HEIGHT - (HEIGHT >> 2);

	icon = getWindowIcon (w, width, height);
	if (!icon)
	    icon = w->screen->defaultIcon;

	if (icon)
	{
	    int iw, ih;

	    iw = width  - SPACE;
	    ih = height - SPACE;

	    if (icon->width < (iw >> 1))
		sAttrib.xScale = (iw / icon->width);
	    else
		sAttrib.xScale = 1.0f;

	    if (icon->height < (ih >> 1))
		sAttrib.yScale = (ih / icon->height);
	    else
		sAttrib.yScale = 1.0f;

	    if (sAttrib.xScale < sAttrib.yScale)
		sAttrib.yScale = sAttrib.xScale;
	    else
		sAttrib.xScale = sAttrib.yScale;

	    width  = icon->width  * sAttrib.xScale;
	    height = icon->height * sAttrib.yScale;

	    wx = x + SPACE + ((WIDTH  - (SPACE << 1)) - width)  / 2;
	    wy = y + SPACE + ((HEIGHT - (SPACE << 1)) - height) / 2;
	}
    }

    if (icon && (icon->texture.name || iconToTexture (w->screen, icon)))
    {
	REGION     iconReg;
	CompMatrix matrix;

	mask |= PAINT_WINDOW_BLEND_MASK;

	iconReg.rects    = &iconReg.extents;
	iconReg.numRects = 1;

	iconReg.extents.x1 = w->attrib.x;
	iconReg.extents.y1 = w->attrib.y;
	iconReg.extents.x2 = w->attrib.x + icon->width;
	iconReg.extents.y2 = w->attrib.y + icon->height;

	matrix = icon->texture.matrix;
	matrix.x0 -= (w->attrib.x * icon->texture.matrix.xx);
	matrix.y0 -= (w->attrib.y * icon->texture.matrix.yy);

	sAttrib.xTranslate = wx - w->attrib.x;
	sAttrib.yTranslate = wy - w->attrib.y;

	w->vCount = w->indexCount = 0;
	addWindowGeometry (w, &matrix, 1, &iconReg, &infiniteRegion);
	if (w->vCount)
	{
	    FragmentAttrib fragment;
	    CompTransform  wTransform = *transform;

	    initFragmentAttrib (&fragment, &sAttrib);

	    matrixTranslate (&wTransform, w->attrib.x, w->attrib.y, 0.0f);
	    matrixScale (&wTransform, sAttrib.xScale, sAttrib.yScale, 1.0f);
	    matrixTranslate (&wTransform,
			     sAttrib.xTranslate / sAttrib.xScale - w->attrib.x,
			     sAttrib.yTranslate / sAttrib.yScale - w->attrib.y,
			     0.0f);

	    glPushMatrix ();
	    glLoadMatrixf (wTransform.m);

	    (*w->screen->drawWindowTexture) (w,
					     &icon->texture, &fragment,
					     mask);

	    glPopMatrix ();
	}
    }
}

static Bool
switchPaintWindow (CompWindow		   *w,
		   const WindowPaintAttrib *attrib,
		   const CompTransform	   *transform,
		   Region		   region,
		   unsigned int		   mask)
{
    CompScreen *s = w->screen;
    int	       zoomType = NORMAL_WINDOW_MASK;
    Bool       status;

    SWITCH_SCREEN (s);

    if (w->id == ss->popupWindow)
    {
	GLenum filter;
	int    x, y, x1, x2, cx, i;

	if (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK)
	    return FALSE;

	UNWRAP (ss, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, transform, region, mask);
	WRAP (ss, s, paintWindow, switchPaintWindow);

	if (!(mask & PAINT_WINDOW_TRANSFORMED_MASK) && region->numRects == 0)
	    return TRUE;

	x1 = w->attrib.x + SPACE;
	x2 = w->attrib.x + w->width - SPACE;

	x = x1 + ss->pos;
	y = w->attrib.y + SPACE;

	filter = s->display->textureFilter;

	if (ss->opt[SWITCH_SCREEN_OPTION_MIPMAP].value.b)
	    s->display->textureFilter = GL_LINEAR_MIPMAP_LINEAR;

	glPushAttrib (GL_SCISSOR_BIT);

	glEnable (GL_SCISSOR_TEST);
	glScissor (x1, 0, x2 - x1, w->screen->height);

	for (i = 0; i < ss->nWindows; i++)
	{
	    if (x + WIDTH > x1)
		switchPaintThumb (ss->windows[i], &w->lastPaint, transform,
				  mask, x, y, x1, x2);

	    x += WIDTH;
	}

	for (i = 0; i < ss->nWindows; i++)
	{
	    if (x > x2)
		break;

	    switchPaintThumb (ss->windows[i], &w->lastPaint, transform, mask,
			      x, y, x1, x2);

	    x += WIDTH;
	}

	glPopAttrib ();

	s->display->textureFilter = filter;

	cx = w->attrib.x + (w->width >> 1);

	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	glEnable (GL_BLEND);
	glColor4us (0, 0, 0, w->lastPaint.opacity);
	glPushMatrix ();
	glTranslatef (cx, y, 0.0f);
	glVertexPointer (2, GL_FLOAT, 0, _boxVertices);
	glDrawArrays (GL_QUADS, 0, 16);
	glPopMatrix ();
	glColor4usv (defaultColor);
	glDisable (GL_BLEND);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    }
    else if (w->id == ss->selectedWindow)
    {
	if (ss->opt[SWITCH_SCREEN_OPTION_BRINGTOFRONT].value.b &&
	    ss->selectedWindow == ss->zoomedWindow)
	    zoomType = ZOOMED_WINDOW_MASK;

	if (!(ss->zoomMask & zoomType))
	    return (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK) ?
		FALSE : TRUE;

	UNWRAP (ss, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, transform, region, mask);
	WRAP (ss, s, paintWindow, switchPaintWindow);
    }
    else if (ss->switching)
    {
	WindowPaintAttrib sAttrib = *attrib;

	if (ss->saturation != COLOR)
	    sAttrib.saturation = (sAttrib.saturation * ss->saturation) >> 16;

	if (ss->brightness != 0xffff)
	    sAttrib.brightness = (sAttrib.brightness * ss->brightness) >> 16;

	if (w->wmType & ~(CompWindowTypeDockMask | CompWindowTypeDesktopMask))
	{
	    if (ss->opacity != OPAQUE)
		sAttrib.opacity = (sAttrib.opacity * ss->opacity) >> 16;
	}

	if (ss->opt[SWITCH_SCREEN_OPTION_BRINGTOFRONT].value.b &&
	    w->id == ss->zoomedWindow)
	    zoomType = ZOOMED_WINDOW_MASK;

	if (!(ss->zoomMask & zoomType))
	    return (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK) ?
		FALSE : TRUE;

	UNWRAP (ss, s, paintWindow);
	status = (*s->paintWindow) (w, &sAttrib, transform, region, mask);
	WRAP (ss, s, paintWindow, switchPaintWindow);
    }
    else
    {
	if (!(ss->zoomMask & zoomType))
	    return (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK) ?
		FALSE : TRUE;

	UNWRAP (ss, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, transform, region, mask);
	WRAP (ss, s, paintWindow, switchPaintWindow);
    }

    return status;
}

static void
switchPaintBackground (CompScreen   *s,
		       Region	    region,
		       unsigned int mask)
{
    SWITCH_SCREEN (s);

    if (!(ss->zoomMask & NORMAL_WINDOW_MASK))
	return;

    UNWRAP (ss, s, paintBackground);
    (*s->paintBackground) (s, region, mask);
    WRAP (ss, s, paintBackground, switchPaintBackground);
}

static Bool
switchDamageWindowRect (CompWindow *w,
			Bool	   initial,
			BoxPtr     rect)
{
    Bool status;

    SWITCH_SCREEN (w->screen);

    if (ss->grabIndex)
    {
	CompWindow *popup;
	int	   i;

	for (i = 0; i < ss->nWindows; i++)
	{
	    if (ss->windows[i] == w)
	    {
		popup = findWindowAtScreen (w->screen, ss->popupWindow);
		if (popup)
		    addWindowDamage (popup);

		break;
	    }
	}
    }

    UNWRAP (ss, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ss, w->screen, damageWindowRect, switchDamageWindowRect);

    return status;
}

static CompOption *
switchGetDisplayOptions (CompPlugin  *plugin,
			 CompDisplay *display,
			 int	     *count)
{
    SWITCH_DISPLAY (display);

    *count = NUM_OPTIONS (sd);
    return sd->opt;
}

static Bool
switchSetDisplayOption (CompPlugin      *plugin,
			CompDisplay     *display,
			const char	*name,
			CompOptionValue *value)
{
    CompOption *o;

    SWITCH_DISPLAY (display);

    o = compFindOption (sd->opt, NUM_OPTIONS (sd), name, NULL);
    if (!o)
	return FALSE;

    return compSetDisplayOption (display, o, value);
}

static const CompMetadataOptionInfo switchDisplayOptionInfo[] = {
    { "next_button", "button", 0, switchNext, switchTerminate },
    { "next_key", "key", 0, switchNext, switchTerminate },
    { "prev_button", "button", 0, switchPrev, switchTerminate },
    { "prev_key", "key", 0, switchPrev, switchTerminate },
    { "next_all_button", "button", 0, switchNextAll, switchTerminate },
    { "next_all_key", "key", 0, switchNextAll, switchTerminate },
    { "prev_all_button", "button", 0, switchPrevAll, switchTerminate },
    { "prev_all_key", "key", 0, switchPrevAll, switchTerminate },
    { "next_no_popup_button", "button", 0, switchNextNoPopup,
      switchTerminate },
    { "next_no_popup_key", "key", 0, switchNextNoPopup, switchTerminate },
    { "prev_no_popup_button", "button", 0, switchPrevNoPopup,
      switchTerminate },
    { "prev_no_popup_key", "key", 0, switchPrevNoPopup, switchTerminate }
};

static Bool
switchInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    SwitchDisplay *sd;

    if (!checkPluginABI ("core", ABIVERSION))
	return FALSE;

    sd = malloc (sizeof (SwitchDisplay));
    if (!sd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &switchMetadata,
					     switchDisplayOptionInfo,
					     sd->opt,
					     SWITCH_DISPLAY_OPTION_NUM))
    {
	free (sd);
	return FALSE;
    }

    sd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (sd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, sd->opt, SWITCH_DISPLAY_OPTION_NUM);
	free (sd);
	return FALSE;
    }

    sd->selectWinAtom = XInternAtom (d->display, SELECT_WIN_PROP, 0);

    WRAP (sd, d, handleEvent, switchHandleEvent);

    d->privates[displayPrivateIndex].ptr = sd;

    return TRUE;
}

static void
switchFiniDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    SWITCH_DISPLAY (d);

    freeScreenPrivateIndex (d, sd->screenPrivateIndex);

    UNWRAP (sd, d, handleEvent);

    compFiniDisplayOptions (d, sd->opt, SWITCH_DISPLAY_OPTION_NUM);

    free (sd);
}

static const CompMetadataOptionInfo switchScreenOptionInfo[] = {
    { "speed", "float", "<min>0.1</min>", 0, 0 },
    { "timestep", "float", "<min>0.1</min>", 0, 0 },
    { "window_match", "match", 0, 0, 0 },
    { "mipmap", "bool", 0, 0, 0 },
    { "saturation", "int", "<min>0</min><max>100</max>", 0, 0 },
    { "brightness", "int", "<min>0</min><max>100</max>", 0, 0 },
    { "opacity", "int", "<min>0</min><max>100</max>", 0, 0 },
    { "bring_to_front", "bool", 0, 0, 0 },
    { "zoom", "float", "<min>0</min>", 0, 0 },
    { "icon", "bool", 0, 0, 0 },
    { "minimized", "bool", 0, 0, 0 },
    { "auto_rotate", "bool", 0, 0, 0 }
};

static Bool
switchInitScreen (CompPlugin *p,
		  CompScreen *s)
{
    SwitchScreen *ss;

    SWITCH_DISPLAY (s->display);

    ss = malloc (sizeof (SwitchScreen));
    if (!ss)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &switchMetadata,
					    switchScreenOptionInfo,
					    ss->opt,
					    SWITCH_SCREEN_OPTION_NUM))
    {
	free (ss);
	return FALSE;
    }

    ss->popupWindow = None;

    ss->selectedWindow = None;
    ss->zoomedWindow   = None;

    ss->lastActiveNum  = 0;

    ss->windows     = 0;
    ss->nWindows    = 0;
    ss->windowsSize = 0;

    ss->pos = ss->move = 0;

    ss->switching = FALSE;

    ss->grabIndex = 0;

    ss->zoom = ss->opt[SWITCH_SCREEN_OPTION_ZOOM].value.f / 30.0f;

    ss->zooming = (ss->opt[SWITCH_SCREEN_OPTION_ZOOM].value.f > 0.05f);

    ss->zoomMask = ~0;

    ss->moreAdjust = 0;

    ss->mVelocity = 0.0f;
    ss->tVelocity = 0.0f;
    ss->sVelocity = 0.0f;

    ss->translate  = 0.0f;
    ss->sTranslate = 0.0f;

    ss->saturation =
	(COLOR  * ss->opt[SWITCH_SCREEN_OPTION_SATURATION].value.i) / 100;
    ss->brightness =
	(0xffff * ss->opt[SWITCH_SCREEN_OPTION_BRIGHTNESS].value.i) / 100;
    ss->opacity    =
	(OPAQUE * ss->opt[SWITCH_SCREEN_OPTION_OPACITY].value.i)    / 100;

    ss->allWindows = FALSE;

    WRAP (ss, s, preparePaintScreen, switchPreparePaintScreen);
    WRAP (ss, s, donePaintScreen, switchDonePaintScreen);
    WRAP (ss, s, paintOutput, switchPaintOutput);
    WRAP (ss, s, paintWindow, switchPaintWindow);
    WRAP (ss, s, paintBackground, switchPaintBackground);
    WRAP (ss, s, damageWindowRect, switchDamageWindowRect);

    s->privates[sd->screenPrivateIndex].ptr = ss;

    return TRUE;
}

static void
switchFiniScreen (CompPlugin *p,
		  CompScreen *s)
{
    SWITCH_SCREEN (s);

    UNWRAP (ss, s, preparePaintScreen);
    UNWRAP (ss, s, donePaintScreen);
    UNWRAP (ss, s, paintOutput);
    UNWRAP (ss, s, paintWindow);
    UNWRAP (ss, s, paintBackground);
    UNWRAP (ss, s, damageWindowRect);

    if (ss->windowsSize)
	free (ss->windows);

    compFiniScreenOptions (s, ss->opt, SWITCH_SCREEN_OPTION_NUM);

    free (ss);
}

static Bool
switchInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&switchMetadata,
					 p->vTable->name,
					 switchDisplayOptionInfo,
					 SWITCH_DISPLAY_OPTION_NUM,
					 switchScreenOptionInfo,
					 SWITCH_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&switchMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&switchMetadata, p->vTable->name);

    return TRUE;
}

static void
switchFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&switchMetadata);
}

static CompMetadata *
switchGetMetadata (CompPlugin *plugin)
{
    return &switchMetadata;
}

CompPluginVTable switchVTable = {
    "switcher",
    switchGetMetadata,
    switchInit,
    switchFini,
    switchInitDisplay,
    switchFiniDisplay,
    switchInitScreen,
    switchFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    switchGetDisplayOptions,
    switchSetDisplayOption,
    switchGetScreenOptions,
    switchSetScreenOption
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &switchVTable;
}
