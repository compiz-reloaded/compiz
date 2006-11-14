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

#define _GNU_SOURCE /* for asprintf */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>

#include <compiz.h>

static unsigned int virtualModMask[] = {
    CompAltMask, CompMetaMask, CompSuperMask, CompHyperMask,
    CompModeSwitchMask, CompNumLockMask, CompScrollLockMask
};

typedef struct _CompTimeout {
    struct _CompTimeout *next;
    int			time;
    int			left;
    CallBackProc	callBack;
    void		*closure;
    CompTimeoutHandle   handle;
} CompTimeout;

static CompTimeout       *timeouts = 0;
static struct timeval    lastTimeout;
static CompTimeoutHandle lastTimeoutHandle = 1;

typedef struct _CompWatchFd {
    struct _CompWatchFd *next;
    int			fd;
    CallBackProc	callBack;
    void		*closure;
    CompWatchFdHandle   handle;
} CompWatchFd;

static CompWatchFd       *watchFds = 0;
static CompWatchFdHandle lastWatchFdHandle = 1;
static struct pollfd     *watchPollFds = 0;
static int               nWatchFds = 0;

static CompScreen *targetScreen = NULL;
static int        targetOutput = 0;

static Bool inHandleEvent = FALSE;

static Bool shutDown = FALSE;

int lastPointerX = 0;
int lastPointerY = 0;
int pointerX     = 0;
int pointerY     = 0;

#define CLICK_TO_FOCUS_DEFAULT TRUE

#define AUTORAISE_DEFAULT TRUE

#define AUTORAISE_DELAY_DEFAULT 1000
#define AUTORAISE_DELAY_MIN	0
#define AUTORAISE_DELAY_MAX	10000

#define SLOW_ANIMATIONS_KEY_DEFAULT       "F10"
#define SLOW_ANIMATIONS_MODIFIERS_DEFAULT ShiftMask

#define MAIN_MENU_KEY_DEFAULT       "F1"
#define MAIN_MENU_MODIFIERS_DEFAULT CompAltMask

#define RUN_DIALOG_KEY_DEFAULT       "F2"
#define RUN_DIALOG_MODIFIERS_DEFAULT CompAltMask

#define CLOSE_WINDOW_KEY_DEFAULT       "F4"
#define CLOSE_WINDOW_MODIFIERS_DEFAULT CompAltMask

#define UNMAXIMIZE_WINDOW_KEY_DEFAULT       "F5"
#define UNMAXIMIZE_WINDOW_MODIFIERS_DEFAULT CompAltMask

#define MINIMIZE_WINDOW_KEY_DEFAULT       "F9"
#define MINIMIZE_WINDOW_MODIFIERS_DEFAULT CompAltMask

#define MAXIMIZE_WINDOW_KEY_DEFAULT       "F10"
#define MAXIMIZE_WINDOW_MODIFIERS_DEFAULT CompAltMask

#define RAISE_WINDOW_BUTTON_DEFAULT    6
#define RAISE_WINDOW_MODIFIERS_DEFAULT ControlMask

#define LOWER_WINDOW_BUTTON_DEFAULT    6
#define LOWER_WINDOW_MODIFIERS_DEFAULT CompAltMask

#define SHOW_DESKTOP_KEY_DEFAULT       "d"
#define SHOW_DESKTOP_MODIFIERS_DEFAULT (CompAltMask | ControlMask)

#define OPACITY_INCREASE_BUTTON_DEFAULT    Button4
#define OPACITY_INCREASE_MODIFIERS_DEFAULT CompAltMask

#define OPACITY_DECREASE_BUTTON_DEFAULT    Button5
#define OPACITY_DECREASE_MODIFIERS_DEFAULT CompAltMask

#define SCREENSHOT_DEFAULT               "gnome-screenshot"
#define RUN_SCREENSHOT_KEY_DEFAULT       "Print"
#define RUN_SCREENSHOT_MODIFIERS_DEFAULT 0

#define WINDOW_SCREENSHOT_DEFAULT               "gnome-screenshot --window"
#define RUN_WINDOW_SCREENSHOT_KEY_DEFAULT       "Print"
#define RUN_WINDOW_SCREENSHOT_MODIFIERS_DEFAULT CompAltMask

#define WINDOW_MENU_BUTTON_DEFAULT    Button3
#define WINDOW_MENU_KEY_DEFAULT       "space"
#define WINDOW_MENU_MODIFIERS_DEFAULT CompAltMask

#define RAISE_ON_CLICK_DEFAULT TRUE

#define AUDIBLE_BELL_DEFAULT TRUE

#define HIDE_SKIP_TASKBAR_WINDOWS_DEFAULT TRUE

#define TOGGLE_WINDOW_SHADING_KEY_DEFAULT       "s"
#define TOGGLE_WINDOW_SHADING_MODIFIERS_DEFAULT (CompAltMask | ControlMask)

#define IGNORE_HINTS_WHEN_MAXIMIZED_DEFAULT TRUE

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static char *textureFilter[] = { N_("Fast"), N_("Good"), N_("Best") };

#define NUM_TEXTURE_FILTER (sizeof (textureFilter) / sizeof (textureFilter[0]))

CompDisplay *compDisplays = 0;

static CompDisplay compDisplay;

static char *displayPrivateIndices = 0;
static int  displayPrivateLen = 0;

static int
reallocDisplayPrivate (int  size,
		       void *closure)
{
    CompDisplay *d = compDisplays;
    void        *privates;

    if (d)
    {
	privates = realloc (d->privates, size * sizeof (CompPrivate));
	if (!privates)
	    return FALSE;

	d->privates = (CompPrivate *) privates;
    }

    return TRUE;
}

int
allocateDisplayPrivateIndex (void)
{
    return allocatePrivateIndex (&displayPrivateLen,
				 &displayPrivateIndices,
				 reallocDisplayPrivate,
				 0);
}

void
freeDisplayPrivateIndex (int index)
{
    freePrivateIndex (displayPrivateLen, displayPrivateIndices, index);
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
    if (w)
	closeWindow (w, time);

    return TRUE;
}

static Bool
mainMenu (CompDisplay     *d,
	  CompAction      *action,
	  CompActionState state,
	  CompOption      *option,
	  int		  nOption)
{
    CompScreen   *s;
    Window       xid;
    unsigned int time;

    xid  = getIntOptionNamed (option, nOption, "root", 0);
    time = getIntOptionNamed (option, nOption, "time", CurrentTime);

    s = findScreenAtDisplay (d, xid);
    if (s && !s->maxGrab)
	toolkitAction (s, s->display->toolkitActionMainMenuAtom, time, s->root,
		       0, 0, 0);

    return TRUE;
}

