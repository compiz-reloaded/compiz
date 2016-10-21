/*
 * Copyright © 2006 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "decoration.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrender.h>
#ifdef HAVE_XINPUT2
#include <X11/extensions/XInput2.h>
#endif
#include <X11/Xregion.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>

#ifdef USE_COMPIZCONFIG
#include <ccs.h>
#endif

#ifdef USE_GSETTINGS
#include <gio/gio.h>
#endif

#ifdef USE_DBUS_GLIB
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#endif

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#include <cairo.h>
#include <cairo-xlib.h>

#if CAIRO_VERSION < CAIRO_VERSION_ENCODE(1, 1, 0)
#define CAIRO_EXTEND_PAD CAIRO_EXTEND_NONE
#endif

#include <pango/pango-context.h>
#include <pango/pangocairo.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#ifdef USE_MARCO
#include <marco-private/theme.h>
#endif

#define GSCHEMA_KEY_GNOME_WM "org.gnome.desktop.wm.preferences"

#define GSCHEMA_KEY_METACITY "org.gnome.metacity"

#define GSCHEMA_KEY_METACITY_THEME "org.gnome.metacity.theme"

#define GSCHEMA_KEY_MARCO "org.mate.Marco.general"

#define META_USE_SYSTEM_FONT_KEY "titlebar-uses-system-font"

#define META_TITLEBAR_FONT_KEY "titlebar-font"

#define META_DOUBLE_CLICK_TITLEBAR_KEY "action-double-click-titlebar"

#define META_MIDDLE_CLICK_TITLEBAR_KEY "action-middle-click-titlebar"

#define META_RIGHT_CLICK_TITLEBAR_KEY "action-right-click-titlebar"

#define META_THEME_KEY_METACITY "name"

#define META_THEME_KEY_MARCO "theme"

#define META_BUTTON_LAYOUT_KEY "button-layout"

#define GSCHEMA_KEY_GWD "org.compiz-0.gwd"

#define GWD_USE_META_THEME_KEY "use-marco-theme"

#define GWD_META_THEME_OPACITY_KEY "marco-theme-opacity"

#define GWD_META_THEME_SHADE_OPACITY_KEY "marco-theme-shade-opacity"

#define GWD_META_THEME_ACTIVE_OPACITY_KEY "marco-theme-active-opacity"

#define GWD_META_THEME_ACTIVE_SHADE_OPACITY_KEY "marco-theme-active-shade-opacity"

#define GWD_BLUR_TYPE_KEY "blur-type"

#define GWD_WHEEL_ACTION_KEY "mouse-wheel-action"


#define GSCHEMA_KEY_GNOME_INTERFACE "org.gnome.desktop.interface"

#define GSCHEMA_KEY_MATE_MOUSE "org.mate.peripherals-mouse"

#define MATE_CURSOR_THEME_KEY "cursor-theme"

#define MATE_CURSOR_SIZE_KEY "cursor-size"


#define COMPIZCONFIG_DECOR_OPTION "decoration"

#define COMPIZCONFIG_SHADOW_RADIUS_OPTION "shadow_radius"

#define COMPIZCONFIG_SHADOW_OPACITY_OPTION "shadow_opacity"

#define COMPIZCONFIG_SHADOW_COLOR_OPTION "shadow_color"

#define COMPIZCONFIG_SHADOW_OFFSET_X_OPTION "shadow_x_offset"

#define COMPIZCONFIG_SHADOW_OFFSET_Y_OPTION "shadow_y_offset"


#define COMPIZCONFIG_CORE_OPTION "core"

#define COMPIZCONFIG_CURSOR_THEME_OPTION "cursor_theme"

#define COMPIZCONFIG_CURSOR_SIZE_OPTION "cursor_size"


#define DBUS_DEST       "org.freedesktop.compiz"
#define DBUS_PATH       "/org/freedesktop/compiz/decoration/allscreens"
#define DBUS_INTERFACE  "org.freedesktop.compiz"
#define DBUS_METHOD_GET "get"

#define STROKE_ALPHA 0.6

#define ICON_SPACE 20

#define DOUBLE_CLICK_DISTANCE 8.0

#define WM_MOVERESIZE_SIZE_TOPLEFT      0
#define WM_MOVERESIZE_SIZE_TOP          1
#define WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define WM_MOVERESIZE_SIZE_RIGHT        3
#define WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define WM_MOVERESIZE_SIZE_BOTTOM       5
#define WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define WM_MOVERESIZE_SIZE_LEFT         7
#define WM_MOVERESIZE_MOVE              8
#define WM_MOVERESIZE_SIZE_KEYBOARD     9
#define WM_MOVERESIZE_MOVE_KEYBOARD    10

#define SHADOW_RADIUS      8.0
#define SHADOW_OPACITY     0.5
#define SHADOW_OFFSET_X    1
#define SHADOW_OFFSET_Y    1
#define SHADOW_COLOR_RED   0x0000
#define SHADOW_COLOR_GREEN 0x0000
#define SHADOW_COLOR_BLUE  0x0000

#define META_OPACITY              1.0
#define META_SHADE_OPACITY        TRUE
#define META_ACTIVE_OPACITY       1.0
#define META_ACTIVE_SHADE_OPACITY TRUE

#define CMDLINE_OPACITY              (1 << 0)
#define CMDLINE_OPACITY_SHADE        (1 << 1)
#define CMDLINE_ACTIVE_OPACITY       (1 << 2)
#define CMDLINE_ACTIVE_OPACITY_SHADE (1 << 3)
#define CMDLINE_BLUR                 (1 << 4)
#define CMDLINE_THEME                (1 << 5)

#define MWM_HINTS_DECORATIONS (1L << 1)

#define MWM_DECOR_ALL      (1L << 0)
#define MWM_DECOR_BORDER   (1L << 1)
#define MWM_DECOR_HANDLE   (1L << 2)
#define MWM_DECOR_TITLE    (1L << 3)
#define MWM_DECOR_MENU     (1L << 4)
#define MWM_DECOR_MINIMIZE (1L << 5)
#define MWM_DECOR_MAXIMIZE (1L << 6)

#define PROP_MOTIF_WM_HINT_ELEMENTS 3

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
} MwmHints;

enum {
    CLICK_ACTION_NONE,
    CLICK_ACTION_SHADE,
    CLICK_ACTION_MAXIMIZE,
    CLICK_ACTION_MINIMIZE,
    CLICK_ACTION_RAISE,
    CLICK_ACTION_LOWER,
    CLICK_ACTION_MENU,
    CLICK_ACTION_MAXIMIZE_HORZ,
    CLICK_ACTION_MAXIMIZE_VERT
};

enum {
    WHEEL_ACTION_NONE,
    WHEEL_ACTION_SHADE
};

#define DOUBLE_CLICK_ACTION_DEFAULT CLICK_ACTION_MAXIMIZE
#define MIDDLE_CLICK_ACTION_DEFAULT CLICK_ACTION_LOWER
#define RIGHT_CLICK_ACTION_DEFAULT  CLICK_ACTION_MENU
#define WHEEL_ACTION_DEFAULT        WHEEL_ACTION_NONE

int double_click_action = DOUBLE_CLICK_ACTION_DEFAULT;
int middle_click_action = MIDDLE_CLICK_ACTION_DEFAULT;
int right_click_action  = RIGHT_CLICK_ACTION_DEFAULT;
int wheel_action        = WHEEL_ACTION_DEFAULT;

static gboolean minimal = FALSE;

static double decoration_alpha = 0.5;

#define SWITCHER_SPACE 40

static decor_extents_t _shadow_extents      = { 0, 0, 0, 0 };
static decor_extents_t _win_extents         = { 6, 6, 4, 6 };
static decor_extents_t _max_win_extents     = { 6, 6, 4, 6 };
static decor_extents_t _default_win_extents = { 6, 6, 4, 6 };
static decor_extents_t _switcher_extents    = { 6, 6, 6, 6 + SWITCHER_SPACE };

static int titlebar_height = 17;
static int max_titlebar_height = 17;

static decor_context_t window_context = {
    { 0, 0, 0, 0 },
    6, 6, 4, 6,
    0, 0, 0, 0
};

static decor_context_t max_window_context = {
    { 0, 0, 0, 0 },
    6, 6, 4, 6,
    0, 0, 0, 0
};

static decor_context_t switcher_context = {
    { 0, 0, 0, 0 },
    6, 6, 6, 6 + SWITCHER_SPACE,
    0, 0, 0, 0
};

static decor_context_t shadow_context = {
    { 0, 0, 0, 0 },
    0, 0, 0, 0,
    0, 0, 0, 0,
};

static gdouble shadow_radius   = SHADOW_RADIUS;
static gdouble shadow_opacity  = SHADOW_OPACITY;
static gushort shadow_color[3] = {
    SHADOW_COLOR_RED,
    SHADOW_COLOR_GREEN,
    SHADOW_COLOR_BLUE
};
static gint    shadow_offset_x = SHADOW_OFFSET_X;
static gint    shadow_offset_y = SHADOW_OFFSET_Y;

#ifdef USE_MARCO
static double   meta_opacity              = META_OPACITY;
static gboolean meta_shade_opacity        = META_SHADE_OPACITY;
static double   meta_active_opacity       = META_ACTIVE_OPACITY;
static gboolean meta_active_shade_opacity = META_ACTIVE_SHADE_OPACITY;

static gboolean         meta_button_layout_set = FALSE;
static MetaButtonLayout meta_button_layout;
#endif

static guint cmdline_options = 0;

static decor_shadow_t *no_border_shadow = NULL;
static decor_shadow_t *border_shadow = NULL;
static decor_shadow_t *max_border_shadow = NULL;
static decor_shadow_t *switcher_shadow = NULL;

static cairo_surface_t *decor_normal_surface = NULL;
static cairo_surface_t *decor_active_surface = NULL;

static Atom frame_window_atom;
static Atom win_decor_atom;
static Atom win_blur_decor_atom;
static Atom wm_move_resize_atom;
static Atom restack_window_atom;
static Atom select_window_atom;
static Atom mwm_hints_atom;
static Atom switcher_fg_atom;

static Atom toolkit_action_atom;
static Atom toolkit_action_window_menu_atom;
static Atom toolkit_action_force_quit_dialog_atom;

static Time dm_sn_timestamp;

#define C(name) { 0, XC_ ## name }

static struct _cursor {
    Cursor	 cursor;
    unsigned int shape;
} cursor[3][3] = {
    { C (top_left_corner),    C (top_side),    C (top_right_corner)    },
    { C (left_side),	      C (left_ptr),    C (right_side)	       },
    { C (bottom_left_corner), C (bottom_side), C (bottom_right_corner) }
};

#define BUTTON_CLOSE   0
#define BUTTON_MAX     1
#define BUTTON_MIN     2
#define BUTTON_MENU    3
#define BUTTON_SHADE   4
#define BUTTON_ABOVE   5
#define BUTTON_STICK   6
#define BUTTON_UNSHADE 7
#define BUTTON_UNABOVE 8
#define BUTTON_UNSTICK 9
#define BUTTON_NUM     10

static struct _pos {
    int x, y, w, h;
    int xw, yh, ww, hh, yth, hth;
} pos[3][3] = {
    {
	{  0,  0, 10, 21,   0, 0, 0, 0, 0, 1 },
	{ 10,  0, -8,  6,   0, 0, 1, 0, 0, 1 },
	{  2,  0, 10, 21,   1, 0, 0, 0, 0, 1 }
    }, {
	{  0, 10,  6, 11,   0, 0, 0, 1, 1, 0 },
	{  6,  6,  0, 15,   0, 0, 1, 0, 0, 1 },
	{  6, 10,  6, 11,   1, 0, 0, 1, 1, 0 }
    }, {
	{  0, 17, 10, 10,   0, 1, 0, 0, 1, 0 },
	{ 10, 21, -8,  6,   0, 1, 1, 0, 1, 0 },
	{  2, 17, 10, 10,   1, 1, 0, 0, 1, 0 }
    }
}, bpos[] = {
    { 0, 6, 16, 16,   1, 0, 0, 0, 0, 0 },
    { 0, 6, 16, 16,   1, 0, 0, 0, 0, 0 },
    { 0, 6, 16, 16,   1, 0, 0, 0, 0, 0 },
    { 6, 2, 16, 16,   0, 0, 0, 0, 0, 0 }
};

typedef struct _decor_color {
    double r;
    double g;
    double b;
} decor_color_t;

#define IN_EVENT_WINDOW      (1 << 0)
#define PRESSED_EVENT_WINDOW (1 << 1)

typedef struct _decor {
    Window	      event_windows[3][3];
    Window	      button_windows[BUTTON_NUM];
    guint	      button_states[BUTTON_NUM];
    cairo_surface_t   *surface;
    cairo_surface_t   *buffer_surface;
    cairo_t           *cr;
    decor_layout_t    border_layout;
    decor_context_t   *context;
    decor_shadow_t    *shadow;
    Picture	      picture;
    gint	      button_width;
    gint	      width;
    gint	      height;
    gint	      client_width;
    gint	      client_height;
    gboolean	      decorated;
    gboolean	      active;
    PangoLayout	      *layout;
    gchar	      *name;
    cairo_pattern_t   *icon;
    cairo_surface_t   *icon_surface;
    GdkPixbuf	      *icon_pixbuf;
    WnckWindowState   state;
    WnckWindowActions actions;
    XID		      prop_xid;
    GtkWidget	      *force_quit_dialog;
    void	      (*draw) (struct _decor *d);
} decor_t;

void     (*theme_draw_window_decoration)    (decor_t *d);
gboolean (*theme_calc_decoration_size)      (decor_t *d,
					     int     client_width,
					     int     client_height,
					     int     text_width,
					     int     *width,
					     int     *height);
void     (*theme_update_border_extents)     (gint    text_height);
void     (*theme_get_event_window_position) (decor_t *d,
					     gint    i,
					     gint    j,
					     gint    width,
					     gint    height,
					     gint    *x,
					     gint    *y,
					     gint    *w,
					     gint    *h);
gboolean (*theme_get_button_position)       (decor_t *d,
					     gint    i,
					     gint    width,
					     gint    height,
					     gint    *x,
					     gint    *y,
					     gint    *w,
					     gint    *h);

typedef void (*event_callback) (WnckWindow *win, XEvent *event);

static char *program_name;

static GtkWidget     *style_window;
static GtkWidget     *switcher_label;

static GHashTable    *frame_table;
static GtkWidget     *action_menu = NULL;
static gboolean      action_menu_mapped = FALSE;
static decor_color_t _title_color[2];
static PangoContext  *pango_context;
static gint	     double_click_timeout = 250;

static GtkWidget     *tip_window;
static GtkWidget     *tip_label;
static gint	     tooltip_timer_tag = 0;

static GSList *draw_list = NULL;
static guint  draw_idle_id = 0;

static PangoFontDescription *titlebar_font = NULL;
static gboolean		    use_system_font = FALSE;
static gint		    text_height;

#define BLUR_TYPE_NONE     0
#define BLUR_TYPE_TITLEBAR 1
#define BLUR_TYPE_ALL      2

static gint blur_type = BLUR_TYPE_NONE;

static gint      switcher_width;
static gint      switcher_height;
static Window    switcher_selected_window = None;

static XRenderPictFormat *xformat;

static void
decor_update_blur_property (decor_t *d,
			    int     width,
			    int     height,
			    Region  top_region,
			    int     top_offset,
			    Region  bottom_region,
			    int     bottom_offset,
			    Region  left_region,
			    int     left_offset,
			    Region  right_region,
			    int     right_offset)
{
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    long    *data = NULL;
    int     size = 0;

    if (blur_type != BLUR_TYPE_ALL)
    {
	bottom_region = NULL;
	left_region   = NULL;
	right_region  = NULL;

	if (blur_type != BLUR_TYPE_TITLEBAR)
	    top_region = NULL;
    }

    if (top_region)
	size += top_region->numRects;
    if (bottom_region)
	size += bottom_region->numRects;
    if (left_region)
	size += left_region->numRects;
    if (right_region)
	size += right_region->numRects;

    if (size)
	data = malloc (sizeof (long) * (2 + size * 6));

    if (data)
    {
	decor_region_to_blur_property (data, 4, 0, width, height,
				       top_region, top_offset,
				       bottom_region, bottom_offset,
				       left_region, left_offset,
				       right_region, right_offset);

	gdk_error_trap_push ();
	XChangeProperty (xdisplay, d->prop_xid,
			 win_blur_decor_atom,
			 XA_INTEGER,
			 32, PropModeReplace, (guchar *) data,
			 2 + size * 6);
	gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
	gdk_error_trap_pop_ignored ();
#else
	gdk_error_trap_pop ();
#endif

	free (data);
    }
    else
    {
	gdk_error_trap_push ();
	XDeleteProperty (xdisplay, d->prop_xid, win_blur_decor_atom);
	gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
	gdk_error_trap_pop_ignored ();
#else
	gdk_error_trap_pop ();
#endif
    }
}

static void
decor_update_window_property (decor_t *d)
{
    long	    *data = NULL;
    unsigned int    n = 1, frame_type = 0, frame_state = 0, frame_actions = 0;
    Display	    *xdisplay =
	GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    decor_extents_t extents = _win_extents;
    gint	    nQuad;
    decor_quad_t    quads[N_QUADS_MAX];
    int		    w, h;
    gint	    stretch_offset;
    REGION	    top, bottom, left, right;

    w = d->border_layout.top.x2 - d->border_layout.top.x1 -
	d->context->left_space - d->context->right_space;

    if (d->border_layout.rotation)
	h = d->border_layout.left.x2 - d->border_layout.left.x1;
    else
	h = d->border_layout.left.y2 - d->border_layout.left.y1;

    stretch_offset = w - d->button_width - 1;

    nQuad = decor_set_lSrStXbS_window_quads (quads, d->context,
					     &d->border_layout,
					     stretch_offset);

    extents.top += titlebar_height;

    data = decor_alloc_property(n, WINDOW_DECORATION_TYPE_PIXMAP);
    decor_quads_to_property (data, n - 1,
			     cairo_xlib_surface_get_drawable (d->surface),
			     &extents, &extents, &extents, &extents,
			     ICON_SPACE + d->button_width, 0, quads, nQuad,
			     frame_type, frame_state, frame_actions);

    gdk_error_trap_push ();
    XChangeProperty (xdisplay, d->prop_xid,
		     win_decor_atom,
		     XA_INTEGER,
		     32, PropModeReplace, (guchar *) data,
		     PROP_HEADER_SIZE + BASE_PROP_SIZE +
		       QUAD_PROP_SIZE * N_QUADS_MAX);
    gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif

    top.rects = &top.extents;
    top.numRects = top.size = 1;

    top.extents.x1 = -extents.left;
    top.extents.y1 = -extents.top;
    top.extents.x2 = w + extents.right;
    top.extents.y2 = 0;

    bottom.rects = &bottom.extents;
    bottom.numRects = bottom.size = 1;

    bottom.extents.x1 = -extents.left;
    bottom.extents.y1 = 0;
    bottom.extents.x2 = w + extents.right;
    bottom.extents.y2 = extents.bottom;

    left.rects = &left.extents;
    left.numRects = left.size = 1;

    left.extents.x1 = -extents.left;
    left.extents.y1 = 0;
    left.extents.x2 = 0;
    left.extents.y2 = h;

    right.rects = &right.extents;
    right.numRects = right.size = 1;

    right.extents.x1 = 0;
    right.extents.y1 = 0;
    right.extents.x2 = extents.right;
    right.extents.y2 = h;

    decor_update_blur_property (d,
				w, h,
				&top, stretch_offset,
				&bottom, w / 2,
				&left, h / 2,
				&right, h / 2);

    if (data)
	free (data);
}

#if !GTK_CHECK_VERSION (3, 0, 0)
static void
gdk_cairo_set_source_color_alpha (cairo_t  *cr,
				  GdkColor *color,
				  double   alpha)
{
    cairo_set_source_rgba (cr,
			   color->red   / 65535.0,
			   color->green / 65535.0,
			   color->blue  / 65535.0,
			   alpha);
}
#endif

static cairo_surface_t *
create_surface (int w,
	        int h)
{
    cairo_surface_t *surface;

    if (w < 0 || h < 0)
	abort ();
    if (w == 0 || h == 0)
	return NULL;

    surface =
      gdk_window_create_similar_surface (gtk_widget_get_window (style_window),
					 CAIRO_CONTENT_COLOR_ALPHA, w, h);

    if (!surface || cairo_surface_get_reference_count (surface) <= 0)
	return NULL;

    if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
	return surface;
    else
    {
	cairo_surface_destroy (surface);
	return NULL;
    }
}

static cairo_surface_t *
new_surface_from_pixbuf (GdkPixbuf *src)
{
    cairo_surface_t *surface;
    guint width, height;
    cairo_t *cr;

    if (!src)
	return NULL;

    width = gdk_pixbuf_get_width (src);
    height = gdk_pixbuf_get_height (src);

    surface = create_surface (width, height);
    if (!surface || cairo_surface_get_reference_count (surface) <= 0)
	return NULL;

    cr = cairo_create (surface);
    gdk_cairo_set_source_pixbuf (cr, src, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy (cr);

    return surface;
}

static gboolean
destroy_surface_idled (gpointer data)
{
    cairo_surface_t *surface = (cairo_surface_t *) data;

    if (surface && cairo_surface_get_reference_count (surface) > 0)
	cairo_surface_destroy (surface);

    return FALSE;
}

#define CORNER_TOPLEFT     (1 << 0)
#define CORNER_TOPRIGHT    (1 << 1)
#define CORNER_BOTTOMRIGHT (1 << 2)
#define CORNER_BOTTOMLEFT  (1 << 3)

static void
rounded_rectangle (cairo_t *cr,
		   double  x,
		   double  y,
		   double  w,
		   double  h,
		   double  radius,
		   int	   corner)
{
    if (corner & CORNER_TOPLEFT)
	cairo_move_to (cr, x + radius, y);
    else
	cairo_move_to (cr, x, y);

    if (corner & CORNER_TOPRIGHT)
	cairo_arc (cr, x + w - radius, y + radius, radius,
		   M_PI * 1.5, M_PI * 2.0);
    else
	cairo_line_to (cr, x + w, y);

    if (corner & CORNER_BOTTOMRIGHT)
	cairo_arc (cr, x + w - radius, y + h - radius, radius,
		   0.0, M_PI * 0.5);
    else
	cairo_line_to (cr, x + w, y + h);

    if (corner & CORNER_BOTTOMLEFT)
	cairo_arc (cr, x + radius, y + h - radius, radius,
		   M_PI * 0.5, M_PI);
    else
	cairo_line_to (cr, x, y + h);

    if (corner & CORNER_TOPLEFT)
	cairo_arc (cr, x + radius, y + radius, radius, M_PI, M_PI * 1.5);
    else
	cairo_line_to (cr, x, y);
}

#define SHADE_LEFT   (1 << 0)
#define SHADE_RIGHT  (1 << 1)
#define SHADE_TOP    (1 << 2)
#define SHADE_BOTTOM (1 << 3)

static void
fill_rounded_rectangle (cairo_t       *cr,
			double        x,
			double        y,
			double        w,
			double        h,
			double	      radius,
			int	      corner,
			decor_color_t *c0,
			double        alpha0,
			decor_color_t *c1,
			double	      alpha1,
			int	      gravity)
{
    cairo_pattern_t *pattern;

    rounded_rectangle (cr, x, y, w, h, radius, corner);

    if (gravity & SHADE_RIGHT)
    {
	x = x + w;
	w = -w;
    }
    else if (!(gravity & SHADE_LEFT))
    {
	x = w = 0;
    }

    if (gravity & SHADE_BOTTOM)
    {
	y = y + h;
	h = -h;
    }
    else if (!(gravity & SHADE_TOP))
    {
	y = h = 0;
    }

    if (w && h)
    {
	cairo_matrix_t matrix;

	pattern = cairo_pattern_create_radial (0.0, 0.0, 0.0, 0.0, 0.0, w);

	cairo_matrix_init_scale (&matrix, 1.0, w / h);
	cairo_matrix_translate (&matrix, -(x + w), -(y + h));

	cairo_pattern_set_matrix (pattern, &matrix);
    }
    else
    {
	pattern = cairo_pattern_create_linear (x + w, y + h, x, y);
    }

    cairo_pattern_add_color_stop_rgba (pattern, 0.0, c0->r, c0->g, c0->b,
				       alpha0);

    cairo_pattern_add_color_stop_rgba (pattern, 1.0, c1->r, c1->g, c1->b,
				       alpha1);

    cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

    cairo_set_source (cr, pattern);
    cairo_fill (cr);
    cairo_pattern_destroy (pattern);
}

static void
draw_shadow_background (decor_t		*d,
			cairo_t		*cr,
			decor_shadow_t  *s,
			decor_context_t *c)
{
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    if (!s || !s->picture ||!d->picture)
    {
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
	cairo_paint (cr);
    }
    else
    {
	decor_fill_picture_extents_with_shadow (xdisplay,
						s, c,
						d->picture,
						&d->border_layout);
    }
}

static void
draw_close_button (decor_t *d,
		   cairo_t *cr,
		   double  s)
{
    cairo_rel_move_to (cr, 0.0, s);

    cairo_rel_line_to (cr, s, -s);
    cairo_rel_line_to (cr, s, s);
    cairo_rel_line_to (cr, s, -s);
    cairo_rel_line_to (cr, s, s);

    cairo_rel_line_to (cr, -s, s);
    cairo_rel_line_to (cr, s, s);
    cairo_rel_line_to (cr, -s, s);
    cairo_rel_line_to (cr, -s, -s);

    cairo_rel_line_to (cr, -s, s);
    cairo_rel_line_to (cr, -s, -s);
    cairo_rel_line_to (cr, s, -s);

    cairo_close_path (cr);
}

static void
draw_max_button (decor_t *d,
		 cairo_t *cr,
		 double  s)
{
    cairo_rel_line_to (cr, 12.0, 0.0);
    cairo_rel_line_to (cr, 0.0, 12.0);
    cairo_rel_line_to (cr, -12.0, 0.0);

    cairo_close_path (cr);

    cairo_rel_move_to (cr, 2.0, s);

    cairo_rel_line_to (cr, 12.0 - 4.0, 0.0);
    cairo_rel_line_to (cr, 0.0, 12.0 - s - 2.0);
    cairo_rel_line_to (cr, -(12.0 - 4.0), 0.0);

    cairo_close_path (cr);
}

static void
draw_unmax_button (decor_t *d,
		   cairo_t *cr,
		   double  s)
{
    cairo_rel_move_to (cr, 1.0, 1.0);

    cairo_rel_line_to (cr, 10.0, 0.0);
    cairo_rel_line_to (cr, 0.0, 10.0);
    cairo_rel_line_to (cr, -10.0, 0.0);

    cairo_close_path (cr);

    cairo_rel_move_to (cr, 2.0, s);

    cairo_rel_line_to (cr, 10.0 - 4.0, 0.0);
    cairo_rel_line_to (cr, 0.0, 10.0 - s - 2.0);
    cairo_rel_line_to (cr, -(10.0 - 4.0), 0.0);

    cairo_close_path (cr);
}

static void
draw_min_button (decor_t *d,
		 cairo_t *cr,
		 double  s)
{
    cairo_rel_move_to (cr, 0.0, 8.0);

    cairo_rel_line_to (cr, 12.0, 0.0);
    cairo_rel_line_to (cr, 0.0, s);
    cairo_rel_line_to (cr, -12.0, 0.0);

    cairo_close_path (cr);
}

typedef void (*draw_proc) (cairo_t *cr);

static void
button_state_offsets (gdouble x,
		      gdouble y,
		      guint   state,
		      gdouble *return_x,
		      gdouble *return_y)
{
    static double off[]	= { 0.0, 0.0, 0.0, 0.5 };

    *return_x  = x + off[state];
    *return_y  = y + off[state];
}

static void
button_state_paint (cairo_t	    *cr,
#if GTK_CHECK_VERSION (3, 0, 0)
		    GtkStyleContext *context,
#else
		    GtkStyle	    *style,
#endif
		    decor_color_t   *color,
		    guint	    state)
{

#define IN_STATE (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW)

#if GTK_CHECK_VERSION (3, 0, 0)
    GdkRGBA fg;

    gtk_style_context_save (context);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_get_color (context, gtk_style_context_get_state (context), &fg);
    gtk_style_context_restore (context);
#endif

    if ((state & IN_STATE) == IN_STATE)
    {
	if (state & IN_EVENT_WINDOW)
	    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	else
	    cairo_set_source_rgba (cr, color->r, color->g, color->b, 0.95);

	cairo_fill_preserve (cr);

#if GTK_CHECK_VERSION (3, 0, 0)
	fg.alpha = STROKE_ALPHA;
	gdk_cairo_set_source_rgba (cr, &fg);
#else
	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  STROKE_ALPHA);
#endif

	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);
	cairo_set_line_width (cr, 2.0);
    }
    else
    {
#if GTK_CHECK_VERSION (3, 0, 0)
	fg.alpha = STROKE_ALPHA;
	gdk_cairo_set_source_rgba (cr, &fg);
#else
	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  STROKE_ALPHA);
#endif
	cairo_stroke_preserve (cr);

	if (state & IN_EVENT_WINDOW)
	    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	else
	    cairo_set_source_rgba (cr, color->r, color->g, color->b, 0.95);

	cairo_fill (cr);
    }
}

static void
copy_to_front_buffer (decor_t *d)
{
    if (!d->buffer_surface)
	return;

    cairo_set_operator (d->cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface (d->cr, d->buffer_surface, 0, 0);
    cairo_paint (d->cr);
}

static void
draw_window_decoration (decor_t *d)
{
    cairo_t         *cr;
#if GTK_CHECK_VERSION (3, 0, 0)
    GtkStyleContext *context;
    GdkRGBA	    bg, fg;
#else
    GtkStyle	    *style;
#endif
    cairo_surface_t *surface;
    decor_color_t   color;
    double          alpha;
    double          x1, y1, x2, y2, x, y, h;
    int		    corners = SHADE_LEFT | SHADE_RIGHT | SHADE_TOP | SHADE_BOTTOM;
    int		    top;
    int		    button_x;

    if (!d->surface)
	return;

#if GTK_CHECK_VERSION (3, 0, 0)
    context = gtk_widget_get_style_context (style_window);

    gtk_style_context_save (context);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_get_background_color (context, gtk_style_context_get_state (context), &bg);
    gtk_style_context_get_color (context, gtk_style_context_get_state (context), &fg);
    gtk_style_context_restore (context);
#else
    style = gtk_widget_get_style (style_window);
#endif

    if (d->state & (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
		    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
	corners = 0;

#if GTK_CHECK_VERSION (3, 0, 0)
    color.r = bg.red;
    color.g = bg.green;
    color.b = bg.blue;
#else
    color.r = style->bg[GTK_STATE_NORMAL].red   / 65535.0;
    color.g = style->bg[GTK_STATE_NORMAL].green / 65535.0;
    color.b = style->bg[GTK_STATE_NORMAL].blue  / 65535.0;
#endif

    if (d->buffer_surface)
	surface = d->buffer_surface;
    else
	surface = d->surface;

    cr = cairo_create (surface);
    if (!cr)
	return;

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    top = _win_extents.top + titlebar_height;

    x1 = d->context->left_space - _win_extents.left;
    y1 = d->context->top_space - _win_extents.top - titlebar_height;
    x2 = d->width - d->context->right_space + _win_extents.right;
    y2 = d->height - d->context->bottom_space + _win_extents.bottom;

    h = d->height - d->context->top_space - d->context->bottom_space;

    cairo_set_line_width (cr, 1.0);

    draw_shadow_background (d, cr, d->shadow, d->context);

    if (d->active)
    {
	decor_color_t *title_color = _title_color;

	alpha = decoration_alpha + 0.3;

	fill_rounded_rectangle (cr,
				x1 + 0.5,
				y1 + 0.5,
				_win_extents.left - 0.5,
				top - 0.5,
				5.0, CORNER_TOPLEFT & corners,
				&title_color[0], 1.0, &title_color[1], alpha,
				SHADE_TOP | SHADE_LEFT);

	fill_rounded_rectangle (cr,
				x1 + _win_extents.left,
				y1 + 0.5,
				x2 - x1 - _win_extents.left -
				_win_extents.right,
				top - 0.5,
				5.0, 0,
				&title_color[0], 1.0, &title_color[1], alpha,
				SHADE_TOP);

	fill_rounded_rectangle (cr,
				x2 - _win_extents.right,
				y1 + 0.5,
				_win_extents.right - 0.5,
				top - 0.5,
				5.0, CORNER_TOPRIGHT & corners,
				&title_color[0], 1.0, &title_color[1], alpha,
				SHADE_TOP | SHADE_RIGHT);
    }
    else
    {
	alpha = decoration_alpha;

	fill_rounded_rectangle (cr,
				x1 + 0.5,
				y1 + 0.5,
				_win_extents.left - 0.5,
				top - 0.5,
				5.0, CORNER_TOPLEFT & corners,
				&color, 1.0, &color, alpha,
				SHADE_TOP | SHADE_LEFT);

	fill_rounded_rectangle (cr,
				x1 + _win_extents.left,
				y1 + 0.5,
				x2 - x1 - _win_extents.left -
				_win_extents.right,
				top - 0.5,
				5.0, 0,
				&color, 1.0, &color, alpha,
				SHADE_TOP);

	fill_rounded_rectangle (cr,
				x2 - _win_extents.right,
				y1 + 0.5,
				_win_extents.right - 0.5,
				top - 0.5,
				5.0, CORNER_TOPRIGHT & corners,
				&color, 1.0, &color, alpha,
				SHADE_TOP | SHADE_RIGHT);
    }

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + top,
			    _win_extents.left - 0.5,
			    h,
			    5.0, 0,
			    &color, 1.0, &color, alpha,
			    SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y1 + top,
			    _win_extents.right - 0.5,
			    h,
			    5.0, 0,
			    &color, 1.0, &color, alpha,
			    SHADE_RIGHT);


    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y2 - _win_extents.bottom,
			    _win_extents.left - 0.5,
			    _win_extents.bottom - 0.5,
			    5.0, CORNER_BOTTOMLEFT & corners,
			    &color, 1.0, &color, alpha,
			    SHADE_BOTTOM | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + _win_extents.left,
			    y2 - _win_extents.bottom,
			    x2 - x1 - _win_extents.left -
			    _win_extents.right,
			    _win_extents.bottom - 0.5,
			    5.0, 0,
			    &color, 1.0, &color, alpha,
			    SHADE_BOTTOM);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y2 - _win_extents.bottom,
			    _win_extents.right - 0.5,
			    _win_extents.bottom - 0.5,
			    5.0, CORNER_BOTTOMRIGHT & corners,
			    &color, 1.0, &color, alpha,
			    SHADE_BOTTOM | SHADE_RIGHT);

    cairo_rectangle (cr,
		     d->context->left_space,
		     d->context->top_space,
		     d->width - d->context->left_space -
		     d->context->right_space,
		     h);
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_cairo_set_source_rgba (cr, &bg);
#else
    gdk_cairo_set_source_color (cr, &style->bg[GTK_STATE_NORMAL]);
#endif
    cairo_fill (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    if (d->active)
    {
#if GTK_CHECK_VERSION (3, 0, 0)
	fg.alpha = 0.7;
	gdk_cairo_set_source_rgba (cr, &fg);
#else
	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  0.7);
#endif

	cairo_move_to (cr, x1 + 0.5, y1 + top - 0.5);
	cairo_rel_line_to (cr, x2 - x1 - 1.0, 0.0);

	cairo_stroke (cr);
    }

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       (CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
			CORNER_BOTTOMRIGHT) & corners);

    cairo_clip (cr);

    cairo_translate (cr, 1.0, 1.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       (CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
			CORNER_BOTTOMRIGHT) & corners);

    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.4);
    cairo_stroke (cr);

    cairo_translate (cr, -2.0, -2.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       (CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
			CORNER_BOTTOMRIGHT) & corners);

    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.1);
    cairo_stroke (cr);

    cairo_translate (cr, 1.0, 1.0);

    cairo_reset_clip (cr);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       (CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
			CORNER_BOTTOMRIGHT) & corners);

#if GTK_CHECK_VERSION (3, 0, 0)
    fg.alpha = alpha;
    gdk_cairo_set_source_rgba (cr, &fg);
#else
    gdk_cairo_set_source_color_alpha (cr,
				      &style->fg[GTK_STATE_NORMAL],
				      alpha);
#endif

    cairo_stroke (cr);

    cairo_set_line_width (cr, 2.0);

    button_x = d->width - d->context->right_space - 13;

    if (d->actions & WNCK_WINDOW_ACTION_CLOSE)
    {
	button_state_offsets (button_x,
			      y1 - 3.0 + titlebar_height / 2,
			      d->button_states[BUTTON_CLOSE], &x, &y);

	button_x -= 17;

	if (d->active)
	{
	    cairo_move_to (cr, x, y);
	    draw_close_button (d, cr, 3.0);
#if GTK_CHECK_VERSION (3, 0, 0)
	    button_state_paint (cr, context, &color,
#else
	    button_state_paint (cr, style, &color,
#endif
				d->button_states[BUTTON_CLOSE]);
	}
	else
	{
#if GTK_CHECK_VERSION (3, 0, 0)
	    fg.alpha = alpha * 0.75;
	    gdk_cairo_set_source_rgba (cr, &fg);
#else
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha * 0.75);
#endif
	    cairo_move_to (cr, x, y);
	    draw_close_button (d, cr, 3.0);
	    cairo_fill (cr);
	}
    }

    if (d->actions & WNCK_WINDOW_ACTION_MAXIMIZE)
    {
	button_state_offsets (button_x,
			      y1 - 3.0 + titlebar_height / 2,
			      d->button_states[BUTTON_MAX], &x, &y);

	button_x -= 17;

	cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);

	if (d->active)
	{
#if GTK_CHECK_VERSION (3, 0, 0)
	    fg.alpha = STROKE_ALPHA;
	    gdk_cairo_set_source_rgba (cr, &fg);
#else
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      STROKE_ALPHA);
#endif
	    cairo_move_to (cr, x, y);

	    if (d->state & (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
			    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
		draw_unmax_button (d, cr, 4.0);
	    else
		draw_max_button (d, cr, 4.0);

#if GTK_CHECK_VERSION (3, 0, 0)
	    button_state_paint (cr, context, &color,
#else
	    button_state_paint (cr, style, &color,
#endif
				d->button_states[BUTTON_MAX]);
	}
	else
	{
#if GTK_CHECK_VERSION (3, 0, 0)
	    fg.alpha = alpha * 0.75;
	    gdk_cairo_set_source_rgba (cr, &fg);
#else
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha * 0.75);
#endif
	    cairo_move_to (cr, x, y);

	    if (d->state & (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
			    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
		draw_unmax_button (d, cr, 4.0);
	    else
		draw_max_button (d, cr, 4.0);

	    cairo_fill (cr);
	}
    }

    if (d->actions & WNCK_WINDOW_ACTION_MINIMIZE)
    {
	button_state_offsets (button_x,
			      y1 - 3.0 + titlebar_height / 2,
			      d->button_states[BUTTON_MIN], &x, &y);

	button_x -= 17;

	if (d->active)
	{
#if GTK_CHECK_VERSION (3, 0, 0)
	    fg.alpha = STROKE_ALPHA;
	    gdk_cairo_set_source_rgba (cr, &fg);
#else
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      STROKE_ALPHA);
#endif
	    cairo_move_to (cr, x, y);
	    draw_min_button (d, cr, 4.0);
#if GTK_CHECK_VERSION (3, 0, 0)
	    button_state_paint (cr, context, &color,
#else
	    button_state_paint (cr, style, &color,
#endif
				d->button_states[BUTTON_MIN]);
	}
	else
	{
#if GTK_CHECK_VERSION (3, 0, 0)
	    fg.alpha = alpha * 0.75;
	    gdk_cairo_set_source_rgba (cr, &fg);
#else
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha * 0.75);
#endif
	    cairo_move_to (cr, x, y);
	    draw_min_button (d, cr, 4.0);
	    cairo_fill (cr);
	}
    }

    if (d->layout)
    {
	if (d->active)
	{
	    cairo_move_to (cr,
			   d->context->left_space + 21.0,
			   y1 + 2.0 + (titlebar_height - text_height) / 2.0);

#if GTK_CHECK_VERSION (3, 0, 0)
	    fg.alpha = STROKE_ALPHA;
	    gdk_cairo_set_source_rgba (cr, &fg);
#else
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      STROKE_ALPHA);
#endif

	    pango_cairo_update_layout (cr, d->layout);
	    pango_cairo_layout_path (cr, d->layout);
	    cairo_stroke (cr);

	    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	}
	else
	{
#if GTK_CHECK_VERSION (3, 0, 0)
	    fg.alpha = alpha;
	    gdk_cairo_set_source_rgba (cr, &fg);
#else
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha);
#endif
	}

	cairo_move_to (cr,
		       d->context->left_space + 21.0,
		       y1 + 2.0 + (titlebar_height - text_height) / 2.0);

	pango_cairo_show_layout (cr, d->layout);
    }

    if (d->icon)
    {
	cairo_translate (cr, d->context->left_space + 1,
			 y1 - 5.0 + titlebar_height / 2);
	cairo_set_source (cr, d->icon);
	cairo_rectangle (cr, 0.0, 0.0, 16.0, 16.0);
	cairo_clip (cr);

	if (d->active)
	    cairo_paint (cr);
	else
	    cairo_paint_with_alpha (cr, alpha);
    }

    cairo_destroy (cr);

    copy_to_front_buffer (d);

    if (d->prop_xid)
    {
	decor_update_window_property (d);
	d->prop_xid = 0;
    }
}

#ifdef USE_MARCO
static void
decor_update_meta_window_property (decor_t	  *d,
				   MetaTheme	  *theme,
				   MetaFrameFlags flags,
				   Region	  top,
				   Region	  bottom,
				   Region	  left,
				   Region	  right)
{
    long	    *data = NULL;
    unsigned int    n = 1, frame_type = 0, frame_state = 0, frame_actions = 0;
    Display	    *xdisplay =
	GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    gint	    nQuad;
    decor_extents_t extents, max_extents;
    decor_quad_t    quads[N_QUADS_MAX];
    gint            w, lh, rh;
    gint	    top_stretch_offset;
    gint	    bottom_stretch_offset;
    gint	    left_stretch_offset;
    gint	    right_stretch_offset;

    w = d->border_layout.top.x2 - d->border_layout.top.x1 -
	d->context->left_space - d->context->right_space;

    if (d->border_layout.rotation)
	lh = d->border_layout.left.x2 - d->border_layout.left.x1;
    else
	lh = d->border_layout.left.y2 - d->border_layout.left.y1;

    if (d->border_layout.rotation)
	rh = d->border_layout.right.x2 - d->border_layout.right.x1;
    else
	rh = d->border_layout.right.y2 - d->border_layout.right.y1;

    left_stretch_offset   = lh / 2;
    right_stretch_offset  = rh / 2;
    top_stretch_offset    = w - d->button_width - 1;
    bottom_stretch_offset = (d->border_layout.bottom.x2 -
			     d->border_layout.bottom.x1 -
			     d->context->left_space -
			     d->context->right_space) / 2;

    nQuad = decor_set_lXrXtXbX_window_quads (quads, d->context,
					     &d->border_layout,
					     left_stretch_offset,
					     right_stretch_offset,
					     top_stretch_offset,
					     bottom_stretch_offset);

    extents = _win_extents;
    max_extents = _max_win_extents;

    extents.top += titlebar_height;
    max_extents.top += max_titlebar_height;

    data = decor_alloc_property (n, WINDOW_DECORATION_TYPE_PIXMAP);
    decor_quads_to_property (data, n - 1,
			     cairo_xlib_surface_get_drawable (d->surface),
			     &extents, &extents, &max_extents, &max_extents,
			     ICON_SPACE + d->button_width, 0, quads, nQuad,
			     frame_type, frame_state, frame_actions);

    gdk_error_trap_push ();
    XChangeProperty (xdisplay, d->prop_xid,
		     win_decor_atom,
		     XA_INTEGER,
		     32, PropModeReplace, (guchar *) data,
		     PROP_HEADER_SIZE + BASE_PROP_SIZE +
		       QUAD_PROP_SIZE * N_QUADS_MAX);
    gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif

    decor_update_blur_property (d,
				w, lh,
				top, top_stretch_offset,
				bottom, bottom_stretch_offset,
				left, left_stretch_offset,
				right, right_stretch_offset);

    if (data)
	free (data);
}

static void
meta_get_corner_radius (const MetaFrameGeometry *fgeom,
			int		        *top_left_radius,
			int			*top_right_radius,
			int		        *bottom_left_radius,
			int			*bottom_right_radius)
{
    *top_left_radius     = fgeom->top_left_corner_rounded_radius;
    *top_right_radius    = fgeom->top_right_corner_rounded_radius;
    *bottom_left_radius  = fgeom->bottom_left_corner_rounded_radius;
    *bottom_right_radius = fgeom->bottom_right_corner_rounded_radius;
}

static int
radius_to_width (int radius,
		 int i)
{
    float r1 = sqrt (radius) + radius;
    float r2 = r1 * r1 - (r1 - (i + 0.5)) * (r1 - (i + 0.5));

    return floor (0.5f + r1 - sqrt (r2));
}

static Region
meta_get_top_border_region (const MetaFrameGeometry *fgeom,
			    int			    width)
{
    Region     corners_xregion, border_xregion;
    XRectangle xrect;
    int	       top_left_radius;
    int	       top_right_radius;
    int	       bottom_left_radius;
    int	       bottom_right_radius;
    int	       w, i;

    corners_xregion = XCreateRegion ();

    meta_get_corner_radius (fgeom,
			    &top_left_radius,
			    &top_right_radius,
			    &bottom_left_radius,
			    &bottom_right_radius);

    if (top_left_radius)
    {
	for (i = 0; i < top_left_radius; i++)
	{
	    w = radius_to_width (top_left_radius, i);

	    xrect.x	 = 0;
	    xrect.y	 = i;
	    xrect.width  = w;
	    xrect.height = 1;

	    XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
	}
    }

    if (top_right_radius)
    {
	for (i = 0; i < top_right_radius; i++)
	{
	    w = radius_to_width (top_right_radius, i);

	    xrect.x	 = width - w;
	    xrect.y	 = i;
	    xrect.width  = w;
	    xrect.height = 1;

	    XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
	}
    }

    border_xregion = XCreateRegion ();

    xrect.x = 0;
    xrect.y = 0;
    xrect.width = width;
    xrect.height = fgeom->top_height;

    XUnionRectWithRegion (&xrect, border_xregion, border_xregion);

    XSubtractRegion (border_xregion, corners_xregion, border_xregion);

    XDestroyRegion (corners_xregion);

    return border_xregion;
}

static Region
meta_get_bottom_border_region (const MetaFrameGeometry *fgeom,
			       int		        width)
{
    Region     corners_xregion, border_xregion;
    XRectangle xrect;
    int	       top_left_radius;
    int	       top_right_radius;
    int	       bottom_left_radius;
    int	       bottom_right_radius;
    int	       w, i;

    corners_xregion = XCreateRegion ();

    meta_get_corner_radius (fgeom,
			    &top_left_radius,
			    &top_right_radius,
			    &bottom_left_radius,
			    &bottom_right_radius);

    if (bottom_left_radius)
    {
	for (i = 0; i < bottom_left_radius; i++)
	{
	    w = radius_to_width (bottom_left_radius, i);

	    xrect.x	 = 0;
	    xrect.y	 = fgeom->bottom_height - i - 1;
	    xrect.width  = w;
	    xrect.height = 1;

	    XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
	}
    }

    if (bottom_right_radius)
    {
	for (i = 0; i < bottom_right_radius; i++)
	{
	    w = radius_to_width (bottom_right_radius, i);

	    xrect.x	 = width - w;
	    xrect.y	 = fgeom->bottom_height - i - 1;
	    xrect.width  = w;
	    xrect.height = 1;

	    XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
	}
    }

    border_xregion = XCreateRegion ();

    xrect.x = 0;
    xrect.y = 0;
    xrect.width = width;
    xrect.height = fgeom->bottom_height;

    XUnionRectWithRegion (&xrect, border_xregion, border_xregion);

    XSubtractRegion (border_xregion, corners_xregion, border_xregion);

    XDestroyRegion (corners_xregion);

    return border_xregion;
}

static Region
meta_get_left_border_region (const MetaFrameGeometry *fgeom,
			     int		     height)
{
    Region     border_xregion;
    XRectangle xrect;

    border_xregion = XCreateRegion ();

    xrect.x	 = 0;
    xrect.y	 = 0;
    xrect.width  = fgeom->left_width;
    xrect.height = height - fgeom->top_height - fgeom->bottom_height;

    XUnionRectWithRegion (&xrect, border_xregion, border_xregion);

    return border_xregion;
}

static Region
meta_get_right_border_region (const MetaFrameGeometry *fgeom,
			      int		      height)
{
    Region     border_xregion;
    XRectangle xrect;

    border_xregion = XCreateRegion ();

    xrect.x	 = 0;
    xrect.y	 = 0;
    xrect.width  = fgeom->right_width;
    xrect.height = height - fgeom->top_height - fgeom->bottom_height;

    XUnionRectWithRegion (&xrect, border_xregion, border_xregion);

    return border_xregion;
}

static MetaButtonState
meta_button_state (int state)
{
    if (state & IN_EVENT_WINDOW)
    {
	if (state & PRESSED_EVENT_WINDOW)
	    return META_BUTTON_STATE_PRESSED;

	return META_BUTTON_STATE_PRELIGHT;
    }

    return META_BUTTON_STATE_NORMAL;
}

static MetaButtonType
meta_function_to_type (MetaButtonFunction function)
{
    switch (function) {
    case META_BUTTON_FUNCTION_MENU:
	return META_BUTTON_TYPE_MENU;
    case META_BUTTON_FUNCTION_MINIMIZE:
	return META_BUTTON_TYPE_MINIMIZE;
    case META_BUTTON_FUNCTION_MAXIMIZE:
	return META_BUTTON_TYPE_MAXIMIZE;
    case META_BUTTON_FUNCTION_CLOSE:
	return META_BUTTON_TYPE_CLOSE;
    case META_BUTTON_FUNCTION_SHADE:
	return META_BUTTON_TYPE_SHADE;
    case META_BUTTON_FUNCTION_ABOVE:
	return META_BUTTON_TYPE_ABOVE;
    case META_BUTTON_FUNCTION_STICK:
	return META_BUTTON_TYPE_STICK;
    case META_BUTTON_FUNCTION_UNSHADE:
	return META_BUTTON_TYPE_UNSHADE;
    case META_BUTTON_FUNCTION_UNABOVE:
	return META_BUTTON_TYPE_UNABOVE;
    case META_BUTTON_FUNCTION_UNSTICK:
	return META_BUTTON_TYPE_UNSTICK;

    default:
	break;
    }

    return META_BUTTON_TYPE_LAST;
}

static MetaButtonState
meta_button_state_for_button_type (decor_t	  *d,
				   MetaButtonType type)
{
    switch (type) {
    case META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND:
	type = meta_function_to_type (meta_button_layout.left_buttons[0]);
	break;
    case META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND:
	type = meta_function_to_type (meta_button_layout.left_buttons[1]);
	break;
    case META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND:
	type = meta_function_to_type (meta_button_layout.left_buttons[2]);
	break;
    case META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND:
	type = meta_function_to_type (meta_button_layout.right_buttons[0]);
	break;
    case META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND:
	type = meta_function_to_type (meta_button_layout.right_buttons[1]);
	break;
    case META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND:
	type = meta_function_to_type (meta_button_layout.right_buttons[2]);
    default:
	break;
    }

    switch (type) {
    case META_BUTTON_TYPE_CLOSE:
	return meta_button_state (d->button_states[BUTTON_CLOSE]);
    case META_BUTTON_TYPE_MAXIMIZE:
	return meta_button_state (d->button_states[BUTTON_MAX]);
    case META_BUTTON_TYPE_MINIMIZE:
	return meta_button_state (d->button_states[BUTTON_MIN]);
    case META_BUTTON_TYPE_MENU:
	return meta_button_state (d->button_states[BUTTON_MENU]);
    case META_BUTTON_TYPE_SHADE:
	return meta_button_state (d->button_states[BUTTON_SHADE]);
    case META_BUTTON_TYPE_ABOVE:
	return meta_button_state (d->button_states[BUTTON_ABOVE]);
    case META_BUTTON_TYPE_STICK:
	return meta_button_state (d->button_states[BUTTON_STICK]);
    case META_BUTTON_TYPE_UNSHADE:
	return meta_button_state (d->button_states[BUTTON_UNSHADE]);
    case META_BUTTON_TYPE_UNABOVE:
	return meta_button_state (d->button_states[BUTTON_UNABOVE]);
    case META_BUTTON_TYPE_UNSTICK:
	return meta_button_state (d->button_states[BUTTON_UNSTICK]);

    default:
	break;
    }

    return META_BUTTON_STATE_NORMAL;
}

static void
meta_get_decoration_geometry (decor_t		*d,
			      MetaTheme	        *theme,
			      MetaFrameFlags    *flags,
			      MetaFrameGeometry *fgeom,
			      MetaButtonLayout  *button_layout,
			      GdkRectangle      *clip)
{
    gint left_width, right_width, top_height, bottom_height;

    if (meta_button_layout_set)
    {
	*button_layout = meta_button_layout;
    }
    else
    {
	gint i;

	button_layout->left_buttons[0] = META_BUTTON_FUNCTION_MENU;

	for (i = 1; i < MAX_BUTTONS_PER_CORNER; i++)
	    button_layout->left_buttons[i] = META_BUTTON_FUNCTION_LAST;

	button_layout->right_buttons[0] = META_BUTTON_FUNCTION_MINIMIZE;
	button_layout->right_buttons[1] = META_BUTTON_FUNCTION_MAXIMIZE;
	button_layout->right_buttons[2] = META_BUTTON_FUNCTION_CLOSE;

	for (i = 3; i < MAX_BUTTONS_PER_CORNER; i++)
	    button_layout->right_buttons[i] = META_BUTTON_FUNCTION_LAST;
    }

    *flags = 0;

    if (d->actions & WNCK_WINDOW_ACTION_CLOSE)
	*flags |= META_FRAME_ALLOWS_DELETE;

    if (d->actions & WNCK_WINDOW_ACTION_MINIMIZE)
	*flags |= META_FRAME_ALLOWS_MINIMIZE;

    if (d->actions & WNCK_WINDOW_ACTION_MAXIMIZE)
	*flags |= META_FRAME_ALLOWS_MAXIMIZE;

    *flags |= META_FRAME_ALLOWS_MENU;

    if (d->actions & WNCK_WINDOW_ACTION_RESIZE)
    {
	if (!(d->state & WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
	    *flags |= META_FRAME_ALLOWS_VERTICAL_RESIZE;
	if (!(d->state & WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY))
	    *flags |= META_FRAME_ALLOWS_HORIZONTAL_RESIZE;
    }

    if (d->actions & WNCK_WINDOW_ACTION_MOVE)
	*flags |= META_FRAME_ALLOWS_MOVE;

    if (d->actions & WNCK_WINDOW_ACTION_MAXIMIZE)
	*flags |= META_FRAME_ALLOWS_MAXIMIZE;

    if (d->actions & WNCK_WINDOW_ACTION_SHADE)
	*flags |= META_FRAME_ALLOWS_SHADE;

    if (d->active)
	*flags |= META_FRAME_HAS_FOCUS;

#define META_MAXIMIZED (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY | \
			WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY)

    if ((d->state & META_MAXIMIZED) == META_MAXIMIZED)
	*flags |= META_FRAME_MAXIMIZED;

    if (d->state & WNCK_WINDOW_STATE_STICKY)
	*flags |= META_FRAME_STUCK;

    if (d->state & WNCK_WINDOW_STATE_FULLSCREEN)
	*flags |= META_FRAME_FULLSCREEN;

    if (d->state & WNCK_WINDOW_STATE_SHADED)
	*flags |= META_FRAME_SHADED;

    if (d->state & WNCK_WINDOW_STATE_ABOVE)
	*flags |= META_FRAME_ABOVE;

    meta_theme_get_frame_borders (theme,
				  META_FRAME_TYPE_NORMAL,
				  text_height,
				  *flags,
				  &top_height,
				  &bottom_height,
				  &left_width,
				  &right_width);

    clip->x = d->context->left_space - left_width;
    clip->y = d->context->top_space - top_height;

    clip->width = d->border_layout.top.x2 - d->border_layout.top.x1;
    clip->width -= d->context->right_space + d->context->left_space;

    if (d->border_layout.rotation)
	clip->height = d->border_layout.left.x2 - d->border_layout.left.x1;
    else
	clip->height = d->border_layout.left.y2 - d->border_layout.left.y1;

    meta_theme_calc_geometry (theme,
			      META_FRAME_TYPE_NORMAL,
			      text_height,
			      *flags,
			      clip->width,
			      clip->height,
			      button_layout,
			      fgeom);

    clip->width  += left_width + right_width;
    clip->height += top_height + bottom_height;
}

static void
meta_draw_window_decoration (decor_t *d)
{
    Display	      *xdisplay =
	GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
    cairo_surface_t   *surface;
    GtkStyleContext   *context;
#else
    GdkPixmap	      *pixmap;
    GtkStyle	      *style;
#endif
    Picture	      src;
    MetaButtonState   button_states[META_BUTTON_TYPE_LAST];
    MetaButtonLayout  button_layout;
    MetaFrameGeometry fgeom;
    MetaFrameFlags    flags;
    MetaTheme	      *theme;
    cairo_t	      *cr;
    gint	      i;
    GdkRectangle      clip;
    Region	      top_region = NULL;
    Region	      bottom_region = NULL;
    Region	      left_region = NULL;
    Region	      right_region = NULL;
    double	      alpha = (d->active) ? meta_active_opacity : meta_opacity;
    gboolean	      shade_alpha = (d->active) ? meta_active_shade_opacity :
	meta_shade_opacity;
    MetaFrameStyle    *frame_style;
#if GTK_CHECK_VERSION (3, 0, 0)
    GdkRGBA	      bg;
#else
    GdkColor	      bg_color;
    double	      bg_alpha;
#endif

    if (!d->surface || !d->picture)
	return;

    if (decoration_alpha == 1.0)
	alpha = 1.0;

#if GTK_CHECK_VERSION (3, 0, 0)
    context = gtk_widget_get_style_context (style_window);
#else
    style = gtk_widget_get_style (style_window);
#endif

    cr = cairo_create (d->buffer_surface ? d->buffer_surface : d->surface);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    theme = meta_theme_get_current ();

    meta_get_decoration_geometry (d, theme, &flags, &fgeom, &button_layout,
				  &clip);

    /* we only have to redraw the shadow background when decoration
       changed size */
    if (d->prop_xid || !d->buffer_surface)
	draw_shadow_background (d, cr, d->shadow, d->context);

    for (i = 0; i < META_BUTTON_TYPE_LAST; i++)
	button_states[i] = meta_button_state_for_button_type (d, i);

    frame_style = meta_theme_get_frame_style (theme,
					      META_FRAME_TYPE_NORMAL,
					      flags);

