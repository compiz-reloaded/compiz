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
#include <X11/extensions/Xfixes.h>

#include <compiz.h>

static Window xdndWindow = None;
static Window edgeWindow = None;

static void
handleWindowDamageRect (CompWindow *w,
			int	   x,
			int	   y,
			int	   width,
			int	   height)
{
    REGION region;
    Bool   initial = FALSE;

    if (!w->redirected)
	return;

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

	damageScreenRegion (w->screen, &region);
    }

    if (!w->attrib.override_redirect)
	w->placed = TRUE;

    if (initial)
	damageWindowOutputExtents (w);
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

	w->syncWait = FALSE;

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
	}
	else
	{
	    /* resizeWindow failing means that there is another pending
	       resize and we must send a new sync request to the client */
	    sendSyncRequest (w);
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
	    updateWindowAttributes (w, FALSE);
    }

    return FALSE;
}

#define REAL_MOD_MASK (ShiftMask | ControlMask | Mod1Mask | \
		       Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)

static Bool
isCallBackBinding (CompOption	   *option,
		   CompBindingType type,
		   CompActionState state)
{
    if (option->type != CompOptionTypeAction)
	return FALSE;

    if (!(option->value.action.type & type))
	return FALSE;

    if (!(option->value.action.state & state))
	return FALSE;

    return TRUE;
}

static Bool
isInitiateBinding (CompOption	   *option,
		   CompBindingType type,
		   CompActionState state,
		   CompAction	   **action)
{
    if (!isCallBackBinding (option, type, state))
	return FALSE;

    if (!option->value.action.initiate)
	return FALSE;

    *action = &option->value.action;

    return TRUE;
}

static Bool
isTerminateBinding (CompOption	    *option,
		    CompBindingType type,
		    CompActionState state,
		    CompAction      **action)
{
    if (!isCallBackBinding (option, type, state))
	return FALSE;

    if (!option->value.action.terminate)
	return FALSE;

    *action = &option->value.action;

    return TRUE;
}

static Bool
triggerButtonPressBindings (CompDisplay *d,
			    CompOption  *option,
			    int		nOption,
			    XEvent	*event,
			    CompOption  *argument,
			    int		nArgument)
{
    CompActionState state = CompActionStateInitButton;
    CompAction	    *action;
    unsigned int    modMask = REAL_MOD_MASK & ~d->ignoredModMask;
    unsigned int    bindMods;
    unsigned int    edge = 0;

    if (edgeWindow)
    {
	CompScreen   *s;
	unsigned int i;

	s = findScreenAtDisplay (d, event->xbutton.root);
	if (!s)
	    return FALSE;

	if (event->xbutton.window != edgeWindow)
	{
	    if (!s->maxGrab || event->xbutton.window != s->root)
		return FALSE;
	}

	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	{
	    if (edgeWindow == s->screenEdge[i].id)
	    {
		edge = 1 << i;
		break;
	    }
	}
    }

    while (nOption--)
    {
	if (isInitiateBinding (option, CompBindingTypeButton, state, &action))
	{
	    if (action->button.button == event->xbutton.button)
	    {
		bindMods = virtualToRealModMask (d, action->button.modifiers);

		if ((bindMods & modMask) == (event->xbutton.state & modMask))
		    if ((*action->initiate) (d, action, state,
					     argument, nArgument))
			return TRUE;
	    }
	}

	if (edge)
	{
	    if (isInitiateBinding (option, CompBindingTypeEdgeButton, state,
				   &action))
	    {
		if (action->edgeMask & edge)
		{
		    if (action->edgeButton == event->xbutton.button)
			if ((*action->initiate) (d, action,
						 CompActionStateInitEdge,
						 argument, nArgument))
			    return TRUE;
		}
	    }
	}

	option++;
    }

    return FALSE;
}

static Bool
triggerButtonReleaseBindings (CompDisplay *d,
			      CompOption  *option,
			      int	  nOption,
			      XEvent	  *event,
			      CompOption  *argument,
			      int	  nArgument)
{
    CompActionState state = CompActionStateTermButton;
    CompAction	    *action;

    while (nOption--)
    {
	if (isTerminateBinding (option, CompBindingTypeButton, state, &action))
	{
	    if (action->button.button == event->xbutton.button)
	    {
		if ((*action->terminate) (d, action, state,
					  argument, nArgument))
		    return TRUE;
	    }
	}

	option++;
    }

    return FALSE;
}

static Bool
triggerKeyPressBindings (CompDisplay *d,
			 CompOption  *option,
			 int	     nOption,
			 XEvent	     *event,
			 CompOption  *argument,
			 int	     nArgument)
{
    CompActionState state = 0;
    CompAction	    *action;
    unsigned int    modMask = REAL_MOD_MASK & ~d->ignoredModMask;
    unsigned int    bindMods;

    if (event->xkey.keycode == d->escapeKeyCode)
	state = CompActionStateCancel;
    else if (event->xkey.keycode == d->returnKeyCode)
	state = CompActionStateCommit;

    if (state)
    {
	CompOption *o = option;
	int	   n = nOption;

	while (n--)
	{
	    if (o->type == CompOptionTypeAction)
	    {
		if (o->value.action.terminate)
		    (*o->value.action.terminate) (d, &o->value.action,
						  state, NULL, 0);
	    }

	    o++;
	}

	if (state == CompActionStateCancel)
	    return FALSE;
    }

    state = CompActionStateInitKey;
    while (nOption--)
    {
	if (isInitiateBinding (option, CompBindingTypeKey, state, &action))
	{
	    bindMods = virtualToRealModMask (d, action->key.modifiers);

	    if (action->key.keycode == event->xkey.keycode)
	    {
		if ((bindMods & modMask) == (event->xkey.state & modMask))
		    if ((*action->initiate) (d, action, state,
					     argument, nArgument))
			break;
	    }
	    else if (!d->xkbEvent && action->key.keycode == 0)
	    {
		if (bindMods == (event->xkey.state & modMask))
		    if ((*action->initiate) (d, action, state,
					     argument, nArgument))
			return TRUE;
	    }
	}

	option++;
    }

    return FALSE;
}