static Bool
runDialog (CompDisplay     *d,
	   CompAction      *action,
	   CompActionState state,
	   CompOption      *option,
	   int		   nOption)
{
    CompScreen   *s;
    Window       xid;
    unsigned int time;

    xid  = getIntOptionNamed (option, nOption, "root", 0);
    time = getIntOptionNamed (option, nOption, "time", CurrentTime);

    s = findScreenAtDisplay (d, xid);
    if (s && !s->maxGrab)
	toolkitAction (s, s->display->toolkitActionRunDialogAtom, time, s->root,
		       0, 0, 0);

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
    if (w)
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
	    enterShowDesktopMode (s);
	else
	    leaveShowDesktopMode (s, NULL);
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
raise (CompDisplay     *d,
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
lower (CompDisplay     *d,
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

static Bool
increaseOpacity (CompDisplay     *d,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int	         nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	changeWindowOpacity (w, 1);

    return TRUE;
}

static Bool
decreaseOpacity (CompDisplay     *d,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int	         nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findTopLevelWindowAtDisplay (d, xid);
    if (w)
	changeWindowOpacity (w, -1);

    return TRUE;
}

static Bool
runCommandDispatch (CompDisplay     *d,
		    CompAction      *action,
		    CompActionState state,
		    CompOption      *option,
		    int		    nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	int index = -1;
	int i = COMP_DISPLAY_OPTION_RUN_COMMAND0;

	while (i <= COMP_DISPLAY_OPTION_RUN_COMMAND11)
	{
	    if (action == &d->opt[i].value.action)
	    {
		index = i - COMP_DISPLAY_OPTION_RUN_COMMAND0 +
		    COMP_DISPLAY_OPTION_COMMAND0;
		break;
	    }

	    i++;
	}

	if (index > 0)
	    runCommand (s, d->opt[index].value.s);
    }

    return TRUE;
}

static Bool
runCommandScreenshot (CompDisplay     *d,
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
	runCommand (s, d->opt[COMP_DISPLAY_OPTION_SCREENSHOT].value.s);

    return TRUE;
}

static Bool
runCommandWindowScreenshot (CompDisplay     *d,
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
	runCommand (s, d->opt[COMP_DISPLAY_OPTION_WINDOW_SCREENSHOT].value.s);

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
    if (w)
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
	updateWindowAttributes (w, FALSE);
    }

    return TRUE;
}

static void
compDisplayInitOptions (CompDisplay *display,
			char	    **plugin,
			int	    nPlugin)
{
    CompOption *o;
    int        i;
    char       *str;

    o = &display->opt[COMP_DISPLAY_OPTION_ACTIVE_PLUGINS];
    o->name	         = "active_plugins";
    o->shortDesc         = N_("Active Plugins");
    o->longDesc	         = N_("List of currently active plugins");
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = nPlugin;
    o->value.list.value  = malloc (sizeof (CompOptionValue) * nPlugin);
    for (i = 0; i < nPlugin; i++)
	o->value.list.value[i].s = strdup (plugin[i]);
    o->rest.s.string     = 0;
    o->rest.s.nString    = 0;

    display->dirtyPluginList = TRUE;

    o = &display->opt[COMP_DISPLAY_OPTION_TEXTURE_FILTER];
    o->name	      = "texture_filter";
    o->shortDesc      = N_("Texture Filter");
    o->longDesc	      = N_("Texture filtering");
    o->type	      = CompOptionTypeString;
    o->value.s	      = strdup (defaultTextureFilter);
    o->rest.s.string  = textureFilter;
    o->rest.s.nString = NUM_TEXTURE_FILTER;

    o = &display->opt[COMP_DISPLAY_OPTION_CLICK_TO_FOCUS];
    o->name	      = "click_to_focus";
    o->shortDesc      = N_("Click To Focus");
    o->longDesc	      = N_("Click on window moves input focus to it");
    o->type	      = CompOptionTypeBool;
    o->value.b	      = CLICK_TO_FOCUS_DEFAULT;

    o = &display->opt[COMP_DISPLAY_OPTION_AUTORAISE];
    o->name	      = "autoraise";
    o->shortDesc      = N_("Auto-Raise");
    o->longDesc	      = N_("Raise selected windows after interval");
    o->type	      = CompOptionTypeBool;
    o->value.b	      = AUTORAISE_DEFAULT;

    o = &display->opt[COMP_DISPLAY_OPTION_AUTORAISE_DELAY];
    o->name	  = "autoraise_delay";
    o->shortDesc  = N_("Auto-Raise Delay");
    o->longDesc	  = N_("Interval before raising selected windows");
    o->type	  = CompOptionTypeInt;
    o->value.i	  = AUTORAISE_DELAY_DEFAULT;
    o->rest.i.min = AUTORAISE_DELAY_MIN;
    o->rest.i.max = AUTORAISE_DELAY_MAX;

    o = &display->opt[COMP_DISPLAY_OPTION_CLOSE_WINDOW];
    o->name			  = "close_window";
    o->shortDesc		  = N_("Close Window");
    o->longDesc			  = N_("Close active window");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = closeWin;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = CLOSE_WINDOW_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (CLOSE_WINDOW_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_MAIN_MENU];
    o->name			  = "main_menu";
    o->shortDesc		  = N_("Show Main Menu");
    o->longDesc			  = N_("Show the main menu");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = mainMenu;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = MAIN_MENU_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (MAIN_MENU_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_RUN_DIALOG];
    o->name			  = "run";
    o->shortDesc		  = N_("Run Dialog");
    o->longDesc			  = N_("Show Run Application dialog");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = runDialog;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = RUN_DIALOG_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (RUN_DIALOG_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_UNMAXIMIZE_WINDOW];
    o->name			  = "unmaximize_window";
    o->shortDesc		  = N_("Unmaximize Window");
    o->longDesc			  = N_("Unmaximize active window");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = unmaximize;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = UNMAXIMIZE_WINDOW_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (UNMAXIMIZE_WINDOW_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_MINIMIZE_WINDOW];
    o->name			  = "minimize_window";
    o->shortDesc		  = N_("Minimize Window");
    o->longDesc			  = N_("Minimize active window");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = minimize;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = MINIMIZE_WINDOW_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (MINIMIZE_WINDOW_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW];
    o->name			  = "maximize_window";
    o->shortDesc		  = N_("Maximize Window");
    o->longDesc			  = N_("Maximize active window");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = maximize;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = MAXIMIZE_WINDOW_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (MAXIMIZE_WINDOW_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW_HORZ];
    o->name			  = "maximize_window_horizontally";
    o->shortDesc		  = N_("Maximize Window Horizontally");
    o->longDesc			  = N_("Maximize active window horizontally");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = maximizeHorizontally;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeNone;

    o = &display->opt[COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW_VERT];
    o->name			  = "maximize_window_vertically";
    o->shortDesc		  = N_("Maximize Window Vertically");
    o->longDesc			  = N_("Maximize active window vertically");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = maximizeVertically;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeNone;

    o = &display->opt[COMP_DISPLAY_OPTION_SHOW_DESKTOP];
    o->name			  = "show_desktop";
    o->shortDesc		  = N_("Hide all windows and focus desktop");
    o->longDesc			  = N_("Hide all windows and focus desktop");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = showDesktop;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = SHOW_DESKTOP_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (SHOW_DESKTOP_KEY_DEFAULT));

#define COMMAND_OPTION_SHORT N_("Command line %d")
#define COMMAND_OPTION_LONG  N_("Command line to be executed in shell when " \
				"run_command%d is invoked")
