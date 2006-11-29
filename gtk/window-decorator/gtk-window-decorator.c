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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "decoration.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xregion.h>

#ifndef GTK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#endif

#include <gtk/gtk.h>
#include <gtk/gtkwindow.h>
#include <gdk/gdkx.h>

#include <gconf/gconf-client.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>
#include <libwnck/window-action-menu.h>

#include <cairo.h>
#include <cairo-xlib.h>
#include <cairo-xlib-xrender.h>

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

#ifdef USE_METACITY
#include <metacity-private/theme.h>
#endif

#define METACITY_GCONF_DIR "/apps/metacity/general"

#define COMPIZ_USE_SYSTEM_FONT_KEY		    \
    METACITY_GCONF_DIR "/titlebar_uses_system_font"

#define COMPIZ_TITLEBAR_FONT_KEY	\
    METACITY_GCONF_DIR "/titlebar_font"

#define COMPIZ_DOUBLE_CLICK_TITLEBAR_KEY	       \
    METACITY_GCONF_DIR "/action_double_click_titlebar"

#define COMPIZ_GCONF_DIR1 "/apps/compiz/plugins/decoration/allscreens/options"

#define COMPIZ_SHADOW_RADIUS_KEY \
    COMPIZ_GCONF_DIR1 "/shadow_radius"

#define COMPIZ_SHADOW_OPACITY_KEY \
    COMPIZ_GCONF_DIR1 "/shadow_opacity"

#define COMPIZ_SHADOW_COLOR_KEY \
    COMPIZ_GCONF_DIR1 "/shadow_color"

#define COMPIZ_SHADOW_OFFSET_X_KEY \
    COMPIZ_GCONF_DIR1 "/shadow_offset_x"

#define COMPIZ_SHADOW_OFFSET_Y_KEY \
    COMPIZ_GCONF_DIR1 "/shadow_offset_y"

#define META_AUDIBLE_BELL_KEY	       \
    METACITY_GCONF_DIR "/audible_bell"

#define META_VISUAL_BELL_KEY	      \
    METACITY_GCONF_DIR "/visual_bell"

#define META_VISUAL_BELL_TYPE_KEY	   \
    METACITY_GCONF_DIR "/visual_bell_type"

#define META_THEME_KEY		\
    METACITY_GCONF_DIR "/theme"

#define COMPIZ_GCONF_DIR2 "/apps/compiz/general/allscreens/options"

#define COMPIZ_AUDIBLE_BELL_KEY	      \
    COMPIZ_GCONF_DIR2 "/audible_bell"

#define COMPIZ_GCONF_DIR3 "/apps/compiz/plugins/fade/screen0/options"

#define COMPIZ_VISUAL_BELL_KEY	     \
    COMPIZ_GCONF_DIR3 "/visual_bell"

#define COMPIZ_FULLSCREEN_VISUAL_BELL_KEY	\
    COMPIZ_GCONF_DIR3 "/fullscreen_visual_bell"

#define GCONF_DIR "/apps/gwd"

#define USE_META_THEME_KEY	    \
    GCONF_DIR "/use_metacity_theme"

#define META_THEME_OPACITY_KEY	        \
    GCONF_DIR "/metacity_theme_opacity"

#define META_THEME_SHADE_OPACITY_KEY	      \
    GCONF_DIR "/metacity_theme_shade_opacity"

#define META_THEME_ACTIVE_OPACITY_KEY	       \
    GCONF_DIR "/metacity_theme_active_opacity"

#define META_THEME_ACTIVE_SHADE_OPACITY_KEY          \
    GCONF_DIR "/metacity_theme_active_shade_opacity"


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

#define META_OPACITY              0.75
#define META_SHADE_OPACITY        TRUE
#define META_ACTIVE_OPACITY       1.0
#define META_ACTIVE_SHADE_OPACITY TRUE

#define N_QUADS_MAX 24

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
    DOUBLE_CLICK_SHADE,
    DOUBLE_CLICK_MAXIMIZE
};

int double_click_action = DOUBLE_CLICK_SHADE;

static gboolean minimal = FALSE;

static double decoration_alpha = 0.5;

static decor_extents_t _shadow_extents   = { 0, 0, 0, 0 };
static decor_extents_t _win_extents      = { 6, 6, 4, 6 };
static decor_extents_t _switcher_extents = { 0, 0, 0, 0 };

#define SWITCHER_SPACE     40
#define SWITCHER_TOP_EXTRA 4

static int titlebar_height = 17;

static decor_context_t window_context = {
    { 0, 0, 0, 0 },
    6, 6, 4, 6,
    0, 0, 0, 0
};

static decor_context_t shadow_context = {
    { 0, 0, 0, 0 },
    0, 0, 0, 0,
    0, 0, 0, 0,
};

static gdouble shadow_radius   = SHADOW_RADIUS;
static gdouble shadow_opacity  = SHADOW_OPACITY;
static gint    shadow_color[3] = {
    SHADOW_COLOR_RED,
    SHADOW_COLOR_GREEN,
    SHADOW_COLOR_BLUE
};
static gint    shadow_offset_x = SHADOW_OFFSET_X;
static gint    shadow_offset_y = SHADOW_OFFSET_Y;

#ifdef USE_METACITY
static double   meta_opacity              = META_OPACITY;
static gboolean meta_shade_opacity        = META_SHADE_OPACITY;
static double   meta_active_opacity       = META_ACTIVE_OPACITY;
static gboolean meta_active_shade_opacity = META_ACTIVE_SHADE_OPACITY;
#endif

static decor_shadow_t *shadow = NULL;
static decor_shadow_t *border_shadow = NULL;

static GdkPixmap *decor_normal_pixmap = NULL;
static GdkPixmap *decor_active_pixmap = NULL;

static cairo_pattern_t *shadow_pattern = NULL;

static Atom frame_window_atom;
static Atom win_decor_atom;
static Atom wm_move_resize_atom;
static Atom restack_window_atom;
static Atom select_window_atom;
static Atom mwm_hints_atom;

static Atom toolkit_action_atom;
static Atom toolkit_action_main_menu_atom;
static Atom toolkit_action_run_dialog_atom;
static Atom toolkit_action_window_menu_atom;
static Atom toolkit_action_force_quit_dialog_atom;

static Atom panel_action_atom;
static Atom panel_action_main_menu_atom;
static Atom panel_action_run_dialog_atom;

static Atom manager_atom;
static Atom targets_atom;
static Atom multiple_atom;
static Atom timestamp_atom;
static Atom version_atom;
static Atom atom_pair_atom;

static Atom utf8_string_atom;

static Atom dm_name_atom;
static Atom dm_sn_atom;

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
}, bpos[3] = {
    { 0, 6, 16, 16,   1, 0, 0, 0, 0, 0 },
    { 0, 6, 16, 16,   1, 0, 0, 0, 0, 0 },
    { 0, 6, 16, 16,   1, 0, 0, 0, 0, 0 }
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
    Window	      button_windows[3];
    guint	      button_states[3];
    GdkPixmap	      *pixmap;
    GdkPixmap	      *buffer_pixmap;
    GdkGC	      *gc;
    gint	      button_width;
    gint	      width;
    gint	      height;
    gboolean	      decorated;
    gboolean	      active;
    PangoLayout	      *layout;
    gchar	      *name;
    cairo_pattern_t   *icon;
    GdkPixmap	      *icon_pixmap;
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
gint     (*theme_calc_titlebar_height)      (gint    text_height);
void     (*theme_get_event_window_position) (decor_t *d,
					     gint    i,
					     gint    j,
					     gint    width,
					     gint    height,
					     gint    *x,
					     gint    *y,
					     gint    *w,
					     gint    *h);
void     (*theme_get_button_position)       (decor_t *d,
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

static GHashTable    *frame_table;
static GtkWidget     *action_menu = NULL;
static gboolean      action_menu_mapped = FALSE;
static decor_color_t _title_color[2];
static PangoContext  *pango_context;
static gint	     double_click_timeout = 250;

static GtkWidget     *tip_window;
static GtkWidget     *tip_label;
static GTimeVal	     tooltip_last_popdown = { -1, -1 };
static gint	     tooltip_timer_tag = 0;

static GSList *draw_list = NULL;
static guint  draw_idle_id = 0;

static PangoFontDescription *titlebar_font = NULL;
static gboolean		    use_system_font = FALSE;
static gint		    text_height;

static GdkPixmap *switcher_pixmap = NULL;
static GdkPixmap *switcher_buffer_pixmap = NULL;
static gint      switcher_width;
static gint      switcher_height;

#define BASE_PROP_SIZE 12
#define QUAD_PROP_SIZE 9

static void
decor_update_window_property (decor_t *d)
{
    long	    data[256];
    Display	    *xdisplay =
	GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    decor_extents_t extents = _win_extents;
    gint	    nQuad;
    decor_quad_t    quads[N_QUADS_MAX];

    nQuad = decor_set_window_quads (&window_context, quads,
				    d->width, d->height, d->button_width);

    extents.top += titlebar_height;

    decor_quads_to_property (data, GDK_PIXMAP_XID (d->pixmap),
			     &extents, &extents,
			     ICON_SPACE + d->button_width,
			     0,
			     quads, nQuad);

    gdk_error_trap_push ();
    XChangeProperty (xdisplay, d->prop_xid,
		     win_decor_atom,
		     XA_INTEGER,
		     32, PropModeReplace, (guchar *) data,
		     BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
		     XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();
}

static int
set_switcher_quads (decor_quad_t *q,
		    int		 width,
		    int		 height)
{
    gint n, nQuad = 0;

    /* 1 top quads */
    q->p1.x	  = -window_context.left_space;
    q->p1.y	  = -window_context.top_space - SWITCHER_TOP_EXTRA;
    q->p1.gravity = GRAVITY_NORTH | GRAVITY_WEST;
    q->p2.x	  = window_context.right_space;
    q->p2.y	  = 0;
    q->p2.gravity = GRAVITY_NORTH | GRAVITY_EAST;
    q->max_width  = SHRT_MAX;
    q->max_height = SHRT_MAX;
    q->align	  = 0;
    q->clamp	  = 0;
    q->m.xx	  = 1.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = 0.0;
    q->m.y0	  = 0.0;

    q++; nQuad++;

    /* left quads */
    n = decor_set_vert_quad_row (q,
				 0,
				 window_context.top_corner_space,
				 0,
				 window_context.bottom_corner_space,
				 -window_context.left_space,
				 0,
				 GRAVITY_WEST,
				 height - window_context.top_space -
				 window_context.bottom_space,
				 0.0,
				 window_context.top_space + SWITCHER_TOP_EXTRA);

    q += n; nQuad += n;

    /* right quads */
    n = decor_set_vert_quad_row (q,
				 0,
				 window_context.top_corner_space,
				 0,
				 window_context.bottom_corner_space,
				 0,
				 window_context.right_space,
				 GRAVITY_EAST,
				 height - window_context.top_space -
				 window_context.bottom_space,
				 width - window_context.right_space,
				 window_context.top_space + SWITCHER_TOP_EXTRA);

    q += n; nQuad += n;

    /* 1 bottom quad */
    q->p1.x	  = -window_context.left_space;
    q->p1.y	  = 0;
    q->p1.gravity = GRAVITY_SOUTH | GRAVITY_WEST;
    q->p2.x	  = window_context.right_space;
    q->p2.y	  = window_context.bottom_space + SWITCHER_SPACE;
    q->p2.gravity = GRAVITY_SOUTH | GRAVITY_EAST;
    q->max_width  = SHRT_MAX;
    q->max_height = SHRT_MAX;
    q->align	  = 0;
    q->clamp	  = 0;
    q->m.xx	  = 1.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = 0.0;
    q->m.y0	  = height - window_context.bottom_space - SWITCHER_SPACE;

    nQuad++;

    return nQuad;
}

static void
decor_update_switcher_property (decor_t *d)
{
    long    data[256];
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    gint    nQuad;
    decor_quad_t    quads[N_QUADS_MAX];
    decor_extents_t extents = _switcher_extents;

    nQuad = set_switcher_quads (quads, d->width, d->height);

    decor_quads_to_property (data, GDK_PIXMAP_XID (d->pixmap),
			     &extents, &extents, 0, 0, quads, nQuad);

    gdk_error_trap_push ();
    XChangeProperty (xdisplay, d->prop_xid,
		     win_decor_atom,
		     XA_INTEGER,
		     32, PropModeReplace, (guchar *) data,
		     BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
    XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();
}

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

static GdkPixmap *
create_pixmap (int w,
	       int h)
{
    GdkPixmap	*pixmap;
    GdkVisual	*visual;
    GdkColormap *colormap;

    visual = gdk_visual_get_best_with_depth (32);
    if (!visual)
	return NULL;

    pixmap = gdk_pixmap_new (NULL, w, h, 32);
    if (!pixmap)
	return NULL;

    colormap = gdk_colormap_new (visual, FALSE);
    if (!colormap)
    {
	gdk_pixmap_unref (pixmap);
	return NULL;
    }

    gdk_drawable_set_colormap (GDK_DRAWABLE (pixmap), colormap);
    gdk_colormap_unref (colormap);

    return pixmap;
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
draw_shadow_background (decor_t *d,
			cairo_t	*cr)
{
    cairo_matrix_t matrix;
    double	   w, h, x2, y2;
    gint	   width, height;
    gint	   left, right, top, bottom;

    if (!border_shadow)
    {
	cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
	cairo_paint (cr);

	return;
    }

    width  = border_shadow->width;
    height = border_shadow->height;

    left   = window_context.left_space   + window_context.left_corner_space;
    right  = window_context.right_space  + window_context.right_corner_space;
    top    = window_context.top_space    + window_context.top_corner_space;
    bottom = window_context.bottom_space + window_context.bottom_corner_space;

    if (d->width - left - right < 0)
    {
	left = d->width / 2;
	right = d->width - left;
    }

    if (d->height - top - bottom < 0)
    {
	top = d->height / 2;
	bottom = d->height - top;
    }

    w = d->width - left - right;
    h = d->height - top - bottom;

    x2 = d->width - right;
    y2 = d->height - bottom;

    /* top left */
    cairo_matrix_init_identity (&matrix);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, 0.0, 0.0, left, top);
    cairo_fill (cr);

    /* top */
    if (w > 0)
    {
	cairo_matrix_init_translate (&matrix, left, 0.0);
	cairo_matrix_scale (&matrix, 1.0 / w, 1.0);
	cairo_matrix_translate (&matrix, -left, 0.0);
	cairo_pattern_set_matrix (shadow_pattern, &matrix);
	cairo_set_source (cr, shadow_pattern);
	cairo_rectangle (cr, left, 0.0, w, top);
	cairo_fill (cr);
    }

    /* top right */
    cairo_matrix_init_translate (&matrix, width - right - x2, 0.0);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, x2, 0.0, right, top);
    cairo_fill (cr);

    /* left */
    if (h > 0)
    {
	cairo_matrix_init_translate (&matrix, 0.0, top);
	cairo_matrix_scale (&matrix, 1.0, 1.0 / h);
	cairo_matrix_translate (&matrix, 0.0, -top);
	cairo_pattern_set_matrix (shadow_pattern, &matrix);
	cairo_set_source (cr, shadow_pattern);
	cairo_rectangle (cr, 0.0, top, left, h);
	cairo_fill (cr);
    }

    /* right */
    if (h > 0)
    {
	cairo_matrix_init_translate (&matrix, width - right - x2, top);
	cairo_matrix_scale (&matrix, 1.0, 1.0 / h);
	cairo_matrix_translate (&matrix, 0.0, -top);
	cairo_pattern_set_matrix (shadow_pattern, &matrix);
	cairo_set_source (cr, shadow_pattern);
	cairo_rectangle (cr, x2, top, right, h);
	cairo_fill (cr);
    }

    /* bottom left */
    cairo_matrix_init_translate (&matrix, 0.0, height - bottom - y2);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, 0.0, y2, left, bottom);
    cairo_fill (cr);

    /* bottom */
    if (w > 0)
    {
	cairo_matrix_init_translate (&matrix, left,
				     height - bottom - y2);
	cairo_matrix_scale (&matrix, 1.0 / w, 1.0);
	cairo_matrix_translate (&matrix, -left, 0.0);
	cairo_pattern_set_matrix (shadow_pattern, &matrix);
	cairo_set_source (cr, shadow_pattern);
	cairo_rectangle (cr, left, y2, w, bottom);
	cairo_fill (cr);
    }

    /* bottom right */
    cairo_matrix_init_translate (&matrix, width - right - x2,
				 height - bottom - y2);
    cairo_pattern_set_matrix (shadow_pattern, &matrix);
    cairo_set_source (cr, shadow_pattern);
    cairo_rectangle (cr, x2, y2, right, bottom);
    cairo_fill (cr);
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
button_state_paint (cairo_t	  *cr,
		    GtkStyle	  *style,
		    decor_color_t *color,
		    guint	  state)
{

#define IN_STATE (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW)

    if ((state & IN_STATE) == IN_STATE)
    {
	if (state & IN_EVENT_WINDOW)
	    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	else
	    cairo_set_source_rgba (cr, color->r, color->g, color->b, 0.95);

	cairo_fill_preserve (cr);

	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  STROKE_ALPHA);

	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);
	cairo_set_line_width (cr, 2.0);
    }
    else
    {
	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  STROKE_ALPHA);
	cairo_stroke_preserve (cr);

	if (state & IN_EVENT_WINDOW)
	    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	else
	    cairo_set_source_rgba (cr, color->r, color->g, color->b, 0.95);

	cairo_fill (cr);
    }
}

