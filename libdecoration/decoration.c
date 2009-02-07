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
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <decoration.h>

#include <X11/Xatom.h>
#include <X11/Xregion.h>

int
decor_version (void)
{
    return DECOR_INTERFACE_VERSION;
}

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
	    (quad->stretch    << 12)   |
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
decor_property_get_version (long *data)
{
    return (int) *data;
}

int
decor_property_to_quads (long		 *data,
			 int		 size,
			 Pixmap		 *pixmap,
			 decor_extents_t *input,
			 decor_extents_t *max_input,
			 int		 *min_width,
			 int		 *min_height,
			 decor_quad_t    *quad)
{
    int i, n, flags;

    if (size < BASE_PROP_SIZE + QUAD_PROP_SIZE)
	return 0;

    if (decor_property_get_version (data) != decor_version ())
	return 0;

    data++;

    memcpy (pixmap, data++, sizeof (Pixmap));

    input->left   = *data++;
    input->right  = *data++;
    input->top    = *data++;
    input->bottom = *data++;

    max_input->left   = *data++;
    max_input->right  = *data++;
    max_input->top    = *data++;
    max_input->bottom = *data++;

    *min_width  = *data++;
    *min_height = *data++;

    n = (size - BASE_PROP_SIZE) / QUAD_PROP_SIZE;

    for (i = 0; i < n; i++)
    {
	flags = *data++;

	quad->p1.gravity = (flags >> 0) & 0xf;
	quad->p2.gravity = (flags >> 4) & 0xf;

	quad->align   = (flags >> 8)  & 0x3;
	quad->clamp   = (flags >> 10) & 0x3;
	quad->stretch = (flags >> 12) & 0x3;

	quad->m.xx = (flags & XX_MASK) ? 1.0f : 0.0f;
	quad->m.xy = (flags & XY_MASK) ? 1.0f : 0.0f;
	quad->m.yx = (flags & YX_MASK) ? 1.0f : 0.0f;
	quad->m.yy = (flags & YY_MASK) ? 1.0f : 0.0f;

	quad->p1.x = *data++;
	quad->p1.y = *data++;
	quad->p2.x = *data++;
	quad->p2.y = *data++;

	quad->max_width  = *data++;
	quad->max_height = *data++;

	quad->m.x0 = *data++;
	quad->m.y0 = *data++;

	quad++;
    }

    return n;
}

static int
add_blur_boxes (long   *data,
		BoxPtr box,
		int    n_box,
		int    width,
		int    height,
		int    gravity,
		int    offset)
{
    int x1, y1, x2, y2;
    int more_gravity;
    int n = n_box;

    while (n--)
    {
	x1 = box->x1;
	y1 = box->y1;
	x2 = box->x2;
	y2 = box->y2;

	if (gravity & (GRAVITY_NORTH | GRAVITY_SOUTH))
	{
	    if (x1 > offset)
	    {
		more_gravity = GRAVITY_EAST;
		x1 -= width;
	    }
	    else
	    {
		more_gravity = GRAVITY_WEST;
	    }
	}
	else
	{
	    if (y1 > offset)
	    {
		more_gravity = GRAVITY_SOUTH;
		y1 -= height;
	    }
	    else
	    {
		more_gravity = GRAVITY_NORTH;
	    }
	}

	*data++ = gravity | more_gravity;
	*data++ = x1;
	*data++ = y1;

	if (gravity & (GRAVITY_NORTH | GRAVITY_SOUTH))
	{
	    if (x2 > offset)
	    {
		more_gravity = GRAVITY_EAST;
		x2 -= width;
	    }
	    else
	    {
		more_gravity = GRAVITY_WEST;
	    }
	}
	else
	{
	    if (y2 > offset)
	    {
		more_gravity = GRAVITY_SOUTH;
		y2 -= height;
	    }
	    else
	    {
		more_gravity = GRAVITY_NORTH;
	    }
	}

	*data++ = gravity | more_gravity;
	*data++ = x2;
	*data++ = y2;

	box++;
    }

    return n_box * 6;
}

void
decor_region_to_blur_property (long   *data,
			       int    threshold,
			       int    filter,
			       int    width,
			       int    height,
			       Region top_region,
			       int    top_offset,
			       Region bottom_region,
			       int    bottom_offset,
			       Region left_region,
			       int    left_offset,
			       Region right_region,
			       int    right_offset)
{
    *data++ = threshold;
    *data++ = filter;

    if (top_region)
	data += add_blur_boxes (data,
				top_region->rects,
				top_region->numRects,
				width, height,
				GRAVITY_NORTH,
				top_offset);

    if (bottom_region)
	data += add_blur_boxes (data,
				bottom_region->rects,
				bottom_region->numRects,
				width, height,
				GRAVITY_SOUTH,
				bottom_offset);

    if (left_region)
	data += add_blur_boxes (data,
				left_region->rects,
				left_region->numRects,
				width, height,
				GRAVITY_WEST,
				left_offset);

    if (right_region)
	data += add_blur_boxes (data,
				right_region->rects,
				right_region->numRects,
				width, height,
				GRAVITY_EAST,
				right_offset);
}

void
decor_apply_gravity (int gravity,
		     int x,
		     int y,
		     int width,
		     int height,
		     int *return_x,
		     int *return_y)
{
    if (gravity & GRAVITY_EAST)
    {
	x += width;
	*return_x = MAX (0, x);
    }
    else if (gravity & GRAVITY_WEST)
    {
	*return_x = MIN (width, x);
    }
    else
    {
	x += width / 2;
	x = MAX (0, x);
	x = MIN (width, x);
	*return_x = x;
    }

    if (gravity & GRAVITY_SOUTH)
    {
	y += height;
	*return_y = MAX (0, y);
    }
    else if (gravity & GRAVITY_NORTH)
    {
	*return_y = MIN (height, y);
    }
    else
    {
	y += height / 2;
	y = MAX (0, y);
	y = MIN (height, y);
	*return_y = y;
    }
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
			 int	      splitY,
			 int	      splitGravity,
			 double	      x0,
			 double	      y0,
			 int	      rotation)
{
    int nQuad = 0;

    q->p1.x	  = left;
    q->p1.y	  = -top;
    q->p1.gravity = gravity | GRAVITY_NORTH;
    q->p2.x	  = right;
    q->p2.y	  = splitY;
    q->p2.gravity = gravity | splitGravity;
    q->max_width  = SHRT_MAX;
    q->max_height = top + top_corner;
    q->align	  = ALIGN_TOP;
    q->clamp	  = CLAMP_VERT;
    q->stretch    = 0;
    q->m.x0	  = x0;
    q->m.y0	  = y0;

    if (rotation)
    {
	q->m.xx	= 0.0;
	q->m.xy	= 1.0;
	q->m.yx	= 1.0;
	q->m.yy	= 0.0;
    }
    else
    {
	q->m.xx	= 1.0;
	q->m.xy	= 0.0;
	q->m.yx	= 0.0;
	q->m.yy	= 1.0;
    }

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
    q->stretch    = 0;

    if (rotation)
    {
	q->m.xx	= 0.0;
	q->m.xy	= 0.0;
	q->m.yx	= 1.0;
	q->m.yy	= 0.0;
	q->m.x0	= x0 + top + top_corner;
	q->m.y0	= y0;
    }
    else
    {
	q->m.xx	= 1.0;
	q->m.xy	= 0.0;
	q->m.yx	= 0.0;
	q->m.yy	= 0.0;
	q->m.x0	= x0;
	q->m.y0	= y0 + top + top_corner;
    }

    q++; nQuad++;

    q->p1.x	  = left;
    q->p1.y	  = splitY;
    q->p1.gravity = gravity | splitGravity;
    q->p2.x	  = right;
    q->p2.y	  = bottom;
    q->p2.gravity = gravity | GRAVITY_SOUTH;
    q->max_width  = SHRT_MAX;
    q->max_height = bottom_corner + bottom;
    q->align	  = ALIGN_BOTTOM;
    q->clamp	  = CLAMP_VERT;
    q->stretch    = 0;

    if (rotation)
    {
	q->m.xx	= 0.0;
	q->m.xy	= 1.0;
	q->m.yx	= 1.0;
	q->m.yy	= 0.0;
	q->m.x0	= x0 + height;
	q->m.y0	= y0;
    }
    else
    {
	q->m.xx	= 1.0;
	q->m.xy	= 0.0;
	q->m.yx	= 0.0;
	q->m.yy	= 1.0;
	q->m.x0	= x0;
	q->m.y0	= y0 + height;
    }

    nQuad++;

    return nQuad;
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
			  int	       splitX,
			  int	       splitGravity,
			  double       x0,
			  double       y0)
{
    int nQuad = 0;

    q->p1.x	  = -left;
    q->p1.y	  = top;
    q->p1.gravity = gravity | GRAVITY_WEST;
    q->p2.x	  = splitX;
    q->p2.y	  = bottom;
    q->p2.gravity = gravity | splitGravity;
    q->max_width  = left + left_corner;
    q->max_height = SHRT_MAX;
    q->align	  = ALIGN_LEFT;
    q->clamp	  = 0;
    q->stretch    = 0;
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
    q->stretch    = 0;
    q->m.xx	  = 0.0;
    q->m.xy	  = 0.0;
    q->m.yx	  = 0.0;
    q->m.yy	  = 1.0;
    q->m.x0	  = x0 + left + left_corner;
    q->m.y0	  = y0;

    q++; nQuad++;

    q->p1.x	  = splitX;
    q->p1.y	  = top;
    q->p1.gravity = gravity | splitGravity;
    q->p2.x	  = right;
    q->p2.y	  = bottom;
    q->p2.gravity = gravity | GRAVITY_EAST;
    q->max_width  = right_corner + right;
    q->max_height = SHRT_MAX;
    q->align	  = ALIGN_RIGHT;
    q->clamp	  = 0;
    q->stretch    = 0;
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
decor_set_lSrS_window_quads (decor_quad_t    *q,
			     decor_context_t *c,
			     decor_layout_t  *l)
{
    int lh, rh, splitY, n, nQuad = 0;

    splitY = (c->top_corner_space - c->bottom_corner_space) / 2;

    if (l->rotation)
    {
	lh = l->left.x2 - l->left.x1;
	rh = l->right.x2 - l->right.x1;
    }
    else
    {
	lh = l->left.y2 - l->left.y1;
	rh = l->right.y2 - l->right.y1;
    }

    /* left quads */
    n = decor_set_vert_quad_row (q,
				 0,
				 c->top_corner_space,
				 0,
				 c->bottom_corner_space,
				 -c->left_space,
				 0,
				 GRAVITY_WEST,
				 lh,
				 splitY,
				 0,
				 l->left.x1,
				 l->left.y1,
				 l->rotation);

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
				 rh,
				 splitY,
				 0,
				 l->right.x1,
				 l->right.y1,
				 l->rotation);

    nQuad += n;

    return nQuad;
}