#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_style_context_save (context);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_get_background_color (context, gtk_style_context_get_state (context), &bg);
    gtk_style_context_restore (context);

    bg.alpha = 1.0;
#else
    bg_color = style->bg[GTK_STATE_NORMAL];
    bg_alpha = 1.0;
#endif

    if (frame_style->window_background_color)
    {
	meta_color_spec_render (frame_style->window_background_color,
#if GTK_CHECK_VERSION (3, 0, 0)
				gtk_widget_get_style_context (style_window),
				&bg);

	bg.alpha = frame_style->window_background_alpha / 255.0;
    }
#else
				gtk_widget_get_style (style_window),
				&bg_color);

	bg_alpha = frame_style->window_background_alpha / 255.0;
    }
#endif

    /* Draw something that will be almost invisible to user. This is hacky way
     * to fix invisible decorations. */
    cairo_set_source_rgba (cr, 0, 0, 0, 0.01);
    cairo_rectangle (cr, 0, 0, 1, 1);
    cairo_fill (cr);
    /* ------------ */

    cairo_destroy (cr);

    if (clip.width != 0 && clip.height != 0 &&
#if GTK_CHECK_VERSION (3, 0, 0)
	(surface = create_surface (clip.width, clip.height)))
    {
	cr = cairo_create (surface);
	gdk_cairo_set_source_rgba (cr, &bg);
#else
	(pixmap = gdk_pixmap_new (GDK_DRAWABLE (gtk_widget_get_window (style_window)),
				  clip.width, clip.height, -1)))
    {
	cr = gdk_cairo_create (GDK_DRAWABLE (pixmap));
	gdk_cairo_set_source_color_alpha (cr, &bg_color, bg_alpha);
#endif
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	src = XRenderCreatePicture (xdisplay,
#if GTK_CHECK_VERSION (3, 0, 0)
				    cairo_xlib_surface_get_drawable (surface),
#else
				    GDK_PIXMAP_XID (pixmap),
#endif
				    xformat, 0, NULL);

	cairo_paint (cr);

	meta_theme_draw_frame (theme,
			       style_window,
#if GTK_CHECK_VERSION (3, 0, 0)
			       cr,
#else
			       pixmap,
			       NULL,
			       0, 0,
#endif
			       META_FRAME_TYPE_NORMAL,
			       flags,
			       clip.width - fgeom.left_width - fgeom.right_width,
			       clip.height - fgeom.top_height - fgeom.bottom_height,
			       d->layout,
			       text_height,
			       &button_layout,
			       button_states,
			       d->icon_pixbuf,
			       NULL);

	if (fgeom.top_height)
	{
	    top_region = meta_get_top_border_region (&fgeom, clip.width);

	    decor_blend_border_picture (xdisplay,
					d->context,
					src,
					0, 0,
					d->picture,
					&d->border_layout,
					BORDER_TOP,
					top_region,
					alpha * 0xffff,
					shade_alpha,
					0);
	}

	if (fgeom.bottom_height)
	{
	    bottom_region = meta_get_bottom_border_region (&fgeom, clip.width);

	    decor_blend_border_picture (xdisplay,
					d->context,
					src,
					0, clip.height - fgeom.bottom_height,
					d->picture,
					&d->border_layout,
					BORDER_BOTTOM,
					bottom_region,
					alpha * 0xffff,
					shade_alpha,
					0);
	}

	if (fgeom.left_width)
	{
	    left_region = meta_get_left_border_region (&fgeom, clip.height);

	    decor_blend_border_picture (xdisplay,
					d->context,
					src,
					0, fgeom.top_height,
					d->picture,
					&d->border_layout,
					BORDER_LEFT,
					left_region,
					alpha * 0xffff,
					shade_alpha,
					0);
	}

	if (fgeom.right_width)
	{
	    right_region = meta_get_right_border_region (&fgeom, clip.height);

	    decor_blend_border_picture (xdisplay,
					d->context,
					src,
					clip.width - fgeom.right_width, fgeom.top_height,
					d->picture,
					&d->border_layout,
					BORDER_RIGHT,
					right_region,
					alpha * 0xffff,
					shade_alpha,
					0);
	}

	cairo_destroy (cr);

#if GTK_CHECK_VERSION (3, 0, 0)
	cairo_surface_destroy (surface);
#else
	g_object_unref (G_OBJECT (pixmap));
#endif

	XRenderFreePicture (xdisplay, src);
    }

    copy_to_front_buffer (d);

    if (d->prop_xid)
    {
	/* translate from frame to client window space */
	if (top_region)
	    XOffsetRegion (top_region, -fgeom.left_width, -fgeom.top_height);
	if (bottom_region)
	    XOffsetRegion (bottom_region, -fgeom.left_width, 0);
	if (left_region)
	    XOffsetRegion (left_region, -fgeom.left_width, 0);

	decor_update_meta_window_property (d, theme, flags,
					   top_region,
					   bottom_region,
					   left_region,
					   right_region);
	d->prop_xid = 0;
    }

    if (top_region)
	XDestroyRegion (top_region);
    if (bottom_region)
	XDestroyRegion (bottom_region);
    if (left_region)
	XDestroyRegion (left_region);
    if (right_region)
	XDestroyRegion (right_region);
}
#endif