static Bool
triggerKeyReleaseBindings (CompDisplay *d,
			   CompOption  *option,
			   int	       nOption,
			   XEvent      *event,
			   CompOption  *argument,
			   int	       nArgument)
{
    if (!d->xkbEvent)
    {
	CompActionState state = CompActionStateTermKey;
	CompAction	*action;
	unsigned int	modMask = REAL_MOD_MASK & ~d->ignoredModMask;
	unsigned int	bindMods;
	unsigned int	mods;

	mods = keycodeToModifiers (d, event->xkey.keycode);
	if (mods == 0)
	    return FALSE;

	while (nOption--)
	{
	    if (isTerminateBinding (option, CompBindingTypeKey, state, &action))
	    {
		bindMods = virtualToRealModMask (d, action->key.modifiers);

		if ((mods & modMask & bindMods) != bindMods)
		{
		    if ((*action->terminate) (d, action, state,
					      argument, nArgument))
			return TRUE;
		}
	    }

	    option++;
	}
    }

    return FALSE;
}

static Bool
triggerStateNotifyBindings (CompDisplay		*d,
			    CompOption		*option,
			    int			nOption,
			    XkbStateNotifyEvent *event,
			    CompOption		*argument,
			    int			nArgument)
{
    CompActionState state;
    CompAction      *action;
    unsigned int    modMask = REAL_MOD_MASK & ~d->ignoredModMask;
    unsigned int    bindMods;

    if (event->event_type == KeyPress)
    {
	state = CompActionStateInitKey;

	while (nOption--)
	{
	    if (isInitiateBinding (option, CompBindingTypeKey, state, &action))
	    {
		if (action->key.keycode == 0)
		{
		    bindMods = virtualToRealModMask (d, action->key.modifiers);

		    if ((event->mods & modMask & bindMods) == bindMods)
		    {
			if ((*action->initiate) (d, action, state,
						 argument, nArgument))
			    return TRUE;
		    }
		}
	    }

	    option++;
	}
    }
    else
    {
	state = CompActionStateTermKey;

	while (nOption--)
	{
	    if (isTerminateBinding (option, CompBindingTypeKey, state, &action))
	    {
		bindMods = virtualToRealModMask (d, action->key.modifiers);

		if ((event->mods & modMask & bindMods) != bindMods)
		{
		    if ((*action->terminate) (d, action, state,
					      argument, nArgument))
			return TRUE;
		}
	    }

	    option++;
	}
    }

    return FALSE;
}

static Bool
isBellAction (CompOption      *option,
	      CompActionState state,
	      CompAction      **action)
{
    if (option->type != CompOptionTypeAction)
	return FALSE;

    if (!option->value.action.bell)
	return FALSE;

    if (!(option->value.action.state & state))
	return FALSE;

    if (!option->value.action.initiate)
	return FALSE;

    *action = &option->value.action;

    return TRUE;
}

static Bool
triggerBellNotifyBindings (CompDisplay *d,
			   CompOption  *option,
			   int	       nOption,
			   CompOption  *argument,
			   int	       nArgument)
{
    CompActionState state = CompActionStateInitBell;
    CompAction      *action;

    while (nOption--)
    {
	if (isBellAction (option, state, &action))
	{
	    if ((*action->initiate) (d, action, state, argument, nArgument))
		return TRUE;
	}

	option++;
    }

    return FALSE;
}

static Bool
isEdgeAction (CompOption      *option,
	      CompActionState state,
	      unsigned int    edge)
{
    if (option->type != CompOptionTypeAction)
	return FALSE;

    if (!(option->value.action.edgeMask & edge))
	return FALSE;

    if (!(option->value.action.state & state))
	return FALSE;

    return TRUE;
}

static Bool
isEdgeEnterAction (CompOption      *option,
		   CompActionState state,
		   unsigned int    edge,
		   CompAction      **action)
{
    if (!isEdgeAction (option, state, edge))
	return FALSE;

    if (option->value.action.type & CompBindingTypeEdgeButton)
	return FALSE;

    if (!option->value.action.initiate)
	return FALSE;

    *action = &option->value.action;

    return TRUE;
}

static Bool
isEdgeLeaveAction (CompOption      *option,
		   CompActionState state,
		   unsigned int    edge,
		   CompAction      **action)
{
    if (!isEdgeAction (option, state, edge))
	return FALSE;

    if (!option->value.action.terminate)
	return FALSE;

    *action = &option->value.action;

    return TRUE;
}

static Bool
triggerEdgeEnterBindings (CompDisplay	  *d,
			  CompOption	  *option,
			  int		  nOption,
			  CompActionState state,
			  unsigned int	  edge,
			  CompOption	  *argument,
			  int		  nArgument)
{
    CompAction *action;

    while (nOption--)
    {
	if (isEdgeEnterAction (option, state, edge, &action))
	{
	    if ((*action->initiate) (d, action, state, argument, nArgument))
		return TRUE;
	}

	option++;
    }

    return FALSE;
}

static Bool
triggerEdgeLeaveBindings (CompDisplay	  *d,
			  CompOption	  *option,
			  int		  nOption,
			  CompActionState state,
			  unsigned int	  edge,
			  CompOption	  *argument,
			  int		  nArgument)
{
    CompAction *action;

    while (nOption--)
    {
	if (isEdgeLeaveAction (option, state, edge, &action))
	{
	    if ((*action->terminate) (d, action, state, argument, nArgument))
		return TRUE;
	}

	option++;
    }

    return FALSE;
}