int
decor_set_lSrStSbS_window_quads (decor_quad_t    *q,
				 decor_context_t *c,
				 decor_layout_t  *l)
{
    int splitX, n, nQuad = 0;

    splitX = (c->left_corner_space - c->right_corner_space) / 2;

    /* top quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  c->left_corner_space,
				  c->right_space,
				  c->right_corner_space,
				  -c->top_space,
				  0,
				  GRAVITY_NORTH,
				  l->top.x2 - l->top.x1,
				  splitX,
				  0,
				  l->top.x1,
				  l->top.y1);

    q += n; nQuad += n;

    n = decor_set_lSrS_window_quads (q, c, l);

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
				  l->bottom.x2 - l->bottom.x1,
				  splitX,
				  0,
				  l->bottom.x1,
				  l->bottom.y1);

    nQuad += n;

    return nQuad;
}

int
decor_set_lSrStXbS_window_quads (decor_quad_t    *q,
				 decor_context_t *c,
				 decor_layout_t  *l,
				 int		 top_stretch_offset)
{
    int splitX, n, nQuad = 0;
    int top_left, top_right;

    splitX = (c->left_corner_space - c->right_corner_space) / 2;

    top_left  = top_stretch_offset;
    top_right = l->top.x2 - l->top.x1 -
	c->left_space - c->right_space - top_left - 1;

    /* top quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  top_left,
				  c->right_space,
				  top_right,
				  -c->top_space,
				  0,
				  GRAVITY_NORTH,
				  l->top.x2 - l->top.x1,
				  -top_right,
				  GRAVITY_EAST,
				  l->top.x1,
				  l->top.y1);

    q += n; nQuad += n;

    n = decor_set_lSrS_window_quads (q, c, l);

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
				  l->bottom.x2 - l->bottom.x1,
				  splitX,
				  0,
				  l->bottom.x1,
				  l->bottom.y1);

    nQuad += n;

    return nQuad;
}

int
decor_set_lSrStSbX_window_quads (decor_quad_t    *q,
				 decor_context_t *c,
				 decor_layout_t  *l,
				 int		 bottom_stretch_offset)
{
    int splitX, n, nQuad = 0;
    int bottom_left, bottom_right;

    splitX = (c->left_corner_space - c->right_corner_space) / 2;

    bottom_left  = bottom_stretch_offset;
    bottom_right = l->bottom.x2 - l->bottom.x1 -
	c->left_space - c->right_space - bottom_left - 1;

    /* top quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  c->left_corner_space,
				  c->right_space,
				  c->right_corner_space,
				  -c->top_space,
				  0,
				  GRAVITY_NORTH,
				  l->top.x2 - l->top.x1,
				  splitX,
				  0,
				  l->top.x1,
				  l->top.y1);

    q += n; nQuad += n;

    n = decor_set_lSrS_window_quads (q, c, l);

    q += n; nQuad += n;

    /* bottom quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  bottom_left,
				  c->right_space,
				  bottom_right,
				  0,
				  c->bottom_space,
				  GRAVITY_SOUTH,
				  l->bottom.x2 - l->bottom.x1,
				  -bottom_right,
				  GRAVITY_EAST,
				  l->bottom.x1,
				  l->bottom.y1);

    nQuad += n;

    return nQuad;
}

int
decor_set_lXrXtXbX_window_quads (decor_quad_t    *q,
				 decor_context_t *c,
				 decor_layout_t  *l,
				 int		 left_stretch_offset,
				 int		 right_stretch_offset,
				 int		 top_stretch_offset,
				 int		 bottom_stretch_offset)
{
    int lh, rh, n, nQuad = 0;
    int left_top, left_bottom;
    int right_top, right_bottom;
    int top_left, top_right;
    int bottom_left, bottom_right;

    top_left  = top_stretch_offset;
    top_right = l->top.x2 - l->top.x1 -
	c->left_space - c->right_space - top_left - 1;

    bottom_left  = bottom_stretch_offset;
    bottom_right = l->bottom.x2 - l->bottom.x1 -
	c->left_space - c->right_space - bottom_left - 1;

    if (l->rotation)
    {
	lh = l->left.x2 - l->left.x1;
	rh = l->right.x2 - l->right.x1;
    }
    else
    {
	lh = l->left.y2 - l->left.y1;
	rh = l->right.y2 - l->right.y1;
    }

    left_top    = left_stretch_offset;
    left_bottom = lh - left_top - 1;

    right_top    = right_stretch_offset;
    right_bottom = rh - right_top - 1;


    /* top quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  top_left,
				  c->right_space,
				  top_right,
				  -c->top_space,
				  0,
				  GRAVITY_NORTH,
				  l->top.x2 - l->top.x1,
				  -top_right,
				  GRAVITY_EAST,
				  l->top.x1,
				  l->top.y1);

    q += n; nQuad += n;

    /* left quads */
    n = decor_set_vert_quad_row (q,
				 0,
				 left_top,
				 0,
				 left_bottom,
				 -c->left_space,
				 0,
				 GRAVITY_WEST,
				 lh,
				 -left_bottom,
				 GRAVITY_SOUTH,
				 l->left.x1,
				 l->left.y1,
				 l->rotation);

    q += n; nQuad += n;

    /* right quads */
    n = decor_set_vert_quad_row (q,
				 0,
				 right_top,
				 0,
				 right_bottom,
				 0,
				 c->right_space,
				 GRAVITY_EAST,
				 rh,
				 -right_bottom,
				 GRAVITY_SOUTH,
				 l->right.x1,
				 l->right.y1,
				 l->rotation);

    q += n; nQuad += n;

    /* bottom quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  bottom_left,
				  c->right_space,
				  bottom_right,
				  0,
				  c->bottom_space,
				  GRAVITY_SOUTH,
				  l->bottom.x2 - l->bottom.x1,
				  -bottom_right,
				  GRAVITY_EAST,
				  l->bottom.x1,
				  l->bottom.y1);

    nQuad += n;

    return nQuad;
}

#if INT_MAX != LONG_MAX

static int errors;

static int
error_handler (Display *xdisplay,
	       XErrorEvent *event)
{
    errors++;
    return 0;
}

/* XRenderSetPictureFilter used to be broken on LP64. This
 * works with either the broken or fixed version.
 */
static void
XRenderSetPictureFilter_wrapper (Display *dpy,
				 Picture picture,
				 char    *filter,
				 XFixed  *params,
				 int     nparams)
{
    int (*old) (Display *, XErrorEvent *);

    errors = 0;

    old = XSetErrorHandler (error_handler);

    XRenderSetPictureFilter (dpy, picture, filter, params, nparams);
    XSync (dpy, False);

    XSetErrorHandler (old);

    if (errors)
    {
	long *long_params = malloc (sizeof (long) * nparams);
	int  i;

	for (i = 0; i < nparams; i++)
	    long_params[i] = params[i];

	XRenderSetPictureFilter (dpy, picture, filter,
				 (XFixed *) long_params, nparams);
	free (long_params);
    }
}

#define XRenderSetPictureFilter XRenderSetPictureFilter_wrapper
#endif

static void
set_picture_transform (Display *xdisplay,
		       Picture p,
		       int     dx,
		       int     dy)
{
    XTransform transform = {
	{
	    { 1 << 16, 0,       -dx << 16 },
	    { 0,       1 << 16, -dy << 16 },
	    { 0,       0,         1 << 16 },
	}
    };

    XRenderSetPictureTransform (xdisplay, p, &transform);
}

static void
set_picture_clip (Display *xdisplay,
		  Picture p,
		  int     width,
		  int	  height,
		  int     clipX1,
		  int     clipY1,
		  int     clipX2,
		  int     clipY2)
{
    XRectangle clip[4];

    clip[0].x      = 0;
    clip[0].y      = 0;
    clip[0].width  = width;
    clip[0].height = clipY1;

    clip[1].x      = 0;
    clip[1].y      = clipY2;
    clip[1].width  = width;
    clip[1].height = height - clipY2;

    clip[2].x      = 0;
    clip[2].y      = clipY1;
    clip[2].width  = clipX1;
    clip[2].height = clipY2 - clipY1;

    clip[3].x      = clipX2;
    clip[3].y      = clipY1;
    clip[3].width  = width - clipX2;
    clip[3].height = clipY2 - clipY1;

    XRenderSetPictureClipRectangles (xdisplay, p, 0, 0, clip, 4);
}

static void
set_no_picture_clip (Display *xdisplay,
		     Picture p)
{
    XRectangle clip;

    clip.x      = 0;
    clip.y      = 0;
    clip.width  = SHRT_MAX;
    clip.height = SHRT_MAX;

    XRenderSetPictureClipRectangles (xdisplay, p, 0, 0, &clip, 1);
}

static XFixed *
create_gaussian_kernel (double radius,
			double sigma,
			double alpha,
			int    *r_size)
{
    XFixed *params;
    double *amp, scale, x_scale, fx, sum;
    int    size, half_size, x, i, n;

    scale = 1.0f / (2.0f * M_PI * sigma * sigma);
    half_size = alpha + 0.5f;

    if (half_size == 0)
	half_size = 1;

    size = half_size * 2 + 1;
    x_scale = 2.0f * radius / size;

    if (size < 3)
	return NULL;

    n = size;

    amp = malloc (sizeof (double) * n);
    if (!amp)
	return NULL;

    n += 2;

    params = malloc (sizeof (XFixed) * n);
    if (!params)
    {
	free (amp);
	return NULL;
    }

    i   = 0;
    sum = 0.0f;

    for (x = 0; x < size; x++)
    {
	fx = x_scale * (x - half_size);

	amp[i] = scale * exp ((-1.0f * (fx * fx)) / (2.0f * sigma * sigma));

	sum += amp[i];

	i++;
    }

    /* normalize */
    if (sum != 0.0)
	sum = 1.0 / sum;

    params[0] = params[1] = 0;

    for (i = 2; i < n; i++)
	params[i] = XDoubleToFixed (amp[i - 2] * sum);

    free (amp);

    *r_size = size;

    return params;
}

#define SIGMA(r) ((r) / 2.0)
#define ALPHA(r) (r)

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
		     decor_context_t	    *c,
		     decor_draw_func_t	    draw,
		     void		    *closure)
{
    static XRenderColor white = { 0xffff, 0xffff, 0xffff, 0xffff };
    XRenderPictFormat   *format;
    Pixmap		pixmap;
    Picture		src, dst, tmp;
    XFixed		opacity, *params;
    XFilters		*filters;
    char		*filter = NULL;
    int			size, n_params = 0;
    XRenderColor	color;
    int			shadow_offset_x;
    int			shadow_offset_y;
    Pixmap		d_pixmap;
    int			d_width;
    int			d_height;
    Window		xroot = screen->root;
    decor_shadow_t	*shadow;
    int			clipX1, clipY1, clipX2, clipY2;

    shadow = malloc (sizeof (decor_shadow_t));
    if (!shadow)
	return NULL;

    shadow->ref_count = 1;

    shadow->pixmap  = 0;
    shadow->picture = 0;
    shadow->width   = 0;
    shadow->height  = 0;

    shadow_offset_x = opt->shadow_offset_x;
    shadow_offset_y = opt->shadow_offset_y;

    /* compute a gaussian convolution kernel */
    params = create_gaussian_kernel (opt->shadow_radius,
				     SIGMA (opt->shadow_radius),
				     ALPHA (opt->shadow_radius),
				     &size);
    if (!params)
	shadow_offset_x = shadow_offset_y = size = 0;

    if (opt->shadow_radius <= 0.0 &&
	shadow_offset_x == 0	  &&
	shadow_offset_y == 0)
	size = 0;

    n_params = size + 2;
    size     = size / 2;

    c->extents.left   = left;
    c->extents.right  = right;
    c->extents.top    = top;
    c->extents.bottom = bottom;

    c->left_space   = left   + size - shadow_offset_x;
    c->right_space  = right  + size + shadow_offset_x;
    c->top_space    = top    + size - shadow_offset_y;
    c->bottom_space = bottom + size + shadow_offset_y;

    c->left_space   = MAX (left,   c->left_space);
    c->right_space  = MAX (right,  c->right_space);
    c->top_space    = MAX (top,    c->top_space);
    c->bottom_space = MAX (bottom, c->bottom_space);

    c->left_corner_space   = MAX (1, size - solid_left   + shadow_offset_x);
    c->right_corner_space  = MAX (1, size - solid_right  - shadow_offset_x);
    c->top_corner_space    = MAX (1, size - solid_top    + shadow_offset_y);
    c->bottom_corner_space = MAX (1, size - solid_bottom - shadow_offset_y);

    width  = MAX (width, c->left_corner_space + c->right_corner_space);
    height = MAX (height, c->top_corner_space + c->bottom_corner_space);

    width  = MAX (1, width);
    height = MAX (1, height);

    d_width  = c->left_space + width + c->right_space;
    d_height = c->top_space + height + c->bottom_space;

    /* all pixmaps are ARGB32 */
    format = XRenderFindStandardFormat (xdisplay, PictStandardARGB32);

    /* no shadow */
    if (size <= 0)
    {
	if (params)
	    free (params);

	return shadow;
    }

    pixmap = XCreatePixmap (xdisplay, xroot, d_width, d_height, 32);
    if (!pixmap)
    {
	free (params);
	return shadow;
    }

    /* query server for convolution filter */
    filters = XRenderQueryFilters (xdisplay, pixmap);
    if (filters)
    {
	int i;

	for (i = 0; i < filters->nfilter; i++)
	{
	    if (strcmp (filters->filter[i], FilterConvolution) == 0)
	    {
		filter = (char *) FilterConvolution;
		break;
	    }
	}

	XFree (filters);
    }

    if (!filter)
    {
	XFreePixmap (xdisplay, pixmap);
	free (params);

	return shadow;
    }

    /* create pixmap for temporary decorations */
    d_pixmap = XCreatePixmap (xdisplay, xroot, d_width, d_height, 32);
    if (!d_pixmap)
    {
	XFreePixmap (xdisplay, pixmap);
	free (params);

	return shadow;
    }

    src = XRenderCreateSolidFill (xdisplay, &white);
    dst = XRenderCreatePicture (xdisplay, d_pixmap, format, 0, NULL);
    tmp = XRenderCreatePicture (xdisplay, pixmap, format, 0, NULL);

    /* draw decoration */
    (*draw) (xdisplay, d_pixmap, dst, d_width, d_height, c, closure);

    /* first pass */
    params[0] = (n_params - 2) << 16;
    params[1] = 1 << 16;

    clipX1 = c->left_space + size;
    clipY1 = c->top_space  + size;
    clipX2 = d_width - c->right_space - size;
    clipY2 = d_height - c->bottom_space - size;

    if (clipX1 < clipX2 && clipY1 < clipY2)
	set_picture_clip (xdisplay, tmp, d_width, d_height,
			  clipX1, clipY1, clipX2, clipY2);

    set_picture_transform (xdisplay, dst, shadow_offset_x, 0);
    XRenderSetPictureFilter (xdisplay, dst, filter, params, n_params);
    XRenderComposite (xdisplay,
		      PictOpSrc,
		      src,
		      dst,
		      tmp,
		      0, 0,
		      0, 0,
		      0, 0,
		      d_width, d_height);

    set_no_picture_clip (xdisplay, tmp);

    XRenderFreePicture (xdisplay, src);

    /* second pass */
    params[0] = 1 << 16;
    params[1] = (n_params - 2) << 16;

    opacity = XDoubleToFixed (opt->shadow_opacity);
    if (opacity < (1 << 16))
    {
	/* apply opacity as shadow color if less than 1.0 */
	color.red   = (opt->shadow_color[0] * opacity) >> 16;
	color.green = (opt->shadow_color[1] * opacity) >> 16;
	color.blue  = (opt->shadow_color[2] * opacity) >> 16;
	color.alpha = opacity;

	opacity = 1 << 16;
    }
    else
    {
	/* shadow color */
	color.red   = opt->shadow_color[0];
	color.green = opt->shadow_color[1];
	color.blue  = opt->shadow_color[2];
	color.alpha = 0xffff;
    }

    src = XRenderCreateSolidFill (xdisplay, &color);

    clipX1 = c->left_space;
    clipY1 = c->top_space;
    clipX2 = d_width - c->right_space;
    clipY2 = d_height - c->bottom_space;

    if (clipX1 < clipX2 && clipY1 < clipY2)
	set_picture_clip (xdisplay, dst, d_width, d_height,
			  clipX1, clipY1, clipX2, clipY2);

    set_picture_transform (xdisplay, tmp, 0, shadow_offset_y);
    XRenderSetPictureFilter (xdisplay, tmp, filter, params, n_params);
    XRenderComposite (xdisplay,
		      PictOpSrc,
		      src,
		      tmp,
		      dst,
		      0, 0,
		      0, 0,
		      0, 0,
		      d_width, d_height);

    set_no_picture_clip (xdisplay, dst);

    XRenderFreePicture (xdisplay, src);

    if (opacity != (1 << 16))
    {
	XFixed p[3];

	p[0] = 1 << 16;
	p[1] = 1 << 16;
	p[2] = opacity;

	if (clipX1 < clipX2 && clipY1 < clipY2)
	    set_picture_clip (xdisplay, tmp, d_width, d_height,
			      clipX1, clipY1, clipX2, clipY2);

	/* apply opacity */
	set_picture_transform (xdisplay, dst, 0, 0);
	XRenderSetPictureFilter (xdisplay, dst, filter, p, 3);
	XRenderComposite (xdisplay,
			  PictOpSrc,
			  dst,
			  None,
			  tmp,
			  0, 0,
			  0, 0,
			  0, 0,
			  d_width, d_height);

	XFreePixmap (xdisplay, d_pixmap);
	shadow->pixmap = pixmap;
    }
    else
    {
	XFreePixmap (xdisplay, pixmap);
	shadow->pixmap = d_pixmap;
    }

    XRenderFreePicture (xdisplay, tmp);
    XRenderFreePicture (xdisplay, dst);

    shadow->picture = XRenderCreatePicture (xdisplay, shadow->pixmap,
					    format, 0, NULL);

    shadow->width  = d_width;
    shadow->height = d_height;

    free (params);

    return shadow;
}

