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
			 double	      y0)
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
    q->p1.y	  = splitY;
    q->p1.gravity = gravity | splitGravity;
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
decor_set_lSrS_window_quads (decor_context_t *c,
			     decor_quad_t    *q,
			     int	     width,
			     int	     height)
{
    int splitY, n, nQuad = 0;

    splitY = (c->top_corner_space - c->bottom_corner_space) / 2;

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
				 splitY,
				 0,
				 0.0,
				 c->top_space + 1.0);

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
				 splitY,
				 0,
				 width - c->right_space,
				 c->top_space + 1.0);

    nQuad += n;

    return nQuad;
}

int
decor_set_lSrStSbS_window_quads (decor_context_t *c,
				 decor_quad_t    *q,
				 int	         width,
				 int	         height)
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
				  width,
				  splitX,
				  0,
				  0.0,
				  0.0);

    q += n; nQuad += n;

    n = decor_set_lSrS_window_quads (c, q, width, height);

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
				  splitX,
				  0,
				  0.0,
				  height - c->bottom_space);

    nQuad += n;

    return nQuad;
}

int
decor_set_lSrStXbS_window_quads (decor_context_t *c,
				 decor_quad_t    *q,
				 int		 width,
				 int		 height,
				 int		 top_stretch_offset)
{
    int    splitX, n, nQuad = 0;
    int    top_left, top_right, y;
    double y0;

    splitX = (c->left_corner_space - c->right_corner_space) / 2;

    top_left  = top_stretch_offset;
    top_right = width - c->left_space - c->right_space - top_left - 1;

    /* if we need a separate line for the shadow */
    if (c->left_corner_space > top_left || c->right_corner_space > top_right)
    {
	y  = -c->extents.top;
	y0 = c->top_space - c->extents.top;

	/* top quads */
	n = decor_set_horz_quad_line (q,
				      c->left_space,
				      c->left_corner_space,
				      c->right_space,
				      c->right_corner_space,
				      -c->top_space,
				      y,
				      GRAVITY_NORTH,
				      width,
				      splitX,
				      0,
				      0.0,
				      0.0);

	q += n; nQuad += n;
    }
    else
    {
	y  = -c->top_space;
	y0 = 0.0;
    }

    /* top quads */
    n = decor_set_horz_quad_line (q,
				  c->left_space,
				  top_left,
				  c->right_space,
				  top_right,
				  y,
				  0,
				  GRAVITY_NORTH,
				  width,
				  top_left,
				  GRAVITY_WEST,
				  0.0,
				  y0);

    q += n; nQuad += n;

    n = decor_set_lSrS_window_quads (c, q, width, height);

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
				  splitX,
				  0,
				  0.0,
				  height - c->bottom_space);

    nQuad += n;

    return nQuad;
}

int
decor_set_lSrStSbN_window_quads (decor_context_t *c,
				 decor_quad_t    *q,
				 int		 width,
				 int		 height,
				 int		 bottom_stretch_offset)
{
    int splitX, n, nQuad = 0;
    int bottom_left, bottom_right;

    splitX = (c->left_corner_space - c->right_corner_space) / 2;

    bottom_left  = bottom_stretch_offset;
    bottom_right = width - c->left_space - c->right_space - bottom_left - 1;

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
				  splitX,
				  0,
				  0.0,
				  0.0);

    q += n; nQuad += n;

    n = decor_set_lSrS_window_quads (c, q, width, height);

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
				  width,
				  bottom_left,
				  GRAVITY_WEST,
				  0.0,
				  height - c->bottom_space);

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
	params[i] = XDoubleToFixed (amp[i - 2] * sum);

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

    c->left_corner_space   = MAX (0, size - solid_left   + shadow_offset_x);
    c->right_corner_space  = MAX (0, size - solid_right  - shadow_offset_x);
    c->top_corner_space    = MAX (0, size - solid_top    + shadow_offset_y);
    c->bottom_corner_space = MAX (0, size - solid_bottom - shadow_offset_y);

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
decor_destroy_shadow (Display	     *xdisplay,
		      decor_shadow_t *shadow)
{
    if (shadow->picture)
	XRenderFreePicture (xdisplay, shadow->picture);

    if (shadow->pixmap)
	XFreePixmap (xdisplay, shadow->pixmap);

    free (shadow);
}