static Bool
handleActionEvent (CompDisplay *d,
		   XEvent      *event)
{
    CompOption *option;
    int	       nOption;
    CompPlugin *p;
    CompOption o[7];

    o[0].type = CompOptionTypeInt;
    o[0].name = "window";

    o[1].type = CompOptionTypeInt;
    o[1].name = "modifiers";

    o[2].type = CompOptionTypeInt;
    o[2].name = "x";

    o[3].type = CompOptionTypeInt;
    o[3].name = "y";

    o[4].type = CompOptionTypeInt;
    o[4].name = "root";

    switch (event->type) {
    case ButtonPress:
	o[0].value.i = event->xbutton.window;
	o[1].value.i = event->xbutton.state;
	o[2].value.i = event->xbutton.x_root;
	o[3].value.i = event->xbutton.y_root;
	o[4].value.i = event->xbutton.root;

	o[5].type    = CompOptionTypeInt;
	o[5].name    = "button";
	o[5].value.i = event->xbutton.button;

	o[6].type    = CompOptionTypeInt;
	o[6].name    = "time";
	o[6].value.i = event->xbutton.time;

	for (p = getPlugins (); p; p = p->next)
	{
	    if (p->vTable->getDisplayOptions)
	    {
		option = (*p->vTable->getDisplayOptions) (d, &nOption);
		if (triggerButtonPressBindings (d, option, nOption, event,
						o, 7))
		    return TRUE;
	    }
	}

	option = compGetDisplayOptions (d, &nOption);
	if (triggerButtonPressBindings (d, option, nOption, event, o, 7))
	    return TRUE;

	break;
    case ButtonRelease:
	o[0].value.i = event->xbutton.window;
	o[1].value.i = event->xbutton.state;
	o[2].value.i = event->xbutton.x_root;
	o[3].value.i = event->xbutton.y_root;
	o[4].value.i = event->xbutton.root;

	o[5].type    = CompOptionTypeInt;
	o[5].name    = "button";
	o[5].value.i = event->xbutton.button;

	o[6].type    = CompOptionTypeInt;
	o[6].name    = "time";
	o[6].value.i = event->xbutton.time;

	for (p = getPlugins (); p; p = p->next)
	{
	    if (p->vTable->getDisplayOptions)
	    {
		option = (*p->vTable->getDisplayOptions) (d, &nOption);
		if (triggerButtonReleaseBindings (d, option, nOption, event,
						  o, 7))
		    return TRUE;
	    }
	}

	option = compGetDisplayOptions (d, &nOption);
	if (triggerButtonReleaseBindings (d, option, nOption, event, o, 7))
	    return TRUE;

	break;
    case KeyPress:
	o[0].value.i = d->activeWindow;
	o[1].value.i = event->xkey.state;
	o[2].value.i = event->xkey.x_root;
	o[3].value.i = event->xkey.y_root;
	o[4].value.i = event->xkey.root;

	o[5].type    = CompOptionTypeInt;
	o[5].name    = "keycode";
	o[5].value.i = event->xkey.keycode;

	o[6].type    = CompOptionTypeInt;
	o[6].name    = "time";
	o[6].value.i = event->xkey.time;

	for (p = getPlugins (); p; p = p->next)
	{
	    if (p->vTable->getDisplayOptions)
	    {
		option = (*p->vTable->getDisplayOptions) (d, &nOption);
		if (triggerKeyPressBindings (d, option, nOption, event, o, 7))
		    return TRUE;
	    }
	}

	option = compGetDisplayOptions (d, &nOption);
	if (triggerKeyPressBindings (d, option, nOption, event, o, 7))
	    return TRUE;

	break;
    case KeyRelease:
	o[0].value.i = d->activeWindow;
	o[1].value.i = event->xkey.state;
	o[2].value.i = event->xkey.x_root;
	o[3].value.i = event->xkey.y_root;
	o[4].value.i = event->xkey.root;

	o[5].type    = CompOptionTypeInt;
	o[5].name    = "keycode";
	o[5].value.i = event->xkey.keycode;

	o[6].type    = CompOptionTypeInt;
	o[6].name    = "time";
	o[6].value.i = event->xkey.time;

	for (p = getPlugins (); p; p = p->next)
	{
	    if (p->vTable->getDisplayOptions)
	    {
		option = (*p->vTable->getDisplayOptions) (d, &nOption);
		if (triggerKeyReleaseBindings (d, option, nOption, event, o, 7))
		    return TRUE;
	    }
	}

	option = compGetDisplayOptions (d, &nOption);
	if (triggerKeyReleaseBindings (d, option, nOption, event, o, 7))
	    return TRUE;

	break;
    case EnterNotify:
	if (event->xcrossing.mode   != NotifyGrab   &&
	    event->xcrossing.mode   != NotifyUngrab &&
	    event->xcrossing.detail != NotifyInferior)
	{
	    CompScreen	    *s;
	    unsigned int    edge, i;
	    CompActionState state;

	    s = findScreenAtDisplay (d, event->xcrossing.root);
	    if (!s)
		return FALSE;

	    if (edgeWindow && edgeWindow != event->xcrossing.window)
	    {
		state = CompActionStateTermEdge;
		edge  = 0;

		for (i = 0; i < SCREEN_EDGE_NUM; i++)
		{
		    if (edgeWindow == s->screenEdge[i].id)
		    {
			edge = 1 << i;
			break;
		    }
		}

		edgeWindow = None;

		o[0].value.i = d->activeWindow;
		o[1].value.i = event->xcrossing.state;
		o[2].value.i = event->xcrossing.x_root;
		o[3].value.i = event->xcrossing.y_root;
		o[4].value.i = event->xcrossing.root;

		o[5].type    = CompOptionTypeInt;
		o[5].name    = "time";
		o[5].value.i = event->xcrossing.time;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (p->vTable->getDisplayOptions)
		    {
			option = (*p->vTable->getDisplayOptions) (d, &nOption);
			if (triggerEdgeLeaveBindings (d, option, nOption, state,
						      edge, o, 6))
			    return TRUE;
		    }
		}

		option = compGetDisplayOptions (d, &nOption);
		if (triggerEdgeLeaveBindings (d, option, nOption, state,
					      edge, o, 6))
		    return TRUE;
	    }

	    edge = 0;

	    for (i = 0; i < SCREEN_EDGE_NUM; i++)
	    {
		if (event->xcrossing.window == s->screenEdge[i].id)
		{
		    edge = 1 << i;
		    break;
		}
	    }

	    if (edge)
	    {
		state = CompActionStateInitEdge;

		edgeWindow = event->xcrossing.window;

		o[0].value.i = d->activeWindow;
		o[1].value.i = event->xcrossing.state;
		o[2].value.i = event->xcrossing.x_root;
		o[3].value.i = event->xcrossing.y_root;
		o[4].value.i = event->xcrossing.root;

		o[5].type    = CompOptionTypeInt;
		o[5].name    = "time";
		o[5].value.i = event->xcrossing.time;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (p->vTable->getDisplayOptions)
		    {
			option = (*p->vTable->getDisplayOptions) (d, &nOption);
			if (triggerEdgeEnterBindings (d, option, nOption, state,
						      edge, o, 6))
			    return TRUE;
		    }
		}

		option = compGetDisplayOptions (d, &nOption);
		if (triggerEdgeEnterBindings (d, option, nOption, state,
					      edge, o, 6))
		    return TRUE;
	    }
	} break;
    case ClientMessage:
	if (event->xclient.message_type == d->xdndEnterAtom)
	{
	    xdndWindow = event->xclient.window;
	}
	else if (event->xclient.message_type == d->xdndLeaveAtom)
	{
	    unsigned int    edge = 0;
	    CompActionState state;
	    Window	    root = None;

	    if (!xdndWindow)
	    {
		CompWindow *w;

		w = findWindowAtDisplay (d, event->xclient.window);
		if (w)
		{
		    CompScreen   *s = w->screen;
		    unsigned int i;

		    for (i = 0; i < SCREEN_EDGE_NUM; i++)
		    {
			if (event->xclient.window == s->screenEdge[i].id)
			{
			    edge = 1 << i;
			    root = s->root;
			    break;
			}
		    }
		}
	    }

	    if (edge)
	    {
		state = CompActionStateTermEdgeDnd;

		o[0].value.i = d->activeWindow;
		o[1].value.i = 0; /* fixme */
		o[2].value.i = 0; /* fixme */
		o[3].value.i = 0; /* fixme */
		o[4].value.i = root;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (p->vTable->getDisplayOptions)
		    {
			option = (*p->vTable->getDisplayOptions) (d, &nOption);
			if (triggerEdgeLeaveBindings (d, option, nOption, state,
						      edge, o, 5))
			    return TRUE;
		    }
		}

		option = compGetDisplayOptions (d, &nOption);
		if (triggerEdgeLeaveBindings (d, option, nOption, state,
					      edge, o, 5))
		    return TRUE;
	    }
	}
	else if (event->xclient.message_type == d->xdndPositionAtom)
	{
	    unsigned int    edge = 0;
	    CompActionState state;
	    Window	    root = None;

	    if (xdndWindow == event->xclient.window)
	    {
		CompWindow *w;

		w = findWindowAtDisplay (d, event->xclient.window);
		if (w)
		{
		    CompScreen   *s = w->screen;
		    unsigned int i;

		    for (i = 0; i < SCREEN_EDGE_NUM; i++)
		    {
			if (xdndWindow == s->screenEdge[i].id)
			{
			    edge = 1 << i;
			    root = s->root;
			    break;
			}
		    }
		}
	    }

	    if (edge)
	    {
		state = CompActionStateInitEdgeDnd;

		o[0].value.i = d->activeWindow;
		o[1].value.i = 0; /* fixme */
		o[2].value.i = event->xclient.data.l[2] >> 16;
		o[3].value.i = event->xclient.data.l[2] & 0xffff;
		o[4].value.i = root;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (p->vTable->getDisplayOptions)
		    {
			option = (*p->vTable->getDisplayOptions) (d, &nOption);
			if (triggerEdgeEnterBindings (d, option, nOption, state,
						      edge, o, 5))
			    return TRUE;
		    }
		}

		option = compGetDisplayOptions (d, &nOption);
		if (triggerEdgeEnterBindings (d, option, nOption, state,
					      edge, o, 5))
		    return TRUE;
	    }

	    xdndWindow = None;
	}
	break;
    default:
	if (event->type == d->fixesEvent + XFixesCursorNotify)
	{
	    /*
	    XFixesCursorNotifyEvent *ce = (XFixesCursorNotifyEvent *) event;
	    CompCursor		    *cursor;

	    cursor = findCursorAtDisplay (d);
	    if (cursor)
		updateCursor (cursor, ce->x, ce->y, ce->cursor_serial);
	    */
	}
	else if (event->type == d->xkbEvent)
	{
	    XkbAnyEvent *xkbEvent = (XkbAnyEvent *) event;

	    if (xkbEvent->xkb_type == XkbStateNotify)
	    {
		XkbStateNotifyEvent *stateEvent = (XkbStateNotifyEvent *) event;

		option = compGetDisplayOptions (d, &nOption);

		o[0].value.i = d->activeWindow;
		o[1].value.i = stateEvent->mods;

		o[2].type    = CompOptionTypeInt;
		o[2].name    = "time";
		o[2].value.i = xkbEvent->time;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (p->vTable->getDisplayOptions)
		    {
			option = (*p->vTable->getDisplayOptions) (d, &nOption);
			if (triggerStateNotifyBindings (d, option, nOption,
							stateEvent, o, 3))
			    return TRUE;
		    }
		}

		option = compGetDisplayOptions (d, &nOption);
		if (triggerStateNotifyBindings (d, option, nOption, stateEvent,
						o, 3))
		    return TRUE;
	    }
	    else if (xkbEvent->xkb_type == XkbBellNotify)
	    {
		option = compGetDisplayOptions (d, &nOption);

		o[0].value.i = d->activeWindow;

		o[1].type    = CompOptionTypeInt;
		o[1].name    = "time";
		o[1].value.i = xkbEvent->time;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (p->vTable->getDisplayOptions)
		    {
			option = (*p->vTable->getDisplayOptions) (d, &nOption);
			if (triggerBellNotifyBindings (d, option, nOption,
						       o, 2))
			    return TRUE;
		    }
		}

		option = compGetDisplayOptions (d, &nOption);
		if (triggerBellNotifyBindings (d, option, nOption, o, 2))
		    return TRUE;
	    }
	}
	break;
    }

    return FALSE;
}

