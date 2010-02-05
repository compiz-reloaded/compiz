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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <assert.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>

#include <compiz-core.h>

static unsigned int virtualModMask[] = {
    CompAltMask, CompMetaMask, CompSuperMask, CompHyperMask,
    CompModeSwitchMask, CompNumLockMask, CompScrollLockMask
};

static CompScreen *targetScreen = NULL;
static CompOutput *targetOutput;

static Bool inHandleEvent = FALSE;

static const CompTransform identity = {
    {
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0
    }
};

int lastPointerX = 0;
int lastPointerY = 0;
int pointerX     = 0;
int pointerY     = 0;

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static char *displayPrivateIndices = 0;
static int  displayPrivateLen = 0;

static int
reallocDisplayPrivate (int  size,
		       void *closure)
{
    CompDisplay *d;
    void        *privates;

    for (d = core.displays; d; d = d->next)
    {
	privates = realloc (d->base.privates, size * sizeof (CompPrivate));
	if (!privates)
	    return FALSE;

	d->base.privates = (CompPrivate *) privates;
    }

    return TRUE;
}

int
allocDisplayObjectPrivateIndex (CompObject *parent)
{
    return allocatePrivateIndex (&displayPrivateLen,
				 &displayPrivateIndices,
				 reallocDisplayPrivate,
				 0);
}

void
freeDisplayObjectPrivateIndex (CompObject *parent,
			       int	  index)
{
    freePrivateIndex (displayPrivateLen, displayPrivateIndices, index);
}

CompBool
forEachDisplayObject (CompObject         *parent,
		      ObjectCallBackProc proc,
		      void	         *closure)
{
    if (parent->type == COMP_OBJECT_TYPE_CORE)
    {
	CompDisplay *d;

	for (d = core.displays; d; d = d->next)
	{
	    if (!(*proc) (&d->base, closure))
		return FALSE;
	}
    }

    return TRUE;
}

char *
nameDisplayObject (CompObject *object)
{
    return NULL;
}

CompObject *
findDisplayObject (CompObject *parent,
		   const char *name)
{
    if (parent->type == COMP_OBJECT_TYPE_CORE)
    {
	if (!name || !name[0])
	    return &core.displays->base;
    }

    return NULL;
}

int
allocateDisplayPrivateIndex (void)
{
    return compObjectAllocatePrivateIndex (NULL, COMP_OBJECT_TYPE_DISPLAY);
}

void
freeDisplayPrivateIndex (int index)
{
    compObjectFreePrivateIndex (NULL, COMP_OBJECT_TYPE_DISPLAY, index);
}

static Bool
closeWin (CompDisplay     *d,
	  CompAction      *action,
	  CompActionState state,
	  CompOption      *option,
	  int		  nOption)
{
    CompWindow   *w;
    Window       xid;
    unsigned int time;

    xid  = getIntOptionNamed (option, nOption, "window", 0);
    time = getIntOptionNamed (option, nOption, "time", CurrentTime);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w && (w->actions & CompWindowActionCloseMask))
	closeWindow (w, time);

    return TRUE;
}

static Bool
unmaximize (CompDisplay     *d,
	    CompAction      *action,
	    CompActionState state,
	    CompOption      *option,
	    int		    nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	maximizeWindow (w, 0);

    return TRUE;
}

static Bool
minimize (CompDisplay     *d,
	  CompAction      *action,
	  CompActionState state,
	  CompOption      *option,
	  int		  nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w && (w->actions & CompWindowActionMinimizeMask))
	minimizeWindow (w);

    return TRUE;
}

static Bool
maximize (CompDisplay     *d,
	  CompAction      *action,
	  CompActionState state,
	  CompOption      *option,
	  int		  nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	maximizeWindow (w, MAXIMIZE_STATE);

    return TRUE;
}

static Bool
maximizeHorizontally (CompDisplay     *d,
		      CompAction      *action,
		      CompActionState state,
		      CompOption      *option,
		      int	      nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	maximizeWindow (w, w->state | CompWindowStateMaximizedHorzMask);

    return TRUE;
}

static Bool
maximizeVertically (CompDisplay     *d,
		    CompAction      *action,
		    CompActionState state,
		    CompOption      *option,
		    int		    nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	maximizeWindow (w, w->state | CompWindowStateMaximizedVertMask);

    return TRUE;
}

static Bool
showDesktop (CompDisplay     *d,
	     CompAction      *action,
	     CompActionState state,
	     CompOption      *option,
	     int	     nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	if (s->showingDesktopMask == 0)
	    (*s->enterShowDesktopMode) (s);
	else
	    (*s->leaveShowDesktopMode) (s, NULL);
    }

    return TRUE;
}

static Bool
toggleSlowAnimations (CompDisplay     *d,
		      CompAction      *action,
		      CompActionState state,
		      CompOption      *option,
		      int	      nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
	s->slowAnimations = !s->slowAnimations;

    return TRUE;
}

static Bool
raiseInitiate (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	raiseWindow (w);

    return TRUE;
}

static Bool
lowerInitiate (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	lowerWindow (w);

    return TRUE;
}

static Bool
windowMenu (CompDisplay     *d,
	    CompAction      *action,
	    CompActionState state,
	    CompOption      *option,
	    int		    nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w && !w->screen->maxGrab)
    {
	int  x, y, button;
	Time time;

	time   = getIntOptionNamed (option, nOption, "time", CurrentTime);
	button = getIntOptionNamed (option, nOption, "button", 0);
	x      = getIntOptionNamed (option, nOption, "x", w->attrib.x);
	y      = getIntOptionNamed (option, nOption, "y", w->attrib.y);

	toolkitAction (w->screen,
		       w->screen->display->toolkitActionWindowMenuAtom,
		       time,
		       w->id,
		       button,
		       x,
		       y);
    }

    return TRUE;
}

static Bool
toggleMaximized (CompDisplay     *d,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int		 nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
    {
	if ((w->state & MAXIMIZE_STATE) == MAXIMIZE_STATE)
	    maximizeWindow (w, 0);
	else
	    maximizeWindow (w, MAXIMIZE_STATE);
    }

    return TRUE;
}

static Bool
toggleMaximizedHorizontally (CompDisplay     *d,
			     CompAction      *action,
			     CompActionState state,
			     CompOption      *option,
			     int	     nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	maximizeWindow (w, w->state ^ CompWindowStateMaximizedHorzMask);

    return TRUE;
}

static Bool
toggleMaximizedVertically (CompDisplay     *d,
			   CompAction      *action,
			   CompActionState state,
			   CompOption      *option,
			   int		   nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	maximizeWindow (w, w->state ^ CompWindowStateMaximizedVertMask);

    return TRUE;
}

static Bool
shade (CompDisplay     *d,
       CompAction      *action,
       CompActionState state,
       CompOption      *option,
       int	       nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w && (w->actions & CompWindowActionShadeMask))
    {
	w->state ^= CompWindowStateShadedMask;
	updateWindowAttributes (w, CompStackingUpdateModeNone);
    }

    return TRUE;
}

const CompMetadataOptionInfo coreDisplayOptionInfo[COMP_DISPLAY_OPTION_NUM] = {
    { "abi", "int", 0, 0, 0 },
    { "active_plugins", "list", "<type>string</type>", 0, 0 },
    { "texture_filter", "int", RESTOSTRING (0, 2), 0, 0 },
    { "click_to_focus", "bool", 0, 0, 0 },
    { "autoraise", "bool", 0, 0, 0 },
    { "autoraise_delay", "int", 0, 0, 0 },
    { "close_window_key", "key", 0, closeWin, 0 },
    { "close_window_button", "button", 0, closeWin, 0 },
    { "slow_animations_key", "key", 0, toggleSlowAnimations, 0 },
    { "raise_window_key", "key", 0, raiseInitiate, 0 },
    { "raise_window_button", "button", 0, raiseInitiate, 0 },
    { "lower_window_key", "key", 0, lowerInitiate, 0 },
    { "lower_window_button", "button", 0, lowerInitiate, 0 },
    { "unmaximize_window_key", "key", 0, unmaximize, 0 },
    { "minimize_window_key", "key", 0, minimize, 0 },
    { "minimize_window_button", "button", 0, minimize, 0 },
    { "maximize_window_key", "key", 0, maximize, 0 },
    { "maximize_window_horizontally_key", "key", 0, maximizeHorizontally, 0 },
    { "maximize_window_vertically_key", "key", 0, maximizeVertically, 0 },
    { "window_menu_button", "button", 0, windowMenu, 0 },
    { "window_menu_key", "key", 0, windowMenu, 0 },
    { "show_desktop_key", "key", 0, showDesktop, 0 },
    { "show_desktop_edge", "edge", 0, showDesktop, 0 },
    { "raise_on_click", "bool", 0, 0, 0 },
    { "audible_bell", "bool", 0, 0, 0 },
    { "toggle_window_maximized_key", "key", 0, toggleMaximized, 0 },
    { "toggle_window_maximized_button", "button", 0, toggleMaximized, 0 },
    { "toggle_window_maximized_horizontally_key", "key", 0,
      toggleMaximizedHorizontally, 0 },
    { "toggle_window_maximized_vertically_key", "key", 0,
      toggleMaximizedVertically, 0 },
    { "hide_skip_taskbar_windows", "bool", 0, 0, 0 },
    { "toggle_window_shaded_key", "key", 0, shade, 0 },
    { "ignore_hints_when_maximized", "bool", 0, 0, 0 },
    { "ping_delay", "int", "<min>1000</min>", 0, 0 },
    { "edge_delay", "int", "<min>0</min>", 0, 0 }
};