#define SWITCHER_ALPHA 0xa0a0

static void
decor_update_switcher_property (decor_t *d)
{
    long	    *data = NULL;
    unsigned int    n = 1, frame_type = 0, frame_state = 0, frame_actions = 0;
    Display	    *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    gint	    nQuad;
    decor_quad_t    quads[N_QUADS_MAX];
#if GTK_CHECK_VERSION (3, 0, 0)
    GtkStyleContext *context;
    GdkRGBA fg;
#else
    GtkStyle        *style;
#endif
    long            fgColor[4];

    nQuad = decor_set_lSrStSbX_window_quads (quads, &switcher_context,
					     &d->border_layout,
					     d->border_layout.top.x2 -
					     d->border_layout.top.x1 -
					     switcher_context.extents.left -
					     switcher_context.extents.right -
					     32);

    data = decor_alloc_property (n, WINDOW_DECORATION_TYPE_PIXMAP);
    decor_quads_to_property (data, n - 1,
			     cairo_xlib_surface_get_drawable (d->surface),
			     &_switcher_extents, &_switcher_extents,
			     &_switcher_extents, &_switcher_extents,
			     0, 0, quads, nQuad,
			     frame_type, frame_state, frame_actions);

#if GTK_CHECK_VERSION (3, 0, 0)
    context = gtk_widget_get_style_context (style_window);

    gtk_style_context_save (context);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_get_color (context, gtk_style_context_get_state (context), &fg);
    gtk_style_context_restore (context);

    fgColor[0] = fg.red;
    fgColor[1] = fg.green;
    fgColor[2] = fg.blue;
#else
    style = gtk_widget_get_style (style_window);

    fgColor[0] = style->fg[GTK_STATE_NORMAL].red;
    fgColor[1] = style->fg[GTK_STATE_NORMAL].green;
    fgColor[2] = style->fg[GTK_STATE_NORMAL].blue;
#endif
    fgColor[3] = SWITCHER_ALPHA;

    gdk_error_trap_push ();
    XChangeProperty (xdisplay, d->prop_xid,
		     win_decor_atom,
		     XA_INTEGER,
		     32, PropModeReplace, (guchar *) data,
		     PROP_HEADER_SIZE + BASE_PROP_SIZE +
		       QUAD_PROP_SIZE * N_QUADS_MAX);
    XChangeProperty (xdisplay, d->prop_xid, switcher_fg_atom,
		     XA_INTEGER, 32, PropModeReplace, (guchar *) fgColor, 4);
    gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif

    if (data)
	free (data);
}

static void
draw_switcher_background (decor_t *d)
{
    Display	    *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    cairo_t	    *cr;
#if GTK_CHECK_VERSION (3, 0, 0)
    GtkStyleContext *context;
    GdkRGBA	    bg, fg;
#else
    GtkStyle	    *style;
#endif
    decor_color_t   color;
    double	    alpha = SWITCHER_ALPHA / 65535.0;
    double	    x1, y1, x2, y2, h;
    int		    top;
    unsigned long   pixel;
    ushort	    a = SWITCHER_ALPHA;

    if (!d->buffer_surface)
	return;

#if GTK_CHECK_VERSION (3, 0, 0)
    context = gtk_widget_get_style_context (style_window);

    gtk_style_context_save (context);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_get_background_color (context, gtk_style_context_get_state (context), &bg);
    gtk_style_context_get_color (context, gtk_style_context_get_state (context), &fg);
    gtk_style_context_restore (context);

    color.r = bg.red;
    color.g = bg.green;
    color.b = bg.blue;
#else
    style = gtk_widget_get_style (style_window);

    color.r = style->bg[GTK_STATE_NORMAL].red   / 65535.0;
    color.g = style->bg[GTK_STATE_NORMAL].green / 65535.0;
    color.b = style->bg[GTK_STATE_NORMAL].blue  / 65535.0;
#endif

    cr = cairo_create (d->buffer_surface);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    top = _switcher_extents.top;

    x1 = switcher_context.left_space - _switcher_extents.left;
    y1 = switcher_context.top_space - _switcher_extents.top;
    x2 = d->width - switcher_context.right_space + _switcher_extents.right;
    y2 = d->height - switcher_context.bottom_space + _switcher_extents.bottom;

    h = y2 - y1 - _switcher_extents.top - _switcher_extents.top;

    cairo_set_line_width (cr, 1.0);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    draw_shadow_background (d, cr, switcher_shadow, &switcher_context);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + 0.5,
			    _switcher_extents.left - 0.5,
			    top - 0.5,
			    5.0, CORNER_TOPLEFT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + _switcher_extents.left,
			    y1 + 0.5,
			    x2 - x1 - _switcher_extents.left -
			    _switcher_extents.right,
			    top - 0.5,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP);

    fill_rounded_rectangle (cr,
			    x2 - _switcher_extents.right,
			    y1 + 0.5,
			    _switcher_extents.right - 0.5,
			    top - 0.5,
			    5.0, CORNER_TOPRIGHT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP | SHADE_RIGHT);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + top,
			    _switcher_extents.left - 0.5,
			    h,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x2 - _switcher_extents.right,
			    y1 + top,
			    _switcher_extents.right - 0.5,
			    h,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_RIGHT);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y2 - _switcher_extents.top,
			    _switcher_extents.left - 0.5,
			    _switcher_extents.top - 0.5,
			    5.0, CORNER_BOTTOMLEFT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + _switcher_extents.left,
			    y2 - _switcher_extents.top,
			    x2 - x1 - _switcher_extents.left -
			    _switcher_extents.right,
			    _switcher_extents.top - 0.5,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM);

    fill_rounded_rectangle (cr,
			    x2 - _switcher_extents.right,
			    y2 - _switcher_extents.top,
			    _switcher_extents.right - 0.5,
			    _switcher_extents.top - 0.5,
			    5.0, CORNER_BOTTOMRIGHT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM | SHADE_RIGHT);

    cairo_rectangle (cr, x1 + _switcher_extents.left,
		     y1 + top,
		     x2 - x1 - _switcher_extents.left - _switcher_extents.right,
		     h);
#if GTK_CHECK_VERSION (3, 0, 0)
    bg.alpha = alpha;
    gdk_cairo_set_source_rgba (cr, &bg);
#else
    gdk_cairo_set_source_color_alpha (cr,
				      &style->bg[GTK_STATE_NORMAL],
				      alpha);
#endif
    cairo_fill (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    cairo_clip (cr);

    cairo_translate (cr, 1.0, 1.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.4);
    cairo_stroke (cr);

    cairo_translate (cr, -2.0, -2.0);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.1);
    cairo_stroke (cr);

    cairo_translate (cr, 1.0, 1.0);

    cairo_reset_clip (cr);

    rounded_rectangle (cr,
		       x1 + 0.5, y1 + 0.5,
		       x2 - x1 - 1.0, y2 - y1 - 1.0,
		       5.0,
		       CORNER_TOPLEFT | CORNER_TOPRIGHT | CORNER_BOTTOMLEFT |
		       CORNER_BOTTOMRIGHT);

#if GTK_CHECK_VERSION (3, 0, 0)
    fg.alpha = alpha;
    gdk_cairo_set_source_rgba (cr, &fg);
#else
    gdk_cairo_set_source_color_alpha (cr,
				      &style->fg[GTK_STATE_NORMAL],
				      alpha);
#endif

    cairo_stroke (cr);
    cairo_destroy (cr);

    copy_to_front_buffer (d);

#if GTK_CHECK_VERSION (3, 0, 0)
    pixel  = (((a * (int) (bg.blue * 65535.0)) >> 24) & 0x0000ff);
    pixel |= (((a * (int) (bg.green * 65535.0)) >> 16) & 0x00ff00);
    pixel |= (((a * (int) (bg.red * 65535.0)) >> 8) & 0xff0000);
#else
    pixel  = (((a * style->bg[GTK_STATE_NORMAL].blue ) >> 24) & 0x0000ff);
    pixel |= (((a * style->bg[GTK_STATE_NORMAL].green) >> 16) & 0x00ff00);
    pixel |= (((a * style->bg[GTK_STATE_NORMAL].red  ) >>  8) & 0xff0000);
#endif
    pixel |= (((a & 0xff00) << 16));

    decor_update_switcher_property (d);

    gdk_error_trap_push ();
    XSetWindowBackground (xdisplay, d->prop_xid, pixel);
    XClearWindow (xdisplay, d->prop_xid);

    gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif

    d->prop_xid = 0;
}

static void
draw_switcher_foreground (decor_t *d)
{
    cairo_t	    *cr;
#if GTK_CHECK_VERSION (3, 0, 0)
    GtkStyleContext *context;
    GdkRGBA	    bg, fg;
#else
    GtkStyle	    *style;
#endif
    double	    alpha = SWITCHER_ALPHA / 65535.0;

    if (!d->surface || !d->buffer_surface)
	return;

#if GTK_CHECK_VERSION (3, 0, 0)
    context = gtk_widget_get_style_context (style_window);

    gtk_style_context_save (context);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_get_background_color (context, gtk_style_context_get_state (context), &bg);
    gtk_style_context_get_color (context, gtk_style_context_get_state (context), &fg);
    gtk_style_context_restore (context);
#else
    style = gtk_widget_get_style (style_window);
#endif

    cr = cairo_create (d->buffer_surface);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    cairo_rectangle (cr, switcher_context.left_space,
		     d->height - switcher_context.bottom_space,
		     d->width - switcher_context.left_space -
		     switcher_context.right_space,
		     SWITCHER_SPACE);

#if GTK_CHECK_VERSION (3, 0, 0)
    bg.alpha = alpha;
    gdk_cairo_set_source_rgba (cr, &bg);
#else
    gdk_cairo_set_source_color_alpha (cr,
				      &style->bg[GTK_STATE_NORMAL],
				      alpha);
#endif
    cairo_fill (cr);

    if (d->layout)
    {
	int w;

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

#if GTK_CHECK_VERSION (3, 0, 0)
	fg.alpha = 1.0;
	gdk_cairo_set_source_rgba (cr, &fg);
#else
	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  1.0);
#endif

	pango_layout_get_pixel_size (d->layout, &w, NULL);

	cairo_move_to (cr, d->width / 2 - w / 2,
		       d->height - switcher_context.bottom_space +
		       SWITCHER_SPACE / 2 - text_height / 2);

	pango_cairo_show_layout (cr, d->layout);
    }

    cairo_destroy (cr);

    copy_to_front_buffer (d);
}

static void
draw_switcher_decoration (decor_t *d)
{
    if (d->prop_xid)
	draw_switcher_background (d);

    draw_switcher_foreground (d);
}

static gboolean
draw_decor_list (void *data)
{
    GSList  *list;
    decor_t *d;

    draw_idle_id = 0;

    for (list = draw_list; list; list = list->next)
    {
	d = (decor_t *) list->data;
	(*d->draw) (d);
    }

    g_slist_free (draw_list);
    draw_list = NULL;

    return FALSE;
}

static void
queue_decor_draw (decor_t *d)
{
    if (g_slist_find (draw_list, d))
	return;

    draw_list = g_slist_append (draw_list, d);

    if (!draw_idle_id)
	draw_idle_id = g_idle_add (draw_decor_list, NULL);
}

static void
update_default_decorations (GdkScreen *screen)
{
    long	    *data = NULL;
    unsigned int    n = 1, frame_type = 0, frame_state = 0, frame_actions = 0;
    Window	    xroot;
    GdkDisplay	    *gdkdisplay = gdk_display_get_default ();
    Display	    *xdisplay = gdk_x11_display_get_xdisplay (gdkdisplay);
    Atom	    bareAtom, activeAtom;
    decor_t	    d;
    gint	    nQuad;
    decor_quad_t    quads[N_QUADS_MAX];
    decor_extents_t extents = _win_extents;

    xroot = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    bareAtom   = XInternAtom (xdisplay, DECOR_BARE_ATOM_NAME, FALSE);
    activeAtom = XInternAtom (xdisplay, DECOR_ACTIVE_ATOM_NAME, FALSE);

    data = decor_alloc_property (n, WINDOW_DECORATION_TYPE_PIXMAP);

    if (no_border_shadow)
    {
	decor_layout_t layout;

	decor_get_default_layout (&shadow_context, 1, 1, &layout);

	nQuad = decor_set_lSrStSbS_window_quads (quads, &shadow_context,
						 &layout);

	decor_quads_to_property (data, n - 1, no_border_shadow->pixmap,
				 &_shadow_extents, &_shadow_extents,
				 &_shadow_extents, &_shadow_extents,
				 0, 0, quads, nQuad,
				 frame_type, frame_state, frame_actions);

	XChangeProperty (xdisplay, xroot,
			 bareAtom,
			 XA_INTEGER,
			 32, PropModeReplace, (guchar *) data,
			 PROP_HEADER_SIZE + BASE_PROP_SIZE +
			   QUAD_PROP_SIZE * N_QUADS_MAX);

	if (minimal)
	{
	    XChangeProperty (xdisplay, xroot,
			     activeAtom,
			     XA_INTEGER,
			     32, PropModeReplace, (guchar *) data,
			     PROP_HEADER_SIZE + BASE_PROP_SIZE +
			       QUAD_PROP_SIZE * N_QUADS_MAX);
	}
    }
    else
    {
	XDeleteProperty (xdisplay, xroot, bareAtom);

	if (minimal)
	    XDeleteProperty (xdisplay, xroot, activeAtom);
    }

    if (minimal)
	return;

    memset (&d, 0, sizeof (d));

    d.context = &window_context;
    d.shadow  = border_shadow;
    d.layout  = pango_layout_new (pango_context);

    decor_get_default_layout (d.context, 1, 1, &d.border_layout);

    d.width  = d.border_layout.width;
    d.height = d.border_layout.height;

    extents.top += titlebar_height;

    d.draw = theme_draw_window_decoration;

    if (decor_normal_surface)
	cairo_surface_destroy (decor_normal_surface);

    nQuad = decor_set_lSrStSbS_window_quads (quads, d.context,
					     &d.border_layout);

    decor_normal_surface = create_surface (d.width, d.height);
    if (decor_normal_surface)
    {
	d.surface = decor_normal_surface;
	d.active  = FALSE;
	d.picture = XRenderCreatePicture (xdisplay,
					  cairo_xlib_surface_get_drawable (d.surface),
					  xformat, 0, NULL);

	(*d.draw) (&d);

	XRenderFreePicture (xdisplay, d.picture);

	data = decor_alloc_property (n, WINDOW_DECORATION_TYPE_PIXMAP);
	decor_quads_to_property (data, n - 1,
				 cairo_xlib_surface_get_drawable (d.surface),
				 &extents, &extents, &extents, &extents,
				 0, 0, quads, nQuad,
				 frame_type, frame_state, frame_actions);
    }

    if (decor_active_surface)
	cairo_surface_destroy (decor_active_surface);

    decor_active_surface = create_surface (d.width, d.height);
    if (decor_active_surface)
    {
	d.surface = decor_active_surface;
	d.active  = TRUE;
	d.picture = XRenderCreatePicture (xdisplay,
					  cairo_xlib_surface_get_drawable (d.surface),
					  xformat, 0, NULL);

	(*d.draw) (&d);

	XRenderFreePicture (xdisplay, d.picture);

	decor_quads_to_property (data, n - 1,
				 cairo_xlib_surface_get_drawable(d.surface),
				 &extents, &extents, &extents, &extents,
				 0, 0, quads, nQuad,
				 frame_type, frame_state, frame_actions);

	XChangeProperty (xdisplay, xroot,
			 activeAtom,
			 XA_INTEGER,
			 32, PropModeReplace, (guchar *) data,
			 PROP_HEADER_SIZE + BASE_PROP_SIZE +
			   QUAD_PROP_SIZE * N_QUADS_MAX);
    }

    if (d.layout)
	g_object_unref (G_OBJECT (d.layout));

    if (data)
	free (data);
}