static void
draw_window_decoration (decor_t *d)
{
    cairo_t       *cr;
    GtkStyle	  *style;
    decor_color_t color;
    double        alpha;
    double        x1, y1, x2, y2, x, y, h;
    int		  corners = SHADE_LEFT | SHADE_RIGHT | SHADE_TOP | SHADE_BOTTOM;
    int		  top;
    int		  button_x;

    if (!d->pixmap)
	return;

    style = gtk_widget_get_style (style_window);

    if (d->state & (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
		    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
	corners = 0;

    color.r = style->bg[GTK_STATE_NORMAL].red   / 65535.0;
    color.g = style->bg[GTK_STATE_NORMAL].green / 65535.0;
    color.b = style->bg[GTK_STATE_NORMAL].blue  / 65535.0;

    if (d->buffer_pixmap)
	cr = gdk_cairo_create (GDK_DRAWABLE (d->buffer_pixmap));
    else
	cr = gdk_cairo_create (GDK_DRAWABLE (d->pixmap));

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    top = _win_extents.top + titlebar_height;

    x1 = window_context.left_space - _win_extents.left;
    y1 = window_context.top_space - _win_extents.top - titlebar_height;
    x2 = d->width - window_context.right_space + _win_extents.right;
    y2 = d->height - window_context.bottom_space + _win_extents.bottom;

    h = d->height - window_context.top_space - window_context.bottom_space;

    cairo_set_line_width (cr, 1.0);

    draw_shadow_background (d, cr);

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
		     window_context.left_space,
		     window_context.top_space,
		     d->width - window_context.left_space -
		     window_context.right_space,
		     h);
    gdk_cairo_set_source_color (cr, &style->bg[GTK_STATE_NORMAL]);
    cairo_fill (cr);

    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    if (d->active)
    {
	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  0.7);

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

    gdk_cairo_set_source_color_alpha (cr,
				      &style->fg[GTK_STATE_NORMAL],
				      alpha);

    cairo_stroke (cr);

    cairo_set_line_width (cr, 2.0);

    button_x = d->width - window_context.right_space - 13;

    if (d->actions & WNCK_WINDOW_ACTION_CLOSE)
    {
	button_state_offsets (button_x,
			      y1 - 3.0 + titlebar_height / 2,
			      d->button_states[0], &x, &y);

	button_x -= 17;

	if (d->active)
	{
	    cairo_move_to (cr, x, y);
	    draw_close_button (d, cr, 3.0);
	    button_state_paint (cr, style, &color, d->button_states[0]);
	}
	else
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha * 0.75);
	    cairo_move_to (cr, x, y);
	    draw_close_button (d, cr, 3.0);
	    cairo_fill (cr);
	}
    }

    if (d->actions & WNCK_WINDOW_ACTION_MAXIMIZE)
    {
	button_state_offsets (button_x,
			      y1 - 3.0 + titlebar_height / 2,
			      d->button_states[1], &x, &y);

	button_x -= 17;

	cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);

	if (d->active)
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      STROKE_ALPHA);
	    cairo_move_to (cr, x, y);

	    if (d->state & (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
			    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
		draw_unmax_button (d, cr, 4.0);
	    else
		draw_max_button (d, cr, 4.0);

	    button_state_paint (cr, style, &color, d->button_states[1]);
	}
	else
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha * 0.75);
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
			      d->button_states[2], &x, &y);

	button_x -= 17;

	if (d->active)
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      STROKE_ALPHA);
	    cairo_move_to (cr, x, y);
	    draw_min_button (d, cr, 4.0);
	    button_state_paint (cr, style, &color, d->button_states[2]);
	}
	else
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha * 0.75);
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
			   window_context.left_space + 21.0,
			   y1 + 2.0 + (titlebar_height - text_height) / 2.0);

	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      STROKE_ALPHA);

	    pango_cairo_layout_path (cr, d->layout);
	    cairo_stroke (cr);

	    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	}
	else
	{
	    gdk_cairo_set_source_color_alpha (cr,
					      &style->fg[GTK_STATE_NORMAL],
					      alpha);
	}

	cairo_move_to (cr,
		       window_context.left_space + 21.0,
		       y1 + 2.0 + (titlebar_height - text_height) / 2.0);

	pango_cairo_show_layout (cr, d->layout);
    }

    if (d->icon)
    {
	cairo_translate (cr, window_context.left_space + 1,
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

    if (d->buffer_pixmap)
	gdk_draw_drawable  (d->pixmap,
			    d->gc,
			    d->buffer_pixmap,
			    0,
			    0,
			    0,
			    0,
			    d->width,
			    d->height);

    if (d->prop_xid)
    {
	decor_update_window_property (d);
	d->prop_xid = 0;
    }
}

#ifdef USE_METACITY
static void
decor_update_meta_window_property (decor_t	  *d,
				   MetaTheme	  *theme,
				   MetaFrameFlags flags)
{
    long	    data[256];
    Display	    *xdisplay =
	GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    decor_extents_t extents, max_extents;
    gint	    nQuad;
    decor_quad_t    quads[N_QUADS_MAX];
    gint	    left_width, right_width, top_height, bottom_height;

    nQuad = decor_set_window_quads (&window_context, quads,
				    d->width, d->height, d->button_width);

    meta_theme_get_frame_borders (theme,
				  META_FRAME_TYPE_NORMAL,
				  text_height,
				  flags & ~META_FRAME_MAXIMIZED,
				  &top_height,
				  &bottom_height,
				  &left_width,
				  &right_width);

    extents.top    = top_height;
    extents.bottom = bottom_height;
    extents.left   = left_width;
    extents.right  = right_width;

    meta_theme_get_frame_borders (theme,
				  META_FRAME_TYPE_NORMAL,
				  text_height,
				  flags | META_FRAME_MAXIMIZED,
				  &top_height,
				  &bottom_height,
				  &left_width,
				  &right_width);

    max_extents.top    = top_height;
    max_extents.bottom = bottom_height;
    max_extents.left   = left_width;
    max_extents.right  = right_width;

    decor_quads_to_property (data, GDK_PIXMAP_XID (d->pixmap),
			     &extents, &max_extents,
			     ICON_SPACE + d->button_width,
			     0,
			     quads, nQuad);

    gdk_error_trap_push ();
    XChangeProperty (xdisplay, d->prop_xid,
		     win_decor_atom,
		     XA_INTEGER,
		     32, PropModeReplace, (guchar *) data,
		     BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
		     XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();
}

static void
meta_get_corner_radius (const MetaFrameGeometry *fgeom,
			int		        *top_left_radius,
			int			*top_right_radius,
			int		        *bottom_left_radius,
			int			*bottom_right_radius)
{

#ifdef HAVE_METACITY_2_17_0
    *top_left_radius     = fgeom->top_left_corner_rounded_radius;
    *top_right_radius    = fgeom->top_right_corner_rounded_radius;
    *bottom_left_radius  = fgeom->bottom_left_corner_rounded_radius;
    *bottom_right_radius = fgeom->bottom_right_corner_rounded_radius;
#else
    *top_left_radius     = fgeom->top_left_corner_rounded ? 5 : 0;
    *top_right_radius    = fgeom->top_right_corner_rounded ? 5 : 0;
    *bottom_left_radius  = fgeom->bottom_left_corner_rounded ? 5 : 0;
    *bottom_right_radius = fgeom->bottom_right_corner_rounded ? 5 : 0;
#endif

}

static int
radius_to_width (int radius,
		 int i)
{
    int r2 = radius * radius - (radius - i) * (radius - i);

    return 1 + (radius - floor (sqrt (r2) + 0.5));
}

static Region
meta_get_window_region (const MetaFrameGeometry *fgeom,
			int		        width,
			int			height)
{
    Region     corners_xregion, window_xregion;
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

    if (bottom_left_radius)
    {
	for (i = 0; i < bottom_left_radius; i++)
	{
	    w = radius_to_width (bottom_left_radius, i);

	    xrect.x	 = 0;
	    xrect.y	 = height - i;
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
	    xrect.y	 = height - i;
	    xrect.width  = w;
	    xrect.height = 1;

	    XUnionRectWithRegion (&xrect, corners_xregion, corners_xregion);
	}
    }

    window_xregion = XCreateRegion ();

    xrect.x = 0;
    xrect.y = 0;
    xrect.width = width;
    xrect.height = height;

    XUnionRectWithRegion (&xrect, window_xregion, window_xregion);

    XSubtractRegion (window_xregion, corners_xregion, window_xregion);

    XDestroyRegion (corners_xregion);

    return window_xregion;
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
static MetaButtonState
meta_button_state_for_button_type (decor_t	  *d,
				   MetaButtonType type)
{
    switch (type) {
    case META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND:
    case META_BUTTON_TYPE_MINIMIZE:
	return meta_button_state (d->button_states[2]);
    case META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND:
    case META_BUTTON_TYPE_MAXIMIZE:
	return meta_button_state (d->button_states[1]);
    case META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND:
    case META_BUTTON_TYPE_CLOSE:
	return meta_button_state (d->button_states[0]);
    case META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND:
    case META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND:
    case META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND:
    case META_BUTTON_TYPE_MENU:
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
    gint i;

    button_layout->left_buttons[0] = META_BUTTON_FUNCTION_MENU;

    for (i = 1; i < MAX_BUTTONS_PER_CORNER; i++)
	button_layout->left_buttons[i] = META_BUTTON_FUNCTION_LAST;

    button_layout->right_buttons[0] = META_BUTTON_FUNCTION_MINIMIZE;
    button_layout->right_buttons[1] = META_BUTTON_FUNCTION_MAXIMIZE;
    button_layout->right_buttons[2] = META_BUTTON_FUNCTION_CLOSE;

    for (i = 3; i < MAX_BUTTONS_PER_CORNER; i++)
	button_layout->right_buttons[i] = META_BUTTON_FUNCTION_LAST;

    *flags = 0;

    if (d->actions & WNCK_WINDOW_ACTION_CLOSE)
	*flags |= META_FRAME_ALLOWS_DELETE;

    if (d->actions & WNCK_WINDOW_ACTION_MINIMIZE)
	*flags |= META_FRAME_ALLOWS_MINIMIZE;

    if (d->actions & WNCK_WINDOW_ACTION_MAXIMIZE)
	*flags |= META_FRAME_ALLOWS_MAXIMIZE;

    *flags |= META_FRAME_ALLOWS_MENU;
    *flags |= META_FRAME_ALLOWS_VERTICAL_RESIZE;
    *flags |= META_FRAME_ALLOWS_HORIZONTAL_RESIZE;
    *flags |= META_FRAME_ALLOWS_MOVE;

    if (d->actions & WNCK_WINDOW_ACTION_MAXIMIZE)
	*flags |= META_FRAME_ALLOWS_MAXIMIZE;

    if (d->active)
	*flags |= META_FRAME_HAS_FOCUS;

#define META_MAXIMIZED (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY | \
			WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY)

    if ((d->state & META_MAXIMIZED) == META_MAXIMIZED)
	*flags |= META_FRAME_MAXIMIZED;

    meta_theme_get_frame_borders (theme,
				  META_FRAME_TYPE_NORMAL,
				  text_height,
				  *flags,
				  &top_height,
				  &bottom_height,
				  &left_width,
				  &right_width);

    clip->x = window_context.left_space - left_width;
    clip->y = window_context.top_space - top_height;
    clip->width = d->width - window_context.right_space + right_width - clip->x;
    clip->height = d->height - window_context.bottom_space + bottom_height -
	clip->y;

    meta_theme_calc_geometry (theme,
			      META_FRAME_TYPE_NORMAL,
			      text_height,
			      *flags,
			      clip->width - left_width - right_width,
			      clip->height - top_height - bottom_height,
			      button_layout,
			      fgeom);
}

static void
meta_draw_window_decoration (decor_t *d)
{
    MetaButtonState   button_states[META_BUTTON_TYPE_LAST];
    MetaButtonLayout  button_layout;
    MetaFrameGeometry fgeom;
    MetaFrameFlags    flags;
    MetaTheme	      *theme;
    GtkStyle	      *style;
    cairo_t	      *cr;
    gint	      i;
    GdkRectangle      clip, rect;
    GdkDrawable       *drawable;
    Region	      region;
    double	      alpha = (d->active) ? meta_active_opacity : meta_opacity;
    MetaFrameStyle    *frame_style;
    GdkColor	      bg_color;
    double	      bg_alpha;

    if (!d->pixmap)
	return;

    style = gtk_widget_get_style (style_window);

    drawable = d->buffer_pixmap ? d->buffer_pixmap : d->pixmap;

    cr = gdk_cairo_create (GDK_DRAWABLE (drawable));

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    draw_shadow_background (d, cr);

    theme = meta_theme_get_current ();

    meta_get_decoration_geometry (d, theme, &flags, &fgeom, &button_layout,
				  &clip);

    for (i = 0; i < META_BUTTON_TYPE_LAST; i++)
	button_states[i] = meta_button_state_for_button_type (d, i);

    frame_style = meta_theme_get_frame_style (theme,
					      META_FRAME_TYPE_NORMAL,
					      flags);

    bg_color = style->bg[GTK_STATE_NORMAL];
    bg_alpha = 1.0;

#ifdef HAVE_METACITY_2_17_0
    if (frame_style->window_background_color)
    {
	meta_color_spec_render (frame_style->window_background_color,
				GTK_WIDGET (style_window),
				&bg_color);

	bg_alpha = frame_style->window_background_alpha / 255.0;
    }
#endif

    region = meta_get_window_region (&fgeom, clip.width, clip.height);

    if (alpha != 1.0)
    {
	GdkPixmap *pixmap;
	cairo_t	  *pcr;
	gboolean  shade_alpha = (d->active) ? meta_active_shade_opacity :
	    meta_shade_opacity;

	pixmap = create_pixmap (clip.width, clip.height);

	pcr = gdk_cairo_create (GDK_DRAWABLE (pixmap));

	gdk_cairo_set_source_color_alpha (pcr, &bg_color, bg_alpha);
	cairo_set_operator (pcr, CAIRO_OPERATOR_SOURCE);
	cairo_paint (pcr);

	rect.x	    = 0;
	rect.y      = 0;
	rect.width  = clip.width;
	rect.height = clip.height;

	meta_theme_draw_frame (theme,
			       style_window,
			       pixmap,
			       &rect,
			       0, 0,
			       META_FRAME_TYPE_NORMAL,
			       flags,
			       clip.width - fgeom.left_width -
			       fgeom.right_width,
			       clip.height - fgeom.top_height -
			       fgeom.bottom_height,
			       d->layout,
			       text_height,
			       &button_layout,
			       button_states,
			       d->icon_pixbuf,
			       NULL);

	cairo_save (cr);

	for (i = 0; i < region->numRects; i++)
	    cairo_rectangle (cr,
			     clip.x + region->rects[i].x1,
			     clip.y + region->rects[i].y1,
			     region->rects[i].x2 - region->rects[i].x1,
			     region->rects[i].y2 - region->rects[i].y1);

	cairo_clip (cr);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, alpha);
	cairo_paint (cr);

	if (shade_alpha)
	{
	    static decor_color_t color = { 0.0, 0.0, 0.0 };
	    int			 corners = 0;
	    int			 top_left_radius;
	    int			 top_right_radius;
	    int			 bottom_left_radius;
	    int			 bottom_right_radius;

	    meta_get_corner_radius (&fgeom,
				    &top_left_radius,
				    &top_right_radius,
				    &bottom_left_radius,
				    &bottom_right_radius);

	    if (top_left_radius)
		corners |= CORNER_TOPLEFT;

	    if (top_right_radius)
		corners |= CORNER_TOPRIGHT;

	    if (bottom_left_radius)
		corners |= CORNER_BOTTOMLEFT;

	    if (bottom_right_radius)
		corners |= CORNER_BOTTOMRIGHT;

	    if (d->state & (WNCK_WINDOW_STATE_MAXIMIZED_HORIZONTALLY |
			    WNCK_WINDOW_STATE_MAXIMIZED_VERTICALLY))
		corners = 0;

	    fill_rounded_rectangle (cr,
				    clip.x,
				    clip.y,
				    fgeom.left_width,
				    fgeom.top_height,
				    top_left_radius,
				    CORNER_TOPLEFT & corners,
				    &color, 1.0,
				    &color, alpha,
				    SHADE_TOP | SHADE_LEFT);

	    fill_rounded_rectangle (cr,
				    clip.x + fgeom.left_width,
				    clip.y,
				    clip.width - fgeom.left_width -
				    fgeom.right_width,
				    fgeom.top_height,
				    5.0, 0,
				    &color, 1.0, &color, alpha,
				    SHADE_TOP);

	    fill_rounded_rectangle (cr,
				    clip.x + clip.width - fgeom.right_width,
				    clip.y,
				    fgeom.right_width,
				    fgeom.top_height,
				    top_right_radius,
				    CORNER_TOPRIGHT & corners,
				    &color, 1.0, &color, alpha,
				    SHADE_TOP | SHADE_RIGHT);

	    fill_rounded_rectangle (cr,
				    clip.x,
				    clip.y + fgeom.top_height,
				    fgeom.left_width,
				    clip.height - fgeom.top_height -
				    fgeom.bottom_height,
				    5.0, 0,
				    &color, 1.0, &color, alpha,
				    SHADE_LEFT);

	    fill_rounded_rectangle (cr,
				    clip.x + clip.width - fgeom.right_width,
				    clip.y + fgeom.top_height,
				    fgeom.right_width,
				    clip.height - fgeom.top_height -
				    fgeom.bottom_height,
				    5.0, 0,
				    &color, 1.0, &color, alpha,
				    SHADE_RIGHT);

	    fill_rounded_rectangle (cr,
				    clip.x,
				    clip.y + clip.height - fgeom.bottom_height,
				    fgeom.left_width,
				    fgeom.bottom_height,
				    bottom_left_radius,
				    CORNER_BOTTOMLEFT & corners,
				    &color, 1.0, &color, alpha,
				    SHADE_BOTTOM | SHADE_LEFT);

	    fill_rounded_rectangle (cr,
				    clip.x + fgeom.left_width,
				    clip.y + clip.height - fgeom.bottom_height,
				    clip.width - fgeom.left_width -
				    fgeom.right_width,
				    fgeom.bottom_height,
				    5.0, 0,
				    &color, 1.0, &color, alpha,
				    SHADE_BOTTOM);

	    fill_rounded_rectangle (cr,
				    clip.x + clip.width - fgeom.right_width,
				    clip.y + clip.height - fgeom.bottom_height,
				    fgeom.right_width,
				    fgeom.bottom_height,
				    bottom_right_radius,
				    CORNER_BOTTOMRIGHT & corners,
				    &color, 1.0, &color, alpha,
				    SHADE_BOTTOM | SHADE_RIGHT);
	}

	cairo_set_operator (cr, CAIRO_OPERATOR_IN);
	cairo_set_source_surface (cr, cairo_get_target (pcr), clip.x, clip.y);
	cairo_paint (cr);

	cairo_restore (cr);

	cairo_destroy (pcr);
	gdk_pixmap_unref (pixmap);
    }
    else
    {
	gdk_cairo_set_source_color_alpha (cr, &bg_color, bg_alpha);

	for (i = 0; i < region->numRects; i++)
	{
	    rect.x	= clip.x + region->rects[i].x1;
	    rect.y	= clip.y + region->rects[i].y1;
	    rect.width  = region->rects[i].x2 - region->rects[i].x1;
	    rect.height = region->rects[i].y2 - region->rects[i].y1;

	    cairo_rectangle (cr, rect.x, rect.y, rect.width, rect.height);
	    cairo_fill (cr);

	    meta_theme_draw_frame (theme,
				   style_window,
				   drawable,
				   &rect,
				   clip.x,
				   clip.y,
				   META_FRAME_TYPE_NORMAL,
				   flags,
				   clip.width - fgeom.left_width -
				   fgeom.right_width,
				   clip.height - fgeom.top_height -
				   fgeom.bottom_height,
				   d->layout,
				   text_height,
				   &button_layout,
				   button_states,
				   d->icon_pixbuf,
				   NULL);
	}
    }

    cairo_destroy (cr);

    XDestroyRegion (region);

    if (d->buffer_pixmap)
	gdk_draw_drawable  (d->pixmap,
			    d->gc,
			    d->buffer_pixmap,
			    0,
			    0,
			    0,
			    0,
			    d->width,
			    d->height);

    if (d->prop_xid)
    {
	decor_update_meta_window_property (d, theme, flags);
	d->prop_xid = 0;
    }
}
#endif

#define SWITCHER_ALPHA 0xa0a0

static void
draw_switcher_background (decor_t *d)
{
    Display	  *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    cairo_t	  *cr;
    GtkStyle	  *style;
    decor_color_t color;
    double	  alpha = SWITCHER_ALPHA / 65535.0;
    double	  x1, y1, x2, y2, h;
    int		  top;
    unsigned long pixel;
    ushort	  a = SWITCHER_ALPHA;

    if (!d->buffer_pixmap)
	return;

    style = gtk_widget_get_style (style_window);

    color.r = style->bg[GTK_STATE_NORMAL].red   / 65535.0;
    color.g = style->bg[GTK_STATE_NORMAL].green / 65535.0;
    color.b = style->bg[GTK_STATE_NORMAL].blue  / 65535.0;

    cr = gdk_cairo_create (GDK_DRAWABLE (d->buffer_pixmap));

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    top = _win_extents.bottom;

    x1 = window_context.left_space - _win_extents.left;
    y1 = window_context.top_space - _win_extents.top - titlebar_height;
    x2 = d->width - window_context.right_space + _win_extents.right;
    y2 = d->height - window_context.bottom_space + _win_extents.bottom;

    h = y2 - y1 - _win_extents.bottom - _win_extents.bottom;

    cairo_set_line_width (cr, 1.0);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    draw_shadow_background (d, cr);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + 0.5,
			    _win_extents.left - 0.5,
			    top - 0.5,
			    5.0, CORNER_TOPLEFT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + _win_extents.left,
			    y1 + 0.5,
			    x2 - x1 - _win_extents.left -
			    _win_extents.right,
			    top - 0.5,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y1 + 0.5,
			    _win_extents.right - 0.5,
			    top - 0.5,
			    5.0, CORNER_TOPRIGHT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_TOP | SHADE_RIGHT);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y1 + top,
			    _win_extents.left - 0.5,
			    h,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y1 + top,
			    _win_extents.right - 0.5,
			    h,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_RIGHT);

    fill_rounded_rectangle (cr,
			    x1 + 0.5,
			    y2 - _win_extents.bottom,
			    _win_extents.left - 0.5,
			    _win_extents.bottom - 0.5,
			    5.0, CORNER_BOTTOMLEFT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM | SHADE_LEFT);

    fill_rounded_rectangle (cr,
			    x1 + _win_extents.left,
			    y2 - _win_extents.bottom,
			    x2 - x1 - _win_extents.left -
			    _win_extents.right,
			    _win_extents.bottom - 0.5,
			    5.0, 0,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM);

    fill_rounded_rectangle (cr,
			    x2 - _win_extents.right,
			    y2 - _win_extents.bottom,
			    _win_extents.right - 0.5,
			    _win_extents.bottom - 0.5,
			    5.0, CORNER_BOTTOMRIGHT,
			    &color, alpha, &color, alpha * 0.75,
			    SHADE_BOTTOM | SHADE_RIGHT);

    cairo_rectangle (cr, x1 + _win_extents.left,
		     y1 + top,
		     x2 - x1 - _win_extents.left - _win_extents.right,
		     h);
    gdk_cairo_set_source_color_alpha (cr,
				      &style->bg[GTK_STATE_NORMAL],
				      alpha);
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

    gdk_cairo_set_source_color_alpha (cr,
				      &style->fg[GTK_STATE_NORMAL],
				      alpha);

    cairo_stroke (cr);

    cairo_destroy (cr);

    gdk_draw_drawable (d->pixmap,
		       d->gc,
		       d->buffer_pixmap,
		       0,
		       0,
		       0,
		       0,
		       d->width,
		       d->height);

    pixel = ((((a * style->bg[GTK_STATE_NORMAL].red  ) >> 24) & 0x0000ff) |
	     (((a * style->bg[GTK_STATE_NORMAL].green) >> 16) & 0x00ff00) |
	     (((a * style->bg[GTK_STATE_NORMAL].blue ) >>  8) & 0xff0000) |
	     (((a & 0xff00) << 16)));

    decor_update_switcher_property (d);

    gdk_error_trap_push ();
    XSetWindowBackground (xdisplay, d->prop_xid, pixel);
    XClearWindow (xdisplay, d->prop_xid);
    XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();

    d->prop_xid = 0;
}

