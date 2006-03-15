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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xrandr.h>

#include <compiz.h>

static void
handleWindowDamageRect (CompWindow *w,
			int	   x,
			int	   y,
			int	   width,
			int	   height)
{
    REGION region;
    Bool   initial = FALSE;

    if (!w->damaged)
    {
	w->damaged = initial = TRUE;
	w->invisible = WINDOW_INVISIBLE (w);
    }

    region.extents.x1 = x;
    region.extents.y1 = y;
    region.extents.x2 = region.extents.x1 + width;
    region.extents.y2 = region.extents.y1 + height;

    if (!(*w->screen->damageWindowRect) (w, initial, &region.extents))
    {
	region.extents.x1 += w->attrib.x + w->attrib.border_width;
	region.extents.y1 += w->attrib.y + w->attrib.border_width;
	region.extents.x2 += w->attrib.x + w->attrib.border_width;
	region.extents.y2 += w->attrib.y + w->attrib.border_width;

	region.rects = &region.extents;
	region.numRects = region.size = 1;

	damageWindowRegion (w, &region);

	if (initial)
	    damageWindowOutputExtents (w);
    }
}

void
handleSyncAlarm (CompWindow *w)
{
    if (w->syncWait)
    {
	if (w->syncWaitHandle)
	{
	    compRemoveTimeout (w->syncWaitHandle);
	    w->syncWaitHandle = 0;
	}

	if (resizeWindow (w,
			  w->syncX, w->syncY,
			  w->syncWidth, w->syncHeight,
			  w->syncBorderWidth))
	{
	    XRectangle *rects;
	    int	       nDamage;

	    nDamage = w->nDamage;
	    rects   = w->damageRects;
	    while (nDamage--)
	    {
		handleWindowDamageRect (w,
					rects[nDamage].x,
					rects[nDamage].y,
					rects[nDamage].width,
					rects[nDamage].height);
	    }

	    w->nDamage = 0;
	    w->syncWait = FALSE;
	}
    }
}

static void
moveInputFocusToOtherWindow (CompWindow *w)
{
    CompDisplay *display = w->screen->display;

    if (w->id == display->activeWindow)
    {
	CompWindow *ancestor;

	if (w->transientFor && w->transientFor != w->screen->root)
	{
	    ancestor = findWindowAtDisplay (display, w->transientFor);
	    if (ancestor && !(ancestor->type & (CompWindowTypeDesktopMask |
						CompWindowTypeDockMask)))
	    {
		moveInputFocusToWindow (ancestor);
	    }
	    else
		focusDefaultWindow (display);
	}
	else if (w->type & (CompWindowTypeDialogMask |
			    CompWindowTypeModalDialogMask))
	{
	    CompWindow *a, *focus = NULL;

	    for (a = w->screen->reverseWindows; a; a = a->prev)
	    {
		if (a->clientLeader == w->clientLeader)
		{
		    if ((*w->screen->focusWindow) (a))
		    {
			if (focus)
			{
			    if (a->type & (CompWindowTypeNormalMask |
					   CompWindowTypeDialogMask |
					   CompWindowTypeModalDialogMask))
			    {
				if (a->activeNum > focus->activeNum)
				    focus = a;
			    }
			}
			else
			    focus = a;
		    }
		}
	    }

	    if (focus && !(focus->type & (CompWindowTypeDesktopMask |
					  CompWindowTypeDockMask)))
	    {
		moveInputFocusToWindow (focus);
	    }
	    else
		focusDefaultWindow (display);
	}
	else
	    focusDefaultWindow (display);
    }
}

static Bool
autoRaiseTimeout (void *closure)
{
    CompDisplay *display = closure;

    if (display->autoRaiseWindow == display->activeWindow)
    {
	CompWindow *w;

	w = findWindowAtDisplay (display, display->autoRaiseWindow);
	if (w)
	    updateWindowAttributes (w);
    }

    return FALSE;
}