void
handleCompizEvent (CompDisplay *d,
		   char        *pluginName,
		   char        *eventName,
		   CompOption  *option,
		   int         nOption)
{
}

void
handleEvent (CompDisplay *d,
	     XEvent      *event)
{
    CompScreen *s;
    CompWindow *w;

    switch (event->type) {
    case ButtonPress:
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s)
	    setCurrentOutput (s, outputDeviceForPoint (s,
						       event->xbutton.x_root,
						       event->xbutton.y_root));
	break;
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	    setCurrentOutput (s, outputDeviceForPoint (s,
						       event->xmotion.x_root,
						       event->xmotion.y_root));
	break;
    case KeyPress:
	w = findWindowAtDisplay (d, d->activeWindow);
	if (w)
	    setCurrentOutput (w->screen, outputDeviceForWindow (w));
    default:
	break;
    }

    if (handleActionEvent (d, event))
    {
	if (!d->screens->maxGrab)
	    XAllowEvents (d->display, AsyncPointer, event->xbutton.time);

	return;
    }

    switch (event->type) {
    case Expose:
	s = findScreenAtDisplay (d, event->xexpose.window);
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
	handleSelectionRequest (d, event);
	break;
    case SelectionClear:
	handleSelectionClear (d, event);
	break;
    case ConfigureNotify:
	w = findWindowAtDisplay (d, event->xconfigure.window);
	if (w)
	{
	    configureWindow (w, &event->xconfigure);
	}
	else
	{
	    s = findScreenAtDisplay (d, event->xconfigure.window);
	    if (s)
		configureScreen (s, &event->xconfigure);
	}
	break;
    case CreateNotify:
	s = findScreenAtDisplay (d, event->xcreatewindow.parent);
	if (s)
	{
	    /* The first time some client asks for the composite
	     * overlay window, the X server creates it, which causes
	     * an errorneous CreateNotify event.  We catch it and
	     * ignore it. */
	    if (s->overlay != event->xcreatewindow.window)
		addWindow (s, event->xcreatewindow.window, getTopWindow (s));
	}
	break;
    case DestroyNotify:
	w = findWindowAtDisplay (d, event->xdestroywindow.window);
	if (w)
	{
	    moveInputFocusToOtherWindow (w);
	    destroyWindow (w);
	}
	break;
    case MapNotify:
	w = findWindowAtDisplay (d, event->xmap.window);
	if (w)
	{
	    /* been shaded */
	    if (w->height == 0)
	    {
		if (w->id == d->activeWindow)
		    moveInputFocusToWindow (w);
	    }

	    mapWindow (w);
	}
	break;
    case UnmapNotify:
	w = findWindowAtDisplay (d, event->xunmap.window);
	if (w)
	{
	    /* Normal -> Iconic */
	    if (w->pendingUnmaps)
	    {
		setWmState (d, IconicState, w->id);
		w->pendingUnmaps--;
	    }
	    else /* X -> Withdrawn */
	    {
		/* Iconic -> Withdrawn */
		if (w->state & CompWindowStateHiddenMask)
		{
		    w->minimized = FALSE;
		    w->state &= ~CompWindowStateHiddenMask;

		    (*w->screen->windowStateChangeNotify) (w);

		    setWindowState (d, w->state, w->id);
		    updateClientListForScreen (w->screen);
		}

		if (!w->attrib.override_redirect)
		    setWmState (d, WithdrawnState, w->id);

		w->placed  = FALSE;
		w->managed = FALSE;
	    }

	    unmapWindow (w);

	    if (!w->shaded)
		moveInputFocusToOtherWindow (w);
	}
	break;
    case ReparentNotify:
	w = findWindowAtDisplay (d, event->xreparent.window);
	s = findScreenAtDisplay (d, event->xreparent.parent);
	if (s && !w)
	{
	    addWindow (s, event->xreparent.window, getTopWindow (s));
	}
	else if (w)
	{
	    /* This is the only case where a window is removed but not
	       destroyed. We must remove our event mask and all passive
	       grabs. */
	    XSelectInput (d->display, w->id, NoEventMask);
	    XShapeSelectInput (d->display, w->id, NoEventMask);
	    XUngrabButton (d->display, AnyButton, AnyModifier, w->id);

	    moveInputFocusToOtherWindow (w);

	    destroyWindow (w);
	}
	break;
    case CirculateNotify:
	w = findWindowAtDisplay (d, event->xcirculate.window);
	if (w)
	    circulateWindow (w, &event->xcirculate);
	break;
    case ButtonPress:
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s)
	{
	    if (event->xbutton.button == Button1 ||
		event->xbutton.button == Button2 ||
		event->xbutton.button == Button3)
	    {
		w = findTopLevelWindowAtScreen (s, event->xbutton.window);
		if (w)
		{
		    if (d->opt[COMP_DISPLAY_OPTION_RAISE_ON_CLICK].value.b)
			updateWindowAttributes (w, TRUE);

		    if (!(w->type & CompWindowTypeDockMask))
			moveInputFocusToWindow (w);
		}
	    }

	    if (!s->maxGrab)
		XAllowEvents (d->display, ReplayPointer, event->xbutton.time);
	}
	break;
    case PropertyNotify:
	if (event->xproperty.atom == d->winActiveAtom)
	{
	    Window newActiveWindow;

	    newActiveWindow = getActiveWindow (d, event->xproperty.window);
	    if (newActiveWindow != d->activeWindow)
	    {
		d->activeWindow = newActiveWindow;

		s = findScreenAtDisplay (d, event->xproperty.window);
		if (s)
		{
		    w = findWindowAtDisplay (d, newActiveWindow);
		    if (w)
			w->activeNum = s->activeNum++;
		}
	    }
	}
	else if (event->xproperty.atom == d->winTypeAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		unsigned int type;

		type = getWindowType (d, w->id);

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
		    recalcWindowActions (w);

		    if (w->type & CompWindowTypeDesktopMask)
			w->paint.opacity = OPAQUE;

		    if (type & (CompWindowTypeDockMask |
				CompWindowTypeDesktopMask))
			setDesktopForWindow (w, 0xffffffff);

		    updateClientListForScreen (w->screen);

		    (*d->matchPropertyChanged) (d, w);
		}
	    }
	}
	else if (event->xproperty.atom == d->winStateAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w && !w->managed)
	    {
		unsigned int state;

		state = getWindowState (d, w->id);
		state = constrainWindowState (state, w->actions);

		if (state != w->state)
		{
		    w->state = state;

		    recalcWindowType (w);
		    recalcWindowActions (w);

		    if (w->type & CompWindowTypeDesktopMask)
			w->paint.opacity = OPAQUE;
		}
	    }
	}
	else if (event->xproperty.atom == XA_WM_NORMAL_HINTS)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		updateNormalHints (w);
		recalcWindowActions (w);
	    }
	}
	else if (event->xproperty.atom == XA_WM_HINTS)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		updateWmHints (w);
	}
	else if (event->xproperty.atom == XA_WM_TRANSIENT_FOR)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		updateTransientHint (w);
	}
	else if (event->xproperty.atom == d->wmClientLeaderAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		w->clientLeader = getClientLeader (w);
	}
	else if (event->xproperty.atom == d->winOpacityAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w && (w->type & CompWindowTypeDesktopMask) == 0)
	    {
		GLushort opacity;

		opacity = getWindowProp32 (d, w->id,
					   d->winOpacityAtom,
					   OPAQUE);
		if (opacity != w->opacity)
		{
		    w->opacity = w->paint.opacity = opacity;
		    addWindowDamage (w);
		}
	    }
	}
	else if (event->xproperty.atom == d->winBrightnessAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		GLushort brightness;

		brightness = getWindowProp32 (d, w->id,
					      d->winBrightnessAtom,
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
	else if (event->xproperty.atom == d->winSaturationAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w && w->screen->canDoSaturated)
	    {
		GLushort saturation;

		saturation = getWindowProp32 (d, w->id,
					      d->winSaturationAtom,
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
	else if (event->xproperty.atom == d->xBackgroundAtom[0] ||
		 event->xproperty.atom == d->xBackgroundAtom[1])
	{
	    s = findScreenAtDisplay (d, event->xproperty.window);
	    if (s)
	    {
		finiTexture (s, &s->backgroundTexture);
		initTexture (s, &s->backgroundTexture);

		if (s->backgroundLoaded)
		{
		    s->backgroundLoaded = FALSE;
		    damageScreen (s);
		}
	    }
	}
	else if (event->xproperty.atom == d->wmStrutAtom ||
		 event->xproperty.atom == d->wmStrutPartialAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		if (updateWindowStruts (w))
		    updateWorkareaForScreen (w->screen);
	    }
	}
	else if (event->xproperty.atom == d->mwmHintsAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		getMwmHints (d, w->id, &w->mwmFunc, &w->mwmDecor);

		recalcWindowActions (w);
	    }
	}
	else if (event->xproperty.atom == d->wmProtocolsAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		w->protocols = getProtocols (d, w->id);
	}
	else if (event->xproperty.atom == d->wmIconAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		freeWindowIcons (w);
	}
	else if (event->xproperty.atom == d->startupIdAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		if (w->startupId)
		    free (w->startupId);

		w->startupId = getStartupId (w);
	    }
	}
	else if (event->xproperty.atom == XA_WM_CLASS)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		updateWindowClassHints (w);
	}
	break;
    case MotionNotify:
	break;
    case ClientMessage:
	if (event->xclient.message_type == d->winActiveAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
		activateWindow (w);
	}
	else if (event->xclient.message_type == d->winOpacityAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w && (w->type & CompWindowTypeDesktopMask) == 0)
	    {
		GLushort opacity;

		opacity = event->xclient.data.l[0] >> 16;
		if (opacity != w->paint.opacity)
		{
		    w->paint.opacity = opacity;

		    setWindowProp32 (d, w->id, d->winOpacityAtom,
				     w->paint.opacity);
		    addWindowDamage (w);
		}
	    }
	}
	else if (event->xclient.message_type == d->winBrightnessAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		GLushort brightness;

		brightness = event->xclient.data.l[0] >> 16;
		if (brightness != w->paint.brightness)
		{
		    w->paint.brightness = brightness;

		    setWindowProp32 (d, w->id, d->winBrightnessAtom,
				     w->paint.brightness);
		    addWindowDamage (w);
		}
	    }
	}
	else if (event->xclient.message_type == d->winSaturationAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w && w->screen->canDoSaturated)
	    {
		GLushort saturation;

		saturation = event->xclient.data.l[0] >> 16;
		if (saturation != w->saturation)
		{
		    w->saturation = saturation;

		    setWindowProp32 (d, w->id, d->winSaturationAtom,
				     w->saturation);

		    if (w->alive)
		    {
			w->paint.saturation = w->saturation;
			addWindowDamage (w);
		    }
		}
	    }
	}
	else if (event->xclient.message_type == d->winStateAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		unsigned long wState, state;
		int	      i;

		wState = w->state;

		for (i = 1; i < 3; i++)
		{
		    state = windowStateMask (d, event->xclient.data.l[i]);
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

		wState = constrainWindowState (wState, w->actions);

		if (wState != w->state)
		{
		    w->state = wState;

		    recalcWindowType (w);
		    recalcWindowActions (w);

		    (*w->screen->windowStateChangeNotify) (w);

		    updateWindowAttributes (w, FALSE);

		    setWindowState (d, w->state, w->id);
		}
	    }
	}
	else if (event->xclient.message_type == d->wmProtocolsAtom)
	{
	    if (event->xclient.data.l[0] == d->wmPingAtom)
	    {
		w = findWindowAtDisplay (d, event->xclient.data.l[2]);
		if (w)
		{
		    if (!w->alive)
		    {
			w->alive	    = TRUE;
			w->paint.saturation = w->saturation;
			w->paint.brightness = w->brightness;

			if (w->lastCloseRequestTime)
			{
			    toolkitAction (w->screen,
					   d->toolkitActionForceQuitDialogAtom,
					   w->lastCloseRequestTime,
					   w->id,
					   FALSE,
					   0,
					   0);

			    w->lastCloseRequestTime = 0;
			}

			addWindowDamage (w);
		    }
		    w->lastPong = d->lastPing;
		}
	    }
	}
	else if (event->xclient.message_type == d->closeWindowAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
		closeWindow (w, event->xclient.data.l[0]);
	}
	else if (event->xclient.message_type == d->desktopGeometryAtom)
	{
	    s = findScreenAtDisplay (d, event->xclient.window);
	    if (s)
	    {
		CompOptionValue value;

		value.i = event->xclient.data.l[0] / s->width;

		(*s->setScreenOption) (s, "hsize", &value);

		value.i = event->xclient.data.l[1] / s->height;

		(*s->setScreenOption) (s, "vsize", &value);
	    }
	}
	else if (event->xclient.message_type == d->moveResizeWindowAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		unsigned int   xwcm = 0;
		XWindowChanges xwc;
		int            gravity;

		memset (&xwc, 0, sizeof (xwc));

		if (event->xclient.data.l[0] & (1 << 8))
		{
		    xwcm |= CWX;
		    xwc.x = event->xclient.data.l[1];
		}

		if (event->xclient.data.l[0] & (1 << 9))
		{
		    xwcm |= CWY;
		    xwc.y = event->xclient.data.l[2];
		}

		if (event->xclient.data.l[0] & (1 << 10))
		{
		    xwcm |= CWWidth;
		    xwc.width = event->xclient.data.l[3];
		}

		if (event->xclient.data.l[0] & (1 << 11))
		{
		    xwcm |= CWHeight;
		    xwc.height = event->xclient.data.l[4];
		}

		gravity = event->xclient.data.l[0] & 0xFF;

		moveResizeWindow (w, &xwc, xwcm, gravity);
	    }
	}
	else if (event->xclient.message_type == d->restackWindowAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		/* TODO: other stack modes than Above and Below */
		if (event->xclient.data.l[1])
		{
		    CompWindow *sibling;

		    sibling = findWindowAtDisplay (d, event->xclient.data.l[1]);
		    if (sibling)
		    {
			if (event->xclient.data.l[2] == Above)
			    restackWindowAbove (w, sibling);
			else if (event->xclient.data.l[2] == Below)
			    restackWindowBelow (w, sibling);
		    }
		}
		else
		{
		    if (event->xclient.data.l[2] == Above)
			raiseWindow (w);
		    else if (event->xclient.data.l[2] == Below)
			lowerWindow (w);
		}
	    }
	}
	else if (event->xclient.message_type == d->wmChangeStateAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w && w->type & CompWindowTypeNormalMask)
	    {
		if (event->xclient.data.l[0] == IconicState)
		    minimizeWindow (w);
		else if (event->xclient.data.l[0] == NormalState)
		    unminimizeWindow (w);
	    }
	}
	else if (event->xclient.message_type == d->showingDesktopAtom)
	{
	    for (s = d->screens; s; s = s->next)
	    {
		if (event->xclient.window == s->root ||
		    event->xclient.window == None)
		{
		    if (event->xclient.data.l[0])
			enterShowDesktopMode (s);
		    else
			leaveShowDesktopMode (s, NULL);
		}
	    }
	}
	else if (event->xclient.message_type == d->numberOfDesktopsAtom)
	{
	    s = findScreenAtDisplay (d, event->xclient.window);
	    if (s)
	    {
		CompOptionValue value;

		value.i = event->xclient.data.l[0];

		(*s->setScreenOption) (s, "number_of_desktops", &value);
	    }
	}
	else if (event->xclient.message_type == d->currentDesktopAtom)
	{
	    s = findScreenAtDisplay (d, event->xclient.window);
	    if (s)
		setCurrentDesktop (s, event->xclient.data.l[0]);
	}
	else if (event->xclient.message_type == d->winDesktopAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
		setDesktopForWindow (w, event->xclient.data.l[0]);
	}
	break;
    case MappingNotify:
	updateModifierMappings (d);
	break;
    case MapRequest:
	w = findWindowAtDisplay (d, event->xmaprequest.window);
	if (w)
	{
	    if (w->minimized)
		unminimizeWindow (w);

	    leaveShowDesktopMode (w->screen, w);

	    w->managed = TRUE;

	    if (!(w->state & CompWindowStateHiddenMask))
	    {
		w->initialViewportX = w->screen->x;
		w->initialViewportY = w->screen->y;

		w->initialTimestampSet = FALSE;

		applyStartupProperties (w->screen, w);

		w->pendingMaps++;

		XMapWindow (d->display, event->xmaprequest.window);

		updateWindowAttributes (w, FALSE);

		if (focusWindowOnMap (w))
		{
		    moveInputFocusToWindow (w);
		}
		else if (w->type & ~CompWindowTypeSplashMask)
		{
		    CompWindow *p;

		    for (p = w->prev; p; p = p->prev)
			if (p->id == d->activeWindow)
			    break;

		    /* window is above active window so we should lower it */
		    if (p)
			restackWindowBelow (w, p);
		}
	    }

	    setWindowProp (d, w->id, d->winDesktopAtom, w->desktop);
	}
	else
	{
	    XMapWindow (d->display, event->xmaprequest.window);
	}
	break;
    case ConfigureRequest:
	w = findWindowAtDisplay (d, event->xconfigurerequest.window);
	if (w && w->managed)
	{
	    XWindowChanges xwc;

	    memset (&xwc, 0, sizeof (xwc));

	    xwc.x	     = event->xconfigurerequest.x;
	    xwc.y	     = event->xconfigurerequest.y;
	    xwc.width	     = event->xconfigurerequest.width;
	    xwc.height       = event->xconfigurerequest.height;
	    xwc.border_width = event->xconfigurerequest.border_width;

	    moveResizeWindow (w, &xwc, event->xconfigurerequest.value_mask, 0);
	}
	else
	{
	    XWindowChanges xwc;
	    unsigned int   xwcm;

	    xwcm = event->xconfigurerequest.value_mask &
		(CWX | CWY | CWWidth | CWHeight | CWBorderWidth);

	    xwc.x	     = event->xconfigurerequest.x;
	    xwc.y	     = event->xconfigurerequest.y;
	    xwc.width	     = event->xconfigurerequest.width;
	    xwc.height	     = event->xconfigurerequest.height;
	    xwc.border_width = event->xconfigurerequest.border_width;

	    if (w)
		configureXWindow (w, xwcm, &xwc);
	    else
		XConfigureWindow (d->display, event->xconfigurerequest.window,
				  xwcm, &xwc);
	}
	break;
    case CirculateRequest:
	break;
    case FocusIn:
	if (event->xfocus.mode != NotifyGrab)
	{
	    w = findWindowAtDisplay (d, event->xfocus.window);
	    if (w && w->managed)
	    {
		unsigned int state = w->state;

		if (w->id != d->activeWindow)
		    XChangeProperty (d->display, w->screen->root,
				     d->winActiveAtom,
				     XA_WINDOW, 32, PropModeReplace,
				     (unsigned char *) &w->id, 1);

		w->state &= ~CompWindowStateDemandsAttentionMask;

		if (w->state != state)
		    setWindowState (d, w->state, w->id);
	    }
	}
	break;
    case EnterNotify:
	if (!d->screens->maxGrab		    &&
	    event->xcrossing.mode   != NotifyGrab   &&
	    event->xcrossing.detail != NotifyInferior)
	{
	    Bool raise;
	    int  delay;

	    raise = d->opt[COMP_DISPLAY_OPTION_AUTORAISE].value.b;
	    delay = d->opt[COMP_DISPLAY_OPTION_AUTORAISE_DELAY].value.i;

	    s = findScreenAtDisplay (d, event->xcrossing.root);
	    if (s)
	    {
		w = findTopLevelWindowAtScreen (s, event->xcrossing.window);
	    }
	    else
		w = NULL;

	    if (w && w->id != d->below)
	    {
		d->below = w->id;

		if (!d->opt[COMP_DISPLAY_OPTION_CLICK_TO_FOCUS].value.b)
		{
		    if (d->autoRaiseHandle &&
			d->autoRaiseWindow != w->id)
		    {
			compRemoveTimeout (d->autoRaiseHandle);
			d->autoRaiseHandle = 0;
		    }

		    if (w->type & ~(CompWindowTypeDockMask |
				    CompWindowTypeDesktopMask))
		    {
			moveInputFocusToWindow (w);

			if (raise)
			{
			    if (delay > 0)
			    {
				d->autoRaiseWindow = w->id;
				d->autoRaiseHandle =
				    compAddTimeout (delay, autoRaiseTimeout, d);
			    }
			    else
				updateWindowAttributes (w, FALSE);
			}
		    }
		}
	    }
	}
	break;
    case LeaveNotify:
	if (event->xcrossing.detail != NotifyInferior)
	{
	    if (event->xcrossing.window == d->below)
		d->below = None;
	}
	break;
    default:
	if (event->type == d->damageEvent + XDamageNotify)
	{
	    XDamageNotifyEvent *de = (XDamageNotifyEvent *) event;

	    if (lastDamagedWindow && de->drawable == lastDamagedWindow->id)
	    {
		w = lastDamagedWindow;
	    }
	    else
	    {
		w = findWindowAtDisplay (d, de->drawable);
		if (w)
		    lastDamagedWindow = w;
	    }

	    if (w)
	    {
		w->texture->oldMipmaps = TRUE;

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
	else if (d->shapeExtension &&
		 event->type == d->shapeEvent + ShapeNotify)
	{
	    w = findWindowAtDisplay (d, ((XShapeEvent *) event)->window);
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
	else if (event->type == d->randrEvent + RRScreenChangeNotify)
	{
	    XRRScreenChangeNotifyEvent *rre;

	    rre = (XRRScreenChangeNotifyEvent *) event;

	    s = findScreenAtDisplay (d, rre->root);
	    if (s)
		detectRefreshRateOfScreen (s);
	}
	else if (event->type == d->syncEvent + XSyncAlarmNotify)
	{
	    XSyncAlarmNotifyEvent *sa;

	    sa = (XSyncAlarmNotifyEvent *) event;

	    w = NULL;

	    for (s = d->screens; s; s = s->next)
		for (w = s->windows; w; w = w->next)
		    if (w->syncAlarm == sa->alarm)
			break;

	    if (w)
		handleSyncAlarm (w);
	}
	break;
    }
}