static void
draw_switcher_foreground (decor_t *d)
{
    cairo_t	  *cr;
    GtkStyle	  *style;
    decor_color_t color;
    double	  alpha = SWITCHER_ALPHA / 65535.0;
    double	  x1, y1, x2;
    int		  top;

    if (!d->pixmap || !d->buffer_pixmap)
	return;

    style = gtk_widget_get_style (style_window);

    color.r = style->bg[GTK_STATE_NORMAL].red   / 65535.0;
    color.g = style->bg[GTK_STATE_NORMAL].green / 65535.0;
    color.b = style->bg[GTK_STATE_NORMAL].blue  / 65535.0;

    top = _win_extents.bottom;

    x1 = window_context.left_space - _win_extents.left;
    y1 = window_context.top_space - _win_extents.top;
    x2 = d->width - window_context.right_space + _win_extents.right;

    cr = gdk_cairo_create (GDK_DRAWABLE (d->buffer_pixmap));

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    cairo_rectangle (cr, x1 + _win_extents.left,
		     y1 + top + window_context.top_corner_space,
		     x2 - x1 - _win_extents.left - _win_extents.right,
		     SWITCHER_SPACE);

    gdk_cairo_set_source_color_alpha (cr,
				      &style->bg[GTK_STATE_NORMAL],
				      alpha);
    cairo_fill (cr);

    if (d->layout)
    {
	int w;

	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	gdk_cairo_set_source_color_alpha (cr,
					  &style->fg[GTK_STATE_NORMAL],
					  1.0);

	pango_layout_get_pixel_size (d->layout, &w, NULL);

	cairo_move_to (cr, d->width / 2 - w / 2,
		       y1 + top + window_context.top_corner_space +
		       SWITCHER_SPACE / 2 - text_height / 2);

	pango_cairo_show_layout (cr, d->layout);
    }

    cairo_destroy (cr);

    gdk_draw_drawable  (d->pixmap,
			d->gc,
			d->buffer_pixmap,
			0,
			0,
			0,
			0,
			d->width,
			d->height);
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

static GdkPixmap *
pixmap_new_from_pixbuf (GdkPixbuf *pixbuf)
{
    GdkPixmap *pixmap;
    guint     width, height;
    cairo_t   *cr;

    width  = gdk_pixbuf_get_width (pixbuf);
    height = gdk_pixbuf_get_height (pixbuf);

    pixmap = create_pixmap (width, height);
    if (!pixmap)
	return NULL;

    cr = (cairo_t *) gdk_cairo_create (GDK_DRAWABLE (pixmap));
    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy (cr);

    return pixmap;
}

static void
update_default_decorations (GdkScreen *screen)
{
    long	    data[256];
    Window	    xroot;
    GdkDisplay	    *gdkdisplay = gdk_display_get_default ();
    Display	    *xdisplay = gdk_x11_display_get_xdisplay (gdkdisplay);
    Atom	    bareAtom, normalAtom, activeAtom;
    decor_t	    d;
    gint	    nQuad;
    decor_quad_t    quads[N_QUADS_MAX];
    decor_extents_t extents = _win_extents;

    xroot = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    bareAtom   = XInternAtom (xdisplay, "_NET_WINDOW_DECOR_BARE", FALSE);
    normalAtom = XInternAtom (xdisplay, "_NET_WINDOW_DECOR_NORMAL", FALSE);
    activeAtom = XInternAtom (xdisplay, "_NET_WINDOW_DECOR_ACTIVE", FALSE);

    if (shadow)
    {
	nQuad = decor_set_shadow_quads (&shadow_context, quads,
					shadow->width,
					shadow->height);

	decor_quads_to_property (data, shadow->pixmap,
				 &_shadow_extents, &_shadow_extents,
				 0, 0, quads, nQuad);

	XChangeProperty (xdisplay, xroot,
			 bareAtom,
			 XA_INTEGER,
			 32, PropModeReplace, (guchar *) data,
			 BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);

	if (minimal)
	{
	    XChangeProperty (xdisplay, xroot,
			     normalAtom,
			     XA_INTEGER,
			     32, PropModeReplace, (guchar *) data,
			     BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
	    XChangeProperty (xdisplay, xroot,
			     activeAtom,
			     XA_INTEGER,
			     32, PropModeReplace, (guchar *) data,
			     BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
	}
    }
    else
    {
	XDeleteProperty (xdisplay, xroot, bareAtom);

	if (minimal)
	{
	    XDeleteProperty (xdisplay, xroot, normalAtom);
	    XDeleteProperty (xdisplay, xroot, activeAtom);
	}
    }

    if (minimal)
	return;

    memset (&d, 0, sizeof (d));

    d.width  = window_context.left_space + window_context.left_corner_space +
	1 + window_context.right_corner_space + window_context.right_space;
    d.height = window_context.top_space + window_context.top_corner_space + 1 +
	window_context.bottom_corner_space + window_context.bottom_space;

    extents.top += titlebar_height;

    d.draw = theme_draw_window_decoration;

    if (decor_normal_pixmap)
	gdk_pixmap_unref (decor_normal_pixmap);

    nQuad = decor_set_no_title_window_quads (&window_context,
					     quads, d.width, d.height);

    decor_normal_pixmap = create_pixmap (d.width, d.height);
    if (decor_normal_pixmap)
    {
	d.pixmap = decor_normal_pixmap;
	d.active = FALSE;

	(*d.draw) (&d);

	decor_quads_to_property (data, GDK_PIXMAP_XID (d.pixmap),
				 &extents, &extents, 0, 0, quads, nQuad);

	XChangeProperty (xdisplay, xroot,
			 normalAtom,
			 XA_INTEGER,
			 32, PropModeReplace, (guchar *) data,
			 BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
    }

    if (decor_active_pixmap)
	gdk_pixmap_unref (decor_active_pixmap);

    decor_active_pixmap = create_pixmap (d.width, d.height);
    if (decor_active_pixmap)
    {
	d.pixmap = decor_active_pixmap;
	d.active = TRUE;

	(*d.draw) (&d);

	decor_quads_to_property (data, GDK_PIXMAP_XID (d.pixmap),
				 &extents, &extents, 0, 0, quads, nQuad);

	XChangeProperty (xdisplay, xroot,
			 activeAtom,
			 XA_INTEGER,
			 32, PropModeReplace, (guchar *) data,
			 BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
    }
}

static void
set_dm_check_hint (GdkScreen *screen)
{
    XSetWindowAttributes attrs;
    unsigned long	 data[1];
    Window		 xroot;
    GdkDisplay		 *gdkdisplay = gdk_display_get_default ();
    Display		 *xdisplay = gdk_x11_display_get_xdisplay (gdkdisplay);
    Atom		 atom;

    attrs.override_redirect = TRUE;
    attrs.event_mask	    = PropertyChangeMask;

    xroot = RootWindowOfScreen (gdk_x11_screen_get_xscreen (screen));

    data[0] = XCreateWindow (xdisplay,
			     xroot,
			     -100, -100, 1, 1,
			     0,
			     CopyFromParent,
			     CopyFromParent,
			     (Visual *) CopyFromParent,
			     CWOverrideRedirect | CWEventMask,
			     &attrs);

    atom = XInternAtom (xdisplay, "_NET_SUPPORTING_DM_CHECK", FALSE);

    XChangeProperty (xdisplay, xroot,
		     atom,
		     XA_WINDOW,
		     32, PropModeReplace, (guchar *) data, 1);
}

static gboolean
get_window_prop (Window xwindow,
		 Atom   atom,
		 Window *val)
{
    Atom   type;
    int	   format;
    gulong nitems;
    gulong bytes_after;
    Window *w;
    int    err, result;

    *val = 0;

    gdk_error_trap_push ();

    type = None;
    result = XGetWindowProperty (gdk_display,
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
    MwmHints	  *mwm_hints;
    unsigned int  decor = MWM_DECOR_ALL;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    gdk_error_trap_push ();

    result = XGetWindowProperty (xdisplay, xwindow, mwm_hints_atom,
				 0L, 20L, FALSE, mwm_hints_atom,
				 &actual, &format, &n, &left,
				 (unsigned char **) &mwm_hints);

    err = gdk_error_trap_pop ();
    if (err != Success || result != Success)
	return decor;

    if (n && mwm_hints)
    {
	if (n >= PROP_MOTIF_WM_HINT_ELEMENTS)
	{
	    if (mwm_hints->flags & MWM_HINTS_DECORATIONS)
		decor = mwm_hints->decorations;
	}

	XFree (mwm_hints);
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
    *w = pos[i][j].w + pos[i][j].ww * width;
    *h = pos[i][j].h + pos[i][j].hh * height + pos[i][j].hth *
	(titlebar_height - 17);
}

static void
get_button_position (decor_t *d,
		     gint    i,
		     gint    width,
		     gint    height,
		     gint    *x,
		     gint    *y,
		     gint    *w,
		     gint    *h)
{
    *x = bpos[i].x + bpos[i].xw * width;
    *y = bpos[i].y + bpos[i].yh * height + bpos[i].yth *
	(titlebar_height - 17);
    *w = bpos[i].w + bpos[i].ww * width;
    *h = bpos[i].h + bpos[i].hh * height + bpos[i].hth +
	(titlebar_height - 17);

    *x -= 10 + 16 * i;
}

#ifdef USE_METACITY

#define TOP_RESIZE_HEIGHT 2
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
	    *x = width - fgeom.right_width;
	    *y = height - fgeom.bottom_height;
	    *w = fgeom.right_width;
	    *h = fgeom.bottom_height;
	    break;
	case 1: /* bottom */
	    *x = fgeom.left_width;
	    *y = height - fgeom.bottom_height;
	    *w = width - fgeom.left_width - fgeom.right_width;
	    *h = fgeom.bottom_height;
	    break;
	case 0: /* bottom left */
	default:
	    *x = 0;
	    *y = height - fgeom.bottom_height;
	    *w = fgeom.left_width;
	    *h = fgeom.bottom_height;
	    break;
	}
	break;
    case 1: /* middle */
	switch (j) {
	case 2: /* right */
	    *x = width - fgeom.right_width;
	    *y = fgeom.top_height;
	    *w = fgeom.right_width;
	    *h = height - fgeom.top_height - fgeom.bottom_height;
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
	    *y = fgeom.top_height;
	    *w = fgeom.left_width;
	    *h = height - fgeom.top_height - fgeom.bottom_height;
	    break;
	}
	break;
    case 0: /* top */
    default:
	switch (j) {
	case 2: /* top right */
	    *x = width - fgeom.right_width;
	    *y = 0;
	    *w = fgeom.right_width;
	    *h = fgeom.top_height;
	    break;
	case 1: /* top */
	    *x = fgeom.left_width;
	    *y = 0;
	    *w = width - fgeom.left_width - fgeom.right_width;
	    *h = fgeom.title_rect.y + TOP_RESIZE_HEIGHT;
	    break;
	case 0: /* top left */
	default:
	    *x = 0;
	    *y = 0;
	    *w = fgeom.left_width;
	    *h = fgeom.top_height;
	    break;
	}
    }
}

static void
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

#ifdef HAVE_METACITY_2_15_21
    MetaButtonSpace   *space;
#else
    GdkRectangle      *space;
#endif

    theme = meta_theme_get_current ();

    meta_get_decoration_geometry (d, theme, &flags, &fgeom, &button_layout,
				  &clip);

    switch (i) {
    case 2:
	space = &fgeom.min_rect;
	break;
    case 1:
	space = &fgeom.max_rect;
	break;
    case 0:
    default:
	space = &fgeom.close_rect;
	break;
    }

#ifdef HAVE_METACITY_2_15_21
    *x = space->clickable.x;
    *y = space->clickable.y;
    *w = space->clickable.width;
    *h = space->clickable.height;
#else
    *x = space->x;
    *y = space->y;
    *w = space->width;
    *h = space->height;
#endif

}

#endif

static void
update_event_windows (WnckWindow *win)
{
    Display *xdisplay;
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    gint    x0, y0, width, height, x, y, w, h;
    gint    i, j, k, l;

    xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

    wnck_window_get_geometry (win, &x0, &y0, &width, &height);

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
	    if (d->actions & event_window_actions[i][j] && i >= k && i <= l)
	    {
		(*theme_get_event_window_position) (d, i, j, width, height,
						    &x, &y, &w, &h);

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

    for (i = 0; i < 3; i++)
    {
	static guint button_actions[3] = {
	    WNCK_WINDOW_ACTION_CLOSE,
	    WNCK_WINDOW_ACTION_MAXIMIZE,
	    WNCK_WINDOW_ACTION_MINIMIZE
	};

	if (d->actions & button_actions[i])
	{
	    (*theme_get_button_position) (d, i, width, height, &x, &y, &w, &h);

	    XMapWindow (xdisplay, d->button_windows[i]);
	    XMoveResizeWindow (xdisplay, d->button_windows[i], x, y, w, h);
	}
	else
	    XUnmapWindow (xdisplay, d->button_windows[i]);
    }

    XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();
}

#if HAVE_WNCK_WINDOW_HAS_NAME
static const char *
wnck_window_get_real_name (WnckWindow *win)
{
    return wnck_window_has_name (win) ? wnck_window_get_name (win) : NULL;
}
#define wnck_window_get_name wnck_window_get_real_name
#endif

static gint
max_window_name_width (WnckWindow *win)
{
    decor_t     *d = g_object_get_data (G_OBJECT (win), "decor");
    const gchar *name;
    gint	w;

    name = wnck_window_get_name (win);
    if (!name)
	return 0;

    if (!d->layout)
    {
	d->layout = pango_layout_new (pango_context);
	if (!d->layout)
	    return 0;

	pango_layout_set_wrap (d->layout, PANGO_WRAP_CHAR);
    }

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
	gint w, n_line;

	w  = d->width - window_context.left_space - window_context.right_space -
	    ICON_SPACE - 4;
	w -= d->button_width;
	if (w < 1)
	    w = 1;

	pango_layout_set_width (d->layout, w * PANGO_SCALE);
	pango_layout_set_text (d->layout, name, name_length);

	n_line = pango_layout_get_line_count (d->layout);

	line = pango_layout_get_line (d->layout, 0);

	name_length = line->length;
	if (pango_layout_get_line_count (d->layout) > 1)
	{
	    if (name_length < 4)
	    {
		g_object_unref (G_OBJECT (d->layout));
		d->layout = NULL;
		return;
	    }

	    d->name = g_strndup (name, name_length);
	    strcpy (d->name + name_length - 3, "...");
	}
	else
	    d->name = g_strndup (name, name_length);

	pango_layout_set_text (d->layout, d->name, name_length);
    }
    else if (d->layout)
    {
	g_object_unref (G_OBJECT (d->layout));
	d->layout = NULL;
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

    if (d->icon_pixmap)
    {
	gdk_pixmap_unref (d->icon_pixmap);
	d->icon_pixmap = NULL;
    }

    if (d->icon_pixbuf)
	gdk_pixbuf_unref (d->icon_pixbuf);

    d->icon_pixbuf = wnck_window_get_mini_icon (win);
    if (d->icon_pixbuf)
    {
	cairo_t	*cr;

	gdk_pixbuf_ref (d->icon_pixbuf);

	d->icon_pixmap = pixmap_new_from_pixbuf (d->icon_pixbuf);
	cr = gdk_cairo_create (GDK_DRAWABLE (d->icon_pixmap));
	d->icon = cairo_pattern_create_for_surface (cairo_get_target (cr));
	cairo_destroy (cr);
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

static gboolean
update_window_button_size (WnckWindow *win)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    gint    button_width;

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

    if (button_width != d->button_width)
    {
	d->button_width = button_width;

	return TRUE;
    }

    return FALSE;
}

static gboolean
calc_decoration_size (decor_t *d,
		      gint    w,
		      gint    h,
		      gint    name_width,
		      gint    *width,
		      gint    *height)
{
    if (w < ICON_SPACE + d->button_width)
	return FALSE;

    *width = name_width + d->button_width + ICON_SPACE;
    if (w < *width)
	*width = MAX (ICON_SPACE + d->button_width, w);

    *width  = MAX (*width, window_context.left_corner_space +
		   window_context.right_corner_space);
    *width += window_context.left_space + 1 + window_context.right_space;

    *height  = titlebar_height +
	window_context.top_corner_space + window_context.bottom_corner_space;
    *height += window_context.top_space + 1 + window_context.bottom_space;

    return (*width != d->width || *height != d->height);
}

#ifdef USE_METACITY
static gboolean
meta_calc_decoration_size (decor_t *d,
			   gint    w,
			   gint    h,
			   gint    name_width,
			   gint    *width,
			   gint    *height)
{
    *width  = MAX (w, window_context.left_corner_space +
		   window_context.right_corner_space);
    *width += window_context.left_space + 1 + window_context.right_space;

    *height  = titlebar_height +
	window_context.top_corner_space + window_context.bottom_corner_space;
    *height += window_context.top_space + 1 + window_context.bottom_space;

    return (*width != d->width || *height != d->height);
}
#endif

static gboolean
update_window_decoration_size (WnckWindow *win)
{
    decor_t   *d = g_object_get_data (G_OBJECT (win), "decor");
    GdkPixmap *pixmap, *buffer_pixmap = NULL;
    gint      width, height;
    gint      w, h, name_width;

    wnck_window_get_geometry (win, NULL, NULL, &w, &h);

    name_width = max_window_name_width (win);

    if (!(*theme_calc_decoration_size) (d, w, h, name_width, &width, &height))
    {
	update_window_decoration_name (win);
	return FALSE;
    }

    pixmap = create_pixmap (width, height);
    if (!pixmap)
	return FALSE;

    buffer_pixmap = create_pixmap (width, height);
    if (!buffer_pixmap)
    {
	gdk_pixmap_unref (pixmap);
	return FALSE;
    }

    if (d->pixmap)
	gdk_pixmap_unref (d->pixmap);

    if (d->buffer_pixmap)
	gdk_pixmap_unref (d->buffer_pixmap);

    if (d->gc)
	gdk_gc_unref (d->gc);

    d->pixmap	     = pixmap;
    d->buffer_pixmap = buffer_pixmap;
    d->gc	     = gdk_gc_new (pixmap);

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
		XDefineCursor (xdisplay, d->event_windows[i][j],
			       cursor[i][j].cursor);
	}
    }

    attr.event_mask |= ButtonReleaseMask;

    for (i = 0; i < 3; i++)
    {
	d->button_windows[i] =
	    XCreateWindow (xdisplay,
			   frame,
			   0, 0, 1, 1, 0,
			   CopyFromParent, CopyFromParent, CopyFromParent,
			   CWOverrideRedirect | CWEventMask, &attr);

	d->button_states[i] = 0;
    }

    XSync (xdisplay, FALSE);
    if (!gdk_error_trap_pop ())
    {
	if (get_mwm_prop (xid) & (MWM_DECOR_ALL | MWM_DECOR_TITLE))
	    d->decorated = TRUE;

	for (i = 0; i < 3; i++)
	    for (j = 0; j < 3; j++)
		g_hash_table_insert (frame_table,
				     GINT_TO_POINTER (d->event_windows[i][j]),
				     GINT_TO_POINTER (xid));

	for (i = 0; i < 3; i++)
	    g_hash_table_insert (frame_table,
				 GINT_TO_POINTER (d->button_windows[i]),
				 GINT_TO_POINTER (xid));


	update_window_decoration_state (win);
	update_window_decoration_actions (win);
	update_window_decoration_icon (win);
	update_window_button_size (win);
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
    decor_t    *d = g_object_get_data (G_OBJECT (win), "decor");
    GdkPixmap  *pixmap, *buffer_pixmap = NULL;
    gint       height, width = 0;
    WnckWindow *selected_win;

    wnck_window_get_geometry (win, NULL, NULL, &width, NULL);

    width  += window_context.left_space + window_context.right_space;
    height  = window_context.top_space + SWITCHER_TOP_EXTRA +
	window_context.top_corner_space + SWITCHER_SPACE +
	window_context.bottom_corner_space + window_context.bottom_space;

    d->decorated = FALSE;
    d->draw	 = draw_switcher_decoration;

    if (!d->pixmap && switcher_pixmap)
    {
	gdk_pixmap_ref (switcher_pixmap);
	d->pixmap = switcher_pixmap;
    }

    if (!d->buffer_pixmap && switcher_buffer_pixmap)
    {
	gdk_pixmap_ref (switcher_buffer_pixmap);
	d->buffer_pixmap = switcher_buffer_pixmap;
    }

    if (!d->width)
	d->width = switcher_width;

    if (!d->height)
	d->height = switcher_height;

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
	    gint n_line;

	    if (!d->layout)
	    {
		d->layout = pango_layout_new (pango_context);
		if (d->layout)
		    pango_layout_set_wrap (d->layout, PANGO_WRAP_CHAR);
	    }

	    if (d->layout)
	    {
		int tw;

		tw = width - window_context.left_space -
		    window_context.right_space - 64;
		pango_layout_set_width (d->layout, tw * PANGO_SCALE);
		pango_layout_set_text (d->layout, name, name_length);

		n_line = pango_layout_get_line_count (d->layout);

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

    if (width == d->width && height == d->height)
    {
	if (!d->gc)
	    d->gc = gdk_gc_new (d->pixmap);

	queue_decor_draw (d);
	return FALSE;
    }

    pixmap = create_pixmap (width, height);
    if (!pixmap)
	return FALSE;

    buffer_pixmap = create_pixmap (width, height);
    if (!buffer_pixmap)
    {
	gdk_pixmap_unref (pixmap);
	return FALSE;
    }

    if (switcher_pixmap)
	gdk_pixmap_unref (switcher_pixmap);

    if (switcher_buffer_pixmap)
	gdk_pixmap_unref (switcher_buffer_pixmap);

    if (d->pixmap)
	gdk_pixmap_unref (d->pixmap);

    if (d->buffer_pixmap)
	gdk_pixmap_unref (d->buffer_pixmap);

    if (d->gc)
	gdk_gc_unref (d->gc);

    switcher_pixmap	   = pixmap;
    switcher_buffer_pixmap = buffer_pixmap;

    switcher_width  = width;
    switcher_height = height;

    gdk_pixmap_ref (pixmap);
    gdk_pixmap_ref (buffer_pixmap);

    d->pixmap	     = pixmap;
    d->buffer_pixmap = buffer_pixmap;
    d->gc	     = gdk_gc_new (pixmap);

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

    if (d->pixmap)
    {
	gdk_pixmap_unref (d->pixmap);
	d->pixmap = NULL;
    }

    if (d->buffer_pixmap)
    {
	gdk_pixmap_unref (d->buffer_pixmap);
	d->buffer_pixmap = NULL;
    }

    if (d->gc)
    {
	gdk_gc_unref (d->gc);
	d->gc = NULL;
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

    if (d->icon_pixmap)
    {
	gdk_pixmap_unref (d->icon_pixmap);
	d->icon_pixmap = NULL;
    }

    if (d->icon_pixbuf)
    {
	gdk_pixbuf_unref (d->icon_pixbuf);
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
	update_window_decoration_size (win);
	update_event_windows (win);
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
	if (update_window_button_size (win))
	{
	    update_window_decoration_size (win);
	    update_event_windows (win);
	}
	else
	{
	    queue_decor_draw (d);
	}
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
	if (d->pixmap)
	{
	    d->active = wnck_window_is_active (win);
	    queue_decor_draw (d);
	}
    }

    win = wnck_screen_get_active_window (screen);
    if (win)
    {
	d = g_object_get_data (G_OBJECT (win), "decor");
	if (d->pixmap)
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

    d = g_malloc (sizeof (decor_t));
    if (!d)
	return;

    d->pixmap	     = NULL;
    d->buffer_pixmap = NULL;
    d->gc	     = NULL;

    d->icon	   = NULL;
    d->icon_pixmap = NULL;
    d->icon_pixbuf = NULL;

    d->button_width = 0;

    d->width  = 0;
    d->height = 0;

    d->active = wnck_window_is_active (win);

    d->layout = NULL;
    d->name   = NULL;

    d->state   = 0;
    d->actions = 0;

    d->prop_xid = 0;

    d->decorated = FALSE;

    d->force_quit_dialog = NULL;

    d->draw = theme_draw_window_decoration;

    g_object_set_data (G_OBJECT (win), "decor", d);

    connect_window (win);

    xid = wnck_window_get_xid (win);

    if (get_window_prop (xid, frame_window_atom, &window))
    {
	add_frame_window (win, window);
    }
    else if (get_window_prop (xid, select_window_atom, &window))
    {
	d->prop_xid = wnck_window_get_xid (win);
	update_switcher_window (win, window);
    }
}

static void
window_closed (WnckScreen *screen,
	       WnckWindow *win)
{
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

    remove_frame_window (win);

    gdk_error_trap_push ();
    XDeleteProperty (xdisplay, wnck_window_get_xid (win), win_decor_atom);
    XSync (xdisplay, FALSE);
    gdk_error_trap_pop ();

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
	gtk_object_destroy (GTK_OBJECT (action_menu));
	action_menu_mapped = FALSE;
	action_menu = NULL;
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
	gtk_object_destroy (GTK_OBJECT (action_menu));
	action_menu_mapped = FALSE;
	action_menu = NULL;
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

#define DEFAULT_DELAY 500           /* Default delay in ms */
#define STICKY_DELAY 0              /* Delay before popping up next tip
				     * if we're sticky
				     */
#define STICKY_REVERT_DELAY 1000    /* Delay before sticky tooltips revert
				     * to normal
				     */

static void
show_tooltip (const char *text)
{
    GdkDisplay     *gdkdisplay;
    GtkRequisition requisition;
    gint	   x, y, w, h;
    GdkScreen	   *screen;
    gint	   monitor_num;
    GdkRectangle   monitor;

    gdkdisplay = gdk_display_get_default ();

    gtk_label_set_text (GTK_LABEL (tip_label), text);

    gtk_widget_size_request (tip_window, &requisition);

    w = requisition.width;
    h = requisition.height;

    gdk_display_get_pointer (gdkdisplay, &screen, &x, &y, NULL);

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
    if (GTK_WIDGET_VISIBLE (tip_window))
	g_get_current_time (&tooltip_last_popdown);

    gtk_widget_hide (tip_window);

    if (tooltip_timer_tag)
    {
	g_source_remove (tooltip_timer_tag);
	tooltip_timer_tag = 0;
    }
}

static gboolean
tooltip_recently_shown (void)
{
    GTimeVal now;
    glong    msec;

    g_get_current_time (&now);

    msec = (now.tv_sec - tooltip_last_popdown.tv_sec) * 1000 +
	(now.tv_usec - tooltip_last_popdown.tv_usec) / 1000;

    return (msec < STICKY_REVERT_DELAY);
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
    guint delay = DEFAULT_DELAY;

    if (tooltip_timer_tag)
	return;

    if (tooltip_recently_shown ())
	delay = STICKY_DELAY;

    tooltip_timer_tag = g_timeout_add (delay,
				       tooltip_timeout,
				       (gpointer) text);
}

static gint
tooltip_paint_window (GtkWidget *tooltip)
{
    GtkRequisition req;

    gtk_widget_size_request (tip_window, &req);
    gtk_paint_flat_box (tip_window->style, tip_window->window,
			GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			NULL, GTK_WIDGET (tip_window), "tooltip",
			0, 0, req.width, req.height);

    return FALSE;
}

static gboolean
create_tooltip_window (void)
{
    tip_window = gtk_window_new (GTK_WINDOW_POPUP);

    gtk_widget_set_app_paintable (tip_window, TRUE);
    gtk_window_set_resizable (GTK_WINDOW (tip_window), FALSE);
    gtk_widget_set_name (tip_window, "gtk-tooltips");
    gtk_container_set_border_width (GTK_CONTAINER (tip_window), 4);

#if GTK_CHECK_VERSION (2, 10, 0)
    if (!gtk_check_version (2, 10, 0))
	gtk_window_set_type_hint (GTK_WINDOW (tip_window),
				  GDK_WINDOW_TYPE_HINT_TOOLTIP);
#endif

    g_signal_connect_swapped (tip_window,
			      "expose_event",
			      G_CALLBACK (tooltip_paint_window),
			      0);

    tip_label = gtk_label_new (NULL);
    gtk_label_set_line_wrap (GTK_LABEL (tip_label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (tip_label), 0.5, 0.5);
    gtk_widget_show (tip_label);

    gtk_container_add (GTK_CONTAINER (tip_window), tip_label);

    gtk_widget_ensure_style (tip_window);

    return TRUE;
}

static void
handle_tooltip_event (WnckWindow *win,
		      XEvent     *xevent,
		      guint	 state,
		      const char *tip)
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
		tooltip_start_delay (tip);
	}
	break;
    case LeaveNotify:
	hide_tooltip ();
	break;
    }
}

static void
close_button_event (WnckWindow *win,
		    XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[0];

    handle_tooltip_event (win, xevent, state, "Close Window");

    switch (xevent->type) {
    case ButtonPress:
	if (xevent->xbutton.button == 1)
	    d->button_states[0] |= PRESSED_EVENT_WINDOW;
	break;
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (d->button_states[0] == (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW))
		wnck_window_close (win, xevent->xbutton.time);

	    d->button_states[0] &= ~PRESSED_EVENT_WINDOW;
	}
	break;
    case EnterNotify:
	d->button_states[0] |= IN_EVENT_WINDOW;
	break;
    case LeaveNotify:
	d->button_states[0] &= ~IN_EVENT_WINDOW;
	break;
    }

    if (state != d->button_states[0])
	queue_decor_draw (d);
}

static void
max_button_event (WnckWindow *win,
		  XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[1];

    if (wnck_window_is_maximized (win))
	handle_tooltip_event (win, xevent, state, "Unmaximize Window");
    else
	handle_tooltip_event (win, xevent, state, "Maximize Window");

    switch (xevent->type) {
    case ButtonPress:
	if (xevent->xbutton.button == 1)
	    d->button_states[1] |= PRESSED_EVENT_WINDOW;
	break;
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (d->button_states[1] == (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW))
	    {
		if (wnck_window_is_maximized (win))
		    wnck_window_unmaximize (win);
		else
		    wnck_window_maximize (win);
	    }

	    d->button_states[1] &= ~PRESSED_EVENT_WINDOW;
	}
	break;
    case EnterNotify:
	d->button_states[1] |= IN_EVENT_WINDOW;
	break;
    case LeaveNotify:
	d->button_states[1] &= ~IN_EVENT_WINDOW;
	break;
    }

    if (state != d->button_states[1])
	queue_decor_draw (d);
}

static void
min_button_event (WnckWindow *win,
		  XEvent     *xevent)
{
    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");
    guint   state = d->button_states[2];

    handle_tooltip_event (win, xevent, state, "Minimize Window");

    switch (xevent->type) {
    case ButtonPress:
	if (xevent->xbutton.button == 1)
	    d->button_states[2] |= PRESSED_EVENT_WINDOW;
	break;
    case ButtonRelease:
	if (xevent->xbutton.button == 1)
	{
	    if (d->button_states[2] == (PRESSED_EVENT_WINDOW | IN_EVENT_WINDOW))
		wnck_window_minimize (win);

	    d->button_states[2] &= ~PRESSED_EVENT_WINDOW;
	}
	break;
    case EnterNotify:
	d->button_states[2] |= IN_EVENT_WINDOW;
	if (wnck_window_is_active (win))
	    tooltip_start_delay ("Minimize Window");
	break;
    case LeaveNotify:
	d->button_states[2] &= ~IN_EVENT_WINDOW;
	break;
    }

    if (state != d->button_states[2])
	queue_decor_draw (d);
}

static void
top_left_event (WnckWindow *win,
		XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_TOPLEFT, xevent);
}

static void
top_event (WnckWindow *win,
	   XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_TOP, xevent);
}

static void
top_right_event (WnckWindow *win,
		 XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_TOPRIGHT, xevent);
}

static void
left_event (WnckWindow *win,
	    XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_LEFT, xevent);
}