CompOption *
getDisplayOptions (CompPlugin  *plugin,
		   CompDisplay *display,
		   int	       *count)
{
    *count = NUM_OPTIONS (display);
    return display->opt;
}

static void
setAudibleBell (CompDisplay *display,
		Bool	    audible)
{
    if (display->xkbExtension)
	XkbChangeEnabledControls (display->display,
				  XkbUseCoreKbd,
				  XkbAudibleBellMask,
				  audible ? XkbAudibleBellMask : 0);
}

static Bool
pingTimeout (void *closure)
{
    CompDisplay *d = closure;
    CompScreen  *s;
    CompWindow  *w;
    XEvent      ev;
    int		ping = d->lastPing + 1;

    ev.type		    = ClientMessage;
    ev.xclient.window	    = 0;
    ev.xclient.message_type = d->wmProtocolsAtom;
    ev.xclient.format	    = 32;
    ev.xclient.data.l[0]    = d->wmPingAtom;
    ev.xclient.data.l[1]    = ping;
    ev.xclient.data.l[2]    = 0;
    ev.xclient.data.l[3]    = 0;
    ev.xclient.data.l[4]    = 0;

    for (s = d->screens; s; s = s->next)
    {
	for (w = s->windows; w; w = w->next)
	{
	    if (w->attrib.map_state != IsViewable)
		continue;

	    if (!(w->type & CompWindowTypeNormalMask))
		continue;

	    if (w->protocols & CompWindowProtocolPingMask)
	    {
		if (w->transientFor)
		    continue;

		if (w->lastPong < d->lastPing)
		{
		    if (w->alive)
		    {
			w->alive = FALSE;

			if (w->closeRequests)
			{
			    toolkitAction (s,
					   d->toolkitActionForceQuitDialogAtom,
					   w->lastCloseRequestTime,
					   w->id,
					   TRUE,
					   0,
					   0);

			    w->closeRequests = 0;
			}

			addWindowDamage (w);
		    }
		}

		ev.xclient.window    = w->id;
		ev.xclient.data.l[2] = w->id;

		XSendEvent (d->display, w->id, FALSE, NoEventMask, &ev);
	    }
	}
    }

    d->lastPing = ping;

    return TRUE;
}

Bool
setDisplayOption (CompPlugin	  *plugin,
		  CompDisplay     *display,
		  const char      *name,
		  CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    o = compFindOption (display->opt, NUM_OPTIONS (display), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case COMP_DISPLAY_OPTION_ABI:
	break;
    case COMP_DISPLAY_OPTION_ACTIVE_PLUGINS:
	if (compSetOptionList (o, value))
	{
	    display->dirtyPluginList = TRUE;
	    return TRUE;
	}
	break;
    case COMP_DISPLAY_OPTION_TEXTURE_FILTER:
	if (compSetIntOption (o, value))
	{
	    CompScreen *s;

	    for (s = display->screens; s; s = s->next)
		damageScreen (s);

	    if (!o->value.i)
		display->textureFilter = GL_NEAREST;
	    else
		display->textureFilter = GL_LINEAR;

	    return TRUE;
	}
	break;
    case COMP_DISPLAY_OPTION_PING_DELAY:
	if (compSetIntOption (o, value))
	{
	    if (display->pingHandle)
		compRemoveTimeout (display->pingHandle);

	    display->pingHandle =
		compAddTimeout (o->value.i, o->value.i + 500,
				pingTimeout, display);
	    return TRUE;
	}
	break;
    case COMP_DISPLAY_OPTION_AUDIBLE_BELL:
	if (compSetBoolOption (o, value))
	{
	    setAudibleBell (display, o->value.b);
	    return TRUE;
	}
	break;
    default:
	if (compSetDisplayOption (display, o, value))
	    return TRUE;
	break;
    }

    return FALSE;
}

static void
updatePlugins (CompDisplay *d)
{
    CompOption      *o;
    CompPlugin      *p, **pop = 0;
    int	            nPop, i, j, k;
    CompOptionValue *pList;
    int             pListCount = 1;

    d->dirtyPluginList = FALSE;

    o = &d->opt[COMP_DISPLAY_OPTION_ACTIVE_PLUGINS];

    /* determine number of plugins, which is core + initial plugins +
       plugins in option list additional to initial plugins */
    for (i = 0; i < nInitialPlugins; i++)
	if (strcmp (initialPlugins[i], "core") != 0)
	    pListCount++;

    for (i = 0; i < o->value.list.nValue; i++)
    {
	if (strcmp (o->value.list.value[i].s, "core") == 0)
	    continue;

	for (j = 0; j < nInitialPlugins; j++)
	{
	    if (strcmp (o->value.list.value[i].s, initialPlugins[j]) == 0)
		break;
	}

	/* plugin is not in initial plugin list */
	if (j == nInitialPlugins)
	    pListCount++;
    }

    pList = malloc (sizeof (CompOptionValue) * pListCount);
    if (!pList)
    {
	(*core.setOptionForPlugin) (&d->base, "core", o->name, &d->plugin);
	return;
    }

    /* new plugin list needs core as first plugin */
    pList[0].s = "core";

    /* afterwards, add the initial plugins */
    for (j = 0, k = 1; j < nInitialPlugins; j++)
    {
	/* avoid adding core twice */
	if (strcmp (initialPlugins[j], "core") == 0)
	    continue;
	
	pList[k++].s = initialPlugins[j];
    }
    j = k;

    /* then add the plugins not in the initial plugin list */
    for (i = 0; i < o->value.list.nValue; i++)
    {
	if (strcmp (o->value.list.value[i].s, "core") == 0)
	    continue;

	for (k = 0; k < nInitialPlugins; k++)
	{
	    if (strcmp (o->value.list.value[i].s, initialPlugins[k]) == 0)
		break;
	}
	
	if (k == nInitialPlugins)
	    pList[j++].s = o->value.list.value[i].s;
    }

    assert (j == pListCount);

    /* j is initialized to 1 to make sure we never pop the core plugin */
    for (i = j = 1; j < d->plugin.list.nValue && i < pListCount; i++, j++)
    {
	if (strcmp (d->plugin.list.value[j].s, pList[i].s))
	    break;
    }

    nPop = d->plugin.list.nValue - j;

    if (nPop)
    {
	pop = malloc (sizeof (CompPlugin *) * nPop);
	if (!pop)
	{
	    (*core.setOptionForPlugin) (&d->base, "core", o->name, &d->plugin);
	    free (pList);
	    return;
	}
    }

    for (j = 0; j < nPop; j++)
    {
	pop[j] = popPlugin ();
	d->plugin.list.nValue--;
	free (d->plugin.list.value[d->plugin.list.nValue].s);
    }

    for (; i < pListCount; i++)
    {
	p = 0;
	for (j = 0; j < nPop; j++)
	{
	    if (pop[j] && strcmp (pop[j]->vTable->name, pList[i].s) == 0)
	    {
		if (pushPlugin (pop[j]))
		{
		    p = pop[j];
		    pop[j] = 0;
		    break;
		}
	    }
	}

	if (p == 0)
	{
	    p = loadPlugin (pList[i].s);
	    if (p)
	    {
		if (!pushPlugin (p))
		{
		    unloadPlugin (p);
		    p = 0;
		}
	    }
	}

	if (p)
	{
	    CompOptionValue *value;

	    value = realloc (d->plugin.list.value, sizeof (CompOptionValue) *
			     (d->plugin.list.nValue + 1));
	    if (value)
	    {
		value[d->plugin.list.nValue].s = strdup (p->vTable->name);

		d->plugin.list.value = value;
		d->plugin.list.nValue++;
	    }
	    else
	    {
		p = popPlugin ();
		unloadPlugin (p);
	    }
	}
    }

    for (j = 0; j < nPop; j++)
    {
	if (pop[j])
	    unloadPlugin (pop[j]);
    }

    if (nPop)
	free (pop);

    free (pList);
    (*core.setOptionForPlugin) (&d->base, "core", o->name, &d->plugin);
}

static void
addTimeout (CompTimeout *timeout)
{
    CompTimeout *p = 0, *t;

    for (t = core.timeouts; t; t = t->next)
    {
	if (timeout->minTime < t->minLeft)
	    break;

	p = t;
    }

    timeout->next = t;
    timeout->minLeft = timeout->minTime;
    timeout->maxLeft = timeout->maxTime;

    if (p)
	p->next = timeout;
    else
	core.timeouts = timeout;
}

CompTimeoutHandle
compAddTimeout (int	     minTime,
		int	     maxTime,
		CallBackProc callBack,
		void	     *closure)
{
    CompTimeout *timeout;

    timeout = malloc (sizeof (CompTimeout));
    if (!timeout)
	return 0;

    timeout->minTime  = minTime;
    timeout->maxTime  = (maxTime >= minTime) ? maxTime : minTime;
    timeout->callBack = callBack;
    timeout->closure  = closure;
    timeout->handle   = core.lastTimeoutHandle++;

    if (core.lastTimeoutHandle == MAXSHORT)
	core.lastTimeoutHandle = 1;

    addTimeout (timeout);

    return timeout->handle;
}