static void
changeWindowOpacity (CompWindow *w,
		     int	direction)
{
    int step, opacity;

    if (w->attrib.override_redirect)
	return;

    if (w->type & CompWindowTypeDesktopMask)
	return;

    step = (OPAQUE * w->screen->opacityStep) / 100;

    opacity = w->paint.opacity + step * direction;
    if (opacity > OPAQUE)
    {
	opacity = OPAQUE;
    }
    else if (opacity < step)
    {
	opacity = step;
    }

    if (w->paint.opacity != opacity)
    {
	w->paint.opacity = opacity;

	setWindowProp32 (w->screen->display, w->id,
			 w->screen->display->winOpacityAtom,
			 w->paint.opacity);
	addWindowDamage (w);
    }
}

void
handleEvent (CompDisplay *display,
	     XEvent      *event)
{
    CompScreen *s;
    CompWindow *w;

    switch (event->type) {
    case Expose:
	s = findScreenAtDisplay (display, event->xexpose.window);
	if (s)
	{
	    int more = event->xexpose.count + 1;

	    if (s->nExpose == s->sizeExpose)
	    {
		if (s->exposeRects)
		{
		    s->exposeRects = realloc (s->exposeRects,
					      (s->sizeExpose + more) *
					      sizeof (XRectangle));
		    s->sizeExpose += more;
		}
		else
		{
		    s->exposeRects = malloc (more * sizeof (XRectangle));
		    s->sizeExpose = more;
		}
	    }

	    s->exposeRects[s->nExpose].x      = event->xexpose.x;
	    s->exposeRects[s->nExpose].y      = event->xexpose.y;
	    s->exposeRects[s->nExpose].width  = event->xexpose.width;
	    s->exposeRects[s->nExpose].height = event->xexpose.height;
	    s->nExpose++;

	    if (event->xexpose.count == 0)
	    {
		REGION rect;

		rect.rects = &rect.extents;
		rect.numRects = rect.size = 1;

		while (s->nExpose--)
		{
		    rect.extents.x1 = s->exposeRects[s->nExpose].x;
		    rect.extents.y1 = s->exposeRects[s->nExpose].y;
		    rect.extents.x2 = rect.extents.x1 +
			s->exposeRects[s->nExpose].width;
		    rect.extents.y2 = rect.extents.y1 +
			s->exposeRects[s->nExpose].height;

		    damageScreenRegion (s, &rect);
		}
		s->nExpose = 0;
	    }
	}
	break;
    case SelectionRequest:
	handleSelectionRequest (display, event);
	break;
    case SelectionClear:
	handleSelectionClear (display, event);
	break;
    case ConfigureNotify:
	w = findWindowAtDisplay (display, event->xconfigure.window);
	if (w)
	{
	    configureWindow (w, &event->xconfigure);
	}
	else
	{
	    s = findScreenAtDisplay (display, event->xconfigure.window);
	    if (s)
		configureScreen (s, &event->xconfigure);
	}
	break;
    case CreateNotify:
	s = findScreenAtDisplay (display, event->xcreatewindow.parent);
	if (s)
	{
	    addWindow (s, event->xcreatewindow.window,
		       s->reverseWindows ? s->reverseWindows->id : 0);
	}
	break;
    case DestroyNotify:
	w = findWindowAtDisplay (display, event->xdestroywindow.window);
	if (w)
	{
	    destroyWindow (w);
	    moveInputFocusToOtherWindow (w);
	}
	break;
    case MapNotify:
	w = findWindowAtDisplay (display, event->xmap.window);
	if (w)
	    mapWindow (w);
	break;
    case UnmapNotify:
	w = findWindowAtDisplay (display, event->xunmap.window);
	if (w)
	{
	    unmapWindow (w);
	    moveInputFocusToOtherWindow (w);
	}
	break;
    case ReparentNotify:
	s = findScreenAtDisplay (display, event->xreparent.parent);
	if (s)
	{
	    addWindow (s, event->xreparent.window, 0);
	}
	else
	{
	    w = findWindowAtDisplay (display, event->xreparent.window);
	    if (w)
	    {
		destroyWindow (w);
		moveInputFocusToOtherWindow (w);
	    }
	}
	break;
    case CirculateNotify:
	w = findWindowAtDisplay (display, event->xcirculate.window);
	if (w)
	    circulateWindow (w, &event->xcirculate);
	break;
    case ButtonPress:
	s = findScreenAtDisplay (display, event->xbutton.root);
	if (s)
	{
	    int eventMode = ReplayPointer;

	    if (event->xbutton.button == Button1 ||
		event->xbutton.button == Button3)
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		{
		    if (!(w->type & CompWindowTypeDockMask))
			activateWindow (w);
		}
	    }

	    if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_CLOSE_WINDOW], event))
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		    closeWindow (w);

		eventMode = AsyncPointer;
	    }

	    if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_MAXIMIZE_WINDOW], event))
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		    maximizeWindow (w);

		eventMode = AsyncPointer;
	    }

	    if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_UNMAXIMIZE_WINDOW],
			   event))
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		    unmaximizeWindow (w);

		eventMode = AsyncPointer;
	    }

	    if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_MINIMIZE_WINDOW], event))
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		    minimizeWindow (w);

		eventMode = AsyncPointer;
	    }

	    /* avoid panel actions when screen is grabbed */
	    if (!display->screens->maxGrab)
	    {
		if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_MAIN_MENU], event))
		{
		    panelAction (s, s->display->panelActionMainMenuAtom);
		    eventMode = AsyncPointer;
		}

		if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_RUN_DIALOG], event))
		{
		    panelAction (s, s->display->panelActionRunDialogAtom);
		    eventMode = AsyncPointer;
		}
	    }