static void
action_menu_unmap (GObject *object)
{
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

    wnck_window_get_geometry (win, x, y, NULL, NULL);

    *push_in = TRUE;
}

static void
action_menu_map (WnckWindow *win,
		 long	    button,
		 Time	    time)
{
    GdkDisplay *gdkdisplay;
    GdkScreen  *screen;

    gdkdisplay = gdk_display_get_default ();
    screen     = gdk_display_get_default_screen (gdkdisplay);

    if (action_menu)
    {
	if (action_menu_mapped)
	{
	    gtk_widget_destroy (action_menu);
	    action_menu_mapped = FALSE;
	    action_menu = NULL;
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
    case WNCK_WINDOW_MODAL_DIALOG:
    case WNCK_WINDOW_TOOLBAR:
    case WNCK_WINDOW_MENU:
    case WNCK_WINDOW_UTILITY:
    case WNCK_WINDOW_SPLASHSCREEN:
	/* allow window action menu */
	break;
    }

    action_menu = wnck_create_window_action_menu (win);

    gtk_menu_set_screen (GTK_MENU (action_menu), screen);

    g_signal_connect_object (G_OBJECT (action_menu), "unmap",
			     G_CALLBACK (action_menu_unmap),
			     0, 0);

    gtk_widget_show (action_menu);

    if (button)
	gtk_menu_popup (GTK_MENU (action_menu),
			NULL, NULL,
			NULL, NULL,
			button,
			time);
    else
	gtk_menu_popup (GTK_MENU (action_menu),
			NULL, NULL,
			position_action_menu, (gpointer) win,
			button,
			time);

    action_menu_mapped = TRUE;
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
	if (xevent->xbutton.button == last_button_num			  &&
	    xevent->xbutton.window == last_button_xwindow		  &&
	    xevent->xbutton.time < last_button_time + double_click_timeout &&
	    dist (xevent->xbutton.x, xevent->xbutton.y,
		  last_button_x, last_button_y) < DOUBLE_CLICK_DISTANCE)
	{
	    switch (double_click_action) {
	    case DOUBLE_CLICK_SHADE:
		if (wnck_window_is_shaded (win))
		    wnck_window_unshade (win);
		else
		    wnck_window_shade (win);
		break;
	    case DOUBLE_CLICK_MAXIMIZE:
		if (wnck_window_is_maximized (win))
		    wnck_window_unmaximize (win);
		else
		    wnck_window_maximize (win);
	    default:
		break;
	    }

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
	restack_window (win, Below);
    }
    else if (xevent->xbutton.button == 3)
    {
	action_menu_map (win,
			 xevent->xbutton.button,
			 xevent->xbutton.time);
    }
}

static void
right_event (WnckWindow *win,
	     XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_RIGHT, xevent);
}

