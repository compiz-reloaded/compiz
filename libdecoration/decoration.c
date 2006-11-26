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

#include <decoration.h>

/*
  decoration property
  -------------------

  data[0] = version

  data[1] = pixmap

  data[2] = input left
  data[3] = input right
  data[4] = input top
  data[5] = input bottom

  data[6] = input left when maximized
  data[7] = input right when maximized
  data[8] = input top when maximized
  data[9] = input bottom when maximized

  data[10] = min width
  data[11] = min height

  flags

  1st to 4nd bit p1 gravity, 5rd to 8th bit p2 gravity,
  9rd and 10th bit alignment, 11rd and 12th bit clamp,
  13th bit XX, 14th bit XY, 15th bit YX, 16th bit YY.

  data[11 + n * 9 + 1] = flags
  data[11 + n * 9 + 2] = p1 x
  data[11 + n * 9 + 3] = p1 y
  data[11 + n * 9 + 4] = p2 x
  data[11 + n * 9 + 5] = p2 y
  data[11 + n * 9 + 6] = widthMax
  data[11 + n * 9 + 7] = heightMax
  data[11 + n * 9 + 8] = x0
  data[11 + n * 9 + 9] = y0
 */
void
decor_quads_to_property (long		 *data,
			 Pixmap		 pixmap,
			 decor_extents_t *input,
			 decor_extents_t *max_input,
			 int		 min_width,
			 int		 min_height,
			 decor_quad_t    *quad,
			 int		 nQuad)
{
    *data++ = DECOR_INTERFACE_VERSION;

    memcpy (data++, &pixmap, sizeof (Pixmap));

    *data++ = input->left;
    *data++ = input->right;
    *data++ = input->top;
    *data++ = input->bottom;

    *data++ = max_input->left;
    *data++ = max_input->right;
    *data++ = max_input->top;
    *data++ = max_input->bottom;

    *data++ = min_width;
    *data++ = min_height;

    while (nQuad--)
    {
	*data++ =
	    (quad->p1.gravity << 0)    |
	    (quad->p2.gravity << 4)    |
	    (quad->align      << 8)    |
	    (quad->clamp      << 10)   |
	    (quad->m.xx ? XX_MASK : 0) |
	    (quad->m.xy ? XY_MASK : 0) |
	    (quad->m.yx ? YX_MASK : 0) |
	    (quad->m.yy ? YY_MASK : 0);

	*data++ = quad->p1.x;
	*data++ = quad->p1.y;
	*data++ = quad->p2.x;
	*data++ = quad->p2.y;
	*data++ = quad->max_width;
	*data++ = quad->max_height;
	*data++ = quad->m.x0;
	*data++ = quad->m.y0;

	quad++;
    }
}

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
			  double       y0)
{
    int dx, nQuad = 0;

    dx = (left_corner - right_corner) >> 1;

    q->p1.x	  = -left;
    q->p1.y	  = top;
    q->p1.gravity = gravity | GRAVITY_WEST;
    q->p2.x	  = dx;
    q->p2.y	  = bottom;
    q->p2.gravity = gravity;
    q->max_width  = left + left_corner;
    q->max_height = SHRT_MAX;
    q->align	  = ALIGN_LEFT;
    q->clamp	  = 0;
    q->m.xx	  = 1.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = x0;
    q->m.y0	  = y0;

    q++; nQuad++;

    q->p1.x	  = left_corner;
    q->p1.y	  = top;
    q->p1.gravity = gravity | GRAVITY_WEST;
    q->p2.x	  = -right_corner;
    q->p2.y	  = bottom;
    q->p2.gravity = gravity | GRAVITY_EAST;
    q->max_width  = SHRT_MAX;
    q->max_height = SHRT_MAX;
    q->align	  = 0;
    q->clamp	  = 0;
    q->m.xx	  = 0.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = x0 + left + left_corner;
    q->m.y0	  = y0;

    q++; nQuad++;

    q->p1.x	  = dx;
    q->p1.y	  = top;
    q->p1.gravity = gravity;
    q->p2.x	  = right;
    q->p2.y	  = bottom;
    q->p2.gravity = gravity | GRAVITY_EAST;
    q->max_width  = right_corner + right;
    q->max_height = SHRT_MAX;
    q->align	  = ALIGN_RIGHT;
    q->clamp	  = 0;
    q->m.xx	  = 1.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = x0 + width;
    q->m.y0	  = y0;

    nQuad++;

    return nQuad;
}

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
			 double	      y0)
{
    int dy, nQuad = 0;

    dy = (top_corner - bottom_corner) >> 1;

    q->p1.x	  = left;
    q->p1.y	  = -top;
    q->p1.gravity = gravity | GRAVITY_NORTH;
    q->p2.x	  = right;
    q->p2.y	  = dy;
    q->p2.gravity = gravity;
    q->max_width  = SHRT_MAX;
    q->max_height = top + top_corner;
    q->align	  = ALIGN_TOP;
    q->clamp	  = CLAMP_VERT;
    q->m.xx	  = 1.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = x0;
    q->m.y0	  = y0;

    q++; nQuad++;

    q->p1.x	  = left;
    q->p1.y	  = top_corner;
    q->p1.gravity = gravity | GRAVITY_NORTH;
    q->p2.x	  = right;
    q->p2.y	  = -bottom_corner;
    q->p2.gravity = gravity | GRAVITY_SOUTH;
    q->max_width  = SHRT_MAX;
    q->max_height = SHRT_MAX;
    q->align	  = 0;
    q->clamp	  = CLAMP_VERT;
    q->m.xx	  = 1.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 0.0;
    q->m.x0	  = x0;
    q->m.y0	  = y0 + top + top_corner;

    q++; nQuad++;

    q->p1.x	  = left;
    q->p1.y	  = dy;
    q->p1.gravity = gravity;
    q->p2.x	  = right;
    q->p2.y	  = bottom;
    q->p2.gravity = gravity | GRAVITY_SOUTH;
    q->max_width  = SHRT_MAX;
    q->max_height = bottom_corner + bottom;
    q->align	  = ALIGN_BOTTOM;
    q->clamp	  = CLAMP_VERT;
    q->m.xx	  = 1.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = x0;
    q->m.y0	  = y0 + height;

    nQuad++;

    return nQuad;
}