#define EV_BUTTON_COMMAND(num)						   \
    if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_RUN_COMMAND ## num], event)) \
    {									   \
	runCommand (s, s->opt[COMP_SCREEN_OPTION_COMMAND ## num].value.s); \
	eventMode = AsyncPointer;					   \
    }

	    EV_BUTTON_COMMAND (0);
	    EV_BUTTON_COMMAND (1);
	    EV_BUTTON_COMMAND (2);
	    EV_BUTTON_COMMAND (3);
	    EV_BUTTON_COMMAND (4);
	    EV_BUTTON_COMMAND (5);
	    EV_BUTTON_COMMAND (6);
	    EV_BUTTON_COMMAND (7);
	    EV_BUTTON_COMMAND (8);
	    EV_BUTTON_COMMAND (9);
	    EV_BUTTON_COMMAND (10);
	    EV_BUTTON_COMMAND (11);

	    if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_SLOW_ANIMATIONS], event))
	    {
		s->slowAnimations = !s->slowAnimations;
		eventMode = AsyncPointer;
	    }

	    if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_LOWER_WINDOW], event))
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		    lowerWindow (w);

		eventMode = AsyncPointer;
	    }

	    if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_OPACITY_INCREASE], event))
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		    changeWindowOpacity (w, 1);

		eventMode = AsyncPointer;
	    }

	    if (EV_BUTTON (&s->opt[COMP_SCREEN_OPTION_OPACITY_DECREASE], event))
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		    changeWindowOpacity (w, -1);

		eventMode = AsyncPointer;
	    }

	    if (!display->screens->maxGrab)
		XAllowEvents (display->display, eventMode, event->xbutton.time);
	}
	else if (!display->screens->maxGrab)
	    XAllowEvents (display->display, ReplayPointer,
			  event->xbutton.time);
	break;
    case ButtonRelease:
	break;
    case KeyPress:
	s = findScreenAtDisplay (display, event->xkey.root);
	if (s)
	{
	    if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_CLOSE_WINDOW], event))
	    {
		w = findTopLevelWindowAtScreen (s, display->activeWindow);
		if (w)
		    closeWindow (w);
	    }

	    if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_MAXIMIZE_WINDOW], event))
	    {
		w = findTopLevelWindowAtScreen (s, display->activeWindow);
		if (w)
		    maximizeWindow (w);
	    }

	    if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_UNMAXIMIZE_WINDOW], event))
	    {
		w = findTopLevelWindowAtScreen (s, display->activeWindow);
		if (w)
		    unmaximizeWindow (w);
	    }

	    if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_MINIMIZE_WINDOW], event))
	    {
		w = findTopLevelWindowAtScreen (s, display->activeWindow);
		if (w)
		    minimizeWindow (w);
	    }

	    /* avoid panel actions when screen is grabbed */
	    if (!display->screens->maxGrab)
	    {
		if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_MAIN_MENU], event))
		    panelAction (s, s->display->panelActionMainMenuAtom);

		if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_RUN_DIALOG], event))
		    panelAction (s, s->display->panelActionRunDialogAtom);
	    }