void *
compRemoveTimeout (CompTimeoutHandle handle)
{
    CompTimeout *p = 0, *t;
    void        *closure = NULL;

    for (t = core.timeouts; t; t = t->next)
    {
	if (t->handle == handle)
	    break;

	p = t;
    }

    if (t)
    {
	if (p)
	    p->next = t->next;
	else
	    core.timeouts = t->next;

	closure = t->closure;

	free (t);
    }

    return closure;
}

CompWatchFdHandle
compAddWatchFd (int	     fd,
		short int    events,
		CallBackProc callBack,
		void	     *closure)
{
    CompWatchFd *watchFd;

    watchFd = malloc (sizeof (CompWatchFd));
    if (!watchFd)
	return 0;

    watchFd->fd	      = fd;
    watchFd->callBack = callBack;
    watchFd->closure  = closure;
    watchFd->handle   = core.lastWatchFdHandle++;

    if (core.lastWatchFdHandle == MAXSHORT)
	core.lastWatchFdHandle = 1;

    watchFd->next = core.watchFds;
    core.watchFds = watchFd;

    core.nWatchFds++;

    core.watchPollFds = realloc (core.watchPollFds,
				 core.nWatchFds * sizeof (struct pollfd));

    core.watchPollFds[core.nWatchFds - 1].fd     = fd;
    core.watchPollFds[core.nWatchFds - 1].events = events;

    return watchFd->handle;
}

void
compRemoveWatchFd (CompWatchFdHandle handle)
{
    CompWatchFd *p = 0, *w;
    int i;

    for (i = core.nWatchFds - 1, w = core.watchFds; w; i--, w = w->next)
    {
	if (w->handle == handle)
	    break;

	p = w;
    }

    if (w)
    {
	if (p)
	    p->next = w->next;
	else
	    core.watchFds = w->next;

	core.nWatchFds--;

	if (i < core.nWatchFds)
	    memmove (&core.watchPollFds[i], &core.watchPollFds[i + 1],
		     (core.nWatchFds - i) * sizeof (struct pollfd));

	free (w);
    }
}

short int
compWatchFdEvents (CompWatchFdHandle handle)
{
    CompWatchFd *w;
    int		i;

    for (i = core.nWatchFds - 1, w = core.watchFds; w; i--, w = w->next)
	if (w->handle == handle)
	    return core.watchPollFds[i].revents;

    return 0;
}

#define TIMEVALDIFF(tv1, tv2)						   \
    ((tv1)->tv_sec == (tv2)->tv_sec || (tv1)->tv_usec >= (tv2)->tv_usec) ? \
    ((((tv1)->tv_sec - (tv2)->tv_sec) * 1000000) +			   \
     ((tv1)->tv_usec - (tv2)->tv_usec)) / 1000 :			   \
    ((((tv1)->tv_sec - 1 - (tv2)->tv_sec) * 1000000) +			   \
     (1000000 + (tv1)->tv_usec - (tv2)->tv_usec)) / 1000

static int
getTimeToNextRedraw (CompScreen     *s,
		     struct timeval *tv,
		     struct timeval *lastTv,
		     Bool	    idle)
{
    int diff, next;

    diff = TIMEVALDIFF (tv, lastTv);

    /* handle clock rollback */
    if (diff < 0)
	diff = 0;

    if (idle ||
	(s->getVideoSync && s->opt[COMP_SCREEN_OPTION_SYNC_TO_VBLANK].value.b))
    {
	if (s->timeMult > 1)
	{
	    s->frameStatus = -1;
	    s->redrawTime = s->optimalRedrawTime;
	    s->timeMult--;
	}
    }
    else
    {
	if (diff > s->redrawTime)
	{
	    if (s->frameStatus > 0)
		s->frameStatus = 0;

	    next = s->optimalRedrawTime * (s->timeMult + 1);
	    if (diff > next)
	    {
		s->frameStatus--;
		if (s->frameStatus < -1)
		{
		    s->timeMult++;
		    s->redrawTime = diff = next;
		}
	    }
	}
	else if (diff < s->redrawTime)
	{
	    if (s->frameStatus < 0)
		s->frameStatus = 0;

	    if (s->timeMult > 1)
	    {
		next = s->optimalRedrawTime * (s->timeMult - 1);
		if (diff < next)
		{
		    s->frameStatus++;
		    if (s->frameStatus > 4)
		    {
			s->timeMult--;
			s->redrawTime = next;
		    }
		}
	    }
	}
    }

    if (diff > s->redrawTime)
	return 0;

    return s->redrawTime - diff;
}

static const int maskTable[] = {
    ShiftMask, LockMask, ControlMask, Mod1Mask,
    Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask
};
static const int maskTableSize = sizeof (maskTable) / sizeof (int);

void
updateModifierMappings (CompDisplay *d)
{
    unsigned int    modMask[CompModNum];
    int		    i, minKeycode, maxKeycode, keysymsPerKeycode = 0;
    KeySym*         key;

    for (i = 0; i < CompModNum; i++)
	modMask[i] = 0;

    XDisplayKeycodes (d->display, &minKeycode, &maxKeycode);
    key = XGetKeyboardMapping (d->display,
			       minKeycode, (maxKeycode - minKeycode + 1),
			       &keysymsPerKeycode);

    if (d->modMap)
	XFreeModifiermap (d->modMap);

    d->modMap = XGetModifierMapping (d->display);
    if (d->modMap && d->modMap->max_keypermod > 0)
    {
	KeySym keysym;
	int    index, size, mask;

	size = maskTableSize * d->modMap->max_keypermod;

	for (i = 0; i < size; i++)
	{
	    if (!d->modMap->modifiermap[i])
		continue;

	    index = 0;
	    do
	    {
		keysym = XKeycodeToKeysym (d->display,
					   d->modMap->modifiermap[i],
					   index++);
	    } while (!keysym && index < keysymsPerKeycode);

	    if (keysym)
	    {
		mask = maskTable[i / d->modMap->max_keypermod];

		if (keysym == XK_Alt_L ||
		    keysym == XK_Alt_R)
		{
		    modMask[CompModAlt] |= mask;
		}
		else if (keysym == XK_Meta_L ||
			 keysym == XK_Meta_R)
		{
		    modMask[CompModMeta] |= mask;
		}
		else if (keysym == XK_Super_L ||
			 keysym == XK_Super_R)
		{
		    modMask[CompModSuper] |= mask;
		}
		else if (keysym == XK_Hyper_L ||
			 keysym == XK_Hyper_R)
		{
		    modMask[CompModHyper] |= mask;
		}
		else if (keysym == XK_Mode_switch)
		{
		    modMask[CompModModeSwitch] |= mask;
		}
		else if (keysym == XK_Scroll_Lock)
		{
		    modMask[CompModScrollLock] |= mask;
		}
		else if (keysym == XK_Num_Lock)
		{
		    modMask[CompModNumLock] |= mask;
		}
	    }
	}

	for (i = 0; i < CompModNum; i++)
	{
	    if (!modMask[i])
		modMask[i] = CompNoMask;
	}

	if (memcmp (modMask, d->modMask, sizeof (modMask)))
	{
	    CompScreen *s;

	    memcpy (d->modMask, modMask, sizeof (modMask));

	    d->ignoredModMask = LockMask |
		(modMask[CompModNumLock]    & ~CompNoMask) |
		(modMask[CompModScrollLock] & ~CompNoMask);

	    for (s = d->screens; s; s = s->next)
		updatePassiveGrabs (s);
	}
    }

    if (key)
	XFree (key);
}

unsigned int
virtualToRealModMask (CompDisplay  *d,
		      unsigned int modMask)
{
    int i;

    for (i = 0; i < CompModNum; i++)
    {
	if (modMask & virtualModMask[i])
	{
	    modMask &= ~virtualModMask[i];
	    modMask |= d->modMask[i];
	}
    }

    return modMask;
}

unsigned int
keycodeToModifiers (CompDisplay *d,
		    int         keycode)
{
    unsigned int mods = 0;
    int mod, k;

    for (mod = 0; mod < maskTableSize; mod++)
    {
	for (k = 0; k < d->modMap->max_keypermod; k++)
	{
	    if (d->modMap->modifiermap[mod * d->modMap->max_keypermod + k] ==
		keycode)
		mods |= maskTable[mod];
	}
    }

    return mods;
}

static int
doPoll (int timeout)
{
    int rv;

    rv = poll (core.watchPollFds, core.nWatchFds, timeout);
    if (rv)
    {
	CompWatchFd *w;
	int	    i;

	for (i = core.nWatchFds - 1, w = core.watchFds; w; i--, w = w->next)
	{
	    if (core.watchPollFds[i].revents != 0 && w->callBack)
		(*w->callBack) (w->closure);
	}
    }

    return rv;
}

static void
handleTimeouts (struct timeval *tv)
{
    CompTimeout *t;
    int		timeDiff;

    timeDiff = TIMEVALDIFF (tv, &core.lastTimeout);

    /* handle clock rollback */
    if (timeDiff < 0)
	timeDiff = 0;

    for (t = core.timeouts; t; t = t->next)
    {
	t->minLeft -= timeDiff;
	t->maxLeft -= timeDiff;
    }

    while (core.timeouts && core.timeouts->minLeft <= 0)
    {
	t = core.timeouts;
	if ((*t->callBack) (t->closure))
	{
	    core.timeouts = t->next;
	    addTimeout (t);
	}
	else
	{
	    core.timeouts = t->next;
	    free (t);
	}
    }

    core.lastTimeout = *tv;
}