#define RUN_OPTION_SHORT     N_("Run command %d")
#define RUN_OPTION_LONG      N_("A keybinding that when invoked, will run " \
				"the shell command identified by command%d")

#define COMMAND_OPTION(num, cname, rname)				    \
    o = &display->opt[COMP_DISPLAY_OPTION_COMMAND ## num ];		    \
    o->name			  = cname;				    \
    asprintf (&str, COMMAND_OPTION_SHORT, num);				    \
    o->shortDesc		  = str;				    \
    asprintf (&str, COMMAND_OPTION_LONG, num);				    \
    o->longDesc			  = str;				    \
    o->type			  = CompOptionTypeString;		    \
    o->value.s			  = strdup ("");			    \
    o->rest.s.string		  = NULL;				    \
    o->rest.s.nString		  = 0;					    \
    o = &display->opt[COMP_DISPLAY_OPTION_RUN_COMMAND ## num ];		    \
    o->name			  =  rname;				    \
    asprintf (&str, RUN_OPTION_SHORT, num);				    \
    o->shortDesc		  = str;				    \
    asprintf (&str, RUN_OPTION_LONG, num);				    \
    o->longDesc			  = str;				    \
    o->type		          = CompOptionTypeAction;		    \
    o->value.action.initiate      = runCommandDispatch;			    \
    o->value.action.terminate     = 0;					    \
    o->value.action.bell          = FALSE;				    \
    o->value.action.edgeMask	  = 0;					    \
    o->value.action.state	  = CompActionStateInitKey;		    \
    o->value.action.state	 |= CompActionStateInitButton;		    \
    o->value.action.type	  = CompBindingTypeNone

    COMMAND_OPTION (0, "command0", "run_command0");
    COMMAND_OPTION (1, "command1", "run_command1");
    COMMAND_OPTION (2, "command2", "run_command2");
    COMMAND_OPTION (3, "command3", "run_command3");
    COMMAND_OPTION (4, "command4", "run_command4");
    COMMAND_OPTION (5, "command5", "run_command5");
    COMMAND_OPTION (6, "command6", "run_command6");
    COMMAND_OPTION (7, "command7", "run_command7");
    COMMAND_OPTION (8, "command8", "run_command8");
    COMMAND_OPTION (9, "command9", "run_command9");
    COMMAND_OPTION (10, "command10", "run_command10");
    COMMAND_OPTION (11, "command11", "run_command11");

    o = &display->opt[COMP_DISPLAY_OPTION_SLOW_ANIMATIONS];
    o->name			  = "slow_animations";
    o->shortDesc		  = N_("Slow Animations");
    o->longDesc			  = N_("Toggle use of slow animations");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = toggleSlowAnimations;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = SLOW_ANIMATIONS_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (SLOW_ANIMATIONS_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_RAISE_WINDOW];
    o->name			     = "raise_window";
    o->shortDesc		     = N_("Raise Window");
    o->longDesc			     = N_("Raise window above other windows");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = raise;
    o->value.action.terminate        = 0;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.state	     = CompActionStateInitKey;
    o->value.action.state	    |= CompActionStateInitButton;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.button.modifiers = RAISE_WINDOW_MODIFIERS_DEFAULT;
    o->value.action.button.button    = RAISE_WINDOW_BUTTON_DEFAULT;

    o = &display->opt[COMP_DISPLAY_OPTION_LOWER_WINDOW];
    o->name			     = "lower_window";
    o->shortDesc		     = N_("Lower Window");
    o->longDesc			     = N_("Lower window beneath other windows");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = lower;
    o->value.action.terminate        = 0;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.state	     = CompActionStateInitKey;
    o->value.action.state	    |= CompActionStateInitButton;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.button.modifiers = LOWER_WINDOW_MODIFIERS_DEFAULT;
    o->value.action.button.button    = LOWER_WINDOW_BUTTON_DEFAULT;

    o = &display->opt[COMP_DISPLAY_OPTION_OPACITY_INCREASE];
    o->name			     = "opacity_increase";
    o->shortDesc		     = N_("Increase Opacity");
    o->longDesc			     = N_("Increase window opacity");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = increaseOpacity;
    o->value.action.terminate        = 0;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.state	     = CompActionStateInitKey;
    o->value.action.state	    |= CompActionStateInitButton;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.button.modifiers = OPACITY_INCREASE_MODIFIERS_DEFAULT;
    o->value.action.button.button    = OPACITY_INCREASE_BUTTON_DEFAULT;

    o = &display->opt[COMP_DISPLAY_OPTION_OPACITY_DECREASE];
    o->name			     = "opacity_decrease";
    o->shortDesc		     = N_("Decrease Opacity");
    o->longDesc			     = N_("Decrease window opacity");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = decreaseOpacity;
    o->value.action.terminate        = 0;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.state	     = CompActionStateInitKey;
    o->value.action.state	    |= CompActionStateInitButton;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.button.modifiers = OPACITY_DECREASE_MODIFIERS_DEFAULT;
    o->value.action.button.button    = OPACITY_DECREASE_BUTTON_DEFAULT;

    o = &display->opt[COMP_DISPLAY_OPTION_RUN_SCREENSHOT];
    o->name			  = "run_command_screenshot";
    o->shortDesc		  = N_("Take a screenshot");
    o->longDesc			  = N_("Take a screenshot");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = runCommandScreenshot;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = RUN_SCREENSHOT_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (RUN_SCREENSHOT_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_SCREENSHOT];
    o->name			  = "command_screenshot";
    o->shortDesc		  = N_("Screenshot command line");
    o->longDesc			  = N_("Screenshot command line");
    o->type			  = CompOptionTypeString;
    o->value.s			  = strdup (SCREENSHOT_DEFAULT);
    o->rest.s.string		  = NULL;
    o->rest.s.nString		  = 0;

    o = &display->opt[COMP_DISPLAY_OPTION_RUN_WINDOW_SCREENSHOT];
    o->name			  = "run_command_window_screenshot";
    o->shortDesc		  = N_("Take a screenshot of a window");
    o->longDesc			  = N_("Take a screenshot of a window");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = runCommandWindowScreenshot;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers =
	RUN_WINDOW_SCREENSHOT_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (RUN_WINDOW_SCREENSHOT_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_WINDOW_SCREENSHOT];
    o->name			  = "command_window_screenshot";
    o->shortDesc		  = N_("Window screenshot command line");
    o->longDesc			  = N_("Window screenshot command line");
    o->type			  = CompOptionTypeString;
    o->value.s			  = strdup (WINDOW_SCREENSHOT_DEFAULT);
    o->rest.s.string		  = NULL;
    o->rest.s.nString		  = 0;

    o = &display->opt[COMP_DISPLAY_OPTION_WINDOW_MENU];
    o->name			     = "window_menu";
    o->shortDesc		     = N_("Window Menu");
    o->longDesc			     = N_("Open window menu");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = windowMenu;
    o->value.action.terminate        = 0;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.state	     = CompActionStateInitKey;
    o->value.action.state	    |= CompActionStateInitButton;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.button.modifiers = WINDOW_MENU_MODIFIERS_DEFAULT;
    o->value.action.button.button    = WINDOW_MENU_BUTTON_DEFAULT;
    o->value.action.type	    |= CompBindingTypeKey;
    o->value.action.key.modifiers    = WINDOW_MENU_MODIFIERS_DEFAULT;
    o->value.action.key.keycode      =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (WINDOW_MENU_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_RAISE_ON_CLICK];
    o->name	      = "raise_on_click";
    o->shortDesc      = N_("Raise On Click");
    o->longDesc	      = N_("Raise windows when clicked");
    o->type	      = CompOptionTypeBool;
    o->value.b	      = RAISE_ON_CLICK_DEFAULT;

    o = &display->opt[COMP_DISPLAY_OPTION_AUDIBLE_BELL];
    o->name	      = "audible_bell";
    o->shortDesc      = N_("Audible Bell");
    o->longDesc	      = N_("Audible system beep");
    o->type	      = CompOptionTypeBool;
    o->value.b	      = AUDIBLE_BELL_DEFAULT;

    o = &display->opt[COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED];
    o->name			  = "toggle_window_maximized";
    o->shortDesc		  = N_("Toggle Window Maximized");
    o->longDesc			  = N_("Toggle active window maximized");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = toggleMaximized;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeNone;

    o = &display->opt[COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED_HORZ];
    o->name			  = "toggle_window_maximized_horizontally";
    o->shortDesc		  = N_("Toggle Window Maximized Horizontally");
    o->longDesc			  =
	N_("Toggle active window maximized horizontally");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = toggleMaximizedHorizontally;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeNone;

    o = &display->opt[COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED_VERT];
    o->name			  = "toggle_window_maximized_vertically";
    o->shortDesc		  = N_("Toggle Window Maximized Vertically");
    o->longDesc			  =
	N_("Toggle active window maximized vertically");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = toggleMaximizedVertically;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeNone;

    o = &display->opt[COMP_DISPLAY_OPTION_HIDE_SKIP_TASKBAR_WINDOWS];
    o->name	 = "hide_skip_taskbar_windows";
    o->shortDesc = N_("Hide Skip Taskbar Windows");
    o->longDesc	 = N_("Hide windows not in taskbar when entering show "
	"desktop mode");
    o->type	 = CompOptionTypeBool;
    o->value.b	 = HIDE_SKIP_TASKBAR_WINDOWS_DEFAULT;

    o = &display->opt[COMP_DISPLAY_OPTION_TOGGLE_WINDOW_SHADED];
    o->name			  = "toggle_window_shaded";
    o->shortDesc		  = N_("Toggle Window Shaded");
    o->longDesc			  = N_("Toggle active window shaded");
    o->type		          = CompOptionTypeAction;
    o->value.action.initiate      = shade;
    o->value.action.terminate     = 0;
    o->value.action.bell          = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = TOGGLE_WINDOW_SHADING_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display->display,
			  XStringToKeysym (TOGGLE_WINDOW_SHADING_KEY_DEFAULT));

    o = &display->opt[COMP_DISPLAY_OPTION_IGNORE_HINTS_WHEN_MAXIMIZED];
    o->name	 = "ignore_hints_when_maximized";
    o->shortDesc = N_("Ignore Hints When Maximized");
    o->longDesc	 = N_("Ignore size increment and aspect hints when window is "
	"maximized");
    o->type	 = CompOptionTypeBool;
    o->value.b	 = IGNORE_HINTS_WHEN_MAXIMIZED_DEFAULT;
}