void
decor_shadow_destroy (Display	     *xdisplay,
		      decor_shadow_t *shadow)
{
    shadow->ref_count--;
    if (shadow->ref_count)
	return;

    if (shadow->picture)
	XRenderFreePicture (xdisplay, shadow->picture);

    if (shadow->pixmap)
	XFreePixmap (xdisplay, shadow->pixmap);

    free (shadow);
}

void
decor_shadow_reference (decor_shadow_t *shadow)
{
    shadow->ref_count++;
}

void
decor_draw_simple (Display	   *xdisplay,
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
			  c->left_space - c->extents.left,
			  c->top_space - c->extents.top,
			  width - c->left_space - c->right_space +
			  c->extents.left + c->extents.right,
			  height - c->top_space - c->bottom_space +
			  c->extents.top + c->extents.bottom);
}

void
decor_get_default_layout (decor_context_t *c,
			  int	          width,
			  int	          height,
			  decor_layout_t  *layout)
{
    width  = MAX (width, c->left_corner_space + c->right_corner_space);
    height = MAX (height, c->top_corner_space + c->bottom_corner_space);

    width += c->left_space + c->right_space;

    layout->top.x1  = 0;
    layout->top.y1  = 0;
    layout->top.x2  = width;
    layout->top.y2  = c->top_space;
    layout->top.pad = 0;

    layout->left.x1  = 0;
    layout->left.y1  = c->top_space;
    layout->left.x2  = c->left_space;
    layout->left.y2  = c->top_space + height;
    layout->left.pad = 0;

    layout->right.x1  = width - c->right_space;
    layout->right.y1  = c->top_space;
    layout->right.x2  = width;
    layout->right.y2  = c->top_space + height;
    layout->right.pad = 0;

    layout->bottom.x1  = 0;
    layout->bottom.y1  = height + c->top_space;
    layout->bottom.x2  = width;
    layout->bottom.y2  = height + c->top_space + c->bottom_space;
    layout->bottom.pad = 0;

    layout->width  = width;
    layout->height = height + c->top_space + c->bottom_space;

    layout->rotation = 0;
}

