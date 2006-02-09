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

#ifndef GTK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xatom.h>

#include <compiz.h>

#define SWITCH_INITIATE_KEY_DEFAULT       "Tab"
#define SWITCH_INITIATE_MODIFIERS_DEFAULT (CompPressMask | CompAltMask)

#define SWITCH_TERMINATE_KEY_DEFAULT       "Alt_L"
#define SWITCH_TERMINATE_MODIFIERS_DEFAULT CompReleaseMask

#define SWITCH_NEXT_WINDOW_KEY_DEFAULT       "Tab"
#define SWITCH_NEXT_WINDOW_MODIFIERS_DEFAULT (CompPressMask | CompAltMask)

#define SWITCH_SPEED_DEFAULT   1.5f
#define SWITCH_SPEED_MIN       0.1f
#define SWITCH_SPEED_MAX       50.0f
#define SWITCH_SPEED_PRECISION 0.1f

#define SWITCH_TIMESTEP_DEFAULT   1.2f
#define SWITCH_TIMESTEP_MIN       0.1f
#define SWITCH_TIMESTEP_MAX       50.0f
#define SWITCH_TIMESTEP_PRECISION 0.1f

static char *winType[] = {
    "Toolbar",
    "Utility",
    "Dialog",
    "Fullscreen",
    "Normal",
};
#define N_WIN_TYPE (sizeof (winType) / sizeof (winType[0]))

static int displayPrivateIndex;

typedef struct _SwitchDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    Atom popupWinAtom;
    Atom selectWinAtom;
} SwitchDisplay;

#define SWITCH_SCREEN_OPTION_INITIATE     0
#define SWITCH_SCREEN_OPTION_TERMINATE    1
#define SWITCH_SCREEN_OPTION_NEXT_WINDOW  2
#define SWITCH_SCREEN_OPTION_SPEED	  3
#define SWITCH_SCREEN_OPTION_TIMESTEP	  4
#define SWITCH_SCREEN_OPTION_WINDOW_TYPE  5
#define SWITCH_SCREEN_OPTION_NUM          6

typedef struct _SwitchScreen {
    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintWindowProc        paintWindow;
    DamageWindowRectProc   damageWindowRect;

    CompOption opt[SWITCH_SCREEN_OPTION_NUM];

    pid_t  pid;
    Window popupWindow;

    Window	 selectedWindow;
    unsigned int lastActiveNum;

    float speed;
    float timestep;

    unsigned int wMask;

    int grabIndex;

    int     moreAdjust;
    GLfloat velocity;

    CompWindow **windows;
    int        windowsSize;
    int        nWindows;

    int pos;
    int move;
} SwitchScreen;

#define POPUP_WIN_PROP  "_SWITCH_POPUP_WINDOW"
#define SELECT_WIN_PROP "_SWITCH_SELECT_WINDOW"

static Atom visibleNameAtom;
static Atom nameAtom;
static Atom utf8StringAtom;

static GtkWidget *topPane, *bottomPane;
static GdkWindow *foreign = NULL;

#define WIDTH  212
#define HEIGHT 192
#define BORDER 6
#define SPACE  10

#define BOX_WIDTH 3

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

#define WINDOW_WIDTH(count) (WIDTH * (count) + (BORDER << 1) + (SPACE << 1))
#define WINDOW_HEIGHT (HEIGHT + (BORDER << 1) + (SPACE << 1) + 40)

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
switchGetScreenOptions (CompScreen *screen,
			int	   *count)
{
    SWITCH_SCREEN (screen);

    *count = NUM_OPTIONS (ss);
    return ss->opt;
}

static Bool
switchSetScreenOption (CompScreen      *screen,
		       char	       *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    SWITCH_SCREEN (screen);

    o = compFindOption (ss->opt, NUM_OPTIONS (ss), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case SWITCH_SCREEN_OPTION_INITIATE:
	if (addScreenBinding (screen, &value->bind))
	{
	    removeScreenBinding (screen, &o->value.bind);

	    if (compSetBindingOption (o, value))
		return TRUE;
	}
	break;
    case SWITCH_SCREEN_OPTION_TERMINATE:
    case SWITCH_SCREEN_OPTION_NEXT_WINDOW:
	if (compSetBindingOption (o, value))
	    return TRUE;
	break;
    case SWITCH_SCREEN_OPTION_SPEED:
	if (compSetFloatOption (o, value))
	{
	    ss->speed = o->value.f;
	    return TRUE;
	}
	break;
    case SWITCH_SCREEN_OPTION_TIMESTEP:
	if (compSetFloatOption (o, value))
	{
	    ss->timestep = o->value.f;
	    return TRUE;
	}
	break;
    case SWITCH_SCREEN_OPTION_WINDOW_TYPE:
	if (compSetOptionList (o, value))
	{
	    ss->wMask = compWindowTypeMaskFromStringList (&o->value);
	    return TRUE;
	}
    default:
	break;
    }

    return FALSE;
}