static gboolean
get_window_prop (Window xwindow,
		 Atom   atom,
		 Window *val)
{
    Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    Atom   type;
    int	   format;
    gulong nitems;
    gulong bytes_after;
    Window *w;
    int    err, result;

    *val = 0;

    gdk_error_trap_push ();

    type = None;
    result = XGetWindowProperty (dpy,
				 xwindow,
				 atom,
				 0, G_MAXLONG,
				 False, XA_WINDOW, &type, &format, &nitems,
				 &bytes_after, (void*) &w);
    err = gdk_error_trap_pop ();
    if (err != Success || result != Success)
	return FALSE;

    if (type != XA_WINDOW)
    {
	XFree (w);
	return FALSE;
    }

    *val = *w;
    XFree (w);

    return TRUE;
}

static unsigned int
get_mwm_prop (Window xwindow)
{
    Display	  *xdisplay;
    Atom	  actual;
    int		  err, result, format;
    unsigned long n, left;
    unsigned char *data;
    unsigned int  decor = MWM_DECOR_ALL;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    gdk_error_trap_push ();

    result = XGetWindowProperty (xdisplay, xwindow, mwm_hints_atom,
				 0L, 20L, FALSE, mwm_hints_atom,
				 &actual, &format, &n, &left, &data);

    err = gdk_error_trap_pop ();
    if (err != Success || result != Success)
	return decor;

    if (data)
    {
	MwmHints *mwm_hints = (MwmHints *) data;

	if (n >= PROP_MOTIF_WM_HINT_ELEMENTS)
	{
	    if (mwm_hints->flags & MWM_HINTS_DECORATIONS)
		decor = mwm_hints->decorations;
	}

	XFree (data);
    }

    return decor;
}

static void
get_event_window_position (decor_t *d,
			   gint    i,
			   gint    j,
			   gint    width,
			   gint    height,
			   gint    *x,
			   gint    *y,
			   gint    *w,
			   gint    *h)
{
    *x = pos[i][j].x + pos[i][j].xw * width;
    *y = pos[i][j].y + pos[i][j].yh * height + pos[i][j].yth *
	(titlebar_height - 17);

    if ((d->state & WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY) &&
	(j == 0 || j == 2))
    {
	*w = 0;
    }
    else
    {
	*w = pos[i][j].w + pos[i][j].ww * width;
    }

    if ((d->state & WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY) &&
	(i == 0 || i == 2))
    {
	*h = 0;
    }
    else
    {
	*h = pos[i][j].h + pos[i][j].hh * height + pos[i][j].hth *
	(titlebar_height - 17);
    }
}

static gboolean
get_button_position (decor_t *d,
		     gint    i,
		     gint    width,
		     gint    height,
		     gint    *x,
		     gint    *y,
		     gint    *w,
		     gint    *h)
{
    if (i > BUTTON_MENU)
	return FALSE;

    *x = bpos[i].x + bpos[i].xw * width;
    *y = bpos[i].y + bpos[i].yh * height + bpos[i].yth *
	(titlebar_height - 17);
    *w = bpos[i].w + bpos[i].ww * width;
    *h = bpos[i].h + bpos[i].hh * height + bpos[i].hth +
	(titlebar_height - 17);

    /* hack to position multiple buttons on the right */
    if (i != BUTTON_MENU)
	*x -= 10 + 16 * i;

    return TRUE;
}

#ifdef USE_MARCO

#define TOP_RESIZE_HEIGHT 2
#define RESIZE_EXTENDS 15

static void
meta_get_event_window_position (decor_t *d,
				gint    i,
				gint    j,
				gint	width,
				gint	height,
				gint    *x,
				gint    *y,
				gint    *w,
				gint    *h)
{
    MetaButtonLayout  button_layout;
    MetaFrameGeometry fgeom;
    MetaFrameFlags    flags;
    MetaTheme	      *theme;
    GdkRectangle      clip;

    theme = meta_theme_get_current ();

    meta_get_decoration_geometry (d, theme, &flags, &fgeom, &button_layout,
				  &clip);

    width  += fgeom.right_width + fgeom.left_width;
    height += fgeom.top_height  + fgeom.bottom_height;

    switch (i) {
    case 2: /* bottom */
	switch (j) {
	case 2: /* bottom right */
	    *x = width - fgeom.right_width - RESIZE_EXTENDS;
	    *y = height - fgeom.bottom_height - RESIZE_EXTENDS;
	    *w = fgeom.right_width + RESIZE_EXTENDS;
	    *h = fgeom.bottom_height + RESIZE_EXTENDS;
	    break;
	case 1: /* bottom */
	    *x = fgeom.left_width + RESIZE_EXTENDS;
	    *y = height - fgeom.bottom_height;
	    *w = width - fgeom.left_width - fgeom.right_width -
		 (2 * RESIZE_EXTENDS);
	    *h = fgeom.bottom_height;
	    break;
	case 0: /* bottom left */
	default:
	    *x = 0;
	    *y = height - fgeom.bottom_height - RESIZE_EXTENDS;
	    *w = fgeom.left_width + RESIZE_EXTENDS;
	    *h = fgeom.bottom_height + RESIZE_EXTENDS;
	    break;
	}
	break;
    case 1: /* middle */
	switch (j) {
	case 2: /* right */
	    *x = width - fgeom.right_width;
	    *y = fgeom.top_height + RESIZE_EXTENDS;
	    *w = fgeom.right_width;
	    *h = height - fgeom.top_height - fgeom.bottom_height -
		 (2 * RESIZE_EXTENDS);
	    break;
	case 1: /* middle */
	    *x = fgeom.left_width;
	    *y = fgeom.title_rect.y + TOP_RESIZE_HEIGHT;
	    *w = width - fgeom.left_width - fgeom.right_width;
	    *h = height - fgeom.top_titlebar_edge - fgeom.bottom_height;
	    break;
	case 0: /* left */
	default:
	    *x = 0;
	    *y = fgeom.top_height + RESIZE_EXTENDS;
	    *w = fgeom.left_width;
	    *h = height - fgeom.top_height - fgeom.bottom_height -
		 (2 * RESIZE_EXTENDS);
	    break;
	}
	break;
    case 0: /* top */
    default:
	switch (j) {
	case 2: /* top right */
	    *x = width - fgeom.right_width - RESIZE_EXTENDS;
	    *y = 0;
	    *w = fgeom.right_width + RESIZE_EXTENDS;
	    *h = fgeom.top_height + RESIZE_EXTENDS;
	    break;
	case 1: /* top */
	    *x = fgeom.left_width + RESIZE_EXTENDS;
	    *y = 0;
	    *w = width - fgeom.left_width - fgeom.right_width -
		 (2 * RESIZE_EXTENDS);
	    *h = fgeom.title_rect.y + TOP_RESIZE_HEIGHT;
	    break;
	case 0: /* top left */
	default:
	    *x = 0;
	    *y = 0;
	    *w = fgeom.left_width + RESIZE_EXTENDS;
	    *h = fgeom.top_height + RESIZE_EXTENDS;
	    break;
	}
    }

    if (!(flags & META_FRAME_ALLOWS_VERTICAL_RESIZE))
    {
	/* turn off top and bottom event windows */
	if (i == 0 || i == 2)
	    *w = *h = 0;
    }

    if (!(flags & META_FRAME_ALLOWS_HORIZONTAL_RESIZE))
    {
	/* turn off left and right event windows */
	if (j == 0 || j == 2)
	    *w = *h = 0;
    }
}

static gboolean
meta_button_present (MetaButtonLayout   *button_layout,
		     MetaButtonFunction function)
{
    int i;

    for (i = 0; i < MAX_BUTTONS_PER_CORNER; i++)
	if (button_layout->left_buttons[i] == function)
	    return TRUE;

    for (i = 0; i < MAX_BUTTONS_PER_CORNER; i++)
	if (button_layout->right_buttons[i] == function)
	    return TRUE;

    return FALSE;
}

static gboolean
meta_get_button_position (decor_t *d,
			  gint    i,
			  gint	  width,
			  gint	  height,
			  gint    *x,
			  gint    *y,
			  gint    *w,
			  gint    *h)
{
    MetaButtonLayout  button_layout;
    MetaFrameGeometry fgeom;
    MetaFrameFlags    flags;
    MetaTheme	      *theme;
    GdkRectangle      clip;
    MetaButtonSpace   *space;

    if (!d->context)
    {
	/* undecorated windows implicitly have no buttons */
	return FALSE;
    }

    theme = meta_theme_get_current ();

    meta_get_decoration_geometry (d, theme, &flags, &fgeom, &button_layout,
				  &clip);

    switch (i) {
    case BUTTON_MENU:
	if (!meta_button_present (&button_layout, META_BUTTON_FUNCTION_MENU))
	    return FALSE;

	space = &fgeom.menu_rect;
	break;
    case BUTTON_MIN:
	if (!meta_button_present (&button_layout,
				  META_BUTTON_FUNCTION_MINIMIZE))
	    return FALSE;

	space = &fgeom.min_rect;
	break;
    case BUTTON_MAX:
	if (!meta_button_present (&button_layout,
				  META_BUTTON_FUNCTION_MAXIMIZE))
	    return FALSE;

	space = &fgeom.max_rect;
	break;
    case BUTTON_CLOSE:
	if (!meta_button_present (&button_layout, META_BUTTON_FUNCTION_CLOSE))
	    return FALSE;

	space = &fgeom.close_rect;
	break;

    case BUTTON_SHADE:
	if (!meta_button_present (&button_layout, META_BUTTON_FUNCTION_SHADE))
	    return FALSE;

	space = &fgeom.shade_rect;
	break;
    case BUTTON_ABOVE:
	if (!meta_button_present (&button_layout, META_BUTTON_FUNCTION_ABOVE))
	    return FALSE;

	space = &fgeom.above_rect;
	break;
    case BUTTON_STICK:
	if (!meta_button_present (&button_layout, META_BUTTON_FUNCTION_STICK))
	    return FALSE;

	space = &fgeom.stick_rect;
	break;
    case BUTTON_UNSHADE:
	if (!meta_button_present (&button_layout, META_BUTTON_FUNCTION_UNSHADE))
	    return FALSE;

	space = &fgeom.unshade_rect;
	break;
    case BUTTON_UNABOVE:
	if (!meta_button_present (&button_layout, META_BUTTON_FUNCTION_UNABOVE))
	    return FALSE;

	space = &fgeom.unabove_rect;
	break;
    case BUTTON_UNSTICK:
	if (!meta_button_present (&button_layout, META_BUTTON_FUNCTION_UNSTICK))
	    return FALSE;

	space = &fgeom.unstick_rect;
	break;

    default:
	return FALSE;
    }

    if (!space->clickable.width && !space->clickable.height)
	return FALSE;

    *x = space->clickable.x;
    *y = space->clickable.y;
    *w = space->clickable.width;
    *h = space->clickable.height;

    return TRUE;
}

#endif

static void
update_event_windows (WnckWindow *win)
{
    Display *xdisplay;
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    gint    x0, y0, width, height, x, y, w, h;
    gint    i, j, k, l;
    gint    actions = d->actions;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    wnck_window_get_client_window_geometry (win, &x0, &y0, &width, &height);

    if (d->state & WNCK_WINDOW_STATE_SHADED)
    {
	height = 0;
	k = l = 1;
    }
    else
    {
	k = 0;
	l = 2;
    }

    gdk_error_trap_push ();

    for (i = 0; i < 3; i++)
    {
	static guint event_window_actions[3][3] = {
	    {
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_RESIZE
	    }, {
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_MOVE,
		WNCK_WINDOW_ACTION_RESIZE
	    }, {
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_RESIZE,
		WNCK_WINDOW_ACTION_RESIZE
	    }
	};

	for (j = 0; j < 3; j++)
	{
	    w = 0;
	    h = 0;

	    if (actions & event_window_actions[i][j] && i >= k && i <= l)
		(*theme_get_event_window_position) (d, i, j, width, height,
						    &x, &y, &w, &h);

	    if (w != 0 && h != 0)
	    {
		if (d->state & WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY)
		    y = 0;
		XMapWindow (xdisplay, d->event_windows[i][j]);
		XMoveResizeWindow (xdisplay, d->event_windows[i][j],
				   x, y, w, h);
	    }
	    else
	    {
		XUnmapWindow (xdisplay, d->event_windows[i][j]);
	    }
	}
    }

    /* no button event windows if width is less than minimum width */
    if (width < ICON_SPACE + d->button_width)
	actions = 0;

    for (i = 0; i < BUTTON_NUM; i++)
    {
	static guint button_actions[BUTTON_NUM] = {
	    WNCK_WINDOW_ACTION_CLOSE,
	    WNCK_WINDOW_ACTION_MAXIMIZE,
	    WNCK_WINDOW_ACTION_MINIMIZE,
	    0,
	    WNCK_WINDOW_ACTION_SHADE,
	    WNCK_WINDOW_ACTION_ABOVE,
	    WNCK_WINDOW_ACTION_STICK,
	    WNCK_WINDOW_ACTION_UNSHADE,
	    WNCK_WINDOW_ACTION_ABOVE,
	    WNCK_WINDOW_ACTION_UNSTICK
	};

	if (button_actions[i] && !(actions & button_actions[i]))
	{
	    XUnmapWindow (xdisplay, d->button_windows[i]);
	    continue;
	}

	if ((*theme_get_button_position) (d, i, width, height, &x, &y, &w, &h))
	{
	    if (d->state & WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY)
		y = 0;
	    XMapWindow (xdisplay, d->button_windows[i]);
	    XMoveResizeWindow (xdisplay, d->button_windows[i], x, y, w, h);
	}
	else
	{
	    XUnmapWindow (xdisplay, d->button_windows[i]);
	}
    }

    gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif
}

static const char *
wnck_window_get_real_name (WnckWindow *win)
{
    return wnck_window_has_name (win) ? wnck_window_get_name (win) : NULL;
}
#define wnck_window_get_name wnck_window_get_real_name

static gint
max_window_name_width (WnckWindow *win)
{
    decor_t     *d = g_object_get_data (G_OBJECT (win), "decor");
    const gchar *name;
    gint	w;

    if (!d->layout)
    {
	d->layout = pango_layout_new (pango_context);
	if (!d->layout)
	    return 0;

	pango_layout_set_wrap (d->layout, PANGO_WRAP_CHAR);
    }

    name = wnck_window_get_name (win);
    if (!name)
	return 0;

    pango_layout_set_auto_dir (d->layout, FALSE);
    pango_layout_set_width (d->layout, -1);
    pango_layout_set_text (d->layout, name, strlen (name));
    pango_layout_get_pixel_size (d->layout, &w, NULL);

    if (d->name)
	pango_layout_set_text (d->layout, d->name, strlen (d->name));

    return w + 6;
}

static void
update_window_decoration_name (WnckWindow *win)
{
    decor_t	    *d = g_object_get_data (G_OBJECT (win), "decor");
    const gchar	    *name;
    glong	    name_length;
    PangoLayoutLine *line;

    if (d->name)
    {
	g_free (d->name);
	d->name = NULL;
    }

    name = wnck_window_get_name (win);
    if (name && (name_length = strlen (name)))
    {
	gint w;

	if (theme_draw_window_decoration != draw_window_decoration)
	{
	    w = SHRT_MAX;
	}
	else
	{
	    gint width;

	    wnck_window_get_client_window_geometry (win, NULL, NULL, &width,
						    NULL);

	    w = width - ICON_SPACE - 2 - d->button_width;
	    if (w < 1)
		w = 1;
	}

	pango_layout_set_auto_dir (d->layout, FALSE);
	pango_layout_set_width (d->layout, w * PANGO_SCALE);
	pango_layout_set_text (d->layout, name, name_length);

	line = pango_layout_get_line (d->layout, 0);

	name_length = line->length;
	if (pango_layout_get_line_count (d->layout) > 1)
	{
	    if (name_length < 4)
	    {
		pango_layout_set_text (d->layout, NULL, 0);
		return;
	    }

	    d->name = g_strndup (name, name_length);
	    strcpy (d->name + name_length - 3, "...");
	}
	else
	    d->name = g_strndup (name, name_length);

	pango_layout_set_text (d->layout, d->name, name_length);
    }
}

static void
update_window_decoration_icon (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->icon)
    {
	cairo_pattern_destroy (d->icon);
	d->icon = NULL;
    }

    if (d->icon_surface)
    {
	cairo_surface_destroy (d->icon_surface);
	d->icon_surface = NULL;
    }

    if (d->icon_pixbuf)
	g_object_unref (G_OBJECT (d->icon_pixbuf));

    d->icon_pixbuf = wnck_window_get_mini_icon (win);
    if (d->icon_pixbuf)
    {
	cairo_t	*cr;

	g_object_ref (G_OBJECT (d->icon_pixbuf));

	d->icon_surface = new_surface_from_pixbuf (d->icon_pixbuf);
	if (d->icon_surface)
	{
	    cr = cairo_create (d->icon_surface);
	    d->icon = cairo_pattern_create_for_surface (cairo_get_target (cr));
	    cairo_destroy (cr);
	}
    }
}

static void
update_window_decoration_state (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    d->state = wnck_window_get_state (win);
}

static void
update_window_decoration_actions (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    d->actions = wnck_window_get_actions (win);
}

static void
calc_button_size (decor_t *d)
{
    gint button_width;

    button_width = 0;

    if (d->actions & WNCK_WINDOW_ACTION_CLOSE)
	button_width += 17;

    if (d->actions & (WNCK_WINDOW_ACTION_MAXIMIZE_HORIZONTALLY   |
		      WNCK_WINDOW_ACTION_MAXIMIZE_VERTICALLY     |
		      WNCK_WINDOW_ACTION_UNMAXIMIZE_HORIZONTALLY |
		      WNCK_WINDOW_ACTION_UNMAXIMIZE_VERTICALLY))
	button_width += 17;

    if (d->actions & (WNCK_WINDOW_ACTION_MINIMIZE |
		      WNCK_WINDOW_ACTION_MINIMIZE))
	button_width += 17;

    if (button_width)
	button_width++;

    d->button_width = button_width;
}

static gboolean
calc_decoration_size (decor_t *d,
		      gint    w,
		      gint    h,
		      gint    name_width,
		      gint    *width,
		      gint    *height)
{
    decor_layout_t layout;
    int		   top_width;

    calc_button_size (d);

    if (w < ICON_SPACE + d->button_width)
	return FALSE;

    top_width = name_width + d->button_width + ICON_SPACE;
    if (w < top_width)
	top_width = MAX (ICON_SPACE + d->button_width, w);

    decor_get_default_layout (&window_context, top_width, 1, &layout);

    if (!d->context || memcmp (&layout, &d->border_layout, sizeof (layout)))
    {
	*width  = layout.width;
	*height = layout.height;

	d->border_layout = layout;
	d->context       = &window_context;
	d->shadow        = border_shadow;

	return TRUE;
    }

    return FALSE;
}

#ifdef USE_MARCO

static void
meta_calc_button_size (decor_t *d)
{
    gint i, min_x, x, y, w, h, width;

    width = d->border_layout.top.x2 - d->border_layout.top.x1 -
	d->context->left_space - d->context->right_space;
    min_x = width;

    for (i = 0; i < 3; i++)
    {
	static guint button_actions[3] = {
	    WNCK_WINDOW_ACTION_CLOSE,
	    WNCK_WINDOW_ACTION_MAXIMIZE,
	    WNCK_WINDOW_ACTION_MINIMIZE
	};

	if (d->actions & button_actions[i])
	{
	    if (meta_get_button_position (d,
					  i,
					  width,
					  256,
					  &x, &y, &w, &h))
	    {
		if (x > width / 2 && x < min_x)
		    min_x = x;
	    }
	}
    }

    d->button_width = width - min_x + 6;
}

static gboolean
meta_calc_decoration_size (decor_t *d,
			   gint    w,
			   gint    h,
			   gint    name_width,
			   gint    *width,
			   gint    *height)
{
    decor_layout_t  layout;
    decor_context_t *context;
    decor_shadow_t  *shadow;

    if ((d->state & META_MAXIMIZED) == META_MAXIMIZED)
    {
	context = &max_window_context;
	shadow  = max_border_shadow;
    }
    else
    {
	context = &window_context;
	shadow  = border_shadow;
    }

    decor_get_best_layout (context, w, h, &layout);

    if (context != d->context ||
	memcmp (&layout, &d->border_layout, sizeof (layout)))
    {
	*width  = layout.width;
	*height = layout.height;

	d->border_layout = layout;
	d->context       = context;
	d->shadow        = shadow;

	meta_calc_button_size (d);

	return TRUE;
    }


    return FALSE;
}
#endif

static gboolean
update_window_decoration_size (WnckWindow *win)
{
    decor_t   *d = g_object_get_data (G_OBJECT (win), "decor");
    cairo_surface_t *surface = NULL, *buffer_surface = NULL;
    Picture   picture;
    gint      width, height;
    gint      w, h, name_width;
    Display   *xdisplay;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    wnck_window_get_client_window_geometry (win, NULL, NULL, &w, &h);

    name_width = max_window_name_width (win);

    if (!(*theme_calc_decoration_size) (d, w, h, name_width, &width, &height))
    {
	update_window_decoration_name (win);
	return FALSE;
    }

    surface = create_surface (width, height);
    if (!surface)
	return FALSE;

    buffer_surface = create_surface (width, height);
    if (!buffer_surface)
    {
	cairo_surface_destroy (surface);
	return FALSE;
    }

    picture = XRenderCreatePicture (xdisplay, cairo_xlib_surface_get_drawable (buffer_surface),
				    xformat, 0, NULL);

    /* wait until old surfaces are not used for sure,
       one second should be enough */
    g_timeout_add_seconds (1, destroy_surface_idled, d->surface);

    if (d->buffer_surface)
	cairo_surface_destroy (d->buffer_surface);

    if (d->picture)
	XRenderFreePicture (xdisplay, d->picture);

    if (d->cr)
	cairo_destroy (d->cr);

    d->surface        = surface;
    d->buffer_surface = buffer_surface;
    d->cr             = cairo_create (surface);

    d->picture = picture;

    d->width  = width;
    d->height = height;

    d->prop_xid = wnck_window_get_xid (win);

    update_window_decoration_name (win);

    queue_decor_draw (d);

    return TRUE;
}

static void
add_frame_window (WnckWindow *win,
		  Window     frame)
{
    Display		 *xdisplay;
    XSetWindowAttributes attr;
    gulong		 xid = wnck_window_get_xid (win);
    decor_t		 *d = g_object_get_data (G_OBJECT (win), "decor");
    gint		 i, j;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    d->active = wnck_window_is_active (win);

    attr.event_mask = ButtonPressMask | EnterWindowMask | LeaveWindowMask;
    attr.override_redirect = TRUE;

    gdk_error_trap_push ();

    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
	{
	    d->event_windows[i][j] =
		XCreateWindow (xdisplay,
			       frame,
			       0, 0, 1, 1, 0,
			       CopyFromParent, CopyFromParent, CopyFromParent,
			       CWOverrideRedirect | CWEventMask, &attr);

	    if (cursor[i][j].cursor)
	    {
#ifdef HAVE_XINPUT2
		int clientPointer;

		XIGetClientPointer (xdisplay, None, &clientPointer);
		XIDefineCursor (xdisplay, clientPointer, d->event_windows[i][j],
				cursor[i][j].cursor);
#else
		XDefineCursor (xdisplay, d->event_windows[i][j],
			       cursor[i][j].cursor);
#endif
	    }
	}
    }

    attr.event_mask |= ButtonReleaseMask;

    for (i = 0; i < BUTTON_NUM; i++)
    {
	d->button_windows[i] =
	    XCreateWindow (xdisplay,
			   frame,
			   0, 0, 1, 1, 0,
			   CopyFromParent, CopyFromParent, CopyFromParent,
			   CWOverrideRedirect | CWEventMask, &attr);

	d->button_states[i] = 0;
    }

    gdk_display_sync (gdk_display_get_default ());
    if (!gdk_error_trap_pop ())
    {
	if (get_mwm_prop (xid) & (MWM_DECOR_ALL | MWM_DECOR_TITLE))
	    d->decorated = TRUE;

	for (i = 0; i < 3; i++)
	    for (j = 0; j < 3; j++)
		g_hash_table_insert (frame_table,
				     GINT_TO_POINTER (d->event_windows[i][j]),
				     GINT_TO_POINTER (xid));

	for (i = 0; i < BUTTON_NUM; i++)
	    g_hash_table_insert (frame_table,
				 GINT_TO_POINTER (d->button_windows[i]),
				 GINT_TO_POINTER (xid));

	update_window_decoration_state (win);
	update_window_decoration_actions (win);
	update_window_decoration_icon (win);
	update_window_decoration_size (win);

	update_event_windows (win);
    }
    else
    {
	memset (d->event_windows, 0, sizeof (d->event_windows));
    }
}