void
decor_get_best_layout (decor_context_t *c,
		       int	       width,
		       int	       height,
		       decor_layout_t  *layout)
{
    int y;

    /* use default layout when no left and right extents */
    if (c->extents.left == 0 && c->extents.right == 0)
    {
	decor_get_default_layout (c, width, 1, layout);
	return;
    }

    width  = MAX (width, c->left_corner_space + c->right_corner_space);
    height = MAX (height, c->top_corner_space + c->bottom_corner_space);

    width += c->left_space + c->right_space;

    if (width >= (height + 2))
    {
	int max;

	layout->width = width;

	layout->top.x1 = 0;
	layout->top.y1 = 0;
	layout->top.x2 = width;
	layout->top.y2 = c->top_space;

	y = c->top_space;

	max = MAX (c->left_space, c->right_space);
	if (max < height)
	{
	    layout->rotation = 1;

	    y += 2;

	    layout->top.pad    = PAD_BOTTOM;
	    layout->bottom.pad = PAD_TOP;
	    layout->left.pad   = PAD_TOP | PAD_BOTTOM | PAD_LEFT | PAD_RIGHT;
	    layout->right.pad  = PAD_TOP | PAD_BOTTOM | PAD_LEFT | PAD_RIGHT;

	    layout->left.x1 = 1;
	    layout->left.y1 = y;
	    layout->left.x2 = 1 + height;
	    layout->left.y2 = y + c->left_space;

	    if ((height + 2) <= (width / 2))
	    {
		layout->right.x1 = height + 3;
		layout->right.y1 = y;
		layout->right.x2 = height + 3 + height;
		layout->right.y2 = y + c->right_space;

		y += max + 2;
	    }
	    else
	    {
		y += c->left_space + 2;

		layout->right.x1 = 1;
		layout->right.y1 = y;
		layout->right.x2 = 1 + height;
		layout->right.y2 = y + c->right_space;

		y += c->right_space + 2;
	    }
	}
	else
	{
	    layout->top.pad    = 0;
	    layout->bottom.pad = 0;
	    layout->left.pad   = 0;
	    layout->right.pad  = 0;

	    layout->left.x1 = 0;
	    layout->left.y1 = y;
	    layout->left.x2 = c->left_space;
	    layout->left.y2 = y + height;

	    layout->right.x1 = width - c->right_space;
	    layout->right.y1 = y;
	    layout->right.x2 = width;
	    layout->right.y2 = y + height;

	    y += height;
	}

	layout->bottom.x1 = 0;
	layout->bottom.y1 = y;
	layout->bottom.x2 = width;
	layout->bottom.y2 = y + c->bottom_space;

	y += c->bottom_space;
    }
    else
    {
	layout->rotation = 1;

	layout->left.pad   = PAD_TOP | PAD_BOTTOM | PAD_LEFT | PAD_RIGHT;
	layout->right.pad  = PAD_TOP | PAD_BOTTOM | PAD_LEFT | PAD_RIGHT;

	layout->top.x1 = 0;
	layout->top.y1 = 0;
	layout->top.x2 = width;
	layout->top.y2 = c->top_space;

	if (((width * 2) + 3) <= (height + 2))
	{
	    layout->width = height + 2;

	    layout->top.pad    = PAD_BOTTOM | PAD_RIGHT;
	    layout->bottom.pad = PAD_TOP | PAD_BOTTOM | PAD_RIGHT | PAD_LEFT;

	    layout->bottom.x1 = width + 2;
	    layout->bottom.y1 = 1;
	    layout->bottom.x2 = width + 2 + width;
	    layout->bottom.y2 = 1 + c->bottom_space;

	    y = MAX (c->top_space, 1 + c->bottom_space) + 2;

	    layout->left.x1 = 1;
	    layout->left.y1 = y;
	    layout->left.x2 = 1 + height;
	    layout->left.y2 = y + c->left_space;

	    y += c->left_space + 2;

	    layout->right.x1 = 1;
	    layout->right.y1 = y;
	    layout->right.x2 = 1 + height;
	    layout->right.y2 = y + c->right_space;

	    y += c->right_space;
	}
	else
	{
	    layout->width = height + 2;

	    layout->top.pad    = PAD_BOTTOM | PAD_RIGHT;
	    layout->bottom.pad = PAD_TOP | PAD_RIGHT;

	    y = c->top_space + 2;

	    layout->left.x1 = 1;
	    layout->left.y1 = y;
	    layout->left.x2 = 1 + height;
	    layout->left.y2 = y + c->left_space;

	    y += c->left_space + 2;

	    layout->right.x1 = 1;
	    layout->right.y1 = y;
	    layout->right.x2 = 1 + height;
	    layout->right.y2 = y + c->right_space;

	    y += c->right_space + 2;

	    layout->bottom.x1 = 0;
	    layout->bottom.y1 = y;
	    layout->bottom.x2 = width;
	    layout->bottom.y2 = y + c->bottom_space;

	    y += c->bottom_space;
	}
    }

    layout->height = y;
}