static void
switchScreenInitOptions (SwitchScreen *ss,
			 Display      *display)
{
    CompOption *o;
    int	       i;

    o = &ss->opt[SWITCH_SCREEN_OPTION_INITIATE];
    o->name			  = "initiate";
    o->shortDesc		  = "Initiate";
    o->longDesc			  = "Layout and start transforming windows";
    o->type			  = CompOptionTypeBinding;
    o->value.bind.type		  = CompBindingTypeKey;
    o->value.bind.u.key.modifiers = SWITCH_INITIATE_MODIFIERS_DEFAULT;
    o->value.bind.u.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (SWITCH_INITIATE_KEY_DEFAULT));

    o = &ss->opt[SWITCH_SCREEN_OPTION_TERMINATE];
    o->name			  = "terminate";
    o->shortDesc		  = "Terminate";
    o->longDesc			  = "Return from expose view";
    o->type			  = CompOptionTypeBinding;
    o->value.bind.type		  = CompBindingTypeKey;
    o->value.bind.u.key.modifiers = SWITCH_TERMINATE_MODIFIERS_DEFAULT;
    o->value.bind.u.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (SWITCH_TERMINATE_KEY_DEFAULT));

    o = &ss->opt[SWITCH_SCREEN_OPTION_NEXT_WINDOW];
    o->name			  = "next_window";
    o->shortDesc		  = "Next Window";
    o->longDesc			  = "Focus next window";
    o->type			  = CompOptionTypeBinding;
    o->value.bind.type		  = CompBindingTypeKey;
    o->value.bind.u.key.modifiers = SWITCH_NEXT_WINDOW_MODIFIERS_DEFAULT;
    o->value.bind.u.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (SWITCH_NEXT_WINDOW_KEY_DEFAULT));

    o = &ss->opt[SWITCH_SCREEN_OPTION_SPEED];
    o->name		= "speed";
    o->shortDesc	= "Speed";
    o->longDesc		= "Expose speed";
    o->type		= CompOptionTypeFloat;
    o->value.f		= SWITCH_SPEED_DEFAULT;
    o->rest.f.min	= SWITCH_SPEED_MIN;
    o->rest.f.max	= SWITCH_SPEED_MAX;
    o->rest.f.precision = SWITCH_SPEED_PRECISION;

    o = &ss->opt[SWITCH_SCREEN_OPTION_TIMESTEP];
    o->name		= "timestep";
    o->shortDesc	= "Timestep";
    o->longDesc		= "Expose timestep";
    o->type		= CompOptionTypeFloat;
    o->value.f		= SWITCH_TIMESTEP_DEFAULT;
    o->rest.f.min	= SWITCH_TIMESTEP_MIN;
    o->rest.f.max	= SWITCH_TIMESTEP_MAX;
    o->rest.f.precision = SWITCH_TIMESTEP_PRECISION;

    o = &ss->opt[SWITCH_SCREEN_OPTION_WINDOW_TYPE];
    o->name	         = "window_types";
    o->shortDesc         = "Window Types";
    o->longDesc	         = "Window types that should scaled in expose mode";
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_WIN_TYPE;
    o->value.list.value  = malloc (sizeof (CompOptionValue) * N_WIN_TYPE);
    for (i = 0; i < N_WIN_TYPE; i++)
	o->value.list.value[i].s = strdup (winType[i]);
    o->rest.s.string     = windowTypeString;
    o->rest.s.nString    = nWindowTypeString;

    ss->wMask = compWindowTypeMaskFromStringList (&o->value);
}

static char *
text_property_to_utf8 (const XTextProperty *prop)
{
    char **list;
    int  count;
    char *retval;

    list = NULL;

    count =
	gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding),
					prop->format,
					prop->value,
					prop->nitems,
					&list);
    if (count == 0)
	return NULL;

    retval = list[0];
    list[0] = g_strdup ("");

    g_strfreev (list);

    return retval;
}

static char *
_get_text_property (Window  xwindow,
		    Atom    atom)
{
    XTextProperty text;
    char	  *retval;

    text.nitems = 0;
    if (XGetTextProperty (gdk_display,
			  xwindow,
			  &text,
			  atom))
    {
	retval = text_property_to_utf8 (&text);

	if (text.value)
	    XFree (text.value);
    }
    else
    {
	retval = NULL;
    }

    return retval;
}