static gboolean
update_switcher_window (WnckWindow *win,
			Window     selected)
{
    decor_t         *d = g_object_get_data (G_OBJECT (win), "decor");
    cairo_surface_t *surface = NULL, *buffer_surface = NULL;
    gint            height, width = 0;
    WnckWindow      *selected_win;
    Display         *xdisplay;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    wnck_window_get_client_window_geometry (win, NULL, NULL, &width, NULL);

    decor_get_default_layout (&switcher_context, width, 1, &d->border_layout);

    width  = d->border_layout.width;
    height = d->border_layout.height;

    d->decorated = FALSE;
    d->draw	 = draw_switcher_decoration;

    selected_win = wnck_window_get (selected);
    if (selected_win)
    {
	glong		name_length;
	PangoLayoutLine *line;
	const gchar	*name;

	if (d->name)
	{
	    g_free (d->name);
	    d->name = NULL;
	}

	name = wnck_window_get_name (selected_win);
	if (name && (name_length = strlen (name)))
	{
	    if (!d->layout)
	    {
		d->layout = pango_layout_new (pango_context);
		if (d->layout)
		    pango_layout_set_wrap (d->layout, PANGO_WRAP_CHAR);
	    }

	    if (d->layout)
	    {
		int tw;

		tw = width - switcher_context.left_space -
		    switcher_context.right_space - 64;
		pango_layout_set_auto_dir (d->layout, FALSE);
		pango_layout_set_width (d->layout, tw * PANGO_SCALE);
		pango_layout_set_text (d->layout, name, name_length);

		line = pango_layout_get_line (d->layout, 0);

		name_length = line->length;
		if (pango_layout_get_line_count (d->layout) > 1)
		{
		    if (name_length < 4)
		    {
			g_object_unref (G_OBJECT (d->layout));
			d->layout = NULL;
		    }
		    else
		    {
			d->name = g_strndup (name, name_length);
			strcpy (d->name + name_length - 3, "...");
		    }
		}
		else
		    d->name = g_strndup (name, name_length);

		if (d->layout)
		    pango_layout_set_text (d->layout, d->name, name_length);
	    }
	}
	else if (d->layout)
	{
	    g_object_unref (G_OBJECT (d->layout));
	    d->layout = NULL;
	}
    }

    if (selected != switcher_selected_window)
    {
	gtk_label_set_text (GTK_LABEL (switcher_label), "");
	if (selected_win && d->name)
	    gtk_label_set_text (GTK_LABEL (switcher_label), d->name);
	switcher_selected_window = selected;
    }

    surface = create_surface (width, height);
    if (!surface)
	return FALSE;

    buffer_surface = create_surface (width, height);
    if (!buffer_surface)
    {
	cairo_surface_destroy (surface);
	return FALSE;
    }

    if (d->surface)
	cairo_surface_destroy (d->surface);

    if (d->buffer_surface)
	cairo_surface_destroy (d->buffer_surface);

    if (d->cr)
	cairo_destroy (d->cr);

    if (d->picture)
	XRenderFreePicture (xdisplay, d->picture);

    d->surface        = surface;
    d->buffer_surface = buffer_surface;
    d->cr             = cairo_create (surface);

    d->picture = XRenderCreatePicture (xdisplay, cairo_xlib_surface_get_drawable (buffer_surface),
				       xformat, 0, NULL);

    d->width  = width;
    d->height = height;

    d->prop_xid = wnck_window_get_xid (win);

    queue_decor_draw (d);

    return TRUE;
}

static void
remove_frame_window (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    Display *xdisplay;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    if (d->surface)
    {
	cairo_surface_destroy (d->surface);
	d->surface = NULL;
    }

    if (d->buffer_surface)
    {
	cairo_surface_destroy (d->buffer_surface);
	d->buffer_surface = NULL;
    }

    if (d->cr)
    {
	cairo_destroy (d->cr);
	d->cr = NULL;
    }

    if (d->picture)
    {
	XRenderFreePicture (xdisplay, d->picture);
	d->picture = 0;
    }

    if (d->name)
    {
	g_free (d->name);
	d->name = NULL;
    }

    if (d->layout)
    {
	g_object_unref (G_OBJECT (d->layout));
	d->layout = NULL;
    }

    if (d->icon)
    {
	cairo_pattern_destroy (d->icon);
	d->icon = NULL;
    }

    if (d->icon_surface)
    {
	cairo_surface_destroy (d->icon_surface);
	d->icon_surface = NULL;
    }

    if (d->icon_pixbuf)
    {
	g_object_unref (G_OBJECT (d->icon_pixbuf));
	d->icon_pixbuf = NULL;
    }

    if (d->force_quit_dialog)
    {
	GtkWidget *dialog = d->force_quit_dialog;

	d->force_quit_dialog = NULL;
	gtk_widget_destroy (dialog);
    }

    d->width  = 0;
    d->height = 0;

    d->decorated = FALSE;

    d->state   = 0;
    d->actions = 0;

    d->context = NULL;
    d->shadow  = NULL;

    draw_list = g_slist_remove (draw_list, d);
}

static void
window_name_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	if (!update_window_decoration_size (win))
	    queue_decor_draw (d);
    }
}

static void
window_geometry_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	int width, height;

	wnck_window_get_client_window_geometry (win, NULL, NULL, &width,
						&height);

	if (width != d->client_width || height != d->client_height)
	{
	    d->client_width  = width;
	    d->client_height = height;

	    update_window_decoration_size (win);
	    update_event_windows (win);
	}
    }
}

static void
window_icon_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	update_window_decoration_icon (win);
	queue_decor_draw (d);
    }
}

static void
window_state_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	update_window_decoration_state (win);
	if (!update_window_decoration_size (win))
	    queue_decor_draw (d);

	update_event_windows (win);
    }
}

static void
window_actions_changed (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	update_window_decoration_actions (win);
	if (!update_window_decoration_size (win))
	    queue_decor_draw (d);

	update_event_windows (win);
    }
}

static void
connect_window (WnckWindow *win)
{
    g_signal_connect_object (win, "name_changed",
			     G_CALLBACK (window_name_changed),
			     0, 0);
    g_signal_connect_object (win, "geometry_changed",
			     G_CALLBACK (window_geometry_changed),
			     0, 0);
    g_signal_connect_object (win, "icon_changed",
			     G_CALLBACK (window_icon_changed),
			     0, 0);
    g_signal_connect_object (win, "state_changed",
			     G_CALLBACK (window_state_changed),
			     0, 0);
    g_signal_connect_object (win, "actions_changed",
			     G_CALLBACK (window_actions_changed),
			     0, 0);
}

static void
active_window_changed (WnckScreen *screen)
{
    WnckWindow *win;
    decor_t    *d;

    win = wnck_screen_get_previously_active_window (screen);
    if (win)
    {
	d = g_object_get_data (G_OBJECT (win), "decor");
	if (d && d->surface)
	{
	    d->active = wnck_window_is_active (win);
	    queue_decor_draw (d);
	}
    }

    win = wnck_screen_get_active_window (screen);
    if (win)
    {
	d = g_object_get_data (G_OBJECT (win), "decor");
	if (d && d->surface)
	{
	    d->active = wnck_window_is_active (win);
	    queue_decor_draw (d);
	}
    }
}

static void
window_opened (WnckScreen *screen,
	       WnckWindow *win)
{
    decor_t *d;
    Window  window;
    gulong  xid;

    d = calloc (1, sizeof (decor_t));
    if (!d)
	return;

    wnck_window_get_client_window_geometry (win, NULL, NULL,
					    &d->client_width,
					    &d->client_height);

    d->draw = theme_draw_window_decoration;

    g_object_set_data (G_OBJECT (win), "decor", d);

    connect_window (win);

    xid = wnck_window_get_xid (win);

    if (get_window_prop (xid, select_window_atom, &window))
    {
	d->prop_xid = wnck_window_get_xid (win);
	update_switcher_window (win, window);
    }
    else if (get_window_prop (xid, frame_window_atom, &window))
    {
	add_frame_window (win, window);
    }
}

static void
window_closed (WnckScreen *screen,
	       WnckWindow *win)
{
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    remove_frame_window (win);

    g_object_set_data (G_OBJECT (win), "decor", NULL);

    gdk_error_trap_push ();
    XDeleteProperty (xdisplay, wnck_window_get_xid (win), win_decor_atom);
    gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif

    g_free (d);
}

static void
connect_screen (WnckScreen *screen)
{
    GList *windows;

    g_signal_connect_object (G_OBJECT (screen), "active_window_changed",
			     G_CALLBACK (active_window_changed),
			     0, 0);
    g_signal_connect_object (G_OBJECT (screen), "window_opened",
			     G_CALLBACK (window_opened),
			     0, 0);
    g_signal_connect_object (G_OBJECT (screen), "window_closed",
			     G_CALLBACK (window_closed),
			     0, 0);

    windows = wnck_screen_get_windows (screen);
    while (windows != NULL)
    {
	window_opened (screen, windows->data);
	windows = windows->next;
    }
}

static void
move_resize_window (WnckWindow *win,
		    int	       direction,
		    XEvent     *xevent)
{
    Display    *xdisplay;
    GdkDisplay *gdkdisplay;
    GdkScreen  *screen;
    Window     xroot;
    XEvent     ev;

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = GDK_DISPLAY_XDISPLAY (gdkdisplay);
    screen     = gdk_display_get_default_screen (gdkdisplay);
    xroot      = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    if (action_menu_mapped)
    {
	gtk_widget_destroy (action_menu);
	return;
    }

    ev.xclient.type    = ClientMessage;
    ev.xclient.display = xdisplay;

    ev.xclient.serial	  = 0;
    ev.xclient.send_event = TRUE;

    ev.xclient.window	    = wnck_window_get_xid (win);
    ev.xclient.message_type = wm_move_resize_atom;
    ev.xclient.format	    = 32;

    ev.xclient.data.l[0] = xevent->xbutton.x_root;
    ev.xclient.data.l[1] = xevent->xbutton.y_root;
    ev.xclient.data.l[2] = direction;
    ev.xclient.data.l[3] = xevent->xbutton.button;
    ev.xclient.data.l[4] = 1;

    XUngrabPointer (xdisplay, xevent->xbutton.time);
    XUngrabKeyboard (xdisplay, xevent->xbutton.time);

    XSendEvent (xdisplay, xroot, FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&ev);

    XSync (xdisplay, FALSE);
}

static void
restack_window (WnckWindow *win,
		int	   stack_mode)
{
    Display    *xdisplay;
    GdkDisplay *gdkdisplay;
    GdkScreen  *screen;
    Window     xroot;
    XEvent     ev;

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = GDK_DISPLAY_XDISPLAY (gdkdisplay);
    screen     = gdk_display_get_default_screen (gdkdisplay);
    xroot      = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    if (action_menu_mapped)
    {
	gtk_widget_destroy (action_menu);
	return;
    }

    ev.xclient.type    = ClientMessage;
    ev.xclient.display = xdisplay;

    ev.xclient.serial	  = 0;
    ev.xclient.send_event = TRUE;

    ev.xclient.window	    = wnck_window_get_xid (win);
    ev.xclient.message_type = restack_window_atom;
    ev.xclient.format	    = 32;

    ev.xclient.data.l[0] = 2;
    ev.xclient.data.l[1] = None;
    ev.xclient.data.l[2] = stack_mode;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = 0;

    XSendEvent (xdisplay, xroot, FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&ev);

    XSync (xdisplay, FALSE);
}

/* stolen from gtktooltip.c */

#define TOOLTIP_DEFAULT_DELAY 400 /* Default delay in ms */

static void
show_tooltip (const char *text)
{
#if GTK_CHECK_VERSION (3, 0, 0)
    GdkDeviceManager *device_manager;
    GdkDevice	     *pointer;
#endif
    gint	     x, y, w, h;
    GdkScreen	     *screen;
    gint	     monitor_num;
    GdkRectangle     monitor;

    screen = gdk_screen_get_default ();

    gtk_label_set_markup (GTK_LABEL (tip_label), text);
    gtk_widget_queue_resize (tip_window);

    gtk_window_get_size (GTK_WINDOW (tip_window), &w, &h);

#if GTK_CHECK_VERSION (3, 0, 0)
    device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
    pointer = gdk_device_manager_get_client_pointer (device_manager);

    gdk_device_get_position (pointer, &screen, &x, &y);
#else
    gdk_display_get_pointer (gdk_display_get_default (), &screen, &x, &y, NULL);
#endif

    x -= (w / 2 + 4);

    monitor_num = gdk_screen_get_monitor_at_point (screen, x, y);
    gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

    if ((x + w) > monitor.x + monitor.width)
	x -= (x + w) - (monitor.x + monitor.width);
    else if (x < monitor.x)
	x = monitor.x;

    if ((y + h + 16) > monitor.y + monitor.height)
	y = y - h - 16;
    else
	y = y + 16;

    gtk_window_move (GTK_WINDOW (tip_window), x, y);
    gtk_widget_show (tip_window);
}

static void
hide_tooltip (void)
{
    gtk_widget_hide (tip_window);

    if (tooltip_timer_tag != 0)
    {
	g_source_remove (tooltip_timer_tag);
	tooltip_timer_tag = 0;
    }
}

static gint
tooltip_timeout (gpointer data)
{
    tooltip_timer_tag = 0;

    show_tooltip ((const char *) data);

    return FALSE;
}

static void
tooltip_start_delay (const char *text)
{
    if (tooltip_timer_tag != 0)
	return;

    tooltip_timer_tag = g_timeout_add (TOOLTIP_DEFAULT_DELAY,
				       tooltip_timeout,
				       (gpointer) text);
}

static gboolean
tooltip_paint_window (GtkWidget *tooltip,
		      cairo_t   *cr)
{
    GtkRequisition req;

    if (!tooltip)
	return FALSE;

#if GTK_CHECK_VERSION (3, 0, 0)
    req.width = gtk_widget_get_allocated_width (tooltip);
    req.height = gtk_widget_get_allocated_height (tooltip);

    gtk_render_background (gtk_widget_get_style_context (tooltip),
			   cr, 0, 0, req.width, req.height);
    gtk_render_frame (gtk_widget_get_style_context (tooltip),
		      cr, 0, 0, req.width, req.height);
#else
    gtk_widget_size_request (tooltip, &req);
    gtk_paint_flat_box (gtk_widget_get_style (tooltip),
			gtk_widget_get_window (tooltip),
			GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			NULL, GTK_WIDGET (tooltip), "tooltip",
			0, 0, req.width, req.height);
#endif

    return FALSE;
}

static gboolean
create_tooltip_window (void)
{
    GdkScreen *screen;
#if GTK_CHECK_VERSION (3, 0, 0)
    GdkVisual *visual;
#else
    GdkColormap *colormap;
#endif

    screen = gdk_screen_get_default ();

    tip_window = gtk_window_new (GTK_WINDOW_POPUP);

    gtk_widget_set_app_paintable (tip_window, TRUE);
    gtk_window_set_resizable (GTK_WINDOW (tip_window), FALSE);
    gtk_widget_set_redraw_on_allocate (tip_window, TRUE);
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_style_context_add_class (gtk_widget_get_style_context (tip_window),
				 GTK_STYLE_CLASS_TOOLTIP);
#else
    gtk_widget_set_name (tip_window, "gtk-tooltips");
#endif
    gtk_container_set_border_width (GTK_CONTAINER (tip_window), 4);
    gtk_window_set_type_hint (GTK_WINDOW (tip_window),
			      GDK_WINDOW_TYPE_HINT_TOOLTIP);

    gtk_window_set_screen (GTK_WINDOW (tip_window), screen);

#if GTK_CHECK_VERSION (3, 0, 0)
    visual = gdk_screen_get_rgba_visual (screen);
    if (visual != NULL)
	gtk_widget_set_visual (tip_window, visual);

    g_signal_connect (G_OBJECT (tip_window),
		      "draw",
		      G_CALLBACK (tooltip_paint_window),
		      NULL);
#else
    colormap = gdk_screen_get_rgba_colormap (screen);
    if (colormap != NULL)
	gtk_widget_set_colormap (tip_window, colormap);

    g_signal_connect_swapped (G_OBJECT (tip_window),
			      "expose_event",
			      G_CALLBACK (tooltip_paint_window),
			      NULL);
#endif

    tip_label = gtk_label_new (NULL);
    gtk_label_set_line_wrap (GTK_LABEL (tip_label), TRUE);
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_widget_set_halign (tip_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (tip_label, GTK_ALIGN_CENTER);
#else
    gtk_misc_set_alignment (GTK_MISC (tip_label), 0.5, 0.5);
#endif
    gtk_widget_show (tip_label);

    gtk_container_add (GTK_CONTAINER (tip_window), tip_label);

#if !GTK_CHECK_VERSION (3, 0, 0)
    gtk_widget_ensure_style (tip_window);
#endif

    return TRUE;
}

static void
handle_tooltip_event (WnckWindow *win,
		      XEvent     *xevent,
		      guint	 state,
		      const char *tip_text)
{
    switch (xevent->type) {
    case ButtonPress:
	hide_tooltip ();
	break;
    case ButtonRelease:
	break;
    case EnterNotify:
	if (!(state & PRESSED_EVENT_WINDOW))
	{
	    if (wnck_window_is_active (win))
		tooltip_start_delay (tip_text);
	}
	break;
    case LeaveNotify:
	hide_tooltip ();
	break;
    }
}

static void
common_button_event (WnckWindow *win,
		     XEvent     *xevent,
		     int	button,
		     int	max,
		     char	*tooltip)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[button];

    handle_tooltip_event (win, xevent, state, tooltip);

    switch (xevent->type) {
    case ButtonPress:
	if (xevent->xbutton.button <= max)
	    d->button_states[button] |= PRESSED_EVENT_WINDOW;
	break;
    case ButtonRelease:
	if (xevent->xbutton.button <= max)
	    d->button_states[button] &= ~PRESSED_EVENT_WINDOW;
	break;
    case EnterNotify:
	d->button_states[button] |= IN_EVENT_WINDOW;
	break;
    case LeaveNotify:
	d->button_states[button] &= ~IN_EVENT_WINDOW;
	break;
    }

    if (state != d->button_states[button])
	queue_decor_draw (d);
}

#define BUTTON_EVENT_ACTION_STATE (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW)

static void
close_button_event (WnckWindow *win,
		    XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[BUTTON_CLOSE];

    common_button_event (win, xevent, BUTTON_CLOSE, 1, _("Close Window"));

    switch (xevent->type) {
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (state == BUTTON_EVENT_ACTION_STATE)
		wnck_window_close (win, xevent->xbutton.time);
	}
	break;
    }
}

static void
max_button_event (WnckWindow *win,
		  XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[BUTTON_MAX];

    if (wnck_window_is_maximized (win))
	common_button_event (win, xevent, BUTTON_MAX,
			     3, _("Unmaximize Window"));
    else
	common_button_event (win, xevent, BUTTON_MAX,
			     3, _("Maximize Window"));

    switch (xevent->type) {
    case ButtonRelease:
	if (xevent->xbutton.button <= 3)
	{
	    if (state == BUTTON_EVENT_ACTION_STATE)
	    {
		if (xevent->xbutton.button == 2)
		{
		    if (wnck_window_is_maximized_vertically (win))
			wnck_window_unmaximize_vertically (win);
		    else
			wnck_window_maximize_vertically (win);
		}
		else if (xevent->xbutton.button == 3)
		{
		    if (wnck_window_is_maximized_horizontally (win))
			wnck_window_unmaximize_horizontally (win);
		    else
			wnck_window_maximize_horizontally (win);
		}
		else
		{
		    if (wnck_window_is_maximized (win))
			wnck_window_unmaximize (win);
		    else
			wnck_window_maximize (win);
		}
	    }
	}
	break;
    }
}

static void
min_button_event (WnckWindow *win,
		  XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[BUTTON_MIN];

    common_button_event (win, xevent, BUTTON_MIN, 1, _("Minimize Window"));

    switch (xevent->type) {
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (state == BUTTON_EVENT_ACTION_STATE)
		wnck_window_minimize (win);
	}
	break;
    }
}

static void
action_menu_unmap (GObject *object)
{
    action_menu_mapped = FALSE;
}

static void
action_menu_destroyed (GObject *object)
{
    g_signal_handlers_disconnect_by_func (action_menu, action_menu_destroyed, NULL);
    g_signal_handlers_disconnect_by_func (action_menu, action_menu_unmap, NULL);
    g_object_unref (action_menu);
    action_menu = NULL;
    action_menu_mapped = FALSE;
}

static void
position_action_menu (GtkMenu  *menu,
		      gint     *x,
		      gint     *y,
		      gboolean *push_in,
		      gpointer user_data)
{
    WnckWindow *win = (WnckWindow *) user_data;
    decor_t    *d = g_object_get_data (G_OBJECT (win), "decor");
    gint       bx, by, width, height;

    wnck_window_get_client_window_geometry (win, x, y, &width, &height);

    if ((*theme_get_button_position) (d, BUTTON_MENU, width, height,
				      &bx, &by, &width, &height))
	*x = *x - _win_extents.left + bx;

    if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
    {
	GtkRequisition req;

#if GTK_CHECK_VERSION (3, 0, 0)
	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &req, NULL);
#else
	gtk_widget_size_request (GTK_WIDGET (menu), &req);
#endif
	*x = MAX (0, *x - req.width + width);
    }

    *push_in = TRUE;
}

static void
action_menu_map (WnckWindow *win,
		 long	     button,
		 Time	     time)
{
    GdkDisplay *gdkdisplay;
    GdkScreen  *screen;
#if GTK_CHECK_VERSION (3, 0, 0)
    Display    *display;
#endif

    gdkdisplay = gdk_display_get_default ();
    screen     = gdk_display_get_default_screen (gdkdisplay);
#if GTK_CHECK_VERSION (3, 0, 0)
    display    = gdk_x11_display_get_xdisplay (gdkdisplay);
#endif

    if (action_menu)
    {
	if (action_menu_mapped)
	{
	    gtk_widget_destroy (action_menu);
	    return;
	}
	else
	    gtk_widget_destroy (action_menu);
    }

    switch (wnck_window_get_window_type (win)) {
    case WNCK_WINDOW_DESKTOP:
    case WNCK_WINDOW_DOCK:
	/* don't allow window action */
	return;
    case WNCK_WINDOW_NORMAL:
    case WNCK_WINDOW_DIALOG:
    case WNCK_WINDOW_TOOLBAR:
    case WNCK_WINDOW_MENU:
    case WNCK_WINDOW_UTILITY:
    case WNCK_WINDOW_SPLASHSCREEN:
	/* allow window action menu */
	break;
    }

    action_menu = wnck_action_menu_new (win);
    g_object_ref_sink (action_menu);

    gtk_menu_set_screen (GTK_MENU (action_menu), screen);

    g_signal_connect (G_OBJECT (action_menu), "destroy",
		      G_CALLBACK (action_menu_destroyed), NULL);
    g_signal_connect (G_OBJECT (action_menu), "unmap",
		      G_CALLBACK (action_menu_unmap), NULL);

    gtk_widget_show (action_menu);

#if GTK_CHECK_VERSION (3, 0, 0)
    XUngrabPointer (display, time);
    XUngrabKeyboard (display, time);
#endif

    if (!button || button == 1)
    {
	gtk_menu_popup (GTK_MENU (action_menu),
			NULL, NULL,
			position_action_menu, (gpointer) win,
			button,
			time);
    }
    else
    {
	gtk_menu_popup (GTK_MENU (action_menu),
			NULL, NULL,
			NULL, NULL,
			button,
			time);
    }

    action_menu_mapped = TRUE;
}

static void
menu_button_event (WnckWindow *win,
		   XEvent     *xevent)
{
    common_button_event (win, xevent, BUTTON_MENU, 1, _("Window Menu"));

    switch (xevent->type) {
    case ButtonPress:
	if (xevent->xbutton.button == 1)
	    action_menu_map (win,
			     xevent->xbutton.button,
			     xevent->xbutton.time);
	break;
    }
}

static void
shade_button_event (WnckWindow *win,
		    XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[BUTTON_SHADE];

    common_button_event (win, xevent, BUTTON_SHADE, 1, _("Shade"));

    switch (xevent->type) {
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (state == BUTTON_EVENT_ACTION_STATE)
		wnck_window_shade (win);
	}
	break;
    }
}

static void
above_button_event (WnckWindow *win,
		    XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[BUTTON_ABOVE];

    common_button_event (win, xevent, BUTTON_ABOVE, 1, _("Make Above"));

    switch (xevent->type) {
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (state == BUTTON_EVENT_ACTION_STATE)
		wnck_window_make_above (win);
	}
	break;
    }
}

static void
stick_button_event (WnckWindow *win,
		    XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[BUTTON_STICK];

    common_button_event (win, xevent, BUTTON_STICK, 1, _("Stick"));

    switch (xevent->type) {
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (state == BUTTON_EVENT_ACTION_STATE)
		wnck_window_stick (win);
	}
	break;
    }
}

static void
unshade_button_event (WnckWindow *win,
		      XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[BUTTON_UNSHADE];

    common_button_event (win, xevent, BUTTON_UNSHADE, 1, _("Unshade"));

    switch (xevent->type) {
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (state == BUTTON_EVENT_ACTION_STATE)
		wnck_window_unshade (win);
	}
	break;
    }
}

static void
unabove_button_event (WnckWindow *win,
		      XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[BUTTON_UNABOVE];

    common_button_event (win, xevent, BUTTON_UNABOVE, 1, _("Unmake Above"));

    switch (xevent->type) {
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (state == BUTTON_EVENT_ACTION_STATE)
		wnck_window_unmake_above (win);
	}
	break;
    }
}

static void
unstick_button_event (WnckWindow *win,
		      XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[BUTTON_UNSTICK];

    common_button_event (win, xevent, BUTTON_UNSTICK, 1, _("Unstick"));

    switch (xevent->type) {
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (state == BUTTON_EVENT_ACTION_STATE)
		wnck_window_unstick (win);
	}
	break;
    }
}

static void
handle_title_button_event (WnckWindow   *win,
			   int          action,
			   XButtonEvent *event)
{
    switch (action) {
    case CLICK_ACTION_SHADE:
	if (wnck_window_is_shaded (win))
	    wnck_window_unshade (win);
	else
	    wnck_window_shade (win);
	break;
    case CLICK_ACTION_MAXIMIZE:
	if (wnck_window_is_maximized (win))
	    wnck_window_unmaximize (win);
	else
	    wnck_window_maximize (win);
	break;
    case CLICK_ACTION_MAXIMIZE_HORZ:
	if (wnck_window_is_maximized_horizontally (win))
	    wnck_window_unmaximize_horizontally (win);
	else
	    wnck_window_maximize_horizontally (win);
	break;
    case CLICK_ACTION_MAXIMIZE_VERT:
	if (wnck_window_is_maximized_vertically (win))
	    wnck_window_unmaximize_vertically (win);
	else
	    wnck_window_maximize_vertically (win);
	break;
    case CLICK_ACTION_MINIMIZE:
	if (!wnck_window_is_minimized (win))
	    wnck_window_minimize (win);
	break;
    case CLICK_ACTION_RAISE:
	restack_window (win, Above);
	break;
    case CLICK_ACTION_LOWER:
	restack_window (win, Below);
	break;
    case CLICK_ACTION_MENU:
	action_menu_map (win, event->button, event->time);
	break;
    }
}