static XTransform xident = {
    {
	{ 1 << 16, 0,             0 },
	{ 0,       1 << 16,       0 },
	{ 0,       0,       1 << 16 },
    }
};

void
decor_fill_picture_extents_with_shadow (Display	        *xdisplay,
					decor_shadow_t  *shadow,
					decor_context_t *context,
					Picture	        picture,
					decor_layout_t  *layout)
{
    int w, h, left, right, top, bottom, width, height;

    if (!shadow->picture)
	return;

    width = layout->top.x2 - layout->top.x1;
    if (layout->rotation)
	height = layout->left.x2 - layout->left.x1;
    else
	height = layout->left.y2 - layout->left.y1;

    height += context->top_space + context->bottom_space;

    left   = context->left_space   + context->left_corner_space;
    right  = context->right_space  + context->right_corner_space;
    top    = context->top_space    + context->top_corner_space;
    bottom = context->bottom_space + context->bottom_corner_space;

    if (width - left - right < 0)
    {
	left = width / 2;
	right = width - left;
    }

    if (height - top - bottom < 0)
    {
	top = height / 2;
	bottom = height - top;
    }

    w = width - left - right;
    h = height - top - bottom;

    /* top left */
    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
		      0, 0,
		      0, 0,
		      layout->top.x1, layout->top.y1,
		      left, context->top_space);

    /* top right */
    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
		      shadow->width - right, 0,
		      0, 0,
		      layout->top.x2 - right, layout->top.y1,
		      right, context->top_space);

    /* bottom left */
    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
		      0, shadow->height - context->bottom_space,
		      0, 0,
		      layout->bottom.x1, layout->bottom.y1,
		      left, context->bottom_space);

    /* bottom right */
    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
		      shadow->width - right,
		      shadow->height - context->bottom_space,
		      0, 0,
		      layout->bottom.x2 - right, layout->bottom.y1,
		      right, context->bottom_space);

    if (w > 0)
    {
	int sw = shadow->width - left - right;
	int sx = left;

	if (sw != w)
	{
	    XTransform t = {
		{
		    { (sw << 16) / w,       0, left << 16 },
		    {              0, 1 << 16,          0 },
		    {              0,       0,    1 << 16 },
		}
	    };

	    sx = 0;

	    XRenderSetPictureTransform (xdisplay, shadow->picture, &t);
	}

	/* top */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  sx, 0,
			  0, 0,
			  layout->top.x1 + left, layout->top.y1,
			  w, context->top_space);

	/* bottom */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  sx, shadow->height - context->bottom_space,
			  0, 0,
			  layout->bottom.x1 + left, layout->bottom.y1,
			  w, context->bottom_space);

	if (sw != w)
	    XRenderSetPictureTransform (xdisplay, shadow->picture, &xident);
    }

    if (layout->rotation)
    {
	XTransform t = {
	    {
		{       0, 1 << 16,       0 },
		{ 1 << 16,       0,       0 },
		{       0,       0, 1 << 16 }
	    }
	};

	t.matrix[1][2] = context->top_space << 16;

	XRenderSetPictureTransform (xdisplay, shadow->picture, &t);

	/* left top */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  0, 0,
			  0, 0,
			  layout->left.x1,
			  layout->left.y1,
			  top - context->top_space, context->left_space);

	t.matrix[0][2] = (shadow->width - context->right_space) << 16;

	XRenderSetPictureTransform (xdisplay, shadow->picture, &t);

	/* right top */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  0, 0,
			  0, 0,
			  layout->right.x1,
			  layout->right.y1,
			  top - context->top_space, context->right_space);

	XRenderSetPictureTransform (xdisplay, shadow->picture, &xident);
    }
    else
    {
	/* left top */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  0, context->top_space,
			  0, 0,
			  layout->left.x1, layout->left.y1,
			  context->left_space, top - context->top_space);

	/* right top */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  shadow->width - context->right_space,
			  context->top_space,
			  0, 0,
			  layout->right.x1, layout->right.y1,
			  context->right_space, top - context->top_space);
    }

    if (layout->rotation)
    {
	XTransform t = {
	    {
		{       0, 1 << 16,       0 },
		{ 1 << 16,       0,       0 },
		{       0,       0, 1 << 16 }
	    }
	};

	t.matrix[1][2] = (shadow->height - bottom) << 16;

	XRenderSetPictureTransform (xdisplay, shadow->picture, &t);

	/* left bottom */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  0, 0,
			  0, 0,
			  layout->left.x2 - (bottom - context->bottom_space),
			  layout->left.y1,
			  bottom - context->bottom_space, context->left_space);

	t.matrix[0][2] = (shadow->width - context->right_space) << 16;

	XRenderSetPictureTransform (xdisplay, shadow->picture, &t);

	/* right bottom */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  0, 0,
			  0, 0,
			  layout->right.x2 - (bottom - context->bottom_space),
			  layout->right.y1,
			  bottom - context->bottom_space, context->right_space);

	XRenderSetPictureTransform (xdisplay, shadow->picture, &xident);
    }
    else
    {
	/* left bottom */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  0, shadow->height - bottom,
			  0, 0,
			  layout->left.x1,
			  layout->left.y2 - (bottom - context->bottom_space),
			  context->left_space, bottom - context->bottom_space);

	/* right bottom */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			  shadow->width - context->right_space,
			  shadow->height - bottom,
			  0, 0,
			  layout->right.x1,
			  layout->right.y2 - (bottom - context->bottom_space),
			  context->right_space, bottom - context->bottom_space);
    }

    if (h > 0)
    {
	int sh = shadow->height - top - bottom;

	if (layout->rotation)
	{
	    XTransform t = {
		{
		    {              0, 1 << 16,       0 },
		    { (sh << 16) / h,       0,       0 },
		    {              0,       0, 1 << 16 }
		}
	    };

	    t.matrix[1][2] = top << 16;

	    XRenderSetPictureTransform (xdisplay, shadow->picture, &t);

	    /* left */
	    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			      0, 0,
			      0, 0,
			      layout->left.x1 + (top - context->top_space),
			      layout->left.y1,
			      h, context->left_space);

	    t.matrix[0][2] = (shadow->width - context->right_space) << 16;

	    XRenderSetPictureTransform (xdisplay, shadow->picture, &t);

	    /* right */
	    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			      0, 0,
			      0, 0,
			      layout->right.x1 + (top - context->top_space),
			      layout->right.y1,
			      h, context->right_space);

	    XRenderSetPictureTransform (xdisplay, shadow->picture, &xident);

	}
	else
	{
	    int sy = top;

	    if (sh != h)
	    {
		XTransform t = {
		    {
			{ 1 << 16,              0,         0 },
			{       0, (sh << 16) / h, top << 16 },
			{       0,              0,   1 << 16 },
		    }
		};

		sy = 0;

		XRenderSetPictureTransform (xdisplay, shadow->picture, &t);
	    }

	    /* left */
	    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			      0, sy,
			      0, 0,
			      layout->left.x1,
			      layout->left.y1 + (top - context->top_space),
			      context->left_space, h);

	    /* right */
	    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, 0, picture,
			      shadow->width - context->right_space, sy,
			      0, 0,
			      layout->right.x2 - context->right_space,
			      layout->right.y1 + (top - context->top_space),
			      context->right_space, h);

	    if (sh != h)
		XRenderSetPictureTransform (xdisplay, shadow->picture, &xident);
	}
    }
}