static void
waitForVideoSync (CompScreen *s)
{
    unsigned int sync;

    if (!s->opt[COMP_SCREEN_OPTION_SYNC_TO_VBLANK].value.b)
	return;

    if (s->getVideoSync)
    {
	glFlush ();

	(*s->getVideoSync) (&sync);
	(*s->waitVideoSync) (2, (sync + 1) % 2, &sync);
    }
}


void
paintScreen (CompScreen   *s,
	     CompOutput   *outputs,
	     int          numOutput,
	     unsigned int mask)
{
    XRectangle r;
    int	       i;

    for (i = 0; i < numOutput; i++)
    {
	targetScreen = s;
	targetOutput = &outputs[i];

	r.x	 = outputs[i].region.extents.x1;
	r.y	 = s->height - outputs[i].region.extents.y2;
	r.width  = outputs[i].width;
	r.height = outputs[i].height;

	if (s->lastViewport.x	   != r.x     ||
	    s->lastViewport.y	   != r.y     ||
	    s->lastViewport.width  != r.width ||
	    s->lastViewport.height != r.height)
	{
	    glViewport (r.x, r.y, r.width, r.height);
	    s->lastViewport = r;
	}

	if (mask & COMP_SCREEN_DAMAGE_ALL_MASK)
	{
	    (*s->paintOutput) (s,
			       &defaultScreenPaintAttrib,
			       &identity,
			       &outputs[i].region, &outputs[i],
			       PAINT_SCREEN_REGION_MASK |
			       PAINT_SCREEN_FULL_MASK);
	}
	else if (mask & COMP_SCREEN_DAMAGE_REGION_MASK)
	{
	    XIntersectRegion (core.tmpRegion,
			      &outputs[i].region,
			      core.outputRegion);

	    if (!(*s->paintOutput) (s,
				    &defaultScreenPaintAttrib,
				    &identity,
				    core.outputRegion, &outputs[i],
				    PAINT_SCREEN_REGION_MASK))
	    {
		(*s->paintOutput) (s,
				   &defaultScreenPaintAttrib,
				   &identity,
				   &outputs[i].region, &outputs[i],
				   PAINT_SCREEN_FULL_MASK);

		XUnionRegion (core.tmpRegion,
			      &outputs[i].region,
			      core.tmpRegion);

	    }
	}
    }
}

void
eventLoop (void)
{
    XEvent	   event;
    int		   timeDiff;
    struct timeval tv;
    CompDisplay    *d;
    CompScreen	   *s;
    CompWindow	   *w;
    CompTimeout    *t;
    int		   time, timeToNextRedraw = 0;
    unsigned int   damageMask, mask;

    for (d = core.displays; d; d = d->next)
	d->watchFdHandle =
	    compAddWatchFd (ConnectionNumber (d->display), POLLIN, NULL, NULL);

    for (;;)
    {
	if (restartSignal || shutDown)
	    break;

	for (d = core.displays; d; d = d->next)
	{
	    if (d->dirtyPluginList)
		updatePlugins (d);

	    while (XPending (d->display))
	    {
		XNextEvent (d->display, &event);

		switch (event.type) {
		case ButtonPress:
		case ButtonRelease:
		    pointerX = event.xbutton.x_root;
		    pointerY = event.xbutton.y_root;
		    break;
		case KeyPress:
		case KeyRelease:
		    pointerX = event.xkey.x_root;
		    pointerY = event.xkey.y_root;
		    break;
		case MotionNotify:
		    pointerX = event.xmotion.x_root;
		    pointerY = event.xmotion.y_root;
		    break;
		case EnterNotify:
		case LeaveNotify:
		    pointerX = event.xcrossing.x_root;
		    pointerY = event.xcrossing.y_root;
		    break;
		case ClientMessage:
		    if (event.xclient.message_type == d->xdndPositionAtom)
		    {
			pointerX = event.xclient.data.l[2] >> 16;
			pointerY = event.xclient.data.l[2] & 0xffff;
		    }
		default:
		    break;
		}

		sn_display_process_event (d->snDisplay, &event);

		inHandleEvent = TRUE;

		(*d->handleEvent) (d, &event);

		inHandleEvent = FALSE;

		lastPointerX = pointerX;
		lastPointerY = pointerY;
	    }
	}

	for (d = core.displays; d; d = d->next)
	{
	    for (s = d->screens; s; s = s->next)
	    {
		if (s->damageMask)
		{
		    finishScreenDrawing (s);
		}
		else
		{
		    s->idle = TRUE;
		}
	    }
	}

	damageMask	 = 0;
	timeToNextRedraw = MAXSHORT;

	for (d = core.displays; d; d = d->next)
	{
	    for (s = d->screens; s; s = s->next)
	    {
		if (!s->damageMask)
		    continue;

		if (!damageMask)
		{
		    gettimeofday (&tv, 0);
		    damageMask |= s->damageMask;
		}

		s->timeLeft = getTimeToNextRedraw (s, &tv, &s->lastRedraw,
						   s->idle);
		if (s->timeLeft < timeToNextRedraw)
		    timeToNextRedraw = s->timeLeft;
	    }
	}

	if (damageMask)
	{
	    time = timeToNextRedraw;
	    if (time)
		time = doPoll (time);

	    if (time == 0)
	    {
		gettimeofday (&tv, 0);

		if (core.timeouts)
		    handleTimeouts (&tv);

		for (d = core.displays; d; d = d->next)
		{
		    for (s = d->screens; s; s = s->next)
		    {
			if (!s->damageMask || s->timeLeft > timeToNextRedraw)
			    continue;

			targetScreen = s;

			timeDiff = TIMEVALDIFF (&tv, &s->lastRedraw);

			/* handle clock rollback */
			if (timeDiff < 0)
			    timeDiff = 0;

			makeScreenCurrent (s);

			if (s->slowAnimations)
			{
			    (*s->preparePaintScreen) (s,
						      s->idle ? 2 :
						      (timeDiff * 2) /
						      s->redrawTime);
			}
			else
			    (*s->preparePaintScreen) (s,
						      s->idle ? s->redrawTime :
						      timeDiff);

			/* substract top most overlay window region */
			if (s->overlayWindowCount)
			{
			    for (w = s->reverseWindows; w; w = w->prev)
			    {
				if (w->destroyed || w->invisible)
				    continue;

				if (!w->redirected)
				    XSubtractRegion (s->damage, w->region,
						     s->damage);

				break;
			    }

			    if (s->damageMask & COMP_SCREEN_DAMAGE_ALL_MASK)
			    {
				s->damageMask &= ~COMP_SCREEN_DAMAGE_ALL_MASK;
				s->damageMask |=
				    COMP_SCREEN_DAMAGE_REGION_MASK;
			    }
			}

			if (s->damageMask & COMP_SCREEN_DAMAGE_REGION_MASK)
			{
			    XIntersectRegion (s->damage, &s->region,
					      core.tmpRegion);

			    if (core.tmpRegion->numRects  == 1	  &&
				core.tmpRegion->rects->x1 == 0	  &&
				core.tmpRegion->rects->y1 == 0	  &&
				core.tmpRegion->rects->x2 == s->width &&
				core.tmpRegion->rects->y2 == s->height)
				damageScreen (s);
			}

			EMPTY_REGION (s->damage);

			mask = s->damageMask;
			s->damageMask = 0;

			if (s->clearBuffers)
			{
			    if (mask & COMP_SCREEN_DAMAGE_ALL_MASK)
				glClear (GL_COLOR_BUFFER_BIT);
			}

			if (s->opt[COMP_SCREEN_OPTION_FORCE_INDEPENDENT].value.b
			    || !s->hasOverlappingOutputs)
			    (*s->paintScreen) (s, s->outputDev,
					       s->nOutputDev,
					       mask);
			else
			    (*s->paintScreen) (s, &s->fullscreenOutput, 1,
					       mask);

			targetScreen = NULL;
			targetOutput = &s->outputDev[0];

			waitForVideoSync (s);

			if (mask & COMP_SCREEN_DAMAGE_ALL_MASK)
			{
			    glXSwapBuffers (d->display, s->output);
			}
			else
			{
			    BoxPtr pBox;
			    int    nBox, y;

			    pBox = core.tmpRegion->rects;
			    nBox = core.tmpRegion->numRects;

			    if (s->copySubBuffer)
			    {
				while (nBox--)
				{
				    y = s->height - pBox->y2;

				    (*s->copySubBuffer) (d->display,
							 s->output,
							 pBox->x1, y,
							 pBox->x2 - pBox->x1,
							 pBox->y2 - pBox->y1);

				    pBox++;
				}
			    }
			    else
			    {
				glEnable (GL_SCISSOR_TEST);
				glDrawBuffer (GL_FRONT);

				while (nBox--)
				{
				    y = s->height - pBox->y2;

				    glBitmap (0, 0, 0, 0,
					      pBox->x1 - s->rasterX,
					      y - s->rasterY,
					      NULL);

				    s->rasterX = pBox->x1;
				    s->rasterY = y;

				    glScissor (pBox->x1, y,
					       pBox->x2 - pBox->x1,
					       pBox->y2 - pBox->y1);

				    glCopyPixels (pBox->x1, y,
						  pBox->x2 - pBox->x1,
						  pBox->y2 - pBox->y1,
						  GL_COLOR);

				    pBox++;
				}

				glDrawBuffer (GL_BACK);
				glDisable (GL_SCISSOR_TEST);
				glFlush ();
			    }
			}

			s->lastRedraw = tv;

			(*s->donePaintScreen) (s);

			/* remove destroyed windows */
			while (s->pendingDestroys)
			{
			    CompWindow *w;

			    for (w = s->windows; w; w = w->next)
			    {
				if (w->destroyed)
				{
				    addWindowDamage (w);
				    removeWindow (w);
				    break;
				}
			    }

			    s->pendingDestroys--;
			}

			s->idle = FALSE;
		    }
		}
	    }
	}
	else
	{
	    if (core.timeouts)
	    {
		if (core.timeouts->minLeft > 0)
		{
		    t = core.timeouts;
		    time = t->maxLeft;
		    while (t && t->minLeft <= time)
		    {
			if (t->maxLeft < time)
			    time = t->maxLeft;
			t = t->next;
		    }
		    doPoll (time);
		}

		gettimeofday (&tv, 0);

		handleTimeouts (&tv);
	    }
	    else
	    {
		doPoll (-1);
	    }
	}
    }

    for (d = core.displays; d; d = d->next)
	compRemoveWatchFd (d->watchFdHandle);
}