static char *
_get_utf8_property (Window  xwindow,
		    Atom    atom)
{
    Atom   type;
    int	   format;
    gulong nitems;
    gulong bytes_after;
    gchar  *val;
    int	   result;
    gchar  *retval;

    type = None;
    val  = NULL;

    result = XGetWindowProperty (gdk_display, xwindow, atom,
				 0, G_MAXLONG,
				 FALSE, utf8StringAtom,
				 &type, &format, &nitems,
				 &bytes_after, (guchar **) &val);

    if (result != Success)
	return NULL;

    if (type != utf8StringAtom || format != 8 || nitems == 0)
    {
	if (val)
	    XFree (val);

	return NULL;
    }

    if (!g_utf8_validate (val, nitems, NULL))
    {
	XFree (val);

	return NULL;
    }

    retval = g_strndup (val, nitems);

    XFree (val);

    return retval;
}

static char *
_get_window_name (Window xwindow)
{
    char *name;

    name = _get_utf8_property (xwindow, visibleNameAtom);
    if (!name)
    {
	name = _get_utf8_property (xwindow, nameAtom);
	if (!name)
	    name = _get_text_property (xwindow, XA_WM_NAME);
    }

    return name;
}

static gboolean
paintWidget (GtkWidget	    *widget,
	     GdkEventExpose *event,
	     gpointer	    data)
{
    cairo_t *cr;
    GList   *child;

    cr = gdk_cairo_create (widget->window);

    /* draw colored border */
    gdk_draw_rectangle (widget->window,
			widget->style->bg_gc[GTK_STATE_SELECTED],
			TRUE,
			widget->allocation.x,
			widget->allocation.y,
			widget->allocation.width - 1,
			widget->allocation.height - 1);

    /* draw black outer outline */
    gdk_draw_rectangle (widget->window,
			widget->style->black_gc,
			FALSE,
			widget->allocation.x,
			widget->allocation.y,
			widget->allocation.width - 1,
			widget->allocation.height - 1);

    /* draw inner outline */
    gdk_draw_rectangle (widget->window,
			widget->style->fg_gc[GTK_STATE_INSENSITIVE],
			FALSE,
			widget->allocation.x + BORDER - 1,
			widget->allocation.y + BORDER - 1,
			widget->allocation.width  - 2 * BORDER + 1,
			widget->allocation.height - 2 * BORDER + 1);

    /* draw top pane background */
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    cairo_set_source_rgba (cr,
			   widget->style->bg[GTK_STATE_NORMAL].red   / 65535.0,
			   widget->style->bg[GTK_STATE_NORMAL].green / 65535.0,
			   widget->style->bg[GTK_STATE_NORMAL].blue  / 65535.0,
			   0.8);

    cairo_rectangle (cr,
		     topPane->allocation.x,
		     topPane->allocation.y,
		     topPane->allocation.width,
		     topPane->allocation.height);

    cairo_fill (cr);

    cairo_rectangle (cr,
		     topPane->allocation.x,
		     topPane->allocation.y,
		     topPane->allocation.width,
		     topPane->allocation.height);

    cairo_fill (cr);

    /* draw bottom pane background */
    cairo_set_source_rgba (cr,
			   widget->style->bg[GTK_STATE_ACTIVE].red   / 65535.0,
			   widget->style->bg[GTK_STATE_ACTIVE].green / 65535.0,
			   widget->style->bg[GTK_STATE_ACTIVE].blue  / 65535.0,
			   0.8);

    cairo_rectangle (cr,
		     bottomPane->allocation.x,
		     bottomPane->allocation.y,
		     bottomPane->allocation.width,
		     bottomPane->allocation.height);

    cairo_fill (cr);

    /* draw pane separator */
    gdk_draw_line (widget->window,
		   widget->style->dark_gc[GTK_STATE_NORMAL],
		   bottomPane->allocation.x,
		   bottomPane->allocation.y,
		   bottomPane->allocation.x + bottomPane->allocation.width - 1,
		   bottomPane->allocation.y);

    cairo_destroy (cr);

    child = gtk_container_get_children (GTK_CONTAINER (widget));

    for (; child; child = child->next)
	gtk_container_propagate_expose (GTK_CONTAINER (widget),
					GTK_WIDGET (child->data),
					event);

    return TRUE;
}

