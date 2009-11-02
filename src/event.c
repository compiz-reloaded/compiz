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

#include <compiz-core.h>

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

    if (!w->redirected || w->bindFailed)
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
    CompScreen  *s = w->screen;
    CompDisplay *d = s->display;
    Bool        focussedAny = FALSE;

    if (w->id != d->activeWindow && w->id != d->nextActiveWindow)
	return;

    if (w->transientFor && w->transientFor != s->root)
    {
	CompWindow *ancestor = findWindowAtDisplay (d, w->transientFor);
	if (ancestor && !(ancestor->type & (CompWindowTypeDesktopMask |
					    CompWindowTypeDockMask)))
	{
	    moveInputFocusToWindow (ancestor);
	    focussedAny = TRUE;
	}
    }
    else if (w->type & (CompWindowTypeDialogMask |
			CompWindowTypeModalDialogMask))
    {
	CompWindow *a, *focus = NULL;

	for (a = s->reverseWindows; a; a = a->prev)
	{
	    if (a->clientLeader != w->clientLeader)
		continue;

	    if (!(*s->focusWindow) (a))
		continue;

	    if (!focus)
	    {
		focus = a;
		continue;
	    }

	    if (a->type & (CompWindowTypeNormalMask |
			   CompWindowTypeDialogMask |
			   CompWindowTypeModalDialogMask))
	    {
		if (compareWindowActiveness (focus, a) < 0)
		    focus = a;
	    }
	}

	if (focus && !(focus->type & (CompWindowTypeDesktopMask |
				      CompWindowTypeDockMask)))
	{
	    moveInputFocusToWindow (focus);
	    focussedAny = TRUE;
	}
    }

    if (!focussedAny)
	focusDefaultWindow (s);
}

static Bool
autoRaiseTimeout (void *closure)
{
    CompDisplay *display = closure;
    CompWindow  *w = findWindowAtDisplay (display, display->activeWindow);

    if (display->autoRaiseWindow == display->activeWindow ||
	(w && (display->autoRaiseWindow == w->transientFor)))
    {
	w = findWindowAtDisplay (display, display->autoRaiseWindow);
	if (w)
	    updateWindowAttributes (w, CompStackingUpdateModeNormal);
    }

    return FALSE;
}

#define REAL_MOD_MASK (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | \
		       Mod3Mask | Mod4Mask | Mod5Mask | CompNoMask)