static int errors = 0;

static int
errorHandler (Display     *dpy,
	      XErrorEvent *e)
{

#ifdef DEBUG
    char str[128];
#endif

    errors++;

#ifdef DEBUG
    XGetErrorDatabaseText (dpy, "XlibMessage", "XError", "", str, 128);
    fprintf (stderr, "%s", str);

    XGetErrorText (dpy, e->error_code, str, 128);
    fprintf (stderr, ": %s\n  ", str);

    XGetErrorDatabaseText (dpy, "XlibMessage", "MajorCode", "%d", str, 128);
    fprintf (stderr, str, e->request_code);

    sprintf (str, "%d", e->request_code);
    XGetErrorDatabaseText (dpy, "XRequest", str, "", str, 128);
    if (strcmp (str, ""))
	fprintf (stderr, " (%s)", str);
    fprintf (stderr, "\n  ");

    XGetErrorDatabaseText (dpy, "XlibMessage", "MinorCode", "%d", str, 128);
    fprintf (stderr, str, e->minor_code);
    fprintf (stderr, "\n  ");

    XGetErrorDatabaseText (dpy, "XlibMessage", "ResourceID", "%d", str, 128);
    fprintf (stderr, str, e->resourceid);
    fprintf (stderr, "\n");

    /* abort (); */
#endif

    return 0;
}

int
compCheckForError (Display *dpy)
{
    int e;

    XSync (dpy, FALSE);

    e = errors;
    errors = 0;

    return e;
}

/* add actions that should be automatically added as no screens
   existed when they were initialized. */
static void
addScreenActions (CompScreen *s)
{
    int i;

    for (i = 0; i < COMP_DISPLAY_OPTION_NUM; i++)
    {
	if (!isActionOption (&s->display->opt[i]))
	    continue;

	if (s->display->opt[i].value.action.state & CompActionStateAutoGrab)
	    addScreenAction (s, &s->display->opt[i].value.action);
    }
}

void
addScreenToDisplay (CompDisplay *display,
		    CompScreen  *s)
{
    CompScreen *prev;

    for (prev = display->screens; prev && prev->next; prev = prev->next);

    if (prev)
	prev->next = s;
    else
	display->screens = s;

    addScreenActions (s);
}

static void
freeDisplay (CompDisplay *d)
{
    compFiniDisplayOptions (d, d->opt, COMP_DISPLAY_OPTION_NUM);

    compFiniOptionValue (&d->plugin, CompOptionTypeList);

    if (d->modMap)
	XFreeModifiermap (d->modMap);

    if (d->screenInfo)
	XFree (d->screenInfo);

    if (d->screenPrivateIndices)
	free (d->screenPrivateIndices);

    if (d->base.privates)
	free (d->base.privates);

    free (d);
}

static Bool
aquireSelection (CompDisplay *d,
		 int         screen,
		 const char  *name,
		 Atom        selection,
		 Window      owner,
		 Time        timestamp)
{
    Display *dpy = d->display;
    Window  root = XRootWindow (dpy, screen);
    XEvent  event;

    XSetSelectionOwner (dpy, selection, owner, timestamp);

    if (XGetSelectionOwner (dpy, selection) != owner)
    {
	compLogMessage ("core", CompLogLevelError,
			"Could not acquire %s manager "
			"selection on screen %d display \"%s\"",
			name, screen, DisplayString (dpy));

	return FALSE;
    }

    /* Send client message indicating that we are now the manager */
    event.xclient.type         = ClientMessage;
    event.xclient.window       = root;
    event.xclient.message_type = d->managerAtom;
    event.xclient.format       = 32;
    event.xclient.data.l[0]    = timestamp;
    event.xclient.data.l[1]    = selection;
    event.xclient.data.l[2]    = 0;
    event.xclient.data.l[3]    = 0;
    event.xclient.data.l[4]    = 0;

    XSendEvent (dpy, root, FALSE, StructureNotifyMask, &event);

    return TRUE;
}

Bool
addDisplay (const char *name)
{
    CompDisplay *d;
    CompPrivate	*privates;
    Display     *dpy;
    Window	focus;
    int		revertTo, i;
    int		compositeMajor, compositeMinor;
    int		fixesMinor;
    int		xkbOpcode;
    int		firstScreen, lastScreen;

    d = malloc (sizeof (CompDisplay));
    if (!d)
	return FALSE;

    if (displayPrivateLen)
    {
	privates = malloc (displayPrivateLen * sizeof (CompPrivate));
	if (!privates)
	{
	    free (d);
	    return FALSE;
	}
    }
    else
	privates = 0;

    compObjectInit (&d->base, privates, COMP_OBJECT_TYPE_DISPLAY);

    d->next    = NULL;
    d->screens = NULL;

    d->watchFdHandle = 0;

    d->screenPrivateIndices = 0;
    d->screenPrivateLen     = 0;

    d->edgeDelayHandle = 0;

    d->modMap = 0;

    for (i = 0; i < CompModNum; i++)
	d->modMask[i] = CompNoMask;

    d->ignoredModMask = LockMask;

    compInitOptionValue (&d->plugin);

    d->plugin.list.type   = CompOptionTypeString;
    d->plugin.list.nValue = 1;
    d->plugin.list.value  = malloc (sizeof (CompOptionValue));

    if (!d->plugin.list.value) {
	free (d);
	return FALSE;
    }

    d->plugin.list.value->s = strdup ("core");
    if (!d->plugin.list.value->s) {
        free (d->plugin.list.value);
	free (d);
	return FALSE;
    }

    d->dirtyPluginList = TRUE;

    d->textureFilter = GL_LINEAR;
    d->below	     = None;

    d->activeWindow = 0;

    d->autoRaiseHandle = 0;
    d->autoRaiseWindow = None;

    d->display = dpy = XOpenDisplay (name);
    if (!d->display)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Couldn't open display %s", XDisplayName (name));
	return FALSE;
    }

    if (!compInitDisplayOptionsFromMetadata (d,
					     &coreMetadata,
					     coreDisplayOptionInfo,
					     d->opt,
					     COMP_DISPLAY_OPTION_NUM))
	return FALSE;

    d->opt[COMP_DISPLAY_OPTION_ABI].value.i = CORE_ABIVERSION;

    snprintf (d->displayString, 255, "DISPLAY=%s", DisplayString (dpy));

#ifdef DEBUG
    XSynchronize (dpy, TRUE);