static GdkFilterReturn
switchNameChangeFilterFunc (GdkXEvent *gdkxevent,
			    GdkEvent  *event,
			    gpointer  data)
{
    Display   *xdisplay;
    XEvent    *xevent = gdkxevent;
    GtkWidget *label = data;
    gchar     *name;
    gchar     *markup = NULL;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    switch (xevent->type) {
    case PropertyNotify:
	if (!foreign || GDK_WINDOW_XID (foreign) != xevent->xproperty.window)
	    return GDK_FILTER_CONTINUE;

	gdk_error_trap_push ();

	name = _get_window_name (xevent->xproperty.window);

	gdk_flush ();
	if (!gdk_error_trap_pop () && name)
	{
	    markup = g_markup_printf_escaped ("<span size=\"x-large\">"
					      "%s"
					      "</span>",
					      name);
	    g_free (name);
	}

	if (markup)
	{
	    gtk_label_set_markup (GTK_LABEL (label), markup);
	    g_free (markup);
	}
	else
	    gtk_label_set_text (GTK_LABEL (label), "");
    default:
	break;
    }

    return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
switchSelectWindowFilterFunc (GdkXEvent *gdkxevent,
			      GdkEvent  *event,
			      gpointer  data)
{
    Display   *xdisplay;
    XEvent    *xevent = gdkxevent;
    GtkWidget *label = data;
    gchar     *markup = NULL;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    switch (xevent->type) {
    case ClientMessage:
	if (foreign)
	{
	    gdk_error_trap_push ();

	    gdk_window_set_events (foreign, 0);
	    gdk_window_unref (foreign);

	    gdk_flush ();
	    gdk_error_trap_pop ();
	}

	gdk_error_trap_push ();

	foreign = gdk_window_foreign_new (xevent->xclient.data.l[0]);

	gdk_flush ();
	if (!gdk_error_trap_pop () && foreign)
	{
	    gchar *name;

	    gdk_window_add_filter (foreign,
				   switchNameChangeFilterFunc,
				   data);

	    gdk_error_trap_push ();

	    gdk_window_set_events (foreign, GDK_PROPERTY_CHANGE_MASK);
	    name = _get_window_name (xevent->xclient.data.l[0]);

	    gdk_flush ();
	    if (!gdk_error_trap_pop () && name)
	    {
		markup = g_markup_printf_escaped ("<span size=\"x-large\">"
						  "%s"
						  "</span>",
						  name);
		g_free (name);
	    }
	}

	if (markup)
	{
	    gtk_label_set_markup (GTK_LABEL (label), markup);
	    g_free (markup);
	}
	else
	    gtk_label_set_text (GTK_LABEL (label), "");
    default:
	break;
    }

    return GDK_FILTER_CONTINUE;
}


static void
switchSendPopupNotify (GdkScreen *screen,
		       Window    xwindow)
{
    Display *xdisplay;
    Screen  *xscreen;
    XEvent  xev;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    xscreen  = gdk_x11_screen_get_xscreen (screen);

    xev.xclient.type	     = ClientMessage;
    xev.xclient.serial	     = 0;
    xev.xclient.send_event   = TRUE;
    xev.xclient.display	     = xdisplay;
    xev.xclient.window	     = RootWindowOfScreen (xscreen);
    xev.xclient.message_type = XInternAtom (xdisplay, POPUP_WIN_PROP, FALSE);
    xev.xclient.format	     = 32;
    xev.xclient.data.l[0]    = xwindow;
    xev.xclient.data.l[1]    = 0;
    xev.xclient.data.l[2]    = 0;
    xev.xclient.data.l[3]    = 0;
    xev.xclient.data.l[4]    = 0;

    XSendEvent (xdisplay,
		RootWindowOfScreen (xscreen),
		FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xev);
}

static void
sendSelectWindowMessage (CompScreen *s)
{
    XEvent xev;

    SWITCH_DISPLAY (s->display);
    SWITCH_SCREEN (s);

    xev.xclient.type	     = ClientMessage;
    xev.xclient.serial	     = 0;
    xev.xclient.send_event   = TRUE;
    xev.xclient.display	     = s->display->display;
    xev.xclient.window	     = ss->popupWindow;
    xev.xclient.message_type = sd->selectWinAtom;
    xev.xclient.format	     = 32;
    xev.xclient.data.l[0]    = ss->selectedWindow;
    xev.xclient.data.l[1]    = 0;
    xev.xclient.data.l[2]    = 0;
    xev.xclient.data.l[3]    = 0;
    xev.xclient.data.l[4]    = 0;

    XSendEvent (s->display->display,
		ss->popupWindow,
		FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xev);
}

static Bool
isSwitchWin (CompWindow *w)
{
    SWITCH_SCREEN (w->screen);

    if (!w->mapNum || w->attrib.map_state != IsViewable)
	return FALSE;

    if (w->attrib.override_redirect)
	return FALSE;

    if (!(ss->wMask & w->type))
	return FALSE;

    if (w->state & CompWindowStateSkipPagerMask)
	return FALSE;

    return TRUE;
}

static int
compareWindows (const void *elem1,
		const void *elem2)
{
    CompWindow *w1 = *((CompWindow **) elem1);
    CompWindow *w2 = *((CompWindow **) elem2);

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

    count -= (count + 1) & 1;
    if (count < 3)
	count = 3;

    ss->pos  = ((count >> 1) - ss->nWindows) * WIDTH;
    ss->move = 0;

    if (ss->popupWindow)
	XResizeWindow (s->display->display, ss->popupWindow,
		       WINDOW_WIDTH (count),
		       WINDOW_HEIGHT);
}

static void
switchActivateWindow (CompWindow *w)
{
    if ((*w->screen->focusWindow) (w))
    {
	activateWindow (w);
    }
    else
	sendWindowActivationRequest (w->screen, w->id);
}

static void
switchNextWindow (CompScreen *s)
{
    CompWindow *next = NULL;
    CompWindow *prev = NULL;
    CompWindow *w;

    SWITCH_SCREEN (s);

    if (!ss->grabIndex)
	return;

    for (w = s->windows; w; w = w->next)
    {
	if (w->id == ss->selectedWindow)
	    continue;

	if (isSwitchWin (w))
	{
	    if (w->activeNum < ss->lastActiveNum)
	    {
		if (next)
		{
		    if (w->activeNum > next->activeNum)
			next = w;
		}
		else
		    next = w;
	    }
	    else if (w->activeNum > ss->lastActiveNum)
	    {
		if (prev)
		{
		    if (w->activeNum > prev->activeNum)
			prev = w;
		}
		else
		    prev = w;
	    }
	}
    }

    if (next)
	w = next;
    else
	w = prev;

    if (w)
    {
	ss->lastActiveNum  = w->activeNum;
	ss->selectedWindow = w->id;

	ss->move -= WIDTH;
	ss->moreAdjust = 1;

	if (ss->popupWindow)
	{
	    w = findWindowAtScreen (s, ss->popupWindow);
	    if (w)
		addWindowDamage (w);

	    sendSelectWindowMessage (s);
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

static void
switchInitiate (CompScreen *s)
{
    int count;

    SWITCH_SCREEN (s);

    if (ss->grabIndex)
	return;

    count = switchCountWindows (s);
    if (count < 2)
	return;

    if (!ss->pid)
    {
	ss->pid = fork ();

	if (ss->pid == 0)
	{
	    GtkWidget  *window, *vbox;
	    GdkDisplay *display;
	    GdkScreen  *screen;
	    GdkVisual  *visual;
	    GdkAtom    selectWinAtom;

	    gtk_init (&programArgc, &programArgv);

	    window = gtk_window_new (GTK_WINDOW_POPUP);

	    /* check for visual with depth 32 */
	    visual = gdk_visual_get_best_with_depth (32);
	    if (visual)
	    {
		GdkColormap *colormap;

		/* create colormap for depth 32 visual and use it with our
		   top level window. */
		colormap = gdk_colormap_new (visual, FALSE);
		gtk_widget_set_colormap (window, colormap);
	    }

	    display = gdk_display_get_default ();

	    screen = gdk_display_get_screen (display, s->screenNum);

	    gtk_window_set_screen (GTK_WINDOW (window), screen);

	    gtk_window_set_position (GTK_WINDOW (window),
				     GTK_WIN_POS_CENTER_ALWAYS);

	    count -= (count + 1) & 1;
	    if (count < 3)
		count = 3;

	    /* enable resizing, to get never-shrink behavior */
	    gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
	    gtk_window_set_default_size (GTK_WINDOW (window),
					 WINDOW_WIDTH (count),
					 WINDOW_HEIGHT);

	    vbox = gtk_vbox_new (FALSE, 0);
	    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
	    gtk_container_add (GTK_CONTAINER (window), vbox);

	    topPane = gtk_event_box_new ();
	    gtk_event_box_set_visible_window (GTK_EVENT_BOX (topPane), FALSE);
	    gtk_widget_set_size_request (topPane, 0, HEIGHT + (SPACE << 1));
	    gtk_box_pack_start (GTK_BOX (vbox), topPane, FALSE, FALSE, 0);

	    bottomPane = gtk_label_new ("");
	    gtk_label_set_ellipsize (GTK_LABEL (bottomPane),
				     PANGO_ELLIPSIZE_END);
	    gtk_label_set_line_wrap (GTK_LABEL (bottomPane), FALSE);
	    gtk_box_pack_end (GTK_BOX (vbox), bottomPane, TRUE, TRUE, 0);

	    gtk_widget_realize (window);

	    selectWinAtom   = gdk_atom_intern (SELECT_WIN_PROP, FALSE);

	    visibleNameAtom = XInternAtom (GDK_DISPLAY_XDISPLAY (display),
					   "_NET_WM_VISIBLE_NAME", FALSE);
	    nameAtom	    = XInternAtom (GDK_DISPLAY_XDISPLAY (display),
					   "_NET_WM_NAME", FALSE);
	    utf8StringAtom  = XInternAtom (GDK_DISPLAY_XDISPLAY (display),
					   "UTF8_STRING", FALSE);

	    gdk_display_add_client_message_filter (display,
						   selectWinAtom,
						   switchSelectWindowFilterFunc,
						   bottomPane);
	    gdk_window_set_events (window->window, GDK_ALL_EVENTS_MASK);

	    g_signal_connect (G_OBJECT (window),
			      "expose-event",
			      G_CALLBACK (paintWidget),
			      NULL);

	    gdk_window_set_type_hint (window->window,
				      GDK_WINDOW_TYPE_HINT_NORMAL);

	    gtk_widget_show_all (window);

	    switchSendPopupNotify (screen, GDK_WINDOW_XID (window->window));

	    gtk_main ();

	    exit (0);
	}
    }

    if (!ss->grabIndex)
    {
	ss->grabIndex = pushScreenGrab (s, s->invisibleCursor);

	if (ss->grabIndex)
	{
	    ss->lastActiveNum  = s->activeNum;
	    ss->selectedWindow = s->display->activeWindow;

	    switchUpdateWindowList (s, count);

	    if (ss->popupWindow)
		XMapRaised (s->display->display, ss->popupWindow);
	}
    }
}

static void
switchTerminate (CompScreen *s,
		 Bool	    select)
{
    SWITCH_SCREEN (s);

    if (ss->grabIndex)
    {
	if (ss->popupWindow)
	    XUnmapWindow (s->display->display, ss->popupWindow);

	removeScreenGrab (s, ss->grabIndex, 0);
	ss->grabIndex = 0;

	if (select && ss->selectedWindow)
	{
	    CompWindow *w;

	    w = findWindowAtScreen (s, ss->selectedWindow);
	    if (w)
		switchActivateWindow (w);
	}

	ss->selectedWindow = None;
	ss->lastActiveNum  = 0;
    }
}

static void
switchWindowRemove (CompDisplay *d,
		    Window	id)
{
    CompWindow *w;

    w = findWindowAtDisplay (d, id);
    if (w)
    {
	SWITCH_SCREEN (w->screen);

	if (ss->grabIndex)
	{
	    int i;

	    for (i = 0; i < ss->nWindows; i++)
	    {
		if (ss->windows[i] == w)
		{
		    ss->lastActiveNum  = w->screen->activeNum;
		    ss->selectedWindow = d->activeWindow;

		    switchUpdateWindowList (w->screen,
					    switchCountWindows (w->screen));

		    break;
		}
	    }
	}
    }
}

static void
switchHandleEvent (CompDisplay *d,
		   XEvent      *event)
{
    CompScreen *s;

    SWITCH_DISPLAY (d);

    switch (event->type) {
    case KeyPress:
    case KeyRelease:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    SWITCH_SCREEN (s);

	    if (EV_KEY (&ss->opt[SWITCH_SCREEN_OPTION_INITIATE], event))
		switchInitiate (s);

	    if (EV_KEY (&ss->opt[SWITCH_SCREEN_OPTION_NEXT_WINDOW], event))
		switchNextWindow (s);

	    if (EV_KEY (&ss->opt[SWITCH_SCREEN_OPTION_TERMINATE], event))
		switchTerminate (s, TRUE);
	    else if (event->type	 == KeyPress &&
		     event->xkey.keycode == s->escapeKeyCode)
		switchTerminate (s, FALSE);
	}
	break;
    case ClientMessage:
	s = findScreenAtDisplay (d, event->xclient.window);
	if (s)
	{
	    SWITCH_SCREEN (s);

	    if (event->xclient.message_type == sd->popupWinAtom)
	    {
		ss->popupWindow = event->xclient.data.l[0];

		if (ss->grabIndex)
		    sendSelectWindowMessage (s);
		else
		    XUnmapWindow (d->display, ss->popupWindow);
	    }
	}
    default:
	break;
    }

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

    ss->velocity = (amount * ss->velocity + adjust) / (amount + 1.0f);

    if (fabs (dx) < 0.1f && fabs (ss->velocity) < 0.2f)
    {
	ss->velocity = 0.0f;
	return 0;
    }

    return 1;
}

static void
switchPreparePaintScreen (CompScreen *s,
			  int	     msSinceLastPaint)
{
    SWITCH_SCREEN (s);

    if (ss->grabIndex && ss->moreAdjust)
    {
	int   steps, m;
	float amount, chunk;

	amount = msSinceLastPaint * 0.05f * ss->speed;
	steps  = amount / (0.5f * ss->timestep);
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    ss->moreAdjust = adjustSwitchVelocity (s);
	    if (!ss->moreAdjust)
	    {
		ss->pos += ss->move;
		ss->move = 0;
		break;
	    }

	    m = ss->velocity * chunk;

	    ss->move -= m;
	    ss->pos  += m;
	    if (ss->pos < -ss->nWindows * WIDTH)
		ss->pos += ss->nWindows * WIDTH;
	    else if (ss->pos > 0)
		ss->pos -= ss->nWindows * WIDTH;
	}
    }

    UNWRAP (ss, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (ss, s, preparePaintScreen, switchPreparePaintScreen);
}

static void
switchDonePaintScreen (CompScreen *s)
{
    SWITCH_SCREEN (s);

    if (ss->grabIndex && ss->moreAdjust)
    {
	CompWindow *w;

	w = findWindowAtScreen (s, ss->popupWindow);
	if (w)
	    addWindowDamage (w);
    }

    UNWRAP (ss, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ss, s, donePaintScreen, switchDonePaintScreen);
}

static void
switchPaintThumb (CompWindow		  *w,
		  const WindowPaintAttrib *attrib,
		  unsigned int		  mask,
		  int			  x,
		  int			  y,
		  int			  x1,
		  int			  x2)
{
    WindowPaintAttrib sAttrib = *attrib;
    int		      dx, dy;
    int		      wx, wy;
    float	      width, height;
    REGION	      reg;

    width  = WIDTH  - (SPACE << 1);
    height = HEIGHT - (SPACE << 1);

    if (w->width > width)
	sAttrib.xScale = width / w->width;
    else
	sAttrib.xScale = 1.0f;

    if (w->height > height)
	sAttrib.yScale = height / w->height;
    else
	sAttrib.yScale = 1.0f;

    if (sAttrib.xScale < sAttrib.yScale)
	sAttrib.yScale = sAttrib.xScale;
    else
	sAttrib.xScale = sAttrib.yScale;

    width  = w->width  * sAttrib.xScale;
    height = w->height * sAttrib.yScale;

    wx = x + SPACE + ((WIDTH  - (SPACE << 1)) - width)  / 2;
    wy = y + SPACE + ((HEIGHT - (SPACE << 1)) - height) / 2;

    if (!w->texture.pixmap)
	bindWindow (w);

    dx = wx - w->attrib.x;
    dy = wy - w->attrib.y;

    moveWindow (w, dx, dy, FALSE);

    mask = mask | PAINT_WINDOW_TRANSFORMED_MASK;
    if (w->alpha)
	mask |= PAINT_WINDOW_TRANSLUCENT_MASK;
    else if (sAttrib.opacity == OPAQUE)
	mask &= ~PAINT_WINDOW_TRANSLUCENT_MASK;

    reg.rects    = &reg.extents;
    reg.numRects = 1;

    reg.extents.y1 = MINSHORT;
    reg.extents.y2 = MAXSHORT;
    reg.extents.x1 = wx + (x1 - wx) / sAttrib.xScale;
    reg.extents.x2 = wx + (x2 - wx) / sAttrib.xScale;

    w->vCount = 0;
    addWindowGeometry (w, &w->matrix, 1, w->region, &reg);
    if (w->vCount)
	drawWindowTexture (w, &w->texture, &sAttrib, mask);

    moveWindow (w, -dx, -dy, FALSE);
}

static Bool
switchPaintWindow (CompWindow		   *w,
		   const WindowPaintAttrib *attrib,
		   Region		   region,
		   unsigned int		   mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    SWITCH_SCREEN (s);

    if (ss->grabIndex && w->id != ss->selectedWindow)
    {
	if (w->id == ss->popupWindow)
	{
	    int	x, y, x1, x2, cx, i;

	    if (mask & PAINT_WINDOW_SOLID_MASK)
		return FALSE;

	    UNWRAP (ss, s, paintWindow);
	    status = (*s->paintWindow) (w, attrib, region, mask);
	    WRAP (ss, s, paintWindow, switchPaintWindow);

	    x1 = w->attrib.x + BORDER + SPACE;
	    x2 = w->attrib.x + w->width - BORDER - SPACE;

	    x = x1 + ss->pos;
	    y = w->attrib.y + BORDER + SPACE;

	    for (i = 0; i < ss->nWindows; i++)
	    {
		if (x + WIDTH > x1)
		    switchPaintThumb (ss->windows[i], attrib, mask,
				      x, y, x1, x2);

		x += WIDTH;
	    }

	    for (i = 0; i < ss->nWindows; i++)
	    {
		if (x > x2)
		    break;

		switchPaintThumb (ss->windows[i], attrib, mask,
				  x, y, x1, x2);

		x += WIDTH;
	    }

	    cx = w->attrib.x + (w->width >> 1);

	    glColor4us (0, 0, 0, attrib->opacity);
	    glPushMatrix ();
	    glTranslatef (cx, y, 0.0f);
	    glVertexPointer (2, GL_FLOAT, 0, _boxVertices);
	    glDrawArrays (GL_QUADS, 0, 16);
	    glPopMatrix ();
	}
	else
	{
	    WindowPaintAttrib sAttrib = *attrib;

	    sAttrib.brightness = (2 * sAttrib.brightness) / 3;

	    if (ss->wMask & w->type)
		sAttrib.opacity >>= 1;

	    UNWRAP (ss, s, paintWindow);
	    status = (*s->paintWindow) (w, &sAttrib, region, mask);
	    WRAP (ss, s, paintWindow, switchPaintWindow);
	}
    }
    else
    {
	UNWRAP (ss, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, region, mask);
	WRAP (ss, s, paintWindow, switchPaintWindow);
    }

    return status;
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
	if (initial)
	{
	    if (isSwitchWin (w))
	    {
		ss->lastActiveNum  = w->screen->activeNum;
		ss->selectedWindow = w->screen->display->activeWindow;

		switchUpdateWindowList (w->screen,
					switchCountWindows (w->screen));
	    }
	}
	else if (!ss->moreAdjust)
	{
	    if (isSwitchWin (w))
	    {
		CompWindow *popup;

		popup = findWindowAtScreen (w->screen, ss->popupWindow);
		if (popup)
		    addWindowDamage (popup);

	    }
	}
    }

    UNWRAP (ss, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ss, w->screen, damageWindowRect, switchDamageWindowRect);

    return status;
}

static Bool
switchInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    SwitchDisplay *sd;

    sd = malloc (sizeof (SwitchDisplay));
    if (!sd)
	return FALSE;

    sd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (sd->screenPrivateIndex < 0)
    {
	free (sd);
	return FALSE;
    }

    sd->popupWinAtom  = XInternAtom (d->display, POPUP_WIN_PROP, 0);
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

    free (sd);
}

static Bool
switchInitScreen (CompPlugin *p,
		  CompScreen *s)
{
    SwitchScreen *ss;

    SWITCH_DISPLAY (s->display);

    ss = malloc (sizeof (SwitchScreen));
    if (!ss)
	return FALSE;

    ss->pid	    = 0;
    ss->popupWindow = None;

    ss->selectedWindow = None;
    ss->lastActiveNum  = 0;

    ss->windows     = 0;
    ss->nWindows    = 0;
    ss->windowsSize = 0;

    ss->pos = ss->move = 0;

    ss->grabIndex = 0;

    ss->speed    = SWITCH_SPEED_DEFAULT;
    ss->timestep = SWITCH_TIMESTEP_DEFAULT;

    ss->moreAdjust = 0;

    ss->velocity = 0.0f;

    switchScreenInitOptions (ss, s->display->display);

    addScreenBinding (s, &ss->opt[SWITCH_SCREEN_OPTION_INITIATE].value.bind);

    WRAP (ss, s, preparePaintScreen, switchPreparePaintScreen);
    WRAP (ss, s, donePaintScreen, switchDonePaintScreen);
    WRAP (ss, s, paintWindow, switchPaintWindow);
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
    UNWRAP (ss, s, paintWindow);
    UNWRAP (ss, s, damageWindowRect);

    if (ss->windowsSize)
	free (ss->windows);

    free (ss);
}

static Bool
switchInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
switchFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginVTable switchVTable = {
    "switcher",
    "Application Switcher",
    "Application Switcher",
    switchInit,
    switchFini,
    switchInitDisplay,
    switchFiniDisplay,
    switchInitScreen,
    switchFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    switchGetScreenOptions,
    switchSetScreenOption,
    NULL,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &switchVTable;
}
