/*
 * Copyright © 2006 Novell, Inc.
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

#ifndef _DECORATION_H
#define _DECORATION_H

#include <string.h>
#include <limits.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#define GRAVITY_WEST  (1 << 0)
#define GRAVITY_EAST  (1 << 1)
#define GRAVITY_NORTH (1 << 2)
#define GRAVITY_SOUTH (1 << 3)

#define ALIGN_LEFT   (0)
#define ALIGN_RIGHT  (1 << 0)
#define ALIGN_TOP    (0)
#define ALIGN_BOTTOM (1 << 1)

#define CLAMP_HORZ (1 << 0)
#define CLAMP_VERT (1 << 1)

#define XX_MASK (1 << 12)
#define XY_MASK (1 << 13)
#define YX_MASK (1 << 14)
#define YY_MASK (1 << 15)

typedef struct _decor_point {
    int x;
    int y;
    int gravity;
} decor_point_t;

typedef struct _decor_matrix {
    double xx; double yx;
    double xy; double yy;
    double x0; double y0;
} decor_matrix_t;

typedef struct _decor_quad {
    decor_point_t  p1;
    decor_point_t  p2;
    int		   max_width;
    int		   max_height;
    int		   align;
    int		   clamp;
    decor_matrix_t m;
} decor_quad_t;

typedef struct _decor_extents {
    int left;
    int right;
    int top;
    int bottom;
} decor_extents_t;

typedef struct _decor_context decor_context_t;

struct _decor_context {
    int left_space;
    int right_space;
    int top_space;
    int bottom_space;

    int left_corner_space;
    int right_corner_space;
    int top_corner_space;
    int bottom_corner_space;

    int titlebar_height;
};

typedef struct _decor_shadow_options {
    double	   shadow_radius;
    double	   shadow_opacity;
    unsigned short shadow_color[3];
    int		   shadow_offset_x;
    int		   shadow_offset_y;
} decor_shadow_options_t;

typedef struct _decor_shadow decor_shadow_t;

typedef void (*decor_draw_func_t) (Display *xdisplay,
				   Pixmap  pixmap,
				   Picture picture,
				   int     width,
				   int     height,
				   void    *closure);

void
decor_quads_to_property (long		 *data,
			 Pixmap		 pixmap,
			 decor_extents_t *input,
			 decor_extents_t *max_input,
			 int		 min_width,
			 int		 min_height,
			 decor_quad_t    *quad,
			 int		 nQuad);

int
decor_set_horz_quad_line (decor_quad_t *q,
			  int	       left,
			  int	       left_corner,
			  int	       right,
			  int	       right_corner,
			  int	       top,
			  int	       bottom,
			  int	       gravity,
			  int	       width,
			  double       x0,
			  double       y0);

int
decor_set_vert_quad_row (decor_quad_t *q,
			 int	      top,
			 int	      top_corner,
			 int	      bottom,
			 int	      bottom_corner,
			 int	      left,
			 int	      right,
			 int	      gravity,
			 int	      height,
			 double	      x0,
			 double	      y0);

int
decor_set_common_window_quads (decor_context_t *c,
			       decor_quad_t    *q,
			       int	       width,
			       int	       height);

int
decor_set_window_quads (decor_context_t *c,
			decor_quad_t    *q,
			int	        width,
			int	        height,
			int	        button_width);

int
decor_set_no_title_window_quads (decor_context_t *c,
				 decor_quad_t    *q,
				 int	         width,
				 int	         height);

int
decor_set_shadow_quads (decor_context_t *c,
			decor_quad_t    *q,
			int	        width,
			int	        height);

decor_shadow_t *
decor_create_shadow (Display		    *xdisplay,
		     Screen		    *screen,
		     int		    width,
		     int		    height,
		     int		    left,
		     int		    right,
		     int		    top,
		     int		    bottom,
		     decor_shadow_options_t *opt,
		     decor_context_t	    *context,
		     decor_draw_func_t      *draw,
		     void		    *closure);

void
decor_destroy_shadow (Display	     *xdisplay,
		      decor_shadow_t *shadow);


#endif