void
decor_fill_picture_extents_with_shadow (Display	        *xdisplay,
					decor_shadow_t  *shadow,
					decor_context_t *context,
					Picture	        picture,
					int	        width,
					int	        height)
{
    static XTransform xident = {
	{
	    { 1 << 16, 0,             0 },
	    { 0,       1 << 16,       0 },
	    { 0,       0,       1 << 16 },
	}
    };
    int w, h, x2, y2, left, right, top, bottom;

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

    x2 = width - right;
    y2 = height - bottom;

    /* top left */
    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, None, picture,
		      0, 0,
		      0, 0,
		      0, 0,
		      left, top);

    /* top right */
    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, None, picture,
		      shadow->width - right, 0,
		      0, 0,
		      x2, 0,
		      right, top);

    /* bottom left */
    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, None, picture,
		      0, shadow->height - bottom,
		      0, 0,
		      0, y2,
		      left, bottom);

    /* bottom right */
    XRenderComposite (xdisplay, PictOpSrc, shadow->picture, None, picture,
		      shadow->width - right, shadow->height - bottom,
		      0, 0,
		      x2, y2,
		      right, bottom);

    if (w > 0)
    {
	int sw = shadow->width - left - right;
	int sx = left;

	if (sw != w)
	{
	    XTransform t = {
		{
		    { (sw << 16) / w,       0, left << 16 },
		    { 0,              1 << 16,          0 },
		    { 0,                    0,    1 << 16 },
		}
	    };

	    sx = 0;

	    XRenderSetPictureTransform (xdisplay, shadow->picture, &t);
	}

	/* top */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, None, picture,
			  sx, 0,
			  0, 0,
			  left, 0,
			  w, top);

	/* bottom */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, None, picture,
			  sx, shadow->height - bottom,
			  0, 0,
			  left, y2,
			  w, bottom);

	if (sw != w)
	    XRenderSetPictureTransform (xdisplay, shadow->picture, &xident);
    }

    if (h > 0)
    {
	int sh = shadow->height - top - bottom;
	int sy = top;

	if (sh != h)
	{
	    XTransform t = {
		{
		    { 1 << 16,              0,         0 },
		    { 0,       (sh << 16) / h, top << 16 },
		    { 0,                    0,   1 << 16 },
		}
	    };

	    sy = 0;

	    XRenderSetPictureTransform (xdisplay, shadow->picture, &t);
	}

	/* left */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, None, picture,
			  0, sy,
			  0, 0,
			  0, top,
			  left, h);

	/* right */
	XRenderComposite (xdisplay, PictOpSrc, shadow->picture, None, picture,
			  shadow->width - right, sy,
			  0, 0,
			  x2, top,
			  right, h);

	if (sh != h)
	    XRenderSetPictureTransform (xdisplay, shadow->picture, &xident);
    }
}