static void
handle_mouse_wheel_title_event (WnckWindow   *win,
				unsigned int button)
{
    switch (wheel_action) {
    case WHEEL_ACTION_SHADE:
	if (button == 4)
	{
	    if (!wnck_window_is_shaded (win))
		wnck_window_shade (win);
	}
	else if (button == 5)
	{
	    if (wnck_window_is_shaded (win))
		wnck_window_unshade (win);
	}
	break;
    default:
	break;
    }
}

static double
square (double x)
{
    return x * x;
}

static double
dist (double x1, double y1,
      double x2, double y2)
{
    return sqrt (square (x1 - x2) + square (y1 - y2));
}

static void
title_event (WnckWindow *win,
	     XEvent     *xevent)
{
    static int	  last_button_num = 0;
    static Window last_button_xwindow = None;
    static Time	  last_button_time = 0;
    static int	  last_button_x = 0;
    static int	  last_button_y = 0;

    if (xevent->type != ButtonPress)
	return;

    if (xevent->xbutton.button == 1)
    {
	if (xevent->xbutton.button == last_button_num			   &&
	    xevent->xbutton.window == last_button_xwindow		   &&
	    xevent->xbutton.time < last_button_time + double_click_timeout &&
	    dist (xevent->xbutton.x, xevent->xbutton.y,
		  last_button_x, last_button_y) < DOUBLE_CLICK_DISTANCE)
	{
	    handle_title_button_event (win, double_click_action,
				       &xevent->xbutton);

	    last_button_num	= 0;
	    last_button_xwindow = None;
	    last_button_time	= 0;
	    last_button_x	= 0;
	    last_button_y	= 0;
	}
	else
	{
	    last_button_num	= xevent->xbutton.button;
	    last_button_xwindow = xevent->xbutton.window;
	    last_button_time	= xevent->xbutton.time;
	    last_button_x	= xevent->xbutton.x;
	    last_button_y	= xevent->xbutton.y;

	    restack_window (win, Above);

	    move_resize_window (win, WM_MOVERESIZE_MOVE, xevent);
	}
    }
    else if (xevent->xbutton.button == 2)
    {
	handle_title_button_event (win, middle_click_action,
				   &xevent->xbutton);
    }
    else if (xevent->xbutton.button == 3)
    {
	handle_title_button_event (win, right_click_action,
				   &xevent->xbutton);
    }
    else if (xevent->xbutton.button == 4 ||
	     xevent->xbutton.button == 5)
    {
	handle_mouse_wheel_title_event (win, xevent->xbutton.button);
    }
}

static void
frame_common_event (WnckWindow *win,
		    int        direction,
		    XEvent     *xevent)
{
    if (xevent->type != ButtonPress)
	return;

    switch (xevent->xbutton.button) {
    case 1:
	move_resize_window (win, direction, xevent);
	restack_window (win, Above);
	break;
    case 2:
	handle_title_button_event (win, middle_click_action,
				   &xevent->xbutton);
	break;
    case 3:
	handle_title_button_event (win, right_click_action,
				   &xevent->xbutton);
	break;
    }
}

static void
top_left_event (WnckWindow *win,
		XEvent     *xevent)
{
    frame_common_event (win, WM_MOVERESIZE_SIZE_TOPLEFT, xevent);
}

static void
top_event (WnckWindow *win,
	   XEvent     *xevent)
{
    frame_common_event (win, WM_MOVERESIZE_SIZE_TOP, xevent);
}

static void
top_right_event (WnckWindow *win,
		 XEvent     *xevent)
{
    frame_common_event (win, WM_MOVERESIZE_SIZE_TOPRIGHT, xevent);
}

static void
left_event (WnckWindow *win,
	    XEvent     *xevent)
{
    frame_common_event (win, WM_MOVERESIZE_SIZE_LEFT, xevent);
}

static void
right_event (WnckWindow *win,
	     XEvent     *xevent)
{
    frame_common_event (win, WM_MOVERESIZE_SIZE_RIGHT, xevent);
}

static void
bottom_left_event (WnckWindow *win,
		   XEvent     *xevent)
{
    frame_common_event (win, WM_MOVERESIZE_SIZE_BOTTOMLEFT, xevent);
}

static void
bottom_event (WnckWindow *win,
	      XEvent     *xevent)
{
    frame_common_event (win, WM_MOVERESIZE_SIZE_BOTTOM, xevent);
}

static void
bottom_right_event (WnckWindow *win,
		    XEvent     *xevent)
{
    frame_common_event (win, WM_MOVERESIZE_SIZE_BOTTOMRIGHT, xevent);
}

static void
force_quit_dialog_realize (GtkWidget *dialog,
			   void      *data)
{
    WnckWindow *win = data;

    gdk_error_trap_push ();
    XSetTransientForHint (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
			  GDK_WINDOW_XID (gtk_widget_get_window (dialog)),
			  wnck_window_get_xid (win));
    gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif
}

static char *
get_client_machine (Window xwindow)
{
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    Atom    atom, type;
    gulong  nitems, bytes_after;
    guchar  *str = NULL;
    int     format, result;
    char    *retval;

    atom = XInternAtom (xdisplay, "WM_CLIENT_MACHINE", FALSE);

    gdk_error_trap_push ();

    result = XGetWindowProperty (xdisplay,
				 xwindow, atom,
				 0, G_MAXLONG,
				 FALSE, XA_STRING, &type, &format, &nitems,
				 &bytes_after, &str);

#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif

    if (result != Success)
	return NULL;

    if (type != XA_STRING)
    {
	XFree (str);
	return NULL;
    }

    retval = g_strdup ((gchar *) str);

    XFree (str);

    return retval;
}

static void
kill_window (WnckWindow *win)
{
    GdkDisplay      *gdk_display = gdk_display_get_default ();
    Display         *xdisplay    = GDK_DISPLAY_XDISPLAY (gdk_display);
    WnckApplication *app;

    app = wnck_window_get_application (win);
    if (app)
    {
	gchar buf[257], *client_machine;
	int   pid;

	pid = wnck_application_get_pid (app);
	client_machine = get_client_machine (wnck_application_get_xid (app));

	if (client_machine && pid > 0)
	{
	    if (gethostname (buf, sizeof (buf) - 1) == 0)
	    {
		if (strcmp (buf, client_machine) == 0)
		    kill (pid, 9);
	    }
	}

	if (client_machine)
	    g_free (client_machine);
    }

    gdk_error_trap_push ();
    XKillClient (xdisplay, wnck_window_get_xid (win));
    gdk_display_sync (gdk_display);
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_error_trap_pop_ignored ();
#else
    gdk_error_trap_pop ();
#endif
}

static void
force_quit_dialog_response (GtkWidget *dialog,
			    gint      response,
			    void      *data)
{
    WnckWindow *win = data;
    decor_t    *d = g_object_get_data (G_OBJECT (win), "decor");

    if (response == GTK_RESPONSE_ACCEPT)
	kill_window (win);

    if (d->force_quit_dialog)
    {
	d->force_quit_dialog = NULL;
	gtk_widget_destroy (dialog);
    }
}

static void
show_force_quit_dialog (WnckWindow *win,
			Time        timestamp)
{
    decor_t     *d = g_object_get_data (G_OBJECT (win), "decor");
    GtkWidget   *dialog;
    gchar       *str, *tmp;
    const gchar *message;

    if (d->force_quit_dialog)
	return;

    tmp = g_markup_escape_text (wnck_window_get_name (win), -1);
    str = g_strdup_printf (_("The window \"%s\" is not responding."), tmp);

    g_free (tmp);

    message = _("Forcing this application to quit will cause you to lose any unsaved changes.");
    dialog = gtk_message_dialog_new_with_markup (NULL, 0,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_NONE,
						 "<b>%s</b>\n\n%s",
						 str,
						 message);
    g_free (str);

    gtk_window_set_icon_name (GTK_WINDOW (dialog), "force-quit");

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			    _("_Cancel"),
			    GTK_RESPONSE_REJECT,
			    _("_Force Quit"),
			    GTK_RESPONSE_ACCEPT,
			    NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

    g_signal_connect (G_OBJECT (dialog), "realize",
		      G_CALLBACK (force_quit_dialog_realize),
		      win);

    g_signal_connect (G_OBJECT (dialog), "response",
		      G_CALLBACK (force_quit_dialog_response),
		      win);

    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

    gtk_widget_realize (dialog);

    gdk_x11_window_set_user_time (gtk_widget_get_window (dialog), timestamp);

    gtk_widget_show (dialog);

    d->force_quit_dialog = dialog;
}

static void
hide_force_quit_dialog (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->force_quit_dialog)
    {
	gtk_widget_destroy (d->force_quit_dialog);
	d->force_quit_dialog = NULL;
    }
}

static GdkFilterReturn
event_filter_func (GdkXEvent *gdkxevent,
		   GdkEvent  *event,
		   gpointer  data)
{
    Display    *xdisplay;
    GdkDisplay *gdkdisplay;
    XEvent     *xevent = gdkxevent;
    gulong     xid = 0;

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = GDK_DISPLAY_XDISPLAY (gdkdisplay);

    switch (xevent->type) {
    case ButtonPress:
    case ButtonRelease:
	xid = (gulong)
	    g_hash_table_lookup (frame_table,
				 GINT_TO_POINTER (xevent->xbutton.window));
	break;
    case EnterNotify:
    case LeaveNotify:
	xid = (gulong)
	    g_hash_table_lookup (frame_table,
				 GINT_TO_POINTER (xevent->xcrossing.window));
	break;
    case MotionNotify:
	xid = (gulong)
	    g_hash_table_lookup (frame_table,
				 GINT_TO_POINTER (xevent->xmotion.window));
	break;
    case PropertyNotify:
	if (xevent->xproperty.atom == frame_window_atom)
	{
	    WnckWindow *win;

	    xid = xevent->xproperty.window;

	    win = wnck_window_get (xid);
	    if (win)
	    {
		Window frame, window;

		if (!get_window_prop (xid, select_window_atom, &window))
		{
		    if (get_window_prop (xid, frame_window_atom, &frame))
			add_frame_window (win, frame);
		    else
			remove_frame_window (win);
		}
	    }
	}
	else if (xevent->xproperty.atom == mwm_hints_atom)
	{
	    WnckWindow *win;

	    xid = xevent->xproperty.window;

	    win = wnck_window_get (xid);
	    if (win)
	    {
		decor_t  *d = g_object_get_data (G_OBJECT (win), "decor");
		gboolean decorated = FALSE;

		if (get_mwm_prop (xid) & (MWM_DECOR_ALL | MWM_DECOR_TITLE))
		    decorated = TRUE;

		if (decorated != d->decorated)
		{
		    d->decorated = decorated;
		    if (decorated)
		    {
			d->context = NULL;
			d->width = d->height = 0;

			update_window_decoration_size (win);
			update_event_windows (win);
		    }
		    else
		    {
			gdk_error_trap_push ();
			XDeleteProperty (xdisplay, xid, win_decor_atom);
			gdk_display_sync (gdk_display_get_default ());
#if GTK_CHECK_VERSION (3, 0, 0)
			gdk_error_trap_pop_ignored ();
#else
			gdk_error_trap_pop ();
#endif
		    }
		}
	    }
	}
	else if (xevent->xproperty.atom == select_window_atom)
	{
	    WnckWindow *win;

	    xid = xevent->xproperty.window;

	    win = wnck_window_get (xid);
	    if (win)
	    {
		Window select;

		if (get_window_prop (xid, select_window_atom, &select))
		    update_switcher_window (win, select);
	    }
	}
	break;
    case DestroyNotify:
	g_hash_table_remove (frame_table,
			     GINT_TO_POINTER (xevent->xproperty.window));
	break;
    case ClientMessage:
	if (xevent->xclient.message_type == toolkit_action_atom)
	{
	    long action;

	    action = xevent->xclient.data.l[0];
	    if (action == toolkit_action_window_menu_atom)
	    {
		WnckWindow *win;

		win = wnck_window_get (xevent->xclient.window);
		if (win)
		{
		    action_menu_map (win,
				     xevent->xclient.data.l[2],
				     xevent->xclient.data.l[1]);
		}
	    }
	    else if (action == toolkit_action_force_quit_dialog_atom)
	    {
		WnckWindow *win;

		win = wnck_window_get (xevent->xclient.window);
		if (win)
		{
		    if (xevent->xclient.data.l[2])
			show_force_quit_dialog (win,
						xevent->xclient.data.l[1]);
		    else
			hide_force_quit_dialog (win);
		}
	    }
	}
    default:
	break;
    }

    if (xid)
    {
	WnckWindow *win;

	win = wnck_window_get (xid);
	if (win)
	{
	    static event_callback callback[3][3] = {
		{ top_left_event,    top_event,    top_right_event    },
		{ left_event,	     title_event,  right_event	      },
		{ bottom_left_event, bottom_event, bottom_right_event }
	    };
	    static event_callback button_callback[BUTTON_NUM] = {
		close_button_event,
		max_button_event,
		min_button_event,
		menu_button_event,
		shade_button_event,
		above_button_event,
		stick_button_event,
		unshade_button_event,
		unabove_button_event,
		unstick_button_event
	    };
	    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

	    if (d->decorated)
	    {
		gint i, j;

		for (i = 0; i < 3; i++)
		    for (j = 0; j < 3; j++)
			if (d->event_windows[i][j] == xevent->xany.window)
			    (*callback[i][j]) (win, xevent);

		for (i = 0; i < BUTTON_NUM; i++)
		    if (d->button_windows[i] == xevent->xany.window)
			(*button_callback[i]) (win, xevent);
	    }
	}
    }

    return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn
selection_event_filter_func (GdkXEvent *gdkxevent,
			     GdkEvent  *event,
			     gpointer  data)
{
    Display    *xdisplay;
    GdkDisplay *gdkdisplay;
    XEvent     *xevent = gdkxevent;
    int	       status;

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = GDK_DISPLAY_XDISPLAY (gdkdisplay);

    switch (xevent->type) {
    case SelectionRequest:
	decor_handle_selection_request (xdisplay, xevent, dm_sn_timestamp);
	break;
    case SelectionClear:
	status = decor_handle_selection_clear (xdisplay, xevent, 0);
	if (status == DECOR_SELECTION_GIVE_UP)
	    exit (0);
    default:
	break;
    }

    return GDK_FILTER_CONTINUE;
}


/* from clearlooks theme */
static void
rgb_to_hls (gdouble *r,
	    gdouble *g,
	    gdouble *b)
{
    gdouble min;
    gdouble max;
    gdouble red;
    gdouble green;
    gdouble blue;
    gdouble h, l, s;
    gdouble delta;

    red = *r;
    green = *g;
    blue = *b;

    if (red > green)
    {
	if (red > blue)
	    max = red;
	else
	    max = blue;

	if (green < blue)
	    min = green;
	else
	    min = blue;
    }
    else
    {
	if (green > blue)
	    max = green;
	else
	    max = blue;

	if (red < blue)
	    min = red;
	else
	    min = blue;
    }

    l = (max + min) / 2;
    s = 0;
    h = 0;

    if (max != min)
    {
	if (l <= 0.5)
	    s = (max - min) / (max + min);
	else
	    s = (max - min) / (2 - max - min);

	delta = max -min;
	if (red == max)
	    h = (green - blue) / delta;
	else if (green == max)
	    h = 2 + (blue - red) / delta;
	else if (blue == max)
	    h = 4 + (red - green) / delta;

	h *= 60;
	if (h < 0.0)
	    h += 360;
    }

    *r = h;
    *g = l;
    *b = s;
}

static void
hls_to_rgb (gdouble *h,
	    gdouble *l,
	    gdouble *s)
{
    gdouble hue;
    gdouble lightness;
    gdouble saturation;
    gdouble m1, m2;
    gdouble r, g, b;

    lightness = *l;
    saturation = *s;

    if (lightness <= 0.5)
	m2 = lightness * (1 + saturation);
    else
	m2 = lightness + saturation - lightness * saturation;

    m1 = 2 * lightness - m2;

    if (saturation == 0)
    {
	*h = lightness;
	*l = lightness;
	*s = lightness;
    }
    else
    {
	hue = *h + 120;
	while (hue > 360)
	    hue -= 360;
	while (hue < 0)
	    hue += 360;

	if (hue < 60)
	    r = m1 + (m2 - m1) * hue / 60;
	else if (hue < 180)
	    r = m2;
	else if (hue < 240)
	    r = m1 + (m2 - m1) * (240 - hue) / 60;
	else
	    r = m1;

	hue = *h;
	while (hue > 360)
	    hue -= 360;
	while (hue < 0)
	    hue += 360;

	if (hue < 60)
	    g = m1 + (m2 - m1) * hue / 60;
	else if (hue < 180)
	    g = m2;
	else if (hue < 240)
	    g = m1 + (m2 - m1) * (240 - hue) / 60;
	else
	    g = m1;

	hue = *h - 120;
	while (hue > 360)
	    hue -= 360;
	while (hue < 0)
	    hue += 360;

	if (hue < 60)
	    b = m1 + (m2 - m1) * hue / 60;
	else if (hue < 180)
	    b = m2;
	else if (hue < 240)
	    b = m1 + (m2 - m1) * (240 - hue) / 60;
	else
	    b = m1;

	*h = r;
	*l = g;
	*s = b;
    }
}

static void
shade (const decor_color_t *a,
       decor_color_t	   *b,
       float		   k)
{
    double red;
    double green;
    double blue;

    red   = a->r;
    green = a->g;
    blue  = a->b;

    rgb_to_hls (&red, &green, &blue);

    green *= k;
    if (green > 1.0)
	green = 1.0;
    else if (green < 0.0)
	green = 0.0;

    blue *= k;
    if (blue > 1.0)
	blue = 1.0;
    else if (blue < 0.0)
	blue = 0.0;

    hls_to_rgb (&red, &green, &blue);

    b->r = red;
    b->g = green;
    b->b = blue;
}

static void
update_style (GtkWidget *widget)
{
    decor_color_t spot_color;
#if GTK_CHECK_VERSION (3, 0, 0)
    GtkStyleContext *context;
    GdkRGBA bg;

    context = gtk_widget_get_style_context (widget);

    gtk_style_context_save (context);
    gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);
    gtk_style_context_get_background_color (context, gtk_style_context_get_state (context), &bg);
    gtk_style_context_restore (context);

    spot_color.r = bg.red;
    spot_color.g = bg.green;
    spot_color.b = bg.blue;
#else
    GtkStyle      *style;

    style = gtk_widget_get_style (widget);
    g_object_ref (G_OBJECT (style));

    style = gtk_style_attach (style, gtk_widget_get_window (widget));

    spot_color.r = style->bg[GTK_STATE_SELECTED].red   / 65535.0;
    spot_color.g = style->bg[GTK_STATE_SELECTED].green / 65535.0;
    spot_color.b = style->bg[GTK_STATE_SELECTED].blue  / 65535.0;

    g_object_unref (G_OBJECT (style));
#endif

    shade (&spot_color, &_title_color[0], 1.05);
    shade (&_title_color[0], &_title_color[1], 0.85);
}

/* to save some memory, value is specific to current decorations */
#define TRANSLUCENT_CORNER_SIZE 3

static void
draw_border_shape (Display	   *xdisplay,
		   Pixmap	   pixmap,
		   Picture	   picture,
		   int		   width,
		   int		   height,
		   decor_context_t *c,
		   void		   *closure)
{
    static XRenderColor white = { 0xffff, 0xffff, 0xffff, 0xffff };
    GdkScreen		*screen;
    decor_t		d;
    double		save_decoration_alpha;

    memset (&d, 0, sizeof (d));

    screen = gdk_screen_get_default ();

    d.surface = cairo_xlib_surface_create (GDK_SCREEN_XDISPLAY (screen),
                                           pixmap,
                                           GDK_VISUAL_XVISUAL (gdk_screen_get_rgba_visual (screen)),
                                           width,
                                           height);

    d.width   = width;
    d.height  = height;
    d.active  = TRUE;
    d.draw    = theme_draw_window_decoration;
    d.picture = picture;
    d.context = c;

    /* we use closure argument if maximized */
    if (closure)
	d.state |=
	    WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
	    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY;

    decor_get_default_layout (c, 1, 1, &d.border_layout);

    /* create shadow from opaque decoration
     * FIXME: Should not modify settings value
     * like this */
    save_decoration_alpha = decoration_alpha;
    decoration_alpha = 1.0;

    (*d.draw) (&d);

    decoration_alpha = save_decoration_alpha;

    XRenderFillRectangle (xdisplay, PictOpSrc, picture, &white,
			  c->left_space,
			  c->top_space,
			  width - c->left_space - c->right_space,
			  height - c->top_space - c->bottom_space);

    cairo_surface_destroy (d.surface);
}

static int
update_shadow (void)
{
    decor_shadow_options_t opt;
    GdkDisplay		   *display  = gdk_display_get_default ();
    Display		   *xdisplay = GDK_DISPLAY_XDISPLAY (display);
    GdkScreen		   *screen   = gdk_display_get_default_screen (display);

    opt.shadow_radius  = shadow_radius;
    opt.shadow_opacity = shadow_opacity;

    memcpy (opt.shadow_color, shadow_color, sizeof (shadow_color));

    opt.shadow_offset_x = shadow_offset_x;
    opt.shadow_offset_y = shadow_offset_y;

    if (no_border_shadow)
    {
	decor_shadow_destroy (xdisplay, no_border_shadow);
	no_border_shadow = NULL;
    }

    no_border_shadow = decor_shadow_create (xdisplay,
					    gdk_x11_screen_get_xscreen (screen),
					    1, 1,
					    0,
					    0,
					    0,
					    0,
					    0, 0, 0, 0,
					    &opt,
					    &shadow_context,
					    decor_draw_simple,
					    0);

    if (border_shadow)
    {
	decor_shadow_destroy (xdisplay, border_shadow);
	border_shadow = NULL;
    }

    border_shadow = decor_shadow_create (xdisplay,
					 gdk_x11_screen_get_xscreen (screen),
					 1, 1,
					 _win_extents.left,
					 _win_extents.right,
					 _win_extents.top + titlebar_height,
					 _win_extents.bottom,
					 _win_extents.left -
					 TRANSLUCENT_CORNER_SIZE,
					 _win_extents.right -
					 TRANSLUCENT_CORNER_SIZE,
					 _win_extents.top + titlebar_height -
					 TRANSLUCENT_CORNER_SIZE,
					 _win_extents.bottom -
					 TRANSLUCENT_CORNER_SIZE,
					 &opt,
					 &window_context,
					 draw_border_shape,
					 0);

    if (max_border_shadow)
    {
	decor_shadow_destroy (xdisplay, max_border_shadow);
	max_border_shadow = NULL;
    }

    max_border_shadow =
	decor_shadow_create (xdisplay,
			     gdk_x11_screen_get_xscreen (screen),
			     1, 1,
			     _max_win_extents.left,
			     _max_win_extents.right,
			     _max_win_extents.top + max_titlebar_height,
			     _max_win_extents.bottom,
			     _max_win_extents.left - TRANSLUCENT_CORNER_SIZE,
			     _max_win_extents.right - TRANSLUCENT_CORNER_SIZE,
			     _max_win_extents.top + max_titlebar_height -
			     TRANSLUCENT_CORNER_SIZE,
			     _max_win_extents.bottom - TRANSLUCENT_CORNER_SIZE,
			     &opt,
			     &max_window_context,
			     draw_border_shape,
			     (void *) 1);

    if (switcher_shadow)
    {
	decor_shadow_destroy (xdisplay, switcher_shadow);
	switcher_shadow = NULL;
    }

    switcher_shadow = decor_shadow_create (xdisplay,
					   gdk_x11_screen_get_xscreen (screen),
					   1, 1,
					   _switcher_extents.left,
					   _switcher_extents.right,
					   _switcher_extents.top,
					   _switcher_extents.bottom,
					   _switcher_extents.left -
					   TRANSLUCENT_CORNER_SIZE,
					   _switcher_extents.right -
					   TRANSLUCENT_CORNER_SIZE,
					   _switcher_extents.top -
					   TRANSLUCENT_CORNER_SIZE,
					   _switcher_extents.bottom -
					   TRANSLUCENT_CORNER_SIZE,
					   &opt,
					   &switcher_context,
					   decor_draw_simple,
					   0);

    return 1;
}

static void
update_window_decoration (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    if (d->decorated)
    {
	/* force size update */
	d->context = NULL;
	d->width = d->height = 0;

	update_window_decoration_size (win);
	update_event_windows (win);
    }
    else
    {
	Window xid = wnck_window_get_xid (win);
	Window select;

	if (get_window_prop (xid, select_window_atom, &select))
	{
	    /* force size update */
	    d->context = NULL;
	    d->width = d->height = 0;
	    switcher_width = switcher_height = 0;

	    update_switcher_window (win, select);
	}
    }
}

static const PangoFontDescription *
get_titlebar_font (void)
{
    if (use_system_font)
    {
	return NULL;
    }
    else
	return titlebar_font;
}

#ifdef USE_GSETTINGS
static void
titlebar_font_changed (GSettings *settings_wm)
{
    if (!settings_wm)
	return;

    gchar *str;

    str = g_settings_get_string (settings_wm,
				 META_TITLEBAR_FONT_KEY);
    if (!str)
	str = g_strdup ("Sans Bold 12");

    if (titlebar_font)
	pango_font_description_free (titlebar_font);

    titlebar_font = pango_font_description_from_string (str);

    g_free (str);
}

static void
titlebar_click_action_changed (GSettings   *settings_wm,
			       const gchar *key,
			       int         *action_value,
			       int          default_value)
{
    if (!settings_wm)
	return;

    gchar *action;

    *action_value = default_value;

    action = g_settings_get_string (settings_wm, key);
    if (action)
    {
	if (strcmp (action, "toggle_shade") == 0)
	    *action_value = CLICK_ACTION_SHADE;
	else if (strcmp (action, "toggle_maximize") == 0)
	    *action_value = CLICK_ACTION_MAXIMIZE;
	else if (strcmp (action, "toggle_maximize_horizontally") == 0)
	    *action_value = CLICK_ACTION_MAXIMIZE_HORZ;
	else if (strcmp (action, "toggle_maximize_vertically") == 0)
	    *action_value = CLICK_ACTION_MAXIMIZE_VERT;
	else if (strcmp (action, "minimize") == 0)
	    *action_value = CLICK_ACTION_MINIMIZE;
	else if (strcmp (action, "raise") == 0)
	    *action_value = CLICK_ACTION_RAISE;
	else if (strcmp (action, "lower") == 0)
	    *action_value = CLICK_ACTION_LOWER;
	else if (strcmp (action, "menu") == 0)
	    *action_value = CLICK_ACTION_MENU;
	else if (strcmp (action, "none") == 0)
	    *action_value = CLICK_ACTION_NONE;

	g_free (action);
    }
}