static void
_decor_pad_border_picture (Display     *xdisplay,
			   Picture     dst,
			   decor_box_t *box)
{
    int x1, y1, x2, y2;

    x1 = box->x1;
    y1 = box->y1;
    x2 = box->x2;
    y2 = box->y2;

    if (box->pad & PAD_TOP)
    {
	XRenderComposite (xdisplay, PictOpSrc, dst, None, dst,
			  x1, y1,
			  0, 0,
			  x1, y1 - 1,
			  x2 - x1, 1);

	y1--;
    }

    if (box->pad & PAD_BOTTOM)
    {
	XRenderComposite (xdisplay, PictOpSrc, dst, None, dst,
			  x1, y2 - 1,
			  0, 0,
			  x1, y2,
			  x2 - x1, 1);

	y2++;
    }

    if (box->pad & PAD_LEFT)
    {
	XRenderComposite (xdisplay, PictOpSrc, dst, None, dst,
			  x1, y1,
			  0, 0,
			  x1 - 1, y1,
			  1, y2 - y1);
    }

    if (box->pad & PAD_RIGHT)
    {
	XRenderComposite (xdisplay, PictOpSrc, dst, None, dst,
			  x2 - 1, y1,
			  0, 0,
			  x2, y1,
			  1, y2 - y1);
    }
}

#ifndef HAVE_XRENDER_0_9_3
/* XRenderCreateLinearGradient and XRenderCreateRadialGradient used to be
 * broken. Flushing Xlib's output buffer before calling one of these
 * functions will avoid this specific issue.
 */
static Picture
XRenderCreateLinearGradient_wrapper (Display		   *xdisplay,
				     const XLinearGradient *gradient,
				     const XFixed	   *stops,
				     const XRenderColor	   *colors,
				     int		   nStops)
{
    XFlush (xdisplay);

    return XRenderCreateLinearGradient (xdisplay, gradient,
					stops, colors, nStops);
}

static Picture
XRenderCreateRadialGradient_wrapper (Display		   *xdisplay,
				     const XRadialGradient *gradient,
				     const XFixed	   *stops,
				     const XRenderColor	   *colors,
				     int		   nStops)
{
    XFlush (xdisplay);

    return XRenderCreateRadialGradient (xdisplay, gradient,
					stops, colors, nStops);
}

#define XRenderCreateLinearGradient XRenderCreateLinearGradient_wrapper
#define XRenderCreateRadialGradient XRenderCreateRadialGradient_wrapper
#endif

static void
_decor_blend_horz_border_picture (Display	  *xdisplay,
				  decor_context_t *context,
				  Picture	  src,
				  int	          xSrc,
				  int	          ySrc,
				  Picture	  dst,
				  decor_layout_t  *layout,
				  Region	  region,
				  unsigned short  alpha,
				  int	          shade_alpha,
				  int		  x1,
				  int		  y1,
				  int		  x2,
				  int		  y2,
				  int		  dy,
				  int		  direction,
				  int             ignore_src_alpha)
{
    XRenderColor color[3] = {
	{ 0xffff, 0xffff, 0xffff, 0xffff },
	{  alpha,  alpha,  alpha,  alpha },
	{    0x0,    0x0,    0x0, 0xffff }
    };
    int		 op = PictOpSrc, gop = PictOpSrc;
    int		 left, right;

    left   = context->extents.left;
    right  = context->extents.right;

    XOffsetRegion (region, x1, y1);
    XRenderSetPictureClipRegion (xdisplay, dst, region);
    XOffsetRegion (region, -x1, -y1);

    if (ignore_src_alpha)
    {
	XRenderComposite (xdisplay, PictOpSrc, src, None, dst,
			  xSrc, ySrc,
			  0, 0,
			  x1, y1,
			  x2 - x1, y2 - y1);
	XRenderFillRectangle (xdisplay, PictOpAdd, dst, &color[2], x1, y1,
			      x2 - x1, y2 - y1);
	gop = PictOpInReverse;
    }

    if (alpha != 0xffff)
    {
	op = PictOpIn;

	if (shade_alpha)
	{
	    static XFixed	     stop[2] = { 0, 1 << 16 };
	    XTransform		     transform = {
		{
		    { 1 << 16,       0,       0 },
		    {       0, 1 << 16,       0 },
		    {       0,       0, 1 << 16 }
		}
	    };
	    Picture		     grad;
	    XLinearGradient	     linear;
	    XRadialGradient	     radial;
	    XRenderPictureAttributes attrib;

	    attrib.repeat = RepeatPad;

	    radial.inner.x	= 0;
	    radial.inner.y	= 0;
	    radial.inner.radius = 0;
	    radial.outer.x	= 0;
	    radial.outer.y	= 0;

	    /* left */
	    radial.outer.radius = left << 16;

	    grad = XRenderCreateRadialGradient (xdisplay,
						&radial,
						stop,
						color,
						2);

	    transform.matrix[1][1] = (left << 16) / dy;
	    transform.matrix[0][2] = -left << 16;

	    if (direction < 0)
		transform.matrix[1][2] = -left << 16;

	    XRenderSetPictureTransform (xdisplay, grad, &transform);
	    XRenderChangePicture (xdisplay, grad, CPRepeat, &attrib);

	    XRenderComposite (xdisplay, gop, grad, None, dst,
			      0, 0,
			      0, 0,
			      x1, y1,
			      left, dy);

	    XRenderFreePicture (xdisplay, grad);

	    /* middle */
	    linear.p1.x = 0;
	    linear.p2.x = 0;

	    if (direction > 0)
	    {
		linear.p1.y = 0;
		linear.p2.y = dy << 16;
	    }
	    else
	    {
		linear.p1.y = dy << 16;
		linear.p2.y = 0;
	    }

	    grad = XRenderCreateLinearGradient (xdisplay,
						&linear,
						stop,
						color,
						2);

	    XRenderChangePicture (xdisplay, grad, CPRepeat, &attrib);

	    XRenderComposite (xdisplay, gop, grad, None, dst,
			      0, 0,
			      0, 0,
			      x1 + left, y1,
			      (x2 - x1) - left - right, dy);

	    XRenderFreePicture (xdisplay, grad);

	    /* right */
	    radial.outer.radius = right << 16;

	    grad = XRenderCreateRadialGradient (xdisplay,
						&radial,
						stop,
						color,
						2);

	    transform.matrix[1][1] = (right << 16) / dy;
	    transform.matrix[0][2] = 1 << 16;

	    if (direction < 0)
		transform.matrix[1][2] = -right << 16;

	    XRenderSetPictureTransform (xdisplay, grad, &transform);
	    XRenderChangePicture (xdisplay, grad, CPRepeat, &attrib);

	    XRenderComposite (xdisplay, gop, grad, None, dst,
			      0, 0,
			      0, 0,
			      x2 - right, y1,
			      right, dy);

	    XRenderFreePicture (xdisplay, grad);
	}
	else
	{
	    XRenderFillRectangle (xdisplay, gop, dst, &color[1],
				  x1, y1, x2 - x1, y2 - y1);
	}
    }

    if (!ignore_src_alpha)
	XRenderComposite (xdisplay, op, src, None, dst,
			  xSrc, ySrc,
			  0, 0,
			  x1, y1,
			  x2 - x1, y2 - y1);

    set_no_picture_clip (xdisplay, dst);
}