int
decor_set_common_window_quads (decor_context_t *c,
			       decor_quad_t    *q,
			       int	       width,
			       int	       height)
{
    int n, nQuad = 0;

    /* left quads */
    n = decor_set_vert_quad_row (q,
				 0,
				 c->top_corner_space,
				 0,
				 c->bottom_corner_space,
				 -c->left_space,
				 0,
				 GRAVITY_WEST,
				 height - c->top_space - c->titlebar_height -
				 c->bottom_space,
				 0.0,
				 c->top_space + c->titlebar_height + 1.0);

    q += n; nQuad += n;

    /* right quads */
    n = decor_set_vert_quad_row (q,
				 0,
				 c->top_corner_space,
				 0,
				 c->bottom_corner_space,
				 0,
				 c->right_space,
				 GRAVITY_EAST,
				 height - c->top_space - c->titlebar_height -
				 c->bottom_space,
				 width - c->right_space,
				 c->top_space + c->titlebar_height + 1.0);

    q += n; nQuad += n;

    /* bottom quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  c->left_corner_space,
				  c->right_space,
				  c->right_corner_space,
				  0,
				  c->bottom_space,
				  GRAVITY_SOUTH,
				  width,
				  0.0,
				  c->top_space + c->titlebar_height +
				  c->top_corner_space +
				  c->bottom_corner_space + 2.0);

    nQuad += n;

    return nQuad;
}

int
decor_set_window_quads (decor_context_t *c,
			decor_quad_t    *q,
			int	        width,
			int	        height,
			int	        button_width)
{
    int     n, nQuad = 0;
    int     top_left, top_right, y;
    double  y0;

    top_right = button_width;
    top_left  = width - c->left_space - c->right_space - top_right - 1;

    /* special case which can happen with large shadows */
    if (c->right_corner_space > top_right || c->left_corner_space > top_left)
    {
	y  = -c->titlebar_height;
	y0 = c->top_space;

	/* top quads */
	n = decor_set_horz_quad_line (q,
				      c->left_space,
				      c->left_corner_space,
				      c->right_space,
				      c->right_corner_space,
				      -c->top_space - c->titlebar_height,
				      y,
				      GRAVITY_NORTH,
				      width,
				      0.0,
				      0.0);

	q += n; nQuad += n;
    }
    else
    {
	y  = -c->top_space - c->titlebar_height;
	y0 = 0.0;
    }

    /* 3 top/titlebar quads */
    q->p1.x	  = -c->left_space;
    q->p1.y	  = y;
    q->p1.gravity = GRAVITY_NORTH | GRAVITY_WEST;
    q->p2.x	  = -top_right;
    q->p2.y	  = 0;
    q->p2.gravity = GRAVITY_NORTH | GRAVITY_EAST;
    q->max_width  = c->left_space + top_left;
    q->max_height = SHRT_MAX;
    q->align	  = ALIGN_LEFT;
    q->clamp	  = 0;
    q->m.xx	  = 1.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = 0.0;
    q->m.y0	  = y0;

    q++; nQuad++;

    q->p1.x	  = top_left;
    q->p1.y	  = y;
    q->p1.gravity = GRAVITY_NORTH | GRAVITY_WEST;
    q->p2.x	  = -top_right;
    q->p2.y	  = 0;
    q->p2.gravity = GRAVITY_NORTH | GRAVITY_EAST;
    q->max_width  = SHRT_MAX;
    q->max_height = SHRT_MAX;
    q->align	  = 0;
    q->clamp	  = 0;
    q->m.xx	  = 0.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = c->left_space + top_left;
    q->m.y0	  = y0;

    q++; nQuad++;

    q->p1.x	  = 0;
    q->p1.y	  = y;
    q->p1.gravity = GRAVITY_NORTH | GRAVITY_WEST;
    q->p2.x	  = c->right_space;
    q->p2.y	  = 0;
    q->p2.gravity = GRAVITY_NORTH | GRAVITY_EAST;
    q->max_width  = c->right_space + top_right;
    q->max_height = SHRT_MAX;
    q->align	  = ALIGN_RIGHT;
    q->clamp	  = 0;
    q->m.xx	  = 1.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = width;
    q->m.y0	  = y0;

    q++; nQuad++;

    n = decor_set_common_window_quads (c, q, width, height);

    nQuad += n;

    return nQuad;
}