static void
wheel_action_changed (GSettings *settings)
{
    if (!settings)
	return;

    gchar *action;

    wheel_action = WHEEL_ACTION_DEFAULT;

    action = g_settings_get_string (settings, GWD_WHEEL_ACTION_KEY);
    if (action)
    {
	if (strcmp (action, "shade") == 0)
	    wheel_action = WHEEL_ACTION_SHADE;
	else if (strcmp (action, "none") == 0)
	    wheel_action = WHEEL_ACTION_NONE;

	g_free (action);
    }
}
#endif

#ifdef USE_MARCO
static MetaButtonFunction
meta_button_function_from_string (const char *str)
{
    if (strcmp (str, "menu") == 0)
	return META_BUTTON_FUNCTION_MENU;
    else if (strcmp (str, "minimize") == 0)
	return META_BUTTON_FUNCTION_MINIMIZE;
    else if (strcmp (str, "maximize") == 0)
	return META_BUTTON_FUNCTION_MAXIMIZE;
    else if (strcmp (str, "close") == 0)
	return META_BUTTON_FUNCTION_CLOSE;
    else if (strcmp (str, "shade") == 0)
	return META_BUTTON_FUNCTION_SHADE;
    else if (strcmp (str, "above") == 0)
	return META_BUTTON_FUNCTION_ABOVE;
    else if (strcmp (str, "stick") == 0)
	return META_BUTTON_FUNCTION_STICK;
    else if (strcmp (str, "unshade") == 0)
	return META_BUTTON_FUNCTION_UNSHADE;
    else if (strcmp (str, "unabove") == 0)
	return META_BUTTON_FUNCTION_UNABOVE;
    else if (strcmp (str, "unstick") == 0)
	return META_BUTTON_FUNCTION_UNSTICK;
    else
	return META_BUTTON_FUNCTION_LAST;
}

static MetaButtonFunction
meta_button_opposite_function (MetaButtonFunction ofwhat)
{
    switch (ofwhat)
    {
    case META_BUTTON_FUNCTION_SHADE:
	return META_BUTTON_FUNCTION_UNSHADE;
    case META_BUTTON_FUNCTION_UNSHADE:
	return META_BUTTON_FUNCTION_SHADE;

    case META_BUTTON_FUNCTION_ABOVE:
	return META_BUTTON_FUNCTION_UNABOVE;
    case META_BUTTON_FUNCTION_UNABOVE:
	return META_BUTTON_FUNCTION_ABOVE;

    case META_BUTTON_FUNCTION_STICK:
	return META_BUTTON_FUNCTION_UNSTICK;
    case META_BUTTON_FUNCTION_UNSTICK:
	return META_BUTTON_FUNCTION_STICK;

    default:
	return META_BUTTON_FUNCTION_LAST;
    }
}

static void
meta_initialize_button_layout (MetaButtonLayout *layout)
{
    int	i;

    for (i = 0; i < MAX_BUTTONS_PER_CORNER; i++)
    {
	layout->left_buttons[i] = META_BUTTON_FUNCTION_LAST;
	layout->right_buttons[i] = META_BUTTON_FUNCTION_LAST;
	layout->left_buttons_has_spacer[i] = FALSE;
	layout->right_buttons_has_spacer[i] = FALSE;
    }
}

static void
meta_update_button_layout (const char *value)
{
    MetaButtonLayout   new_layout;
    MetaButtonFunction f;
    char	       **sides;
    int		       i;

    meta_initialize_button_layout (&new_layout);

    sides = g_strsplit (value, ":", 2);

    if (sides[0] != NULL)
    {
	char	 **buttons;
	int	 b;
	gboolean used[META_BUTTON_FUNCTION_LAST];

	for (i = 0; i < META_BUTTON_FUNCTION_LAST; i++)
	   used[i] = FALSE;

	buttons = g_strsplit (sides[0], ",", -1);

	i = b = 0;
	while (buttons[b] != NULL)
	{
	    f = meta_button_function_from_string (buttons[b]);
	    if (i > 0 && strcmp("spacer", buttons[b]) == 0)
            {
	       new_layout.left_buttons_has_spacer[i - 1] = TRUE;
	       f = meta_button_opposite_function (f);

	       if (f != META_BUTTON_FUNCTION_LAST)
                  new_layout.left_buttons_has_spacer[i - 2] = TRUE;
            }
	    else
	    {
	       if (f != META_BUTTON_FUNCTION_LAST && !used[f])
	       {
                  used[f] = TRUE;
                  new_layout.left_buttons[i++] = f;

		  f = meta_button_opposite_function (f);

                  if (f != META_BUTTON_FUNCTION_LAST)
                      new_layout.left_buttons[i++] = f;

	       }
	       else
	       {
		  fprintf (stderr, "%s: Ignoring unknown or already-used "
			   "button name \"%s\"\n", program_name, buttons[b]);
	       }
	    }
	    b++;
	}

	new_layout.left_buttons[i] = META_BUTTON_FUNCTION_LAST;

	g_strfreev (buttons);

	if (sides[1] != NULL)
	{
	    for (i = 0; i < META_BUTTON_FUNCTION_LAST; i++)
		used[i] = FALSE;

	    buttons = g_strsplit (sides[1], ",", -1);

	    i = b = 0;
	    while (buttons[b] != NULL)
	    {
	       f = meta_button_function_from_string (buttons[b]);
	       if (i > 0 && strcmp("spacer", buttons[b]) == 0)
	       {
		  new_layout.right_buttons_has_spacer[i - 1] = TRUE;
		  f = meta_button_opposite_function (f);
		  if (f != META_BUTTON_FUNCTION_LAST)
		     new_layout.right_buttons_has_spacer[i - 2] = TRUE;
	       }
	       else
	       {
		   if (f != META_BUTTON_FUNCTION_LAST && !used[f])
		   {
		       used[f] = TRUE;
		       new_layout.right_buttons[i++] = f;

		       f = meta_button_opposite_function (f);

		       if (f != META_BUTTON_FUNCTION_LAST)
			   new_layout.right_buttons[i++] = f;
		   }
		   else
		   {
		       fprintf (stderr, "%s: Ignoring unknown or "
				"already-used button name \"%s\"\n",
				program_name, buttons[b]);
		   }
	       }
	       b++;
	    }
	    new_layout.right_buttons[i] = META_BUTTON_FUNCTION_LAST;

	    g_strfreev (buttons);
	}
    }

    g_strfreev (sides);

    /* Invert the button layout for RTL languages */
    if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
    {
	MetaButtonLayout rtl_layout;
	int j;

	meta_initialize_button_layout (&rtl_layout);

	i = 0;
	while (new_layout.left_buttons[i] != META_BUTTON_FUNCTION_LAST)
	    i++;

	for (j = 0; j < i; j++)
	{
	    rtl_layout.right_buttons[j] = new_layout.left_buttons[i - j - 1];
	    if (j == 0)
		rtl_layout.right_buttons_has_spacer[i - 1] =
		    new_layout.left_buttons_has_spacer[i - j - 1];
	    else
		rtl_layout.right_buttons_has_spacer[j - 1] =
		    new_layout.left_buttons_has_spacer[i - j - 1];
	}

	i = 0;
	while (new_layout.right_buttons[i] != META_BUTTON_FUNCTION_LAST)
	    i++;

	for (j = 0; j < i; j++)
	{
	    rtl_layout.left_buttons[j] = new_layout.right_buttons[i - j - 1];
	    if (j == 0)
		rtl_layout.left_buttons_has_spacer[i - 1] =
		    new_layout.right_buttons_has_spacer[i - j - 1];
	    else
		rtl_layout.left_buttons_has_spacer[j - 1] =
		    new_layout.right_buttons_has_spacer[i - j - 1];
	}

	new_layout = rtl_layout;
    }

    meta_button_layout = new_layout;
}
#endif

static void
update_border_extents (gint text_height)
{
    _win_extents = _default_win_extents;
    _max_win_extents = _default_win_extents;
    max_titlebar_height = titlebar_height =
	(text_height < 17) ? 17 : text_height;
}

#ifdef USE_MARCO
static void
meta_update_border_extents (gint text_height)
{
    MetaTheme *theme;
    gint      top_height, bottom_height, left_width, right_width;

    theme = meta_theme_get_current ();

    meta_theme_get_frame_borders (theme,
				  META_FRAME_TYPE_NORMAL,
				  text_height,
				  0,
				  &top_height,
				  &bottom_height,
				  &left_width,
				  &right_width);

    _win_extents.top    = _default_win_extents.top;
    _win_extents.bottom = bottom_height;
    _win_extents.left   = left_width;
    _win_extents.right  = right_width;

    titlebar_height = top_height - _win_extents.top;

    meta_theme_get_frame_borders (theme,
				  META_FRAME_TYPE_NORMAL,
				  text_height,
				  META_FRAME_MAXIMIZED,
				  &top_height,
				  &bottom_height,
				  &left_width,
				  &right_width);

    _max_win_extents.top    = _default_win_extents.top;
    _max_win_extents.bottom = bottom_height;
    _max_win_extents.left   = left_width;
    _max_win_extents.right  = right_width;

    max_titlebar_height = top_height - _max_win_extents.top;
}
#endif

static void
update_titlebar_font (void)
{
    const PangoFontDescription *font_desc;
#if GTK_CHECK_VERSION (3, 0, 0)
    PangoFontDescription       *free_font_desc = NULL;
    PangoFontMetrics	       *metrics;
    PangoLanguage	       *lang;
#else
    PangoFontMetrics	       *metrics;
    PangoLanguage	       *lang;
#endif

    font_desc = get_titlebar_font ();
    if (!font_desc)
    {
#if GTK_CHECK_VERSION (3, 0, 0)
	GtkCssProvider *provider = gtk_css_provider_get_default ();
	GtkStyleContext *context = gtk_style_context_new ();
	GtkWidgetPath *path = gtk_widget_path_new ();

	gtk_widget_path_prepend_type (path, GTK_TYPE_WIDGET);
	gtk_style_context_set_path (context, path);

	gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

	gtk_style_context_save (context);
	gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
	gtk_style_context_get (context, gtk_style_context_get_state (context), "font", &free_font_desc, NULL);
	gtk_style_context_restore (context);

	font_desc = (const PangoFontDescription *) free_font_desc;

	gtk_widget_path_free (path);
#else
	GtkStyle *default_style;

	default_style = gtk_widget_get_default_style ();
	font_desc = default_style->font_desc;
#endif
    }

    pango_context_set_font_description (pango_context, font_desc);

    lang    = pango_context_get_language (pango_context);
    metrics = pango_context_get_metrics (pango_context, font_desc, lang);

    text_height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
				pango_font_metrics_get_descent (metrics));

    pango_font_metrics_unref (metrics);

#if GTK_CHECK_VERSION (3, 0, 0)
    if (free_font_desc)
        pango_font_description_free (free_font_desc);
#endif
}

static void
decorations_changed (WnckScreen *screen)
{
    GdkDisplay *gdkdisplay;
    GdkScreen  *gdkscreen;
    GList      *windows;

    gdkdisplay = gdk_display_get_default ();
    gdkscreen  = gdk_display_get_default_screen (gdkdisplay);

    update_titlebar_font ();
    (*theme_update_border_extents) (text_height);
    update_shadow ();

    update_default_decorations (gdkscreen);

    if (minimal)
	return;

    windows = wnck_screen_get_windows (screen);
    while (windows != NULL)
    {
	decor_t *d = g_object_get_data (G_OBJECT (windows->data), "decor");

	if (d->decorated)
	{

#ifdef USE_MARCO
	    if (d->draw == draw_window_decoration ||
		d->draw == meta_draw_window_decoration)
		d->draw = theme_draw_window_decoration;
#endif

	}

	update_window_decoration (WNCK_WINDOW (windows->data));
	windows = windows->next;
    }
}

static void
style_changed (GtkWidget *widget,
               void      *user_data)
{
    GdkDisplay *gdkdisplay;
    GdkScreen  *gdkscreen;
    WnckScreen *screen;

    PangoContext *context = (PangoContext *) user_data;

    gdkdisplay = gdk_display_get_default ();
    gdkscreen  = gdk_display_get_default_screen (gdkdisplay);
    screen     = wnck_screen_get_default ();

    update_style (widget);

    pango_cairo_context_set_resolution (context, gdk_screen_get_resolution (gdkscreen));

    decorations_changed (screen);
}

#ifdef USE_GSETTINGS
static gboolean
blur_settings_changed (GSettings *settings)
{
    if (!settings)
	return FALSE;

    gchar *type;
    int   new_type = blur_type;

    if (cmdline_options & CMDLINE_BLUR)
	return FALSE;

    type = g_settings_get_string (settings,
				  GWD_BLUR_TYPE_KEY);

    if (type)
    {
	if (strcmp (type, "titlebar") == 0)
	    new_type = BLUR_TYPE_TITLEBAR;
	else if (strcmp (type, "all") == 0)
	    new_type = BLUR_TYPE_ALL;
	else if (strcmp (type, "none") == 0)
	    new_type = BLUR_TYPE_NONE;

	g_free (type);
    }

    if (new_type != blur_type)
    {
	blur_type = new_type;
	return TRUE;
    }

    return FALSE;
}

static gboolean
theme_changed (GSettings *settings, GSettings *settings_wm_theme)
{
#ifdef USE_MARCO
    gboolean  use_meta_theme = TRUE;
    gchar    *wm_schema;

    if (!settings || !settings_wm_theme)
	return FALSE;

    if (cmdline_options & CMDLINE_THEME)
	return FALSE;

    g_object_get (G_OBJECT (settings_wm_theme), "schema-id", &wm_schema, NULL);

    use_meta_theme = g_settings_get_boolean (settings,
					     GWD_USE_META_THEME_KEY);

    if (use_meta_theme)
    {
	gchar *theme;

	if (g_strcmp0 (wm_schema, GSCHEMA_KEY_METACITY_THEME) != 0)
	{
	    theme = g_settings_get_string (settings_wm_theme,
	                                   META_THEME_KEY_MARCO);
	}
	else
	{
	    theme = g_settings_get_string (settings_wm_theme,
	                                   META_THEME_KEY_METACITY);
	}

	/* if string is empty, release it */
	if (theme && strlen (theme) <= 0)
	{
	    g_free (theme);
	    theme = NULL;
	}

	/* if there's no theme defined, try using Gtk theme name,
	   especially useful for Metacity */
	if (!theme)
	{
	    g_object_get (G_OBJECT (gtk_settings_get_default ()),
	                  "gtk-theme-name", &theme,
	                  NULL);
	}

	if (theme)
	{
	    meta_theme_set_current (theme, TRUE);
	    if (!meta_theme_get_current ())
		use_meta_theme = FALSE;

	    g_free (theme);
	}
	else
	    use_meta_theme = FALSE;
    }

    if (use_meta_theme)
    {
	theme_draw_window_decoration	= meta_draw_window_decoration;
	theme_calc_decoration_size	= meta_calc_decoration_size;
	theme_update_border_extents	= meta_update_border_extents;
	theme_get_event_window_position = meta_get_event_window_position;
	theme_get_button_position	= meta_get_button_position;
    }
    else
    {
	theme_draw_window_decoration	= draw_window_decoration;
	theme_calc_decoration_size	= calc_decoration_size;
	theme_update_border_extents	= update_border_extents;
	theme_get_event_window_position = get_event_window_position;
	theme_get_button_position	= get_button_position;
    }

    g_free (wm_schema);

    return TRUE;
#else
    theme_draw_window_decoration    = draw_window_decoration;
    theme_calc_decoration_size	    = calc_decoration_size;
    theme_update_border_extents	    = update_border_extents;
    theme_get_event_window_position = get_event_window_position;
    theme_get_button_position	    = get_button_position;

    return FALSE;
#endif

}

static gboolean
theme_opacity_changed (GSettings *settings)
{
    if (!settings)
	return FALSE;

#ifdef USE_MARCO
    gboolean shade_opacity, changed = FALSE;
    gdouble  opacity;

    opacity = g_settings_get_double (settings,
				     GWD_META_THEME_OPACITY_KEY);

    if (!(cmdline_options & CMDLINE_OPACITY) &&
	opacity != meta_opacity)
    {
	meta_opacity = opacity;
	changed = TRUE;
    }

    if (opacity < 1.0)
    {
	shade_opacity = g_settings_get_boolean (settings,
						GWD_META_THEME_SHADE_OPACITY_KEY);

	if (!(cmdline_options & CMDLINE_OPACITY_SHADE) &&
	    shade_opacity != meta_shade_opacity)
	{
	    meta_shade_opacity = shade_opacity;
	    changed = TRUE;
	}
    }

    opacity = g_settings_get_double (settings,
				     GWD_META_THEME_ACTIVE_OPACITY_KEY);

    if (!(cmdline_options & CMDLINE_ACTIVE_OPACITY) &&
	opacity != meta_active_opacity)
    {
	meta_active_opacity = opacity;
	changed = TRUE;
    }

    if (opacity < 1.0)
    {
	shade_opacity =
	    g_settings_get_boolean (settings,
				    GWD_META_THEME_ACTIVE_SHADE_OPACITY_KEY);

	if (!(cmdline_options & CMDLINE_ACTIVE_OPACITY_SHADE) &&
	    shade_opacity != meta_active_shade_opacity)
	{
	    meta_active_shade_opacity = shade_opacity;
	    changed = TRUE;
	}
    }

    return changed;
#else
    return FALSE;
#endif

}

static gboolean
button_layout_changed (GSettings *settings_wm)
{
    if (!settings_wm)
	return FALSE;

#ifdef USE_MARCO
    gchar *button_layout;

    button_layout = g_settings_get_string (settings_wm,
					   META_BUTTON_LAYOUT_KEY);

    if (button_layout)
    {
	meta_update_button_layout (button_layout);

	meta_button_layout_set = TRUE;

	g_free (button_layout);

	return TRUE;
    }

    if (meta_button_layout_set)
    {
	meta_button_layout_set = FALSE;
	return TRUE;
    }
#endif

    return FALSE;
}

#ifdef USE_COMPIZCONFIG
static void
cursor_theme_changed (GSettings *settings_mouse, CCSContext *ccs_context)
{
    CCSPlugin *core_plugin;
    CCSSetting *plugin_setting;
    gchar *theme;
    gint size;

    if (!(ccs_context && ccsPluginIsActive (ccs_context, COMPIZCONFIG_CORE_OPTION)))
	return;

    /* Note that ccsFindPlugin and ccsFindSetting are not really allocating anything */
    core_plugin = ccsFindPlugin (ccs_context, COMPIZCONFIG_CORE_OPTION);
    if (!core_plugin)
	return;

    theme = g_settings_get_string (settings_mouse, MATE_CURSOR_THEME_KEY);
    size = g_settings_get_int (settings_mouse, MATE_CURSOR_SIZE_KEY);

    if (theme && strlen (theme) > 0)
    {
	int i = 0, j = 0;
	GdkDisplay *gdkdisplay = gdk_display_get_default ();
	Display *xdisplay = gdk_x11_display_get_xdisplay (gdkdisplay);

	for (i = 0; i < 3; i++)
	{
	    for (j = 0; j < 3; j++)
	    {
		if (cursor[i][j].shape != XC_left_ptr)
		{
		    XFreeCursor (xdisplay, cursor[i][j].cursor);
		    cursor[i][j].cursor = XCreateFontCursor (xdisplay, cursor[i][j].shape);
		}
	    }
	}

	plugin_setting = ccsFindSetting (core_plugin, COMPIZCONFIG_CURSOR_THEME_OPTION, 0, 0);
	if (plugin_setting)
	    ccsSetString (plugin_setting, theme);

	plugin_setting = ccsFindSetting (core_plugin, COMPIZCONFIG_CURSOR_SIZE_OPTION, 0, 0);
	if (plugin_setting)
	    ccsSetInt (plugin_setting, size);

	ccsWriteChangedSettings (ccs_context);

	g_free (theme);
    }
}
#endif

static void
gsettings_value_changed (GSettings   *settings,
		         const gchar *key,
		         WnckScreen  *screen)
{
    GSettings  *settings_gwd = NULL;
    GSettings  *settings_wm = NULL, *settings_wm_theme = NULL;
#ifdef USE_COMPIZCONFIG
    GSettings  *settings_mouse = NULL;
    CCSContext *ccs_context = NULL;
#endif
    gboolean   changed = FALSE;

    if (!settings)
	return;

    settings_gwd      = g_object_get_data (G_OBJECT (settings), "gsettings_gwd");
    settings_wm       = g_object_get_data (G_OBJECT (settings), "gsettings_wm");
    settings_wm_theme = g_object_get_data (G_OBJECT (settings), "gsettings_wm_theme");
#ifdef USE_COMPIZCONFIG
    settings_mouse    = g_object_get_data (G_OBJECT (settings), "gsettings_mouse");
    ccs_context       = g_object_get_data (G_OBJECT (settings), "ccs_context");
#endif

    if (!settings_gwd || !settings_wm || !settings_wm_theme)
	return;

    if (strcmp (key, META_USE_SYSTEM_FONT_KEY) == 0)
    {
	if (g_settings_get_boolean (settings_wm,
				    META_USE_SYSTEM_FONT_KEY)
				    != use_system_font)
	{
	    use_system_font = !use_system_font;
	    changed = TRUE;
	}
    }
    else if (strcmp (key, META_TITLEBAR_FONT_KEY) == 0)
    {
	titlebar_font_changed (settings_wm);
	changed = !use_system_font;
    }
    else if (strcmp (key, META_DOUBLE_CLICK_TITLEBAR_KEY) == 0)
    {
	titlebar_click_action_changed (settings_wm, key,
				       &double_click_action,
				       DOUBLE_CLICK_ACTION_DEFAULT);
    }
    else if (strcmp (key, META_MIDDLE_CLICK_TITLEBAR_KEY) == 0)
    {
	titlebar_click_action_changed (settings_wm, key,
				       &middle_click_action,
				       MIDDLE_CLICK_ACTION_DEFAULT);
    }
    else if (strcmp (key, META_RIGHT_CLICK_TITLEBAR_KEY) == 0)
    {
	titlebar_click_action_changed (settings_wm, key,
				       &right_click_action,
				       RIGHT_CLICK_ACTION_DEFAULT);
    }
    else if (strcmp (key, GWD_WHEEL_ACTION_KEY) == 0)
    {
	wheel_action_changed (settings_gwd);
    }
    else if (strcmp (key, GWD_BLUR_TYPE_KEY) == 0)
    {
	if (blur_settings_changed (settings_gwd))
	    changed = TRUE;
    }
    else if (strcmp (key, GWD_USE_META_THEME_KEY) == 0 ||
	     strcmp (key, META_THEME_KEY_MARCO) == 0 ||
	     strcmp (key, META_THEME_KEY_METACITY) == 0)
    {
	if (theme_changed (settings_gwd, settings_wm_theme))
	    changed = TRUE;
    }
    else if (strcmp (key, META_BUTTON_LAYOUT_KEY) == 0)
    {
	if (button_layout_changed (settings_wm))
	    changed = TRUE;
    }
    else if (strcmp (key, GWD_META_THEME_OPACITY_KEY)	       == 0 ||
	     strcmp (key, GWD_META_THEME_SHADE_OPACITY_KEY)	       == 0 ||
	     strcmp (key, GWD_META_THEME_ACTIVE_OPACITY_KEY)       == 0 ||
	     strcmp (key, GWD_META_THEME_ACTIVE_SHADE_OPACITY_KEY) == 0)
    {
	if (theme_opacity_changed (settings_gwd))
	    changed = TRUE;
    }
#ifdef USE_COMPIZCONFIG
    else if (settings_mouse && ccs_context)
    {
	if (strcmp (key, MATE_CURSOR_THEME_KEY) == 0 ||
	    strcmp (key, MATE_CURSOR_SIZE_KEY) == 0)
	{
	    cursor_theme_changed (settings_mouse, ccs_context);
	}
    }
#endif

    if (changed)
	decorations_changed (screen);
}
#endif

#ifdef USE_COMPIZCONFIG
static gboolean
shadow_settings_changed (CCSContext *context)
{
    CCSPlugin		 *decor_plugin;
    CCSSetting		 *plugin_setting;
    float		 radius = 0.0, opacity = 0.0;
    int			 offset = 0;
    CCSSettingColorValue color;

    gboolean changed = FALSE;

    if (!(context && ccsPluginIsActive (context, COMPIZCONFIG_DECOR_OPTION)))
	return FALSE;

    /* Note that ccsFindPlugin and ccsFindSetting are not really allocating anything */
    decor_plugin = ccsFindPlugin (context, COMPIZCONFIG_DECOR_OPTION);
    if (!decor_plugin)
	return FALSE;

    plugin_setting = ccsFindSetting (decor_plugin, COMPIZCONFIG_SHADOW_RADIUS_OPTION, 0, 0);
    if (plugin_setting)
	ccsGetFloat (plugin_setting, &radius);
    radius = MAX (0.0, MIN (radius, 48.0));
    if (shadow_radius != radius)
    {
	shadow_radius = radius;
	changed = TRUE;
    }

    plugin_setting = ccsFindSetting (decor_plugin, COMPIZCONFIG_SHADOW_OPACITY_OPTION, 0, 0);
    if (plugin_setting)
	ccsGetFloat (plugin_setting, &opacity);
    opacity = MAX (0.0, MIN (opacity, 6.0));
    if (shadow_opacity != opacity)
    {
	shadow_opacity = opacity;
	changed = TRUE;
    }

    plugin_setting = ccsFindSetting (decor_plugin, COMPIZCONFIG_SHADOW_COLOR_OPTION, 0, 0);
    if (plugin_setting)
    {
	ccsGetColor (plugin_setting, &color);
	if (shadow_color[0] != color.array.array[0] ||
	    shadow_color[1] != color.array.array[1] ||
	    shadow_color[2] != color.array.array[2])
	{
	    shadow_color[0] = color.array.array[0];
	    shadow_color[1] = color.array.array[1];
	    shadow_color[2] = color.array.array[2];
	    changed = TRUE;
	}
    }

    plugin_setting = ccsFindSetting (decor_plugin, COMPIZCONFIG_SHADOW_OFFSET_X_OPTION, 0, 0);
    if (plugin_setting)
	ccsGetInt (plugin_setting, &offset);
    offset = MAX (-16, MIN (offset, 16));
    if (shadow_offset_x != offset)
    {
	shadow_offset_x = offset;
	changed = TRUE;
    }

    plugin_setting = ccsFindSetting (decor_plugin, COMPIZCONFIG_SHADOW_OFFSET_Y_OPTION, 0, 0);
    if (plugin_setting)
	ccsGetInt (plugin_setting, &offset);
    offset = MAX (-16, MIN (offset, 16));
    if (shadow_offset_y != offset)
    {
	shadow_offset_y = offset;
	changed = TRUE;
    }

    return changed;
}

