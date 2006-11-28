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

#include <stdlib.h>
#include <math.h>

#include <decoration.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

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

static XFixed *
create_gaussian_kernel (double radius,
			double sigma,
			double alpha,
			double opacity,
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
	return NULL;

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
	params[i] = XDoubleToFixed (amp[i - 2] * sum * opacity * 1.2);

    free (amp);

    *r_size = size;

    return params;
}

#define SIGMA(r) ((r) / 2.0)
#define ALPHA(r) (r)

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
		     decor_context_t	    *c,
		     decor_draw_func_t	    *draw,
		     void		    *closure)
{
    XRenderPictFormat   *format;
    Pixmap		pixmap;
    Picture		src, dst, tmp;
    XFixed		*params;
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

    shadow = malloc (sizeof (decor_shadow_t));
    if (!shadow)
	return NULL;

    shadow->pixmap  = 0;
    shadow->picture = 0;
    shadow->width   = 0;
    shadow->height  = 0;

    color.red   = opt->shadow_color[0];
    color.green = opt->shadow_color[1];
    color.blue  = opt->shadow_color[2];
    color.alpha = 0xffff;

    shadow_offset_x = opt->shadow_offset_x;
    shadow_offset_y = opt->shadow_offset_y;

    /* compute a gaussian convolution kernel */
    params = create_gaussian_kernel (opt->shadow_radius,
				     SIGMA (opt->shadow_radius),
				     ALPHA (opt->shadow_radius),
				     opt->shadow_opacity,
				     &size);
    if (!params)
	shadow_offset_x = shadow_offset_y = size = 0;

    if (opt->shadow_radius <= 0.0 &&
	shadow_offset_x == 0	  &&
	shadow_offset_y == 0)
	size = 0;

    n_params = size + 2;
    size     = size / 2;

    c->left_space   = left   + size - shadow_offset_x;
    c->right_space  = right  + size + shadow_offset_x;
    c->top_space    = top    + size - shadow_offset_y;
    c->bottom_space = bottom + size + shadow_offset_y;

    c->left_space   = MAX (left,   c->left_space);
    c->right_space  = MAX (right,  c->right_space);
    c->top_space    = MAX (top,    c->top_space);
    c->bottom_space = MAX (bottom, c->bottom_space);

    c->left_corner_space   = MAX (0, size + shadow_offset_x);
    c->right_corner_space  = MAX (0, size - shadow_offset_x);
    c->top_corner_space    = MAX (0, size + shadow_offset_y);
    c->bottom_corner_space = MAX (0, size - shadow_offset_y);

    d_width  = c->left_space + c->left_corner_space + width +
	c->right_corner_space + c->right_space;
    d_height = c->top_space + c->top_corner_space + height +
	c->bottom_corner_space + c->bottom_space;

    /* all pixmaps are ARGB32 */
    format = XRenderFindStandardFormat (xdisplay, PictStandardARGB32);

    /* shadow color */
    src = XRenderCreateSolidFill (xdisplay, &color);

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
	free (params);
	XFreePixmap (xdisplay, pixmap);

	return shadow;
    }

    dst = XRenderCreatePicture (xdisplay, d_pixmap, format, 0, NULL);
    tmp = XRenderCreatePicture (xdisplay, pixmap, format, 0, NULL);

    /* draw decoration */
    (*draw) (xdisplay, d_pixmap, dst, d_width, d_height, closure);

    /* first pass */
    params[0] = (n_params - 2) << 16;
    params[1] = 1 << 16;

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

    /* second pass */
    params[0] = 1 << 16;
    params[1] = (n_params - 2) << 16;

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

    XRenderFreePicture (xdisplay, tmp);
    XRenderFreePicture (xdisplay, src);

    XFreePixmap (xdisplay, pixmap);

    shadow->pixmap  = d_pixmap;
    shadow->picture = dst;
    shadow->width   = d_width;
    shadow->height  = d_height;

    free (params);

    return shadow;
}

void
decor_destroy_shadow (Display	     *xdisplay,
		      decor_shadow_t *shadow)
{
    if (shadow->picture)
	XRenderFreePicture (xdisplay, shadow->picture);

    if (shadow->pixmap)
	XFreePixmap (xdisplay, shadow->pixmap);

    free (shadow);
}