static Bool
isCallBackBinding (CompOption	   *option,
		   CompBindingType type,
		   CompActionState state)
{
    if (!isActionOption (option))
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
triggerButtonPressBindings (CompDisplay  *d,
			    CompOption   *option,
			    int		 nOption,
			    XButtonEvent *event,
			    CompOption   *argument,
			    int		 nArgument)
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

	s = findScreenAtDisplay (d, event->root);
	if (!s)
	    return FALSE;

	if (event->window != edgeWindow)
	{
	    if (!s->maxGrab || event->window != s->root)
		return FALSE;
	}

	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	{
	    if (edgeWindow == s->screenEdge[i].id)
	    {
		edge = 1 << i;
		argument[1].value.i = d->activeWindow;
		break;
	    }
	}
    }

    while (nOption--)
    {
	if (isInitiateBinding (option, CompBindingTypeButton, state, &action))
	{
	    if (action->button.button == event->button)
	    {
		bindMods = virtualToRealModMask (d, action->button.modifiers);

		if ((bindMods & modMask) == (event->state & modMask))
		    if ((*action->initiate) (d, action, state,
					     argument, nArgument))
			return TRUE;
	    }
	}

	if (edge)
	{
	    if (isInitiateBinding (option, CompBindingTypeEdgeButton,
				   state | CompActionStateInitEdge, &action))
	    {
		if ((action->button.button == event->button) &&
		    (action->edgeMask & edge))
		{
		    bindMods = virtualToRealModMask (d,
						     action->button.modifiers);

		    if ((bindMods & modMask) == (event->state & modMask))
			if ((*action->initiate) (d, action, state |
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
triggerButtonReleaseBindings (CompDisplay  *d,
			      CompOption   *option,
			      int	   nOption,
			      XButtonEvent *event,
			      CompOption   *argument,
			      int	   nArgument)
{
    CompActionState state = CompActionStateTermButton;
    CompBindingType type  = CompBindingTypeButton | CompBindingTypeEdgeButton;
    CompAction	    *action;

    while (nOption--)
    {
	if (isTerminateBinding (option, type, state, &action))
	{
	    if (action->button.button == event->button)
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
			 XKeyEvent   *event,
			 CompOption  *argument,
			 int	     nArgument)
{
    CompActionState state = 0;
    CompAction	    *action;
    unsigned int    modMask = REAL_MOD_MASK & ~d->ignoredModMask;
    unsigned int    bindMods;

    if (event->keycode == d->escapeKeyCode)
	state = CompActionStateCancel;
    else if (event->keycode == d->returnKeyCode)
	state = CompActionStateCommit;

    if (state)
    {
	CompOption *o = option;
	int	   n = nOption;

	while (n--)
	{
	    if (isActionOption (o))
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

	    if (action->key.keycode == event->keycode)
	    {
		if ((bindMods & modMask) == (event->state & modMask))
		    if ((*action->initiate) (d, action, state,
					     argument, nArgument))
			return TRUE;
	    }
	    else if (!d->xkbEvent && action->key.keycode == 0)
	    {
		if (bindMods == (event->state & modMask))
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
			   XKeyEvent   *event,
			   CompOption  *argument,
			   int	       nArgument)
{
    CompActionState state = CompActionStateTermKey;
    CompAction	    *action;
    unsigned int    modMask = REAL_MOD_MASK & ~d->ignoredModMask;
    unsigned int    bindMods;
    unsigned int    mods;

    mods = keycodeToModifiers (d, event->keycode);
    if (!d->xkbEvent && !mods)
	return FALSE;

    while (nOption--)
    {
	if (isTerminateBinding (option, CompBindingTypeKey, state, &action))
	{
	    bindMods = virtualToRealModMask (d, action->key.modifiers);

	    if ((bindMods & modMask) == 0)
	    {
		if (action->key.keycode == event->keycode)
		{
		    if ((*action->terminate) (d, action, state,
					      argument, nArgument))
			return TRUE;
		}
	    }
	    else if (!d->xkbEvent && ((mods & modMask & bindMods) != bindMods))
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
    if (option->type != CompOptionTypeAction &&
	option->type != CompOptionTypeBell)
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
    if (option->type != CompOptionTypeAction &&
	option->type != CompOptionTypeButton &&
	option->type != CompOptionTypeEdge)
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
		   CompActionState delayState,
		   unsigned int    edge,
		   CompAction      **action)
{
    if (!isEdgeAction (option, state, edge))
	return FALSE;

    if (option->value.action.type & CompBindingTypeEdgeButton)
	return FALSE;

    if (!option->value.action.initiate)
	return FALSE;

    if (delayState)
    {
	if ((option->value.action.state & CompActionStateNoEdgeDelay) !=
	    (delayState & CompActionStateNoEdgeDelay))
	{
	    /* ignore edge actions which shouldn't be delayed when invoking
	       undelayed edges (or vice versa) */
	    return FALSE;
	}
    }


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
			  CompActionState delayState,
			  unsigned int	  edge,
			  CompOption	  *argument,
			  int		  nArgument)
{
    CompAction *action;

    while (nOption--)
    {
	if (isEdgeEnterAction (option, state, delayState, edge, &action))
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
triggerAllEdgeEnterBindings (CompDisplay     *d,
			     CompActionState state,
			     CompActionState delayState,
			     unsigned int    edge,
			     CompOption	     *argument,
			     int	     nArgument)
{
    CompOption *option;
    int        nOption;
    CompPlugin *p;

    for (p = getPlugins (); p; p = p->next)
    {
	if (p->vTable->getObjectOptions)
	{
	    option = (*p->vTable->getObjectOptions) (p, &d->base, &nOption);
	    if (triggerEdgeEnterBindings (d,
					  option, nOption,
					  state, delayState, edge,
					  argument, nArgument))
	    {
		return TRUE;
	    }
	}
    }
    return FALSE;
}

static Bool
delayedEdgeTimeout (void *closure)
{
    CompDelayedEdgeSettings *settings = (CompDelayedEdgeSettings *) closure;

    triggerAllEdgeEnterBindings (settings->d,
				 settings->state,
				 ~CompActionStateNoEdgeDelay,
				 settings->edge,
				 settings->option,
				 settings->nOption);

    free (settings);

    return FALSE;
}

static Bool
triggerEdgeEnter (CompDisplay     *d,
		  unsigned int    edge,
		  CompActionState state,
		  CompOption      *argument,
		  unsigned int    nArgument)
{
    int                     delay;
    CompDelayedEdgeSettings *delayedSettings = NULL;

    delay = d->opt[COMP_DISPLAY_OPTION_EDGE_DELAY].value.i;

    if (nArgument > 7)
	nArgument = 7;

    if (delay > 0)
    {
	delayedSettings = malloc (sizeof (CompDelayedEdgeSettings));
	if (delayedSettings)
	{
	    delayedSettings->d       = d;
	    delayedSettings->edge    = edge;
	    delayedSettings->state   = state;
	    delayedSettings->nOption = nArgument;
	}
    }

    if (delayedSettings)
    {
	CompActionState delayState;
	int             i;

	for (i = 0; i < nArgument; i++)
	    delayedSettings->option[i] = argument[i];

	d->edgeDelayHandle = compAddTimeout (delay, (float) delay * 1.2,
					     delayedEdgeTimeout,
					     delayedSettings);

	delayState = CompActionStateNoEdgeDelay;
	if (triggerAllEdgeEnterBindings (d, state, delayState,
					 edge, argument, nArgument))
	    return TRUE;
    }
    else
    {
	if (triggerAllEdgeEnterBindings (d, state, 0, edge,
					 argument, nArgument))
	    return TRUE;
    }

    return FALSE;
}

static Bool
handleActionEvent (CompDisplay *d,
		   XEvent      *event)
{
    CompObject *obj = &d->base;
    CompOption *option;
    int	       nOption;
    CompPlugin *p;
    CompOption o[8];

    o[0].type = CompOptionTypeInt;
    o[0].name = "event_window";

    o[1].type = CompOptionTypeInt;
    o[1].name = "window";

    o[2].type = CompOptionTypeInt;
    o[2].name = "modifiers";

    o[3].type = CompOptionTypeInt;
    o[3].name = "x";

    o[4].type = CompOptionTypeInt;
    o[4].name = "y";

    o[5].type = CompOptionTypeInt;
    o[5].name = "root";

    switch (event->type) {
    case ButtonPress:
	o[0].value.i = event->xbutton.window;
	o[1].value.i = event->xbutton.window;
	o[2].value.i = event->xbutton.state;
	o[3].value.i = event->xbutton.x_root;
	o[4].value.i = event->xbutton.y_root;
	o[5].value.i = event->xbutton.root;

	o[6].type    = CompOptionTypeInt;
	o[6].name    = "button";
	o[6].value.i = event->xbutton.button;

	o[7].type    = CompOptionTypeInt;
	o[7].name    = "time";
	o[7].value.i = event->xbutton.time;

	for (p = getPlugins (); p; p = p->next)
	{
	    if (!p->vTable->getObjectOptions)
		continue;

	    option = (*p->vTable->getObjectOptions) (p, obj, &nOption);
	    if (triggerButtonPressBindings (d, option, nOption,
					    &event->xbutton, o, 8))
		return TRUE;
	}
	break;
    case ButtonRelease:
	o[0].value.i = event->xbutton.window;
	o[1].value.i = event->xbutton.window;
	o[2].value.i = event->xbutton.state;
	o[3].value.i = event->xbutton.x_root;
	o[4].value.i = event->xbutton.y_root;
	o[5].value.i = event->xbutton.root;

	o[6].type    = CompOptionTypeInt;
	o[6].name    = "button";
	o[6].value.i = event->xbutton.button;

	o[7].type    = CompOptionTypeInt;
	o[7].name    = "time";
	o[7].value.i = event->xbutton.time;

	for (p = getPlugins (); p; p = p->next)
	{
	    if (!p->vTable->getObjectOptions)
		continue;

	    option = (*p->vTable->getObjectOptions) (p, obj, &nOption);
	    if (triggerButtonReleaseBindings (d, option, nOption,
					      &event->xbutton, o, 8))
		return TRUE;
	}
	break;
    case KeyPress:
	o[0].value.i = event->xkey.window;
	o[1].value.i = d->activeWindow;
	o[2].value.i = event->xkey.state;
	o[3].value.i = event->xkey.x_root;
	o[4].value.i = event->xkey.y_root;
	o[5].value.i = event->xkey.root;

	o[6].type    = CompOptionTypeInt;
	o[6].name    = "keycode";
	o[6].value.i = event->xkey.keycode;

	o[7].type    = CompOptionTypeInt;
	o[7].name    = "time";
	o[7].value.i = event->xkey.time;

	for (p = getPlugins (); p; p = p->next)
	{
	    if (!p->vTable->getObjectOptions)
		continue;

	    option = (*p->vTable->getObjectOptions) (p, obj, &nOption);
	    if (triggerKeyPressBindings (d, option, nOption,
					 &event->xkey, o, 8))
		return TRUE;
	}
	break;
    case KeyRelease:
	o[0].value.i = event->xkey.window;
	o[1].value.i = d->activeWindow;
	o[2].value.i = event->xkey.state;
	o[3].value.i = event->xkey.x_root;
	o[4].value.i = event->xkey.y_root;
	o[5].value.i = event->xkey.root;

	o[6].type    = CompOptionTypeInt;
	o[6].name    = "keycode";
	o[6].value.i = event->xkey.keycode;

	o[7].type    = CompOptionTypeInt;
	o[7].name    = "time";
	o[7].value.i = event->xkey.time;

	for (p = getPlugins (); p; p = p->next)
	{
	    if (!p->vTable->getObjectOptions)
		continue;
	    option = (*p->vTable->getObjectOptions) (p, obj, &nOption);
	    if (triggerKeyReleaseBindings (d, option, nOption,
					   &event->xkey, o, 8))
		return TRUE;
	}
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

	    if (d->edgeDelayHandle)
	    {
		void *closure;

		closure = compRemoveTimeout (d->edgeDelayHandle);
		if (closure)
		    free (closure);
		d->edgeDelayHandle = 0;
	    }

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

		o[0].value.i = event->xcrossing.window;
		o[1].value.i = d->activeWindow;
		o[2].value.i = event->xcrossing.state;
		o[3].value.i = event->xcrossing.x_root;
		o[4].value.i = event->xcrossing.y_root;
		o[5].value.i = event->xcrossing.root;

		o[6].type    = CompOptionTypeInt;
		o[6].name    = "time";
		o[6].value.i = event->xcrossing.time;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (!p->vTable->getObjectOptions)
			continue;

		    option = (*p->vTable->getObjectOptions) (p, obj, &nOption);
		    if (triggerEdgeLeaveBindings (d, option, nOption, state,
						  edge, o, 7))
			return TRUE;
		}
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

		o[0].value.i = event->xcrossing.window;
		o[1].value.i = d->activeWindow;
		o[2].value.i = event->xcrossing.state;
		o[3].value.i = event->xcrossing.x_root;
		o[4].value.i = event->xcrossing.y_root;
		o[5].value.i = event->xcrossing.root;

		o[6].type    = CompOptionTypeInt;
		o[6].name    = "time";
		o[6].value.i = event->xcrossing.time;

		if (triggerEdgeEnter (d, edge, state, o, 7))
		    return TRUE;
	    }
	}
	break;
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

		o[0].value.i = event->xclient.window;
		o[1].value.i = d->activeWindow;
		o[2].value.i = 0; /* fixme */
		o[3].value.i = 0; /* fixme */
		o[4].value.i = 0; /* fixme */
		o[5].value.i = root;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (!p->vTable->getObjectOptions)
			continue;

		    option = (*p->vTable->getObjectOptions) (p, obj, &nOption);
		    if (triggerEdgeLeaveBindings (d, option, nOption, state,
						  edge, o, 6))
			return TRUE;
		}
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

		o[0].value.i = event->xclient.window;
		o[1].value.i = d->activeWindow;
		o[2].value.i = 0; /* fixme */
		o[3].value.i = event->xclient.data.l[2] >> 16;
		o[4].value.i = event->xclient.data.l[2] & 0xffff;
		o[5].value.i = root;

		if (triggerEdgeEnter (d, edge, state, o, 6))
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

		o[0].value.i = d->activeWindow;
		o[1].value.i = d->activeWindow;
		o[2].value.i = stateEvent->mods;

		o[3].type    = CompOptionTypeInt;
		o[3].name    = "time";
		o[3].value.i = xkbEvent->time;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (!p->vTable->getObjectOptions)
			continue;

		    option = (*p->vTable->getObjectOptions) (p, obj, &nOption);
		    if (triggerStateNotifyBindings (d, option, nOption,
						    stateEvent, o, 4))
			return TRUE;
		}
	    }
	    else if (xkbEvent->xkb_type == XkbBellNotify)
	    {
		o[0].value.i = d->activeWindow;
		o[1].value.i = d->activeWindow;

		o[2].type    = CompOptionTypeInt;
		o[2].name    = "time";
		o[2].value.i = xkbEvent->time;

		for (p = getPlugins (); p; p = p->next)
		{
		    if (!p->vTable->getObjectOptions)
			continue;

		    option = (*p->vTable->getObjectOptions) (p, obj, &nOption);
		    if (triggerBellNotifyBindings (d, option, nOption, o, 3))
			return TRUE;
		}
	    }
	}
	break;
    }

    return FALSE;
}

void
handleCompizEvent (CompDisplay *d,
		   const char  *pluginName,
		   const char  *eventName,
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
	for (s = d->screens; s; s = s->next)
	    if (s->output == event->xexpose.window)
		break;

	if (s)
	{
	    int more = event->xexpose.count + 1;

	    if (s->nExpose == s->sizeExpose)
	    {
		s->exposeRects = realloc (s->exposeRects,
					      (s->sizeExpose + more) *
					      sizeof (XRectangle));
		s->sizeExpose += more;
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
	    if (w->pendingMaps)
		w->managed = TRUE;

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

		    changeWindowState (w,
				       w->state & ~CompWindowStateHiddenMask);

		    updateClientListForScreen (w->screen);
		}

		if (!w->attrib.override_redirect)
		    setWmState (d, WithdrawnState, w->id);

		w->placed     = FALSE;
		w->managed    = FALSE;
		w->unmanaging = TRUE;
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
			updateWindowAttributes (w,
					CompStackingUpdateModeAboveFullscreen);

		    if (w->id != d->activeWindow)
			if (!(w->type & CompWindowTypeDockMask))
			    if ((*s->focusWindow) (w))
				moveInputFocusToWindow (w);
		}
	    }

	    if (!s->maxGrab)
		XAllowEvents (d->display, ReplayPointer, event->xbutton.time);
	}
	break;
    case PropertyNotify:
	if (event->xproperty.atom == d->winTypeAtom)
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

		/* EWMH suggests that we ignore changes
		   to _NET_WM_STATE_HIDDEN */
		if (w->state & CompWindowStateHiddenMask)
		    state |= CompWindowStateHiddenMask;
		else
		    state &= ~CompWindowStateHiddenMask;

		if (state != w->state)
		{
		    if (w->type & CompWindowTypeDesktopMask)
			w->paint.opacity = OPAQUE;

		    changeWindowState (w, state);
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
	    {
		updateTransientHint (w);
		recalcWindowActions (w);
	    }
	}
	else if (event->xproperty.atom == d->wmClientLeaderAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		w->clientLeader = getClientLeader (w);
	}
	else if (event->xproperty.atom == d->wmIconGeometryAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		updateIconGeometry (w);
	}
	else if (event->xproperty.atom == d->winOpacityAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w && !(w->type & CompWindowTypeDesktopMask))
	    {
		int opacity;

		opacity = getWindowProp32 (d, w->id, d->winOpacityAtom, OPAQUE);
		if (opacity != w->paint.opacity)
		{
		    w->paint.opacity = opacity;
		    addWindowDamage (w);
		}
	    }
	}
	else if (event->xproperty.atom == d->winBrightnessAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		int brightness;

		brightness = getWindowProp32 (d, w->id,
	     				      d->winBrightnessAtom, BRIGHT);
		if (brightness != w->paint.brightness)
		{
		    w->paint.brightness = brightness;
		    addWindowDamage (w);
		}
	    }
	}
	else if (event->xproperty.atom == d->winSaturationAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w && w->screen->canDoSaturated)
	    {
		int saturation;

		saturation = getWindowProp32 (d, w->id,
					      d->winSaturationAtom, COLOR);
		if (saturation != w->paint.saturation)
		{
		    w->paint.saturation = saturation;
		    addWindowDamage (w);
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

		if (w->managed && w->startupId)
		{
		    Time            timestamp = 0;
		    int             vx, vy, x, y;
		    CompScreen      *s = w->screen;
		    CompFocusResult focus;

		    w->initialTimestampSet = FALSE;
		    applyStartupProperties (w->screen, w);

		    if (w->initialTimestampSet)
			timestamp = w->initialTimestamp;

		    /* as the viewport can't be transmitted via startup
		       notification, assume the client changing the ID
		       wanted to activate the window on the current viewport */

		    defaultViewportForWindow (w, &vx, &vy);
		    x = w->attrib.x + (s->x - vx) * s->width;
		    y = w->attrib.y + (s->y - vy) * s->height;
		    moveWindowToViewportPosition (w, x, y, TRUE);

		    focus = allowWindowFocus (w, 0,
					      w->initialViewportX,
					      w->initialViewportY,
					      timestamp);

		    if (focus == CompFocusAllowed)
			(*w->screen->activateWindow) (w);
		}
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
	    {
		CompFocusResult focus = CompFocusAllowed;

		/* use focus stealing prevention if request came
		   from an application */
		if (event->xclient.data.l[0] == ClientTypeApplication)
		    focus = allowWindowFocus (w, 0,
					      w->screen->x,
					      w->screen->y,
					      event->xclient.data.l[1]);

		if (focus == CompFocusAllowed)
		    (*w->screen->activateWindow) (w);
	    }
	}
	else if (event->xclient.message_type == d->winOpacityAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w && !(w->type & CompWindowTypeDesktopMask))
	    {
		GLushort opacity = event->xclient.data.l[0] >> 16;

		setWindowProp32 (d, w->id, d->winOpacityAtom, opacity);
	    }
	}
	else if (event->xclient.message_type == d->winBrightnessAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		GLushort brightness = event->xclient.data.l[0] >> 16;

		setWindowProp32 (d, w->id, d->winBrightnessAtom, brightness);
	    }
	}
	else if (event->xclient.message_type == d->winSaturationAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w && w->screen->canDoSaturated)
	    {
		GLushort saturation = event->xclient.data.l[0] >> 16;

		setWindowProp32 (d, w->id, d->winSaturationAtom, saturation);
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
		if (w->id == d->activeWindow)
		    wState &= ~CompWindowStateDemandsAttentionMask;

		if (wState != w->state)
		{
		    CompStackingUpdateMode stackingUpdateMode;
		    unsigned long          dState = wState ^ w->state;

		    stackingUpdateMode = CompStackingUpdateModeNone;

		    /* raise the window whenever its fullscreen state,
		       above/below state or maximization state changed */
		    if (dState & CompWindowStateFullscreenMask)
			stackingUpdateMode = CompStackingUpdateModeAboveFullscreen;
		    else if (dState & (CompWindowStateAboveMask         |
				       CompWindowStateBelowMask         |
				       CompWindowStateMaximizedHorzMask |
				       CompWindowStateMaximizedVertMask))
			stackingUpdateMode = CompStackingUpdateModeNormal;

		    changeWindowState (w, wState);

		    updateWindowAttributes (w, stackingUpdateMode);
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
			w->alive = TRUE;

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

		(*core.setOptionForPlugin) (&s->base, "core", "hsize", &value);

		value.i = event->xclient.data.l[1] / s->height;

		(*core.setOptionForPlugin) (&s->base, "core", "vsize", &value);
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
		unsigned int   source;

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
		source  = (event->xclient.data.l[0] >> 12) & 0xF;

		moveResizeWindow (w, &xwc, xwcm, gravity, source);
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
	    if (w)
	    {
		if (event->xclient.data.l[0] == IconicState)
		{
		    if (w->actions & CompWindowActionMinimizeMask)
			minimizeWindow (w);
		}
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
			(*s->enterShowDesktopMode) (s);
		    else
			(*s->leaveShowDesktopMode) (s, NULL);
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

		(*core.setOptionForPlugin) (&s->base,
					    "core", "number_of_desktops",
					    &value);
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
	else if (event->xclient.message_type == d->wmFullscreenMonitorsAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		CompFullscreenMonitorSet monitors;

		monitors.top    = event->xclient.data.l[0];
		monitors.bottom = event->xclient.data.l[1];
		monitors.left   = event->xclient.data.l[2];
		monitors.right  = event->xclient.data.l[3];

		setWindowFullscreenMonitors (w, &monitors);
	    }
	}
	break;
    case MappingNotify:
	updateModifierMappings (d);
	break;
    case MapRequest:
	w = findWindowAtDisplay (d, event->xmaprequest.window);
	if (w)
	{
	    XWindowAttributes attr;
	    Bool              doMapProcessing = TRUE;

	    /* We should check the override_redirect flag here, because the
	       client might have changed it while being unmapped. */
	    if (XGetWindowAttributes (d->display, w->id, &attr))
	    {
		if (w->attrib.override_redirect != attr.override_redirect)
		{
		    w->attrib.override_redirect = attr.override_redirect;
		    recalcWindowType (w);
		    recalcWindowActions (w);

		    (*d->matchPropertyChanged) (d, w);
		}
	    }

	    if (w->state & CompWindowStateHiddenMask)
		if (!w->minimized && !w->inShowDesktopMode)
		    doMapProcessing = FALSE;

	    if (doMapProcessing)
	    {
		w->initialViewportX = w->screen->x;
		w->initialViewportY = w->screen->y;

		w->initialTimestampSet = FALSE;

		applyStartupProperties (w->screen, w);
	    }

	    w->managed = TRUE;

	    if (doMapProcessing)
	    {
		CompFocusResult        focus;
		CompStackingUpdateMode stackingMode;

		if (!w->placed)
		{
		    int            newX, newY;
		    int            gravity = w->sizeHints.win_gravity;
		    XWindowChanges xwc;
		    unsigned int   xwcm, source;

		    /* adjust for gravity */
		    xwc.x      = w->serverX;
		    xwc.y      = w->serverY;
		    xwc.width  = w->serverWidth;
		    xwc.height = w->serverHeight;

		    xwcm = adjustConfigureRequestForGravity (w, &xwc,
							     CWX | CWY,
							     gravity, 1);

		    source = ClientTypeApplication;
		    (*w->screen->validateWindowResizeRequest) (w, &xwcm, &xwc,
							       source);

		    if (xwcm)
			configureXWindow (w, xwcm, &xwc);

		    if ((*w->screen->placeWindow) (w, xwc.x, xwc.y,
						   &newX, &newY))
		    {
			xwc.x = newX;
			xwc.y = newY;
			configureXWindow (w, CWX | CWY, &xwc);
		    }

		    w->placed   = TRUE;
		}

		focus = allowWindowFocus (w, NO_FOCUS_MASK,
					  w->screen->x, w->screen->y, 0);

		if (focus == CompFocusDenied)
		    stackingMode = CompStackingUpdateModeInitialMapDeniedFocus;
		else
		    stackingMode = CompStackingUpdateModeInitialMap;

		updateWindowAttributes (w, stackingMode);

		if (w->minimized)
		    unminimizeWindow (w);

		(*w->screen->leaveShowDesktopMode) (w->screen, w);

		if (focus == CompFocusAllowed && !onCurrentDesktop (w))
		    setCurrentDesktop (w->screen, w->desktop);

		if (!(w->state & CompWindowStateHiddenMask))
		    showWindow (w);

		if (focus == CompFocusAllowed)
		    moveInputFocusToWindow (w);
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

	    moveResizeWindow (w, &xwc, event->xconfigurerequest.value_mask,
			      0, ClientTypeUnknown);

	    if (event->xconfigurerequest.value_mask & CWStackMode)
	    {
		Window          above    = None;
		CompWindow      *sibling = NULL;
		CompFocusResult focus;

		if (event->xconfigurerequest.value_mask & CWSibling)
		{
		    above   = event->xconfigurerequest.above;
		    sibling = findTopLevelWindowAtDisplay (d, above);
		}

		switch (event->xconfigurerequest.detail) {
		case Above:
		    focus = allowWindowFocus (w, NO_FOCUS_MASK,
					      w->screen->x, w->screen->y, 0);
		    if (focus == CompFocusAllowed)
		    {
			if (above)
			{
			    if (sibling)
				restackWindowAbove (w, sibling);
			}
			else
			    raiseWindow (w);
		    }
		    break;
		case Below:
		    if (above)
		    {
			if (sibling)
			    restackWindowBelow (w, sibling);
		    }
		    else
			lowerWindow (w);
		    break;
		default:
		    /* no handling of the TopIf, BottomIf, Opposite cases -
		       there will hardly be any client using that */
		    break;
		}
	    }
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
	    w = findTopLevelWindowAtDisplay (d, event->xfocus.window);
	    if (w && w->managed)
	    {
		unsigned int state = w->state;

		if (w->id != d->activeWindow)
		{
		    d->activeWindow = w->id;
		    w->activeNum = w->screen->activeNum++;

		    addToCurrentActiveWindowHistory (w->screen, w->id);

		    XChangeProperty (d->display, w->screen->root,
				     d->winActiveAtom,
				     XA_WINDOW, 32, PropModeReplace,
				     (unsigned char *) &w->id, 1);
		}

		state &= ~CompWindowStateDemandsAttentionMask;
		changeWindowState (w, state);

		if (d->nextActiveWindow == event->xfocus.window)
		    d->nextActiveWindow = None;
	    }
	}
	break;
    case EnterNotify:
	s = findScreenAtDisplay (d, event->xcrossing.root);
	if (s)
	    w = findTopLevelWindowAtScreen (s, event->xcrossing.window);
	else
	    w = NULL;

	if (w && w->id != d->below)
	{
	    d->below = w->id;

	    if (!d->opt[COMP_DISPLAY_OPTION_CLICK_TO_FOCUS].value.b &&
		!s->maxGrab				            &&
		event->xcrossing.mode   != NotifyGrab		    &&
		event->xcrossing.detail != NotifyInferior)
	    {
		Bool raise, focus;
		int  delay;

		raise = d->opt[COMP_DISPLAY_OPTION_AUTORAISE].value.b;
		delay = d->opt[COMP_DISPLAY_OPTION_AUTORAISE_DELAY].value.i;

		if (d->autoRaiseHandle && d->autoRaiseWindow != w->id)
		{
		    compRemoveTimeout (d->autoRaiseHandle);
		    d->autoRaiseHandle = 0;
		}

		if (w->type & NO_FOCUS_MASK)
		    focus = FALSE;
		else
		    focus = (*w->screen->focusWindow) (w);

		if (focus)
		{
		    moveInputFocusToWindow (w);

		    if (raise)
		    {
			if (delay > 0)
			{
			    d->autoRaiseWindow = w->id;
			    d->autoRaiseHandle =
				compAddTimeout (delay, (float) delay * 1.2,
						autoRaiseTimeout, d);
			}
			else
			{
			    CompStackingUpdateMode mode =
				CompStackingUpdateModeNormal;

			    updateWindowAttributes (w, mode);
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
			w->damageRects = realloc (w->damageRects,
						  (w->sizeDamage + 1) *
						  sizeof (XRectangle));
			w->sizeDamage += 1;
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
	else if (d->randrExtension &&
		 event->type == d->randrEvent + RRScreenChangeNotify)
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

	    for (s = d->screens; s; s = s->next)
	    {
		for (w = s->windows; w; w = w->next)
		{
		    if (w->syncAlarm == sa->alarm)
			break;
		}

		if (w)
		{
		    handleSyncAlarm (w);
		    /* it makes no sense to search for the already
		       found window on other screens, so leave screen loop */
		    break;
		}
	    }
	}
	break;
    }
}