#define EV_KEY_COMMAND(num)						\
    if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_RUN_COMMAND ## num], event)) \
	runCommand (s, s->opt[COMP_SCREEN_OPTION_COMMAND ## num].value.s)

	    EV_KEY_COMMAND (0);
	    EV_KEY_COMMAND (1);
	    EV_KEY_COMMAND (2);
	    EV_KEY_COMMAND (3);
	    EV_KEY_COMMAND (4);
	    EV_KEY_COMMAND (5);
	    EV_KEY_COMMAND (6);
	    EV_KEY_COMMAND (7);
	    EV_KEY_COMMAND (8);
	    EV_KEY_COMMAND (9);
	    EV_KEY_COMMAND (10);
	    EV_KEY_COMMAND (11);

	    if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_SLOW_ANIMATIONS], event))
		s->slowAnimations = !s->slowAnimations;

	    if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_LOWER_WINDOW], event))
	    {
		w = findTopLevelWindowAtScreen (s, display->activeWindow);
		if (w)
		    lowerWindow (w);
	    }

	    if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_OPACITY_INCREASE], event))
	    {
		w = findTopLevelWindowAtScreen (s, display->activeWindow);
		if (w)
		    changeWindowOpacity (w, 1);
	    }

	    if (EV_KEY (&s->opt[COMP_SCREEN_OPTION_OPACITY_DECREASE], event))
	    {
		w = findTopLevelWindowAtScreen (s, display->activeWindow);
		if (w)
		    changeWindowOpacity (w, -1);
	    }
	}
	break;
    case KeyRelease:
	break;
    case PropertyNotify:
	if (event->xproperty.atom == display->winActiveAtom)
	{
	    Window newActiveWindow;

	    newActiveWindow = getActiveWindow (display,
					       event->xproperty.window);
	    if (newActiveWindow != display->activeWindow)
	    {
		display->activeWindow = newActiveWindow;

		s = findScreenAtDisplay (display, event->xproperty.window);
		if (s)
		{
		    w = findWindowAtDisplay (display, newActiveWindow);
		    if (w)
			w->activeNum = s->activeNum++;
		}
	    }
	}
	else if (event->xproperty.atom == display->winTypeAtom)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
	    {
		unsigned int type;

		type = getWindowType (display, w->id);

		if (type != w->wmType)
		{
		    if (w->attrib.map_state == IsViewable)
		    {
			if (w->type == CompWindowTypeDesktopMask)
			    w->screen->desktopWindowCount--;
			else if (type == CompWindowTypeDesktopMask)
			    w->screen->desktopWindowCount++;
		    }

		    w->wmType = type;

		    recalcWindowType (w);

		    if (w->type & CompWindowTypeDesktopMask)
			w->paint.opacity = OPAQUE;
		}
	    }
	}
	else if (event->xproperty.atom == display->winStateAtom)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
	    {
		unsigned int state;

		state = getWindowState (display, w->id);

		if (state != w->state)
		{
		    w->state = state;

		    recalcWindowType (w);

		    if (w->type & CompWindowTypeDesktopMask)
			w->paint.opacity = OPAQUE;
		}
	    }
	}
	else if (event->xproperty.atom == XA_WM_NORMAL_HINTS)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
		updateNormalHints (w);
	}
	else if (event->xproperty.atom == XA_WM_HINTS)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
		updateWmHints (w);
	}
	else if (event->xproperty.atom == XA_WM_TRANSIENT_FOR)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
		XGetTransientForHint (display->display,
				      w->id, &w->transientFor);
	}
	else if (event->xproperty.atom == display->wmClientLeaderAtom)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
		w->clientLeader = getClientLeader (w);
	}
	else if (event->xproperty.atom == display->winOpacityAtom)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w && (w->type & CompWindowTypeDesktopMask) == 0)
	    {
		GLushort opacity;

		opacity = getWindowProp32 (display, w->id,
					   display->winOpacityAtom,
					   OPAQUE);
		if (opacity != w->opacity)
		{
		    w->opacity = w->paint.opacity = opacity;
		    addWindowDamage (w);
		}
	    }
	}
	else if (event->xproperty.atom == display->winBrightnessAtom)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
	    {
		GLushort brightness;

		brightness = getWindowProp32 (display, w->id,
					      display->winBrightnessAtom,
					      BRIGHT);
		if (brightness != w->brightness)
		{
		    w->brightness = brightness;
		    if (w->alive)
		    {
			w->paint.brightness = w->brightness;
			addWindowDamage (w);
		    }
		}
	    }
	}
	else if (event->xproperty.atom == display->winSaturationAtom)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w && w->screen->canDoSaturated)
	    {
		GLushort saturation;

		saturation = getWindowProp32 (display, w->id,
					      display->winSaturationAtom,
					      COLOR);
		if (saturation != w->saturation)
		{
		    w->saturation = saturation;
		    if (w->alive)
		    {
			w->paint.saturation = w->saturation;
			addWindowDamage (w);
		    }
		}
	    }
	}
	else if (event->xproperty.atom == display->xBackgroundAtom[0] ||
		 event->xproperty.atom == display->xBackgroundAtom[1])
	{
	    s = findScreenAtDisplay (display, event->xproperty.window);
	    if (s)
	    {
		finiTexture (s, &s->backgroundTexture);
		initTexture (s, &s->backgroundTexture);

		damageScreen (s);
	    }
	}
	else if (event->xproperty.atom == display->wmStrutAtom ||
		 event->xproperty.atom == display->wmStrutPartialAtom)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
	    {
		if (updateWindowStruts (w))
		    updateWorkareaForScreen (w->screen);
	    }
	}
	else if (event->xproperty.atom == display->mwmHintsAtom)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
	    {
		w->mwmDecor = getMwmDecor (w->screen->display, w->id);

		updateWindowAttributes (w);
	    }
	}
	else if (event->xproperty.atom == display->wmProtocolsAtom)
	{
	    w = findWindowAtDisplay (display, event->xproperty.window);
	    if (w)
		w->protocols = getProtocols (w->screen->display, w->id);
	}
	break;
    case MotionNotify:
	break;
    case ClientMessage:
	if (event->xclient.message_type == display->winActiveAtom)
	{
	    w = findWindowAtDisplay (display, event->xclient.window);
	    if (w)
		activateWindow (w);
	}
	else if (event->xclient.message_type == display->winOpacityAtom)
	{
	    w = findWindowAtDisplay (display, event->xclient.window);
	    if (w && (w->type & CompWindowTypeDesktopMask) == 0)
	    {
		GLushort opacity;

		opacity = event->xclient.data.l[0] >> 16;
		if (opacity != w->paint.opacity)
		{
		    w->paint.opacity = opacity;

		    setWindowProp32 (display, w->id, display->winOpacityAtom,
				     w->paint.opacity);
		    addWindowDamage (w);
		}
	    }
	}
	else if (event->xclient.message_type == display->winBrightnessAtom)
	{
	    w = findWindowAtDisplay (display, event->xclient.window);
	    if (w)
	    {
		GLushort brightness;

		brightness = event->xclient.data.l[0] >> 16;
		if (brightness != w->paint.brightness)
		{
		    w->paint.brightness = brightness;

		    setWindowProp32 (display, w->id, display->winBrightnessAtom,
				     w->paint.brightness);
		    addWindowDamage (w);
		}
	    }
	}
	else if (event->xclient.message_type == display->winSaturationAtom)
	{
	    w = findWindowAtDisplay (display, event->xclient.window);
	    if (w && w->screen->canDoSaturated)
	    {
		GLushort saturation;

		saturation = event->xclient.data.l[0] >> 16;
		if (saturation != w->saturation)
		{
		    w->saturation = saturation;

		    setWindowProp32 (display, w->id, display->winSaturationAtom,
				     w->saturation);

		    if (w->alive)
		    {
			w->paint.saturation = w->saturation;
			addWindowDamage (w);
		    }
		}
	    }
	}
	else if (event->xclient.message_type == display->winStateAtom)
	{
	    w = findWindowAtDisplay (display, event->xclient.window);
	    if (w)
	    {
		unsigned long wState, state;
		int	      i;

		wState = w->state;

		for (i = 1; i < 3; i++)
		{
		    state = windowStateMask (display, event->xclient.data.l[i]);
		    if (state & ~CompWindowStateHiddenMask)
		    {

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD    1
#define _NET_WM_STATE_TOGGLE 2

			switch (event->xclient.data.l[0]) {
			case _NET_WM_STATE_REMOVE:
			    wState &= ~state;
			    break;
			case _NET_WM_STATE_ADD:
			    wState |= state;
			    break;
			case _NET_WM_STATE_TOGGLE:
			    wState ^= state;
			    break;
			}
		    }
		}

		if (wState != w->state)
		{
		    w->state = wState;

		    recalcWindowType (w);

		    if (!w->attrib.override_redirect)
			updateWindowAttributes (w);

		    setWindowState (display, wState, w->id);
		}
	    }
	}
	else if (event->xclient.message_type == display->wmProtocolsAtom)
	{
	    if (event->xclient.data.l[0] == display->wmPingAtom)
	    {
		w = findWindowAtDisplay (display, event->xclient.data.l[2]);
		if (w)
		{
		    if (!w->alive)
		    {
			w->alive	    = TRUE;
			w->paint.saturation = w->saturation;
			w->paint.brightness = w->brightness;

			addWindowDamage (w);
		    }
		    w->lastPong = display->lastPing;
		}
	    }
	}
	else if (event->xclient.message_type == display->closeWindowAtom)
	{
	    w = findWindowAtDisplay (display, event->xclient.window);
	    if (w)
		closeWindow (w);
	}
	else if (event->xclient.message_type == display->desktopGeometryAtom)
	{
	    s = findScreenAtDisplay (display, event->xclient.window);
	    if (s)
	    {
		CompOptionValue value;

		value.i = event->xclient.data.l[0] / s->width;

		(*s->setScreenOption) (s, "size", &value);
	    }
	}
	else if (event->xclient.message_type == display->moveResizeWindowAtom)
	{
	    unsigned int   xwcm = 0;
	    XWindowChanges xwc;

	    if (event->xclient.data.l[0] & (1 << 7))
	    {
		xwcm |= CWX;
		xwc.x = event->xclient.data.l[1];
	    }

	    if (event->xclient.data.l[0] & (1 << 8))
	    {
		xwcm |= CWY;
		xwc.y = event->xclient.data.l[2];
	    }

	    if (event->xclient.data.l[0] & (1 << 9))
	    {
		xwcm |= CWWidth;
		xwc.width = event->xclient.data.l[3];
	    }

	    if (event->xclient.data.l[0] & (1 << 10))
	    {
		xwcm |= CWHeight;
		xwc.height = event->xclient.data.l[4];
	    }

	    /* TODO: gravity */

	    if (xwcm & (CWX | CWY))
	    {
		w = findWindowAtDisplay (display, event->xclient.window);
		if (w)
		{
		    if (xwcm & CWX)
			xwc.x += w->input.left;

		    if (xwcm & CWY)
			xwc.y += w->input.top;
		}
	    }

	    XConfigureWindow (display->display,
			      event->xclient.window,
			      xwcm, &xwc);
	}
	else if (event->xclient.message_type == display->restackWindowAtom)
	{
	    w = findWindowAtDisplay (display, event->xclient.window);
	    if (w)
	    {
		CompWindow *sibling;

		sibling = findWindowAtDisplay (display,
					       event->xclient.data.l[1]);
		if (sibling)
		{
		    /* TODO: other stack modes than Above and Below */
		    if (event->xclient.data.l[2] == Above)
			restackWindowAbove (w, sibling);
		    else if (event->xclient.data.l[2] == Below)
			restackWindowBelow (w, sibling);
		}
	    }
	}
	else if (event->xclient.message_type == display->wmChangeStateAtom)
	{
	    w = findWindowAtDisplay (display, event->xclient.window);
	    if (w && w->type & CompWindowTypeNormalMask)
	    {
		if (event->xclient.data.l[0] == IconicState)
		    minimizeWindow (w);
		else if (event->xclient.data.l[0] == NormalState)
		    unminimizeWindow (w);
	    }
	}
	else if (event->xclient.message_type == display->showingDesktopAtom)
	{
	    s = findScreenAtDisplay (display, event->xclient.window);
	    if (s)
	    {
		if (event->xclient.data.l[0])
		    enterShowDesktopMode (s);
		else
		    leaveShowDesktopMode (s);
	    }
	}
	break;
    case MappingNotify:
	updateModifierMappings (display);
	break;
    case MapRequest:
	w = findWindowAtDisplay (display, event->xmaprequest.window);
	if (w)
	{
	    if (w->minimized)
		unminimizeWindow (w);

	    if (w->screen->showingDesktopMask)
		leaveShowDesktopMode (w->screen);

	    if (!(w->state & CompWindowStateHiddenMask))
	    {
		XMapWindow (display->display, event->xmaprequest.window);

		updateWindowAttributes (w);

		if (!(w->type & (CompWindowTypeDesktopMask |
				 CompWindowTypeDockMask)) &&
		    (w->inputHint ||
		     (w->protocols & CompWindowProtocolTakeFocusMask)))
		    moveInputFocusToWindow (w);
	    }
	}
	else
	{
	    XMapWindow (display->display, event->xmaprequest.window);
	}
	break;
    case ConfigureRequest: {
	  unsigned int   xwcm;
	  XWindowChanges xwc;

	  xwcm = event->xconfigurerequest.value_mask &
	      (CWX | CWY | CWWidth | CWHeight | CWBorderWidth);

	  xwc.x		   = event->xconfigurerequest.x;
	  xwc.y		   = event->xconfigurerequest.y;
	  xwc.width	   = event->xconfigurerequest.width;
	  xwc.height	   = event->xconfigurerequest.height;
	  xwc.border_width = event->xconfigurerequest.border_width;

	  /* TODO: gravity */

	  if (xwcm & (CWX | CWY))
	  {
	      w = findWindowAtDisplay (display,
				       event->xconfigurerequest.window);
	      if (w)
	      {
		  xwc.x += w->input.left;
		  xwc.y += w->input.top;
	      }
	  }

	  XConfigureWindow (display->display,
			    event->xconfigurerequest.window,
			    xwcm, &xwc);
    } break;
    case CirculateRequest:
	break;
    case FocusIn:
	if (event->xfocus.window != display->activeWindow &&
	    event->xfocus.mode   != NotifyGrab)
	{
	    w = findWindowAtDisplay (display, event->xfocus.window);
	    if (w)
	    {
		XChangeProperty (display->display, w->screen->root,
				 display->winActiveAtom,
				 XA_WINDOW, 32, PropModeReplace,
				 (unsigned char *) &w->id, 1);
	    }
	}
	break;
    case EnterNotify:
	if (!display->screens->maxGrab		    &&
	    event->xcrossing.mode   != NotifyGrab   &&
	    event->xcrossing.detail != NotifyInferior)
	{
	    Bool raise;
	    int  delay;

	    raise = display->opt[COMP_DISPLAY_OPTION_AUTORAISE].value.b;
	    delay = display->opt[COMP_DISPLAY_OPTION_AUTORAISE_DELAY].value.i;

	    s = findScreenAtDisplay (display, event->xcrossing.root);
	    if (s)
		w = findTopLevelWindowAtScreen (s, event->xcrossing.window);
	    else
		w = NULL;

	    if (w && w->id != display->below)
	    {
		display->below = w->id;

		if (!display->opt[COMP_DISPLAY_OPTION_CLICK_TO_FOCUS].value.b)
		{
		    if (display->autoRaiseHandle &&
			display->autoRaiseWindow != w->id)
		    {
			compRemoveTimeout (display->autoRaiseHandle);
			display->autoRaiseHandle = 0;
		    }

		    if (w->type & ~(CompWindowTypeDockMask |
				    CompWindowTypeDesktopMask))
		    {
			moveInputFocusToWindow (w);

			if (raise)
			{
			    if (delay > 0)
			    {
				display->autoRaiseWindow = w->id;
				display->autoRaiseHandle =
				    compAddTimeout (delay, autoRaiseTimeout,
						    display);
			    }
			    else
				updateWindowAttributes (w);
			}
		    }
		}
	    }
	}
	break;
    case LeaveNotify:
	if (event->xcrossing.detail != NotifyInferior)
	{
	    if (event->xcrossing.window == display->below)
		display->below = None;
	}
	break;
    default:
	if (event->type == display->damageEvent + XDamageNotify)
	{
	    XDamageNotifyEvent *de = (XDamageNotifyEvent *) event;

	    if (lastDamagedWindow && de->drawable == lastDamagedWindow->id)
	    {
		w = lastDamagedWindow;
	    }
	    else
	    {
		w = findWindowAtDisplay (display, de->drawable);
		if (w)
		    lastDamagedWindow = w;
	    }

	    if (w)
	    {
		w->texture.oldMipmaps = TRUE;

		if (w->syncWait)
		{
		    if (w->nDamage == w->sizeDamage)
		    {
			if (w->damageRects)
			{
			    w->damageRects = realloc (w->damageRects,
						      (w->sizeDamage + 1) *
						      sizeof (XRectangle));
			    w->sizeDamage += 1;
			}
			else
			{
			    w->damageRects = malloc (sizeof (XRectangle));
			    w->sizeDamage  = 1;
			}
		    }

		    w->damageRects[w->nDamage].x      = de->area.x;
		    w->damageRects[w->nDamage].y      = de->area.y;
		    w->damageRects[w->nDamage].width  = de->area.width;
		    w->damageRects[w->nDamage].height = de->area.height;
		    w->nDamage++;
		}
		else
		{
		    handleWindowDamageRect (w,
					    de->area.x,
					    de->area.y,
					    de->area.width,
					    de->area.height);
		}
	    }
	}
	else if (display->shapeExtension &&
		 event->type == display->shapeEvent + ShapeNotify)
	{
	    w = findWindowAtDisplay (display, ((XShapeEvent *) event)->window);
	    if (w)
	    {
		if (w->mapNum)
		{
		    addWindowDamage (w);
		    updateWindowRegion (w);
		    addWindowDamage (w);
		}
	    }
	}
	else if (event->type == display->randrEvent + RRScreenChangeNotify)
	{
	    XRRScreenChangeNotifyEvent *rre;

	    rre = (XRRScreenChangeNotifyEvent *) event;

	    s = findScreenAtDisplay (display, rre->root);
	    if (s)
		detectRefreshRateOfScreen (s);
	}
	else if (event->type == display->syncEvent + XSyncAlarmNotify)
	{
	    XSyncAlarmNotifyEvent *sa;

	    sa = (XSyncAlarmNotifyEvent *) event;

	    w = NULL;

	    for (s = display->screens; s; s = s->next)
		for (w = s->windows; w; w = w->next)
		    if (w->syncAlarm == sa->alarm)
			break;

	    if (w)
		handleSyncAlarm (w);
	}
	break;
    }
}