CompOption *
compGetDisplayOptions (CompDisplay *display,
		       int	   *count)
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
setDisplayOption (CompDisplay     *display,
		  char	          *name,
		  CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    o = compFindOption (display->opt, NUM_OPTIONS (display), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case COMP_DISPLAY_OPTION_ACTIVE_PLUGINS:
	if (compSetOptionList (o, value))
	{
	    display->dirtyPluginList = TRUE;
	    return TRUE;
	}
	break;
    case COMP_DISPLAY_OPTION_TEXTURE_FILTER:
	if (compSetStringOption (o, value))
	{
	    CompScreen *s;

	    for (s = display->screens; s; s = s->next)
		damageScreen (s);

	    if (strcmp (o->value.s, "Fast") == 0)
		display->textureFilter = GL_NEAREST;
	    else
		display->textureFilter = GL_LINEAR;

	    return TRUE;
	}
	break;
    case COMP_DISPLAY_OPTION_CLICK_TO_FOCUS:
    case COMP_DISPLAY_OPTION_AUTORAISE:
    case COMP_DISPLAY_OPTION_RAISE_ON_CLICK:
    case COMP_DISPLAY_OPTION_HIDE_SKIP_TASKBAR_WINDOWS:
    case COMP_DISPLAY_OPTION_IGNORE_HINTS_WHEN_MAXIMIZED:
	if (compSetBoolOption (o, value))
	    return TRUE;
	break;
    case COMP_DISPLAY_OPTION_AUTORAISE_DELAY:
	if (compSetIntOption (o, value))
	    return TRUE;
	break;
    case COMP_DISPLAY_OPTION_COMMAND0:
    case COMP_DISPLAY_OPTION_COMMAND1:
    case COMP_DISPLAY_OPTION_COMMAND2:
    case COMP_DISPLAY_OPTION_COMMAND3:
    case COMP_DISPLAY_OPTION_COMMAND4:
    case COMP_DISPLAY_OPTION_COMMAND5:
    case COMP_DISPLAY_OPTION_COMMAND6:
    case COMP_DISPLAY_OPTION_COMMAND7:
    case COMP_DISPLAY_OPTION_COMMAND8:
    case COMP_DISPLAY_OPTION_COMMAND9:
    case COMP_DISPLAY_OPTION_COMMAND10:
    case COMP_DISPLAY_OPTION_COMMAND11:
    case COMP_DISPLAY_OPTION_SCREENSHOT:
    case COMP_DISPLAY_OPTION_WINDOW_SCREENSHOT:
	if (compSetStringOption (o, value))
	    return TRUE;
	break;
    case COMP_DISPLAY_OPTION_CLOSE_WINDOW:
    case COMP_DISPLAY_OPTION_MAIN_MENU:
    case COMP_DISPLAY_OPTION_RUN_DIALOG:
    case COMP_DISPLAY_OPTION_MINIMIZE_WINDOW:
    case COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW:
    case COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW_HORZ:
    case COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW_VERT:
    case COMP_DISPLAY_OPTION_UNMAXIMIZE_WINDOW:
    case COMP_DISPLAY_OPTION_SHOW_DESKTOP:
    case COMP_DISPLAY_OPTION_RUN_COMMAND0:
    case COMP_DISPLAY_OPTION_RUN_COMMAND1:
    case COMP_DISPLAY_OPTION_RUN_COMMAND2:
    case COMP_DISPLAY_OPTION_RUN_COMMAND3:
    case COMP_DISPLAY_OPTION_RUN_COMMAND4:
    case COMP_DISPLAY_OPTION_RUN_COMMAND5:
    case COMP_DISPLAY_OPTION_RUN_COMMAND6:
    case COMP_DISPLAY_OPTION_RUN_COMMAND7:
    case COMP_DISPLAY_OPTION_RUN_COMMAND8:
    case COMP_DISPLAY_OPTION_RUN_COMMAND9:
    case COMP_DISPLAY_OPTION_RUN_COMMAND10:
    case COMP_DISPLAY_OPTION_RUN_COMMAND11:
    case COMP_DISPLAY_OPTION_SLOW_ANIMATIONS:
    case COMP_DISPLAY_OPTION_RAISE_WINDOW:
    case COMP_DISPLAY_OPTION_LOWER_WINDOW:
    case COMP_DISPLAY_OPTION_OPACITY_INCREASE:
    case COMP_DISPLAY_OPTION_OPACITY_DECREASE:
    case COMP_DISPLAY_OPTION_RUN_SCREENSHOT:
    case COMP_DISPLAY_OPTION_RUN_WINDOW_SCREENSHOT:
    case COMP_DISPLAY_OPTION_WINDOW_MENU:
    case COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED:
    case COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED_HORZ:
    case COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED_VERT:
    case COMP_DISPLAY_OPTION_TOGGLE_WINDOW_SHADED:
	if (setDisplayAction (display, o, value))
	    return TRUE;
	break;
    case COMP_DISPLAY_OPTION_AUDIBLE_BELL:
	if (compSetBoolOption (o, value))
	{
	    setAudibleBell (display, o->value.b);
	    return TRUE;
	}
    default:
	break;
    }

    return FALSE;
}