static void
bottom_left_event (WnckWindow *win,
		   XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_BOTTOMLEFT, xevent);
}

static void
bottom_event (WnckWindow *win,
	      XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_BOTTOM, xevent);
}

static void
bottom_right_event (WnckWindow *win,
		    XEvent     *xevent)
{
    if (xevent->xbutton.button == 1)
	move_resize_window (win, WM_MOVERESIZE_SIZE_BOTTOMRIGHT, xevent);
}

static void
panel_action (Display *xdisplay,
	      Window  root,
	      Atom    panel_action,
	      Time    event_time)
{
    XEvent ev;

    ev.type		    = ClientMessage;
    ev.xclient.window	    = root;
    ev.xclient.message_type = panel_action_atom;
    ev.xclient.format	    = 32;
    ev.xclient.data.l[0]    = panel_action;
    ev.xclient.data.l[1]    = event_time;
    ev.xclient.data.l[2]    = 0;
    ev.xclient.data.l[3]    = 0;
    ev.xclient.data.l[4]    = 0;

    XSendEvent (xdisplay, root, FALSE, StructureNotifyMask, &ev);
}

static void
force_quit_dialog_realize (GtkWidget *dialog,
			   void      *data)
{
    WnckWindow *win = data;

    gdk_error_trap_push ();
    XSetTransientForHint (gdk_display,
			  GDK_WINDOW_XID (dialog->window),
			  wnck_window_get_xid (win));
    XSync (gdk_display, FALSE);
    gdk_error_trap_pop ();
}

