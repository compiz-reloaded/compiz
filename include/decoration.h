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

#ifdef  __cplusplus
extern "C" {
#endif

#define DECOR_SUPPORTING_DM_CHECK_ATOM_NAME     "_COMPIZ_SUPPORTING_DM_CHECK"
#define DECOR_BARE_ATOM_NAME                    "_COMPIZ_WINDOW_DECOR_BARE"
#define DECOR_NORMAL_ATOM_NAME                  "_COMPIZ_WINDOW_DECOR_NORMAL"
#define DECOR_ACTIVE_ATOM_NAME                  "_COMPIZ_WINDOW_DECOR_ACTIVE"
#define DECOR_WINDOW_ATOM_NAME                  "_COMPIZ_WINDOW_DECOR"
#define DECOR_BLUR_ATOM_NAME                    "_COMPIZ_WM_WINDOW_BLUR_DECOR"
#define DECOR_SWITCH_WINDOW_ATOM_NAME           "_COMPIZ_SWITCH_SELECT_WINDOW"
#define DECOR_SWITCH_FOREGROUND_COLOR_ATOM_NAME "_COMPIZ_SWITCH_FOREGROUND_COLOR"

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

#define STRETCH_X (1 << 0)
#define STRETCH_Y (1 << 1)

#define XX_MASK (1 << 16)
#define XY_MASK (1 << 17)
#define YX_MASK (1 << 18)
#define YY_MASK (1 << 19)

#define PAD_TOP    (1 << 0)
#define PAD_BOTTOM (1 << 1)
#define PAD_LEFT   (1 << 2)
#define PAD_RIGHT  (1 << 3)

#define BORDER_TOP    0
#define BORDER_BOTTOM 1
#define BORDER_LEFT   2
#define BORDER_RIGHT  3

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
    int            stretch;
    decor_matrix_t m;
} decor_quad_t;

typedef struct _decor_extents {
    int left;
    int right;
    int top;
    int bottom;
} decor_extents_t;

typedef struct _decor_context {
    decor_extents_t extents;

    int left_space;
    int right_space;
    int top_space;
    int bottom_space;

    int left_corner_space;
    int right_corner_space;
    int top_corner_space;
    int bottom_corner_space;
} decor_context_t;

typedef struct _decor_box {
    int x1;
    int y1;
    int x2;
    int y2;

    int pad;
} decor_box_t;

typedef struct _decor_layout {
    int width;
    int height;

    decor_box_t left;
    decor_box_t right;
    decor_box_t top;
    decor_box_t bottom;

    int rotation;
} decor_layout_t;

typedef struct _decor_shadow_options {
    double	   shadow_radius;
    double	   shadow_opacity;
    unsigned short shadow_color[3];
    int		   shadow_offset_x;
    int		   shadow_offset_y;
} decor_shadow_options_t;

typedef struct _decor_shadow {
    int     ref_count;
    Pixmap  pixmap;
    Picture picture;
    int	    width;
    int	    height;
} decor_shadow_t;

typedef void (*decor_draw_func_t) (Display	   *xdisplay,
				   Pixmap	   pixmap,
				   Picture	   picture,
				   int		   width,
				   int		   height,
				   decor_context_t *context,
				   void		   *closure);

#define BASE_PROP_SIZE 12
#define QUAD_PROP_SIZE 9
#define N_QUADS_MAX    24

int
decor_version (void);

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
decor_property_get_version (long *data);

int
decor_property_to_quads (long		 *data,
			 int		 size,
			 Pixmap		 *pixmap,
			 decor_extents_t *input,
			 decor_extents_t *max_input,
			 int		 *min_width,
			 int		 *min_height,
			 decor_quad_t    *quad);

void
decor_region_to_blur_property (long   *data,
			       int    threshold,
			       int    filter,
			       int    width,
			       int    height,
			       Region topRegion,
			       int    topOffset,
			       Region bottomRegion,
			       int    bottomOffset,
			       Region leftRegion,
			       int    leftOffset,
			       Region rightRegion,
			       int    rightOffset);