static Bool
setDisplayOptionForPlugin (CompDisplay     *display,
			   char	           *plugin,
			   char	           *name,
			   CompOptionValue *value)
{
    CompPlugin *p;

    p = findActivePlugin (plugin);
    if (p && p->vTable->setDisplayOption)
	return (*p->vTable->setDisplayOption) (display, name, value);

    return FALSE;
}

static void
updatePlugins (CompDisplay *d)
{
    CompOption *o;
    CompPlugin *p, **pop = 0;
    int	       nPop, i, j;

    d->dirtyPluginList = FALSE;

    o = &d->opt[COMP_DISPLAY_OPTION_ACTIVE_PLUGINS];
    for (i = 0; i < d->plugin.list.nValue && i < o->value.list.nValue; i++)
    {
	if (strcmp (d->plugin.list.value[i].s, o->value.list.value[i].s))
	    break;
    }

    nPop = d->plugin.list.nValue - i;

    if (nPop)
    {
	pop = malloc (sizeof (CompPlugin *) * nPop);
	if (!pop)
	{
	    (*d->setDisplayOption) (d, o->name, &d->plugin);
	    return;
	}
    }

    for (j = 0; j < nPop; j++)
    {
	pop[j] = popPlugin ();
	d->plugin.list.nValue--;
	free (d->plugin.list.value[d->plugin.list.nValue].s);
    }

    for (; i < o->value.list.nValue; i++)
    {
	p = 0;
	for (j = 0; j < nPop; j++)
	{
	    if (pop[j] && strcmp (pop[j]->vTable->name,
				  o->value.list.value[i].s) == 0)
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
	    p = loadPlugin (o->value.list.value[i].s);
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

	    value = realloc (d->plugin.list.value, sizeof (CompOption) *
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

    (*d->setDisplayOption) (d, o->name, &d->plugin);
}

static void
addTimeout (CompTimeout *timeout)
{
    CompTimeout *p = 0, *t;

    for (t = timeouts; t; t = t->next)
    {
	if (timeout->time < t->left)
	    break;

	p = t;
    }

    timeout->next = t;
    timeout->left = timeout->time;

    if (p)
	p->next = timeout;
    else
	timeouts = timeout;
}

CompTimeoutHandle
compAddTimeout (int	     time,
		CallBackProc callBack,
		void	     *closure)
{
    CompTimeout *timeout;

    timeout = malloc (sizeof (CompTimeout));
    if (!timeout)
	return 0;

    timeout->time     = time;
    timeout->callBack = callBack;
    timeout->closure  = closure;
    timeout->handle   = lastTimeoutHandle++;

    if (lastTimeoutHandle == MAXSHORT)
	lastTimeoutHandle = 1;

    if (!timeouts)
	gettimeofday (&lastTimeout, 0);

    addTimeout (timeout);

    return timeout->handle;
}

void
compRemoveTimeout (CompTimeoutHandle handle)
{
    CompTimeout *p = 0, *t;

    for (t = timeouts; t; t = t->next)
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
	    timeouts = t->next;

	free (t);
    }
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
    watchFd->handle   = lastWatchFdHandle++;

    if (lastWatchFdHandle == MAXSHORT)
	lastWatchFdHandle = 1;

    watchFd->next = watchFds;
    watchFds = watchFd;

    nWatchFds++;

    watchPollFds = realloc (watchPollFds, nWatchFds * sizeof (struct pollfd));

    watchPollFds[nWatchFds - 1].fd     = fd;
    watchPollFds[nWatchFds - 1].events = events;

    return watchFd->handle;
}

void
compRemoveWatchFd (CompWatchFdHandle handle)
{
    CompWatchFd *p = 0, *w;
    int i;

    for (i = nWatchFds - 1, w = watchFds; w; i--, w = w->next)
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
	    watchFds = w->next;

	nWatchFds--;

	if (i < nWatchFds)
	    memmove (&watchPollFds[i], &watchPollFds[i + 1],
		     (nWatchFds - i) * sizeof (struct pollfd));

	free (w);
    }
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

    for (i = 0; i < CompModNum; i++)
	modMask[i] = 0;

    XDisplayKeycodes (d->display, &minKeycode, &maxKeycode);
    XGetKeyboardMapping (d->display, minKeycode, (maxKeycode - minKeycode + 1),
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

    rv = poll (watchPollFds, nWatchFds, timeout);
    if (rv)
    {
	CompWatchFd *w;
	int	    i;

	for (i = nWatchFds - 1, w = watchFds; w; i--, w = w->next)
	{
	    if (watchPollFds[i].revents != 0 && w->callBack)
		w->callBack (w->closure);
	}
    }

    return rv;
}

static void
handleTimeouts (struct timeval *tv)
{
    CompTimeout *t;
    int		timeDiff;

    timeDiff = TIMEVALDIFF (tv, &lastTimeout);

    /* handle clock rollback */
    if (timeDiff < 0)
	timeDiff = 0;

    for (t = timeouts; t; t = t->next)
	t->left -= timeDiff;

    while (timeouts && timeouts->left <= 0)
    {
	t = timeouts;
	if ((*t->callBack) (t->closure))
	{
	    timeouts = t->next;
	    addTimeout (t);
	}
	else
	{
	    timeouts = t->next;
	    free (t);
	}
    }

    lastTimeout = *tv;
}

static void
waitForVideoSync (CompScreen *s)
{
    unsigned int sync;

    if (!s->opt[COMP_SCREEN_OPTION_SYNC_TO_VBLANK].value.b)
	return;

    /* we currently can't handle sync to vblank when we have more than one
       output device */
    if (s->nOutputDev > 1)
	return;

    if (s->getVideoSync)
    {
	glFlush ();

	(*s->getVideoSync) (&sync);
	(*s->waitVideoSync) (2, (sync + 1) % 2, &sync);
    }
}

void
eventLoop (void)
{
    XEvent	   event;
    int		   timeDiff, i;
    struct timeval tv;
    Region	   tmpRegion, outputRegion;
    CompDisplay    *display = compDisplays;
    CompScreen	   *s;
    int		   time, timeToNextRedraw = 0;
    CompWindow	   *w;
    unsigned int   damageMask, mask;

    tmpRegion = XCreateRegion ();
    outputRegion = XCreateRegion ();
    if (!tmpRegion || !outputRegion)
    {
	fprintf (stderr, "%s: Couldn't create temporary regions\n",
		 programName);
	return;
    }

    compAddWatchFd (ConnectionNumber (display->display), POLLIN, NULL, NULL);

    for (;;)
    {
	if (display->dirtyPluginList)
	    updatePlugins (display);

	if (restartSignal)
	{
	    execvp (programName, programArgv);
	    exit (1);
	}
	else if (shutDown)
	{
	    exit (0);
	}

	while (XPending (display->display))
	{
	    XNextEvent (display->display, &event);

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
		if (event.xclient.message_type == display->xdndPositionAtom)
		{
		    pointerX = event.xclient.data.l[2] >> 16;
		    pointerY = event.xclient.data.l[2] & 0xffff;
		}
	    default:
		break;
	    }

	    sn_display_process_event (display->snDisplay, &event);

	    inHandleEvent = TRUE;

	    (*display->handleEvent) (display, &event);

	    inHandleEvent = FALSE;

	    lastPointerX = pointerX;
	    lastPointerY = pointerY;
	}

	for (s = display->screens; s; s = s->next)
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

	damageMask	 = 0;
	timeToNextRedraw = MAXSHORT;

	for (s = display->screens; s; s = s->next)
	{
	    if (!s->damageMask)
		continue;

	    if (!damageMask)
	    {
		gettimeofday (&tv, 0);
		damageMask |= s->damageMask;
	    }

	    s->timeLeft = getTimeToNextRedraw (s, &tv, &s->lastRedraw, s->idle);
	    if (s->timeLeft < timeToNextRedraw)
		timeToNextRedraw = s->timeLeft;
	}

	if (damageMask)
	{
	    time = timeToNextRedraw;
	    if (time)
		time = doPoll (time);

	    if (time == 0)
	    {
		gettimeofday (&tv, 0);

		if (timeouts)
		    handleTimeouts (&tv);

		for (s = display->screens; s; s = s->next)
		{
		    if (!s->damageMask || s->timeLeft > timeToNextRedraw)
			continue;

		    targetScreen = s;

		    timeDiff = TIMEVALDIFF (&tv, &s->lastRedraw);

		    /* handle clock rollback */
		    if (timeDiff < 0)
			timeDiff = 0;

		    s->stencilRef = 0;

		    makeScreenCurrent (s);

		    if (s->slowAnimations)
		    {
			(*s->preparePaintScreen) (s,
						  s->idle ? 2 : (timeDiff * 2) /
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
			    s->damageMask |= COMP_SCREEN_DAMAGE_REGION_MASK;
			}
		    }

		    if (s->damageMask & COMP_SCREEN_DAMAGE_REGION_MASK)
		    {
			XIntersectRegion (s->damage, &s->region, tmpRegion);

			if (tmpRegion->numRects  == 1	     &&
			    tmpRegion->rects->x1 == 0	     &&
			    tmpRegion->rects->y1 == 0	     &&
			    tmpRegion->rects->x2 == s->width &&
			    tmpRegion->rects->y2 == s->height)
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

		    for (i = 0; i < s->nOutputDev; i++)
		    {
			targetScreen = s;
			targetOutput = i;
			
			if (s->nOutputDev > 1)
			    glViewport (s->outputDev[i].region.extents.x1,
					s->height -
					s->outputDev[i].region.extents.y2,
					s->outputDev[i].width,
					s->outputDev[i].height);

			if (mask & COMP_SCREEN_DAMAGE_ALL_MASK)
			{
			    (*s->paintScreen) (s,
					       &defaultScreenPaintAttrib,
					       &s->outputDev[i].region, i,
					       PAINT_SCREEN_REGION_MASK |
					       PAINT_SCREEN_FULL_MASK);

			    if (i + 1 == s->nOutputDev)
			    {
				waitForVideoSync (s);
				glXSwapBuffers (s->display->display, s->output);
			    }
			}
			else if (mask & COMP_SCREEN_DAMAGE_REGION_MASK)
			{
			    XIntersectRegion (tmpRegion,
					      &s->outputDev[i].region,
					      outputRegion);

			    if ((*s->paintScreen) (s,
						   &defaultScreenPaintAttrib,
						   outputRegion, i,
						   PAINT_SCREEN_REGION_MASK))
			    {
				BoxPtr pBox;
				int    nBox, y;

				pBox = outputRegion->rects;
				nBox = outputRegion->numRects;

				waitForVideoSync (s);

				if (s->copySubBuffer)
				{
				    while (nBox--)
				    {
					y = s->height - pBox->y2;

					(*s->copySubBuffer) (display->display,
							     s->output,
							     pBox->x1, y,
							     pBox->x2 -
							     pBox->x1,
							     pBox->y2 -
							     pBox->y1);

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
			    else
			    {
				(*s->paintScreen) (s,
						   &defaultScreenPaintAttrib,
						   &s->outputDev[i].region, i,
						   PAINT_SCREEN_FULL_MASK);

				if (i + 1 == s->nOutputDev)
				{
				    waitForVideoSync (s);
				    glXSwapBuffers (display->display,
						    s->output);
				}
			    }
			}
		    }

		    targetScreen = NULL;
		    targetOutput = 0;

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
	else
	{
	    if (timeouts)
	    {
		if (timeouts->left > 0)
		    doPoll (timeouts->left);

		gettimeofday (&tv, 0);

		handleTimeouts (&tv);
	    }
	    else
	    {
		doPoll (1000);
	    }
	}
    }
}

static int errors = 0;

static int
errorHandler (Display     *dpy,
	      XErrorEvent *e)
{

#ifdef DEBUG
    char str[128];
    char *name = 0;
    int  o;
#endif

    errors++;

#ifdef DEBUG
    XGetErrorDatabaseText (dpy, "XlibMessage", "XError", "", str, 128);
    fprintf (stderr, "%s", str);

    o = e->error_code - compDisplays->damageError;
    switch (o) {
    case BadDamage:
	name = "BadDamage";
	break;
    default:
	break;
    }

    if (name)
    {
	fprintf (stderr, ": %s\n  ", name);
    }
    else
    {
	XGetErrorText (dpy, e->error_code, str, 128);
	fprintf (stderr, ": %s\n  ", str);
    }

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

#define PING_DELAY 5000

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
			w->alive	    = FALSE;
			w->paint.brightness = 0xa8a8;
			w->paint.saturation = 0;

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

static void
addScreenActions (CompDisplay *d, CompScreen *s)
{
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_CLOSE_WINDOW].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_MAIN_MENU].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_DIALOG].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_MINIMIZE_WINDOW].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW_HORZ].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_MAXIMIZE_WINDOW_VERT].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_UNMAXIMIZE_WINDOW].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_SHOW_DESKTOP].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND0].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND1].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND2].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND3].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND4].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND5].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND6].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND7].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND8].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND9].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND10].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_COMMAND11].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_SLOW_ANIMATIONS].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RAISE_WINDOW].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_LOWER_WINDOW].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_OPACITY_INCREASE].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_OPACITY_DECREASE].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_SCREENSHOT].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_RUN_WINDOW_SCREENSHOT].value.action);
    addScreenAction (s, &d->opt[COMP_DISPLAY_OPTION_WINDOW_MENU].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED_HORZ].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_TOGGLE_WINDOW_MAXIMIZED_VERT].value.action);
    addScreenAction (s,
		     &d->opt[COMP_DISPLAY_OPTION_TOGGLE_WINDOW_SHADED].value.action);
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

    addScreenActions (display, s);
}