static char *
get_client_machine (Window xwindow)
{
    Atom   atom, type;
    gulong nitems, bytes_after;
    gchar  *str = NULL;
    int    format, result;
    char   *retval;

    atom = XInternAtom (gdk_display, "WM_CLIENT_MACHINE", FALSE);

    gdk_error_trap_push ();

    result = XGetWindowProperty (gdk_display,
				 xwindow, atom,
				 0, G_MAXLONG,
				 FALSE, XA_STRING, &type, &format, &nitems,
				 &bytes_after, (guchar **) &str);

    gdk_error_trap_pop ();

    if (result != Success)
	return NULL;

    if (type != XA_STRING)
    {
	XFree (str);
	return NULL;
    }

    retval = g_strdup (str);

    XFree (str);

    return retval;
}

static void
kill_window (WnckWindow *win)
{
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
    XKillClient (gdk_display, wnck_window_get_xid (win));
    XSync (gdk_display, FALSE);
    gdk_error_trap_pop ();
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
    decor_t   *d = g_object_get_data (G_OBJECT (win), "decor");
    GtkWidget *dialog;
    gchar     *str, *tmp;

    if (d->force_quit_dialog)
	return;

    tmp = g_markup_escape_text (wnck_window_get_name (win), -1);
    str = g_strdup_printf ("The window \"%s\" is not responding.", tmp);

    g_free (tmp);

    dialog = gtk_message_dialog_new (NULL, 0,
				     GTK_MESSAGE_WARNING,
				     GTK_BUTTONS_NONE,
				     "<b>%s</b>\n\n%s",
				     str,
				     "Forcing this application to "
				     "quit will cause you to lose any "
				     "unsaved changes.");
    g_free (str);

    gtk_window_set_icon_name (GTK_WINDOW (dialog), "force-quit");

    gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label),
			      TRUE);
    gtk_label_set_line_wrap (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label),
			     TRUE);

    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			    GTK_STOCK_CANCEL,
			    GTK_RESPONSE_REJECT,
			    "_Force Quit",
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

    gdk_x11_window_set_user_time (dialog->window, timestamp);

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

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
static gboolean
convert_property (Display *xdisplay,
		  Window  w,
		  Atom    target,
		  Atom    property)
{

#define N_TARGETS 4

    Atom conversion_targets[N_TARGETS];
    long icccm_version[] = { 2, 0 };

    conversion_targets[0] = targets_atom;
    conversion_targets[1] = multiple_atom;
    conversion_targets[2] = timestamp_atom;
    conversion_targets[3] = version_atom;

    if (target == targets_atom)
	XChangeProperty (xdisplay, w, property,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) conversion_targets, N_TARGETS);
    else if (target == timestamp_atom)
	XChangeProperty (xdisplay, w, property,
			 XA_INTEGER, 32, PropModeReplace,
			 (unsigned char *) &dm_sn_timestamp, 1);
    else if (target == version_atom)
	XChangeProperty (xdisplay, w, property,
			 XA_INTEGER, 32, PropModeReplace,
			 (unsigned char *) icccm_version, 2);
    else
	return FALSE;

    /* Be sure the PropertyNotify has arrived so we
     * can send SelectionNotify
     */
    XSync (xdisplay, FALSE);

    return TRUE;
}