void
decor_blend_transform_picture (Display	       *xdisplay,
			       decor_context_t *context,
			       Picture	       src,
			       int	       xSrc,
			       int	       ySrc,
			       Picture	       dst,
			       int	       width,
			       int	       height,
			       Region	       region,
			       unsigned short  alpha,
			       int	       shade_alpha)
{
    XRenderColor color[3] = {
	{ 0xffff, 0xffff, 0xffff, 0xffff },
	{  alpha,  alpha,  alpha,  alpha }
    };

    XRenderSetPictureClipRegion (xdisplay, dst, region);

    if (shade_alpha)
    {
	static XFixed	stop[2] = { 0, 1 << 16 };
	XTransform      transform = {
	    {
		{ 1 << 16,       0,       0 },
		{       0, 1 << 16,       0 },
		{        0,      0, 1 << 16 }
	    }
	};
	Picture         grad;
	XLinearGradient linear;
	XRadialGradient radial;

	radial.inner.x	    = 0;
	radial.inner.y	    = 0;
	radial.inner.radius = 0;
	radial.outer.x	    = 0;
	radial.outer.y	    = 0;

	/* top left */
	radial.outer.radius = context->extents.left << 16;

	grad = XRenderCreateRadialGradient (xdisplay,
					    &radial,
					    stop,
					    color,
					    2);

	transform.matrix[1][1] = (context->extents.left << 16) /
	    context->extents.top;
	transform.matrix[0][2] = -context->extents.left << 16;
	transform.matrix[1][2] = -context->extents.left << 16;

	XRenderSetPictureTransform (xdisplay, grad, &transform);

	XRenderComposite (xdisplay, PictOpSrc, grad, None, dst,
			  0, 0,
			  0, 0,
			  context->left_space - context->extents.left,
			  context->top_space - context->extents.top,
			  context->extents.left, context->extents.top);

	XRenderFreePicture (xdisplay, grad);

	/* top */
	linear.p1.x = 0;
	linear.p1.y = context->extents.top << 16;
	linear.p2.x = 0;
	linear.p2.y = 0;

	grad = XRenderCreateLinearGradient (xdisplay,
					    &linear,
					    stop,
					    color,
					    2);

	XRenderComposite (xdisplay, PictOpSrc, grad, None, dst,
			  0, 0,
			  0, 0,
			  context->left_space,
			  context->top_space - context->extents.top,
			  width - context->left_space - context->right_space,
			  context->extents.top);

	XRenderFreePicture (xdisplay, grad);

	/* top right */
	radial.outer.radius = context->extents.right << 16;

	grad = XRenderCreateRadialGradient (xdisplay,
					    &radial,
					    stop,
					    color,
					    2);

	transform.matrix[1][1] = (context->extents.right << 16) /
	    context->extents.top;
	transform.matrix[0][2] = 0;
	transform.matrix[1][2] = -context->extents.right << 16;

	XRenderSetPictureTransform (xdisplay, grad, &transform);

	XRenderComposite (xdisplay, PictOpSrc, grad, None, dst,
			  0, 0,
			  0, 0,
			  width - context->right_space,
			  context->top_space - context->extents.top,
			  context->extents.right, context->extents.top);

	XRenderFreePicture (xdisplay, grad);

	/* left */
	linear.p1.x = context->extents.left << 16;
	linear.p1.y = 0;
	linear.p2.x = 0;
	linear.p2.y = 0;

	grad = XRenderCreateLinearGradient (xdisplay,
					    &linear,
					    stop,
					    color,
					    2);

	XRenderComposite (xdisplay, PictOpSrc, grad, None, dst,
			  0, 0,
			  0, 0,
			  context->left_space - context->extents.left,
			  context->top_space,
			  context->extents.left,
			  height - context->top_space - context->bottom_space);

	XRenderFreePicture (xdisplay, grad);

	/* right */
	linear.p1.x = 0;
	linear.p1.y = 0;
	linear.p2.x = context->extents.right << 16;
	linear.p2.y = 0;

	grad = XRenderCreateLinearGradient (xdisplay,
					    &linear,
					    stop,
					    color,
					    2);

	XRenderComposite (xdisplay, PictOpSrc, grad, None, dst,
			  0, 0,
			  0, 0,
			  width - context->right_space, context->top_space,
			  context->extents.right,
			  height - context->top_space - context->bottom_space);

	XRenderFreePicture (xdisplay, grad);

	/* bottom left */
	radial.outer.radius = context->extents.left << 16;

	grad = XRenderCreateRadialGradient (xdisplay,
					    &radial,
					    stop,
					    color,
					    2);

	transform.matrix[1][1] = (context->extents.left << 16) /
	    context->extents.bottom;
	transform.matrix[0][2] = -context->extents.left << 16;
	transform.matrix[1][2] = 0;

	XRenderSetPictureTransform (xdisplay, grad, &transform);

	XRenderComposite (xdisplay, PictOpSrc, grad, None, dst,
			  0, 0,
			  0, 0,
			  context->left_space - context->extents.left,
			  height - context->bottom_space,
			  context->extents.left, context->extents.bottom);

	XRenderFreePicture (xdisplay, grad);

	/* bottom */
	linear.p1.x = 0;
	linear.p1.y = 0;
	linear.p2.x = 0;
	linear.p2.y = context->extents.bottom << 16;

	grad = XRenderCreateLinearGradient (xdisplay,
					    &linear,
					    stop,
					    color,
					    2);

	XRenderComposite (xdisplay, PictOpSrc, grad, None, dst,
			  0, 0,
			  0, 0,
			  context->left_space, height - context->bottom_space,
			  width - context->left_space - context->right_space,
			  context->extents.bottom);

	XRenderFreePicture (xdisplay, grad);

	/* bottom right */
	radial.outer.radius = context->extents.right << 16;

	grad = XRenderCreateRadialGradient (xdisplay,
					    &radial,
					    stop,
					    color,
					    2);

	transform.matrix[1][1] = (context->extents.right << 16) /
	    context->extents.bottom;
	transform.matrix[0][2] = 0;
	transform.matrix[1][2] = 0;

	XRenderSetPictureTransform (xdisplay, grad, &transform);

	XRenderComposite (xdisplay, PictOpSrc, grad, None, dst,
			  0, 0,
			  0, 0,
			  width - context->right_space,
			  height - context->bottom_space,
			  context->extents.right, context->extents.bottom);

	XRenderFreePicture (xdisplay, grad);
    }
    else
    {
	XRenderFillRectangle (xdisplay, PictOpSrc, dst, &color[1],
			      0, 0, width, height);
    }

    XRenderComposite (xdisplay, PictOpIn, src, None, dst,
		      -xSrc, -ySrc,
		      0, 0,
		      0, 0,
		      width, height);

    set_no_picture_clip (xdisplay, dst);
}