static gboolean
compizconfig_value_changed (gpointer *data)
{
    gboolean changed = FALSE;

    WnckScreen *screen  = (WnckScreen *) data[0];
    CCSContext *context = (CCSContext *) data[1];

    if (!context)
	return FALSE;

    ccsProcessEvents (context, ProcessEventsNoGlibMainLoopMask);

    if (shadow_settings_changed (context))
	changed = TRUE;

    if (changed)
	decorations_changed (screen);

    return TRUE;
}
#elif USE_DBUS_GLIB
static DBusHandlerResult
dbus_handle_message (DBusConnection *connection,
		     DBusMessage    *message,
		     void           *user_data)
{
    WnckScreen	      *screen = user_data;
    char	      **path;
    const char        *interface, *member;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    interface = dbus_message_get_interface (message);
    member    = dbus_message_get_member (message);

    (void) connection;

    if (!interface || !member)
	return result;

    if (!dbus_message_is_signal (message, interface, member))
	return result;

    if (strcmp (member, "changed"))
	return result;

    if (!dbus_message_get_path_decomposed (message, &path))
	return result;

    if (!path[0] || !path[1] || !path[2] || !path[3] || !path[4] || !path[5])
    {
	dbus_free_string_array (path);
	return result;
    }

    if (!strcmp (path[0], "org")	 &&
	!strcmp (path[1], "freedesktop") &&
	!strcmp (path[2], "compiz")      &&
	!strcmp (path[3], "decoration")  &&
	!strcmp (path[4], "allscreens"))
    {
	result = DBUS_HANDLER_RESULT_HANDLED;

	if (strcmp (path[5], "shadow_radius") == 0)
	{
	    dbus_message_get_args (message, NULL,
				   DBUS_TYPE_DOUBLE, &shadow_radius,
				   DBUS_TYPE_INVALID);
	}
	else if (strcmp (path[5], "shadow_opacity") == 0)
	{
	    dbus_message_get_args (message, NULL,
				   DBUS_TYPE_DOUBLE, &shadow_opacity,
				   DBUS_TYPE_INVALID);
	}
	else if (strcmp (path[5], "shadow_color") == 0)
	{
	    DBusError error;
	    char      *str;

	    dbus_error_init (&error);

	    dbus_message_get_args (message, &error,
				   DBUS_TYPE_STRING, &str,
				   DBUS_TYPE_INVALID);

	    if (!dbus_error_is_set (&error))
	    {
		int c[4];

		if (sscanf (str, "#%2x%2x%2x%2x",
			    &c[0], &c[1], &c[2], &c[3]) == 4)
		{
		    shadow_color[0] = c[0] << 8 | c[0];
		    shadow_color[1] = c[1] << 8 | c[1];
		    shadow_color[2] = c[2] << 8 | c[2];
		}
	    }

	    dbus_error_free (&error);
	}
	else if (strcmp (path[5], "shadow_x_offset") == 0)
	{
	    dbus_message_get_args (message, NULL,
				   DBUS_TYPE_INT32, &shadow_offset_x,
				   DBUS_TYPE_INVALID);
	}
	else if (strcmp (path[5], "shadow_y_offset") == 0)
	{
	    dbus_message_get_args (message, NULL,
				   DBUS_TYPE_INT32, &shadow_offset_y,
				   DBUS_TYPE_INVALID);
	}

	decorations_changed (screen);
    }

    dbus_free_string_array (path);

    return result;
}

static DBusMessage *
send_and_block_for_shadow_option_reply (DBusConnection *connection,
					char	       *path)
{
    DBusMessage *message;

    message = dbus_message_new_method_call (NULL,
					    path,
					    DBUS_INTERFACE,
					    DBUS_METHOD_GET);
    if (message)
    {
	DBusMessage *reply;
	DBusError   error;

	dbus_message_set_destination (message, DBUS_DEST);

	dbus_error_init (&error);
	reply = dbus_connection_send_with_reply_and_block (connection,
							   message, -1,
							   &error);
	dbus_message_unref (message);

	if (!dbus_error_is_set (&error))
	    return reply;
    }

    return NULL;
}
#endif

#ifdef USE_GSETTINGS
static GSettings *
new_gsettings0 (const gchar *schema_id)
{
    GSettingsSchemaSource *schema_source;
    GSettingsSchema       *schema;

    schema_source = g_settings_schema_source_get_default ();
    schema = g_settings_schema_source_lookup (schema_source,
                                              schema_id,
                                              TRUE);

    if (schema)
    {
	g_settings_schema_unref (schema);
	return g_settings_new (schema_id);
    }
    else
	return NULL;
}
#endif

static gboolean
init_settings (WnckScreen *screen)
{
    GtkSettings	   *settings;
    GdkScreen	   *gdkscreen;
#if GTK_CHECK_VERSION (3, 0, 0)
    GdkVisual	   *visual;
#else
    GdkColormap	   *colormap;
#endif
    AtkObject	   *switcher_label_obj;

#ifdef USE_COMPIZCONFIG
    gpointer *compizconfig_value_data = malloc (sizeof (WnckScreen *) + sizeof (CCSContext *));
    CCSContext *ccs_context = ccsContextNew (NULL, 0);
#elif USE_DBUS_GLIB
    DBusConnection *connection;
    DBusMessage	   *reply;
    DBusError	   error;
#endif

#ifdef USE_GSETTINGS
    const gchar *session = g_getenv ("XDG_CURRENT_DESKTOP");
    GSettings	*gsettings_gwd, *gsettings_wm, *gsettings_wm_theme;
    GSettings	*gsettings_marco, *gsettings_gnome_wm, *gsettings_metacity;
#ifdef USE_COMPIZCONFIG
    GSettings	*gsettings_mouse, *gsettings_gnome;
#endif
    gboolean	 is_mate_desktop;
#endif

#ifdef USE_COMPIZCONFIG
    compizconfig_value_data[0] = (gpointer) screen;
    compizconfig_value_data[1] = (gpointer) ccs_context;

    /* check settings every second */
    g_timeout_add_seconds (1,
			   (GSourceFunc) compizconfig_value_changed,
			   (gpointer) compizconfig_value_data);
#elif USE_DBUS_GLIB
    DBusConnection *connection;
    DBusMessage	   *reply;
    DBusError	   error;

    dbus_error_init (&error);

    connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (!dbus_error_is_set (&error))
    {
	dbus_bus_add_match (connection, "type='signal'", &error);

	dbus_connection_add_filter (connection,
				    dbus_handle_message,
				    screen, NULL);

	dbus_connection_setup_with_g_main (connection, NULL);
    }

    reply = send_and_block_for_shadow_option_reply (connection, DBUS_PATH
						    "/shadow_radius");
    if (reply)
    {
	dbus_message_get_args (reply, NULL,
			       DBUS_TYPE_DOUBLE, &shadow_radius,
			       DBUS_TYPE_INVALID);

	dbus_message_unref (reply);
    }

    reply = send_and_block_for_shadow_option_reply (connection, DBUS_PATH
						    "/shadow_opacity");
    if (reply)
    {
	dbus_message_get_args (reply, NULL,
			       DBUS_TYPE_DOUBLE, &shadow_opacity,
			       DBUS_TYPE_INVALID);
	dbus_message_unref (reply);
    }

    reply = send_and_block_for_shadow_option_reply (connection, DBUS_PATH
						    "/shadow_color");
    if (reply)
    {
	DBusError error;
	char      *str;

	dbus_error_init (&error);

	dbus_message_get_args (reply, &error,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_INVALID);

	if (!dbus_error_is_set (&error))
	{
	    int c[4];

	    if (sscanf (str, "#%2x%2x%2x%2x", &c[0], &c[1], &c[2], &c[3]) == 4)
	    {
		shadow_color[0] = c[0] << 8 | c[0];
		shadow_color[1] = c[1] << 8 | c[1];
		shadow_color[2] = c[2] << 8 | c[2];
	    }
	}

	dbus_error_free (&error);

	dbus_message_unref (reply);
    }

    reply = send_and_block_for_shadow_option_reply (connection, DBUS_PATH
						    "/shadow_x_offset");
    if (reply)
    {
	dbus_message_get_args (reply, NULL,
			       DBUS_TYPE_INT32, &shadow_offset_x,
			       DBUS_TYPE_INVALID);
	dbus_message_unref (reply);
    }

    reply = send_and_block_for_shadow_option_reply (connection, DBUS_PATH
						    "/shadow_y_offset");
    if (reply)
    {
	dbus_message_get_args (reply, NULL,
			       DBUS_TYPE_INT32, &shadow_offset_y,
			       DBUS_TYPE_INVALID);
	dbus_message_unref (reply);
    }
#endif

#ifdef USE_GSETTINGS
    is_mate_desktop = FALSE;
    if (session && strlen (session) > 0) {
	int i = 0;
	gchar **sessions = g_strsplit (session, ":", -1);

	while (sessions[i])
	{
	    if (g_strcmp0 (sessions[i], "MATE") == 0)
	    {
		is_mate_desktop = TRUE;
		break;
	    }
	    ++i;
	}

	g_strfreev (sessions);
    }

    gsettings_gwd = new_gsettings0 (GSCHEMA_KEY_GWD);
    gsettings_wm = NULL;
    gsettings_wm_theme = NULL;

    gsettings_marco = new_gsettings0 (GSCHEMA_KEY_MARCO);
    gsettings_gnome_wm = new_gsettings0 (GSCHEMA_KEY_GNOME_WM);
    gsettings_metacity = new_gsettings0 (GSCHEMA_KEY_METACITY_THEME);
    if (!gsettings_metacity)
	gsettings_metacity = new_gsettings0 (GSCHEMA_KEY_METACITY);

    /* in general prioritise GNOME settings but Marco settings in MATE */
    if (gsettings_marco && is_mate_desktop)
	gsettings_wm = G_SETTINGS (g_object_ref (G_OBJECT (gsettings_marco)));
    else if (gsettings_gnome_wm)
	gsettings_wm = G_SETTINGS (g_object_ref (G_OBJECT (gsettings_gnome_wm)));
    else if (gsettings_marco)
	gsettings_wm = G_SETTINGS (g_object_ref (G_OBJECT (gsettings_marco)));

    /* in general prioritise Metacity settings but Marco settings in MATE */
    if (gsettings_marco && is_mate_desktop)
	gsettings_wm_theme = G_SETTINGS (g_object_ref (G_OBJECT (gsettings_marco)));
    else if (gsettings_metacity)
	gsettings_wm_theme = G_SETTINGS (g_object_ref (G_OBJECT (gsettings_metacity)));
    else if (gsettings_marco)
	gsettings_wm_theme = G_SETTINGS (g_object_ref (G_OBJECT (gsettings_marco)));

    /* unreference all, good stuff was rereferenced above */
    if (gsettings_marco)
	g_object_unref (G_OBJECT (gsettings_marco));
    gsettings_marco = NULL;
    if (gsettings_gnome_wm)
	g_object_unref (G_OBJECT (gsettings_gnome_wm));
    gsettings_gnome_wm = NULL;
    if (gsettings_metacity)
	g_object_unref (G_OBJECT (gsettings_metacity));
    gsettings_metacity = NULL;

#ifdef USE_COMPIZCONFIG
    /* prioritise GNOME settings in general and MATE settings in, well, MATE */
    gsettings_mouse = new_gsettings0 (GSCHEMA_KEY_MATE_MOUSE);
    gsettings_gnome = new_gsettings0 (GSCHEMA_KEY_GNOME_INTERFACE);

    if (!gsettings_mouse || !is_mate_desktop)
    {
	if (gsettings_mouse)
	    g_object_unref (G_OBJECT (gsettings_mouse));
	gsettings_mouse = G_SETTINGS (g_object_ref (G_OBJECT (gsettings_gnome)));
    }
    if (gsettings_gnome)
	g_object_unref (G_OBJECT (gsettings_gnome));
    gsettings_gnome = NULL;
#endif

    /* theme_changed() needs both thus sending second object as data */
    if (gsettings_gwd)
    {
	g_object_set_data (G_OBJECT (gsettings_gwd), "gsettings_gwd", gsettings_gwd);
	g_object_set_data (G_OBJECT (gsettings_gwd), "gsettings_wm", gsettings_wm);
	g_object_set_data (G_OBJECT (gsettings_gwd), "gsettings_wm_theme", gsettings_wm_theme);
#ifdef USE_COMPIZCONFIG
	g_object_set_data (G_OBJECT (gsettings_gwd), "gsettings_mouse", gsettings_mouse);
	g_object_set_data (G_OBJECT (gsettings_gwd), "ccs_context", ccs_context);
#endif
	g_signal_connect_data (gsettings_gwd,
			       "changed",
			       G_CALLBACK (gsettings_value_changed),
			       (gpointer) screen,
			       0, 0);
    }

    if (gsettings_wm)
    {
	g_object_set_data (G_OBJECT (gsettings_wm), "gsettings_gwd", gsettings_gwd);
	g_object_set_data (G_OBJECT (gsettings_wm), "gsettings_wm", gsettings_wm);
	g_object_set_data (G_OBJECT (gsettings_wm), "gsettings_wm_theme", gsettings_wm_theme);
#ifdef USE_COMPIZCONFIG
	g_object_set_data (G_OBJECT (gsettings_wm), "gsettings_mouse", gsettings_mouse);
	g_object_set_data (G_OBJECT (gsettings_wm), "ccs_context", ccs_context);
#endif
	g_signal_connect_data (gsettings_wm,
			       "changed",
			       G_CALLBACK (gsettings_value_changed),
			       (gpointer) screen,
			       0, 0);
    }

    if (gsettings_wm_theme)
    {
	g_object_set_data (G_OBJECT (gsettings_wm_theme), "gsettings_gwd", gsettings_gwd);
	g_object_set_data (G_OBJECT (gsettings_wm_theme), "gsettings_wm", gsettings_wm);
	g_object_set_data (G_OBJECT (gsettings_wm_theme), "gsettings_wm_theme", gsettings_wm_theme);
#ifdef USE_COMPIZCONFIG
	g_object_set_data (G_OBJECT (gsettings_wm_theme), "gsettings_mouse", gsettings_mouse);
	g_object_set_data (G_OBJECT (gsettings_wm_theme), "ccs_context", ccs_context);
#endif
	g_signal_connect_data (gsettings_wm_theme,
			       "changed",
			       G_CALLBACK (gsettings_value_changed),
			       (gpointer) screen,
			       0, 0);
    }

#ifdef USE_COMPIZCONFIG
    if (gsettings_mouse)
    {
	g_object_set_data (G_OBJECT (gsettings_mouse), "gsettings_gwd", gsettings_gwd);
	g_object_set_data (G_OBJECT (gsettings_mouse), "gsettings_wm", gsettings_wm);
	g_object_set_data (G_OBJECT (gsettings_mouse), "gsettings_wm_theme", gsettings_wm_theme);
	g_object_set_data (G_OBJECT (gsettings_mouse), "gsettings_mouse", gsettings_mouse);
	g_object_set_data (G_OBJECT (gsettings_mouse), "ccs_context", ccs_context);
	g_signal_connect_data (gsettings_mouse,
			       "changed",
			       G_CALLBACK (gsettings_value_changed),
			       (gpointer) screen,
			       0, 0);
    }

    cursor_theme_changed(gsettings_mouse, ccs_context);
#endif

#endif

    style_window = gtk_window_new (GTK_WINDOW_POPUP);

    gdkscreen = gdk_screen_get_default ();
#if GTK_CHECK_VERSION (3, 0, 0)
    visual = gdk_screen_get_system_visual (gdkscreen);
    if (visual)
        gtk_widget_set_visual (style_window, visual);
#else
    colormap = gdk_screen_get_rgba_colormap (gdkscreen);
    if (colormap)
	gtk_widget_set_colormap (style_window, colormap);
#endif

    gtk_widget_realize (style_window);

    switcher_label = gtk_label_new ("");
    switcher_label_obj = gtk_widget_get_accessible (switcher_label);
    atk_object_set_role (switcher_label_obj, ATK_ROLE_STATUSBAR);
    gtk_container_add (GTK_CONTAINER (style_window), switcher_label);

    gtk_widget_set_size_request (style_window, 0, 0);
    gtk_window_move (GTK_WINDOW (style_window), -100, -100);

    pango_context = gtk_widget_create_pango_context (style_window);

#if GTK_CHECK_VERSION (3, 0, 0)
    g_signal_connect_data (style_window, "style-updated",
			   G_CALLBACK (style_changed),
			   (gpointer) pango_context, 0, 0);
#else
    g_signal_connect_data (style_window, "style-set",
			   G_CALLBACK (style_changed),
			   (gpointer) pango_context, 0, 0);
#endif

    settings = gtk_widget_get_settings (style_window);

    g_object_get (G_OBJECT (settings), "gtk-double-click-time",
		  &double_click_timeout, NULL);

#ifdef USE_GSETTINGS
    use_system_font = g_settings_get_boolean (gsettings_wm,
					      META_USE_SYSTEM_FONT_KEY);
    theme_changed (gsettings_gwd, gsettings_wm_theme);
    theme_opacity_changed (gsettings_gwd);
    button_layout_changed (gsettings_wm);
#endif

    update_style (style_window);

#ifdef USE_GSETTINGS
    titlebar_font_changed (gsettings_wm);
#endif

    update_titlebar_font ();

#ifdef USE_COMPIZCONFIG
    shadow_settings_changed (ccs_context);
#endif

#ifdef USE_GSETTINGS
    titlebar_click_action_changed (gsettings_wm,
				   META_DOUBLE_CLICK_TITLEBAR_KEY,
				   &double_click_action,
				   DOUBLE_CLICK_ACTION_DEFAULT);
    titlebar_click_action_changed (gsettings_wm,
				   META_MIDDLE_CLICK_TITLEBAR_KEY,
				   &middle_click_action,
				   MIDDLE_CLICK_ACTION_DEFAULT);
    titlebar_click_action_changed (gsettings_wm,
				   META_RIGHT_CLICK_TITLEBAR_KEY,
				   &right_click_action,
				   RIGHT_CLICK_ACTION_DEFAULT);
    wheel_action_changed (gsettings_gwd);
    blur_settings_changed (gsettings_gwd);
#endif

    (*theme_update_border_extents) (text_height);

    update_shadow ();

    return TRUE;
}

int
main (int argc, char *argv[])
{
    GdkDisplay *gdkdisplay;
    Display    *xdisplay;
    GdkScreen  *gdkscreen;
    WnckScreen *screen;
    gint       i, j, status;
    gboolean   replace = FALSE;

#ifdef USE_MARCO
    char       *meta_theme = NULL;
    char       *meta_button_layout_string = "menu:minimize,maximize,close";
#endif

    program_name = argv[0];

    gtk_init (&argc, &argv);

    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    for (i = 0; i < argc; i++)
    {
	if (strcmp (argv[i], "--minimal") == 0)
	{
	    minimal = TRUE;
	}
	else if (strcmp (argv[i], "--replace") == 0)
	{
	    replace = TRUE;
	}
	else if (strcmp (argv[i], "--blur") == 0)
	{
	    if (argc > ++i)
	    {
		if (strcmp (argv[i], "titlebar") == 0)
		    blur_type = BLUR_TYPE_TITLEBAR;
		else if (strcmp (argv[i], "all") == 0)
		    blur_type = BLUR_TYPE_ALL;
	    }
	    cmdline_options |= CMDLINE_BLUR;
	}
	else if (strcmp (argv[i], "--titlebar-font") == 0)
	{
	    if (argc > ++i)
		titlebar_font = pango_font_description_from_string (argv[i]);
	}

#ifdef USE_MARCO
	else if (strcmp (argv[i], "--opacity") == 0)
	{
	    if (argc > ++i)
		meta_opacity = atof (argv[i]);
	    cmdline_options |= CMDLINE_OPACITY;
	}
	else if (strcmp (argv[i], "--no-opacity-shade") == 0)
	{
	    meta_shade_opacity = FALSE;
	    cmdline_options |= CMDLINE_OPACITY_SHADE;
	}
	else if (strcmp (argv[i], "--active-opacity") == 0)
	{
	    if (argc > ++i)
		meta_active_opacity = atof (argv[i]);
	    cmdline_options |= CMDLINE_ACTIVE_OPACITY;
	}
	else if (strcmp (argv[i], "--no-active-opacity-shade") == 0)
	{
	    meta_active_shade_opacity = FALSE;
	    cmdline_options |= CMDLINE_ACTIVE_OPACITY_SHADE;
	}
	else if (strcmp (argv[i], "--metacity-theme") == 0 || strcmp (argv[i], "--marco-theme") == 0)
	{
	    if (argc > ++i)
		meta_theme = argv[i];
	    cmdline_options |= CMDLINE_THEME;
	}
	else if (strcmp (argv[i], "--button-layout") == 0)
	{
	    if (argc > ++i)
		meta_button_layout_string = argv[i];
	}
#endif

	else if (strcmp (argv[i], "--help") == 0)
	{
	    fprintf (stderr, "%s "
		     "[--minimal] "
		     "[--replace] "
		     "[--blur none|titlebar|all] "
		     "[--titlebar-font FONT] "

#ifdef USE_MARCO
		     "[--opacity OPACITY] "
		     "[--no-opacity-shade] "
		     "[--active-opacity OPACITY] "
		     "[--no-active-opacity-shade] "
		     "[--marco-theme THEME] "
		     "[--button-layout LAYOUT] "
#endif
		     "[--help]"

		     "\n", program_name);
	    return 0;
	}
    }

    theme_draw_window_decoration    = draw_window_decoration;
    theme_calc_decoration_size	    = calc_decoration_size;
    theme_update_border_extents	    = update_border_extents;
    theme_get_event_window_position = get_event_window_position;
    theme_get_button_position       = get_button_position;

#ifdef USE_MARCO
    if (meta_theme)
    {
	meta_theme_set_current (meta_theme, TRUE);
	if (meta_theme_get_current ())
	{
	    theme_draw_window_decoration    = meta_draw_window_decoration;
	    theme_calc_decoration_size	    = meta_calc_decoration_size;
	    theme_update_border_extents	    = meta_update_border_extents;
	    theme_get_event_window_position = meta_get_event_window_position;
	    theme_get_button_position	    = meta_get_button_position;
	    meta_update_button_layout(meta_button_layout_string);
	    meta_button_layout_set = TRUE;
	}
    }
#endif

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = gdk_x11_display_get_xdisplay (gdkdisplay);
    gdkscreen  = gdk_display_get_default_screen (gdkdisplay);

#if GTK_CHECK_VERSION(3, 10, 0)
    /* We need to be able to fully trust that the window and monitor sizes
     * that GDK reports corresponds to the X ones, so we disable the automatic
     * scale handling */
    gdk_x11_display_set_window_scale(gdkdisplay, 1);
#endif

    frame_window_atom	= XInternAtom (xdisplay, DECOR_INPUT_FRAME_ATOM_NAME, FALSE);
    win_decor_atom	= XInternAtom (xdisplay, DECOR_WINDOW_ATOM_NAME, FALSE);
    win_blur_decor_atom	= XInternAtom (xdisplay, DECOR_BLUR_ATOM_NAME, FALSE);
    wm_move_resize_atom = XInternAtom (xdisplay, "_NET_WM_MOVERESIZE", FALSE);
    restack_window_atom = XInternAtom (xdisplay, "_NET_RESTACK_WINDOW", FALSE);
    select_window_atom	= XInternAtom (xdisplay, DECOR_SWITCH_WINDOW_ATOM_NAME,
				       FALSE);
    mwm_hints_atom	= XInternAtom (xdisplay, "_MOTIF_WM_HINTS", FALSE);
    switcher_fg_atom    = XInternAtom (xdisplay,
				       DECOR_SWITCH_FOREGROUND_COLOR_ATOM_NAME,
				       FALSE);

    toolkit_action_atom			  =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION", FALSE);
    toolkit_action_window_menu_atom	  =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_WINDOW_MENU", FALSE);
    toolkit_action_force_quit_dialog_atom =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_FORCE_QUIT_DIALOG",
		     FALSE);

    status = decor_acquire_dm_session (xdisplay,
				       gdk_screen_get_number (gdkscreen),
				       "gwd", replace, &dm_sn_timestamp);
    if (status != DECOR_ACQUIRE_STATUS_SUCCESS)
    {
	if (status == DECOR_ACQUIRE_STATUS_FAILED)
	{
	    fprintf (stderr,
		     "%s: Could not acquire decoration manager "
		     "selection on screen %d display \"%s\"\n",
		     program_name, gdk_screen_get_number (gdkscreen),
		     DisplayString (xdisplay));
	}
	else if (status == DECOR_ACQUIRE_STATUS_OTHER_DM_RUNNING)
	{
	    fprintf (stderr,
		     "%s: Screen %d on display \"%s\" already "
		     "has a decoration manager; try using the "
		     "--replace option to replace the current "
		     "decoration manager.\n",
		     program_name, gdk_screen_get_number (gdkscreen),
		     DisplayString (xdisplay));
	}

	return 1;
    }

    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
	{
	    if (cursor[i][j].shape != XC_left_ptr)
		cursor[i][j].cursor =
		    XCreateFontCursor (xdisplay, cursor[i][j].shape);
	}
    }

    xformat = XRenderFindStandardFormat (xdisplay, PictStandardARGB32);

    frame_table = g_hash_table_new (NULL, NULL);

    if (!create_tooltip_window ())
    {
	fprintf (stderr, "%s, Couldn't create tooltip window\n", argv[0]);
	return 1;
    }

    screen = wnck_screen_get_default ();
    wnck_set_client_type (WNCK_CLIENT_TYPE_PAGER);

    gdk_window_add_filter (NULL,
			   selection_event_filter_func,
			   NULL);

    if (!minimal)
    {
	gdk_window_add_filter (NULL,
			       event_filter_func,
			       NULL);

	connect_screen (screen);
    }

    if (!init_settings (screen))
    {
	fprintf (stderr, "%s: Failed to get necessary gtk settings\n", argv[0]);
	return 1;
    }

    decor_set_dm_check_hint (xdisplay, gdk_screen_get_number (gdkscreen),
			     WINDOW_DECORATION_TYPE_PIXMAP);

    update_default_decorations (gdkscreen);

    gtk_main ();

    return 0;
}