#endif

    XSetErrorHandler (errorHandler);

    updateModifierMappings (d);

    d->handleEvent	 = handleEvent;
    d->handleCompizEvent = handleCompizEvent;

    d->fileToImage = fileToImage;
    d->imageToFile = imageToFile;

    d->matchInitExp	      = matchInitExp;
    d->matchExpHandlerChanged = matchExpHandlerChanged;
    d->matchPropertyChanged   = matchPropertyChanged;

    d->supportedAtom	     = XInternAtom (dpy, "_NET_SUPPORTED", 0);
    d->supportingWmCheckAtom = XInternAtom (dpy, "_NET_SUPPORTING_WM_CHECK", 0);

    d->utf8StringAtom = XInternAtom (dpy, "UTF8_STRING", 0);

    d->wmNameAtom = XInternAtom (dpy, "_NET_WM_NAME", 0);

    d->winTypeAtom	  = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", 0);
    d->winTypeDesktopAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DESKTOP",
					 0);
    d->winTypeDockAtom    = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DOCK", 0);
    d->winTypeToolbarAtom = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR",
					 0);
    d->winTypeMenuAtom    = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_MENU", 0);
    d->winTypeUtilAtom    = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_UTILITY",
					 0);
    d->winTypeSplashAtom  = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_SPLASH", 0);
    d->winTypeDialogAtom  = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DIALOG", 0);
    d->winTypeNormalAtom  = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NORMAL", 0);

    d->winTypeDropdownMenuAtom =
	XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", 0);
    d->winTypePopupMenuAtom    =
	XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", 0);
    d->winTypeTooltipAtom      =
	XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_TOOLTIP", 0);
    d->winTypeNotificationAtom =
	XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", 0);
    d->winTypeComboAtom        =
	XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_COMBO", 0);
    d->winTypeDndAtom          =
	XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DND", 0);

    d->winOpacityAtom	 = XInternAtom (dpy, "_NET_WM_WINDOW_OPACITY", 0);
    d->winBrightnessAtom = XInternAtom (dpy, "_NET_WM_WINDOW_BRIGHTNESS", 0);
    d->winSaturationAtom = XInternAtom (dpy, "_NET_WM_WINDOW_SATURATION", 0);

    d->winActiveAtom = XInternAtom (dpy, "_NET_ACTIVE_WINDOW", 0);

    d->winDesktopAtom = XInternAtom (dpy, "_NET_WM_DESKTOP", 0);

    d->workareaAtom = XInternAtom (dpy, "_NET_WORKAREA", 0);

    d->desktopViewportAtom  = XInternAtom (dpy, "_NET_DESKTOP_VIEWPORT", 0);
    d->desktopGeometryAtom  = XInternAtom (dpy, "_NET_DESKTOP_GEOMETRY", 0);
    d->currentDesktopAtom   = XInternAtom (dpy, "_NET_CURRENT_DESKTOP", 0);
    d->numberOfDesktopsAtom = XInternAtom (dpy, "_NET_NUMBER_OF_DESKTOPS", 0);

    d->winStateAtom		    = XInternAtom (dpy, "_NET_WM_STATE", 0);
    d->winStateModalAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_MODAL", 0);
    d->winStateStickyAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_STICKY", 0);
    d->winStateMaximizedVertAtom    =
	XInternAtom (dpy, "_NET_WM_STATE_MAXIMIZED_VERT", 0);
    d->winStateMaximizedHorzAtom    =
	XInternAtom (dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", 0);
    d->winStateShadedAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_SHADED", 0);
    d->winStateSkipTaskbarAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_SKIP_TASKBAR", 0);
    d->winStateSkipPagerAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_SKIP_PAGER", 0);
    d->winStateHiddenAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_HIDDEN", 0);
    d->winStateFullscreenAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_FULLSCREEN", 0);
    d->winStateAboveAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_ABOVE", 0);
    d->winStateBelowAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_BELOW", 0);
    d->winStateDemandsAttentionAtom =
	XInternAtom (dpy, "_NET_WM_STATE_DEMANDS_ATTENTION", 0);
    d->winStateDisplayModalAtom	    =
	XInternAtom (dpy, "_NET_WM_STATE_DISPLAY_MODAL", 0);

    d->winActionMoveAtom	  = XInternAtom (dpy, "_NET_WM_ACTION_MOVE", 0);
    d->winActionResizeAtom	  =
	XInternAtom (dpy, "_NET_WM_ACTION_RESIZE", 0);
    d->winActionStickAtom	  =
	XInternAtom (dpy, "_NET_WM_ACTION_STICK", 0);
    d->winActionMinimizeAtom	  =
	XInternAtom (dpy, "_NET_WM_ACTION_MINIMIZE", 0);
    d->winActionMaximizeHorzAtom  =
	XInternAtom (dpy, "_NET_WM_ACTION_MAXIMIZE_HORZ", 0);
    d->winActionMaximizeVertAtom  =
	XInternAtom (dpy, "_NET_WM_ACTION_MAXIMIZE_VERT", 0);
    d->winActionFullscreenAtom	  =
	XInternAtom (dpy, "_NET_WM_ACTION_FULLSCREEN", 0);
    d->winActionCloseAtom	  =
	XInternAtom (dpy, "_NET_WM_ACTION_CLOSE", 0);
    d->winActionShadeAtom	  =
	XInternAtom (dpy, "_NET_WM_ACTION_SHADE", 0);
    d->winActionChangeDesktopAtom =
	XInternAtom (dpy, "_NET_WM_ACTION_CHANGE_DESKTOP", 0);
    d->winActionAboveAtom	  =
	XInternAtom (dpy, "_NET_WM_ACTION_ABOVE", 0);
    d->winActionBelowAtom	  =
	XInternAtom (dpy, "_NET_WM_ACTION_BELOW", 0);

    d->wmAllowedActionsAtom = XInternAtom (dpy, "_NET_WM_ALLOWED_ACTIONS", 0);

    d->wmStrutAtom	  = XInternAtom (dpy, "_NET_WM_STRUT", 0);
    d->wmStrutPartialAtom = XInternAtom (dpy, "_NET_WM_STRUT_PARTIAL", 0);

    d->wmUserTimeAtom = XInternAtom (dpy, "_NET_WM_USER_TIME", 0);

    d->wmIconAtom         = XInternAtom (dpy,"_NET_WM_ICON", 0);
    d->wmIconGeometryAtom = XInternAtom (dpy, "_NET_WM_ICON_GEOMETRY", 0);

    d->clientListAtom	      = XInternAtom (dpy, "_NET_CLIENT_LIST", 0);
    d->clientListStackingAtom =
	XInternAtom (dpy, "_NET_CLIENT_LIST_STACKING", 0);

    d->frameExtentsAtom = XInternAtom (dpy, "_NET_FRAME_EXTENTS", 0);
    d->frameWindowAtom  = XInternAtom (dpy, "_NET_FRAME_WINDOW", 0);

    d->wmStateAtom	  = XInternAtom (dpy, "WM_STATE", 0);
    d->wmChangeStateAtom  = XInternAtom (dpy, "WM_CHANGE_STATE", 0);
    d->wmProtocolsAtom	  = XInternAtom (dpy, "WM_PROTOCOLS", 0);
    d->wmClientLeaderAtom = XInternAtom (dpy, "WM_CLIENT_LEADER", 0);

    d->wmDeleteWindowAtom = XInternAtom (dpy, "WM_DELETE_WINDOW", 0);
    d->wmTakeFocusAtom	  = XInternAtom (dpy, "WM_TAKE_FOCUS", 0);
    d->wmPingAtom	  = XInternAtom (dpy, "_NET_WM_PING", 0);

    d->wmSyncRequestAtom  = XInternAtom (dpy, "_NET_WM_SYNC_REQUEST", 0);
    d->wmSyncRequestCounterAtom =
	XInternAtom (dpy, "_NET_WM_SYNC_REQUEST_COUNTER", 0);

    d->wmFullscreenMonitorsAtom =
	XInternAtom (dpy, "_NET_WM_FULLSCREEN_MONITORS", 0);

    d->closeWindowAtom	    = XInternAtom (dpy, "_NET_CLOSE_WINDOW", 0);
    d->wmMoveResizeAtom	    = XInternAtom (dpy, "_NET_WM_MOVERESIZE", 0);
    d->moveResizeWindowAtom = XInternAtom (dpy, "_NET_MOVERESIZE_WINDOW", 0);
    d->restackWindowAtom    = XInternAtom (dpy, "_NET_RESTACK_WINDOW", 0);

    d->showingDesktopAtom = XInternAtom (dpy, "_NET_SHOWING_DESKTOP", 0);

    d->xBackgroundAtom[0] = XInternAtom (dpy, "_XSETROOT_ID", 0);
    d->xBackgroundAtom[1] = XInternAtom (dpy, "_XROOTPMAP_ID", 0);

    d->toolkitActionAtom	  =
	XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION", 0);
    d->toolkitActionWindowMenuAtom  =
	XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION_WINDOW_MENU", 0);
    d->toolkitActionForceQuitDialogAtom  =
	XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION_FORCE_QUIT_DIALOG", 0);

    d->mwmHintsAtom = XInternAtom (dpy, "_MOTIF_WM_HINTS", 0);

    d->xdndAwareAtom    = XInternAtom (dpy, "XdndAware", 0);
    d->xdndEnterAtom    = XInternAtom (dpy, "XdndEnter", 0);
    d->xdndLeaveAtom    = XInternAtom (dpy, "XdndLeave", 0);
    d->xdndPositionAtom = XInternAtom (dpy, "XdndPosition", 0);
    d->xdndStatusAtom   = XInternAtom (dpy, "XdndStatus", 0);
    d->xdndDropAtom     = XInternAtom (dpy, "XdndDrop", 0);

    d->managerAtom   = XInternAtom (dpy, "MANAGER", 0);
    d->targetsAtom   = XInternAtom (dpy, "TARGETS", 0);
    d->multipleAtom  = XInternAtom (dpy, "MULTIPLE", 0);
    d->timestampAtom = XInternAtom (dpy, "TIMESTAMP", 0);
    d->versionAtom   = XInternAtom (dpy, "VERSION", 0);
    d->atomPairAtom  = XInternAtom (dpy, "ATOM_PAIR", 0);

    d->startupIdAtom = XInternAtom (dpy, "_NET_STARTUP_ID", 0);

    d->snDisplay = sn_display_new (dpy, NULL, NULL);
    if (!d->snDisplay)
	return FALSE;

    d->lastPing = 1;

    if (!XQueryExtension (dpy,
			  COMPOSITE_NAME,
			  &d->compositeOpcode,
			  &d->compositeEvent,
			  &d->compositeError))
    {
	compLogMessage ("core", CompLogLevelFatal,
			"No composite extension");
	return FALSE;
    }

    XCompositeQueryVersion (dpy, &compositeMajor, &compositeMinor);
    if (compositeMajor == 0 && compositeMinor < 2)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"Old composite extension");
	return FALSE;
    }

    if (!XDamageQueryExtension (dpy, &d->damageEvent, &d->damageError))
    {
	compLogMessage ("core", CompLogLevelFatal,
			"No damage extension");
	return FALSE;
    }

    if (!XSyncQueryExtension (dpy, &d->syncEvent, &d->syncError))
    {
	compLogMessage ("core", CompLogLevelFatal,
			"No sync extension");
	return FALSE;
    }

    if (!XFixesQueryExtension (dpy, &d->fixesEvent, &d->fixesError))
    {
	compLogMessage ("core", CompLogLevelFatal,
			"No fixes extension");
	return FALSE;
    }

    XFixesQueryVersion (dpy, &d->fixesVersion, &fixesMinor);
    /*
    if (d->fixesVersion < 5)
    {
	fprintf (stderr, "%s: Need fixes extension version 5 or later "
		 "for client-side cursor\n", programName);
    }
    */

    d->randrExtension = XRRQueryExtension (dpy,
					   &d->randrEvent,
					   &d->randrError);

    d->shapeExtension = XShapeQueryExtension (dpy,
					      &d->shapeEvent,
					      &d->shapeError);

    d->xkbExtension = XkbQueryExtension (dpy,
					 &xkbOpcode,
					 &d->xkbEvent,
					 &d->xkbError,
					 NULL, NULL);
    if (d->xkbExtension)
    {
	XkbSelectEvents (dpy,
			 XkbUseCoreKbd,
			 XkbBellNotifyMask | XkbStateNotifyMask,
			 XkbAllEventsMask);
    }
    else
    {
	compLogMessage ("core", CompLogLevelFatal,
			"No XKB extension");

	d->xkbEvent = d->xkbError = -1;
    }

    d->screenInfo  = NULL;
    d->nScreenInfo = 0;

    d->xineramaExtension = XineramaQueryExtension (dpy,
						   &d->xineramaEvent,
						   &d->xineramaError);

    if (d->xineramaExtension)
	d->screenInfo = XineramaQueryScreens (dpy, &d->nScreenInfo);

    d->escapeKeyCode = XKeysymToKeycode (dpy, XStringToKeysym ("Escape"));
    d->returnKeyCode = XKeysymToKeycode (dpy, XStringToKeysym ("Return"));

    addDisplayToCore (d);

    /* TODO: bailout properly when objectInitPlugins fails */
    assert (objectInitPlugins (&d->base));

    (*core.objectAdd) (&core.base, &d->base);

    if (onlyCurrentScreen)
    {
	firstScreen = DefaultScreen (dpy);
	lastScreen  = DefaultScreen (dpy);
    }
    else
    {
	firstScreen = 0;
	lastScreen  = ScreenCount (dpy) - 1;
    }

    for (i = firstScreen; i <= lastScreen; i++)
    {
	Window		     newWmSnOwner = None, newCmSnOwner = None;
	Atom		     wmSnAtom = 0, cmSnAtom = 0;
	Time		     wmSnTimestamp = 0;
	XEvent		     event;
	XSetWindowAttributes attr;
	Window		     currentWmSnOwner, currentCmSnOwner;
	char		     buf[128];
	Window		     rootDummy, childDummy;
	unsigned int	     uDummy;
	int		     x, y, dummy;

	sprintf (buf, "WM_S%d", i);
	wmSnAtom = XInternAtom (dpy, buf, 0);

	currentWmSnOwner = XGetSelectionOwner (dpy, wmSnAtom);

	if (currentWmSnOwner != None)
	{
	    if (!replaceCurrentWm)
	    {
		compLogMessage ("core", CompLogLevelError,
				"Screen %d on display \"%s\" already "
				"has a window manager; try using the "
				"--replace option to replace the current "
				"window manager.",
				i, DisplayString (dpy));

		continue;
	    }

	    XSelectInput (dpy, currentWmSnOwner,
			  StructureNotifyMask);
	}

	sprintf (buf, "_NET_WM_CM_S%d", i);
	cmSnAtom = XInternAtom (dpy, buf, 0);

	currentCmSnOwner = XGetSelectionOwner (dpy, cmSnAtom);

	if (currentCmSnOwner != None)
	{
	    if (!replaceCurrentWm)
	    {
		compLogMessage ("core", CompLogLevelError,
				"Screen %d on display \"%s\" already "
				"has a compositing manager; try using the "
				"--replace option to replace the current "
				"compositing manager.",
				i, DisplayString (dpy));

		continue;
	    }
	}

	attr.override_redirect = TRUE;
	attr.event_mask	       = PropertyChangeMask;

	newCmSnOwner = newWmSnOwner =
	    XCreateWindow (dpy, XRootWindow (dpy, i),
			   -100, -100, 1, 1, 0,
			   CopyFromParent, CopyFromParent,
			   CopyFromParent,
			   CWOverrideRedirect | CWEventMask,
			   &attr);

	XChangeProperty (dpy,
			 newWmSnOwner,
			 d->wmNameAtom,
			 d->utf8StringAtom, 8,
			 PropModeReplace,
			 (unsigned char *) PACKAGE,
			 strlen (PACKAGE));

	XWindowEvent (dpy,
		      newWmSnOwner,
		      PropertyChangeMask,
		      &event);

	wmSnTimestamp = event.xproperty.time;

	if (!aquireSelection (d, i, "window", wmSnAtom, newWmSnOwner,
			      wmSnTimestamp))
	{
	    XDestroyWindow (dpy, newWmSnOwner);

	    continue;
	}

	/* Wait for old window manager to go away */
	if (currentWmSnOwner != None)
	{
	    do {
		XWindowEvent (dpy, currentWmSnOwner,
			      StructureNotifyMask, &event);
	    } while (event.type != DestroyNotify);
	}

	compCheckForError (dpy);

	XCompositeRedirectSubwindows (dpy, XRootWindow (dpy, i),
				      CompositeRedirectManual);

	if (compCheckForError (dpy))
	{
	    compLogMessage ("core", CompLogLevelError,
			    "Another composite manager is already "
			    "running on screen: %d", i);

	    continue;
	}

	if (!aquireSelection (d, i, "compositing", cmSnAtom,
			      newCmSnOwner, wmSnTimestamp))
	{
	    continue;
	}

	XGrabServer (dpy);

	XSelectInput (dpy, XRootWindow (dpy, i),
		      SubstructureRedirectMask |
		      SubstructureNotifyMask   |
		      StructureNotifyMask      |
		      PropertyChangeMask       |
		      LeaveWindowMask	       |
		      EnterWindowMask	       |
		      KeyPressMask	       |
		      KeyReleaseMask	       |
		      ButtonPressMask	       |
		      ButtonReleaseMask	       |
		      FocusChangeMask	       |
		      ExposureMask);

	if (compCheckForError (dpy))
	{
	    compLogMessage ("core", CompLogLevelError,
			    "Another window manager is "
			    "already running on screen: %d", i);

	    XUngrabServer (dpy);
	    continue;
	}

	if (!addScreen (d, i, newWmSnOwner, wmSnAtom, wmSnTimestamp))
	{
	    compLogMessage ("core", CompLogLevelError,
			    "Failed to manage screen: %d", i);
	}

	if (XQueryPointer (dpy, XRootWindow (dpy, i),
			   &rootDummy, &childDummy,
			   &x, &y, &dummy, &dummy, &uDummy))
	{
	    lastPointerX = pointerX = x;
	    lastPointerY = pointerY = y;
	}

	XUngrabServer (dpy);
    }

    if (!d->screens)
    {
	compLogMessage ("core", CompLogLevelFatal,
			"No manageable screens found on display %s",
			XDisplayName (name));
	return FALSE;
    }

    setAudibleBell (d, d->opt[COMP_DISPLAY_OPTION_AUDIBLE_BELL].value.b);

    XGetInputFocus (dpy, &focus, &revertTo);

    /* move input focus to root window so that we get a FocusIn event when
       moving it to the default window */
    XSetInputFocus (dpy, d->screens->root, RevertToPointerRoot, CurrentTime);

    if (focus == None || focus == PointerRoot)
    {
	focusDefaultWindow (d->screens);
    }
    else
    {
	CompWindow *w;

	w = findWindowAtDisplay (d, focus);
	if (w)
	{
	    moveInputFocusToWindow (w);
	}
	else
	    focusDefaultWindow (d->screens);
    }

    d->pingHandle =
	compAddTimeout (d->opt[COMP_DISPLAY_OPTION_PING_DELAY].value.i,
			d->opt[COMP_DISPLAY_OPTION_PING_DELAY].value.i + 500,
			pingTimeout, d);

    return TRUE;
}