int
decor_set_no_title_window_quads (decor_context_t *c,
				 decor_quad_t    *q,
				 int	         width,
				 int	         height)
{
    int n, nQuad = 0;

    /* top quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  c->left_corner_space,
				  c->right_space,
				  c->right_corner_space,
				  -c->top_space - c->titlebar_height,
				  0,
				  GRAVITY_NORTH,
				  width,
				  0.0,
				  0.0);

    q += n; nQuad += n;

    n = decor_set_common_window_quads (c, q, width, height);

    nQuad += n;

    return nQuad;
}

int
decor_set_shadow_quads (decor_context_t *c,
			decor_quad_t    *q,
			int	        width,
			int	        height)
{
    int n, nQuad = 0;

    /* top quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  c->left_corner_space,
				  c->right_space,
				  c->right_corner_space,
				  -c->top_space,
				  0,
				  GRAVITY_NORTH,
				  width,
				  0.0,
				  0.0);

    q += n; nQuad += n;

    /* left quads */
    n = decor_set_vert_quad_row (q,
				 0,
				 c->top_corner_space,
				 0,
				 c->bottom_corner_space,
				 -c->left_space,
				 0,
				 GRAVITY_WEST,
				 height - c->top_space - c->bottom_space,
				 0.0,
				 c->top_space);

    q += n; nQuad += n;

    /* right quads */
    n = decor_set_vert_quad_row (q,
				 0,
				 c->top_corner_space,
				 0,
				 c->bottom_corner_space,
				 0,
				 c->right_space,
				 GRAVITY_EAST,
				 height - c->top_space - c->bottom_space,
				 width - c->right_space,
				 c->top_space);

    q += n; nQuad += n;

    /* bottom quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  c->left_corner_space,
				  c->right_space,
				  c->right_corner_space,
				  0,
				  c->bottom_space,
				  GRAVITY_SOUTH,
				  width,
				  0.0,
				  c->top_space + c->top_corner_space +
				  c->bottom_corner_space + 1.0);

    nQuad += n;

    return nQuad;
}