static void
_decor_blend_vert_border_picture (Display	  *xdisplay,
				  decor_context_t *context,
				  Picture	  src,
				  int	          xSrc,
				  int	          ySrc,
				  Picture	  dst,
				  decor_layout_t  *layout,
				  Region	  region,
				  unsigned short  alpha,
				  int	          shade_alpha,
				  int		  x1,
				  int		  y1,
				  int		  x2,
				  int		  y2,
				  int		  direction,
				  int             ignore_src_alpha)
{
    XRenderColor color[3] = {
	{ 0xffff, 0xffff, 0xffff, 0xffff },
	{  alpha,  alpha,  alpha,  alpha },
	{    0x0,    0x0,    0x0, 0xffff }
    };
    int		 op = PictOpSrc, gop = PictOpSrc;

    if (layout->rotation)
    {
	Region     rotated_region;
	XRectangle rect;
	BoxPtr     pBox = region->rects;
	int	   nBox = region->numRects;

	rotated_region = XCreateRegion ();

	while (nBox--)
	{
	    rect.x      = x1 + pBox->y1;
	    rect.y	= y1 + pBox->x1;
	    rect.width  = pBox->y2 - pBox->y1;
	    rect.height = pBox->x2 - pBox->x1;

	    XUnionRectWithRegion (&rect, rotated_region, rotated_region);

	    pBox++;
	}

	XRenderSetPictureClipRegion (xdisplay, dst, rotated_region);
	XDestroyRegion (rotated_region);
    }
    else
    {
	XOffsetRegion (region, x1, y1);
	XRenderSetPictureClipRegion (xdisplay, dst, region);
	XOffsetRegion (region, -x1, -y1);
    }

    if (ignore_src_alpha)
    {
	if (layout->rotation)
	{
	    XTransform t = {
		{
		    {       0, 1 << 16,       0 },
		    { 1 << 16,       0,       0 },
		    {       0,       0, 1 << 16 }
		}
	    };

	    t.matrix[0][2] = xSrc << 16;
	    t.matrix[1][2] = ySrc << 16;

	    XRenderSetPictureTransform (xdisplay, src, &t);

	    XRenderComposite (xdisplay, PictOpSrc, src, None, dst,
			      0, 0,
			      0, 0,
			      x1, y1, x2 - x1, y2 - y1);
	    XRenderFillRectangle (xdisplay, PictOpAdd, dst, &color[2], x1, y1,
			          x2 - x1, y2 - y1);

	    XRenderSetPictureTransform (xdisplay, src, &xident);
	}
	else
	{
	    XRenderComposite (xdisplay, PictOpSrc, src, None, dst,
			      xSrc, ySrc,
			      0, 0,
			      x1, y1, x2 - x1, y2 - y1);
	    XRenderFillRectangle (xdisplay, PictOpAdd, dst, &color[2], x1, y1,
			      x2 - x1, y2 - y1);
	}
	gop = PictOpInReverse;
    }

    if (alpha != 0xffff)
    {
	op = PictOpIn;

	if (shade_alpha)
	{
	    static XFixed	     stop[2] = { 0, 1 << 16 };
	    Picture		     grad;
	    XLinearGradient	     linear;
	    XRenderPictureAttributes attrib;

	    attrib.repeat = RepeatPad;

	    if (layout->rotation)
	    {
		linear.p1.x = 0;
		linear.p2.x = 0;

		if (direction < 0)
		{
		    linear.p1.y = 0;
		    linear.p2.y = (y2 - y1) << 16;
		}
		else
		{
		    linear.p1.y = (y2 - y1) << 16;
		    linear.p2.y = 0 << 16;
		}
	    }
	    else
	    {
		linear.p1.y = 0;
		linear.p2.y = 0;

		if (direction < 0)
		{
		    linear.p1.x = 0;
		    linear.p2.x = (x2 - x1) << 16;
		}
		else
		{
		    linear.p1.x = (x2 - x1) << 16;
		    linear.p2.x = 0;
		}
	    }

	    grad = XRenderCreateLinearGradient (xdisplay,
						&linear,
						stop,
						color,
						2);

	    XRenderChangePicture (xdisplay, grad, CPRepeat, &attrib);

	    XRenderComposite (xdisplay, gop, grad, None, dst,
			      0, 0,
			      0, 0,
			      x1, y1,
			      x2 - x1, y2 - y1);

	    XRenderFreePicture (xdisplay, grad);
	}
	else
	{
	    XRenderFillRectangle (xdisplay, gop, dst, &color[1],
				  x1, y1, x2 - x1, y2 - y1);
	}
    }

    if (!ignore_src_alpha)
    {
	if (layout->rotation)
	{
	    XTransform t = {
		{
		    {       0, 1 << 16,       0 },
		    { 1 << 16,       0,       0 },
		    {       0,       0, 1 << 16 }
		}
	    };

	    t.matrix[0][2] = xSrc << 16;
	    t.matrix[1][2] = ySrc << 16;

	    XRenderSetPictureTransform (xdisplay, src, &t);

	    XRenderComposite (xdisplay, op, src, None, dst,
			    0, 0,
			    0, 0,
			    x1, y1, x2 - x1, y2 - y1);

	    XRenderSetPictureTransform (xdisplay, src, &xident);
	}
	else
	{
	    XRenderComposite (xdisplay, op, src, None, dst,
			    xSrc, ySrc,
			    0, 0,
			    x1, y1, x2 - x1, y2 - y1);
	}
    }

    set_no_picture_clip (xdisplay, dst);
}

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
			    int             ignore_src_alpha)
{
    int left, right, bottom, top;
    int x1, y1, x2, y2;

    left   = context->extents.left;
    right  = context->extents.right;
    top    = context->extents.top;
    bottom = context->extents.bottom;

    switch (border)
    {
    case BORDER_TOP:
	x1 = layout->top.x1 + context->left_space - left;
	y1 = layout->top.y1 + context->top_space - top;
	x2 = layout->top.x2 - context->right_space + right;
	y2 = layout->top.y2;

	_decor_blend_horz_border_picture (xdisplay,
					context,
					src,
					xSrc,
					ySrc,
					dst,
					layout,
					region,
					alpha,
					shade_alpha,
					x1,
					y1,
					x2,
					y2,
					top,
					-1,
					ignore_src_alpha);

	_decor_pad_border_picture (xdisplay, dst, &layout->top);
	break;
    case BORDER_BOTTOM:
	x1 = layout->bottom.x1 + context->left_space - left;
	y1 = layout->bottom.y1;
	x2 = layout->bottom.x2 - context->right_space + right;
	y2 = layout->bottom.y1 + bottom;

	_decor_blend_horz_border_picture (xdisplay,
					context,
					src,
					xSrc,
					ySrc,
					dst,
					layout,
					region,
					alpha,
					shade_alpha,
					x1,
					y1,
					x2,
					y2,
					bottom,
					1,
					ignore_src_alpha);

	_decor_pad_border_picture (xdisplay, dst, &layout->bottom);
	break;
    case BORDER_LEFT:
	x1 = layout->left.x1;
	y1 = layout->left.y1;
	x2 = layout->left.x2;
	y2 = layout->left.y2;

	if (layout->rotation)
	    y1 += context->left_space - context->extents.left;
	else
	    x1 += context->left_space - context->extents.left;

	_decor_blend_vert_border_picture (xdisplay,
					context,
					src,
					xSrc,
					ySrc,
					dst,
					layout,
					region,
					alpha,
					shade_alpha,
					x1,
					y1,
					x2,
					y2,
					1,
					ignore_src_alpha);

	_decor_pad_border_picture (xdisplay, dst, &layout->left);
	break;
    case BORDER_RIGHT:
	x1 = layout->right.x1;
	y1 = layout->right.y1;
	x2 = layout->right.x2;
	y2 = layout->right.y2;

	if (layout->rotation)
	    y2 -= context->right_space - context->extents.right;
	else
	    x2 -= context->right_space - context->extents.right;

	_decor_blend_vert_border_picture (xdisplay,
					context,
					src,
					xSrc,
					ySrc,
					dst,
					layout,
					region,
					alpha,
					shade_alpha,
					x1,
					y1,
					x2,
					y2,
					-1,
					ignore_src_alpha);

	_decor_pad_border_picture (xdisplay, dst, &layout->right);
	break;
    default:
	break;
    }
}