void
decor_apply_gravity (int gravity,
		     int x,
		     int y,
		     int width,
		     int height,
		     int *return_x,
		     int *return_y);

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
			 int	      splitY,
			 int	      splitGravity,
			 double	      x0,
			 double	      y0,
			 int	      rotation);

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
			  int	       splitX,
			  int	       splitGravity,
			  double       x0,
			  double       y0);

int
decor_set_lSrS_window_quads (decor_quad_t    *q,
			     decor_context_t *c,
			     decor_layout_t  *l);

int
decor_set_lSrStSbS_window_quads (decor_quad_t    *q,
				 decor_context_t *c,
				 decor_layout_t  *l);

int
decor_set_lSrStXbS_window_quads (decor_quad_t    *q,
				 decor_context_t *c,
				 decor_layout_t  *l,
				 int		 top_stretch_offset);

int
decor_set_lSrStSbX_window_quads (decor_quad_t    *q,
				 decor_context_t *c,
				 decor_layout_t  *l,
				 int		 bottom_stretch_offset);

int
decor_set_lXrXtXbX_window_quads (decor_quad_t    *q,
				 decor_context_t *c,
				 decor_layout_t  *l,
				 int		 left_stretch_offset,
				 int		 right_stretch_offset,
				 int		 top_stretch_offset,
				 int		 bottom_stretch_offset);

decor_shadow_t *
decor_shadow_create (Display		    *xdisplay,
		     Screen		    *screen,
		     int		    width,
		     int		    height,
		     int		    left,
		     int		    right,
		     int		    top,
		     int		    bottom,
		     int		    solid_left,
		     int		    solid_right,
		     int		    solid_top,
		     int		    solid_bottom,
		     decor_shadow_options_t *opt,
		     decor_context_t	    *context,
		     decor_draw_func_t      draw,
		     void		    *closure);

void
decor_shadow_destroy (Display	     *xdisplay,
		      decor_shadow_t *shadow);

void
decor_shadow_reference (decor_shadow_t *shadow);

void
decor_shadow (Display	     *xdisplay,
		      decor_shadow_t *shadow);

void
decor_draw_simple (Display	   *xdisplay,
		   Pixmap	   pixmap,
		   Picture	   picture,
		   int		   width,
		   int		   height,
		   decor_context_t *c,
		   void		   *closure);

void
decor_get_default_layout (decor_context_t *c,
			  int	          width,
			  int	          height,
			  decor_layout_t  *layout);

void
decor_get_best_layout (decor_context_t *c,
		       int	       width,
		       int	       height,
		       decor_layout_t  *layout);

void
decor_fill_picture_extents_with_shadow (Display	        *xdisplay,
					decor_shadow_t  *shadow,
					decor_context_t *context,
					Picture	        picture,
					decor_layout_t  *layout);

void
decor_blend_transform_picture (Display	       *xdisplay,
			       decor_context_t *context,
			       Picture	       src,
			       int	       xSrc,
			       int	       ySrc,
			       Picture	       dst,
			       decor_layout_t  *layout,
			       Region	       region,
			       unsigned short  alpha,
			       int	       shade_alpha);

void
decor_blend_border_picture (Display	    *xdisplay,
			    decor_context_t *context,
			    Picture	    src,
			    int	            xSrc,
			    int	            ySrc,
			    Picture	    dst,
			    decor_layout_t  *layout,
			    unsigned int    border,
			    Region	    region,
			    unsigned short  alpha,
			    int	            shade_alpha,
			    int             ignore_src_alpha);

#define DECOR_ACQUIRE_STATUS_SUCCESS	      0
#define DECOR_ACQUIRE_STATUS_FAILED	      1
#define DECOR_ACQUIRE_STATUS_OTHER_DM_RUNNING 2

int
decor_acquire_dm_session (Display    *xdisplay,
			  int	     screen,
			  const char *name,
			  int	     replace_current_dm,
			  Time	     *timestamp);

void
decor_set_dm_check_hint (Display *xdisplay,
			 int	 screen);

#define DECOR_SELECTION_KEEP    0
#define DECOR_SELECTION_GIVE_UP 1

int
decor_handle_selection_clear (Display *xdisplay,
			      XEvent  *xevent,
			      int     screen);

void
decor_handle_selection_request (Display *xdisplay,
				XEvent  *event,
				Time    dm_sn_timestamp);

#ifdef  __cplusplus
}
#endif

#endif