void
removeDisplay (CompDisplay *d)
{
    CompDisplay *p;

    for (p = core.displays; p; p = p->next)
	if (p->next == d)
	    break;

    if (p)
	p->next = d->next;
    else
	core.displays = NULL;

    while (d->screens)
	removeScreen (d->screens);

    (*core.objectRemove) (&core.base, &d->base);

    objectFiniPlugins (&d->base);

    if (d->edgeDelayHandle)
    {
	void *closure;

	closure = compRemoveTimeout (d->edgeDelayHandle);
	if (closure)
	    free (closure);
    }

    if (d->autoRaiseHandle)
	compRemoveTimeout (d->autoRaiseHandle);

    compRemoveTimeout (d->pingHandle);

    if (d->snDisplay)
	sn_display_unref (d->snDisplay);

    XSync (d->display, False);
    XCloseDisplay (d->display);

    freeDisplay (d);
}

Time
getCurrentTimeFromDisplay (CompDisplay *d)
{
    XEvent event;

    XChangeProperty (d->display, d->screens->grabWindow,
		     XA_PRIMARY, XA_STRING, 8,
		     PropModeAppend, NULL, 0);
    XWindowEvent (d->display, d->screens->grabWindow,
		  PropertyChangeMask,
		  &event);

    return event.xproperty.time;
}