int
decor_acquire_dm_session (Display    *xdisplay,
			  int	     screen,
			  const char *name,
			  int	     replace_current_dm,
			  Time	     *timestamp)
{
    XEvent		 event;
    XSetWindowAttributes attr;
    Window		 current_dm_sn_owner, new_dm_sn_owner;
    Atom		 dm_sn_atom;
    Atom		 manager_atom;
    Atom		 dm_name_atom;
    Atom		 utf8_string_atom;
    Time		 dm_sn_timestamp;
    char		 buf[128];

    manager_atom = XInternAtom (xdisplay, "MANAGER", FALSE);
    dm_name_atom = XInternAtom (xdisplay, "_COMPIZ_DM_NAME", 0);

    utf8_string_atom = XInternAtom (xdisplay, "UTF8_STRING", 0);

    sprintf (buf, "_COMPIZ_DM_S%d", screen);
    dm_sn_atom = XInternAtom (xdisplay, buf, 0);

    current_dm_sn_owner = XGetSelectionOwner (xdisplay, dm_sn_atom);

    if (current_dm_sn_owner != None)
    {
	if (!replace_current_dm)
	    return DECOR_ACQUIRE_STATUS_OTHER_DM_RUNNING;

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
		     (unsigned char *) name,
		     strlen (name));

    XWindowEvent (xdisplay,
		  new_dm_sn_owner,
		  PropertyChangeMask,
		  &event);

    dm_sn_timestamp = event.xproperty.time;

    XSetSelectionOwner (xdisplay, dm_sn_atom, new_dm_sn_owner,
			dm_sn_timestamp);

    if (XGetSelectionOwner (xdisplay, dm_sn_atom) != new_dm_sn_owner)
    {
	XDestroyWindow (xdisplay, new_dm_sn_owner);

	return DECOR_ACQUIRE_STATUS_FAILED;
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

    XSendEvent (xdisplay, XRootWindow (xdisplay, screen), 0,
		StructureNotifyMask, &event);

    /* Wait for old decoration manager to go away */
    if (current_dm_sn_owner != None)
    {
	do {
	    XWindowEvent (xdisplay, current_dm_sn_owner,
			  StructureNotifyMask, &event);
	} while (event.type != DestroyNotify);
    }

    *timestamp = dm_sn_timestamp;

    return DECOR_ACQUIRE_STATUS_SUCCESS;
}

void
decor_set_dm_check_hint (Display *xdisplay,
			 int	 screen)
{
    XSetWindowAttributes attrs;
    unsigned long	 data;
    Window		 xroot;
    Atom		 atom;

    attrs.override_redirect = 1;
    attrs.event_mask	    = PropertyChangeMask;

    xroot = RootWindow (xdisplay, screen);

    data = XCreateWindow (xdisplay,
			  xroot,
			  -100, -100, 1, 1,
			  0,
			  CopyFromParent,
			  CopyFromParent,
			  (Visual *) CopyFromParent,
			  CWOverrideRedirect | CWEventMask,
			  &attrs);

    atom = XInternAtom (xdisplay, DECOR_SUPPORTING_DM_CHECK_ATOM_NAME, 0);

    XChangeProperty (xdisplay, xroot,
		     atom,
		     XA_WINDOW,
		     32, PropModeReplace, (unsigned char *) &data, 1);
}

/* from fvwm2, Copyright Matthias Clasen, Dominik Vogt */
static int
convert_property (Display *xdisplay,
		  Window  w,
		  Atom    target,
		  Atom    property,
		  Time    dm_sn_timestamp)
{

#define N_TARGETS 4

    Atom conversion_targets[N_TARGETS];
    long icccm_version[] = { 2, 0 };

    conversion_targets[0] = XInternAtom (xdisplay, "TARGETS", 0);
    conversion_targets[1] = XInternAtom (xdisplay, "MULTIPLE", 0);
    conversion_targets[2] = XInternAtom (xdisplay, "TIMESTAMP", 0);
    conversion_targets[3] = XInternAtom (xdisplay, "VERSION", 0);

    if (target == conversion_targets[0])
	XChangeProperty (xdisplay, w, property,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) conversion_targets, N_TARGETS);
    else if (target == conversion_targets[2])
	XChangeProperty (xdisplay, w, property,
			 XA_INTEGER, 32, PropModeReplace,
			 (unsigned char *) &dm_sn_timestamp, 1);
    else if (target == conversion_targets[3])
	XChangeProperty (xdisplay, w, property,
			 XA_INTEGER, 32, PropModeReplace,
			 (unsigned char *) icccm_version, 2);
    else
	return 0;

    /* Be sure the PropertyNotify has arrived so we
     * can send SelectionNotify
     */
    XSync (xdisplay, 0);

    return 1;
}

void
decor_handle_selection_request (Display *xdisplay,
				XEvent  *event,
				Time    timestamp)
{
    XSelectionEvent reply;
    Atom	    multiple_atom;
    Atom	    atom_pair_atom;

    reply.type	    = SelectionNotify;
    reply.display   = xdisplay;
    reply.requestor = event->xselectionrequest.requestor;
    reply.selection = event->xselectionrequest.selection;
    reply.target    = event->xselectionrequest.target;
    reply.property  = None;
    reply.time	    = event->xselectionrequest.time;

    multiple_atom  = XInternAtom (xdisplay, "MULTIPLE", 0);
    atom_pair_atom = XInternAtom (xdisplay, "ATOM_PAIR", 0);

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
				       adata[i], adata[i + 1],
				       timestamp))
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
			      event->xselectionrequest.property,
			      timestamp))
	    reply.property = event->xselectionrequest.property;
    }

    XSendEvent (xdisplay,
		event->xselectionrequest.requestor,
		FALSE, 0L, (XEvent *) &reply);
}

int
decor_handle_selection_clear (Display *xdisplay,
			      XEvent  *xevent,
			      int     screen)
{
    Atom dm_sn_atom;
    char buf[128];

    sprintf (buf, "_COMPIZ_DM_S%d", screen);
    dm_sn_atom = XInternAtom (xdisplay, buf, 0);

    if (xevent->xselectionclear.selection == dm_sn_atom)
	return DECOR_SELECTION_GIVE_UP;

    return DECOR_SELECTION_KEEP;
}