Bool
addDisplay (char *name,
	    char **plugin,
	    int  nPlugin)
{
    CompDisplay *d;
    Display	*dpy;
    Window	focus;
    int		revertTo, i;
    int		compositeMajor, compositeMinor;
    int		xkbOpcode;

    d = &compDisplay;

    if (displayPrivateLen)
    {
	d->privates = malloc (displayPrivateLen * sizeof (CompPrivate));
	if (!d->privates)
	    return FALSE;
    }
    else
	d->privates = 0;

    d->screenPrivateIndices = 0;
    d->screenPrivateLen     = 0;

    d->modMap = 0;

    for (i = 0; i < CompModNum; i++)
	d->modMask[i] = CompNoMask;

    d->ignoredModMask = LockMask;

    d->plugin.list.type   = CompOptionTypeString;
    d->plugin.list.nValue = 0;
    d->plugin.list.value  = 0;

    d->textureFilter = GL_LINEAR;
    d->below	     = None;

    d->activeWindow = 0;

    d->autoRaiseHandle = 0;
    d->autoRaiseWindow = None;

    d->display = dpy = XOpenDisplay (name);
    if (!d->display)
    {
	fprintf (stderr, "%s: Couldn't open display %s\n",
		 programName, XDisplayName (name));
	return FALSE;
    }

    compDisplayInitOptions (d, plugin, nPlugin);

    snprintf (d->displayString, 255, "DISPLAY=%s", DisplayString (dpy));

#ifdef DEBUG
    XSynchronize (dpy, TRUE);
#endif

    XSetErrorHandler (errorHandler);

    updateModifierMappings (d);

    d->setDisplayOption		 = setDisplayOption;
    d->setDisplayOptionForPlugin = setDisplayOptionForPlugin;

    d->initPluginForDisplay = initPluginForDisplay;
    d->finiPluginForDisplay = finiPluginForDisplay;

    d->handleEvent	 = handleEvent;
    d->handleCompizEvent = handleCompizEvent;

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

    d->wmAllowedActionsAtom = XInternAtom (dpy, "_NET_WM_ALLOWED_ACTIONS", 0);

    d->wmStrutAtom	  = XInternAtom (dpy, "_NET_WM_STRUT", 0);
    d->wmStrutPartialAtom = XInternAtom (dpy, "_NET_WM_STRUT_PARTIAL", 0);

    d->wmUserTimeAtom = XInternAtom (dpy, "_NET_WM_USER_TIME", 0);

    d->wmIconAtom = XInternAtom (dpy,"_NET_WM_ICON", 0);

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

    d->closeWindowAtom	    = XInternAtom (dpy, "_NET_CLOSE_WINDOW", 0);
    d->wmMoveResizeAtom	    = XInternAtom (dpy, "_NET_WM_MOVERESIZE", 0);
    d->moveResizeWindowAtom = XInternAtom (dpy, "_NET_MOVERESIZE_WINDOW", 0);
    d->restackWindowAtom    = XInternAtom (dpy, "_NET_RESTACK_WINDOW", 0);

    d->showingDesktopAtom = XInternAtom (dpy, "_NET_SHOWING_DESKTOP", 0);

    d->xBackgroundAtom[0] = XInternAtom (dpy, "_XSETROOT_ID", 0);
    d->xBackgroundAtom[1] = XInternAtom (dpy, "_XROOTPMAP_ID", 0);

    d->toolkitActionAtom	  =
	XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION", 0);
    d->toolkitActionMainMenuAtom  =
	XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION_MAIN_MENU", 0);
    d->toolkitActionRunDialogAtom =
	XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION_RUN_DIALOG", 0);
    d->toolkitActionWindowMenuAtom  =
	XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION_WINDOW_MENU", 0);
    d->toolkitActionForceQuitDialogAtom  =
	XInternAtom (dpy, "_COMPIZ_TOOLKIT_ACTION_FORCE_QUIT_DIALOG", 0);

    d->mwmHintsAtom = XInternAtom (dpy, "_MOTIF_WM_HINTS", 0);

    d->xdndAwareAtom    = XInternAtom (dpy, "XdndAware", 0);
    d->xdndEnterAtom    = XInternAtom (dpy, "XdndEnter", 0);
    d->xdndLeaveAtom    = XInternAtom (dpy, "XdndLeave", 0);
    d->xdndPositionAtom = XInternAtom (dpy, "XdndPosition", 0);

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
	fprintf (stderr, "%s: No composite extension\n", programName);
	return FALSE;
    }

    XCompositeQueryVersion (dpy, &compositeMajor, &compositeMinor);
    if (compositeMajor == 0 && compositeMinor < 2)
    {
	fprintf (stderr, "%s: Old composite extension\n", programName);
	return FALSE;
    }

    if (!XDamageQueryExtension (dpy, &d->damageEvent, &d->damageError))
    {
	fprintf (stderr, "%s: No damage extension\n", programName);
	return FALSE;
    }

    if (!XRRQueryExtension (dpy, &d->randrEvent, &d->randrError))
    {
	fprintf (stderr, "%s: No RandR extension\n", programName);
	return FALSE;
    }

    if (!XSyncQueryExtension (dpy, &d->syncEvent, &d->syncError))
    {
	fprintf (stderr, "%s: No sync extension\n", programName);
	return FALSE;
    }

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
	fprintf (stderr, "%s: No XKB extension\n", programName);

	d->xkbEvent = d->xkbError = -1;
    }

    d->xineramaExtension = XineramaQueryExtension (dpy,
						   &d->xineramaEvent,
						   &d->xineramaError);
    if (d->xineramaExtension)
    {
	d->screenInfo = XineramaQueryScreens (dpy, &d->nScreenInfo);
    }
    else
    {
	d->screenInfo  = NULL;
	d->nScreenInfo = 0;
    }

    compDisplays = d;

    d->escapeKeyCode = XKeysymToKeycode (dpy, XStringToKeysym ("Escape"));
    d->returnKeyCode = XKeysymToKeycode (dpy, XStringToKeysym ("Return"));

    for (i = 0; i < ScreenCount (dpy); i++)
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
		fprintf (stderr,
			 "%s: Screen %d on display \"%s\" already "
			 "has a window manager; try using the "
			 "--replace option to replace the current "
			 "window manager.\n",
			 programName, i, DisplayString (dpy));

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
		fprintf (stderr,
			 "%s: Screen %d on display \"%s\" already "
			 "has a compositing manager; try using the "
			 "--replace option to replace the current "
			 "compositing manager .\n",
			 programName, i, DisplayString (dpy));

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

	XSetSelectionOwner (dpy, wmSnAtom, newWmSnOwner, wmSnTimestamp);

	if (XGetSelectionOwner (dpy, wmSnAtom) != newWmSnOwner)
	{
	    fprintf (stderr,
		     "%s: Could not acquire window manager "
		     "selection on screen %d display \"%s\"\n",
		     programName, i, DisplayString (dpy));

	    XDestroyWindow (dpy, newWmSnOwner);

	    continue;
	}

	/* Send client message indicating that we are now the WM */
	event.xclient.type	   = ClientMessage;
	event.xclient.window       = XRootWindow (dpy, i);
	event.xclient.message_type = d->managerAtom;
	event.xclient.format       = 32;
	event.xclient.data.l[0]    = wmSnTimestamp;
	event.xclient.data.l[1]    = wmSnAtom;
	event.xclient.data.l[2]    = 0;
	event.xclient.data.l[3]    = 0;
	event.xclient.data.l[4]    = 0;

	XSendEvent (dpy, XRootWindow (dpy, i), FALSE,
		    StructureNotifyMask, &event);

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
	    fprintf (stderr, "%s: Another composite manager is already "
		     "running on screen: %d\n", programName, i);

	    continue;
	}

	XSetSelectionOwner (dpy, cmSnAtom, newCmSnOwner, wmSnTimestamp);

	if (XGetSelectionOwner (dpy, cmSnAtom) != newCmSnOwner)
	{
	    fprintf (stderr,
		     "%s: Could not acquire compositing manager "
		     "selection on screen %d display \"%s\"\n",
		     programName, i, DisplayString (dpy));

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
	    fprintf (stderr, "%s: Another window manager is "
		     "already running on screen: %d\n",
		     programName, i);

	    XUngrabServer (dpy);
	    continue;
	}

	if (!addScreen (d, i, newWmSnOwner, wmSnAtom, wmSnTimestamp))
	{
	    fprintf (stderr, "%s: Failed to manage screen: %d\n",
		     programName, i);
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
	fprintf (stderr, "%s: No manageable screens found on display %s\n",
		 programName, XDisplayName (name));
	return FALSE;
    }

    setAudibleBell (d, d->opt[COMP_DISPLAY_OPTION_AUDIBLE_BELL].value.b);

    XGetInputFocus (dpy, &focus, &revertTo);

    if (focus == None || focus == PointerRoot)
    {
	focusDefaultWindow (d);
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
	    focusDefaultWindow (d);
    }

    d->pingHandle = compAddTimeout (PING_DELAY, pingTimeout, d);

    return TRUE;
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