static void
handle_selection_request (Display *xdisplay,
			  XEvent  *event)
{
    XSelectionEvent reply;

    reply.type	    = SelectionNotify;
    reply.display   = xdisplay;
    reply.requestor = event->xselectionrequest.requestor;
    reply.selection = event->xselectionrequest.selection;
    reply.target    = event->xselectionrequest.target;
    reply.property  = None;
    reply.time	    = event->xselectionrequest.time;

    if (event->xselectionrequest.target == multiple_atom)
    {
	if (event->xselectionrequest.property != None)
	{
	    Atom	  type, *adata;
	    int		  i, format;
	    unsigned long num, rest;
	    unsigned char *data;

	    if (XGetWindowProperty (xdisplay,
				    event->xselectionrequest.requestor,
				    event->xselectionrequest.property,
				    0, 256, FALSE,
				    atom_pair_atom,
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
		if (!convert_property (xdisplay,
				       event->xselectionrequest.requestor,
				       adata[i], adata[i + 1]))
		    adata[i + 1] = None;

		i += 2;
	    }

	    XChangeProperty (xdisplay,
			     event->xselectionrequest.requestor,
			     event->xselectionrequest.property,
			     atom_pair_atom,
			     32, PropModeReplace, data, num);
	}
    }
    else
    {
	if (event->xselectionrequest.property == None)
	    event->xselectionrequest.property = event->xselectionrequest.target;

	if (convert_property (xdisplay,
			      event->xselectionrequest.requestor,
			      event->xselectionrequest.target,
			      event->xselectionrequest.property))
	    reply.property = event->xselectionrequest.property;
    }

    XSendEvent (xdisplay,
		event->xselectionrequest.requestor,
		FALSE, 0L, (XEvent *) &reply);
}

static void
handle_selection_clear (Display *xdisplay,
			XEvent  *xevent)
{
    if (xevent->xselectionclear.selection == dm_sn_atom)
	exit (0);
}

static gboolean
acquire_dm_session (Display  *xdisplay,
		    int	     screen,
		    gboolean replace_current_dm)
{
    XEvent		 event;
    XSetWindowAttributes attr;
    Window		 current_dm_sn_owner, new_dm_sn_owner;
    char		 buf[128];

    sprintf (buf, "DM_S%d", screen);
    dm_sn_atom = XInternAtom (xdisplay, buf, 0);

    current_dm_sn_owner = XGetSelectionOwner (xdisplay, dm_sn_atom);

    if (current_dm_sn_owner != None)
    {
	if (!replace_current_dm)
	{
	    fprintf (stderr,
		     "%s: Screen %d on display \"%s\" already "
		     "has a decoration manager; try using the "
		     "--replace option to replace the current "
		     "decoration manager.\n",
		     program_name, screen, DisplayString (xdisplay));

	    return FALSE;
	}

	XSelectInput (xdisplay, current_dm_sn_owner, StructureNotifyMask);
    }

    attr.override_redirect = TRUE;
    attr.event_mask	   = PropertyChangeMask;

    new_dm_sn_owner =
	XCreateWindow (xdisplay, XRootWindow (xdisplay, screen),
		       -100, -100, 1, 1, 0,
		       CopyFromParent, CopyFromParent,
		       CopyFromParent,
		       CWOverrideRedirect | CWEventMask,
		       &attr);

    XChangeProperty (xdisplay,
		     new_dm_sn_owner,
		     dm_name_atom,
		     utf8_string_atom, 8,
		     PropModeReplace,
		     (unsigned char *) "gwd",
		     strlen ("gwd"));

    XWindowEvent (xdisplay,
		  new_dm_sn_owner,
		  PropertyChangeMask,
		  &event);

    dm_sn_timestamp = event.xproperty.time;

    XSetSelectionOwner (xdisplay, dm_sn_atom, new_dm_sn_owner,
			dm_sn_timestamp);

    if (XGetSelectionOwner (xdisplay, dm_sn_atom) != new_dm_sn_owner)
    {
	fprintf (stderr,
		 "%s: Could not acquire decoration manager "
		 "selection on screen %d display \"%s\"\n",
		 program_name, screen, DisplayString (xdisplay));

	XDestroyWindow (xdisplay, new_dm_sn_owner);

	return FALSE;
    }

    /* Send client message indicating that we are now the DM */
    event.xclient.type	       = ClientMessage;
    event.xclient.window       = XRootWindow (xdisplay, screen);
    event.xclient.message_type = manager_atom;
    event.xclient.format       = 32;
    event.xclient.data.l[0]    = dm_sn_timestamp;
    event.xclient.data.l[1]    = dm_sn_atom;
    event.xclient.data.l[2]    = 0;
    event.xclient.data.l[3]    = 0;
    event.xclient.data.l[4]    = 0;

    XSendEvent (xdisplay, XRootWindow (xdisplay, screen), FALSE,
		StructureNotifyMask, &event);

    /* Wait for old decoration manager to go away */
    if (current_dm_sn_owner != None)
    {
	do {
	    XWindowEvent (xdisplay, current_dm_sn_owner,
			  StructureNotifyMask, &event);
	} while (event.type != DestroyNotify);
    }

    return TRUE;
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
		Window frame;

		if (get_window_prop (xid, frame_window_atom, &frame))
		    add_frame_window (win, frame);
		else
		    remove_frame_window (win);
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
			d->width = d->height = 0;

			update_window_decoration_size (win);
			update_event_windows (win);
		    }
		    else
		    {
			gdk_error_trap_push ();
			XDeleteProperty (xdisplay, xid, win_decor_atom);
			XSync (xdisplay, FALSE);
			gdk_error_trap_pop ();
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
	    if (action == toolkit_action_main_menu_atom)
	    {
		panel_action (xdisplay, xevent->xclient.window,
			      panel_action_main_menu_atom,
			      xevent->xclient.data.l[1]);
	    }
	    else if (action == toolkit_action_run_dialog_atom)
	    {
		panel_action (xdisplay, xevent->xclient.window,
			      panel_action_run_dialog_atom,
			      xevent->xclient.data.l[1]);
	    }
	    else if (action == toolkit_action_window_menu_atom)
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
	    static event_callback button_callback[3] = {
		close_button_event,
		max_button_event,
		min_button_event
	    };
	    decor_t *d = g_object_get_data (G_OBJECT (win), "decor");

	    if (d->decorated)
	    {
		gint i, j;

		for (i = 0; i < 3; i++)
		    for (j = 0; j < 3; j++)
			if (d->event_windows[i][j] == xevent->xany.window)
			    (*callback[i][j]) (win, xevent);

		for (i = 0; i < 3; i++)
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

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = GDK_DISPLAY_XDISPLAY (gdkdisplay);

    switch (xevent->type) {
    case SelectionRequest:
	handle_selection_request (xdisplay, xevent);
	break;
    case SelectionClear:
	handle_selection_clear (xdisplay, xevent);
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
    GtkStyle      *style;
    decor_color_t spot_color;

    style = gtk_widget_get_style (widget);
    gtk_style_attach (style, widget->window);

    spot_color.r = style->bg[GTK_STATE_SELECTED].red   / 65535.0;
    spot_color.g = style->bg[GTK_STATE_SELECTED].green / 65535.0;
    spot_color.b = style->bg[GTK_STATE_SELECTED].blue  / 65535.0;

    shade (&spot_color, &_title_color[0], 1.05);
    shade (&_title_color[0], &_title_color[1], 0.85);
}

/* to save some memory, value is specific to current decorations */
#define CORNER_REDUCTION 3

static void
draw_simple_shape (Display	   *xdisplay,
		   Pixmap	   pixmap,
		   Picture	   picture,
		   int		   width,
		   int		   height,
		   decor_context_t *c,
		   void		   *closure)
{
    static XRenderColor clear = { 0x0000, 0x0000, 0x0000, 0x0000 };
    static XRenderColor white = { 0xffff, 0xffff, 0xffff, 0xffff };

    XRenderFillRectangle (xdisplay, PictOpSrc, picture, &clear,
			  0,
			  0,
			  width,
			  height);
    XRenderFillRectangle (xdisplay, PictOpSrc, picture, &white,
			  c->left_space,
			  c->top_space,
			  width - c->left_space - c->right_space,
			  height - c->top_space - c->bottom_space);
}

static void
draw_border_shape (Display	   *xdisplay,
		   Pixmap	   pixmap,
		   Picture	   picture,
		   int		   width,
		   int		   height,
		   decor_context_t *c,
		   void		   *closure)
{
    GdkScreen   *screen;
    GdkColormap *colormap;
    decor_t     d;
    double	save_decoration_alpha;

    memset (&d, 0, sizeof (d));

    d.pixmap = gdk_pixmap_foreign_new_for_display (gdk_display_get_default (),
						   pixmap);
    d.width  = width;
    d.height = height;
    d.active = TRUE;
    d.draw   = theme_draw_window_decoration;

    screen   = gdk_display_get_default_screen (gdk_display_get_default ());
    colormap = gdk_screen_get_rgba_colormap (screen);

    gdk_drawable_set_colormap (d.pixmap, colormap);

    /* create shadow from opaque decoration */
    save_decoration_alpha = decoration_alpha;
    decoration_alpha = 1.0;

    (*d.draw) (&d);

    decoration_alpha = save_decoration_alpha;

    gdk_pixmap_unref (d.pixmap);
}

static int
update_shadow (void)
{
    decor_shadow_options_t opt;
    Display		   *xdisplay = gdk_display;
    GdkDisplay		   *display = gdk_display_get_default ();
    GdkScreen		   *screen = gdk_display_get_default_screen (display);

    opt.shadow_radius  = shadow_radius;
    opt.shadow_opacity = shadow_opacity;

    memcpy (opt.shadow_color, shadow_color, sizeof (shadow_color));

    opt.shadow_offset_x = shadow_offset_x;
    opt.shadow_offset_y = shadow_offset_y;

    if (shadow)
	decor_destroy_shadow (xdisplay, shadow);

    shadow = decor_create_shadow (xdisplay,
				  gdk_x11_screen_get_xscreen (screen),
				  1, 1,
				  0,
				  0,
				  0,
				  0,
				  &opt,
				  &shadow_context,
				  draw_simple_shape,
				  (void *) &shadow_context);

    if (shadow_pattern)
    {
	cairo_pattern_destroy (shadow_pattern);
	shadow_pattern = NULL;
    }

    if (border_shadow)
    {
	decor_destroy_shadow (xdisplay, border_shadow);
	border_shadow = NULL;
    }

    border_shadow = decor_create_shadow (xdisplay,
					 gdk_x11_screen_get_xscreen (screen),
					 1, 1,
					 _win_extents.left,
					 _win_extents.right,
					 _win_extents.top + titlebar_height,
					 _win_extents.bottom,
					 &opt,
					 &window_context,
					 draw_border_shape,
					 (void *) &window_context);

    if (border_shadow && border_shadow->pixmap)
    {
	cairo_surface_t   *surface;
	cairo_t		  *cr;
	Pixmap		  pixmap = border_shadow->pixmap;
	int		  width = border_shadow->width;
	int		  height = border_shadow->height;
	XRenderPictFormat *format;
	Screen		  *xscreen = gdk_x11_screen_get_xscreen (screen);

	format = XRenderFindStandardFormat (xdisplay, PictStandardARGB32);
	surface =
	    cairo_xlib_surface_create_with_xrender_format (xdisplay,
							   pixmap,
							   xscreen,
							   format,
							   width,
							   height);
	cr = cairo_create (surface);
	cairo_surface_destroy (surface);
	shadow_pattern =
	    cairo_pattern_create_for_surface (cairo_get_target (cr));
	cairo_pattern_set_filter (shadow_pattern, CAIRO_FILTER_NEAREST);
	cairo_destroy (cr);
    }

    return 1;
}

static void
style_changed (GtkWidget *widget)
{
    GdkDisplay *gdkdisplay;
    GdkScreen  *gdkscreen;
    WnckScreen *screen;
    GList      *windows;

    gdkdisplay = gdk_display_get_default ();
    gdkscreen  = gdk_display_get_default_screen (gdkdisplay);
    screen     = wnck_screen_get_default ();

    update_style (widget);

    update_default_decorations (gdkscreen);

    if (minimal)
	return;

    windows = wnck_screen_get_windows (screen);
    while (windows != NULL)
    {
	decor_t *d = g_object_get_data (G_OBJECT (windows->data), "decor");

	if (d->decorated)
	{
	    /* force size update */
	    d->width = d->height = 0;

	    update_window_decoration_size (WNCK_WINDOW (windows->data));
	    update_event_windows (WNCK_WINDOW (windows->data));
	}
	windows = windows->next;
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

static void
titlebar_font_changed (GConfClient *client)
{
    gchar *str;

    str = gconf_client_get_string (client,
				   COMPIZ_TITLEBAR_FONT_KEY,
				   NULL);
    if (!str)
	str = g_strdup ("Sans Bold 12");

    if (titlebar_font)
	pango_font_description_free (titlebar_font);

    titlebar_font = pango_font_description_from_string (str);

    g_free (str);
}

static void
double_click_titlebar_changed (GConfClient *client)
{
    gchar *action;

    double_click_action = DOUBLE_CLICK_MAXIMIZE;

    action = gconf_client_get_string (client,
				      COMPIZ_DOUBLE_CLICK_TITLEBAR_KEY,
				      NULL);
    if (action)
    {
	if (strcmp (action, "toggle_shade") == 0)
	    double_click_action = DOUBLE_CLICK_SHADE;
	else if (strcmp (action, "toggle_maximize") == 0)
	    double_click_action = DOUBLE_CLICK_MAXIMIZE;

	g_free (action);
    }
}

static gint
calc_titlebar_height (gint text_height)
{
    return (text_height < 17) ? 17 : text_height;
}

#ifdef USE_METACITY
static gint
meta_calc_titlebar_height (gint text_height)
{
    MetaTheme *theme;
    gint      top_height, bottom_height, left_width, right_width;

    theme = meta_theme_get_current ();

    meta_theme_get_frame_borders (theme,
				  META_FRAME_TYPE_NORMAL,
				  text_height, 0,
				  &top_height,
				  &bottom_height,
				  &left_width,
				  &right_width);

    return top_height - _win_extents.top;
}
#endif

static void
update_titlebar_font (void)
{
    const PangoFontDescription *font_desc;
    PangoFontMetrics	       *metrics;
    PangoLanguage	       *lang;

    font_desc = get_titlebar_font ();
    if (!font_desc)
    {
	GtkStyle *default_style;

	default_style = gtk_widget_get_default_style ();
	font_desc = default_style->font_desc;
    }

    pango_context_set_font_description (pango_context, font_desc);

    lang    = pango_context_get_language (pango_context);
    metrics = pango_context_get_metrics (pango_context, font_desc, lang);

    text_height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
				pango_font_metrics_get_descent (metrics));

    titlebar_height = (*theme_calc_titlebar_height) (text_height);

    pango_font_metrics_unref (metrics);
}

static gboolean
shadow_settings_changed (GConfClient *client)
{
    double   radius, opacity;
    int      offset;
    gchar    *color;
    gboolean changed = FALSE;

    radius = gconf_client_get_float (client,
				     COMPIZ_SHADOW_RADIUS_KEY,
				     NULL);
    radius = MAX (0.0, MIN (radius, 48.0));
    if (shadow_radius != radius)
    {
	shadow_radius = radius;
	changed = TRUE;
    }

    opacity = gconf_client_get_float (client,
				      COMPIZ_SHADOW_OPACITY_KEY,
				      NULL);
    opacity = MAX (0.0, MIN (opacity, 6.0));
    if (shadow_opacity != opacity)
    {
	shadow_opacity = opacity;
	changed = TRUE;
    }

    color = gconf_client_get_string (client,
				     COMPIZ_SHADOW_COLOR_KEY,
				     NULL);
    if (color)
    {
	int c[4];

	if (sscanf (color, "#%2x%2x%2x%2x", &c[0], &c[1], &c[2], &c[3]) == 4)
	{
	    shadow_color[0] = c[0] << 8 | c[0];
	    shadow_color[1] = c[1] << 8 | c[1];
	    shadow_color[2] = c[2] << 8 | c[2];
	    changed = TRUE;
	}

	g_free (color);
    }

    offset = gconf_client_get_int (client,
				   COMPIZ_SHADOW_OFFSET_X_KEY,
				   NULL);
    offset = MAX (-16, MIN (offset, 16));
    if (shadow_offset_x != offset)
    {
	shadow_offset_x = offset;
	changed = TRUE;
    }

    offset = gconf_client_get_int (client,
				   COMPIZ_SHADOW_OFFSET_Y_KEY,
				   NULL);
    offset = MAX (-16, MIN (offset, 16));
    if (shadow_offset_y != offset)
    {
	shadow_offset_y = offset;
	changed = TRUE;
    }

    return changed;
}

static void
bell_settings_changed (GConfClient *client)
{
    gboolean audible, visual, fullscreen;
    gchar    *type;

    audible = gconf_client_get_bool (client,
				     META_AUDIBLE_BELL_KEY,
				     NULL);

    visual = gconf_client_get_bool (client,
				    META_VISUAL_BELL_KEY,
				    NULL);

    type = gconf_client_get_string (client,
				    META_VISUAL_BELL_TYPE_KEY,
				    NULL);

    if (type && strcmp (type, "fullscreen") == 0)
	fullscreen = TRUE;
    else
	fullscreen = FALSE;

    g_free (type);

    gconf_client_set_bool (client,
			   COMPIZ_AUDIBLE_BELL_KEY,
			   audible,
			   NULL);

    gconf_client_set_bool (client,
			   COMPIZ_VISUAL_BELL_KEY,
			   visual,
			   NULL);

    gconf_client_set_bool (client,
			   COMPIZ_FULLSCREEN_VISUAL_BELL_KEY,
			   fullscreen,
			   NULL);
}

static gboolean
theme_changed (GConfClient *client)
{

#ifdef USE_METACITY
    gboolean use_meta_theme;

    use_meta_theme = gconf_client_get_bool (client,
					    USE_META_THEME_KEY,
					    NULL);

    if (use_meta_theme)
    {
	gchar *theme;

	theme = gconf_client_get_string (client,
					 META_THEME_KEY,
					 NULL);

	if (theme)
	{
	    meta_theme_set_current (theme, TRUE);
	    if (!meta_theme_get_current ())
		use_meta_theme = FALSE;

	    g_free (theme);
	}
	else
	{
	    use_meta_theme = FALSE;
	}
    }

    if (use_meta_theme)
    {
	theme_draw_window_decoration	= meta_draw_window_decoration;
	theme_calc_decoration_size	= meta_calc_decoration_size;
	theme_calc_titlebar_height	= meta_calc_titlebar_height;
	theme_get_event_window_position = meta_get_event_window_position;
	theme_get_button_position	= meta_get_button_position;
    }
    else
    {
	theme_draw_window_decoration	= draw_window_decoration;
	theme_calc_decoration_size	= calc_decoration_size;
	theme_calc_titlebar_height	= calc_titlebar_height;
	theme_get_event_window_position = get_event_window_position;
	theme_get_button_position	= get_button_position;
    }

    return TRUE;
#else
    theme_draw_window_decoration    = draw_window_decoration;
    theme_calc_decoration_size	    = calc_decoration_size;
    theme_calc_titlebar_height	    = calc_titlebar_height;
    theme_get_event_window_position = get_event_window_position;
    theme_get_button_position	    = get_button_position;

    return FALSE;
#endif

}

static gboolean
theme_opacity_changed (GConfClient *client)
{

#ifdef USE_METACITY
    gboolean shade_opacity, changed = FALSE;
    gdouble  opacity;

    opacity = gconf_client_get_float (client,
				    META_THEME_OPACITY_KEY,
				    NULL);

    if (opacity != meta_opacity)
    {
	meta_opacity = opacity;
	changed = TRUE;
    }

    if (opacity < 1.0)
    {
	shade_opacity = gconf_client_get_bool (client,
					     META_THEME_SHADE_OPACITY_KEY,
					     NULL);

	if (shade_opacity != meta_shade_opacity)
	{
	    meta_shade_opacity = shade_opacity;
	    changed = TRUE;
	}
    }

    opacity = gconf_client_get_float (client,
				    META_THEME_ACTIVE_OPACITY_KEY,
				    NULL);

    if (opacity != meta_active_opacity)
    {
	meta_active_opacity = opacity;
	changed = TRUE;
    }

    if (opacity < 1.0)
    {
	shade_opacity =
	    gconf_client_get_bool (client,
				   META_THEME_ACTIVE_SHADE_OPACITY_KEY,
				   NULL);

	if (shade_opacity != meta_active_shade_opacity)
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

static void
value_changed (GConfClient *client,
	       const gchar *key,
	       GConfValue  *value,
	       void        *data)
{
    gboolean changed = FALSE;

    if (strcmp (key, COMPIZ_USE_SYSTEM_FONT_KEY) == 0)
    {
	if (gconf_client_get_bool (client,
				   COMPIZ_USE_SYSTEM_FONT_KEY,
				   NULL) != use_system_font)
	{
	    use_system_font = !use_system_font;
	    changed = TRUE;
	}
    }
    else if (strcmp (key, COMPIZ_TITLEBAR_FONT_KEY) == 0)
    {
	titlebar_font_changed (client);
	changed = !use_system_font;
    }
    else if (strcmp (key, COMPIZ_DOUBLE_CLICK_TITLEBAR_KEY) == 0)
    {
	double_click_titlebar_changed (client);
    }
    else if (strcmp (key, COMPIZ_SHADOW_RADIUS_KEY)   == 0 ||
	     strcmp (key, COMPIZ_SHADOW_OPACITY_KEY)  == 0 ||
	     strcmp (key, COMPIZ_SHADOW_OFFSET_X_KEY) == 0 ||
	     strcmp (key, COMPIZ_SHADOW_OFFSET_Y_KEY) == 0 ||
	     strcmp (key, COMPIZ_SHADOW_COLOR_KEY) == 0)
    {
	if (shadow_settings_changed (client))
	    changed = TRUE;
    }
    else if (strcmp (key, META_AUDIBLE_BELL_KEY)     == 0 ||
	     strcmp (key, META_VISUAL_BELL_KEY)      == 0 ||
	     strcmp (key, META_VISUAL_BELL_TYPE_KEY) == 0)
    {
	bell_settings_changed (client);
    }
    else if (strcmp (key, USE_META_THEME_KEY) == 0 ||
	     strcmp (key, META_THEME_KEY) == 0)
    {
	if (theme_changed (client))
	    changed = TRUE;
    }
    else if (strcmp (key, META_THEME_OPACITY_KEY)	       == 0 ||
	     strcmp (key, META_THEME_SHADE_OPACITY_KEY)	       == 0 ||
	     strcmp (key, META_THEME_ACTIVE_OPACITY_KEY)       == 0 ||
	     strcmp (key, META_THEME_ACTIVE_SHADE_OPACITY_KEY) == 0)
    {
	if (theme_opacity_changed (client))
	    changed = TRUE;
    }

    if (changed)
    {
	GdkDisplay *gdkdisplay;
	GdkScreen  *gdkscreen;
	WnckScreen *screen = data;
	GList	   *windows;

	gdkdisplay = gdk_display_get_default ();
	gdkscreen  = gdk_display_get_default_screen (gdkdisplay);

	update_titlebar_font ();
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
		d->width = d->height = 0;

#ifdef USE_METACITY
		if (d->draw == draw_window_decoration ||
		    d->draw == meta_draw_window_decoration)
		    d->draw = theme_draw_window_decoration;
#endif

		update_window_decoration_size (WNCK_WINDOW (windows->data));
		update_event_windows (WNCK_WINDOW (windows->data));
	    }
	    windows = windows->next;
	}
    }
}

static gboolean
init_settings (WnckScreen *screen)
{
    GtkSettings	*settings;
    GConfClient	*gconf;
    GdkScreen   *gdkscreen;
    GdkColormap *colormap;

    gconf = gconf_client_get_default ();

    gconf_client_add_dir (gconf,
			  GCONF_DIR,
			  GCONF_CLIENT_PRELOAD_ONELEVEL,
			  NULL);

    gconf_client_add_dir (gconf,
			  METACITY_GCONF_DIR,
			  GCONF_CLIENT_PRELOAD_ONELEVEL,
			  NULL);

    gconf_client_add_dir (gconf,
			  COMPIZ_GCONF_DIR1,
			  GCONF_CLIENT_PRELOAD_ONELEVEL,
			  NULL);

    gconf_client_add_dir (gconf,
			  COMPIZ_GCONF_DIR2,
			  GCONF_CLIENT_PRELOAD_ONELEVEL,
			  NULL);

    gconf_client_add_dir (gconf,
			  COMPIZ_GCONF_DIR3,
			  GCONF_CLIENT_PRELOAD_ONELEVEL,
			  NULL);

    g_signal_connect (G_OBJECT (gconf),
		      "value_changed",
		      G_CALLBACK (value_changed),
		      screen);

    style_window = gtk_window_new (GTK_WINDOW_POPUP);

    gdkscreen = gdk_display_get_default_screen (gdk_display_get_default ());
    colormap = gdk_screen_get_rgba_colormap (gdkscreen);
    if (colormap)
	gtk_widget_set_colormap (style_window, colormap);

    gtk_widget_realize (style_window);

    g_signal_connect_object (style_window, "style-set",
			     G_CALLBACK (style_changed),
			     0, 0);

    settings = gtk_widget_get_settings (style_window);

    g_object_get (G_OBJECT (settings), "gtk-double-click-time",
		  &double_click_timeout, NULL);

    pango_context = gtk_widget_create_pango_context (style_window);

    use_system_font = gconf_client_get_bool (gconf,
					     COMPIZ_USE_SYSTEM_FONT_KEY,
					     NULL);

    theme_changed (gconf);
    theme_opacity_changed (gconf);
    update_style (style_window);
    titlebar_font_changed (gconf);
    update_titlebar_font ();
    double_click_titlebar_changed (gconf);
    shadow_settings_changed (gconf);
    bell_settings_changed (gconf);
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
    gint       i, j;
    gboolean   replace = FALSE;

    program_name = argv[0];

    gtk_init (&argc, &argv);

    for (i = 0; i < argc; i++)
    {
	if (strcmp (argv[i], "--minimal") == 0)
	{
	    minimal = TRUE;
	}
	else  if (strcmp (argv[i], "--replace") == 0)
	{
	    replace = TRUE;
	}
	else  if (strcmp (argv[i], "--help") == 0)
	{
	    fprintf (stderr, "%s [--minimal] [--replace] [--help]\n",
		     program_name);
	    return 0;
	}
    }

    gdkdisplay = gdk_display_get_default ();
    xdisplay   = gdk_x11_display_get_xdisplay (gdkdisplay);
    gdkscreen  = gdk_display_get_default_screen (gdkdisplay);

    frame_window_atom	= XInternAtom (xdisplay, "_NET_FRAME_WINDOW", FALSE);
    win_decor_atom	= XInternAtom (xdisplay, "_NET_WINDOW_DECOR", FALSE);
    wm_move_resize_atom = XInternAtom (xdisplay, "_NET_WM_MOVERESIZE", FALSE);
    restack_window_atom = XInternAtom (xdisplay, "_NET_RESTACK_WINDOW", FALSE);
    select_window_atom	= XInternAtom (xdisplay, "_SWITCH_SELECT_WINDOW",
				       FALSE);
    mwm_hints_atom	= XInternAtom (xdisplay, "_MOTIF_WM_HINTS", FALSE);

    toolkit_action_atom			  =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION", FALSE);
    toolkit_action_main_menu_atom	  =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_MAIN_MENU", FALSE);
    toolkit_action_run_dialog_atom	  =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_RUN_DIALOG", FALSE);
    toolkit_action_window_menu_atom	  =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_WINDOW_MENU", FALSE);
    toolkit_action_force_quit_dialog_atom =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_FORCE_QUIT_DIALOG",
		     FALSE);

    panel_action_atom		 =
	XInternAtom (xdisplay, "_GNOME_PANEL_ACTION", FALSE);
    panel_action_main_menu_atom  =
	XInternAtom (xdisplay, "_GNOME_PANEL_ACTION_MAIN_MENU", FALSE);
    panel_action_run_dialog_atom =
	XInternAtom (xdisplay, "_GNOME_PANEL_ACTION_RUN_DIALOG", FALSE);

    manager_atom   = XInternAtom (xdisplay, "MANAGER", FALSE);
    targets_atom   = XInternAtom (xdisplay, "TARGETS", FALSE);
    multiple_atom  = XInternAtom (xdisplay, "MULTIPLE", FALSE);
    timestamp_atom = XInternAtom (xdisplay, "TIMESTAMP", FALSE);
    version_atom   = XInternAtom (xdisplay, "VERSION", FALSE);
    atom_pair_atom = XInternAtom (xdisplay, "ATOM_PAIR", FALSE);

    utf8_string_atom = XInternAtom (xdisplay, "UTF8_STRING", FALSE);

    dm_name_atom = XInternAtom (xdisplay, "_NET_DM_NAME", FALSE);

    if (!acquire_dm_session (xdisplay, 0, replace))
	return 1;

    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
	{
	    if (cursor[i][j].shape != XC_left_ptr)
		cursor[i][j].cursor =
		    XCreateFontCursor (xdisplay, cursor[i][j].shape);
	}
    }

    frame_table = g_hash_table_new (NULL, NULL);

    if (!create_tooltip_window ())
    {
	fprintf (stderr, "%s, Couldn't create tooltip window\n", argv[0]);
	return 1;
    }

    screen = wnck_screen_get_default ();

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

    set_dm_check_hint (gdk_display_get_default_screen (gdkdisplay));

    update_default_decorations (gdkscreen);

    gtk_main ();

    return 0;
}