CompScreen *
findScreenAtDisplay (CompDisplay *d,
		     Window      root)
{
    CompScreen *s;

    for (s = d->screens; s; s = s->next)
    {
	if (s->root == root)
	    return s;
    }

    return 0;
}

void
forEachWindowOnDisplay (CompDisplay	  *display,
			ForEachWindowProc proc,
			void		  *closure)
{
    CompScreen *s;

    for (s = display->screens; s; s = s->next)
	forEachWindowOnScreen (s, proc, closure);
}

CompWindow *
findWindowAtDisplay (CompDisplay *d,
		     Window      id)
{
    CompScreen *s;
    CompWindow *w;

    for (s = d->screens; s; s = s->next)
    {
	w = findWindowAtScreen (s, id);
	if (w)
	    return w;
    }

    return 0;
}

CompWindow *
findTopLevelWindowAtDisplay (CompDisplay *d,
			     Window      id)
{
    CompScreen *s;
    CompWindow *w;

    for (s = d->screens; s; s = s->next)
    {
	w = findTopLevelWindowAtScreen (s, id);
	if (w)
	    return w;
    }

    return 0;
}

static CompScreen *
findScreenForSelection (CompDisplay *display,
			Window       owner,
			Atom         selection)
{
    CompScreen *s;

    for (s = display->screens; s; s = s->next)
    {
	if (s->wmSnSelectionWindow == owner && s->wmSnAtom == selection)
	    return s;
    }

    return NULL;
}

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
static Bool
convertProperty (CompDisplay *display,
		 CompScreen  *screen,
		 Window      w,
		 Atom        target,
		 Atom        property)
{

#define N_TARGETS 4

    Atom conversionTargets[N_TARGETS];
    long icccmVersion[] = { 2, 0 };

    conversionTargets[0] = display->targetsAtom;
    conversionTargets[1] = display->multipleAtom;
    conversionTargets[2] = display->timestampAtom;
    conversionTargets[3] = display->versionAtom;

    if (target == display->targetsAtom)
	XChangeProperty (display->display, w, property,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) conversionTargets, N_TARGETS);
    else if (target == display->timestampAtom)
	XChangeProperty (display->display, w, property,
			 XA_INTEGER, 32, PropModeReplace,
			 (unsigned char *) &screen->wmSnTimestamp, 1);
    else if (target == display->versionAtom)
	XChangeProperty (display->display, w, property,
			 XA_INTEGER, 32, PropModeReplace,
			 (unsigned char *) icccmVersion, 2);
    else
	return FALSE;

    /* Be sure the PropertyNotify has arrived so we
     * can send SelectionNotify
     */
    XSync (display->display, FALSE);

    return TRUE;
}

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
void
handleSelectionRequest (CompDisplay *display,
			XEvent      *event)
{
    XSelectionEvent reply;
    CompScreen      *screen;

    screen = findScreenForSelection (display,
				     event->xselectionrequest.owner,
				     event->xselectionrequest.selection);
    if (!screen)
	return;

    reply.type	    = SelectionNotify;
    reply.display   = display->display;
    reply.requestor = event->xselectionrequest.requestor;
    reply.selection = event->xselectionrequest.selection;
    reply.target    = event->xselectionrequest.target;
    reply.property  = None;
    reply.time	    = event->xselectionrequest.time;

    if (event->xselectionrequest.target == display->multipleAtom)
    {
	if (event->xselectionrequest.property != None)
	{
	    Atom	  type, *adata;
	    int		  i, format;
	    unsigned long num, rest;
	    unsigned char *data;

	    if (XGetWindowProperty (display->display,
				    event->xselectionrequest.requestor,
				    event->xselectionrequest.property,
				    0, 256, FALSE,
				    display->atomPairAtom,
				    &type, &format, &num, &rest,
				    &data) != Success)
		return;

	    /* FIXME: to be 100% correct, should deal with rest > 0,
	     * but since we have 4 possible targets, we will hardly ever
	     * meet multiple requests with a length > 8
	     */
	    adata = (Atom *) data;
	    i = 0;
	    while (i < (int) num)
	    {
		if (!convertProperty (display, screen,
				      event->xselectionrequest.requestor,
				      adata[i], adata[i + 1]))
		    adata[i + 1] = None;

		i += 2;
	    }

	    XChangeProperty (display->display,
			     event->xselectionrequest.requestor,
			     event->xselectionrequest.property,
			     display->atomPairAtom,
			     32, PropModeReplace, data, num);

	    if (data)
		XFree (data);
	}
    }
    else
    {
	if (event->xselectionrequest.property == None)
	    event->xselectionrequest.property = event->xselectionrequest.target;

	if (convertProperty (display, screen,
			     event->xselectionrequest.requestor,
			     event->xselectionrequest.target,
			     event->xselectionrequest.property))
	    reply.property = event->xselectionrequest.property;
    }

    XSendEvent (display->display,
		event->xselectionrequest.requestor,
		FALSE, 0L, (XEvent *) &reply);
}

void
handleSelectionClear (CompDisplay *display,
		      XEvent      *event)
{
    /* We need to unmanage the screen on which we lost the selection */
    CompScreen *screen;

    screen = findScreenForSelection (display,
				     event->xselectionclear.window,
				     event->xselectionclear.selection);

    if (screen)
	shutDown = TRUE;
}

void
warpPointer (CompScreen *s,
	     int	 dx,
	     int	 dy)
{
    CompDisplay *display = s->display;
    XEvent      event;

    pointerX += dx;
    pointerY += dy;

    if (pointerX >= s->width)
	pointerX = s->width - 1;
    else if (pointerX < 0)
	pointerX = 0;

    if (pointerY >= s->height)
	pointerY = s->height - 1;
    else if (pointerY < 0)
	pointerY = 0;

    XWarpPointer (display->display,
		  None, s->root,
		  0, 0, 0, 0,
		  pointerX, pointerY);

    XSync (display->display, FALSE);

    while (XCheckMaskEvent (display->display,
			    LeaveWindowMask |
			    EnterWindowMask |
			    PointerMotionMask,
			    &event));

    if (!inHandleEvent)
    {
	lastPointerX = pointerX;
	lastPointerY = pointerY;
    }
}

Bool
setDisplayAction (CompDisplay     *display,
		  CompOption      *o,
		  CompOptionValue *value)
{
    CompScreen *s;

    for (s = display->screens; s; s = s->next)
	if (!addScreenAction (s, &value->action))
	    break;

    if (s)
    {
	CompScreen *failed = s;

	for (s = display->screens; s && s != failed; s = s->next)
	    removeScreenAction (s, &value->action);

	return FALSE;
    }
    else
    {
	for (s = display->screens; s; s = s->next)
	    removeScreenAction (s, &o->value.action);
    }

    if (compSetActionOption (o, value))
	return TRUE;

    return FALSE;
}

void
clearTargetOutput (CompDisplay	*display,
		   unsigned int mask)
{
    if (targetScreen)
	clearScreenOutput (targetScreen,
			   targetOutput,
			   mask);
}

#define HOME_IMAGEDIR ".compiz/images"

Bool
readImageFromFile (CompDisplay *display,
		   const char  *name,
		   int	       *width,
		   int	       *height,
		   void	       **data)
{
    Bool status;
    int  stride;

    status = (*display->fileToImage) (display, NULL, name, width, height,
				      &stride, data);
    if (!status)
    {
	char *home;

	home = getenv ("HOME");
	if (home)
	{
	    char *path;

	    path = malloc (strlen (home) + strlen (HOME_IMAGEDIR) + 2);
	    if (path)
	    {
		sprintf (path, "%s/%s", home, HOME_IMAGEDIR);
		status = (*display->fileToImage) (display, path, name,
						  width, height, &stride,
						  data);

		free (path);

		if (status)
		    return TRUE;
	    }
	}

	status = (*display->fileToImage) (display, IMAGEDIR, name,
					  width, height, &stride, data);
    }

    return status;
}

Bool
writeImageToFile (CompDisplay *display,
		  const char  *path,
		  const char  *name,
		  const char  *format,
		  int	      width,
		  int	      height,
		  void	      *data)
{
    return (*display->imageToFile) (display, path, name, format, width, height,
				    width * 4, data);
}

Bool
fileToImage (CompDisplay *display,
	     const char	 *path,
	     const char	 *name,
	     int	 *width,
	     int	 *height,
	     int	 *stride,
	     void	 **data)
{
    return FALSE;
}

Bool
imageToFile (CompDisplay *display,
	     const char	 *path,
	     const char	 *name,
	     const char	 *format,
	     int	 width,
	     int	 height,
	     int	 stride,
	     void	 *data)
{
    return FALSE;
}

CompCursor *
findCursorAtDisplay (CompDisplay *display)
{
    CompScreen *s;

    for (s = display->screens; s; s = s->next)
	if (s->cursors)
	    return s->cursors;

    return NULL;
}