void
focusDefaultWindow (CompDisplay *d)
{
    CompScreen *s;
    CompWindow *w;
    CompWindow *focus = NULL;

    for (s = d->screens; s; s = s->next)
    {
	for (w = s->reverseWindows; w; w = w->prev)
	{
	    if (w->type & CompWindowTypeDockMask)
		continue;

	    if ((*s->focusWindow) (w))
	    {
		if (focus)
		{
		    if (w->type & (CompWindowTypeNormalMask |
				   CompWindowTypeDialogMask |
				   CompWindowTypeModalDialogMask))
		    {
			if (w->activeNum > focus->activeNum)
			    focus = w;
		    }
		}
		else
		    focus = w;
	    }
	}
    }

    if (focus)
    {
	if (focus->id != d->activeWindow)
	    moveInputFocusToWindow (focus);
    }
    else
    {
	XSetInputFocus (d->display, d->screens->root, RevertToPointerRoot,
			CurrentTime);
    }
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
    if (lastFoundWindow && lastFoundWindow->id == id)
    {
	return lastFoundWindow;
    }
    else
    {
	CompScreen *s;
	CompWindow *w;

	for (s = d->screens; s; s = s->next)
	{
	    w = findWindowAtScreen (s, id);
	    if (w)
		return w;
	}
    }

    return 0;
}

CompWindow *
findTopLevelWindowAtDisplay (CompDisplay *d,
			     Window      id)
{
    if (lastFoundWindow && lastFoundWindow->id == id)
    {
	return lastFoundWindow;
    }
    else
    {
	CompScreen *s;
	CompWindow *w;

	for (s = d->screens; s; s = s->next)
	{
	    w = findTopLevelWindowAtScreen (s, id);
	    if (w)
		return w;
	}
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
warpPointer (CompDisplay *display,
	     int	 dx,
	     int	 dy)
{
    CompScreen *s = display->screens;
    XEvent     event;

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
