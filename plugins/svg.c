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

#include <stdlib.h>
#include <string.h>

#include <cairo/cairo-xlib.h>
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <compiz-core.h>
#include <decoration.h>

static CompMetadata svgMetadata;

#define SVG_DISPLAY_OPTION_SET 0
#define SVG_DISPLAY_OPTION_NUM 1

static int displayPrivateIndex;

typedef struct _SvgDisplay {
    CompOption opt[SVG_DISPLAY_OPTION_NUM];

    int	screenPrivateIndex;

    HandleCompizEventProc handleCompizEvent;

    FileToImageProc fileToImage;
} SvgDisplay;

typedef struct _SvgScreen {
    int	windowPrivateIndex;

    DrawWindowProc drawWindow;

    WindowMoveNotifyProc   windowMoveNotify;
    WindowResizeNotifyProc windowResizeNotify;

    BoxRec zoom;
} SvgScreen;

typedef struct _SvgSource {
    decor_point_t p1;
    decor_point_t p2;

    RsvgHandle	      *svg;
    RsvgDimensionData dimension;
} SvgSource;

typedef struct _SvgTexture {
    CompTexture texture;
    CompMatrix  matrix;
    cairo_t     *cr;
    Pixmap      pixmap;
    int		width;
    int	        height;
} SvgTexture;

typedef struct _SvgContext {
    SvgSource  *source;
    REGION     box;
    SvgTexture texture[2];
    BoxRec     rect;
    int	       width, height;
} SvgContext;

typedef struct _SvgWindow {
    SvgSource  *source;
    SvgContext *context;
} SvgWindow;

#define GET_SVG_DISPLAY(d)					 \
    ((SvgDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define SVG_DISPLAY(d)			 \
    SvgDisplay *sd = GET_SVG_DISPLAY (d)

#define GET_SVG_SCREEN(s, sd)					     \
    ((SvgScreen *) (s)->base.privates[(sd)->screenPrivateIndex].ptr)

#define SVG_SCREEN(s)						     \
    SvgScreen *ss = GET_SVG_SCREEN (s, GET_SVG_DISPLAY (s->display))

#define GET_SVG_WINDOW(w, ss)					     \
    ((SvgWindow *) (w)->base.privates[(ss)->windowPrivateIndex].ptr)

#define SVG_WINDOW(w)					   \
    SvgWindow *sw = GET_SVG_WINDOW  (w,			   \
		    GET_SVG_SCREEN  (w->screen,		   \
		    GET_SVG_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static void
renderSvg (CompScreen *s,
	   SvgSource  *source,
	   SvgTexture *texture,
	   float      x1,
	   float      y1,
	   float      x2,
	   float      y2,
	   int	      width,
	   int	      height)
{
    float w = x2 - x1;
    float h = y2 - y1;

    cairo_save (texture->cr);

    cairo_set_operator (texture->cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba (texture->cr, 1.0, 1.0, 1.0, 0.0);
    cairo_paint (texture->cr);
    cairo_set_operator (texture->cr, CAIRO_OPERATOR_OVER);

    cairo_scale (texture->cr, 1.0 / w, 1.0 / h);

    cairo_scale (texture->cr,
		 (double) width / source->dimension.width,
		 (double) height / source->dimension.height);

    cairo_translate (texture->cr,
		     -x1 * source->dimension.width,
		     -y1 * source->dimension.height);

    rsvg_handle_render_cairo (source->svg, texture->cr);

    cairo_restore (texture->cr);
}

static Bool
initSvgTexture (CompWindow *w,
		SvgSource  *source,
		SvgTexture *texture,
		int	   width,
		int	   height)
{
    cairo_surface_t *surface;
    CompScreen      *s = w->screen;
    Visual	    *visual;
    int		    depth;

    initTexture (s, &texture->texture);

    texture->width  = width;
    texture->height = height;

    texture->pixmap = None;
    texture->cr     = NULL;

    if (width && height)
    {
	XWindowAttributes attr;
	XGetWindowAttributes (s->display->display, w->id, &attr);

	depth = attr.depth;
	texture->pixmap = XCreatePixmap (s->display->display, s->root,
					 width, height, depth);

	if (!bindPixmapToTexture (s,
				  &texture->texture,
				  texture->pixmap,
				  width, height, depth))
	{
	    fprintf (stderr, "%s: Couldn't bind pixmap 0x%x to "
		     "texture\n", programName, (int) texture->pixmap);

	    XFreePixmap (s->display->display, texture->pixmap);

	    return FALSE;
	}

	visual = attr.visual;
	surface = cairo_xlib_surface_create (s->display->display,
					     texture->pixmap, visual,
					     width, height);
	texture->cr = cairo_create (surface);
	cairo_surface_destroy (surface);
    }

    return TRUE;
}

static void
finiSvgTexture (CompScreen *s,
		SvgTexture *texture)
{
    if (texture->cr)
	cairo_destroy (texture->cr);

    if (texture->pixmap)
	XFreePixmap (s->display->display, texture->pixmap);

    finiTexture (s, &texture->texture);
}

static void
updateWindowSvgMatrix (CompWindow *w)
{
    CompMatrix *m;
    int	       width, height;

    SVG_WINDOW (w);

    width  = sw->context->box.extents.x2 - sw->context->box.extents.x1;
    height = sw->context->box.extents.y2 - sw->context->box.extents.y1;

    m = &sw->context->texture[0].matrix;
    *m = sw->context->texture[0].texture.matrix;

    m->xx *= (float) sw->context->texture[0].width  / width;
    m->yy *= (float) sw->context->texture[0].height / height;

    m->x0 -= (sw->context->box.extents.x1 * m->xx);
    m->y0 -= (sw->context->box.extents.y1 * m->yy);

    m = &sw->context->texture[1].matrix;
    *m = sw->context->texture[1].texture.matrix;

    width  = sw->context->rect.x2 - sw->context->rect.x1;
    height = sw->context->rect.y2 - sw->context->rect.y1;

    m->xx *= (float) sw->context->texture[1].width  / width;
    m->yy *= (float) sw->context->texture[1].height / height;

    m->x0 -= (sw->context->rect.x1 * m->xx);
    m->y0 -= (sw->context->rect.y1 * m->yy);
}

static Bool
svgDrawWindow (CompWindow	      *w,
		 const CompTransform  *transform,
		 const FragmentAttrib *attrib,
		 Region		      region,
		 unsigned int	      mask)
{
    Bool status;

    SVG_SCREEN (w->screen);

    UNWRAP (ss, w->screen, drawWindow);
    status = (*w->screen->drawWindow) (w, transform, attrib, region, mask);
    WRAP (ss, w->screen, drawWindow, svgDrawWindow);

    if (status)
    {
	SVG_WINDOW (w);

	if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	    region = &infiniteRegion;

	if (sw->context && region->numRects)
	{
	    CompTexture *texture = &sw->context->texture[0].texture;
	    CompMatrix  *matrix = &sw->context->texture[0].matrix;
	    REGION	r;

	    r.rects    = &r.extents;
	    r.numRects = 1;

	    r.extents = sw->context->box.extents;

	    if (r.extents.x1 < ss->zoom.x1)
		r.extents.x1 = ss->zoom.x1;
	    if (r.extents.y1 < ss->zoom.y1)
		r.extents.y1 = ss->zoom.y1;
	    if (r.extents.x2 > ss->zoom.x2)
		r.extents.x2 = ss->zoom.x2;
	    if (r.extents.y2 > ss->zoom.y2)
		r.extents.y2 = ss->zoom.y2;

	    w->vCount = w->indexCount = 0;

	    (*w->screen->addWindowGeometry) (w,
					     matrix, 1,
					     &sw->context->box,
					     region);

	    if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
		mask |= PAINT_WINDOW_BLEND_MASK;

	    (*w->screen->drawWindowTexture) (w, texture, attrib, mask);

	    if (r.extents.x1 < r.extents.x2 && r.extents.y1 < r.extents.y2)
	    {
		float xScale, yScale;
		float dx, dy;
		int   width, height;
		int   saveFilter;

		r.extents.x1--;
		r.extents.y1--;
		r.extents.x2++;
		r.extents.y2++;

		xScale = w->screen->width  / (float)
		    (ss->zoom.x2 - ss->zoom.x1);
		yScale = w->screen->height / (float)
		    (ss->zoom.y2 - ss->zoom.y1);

		dx = r.extents.x2 - r.extents.x1;
		dy = r.extents.y2 - r.extents.y1;

		width  = dx * xScale + 0.5f;
		height = dy * yScale + 0.5f;

		if (r.extents.x1 != sw->context->rect.x1 ||
		    r.extents.y1 != sw->context->rect.y1 ||
		    r.extents.x2 != sw->context->rect.x2 ||
		    r.extents.y2 != sw->context->rect.y2 ||
		    width	 != sw->context->width   ||
		    height	 != sw->context->height)
		{
		    float x1, y1, x2, y2;

		    sw->context->rect = r.extents;

		    sw->context->width  = width;
		    sw->context->height = height;

		    dx = sw->context->box.extents.x2 -
			sw->context->box.extents.x1;
		    dy = sw->context->box.extents.y2 -
			sw->context->box.extents.y1;

		    x1 = (r.extents.x1 - sw->context->box.extents.x1) / dx;
		    y1 = (r.extents.y1 - sw->context->box.extents.y1) / dy;
		    x2 = (r.extents.x2 - sw->context->box.extents.x1) / dx;
		    y2 = (r.extents.y2 - sw->context->box.extents.y1) / dy;

		    finiSvgTexture (w->screen, &sw->context->texture[1]);

		    if (initSvgTexture (w, sw->context->source,
					&sw->context->texture[1],
					width, height))
		    {
			renderSvg (w->screen, sw->context->source,
				   &sw->context->texture[1],
				   x1, y1, x2, y2,
				   width, height);

			updateWindowSvgMatrix (w);
		    }
		}

		texture = &sw->context->texture[1].texture;
		matrix = &sw->context->texture[1].matrix;

		w->vCount = w->indexCount = 0;

		saveFilter = w->screen->filter[SCREEN_TRANS_FILTER];
		w->screen->filter[SCREEN_TRANS_FILTER] =
		    COMP_TEXTURE_FILTER_GOOD;

		(*w->screen->addWindowGeometry) (w, matrix, 1, &r, region);
		(*w->screen->drawWindowTexture) (w, texture, attrib, mask);

		w->screen->filter[SCREEN_TRANS_FILTER] = saveFilter;
	    }
	    else if (sw->context->texture[1].width)
	    {
		finiSvgTexture (w->screen, &sw->context->texture[1]);
		initSvgTexture (w, sw->source, &sw->context->texture[1], 0, 0);

		memset (&sw->context->rect, 0, sizeof (BoxRec));

		sw->context->width  = 0;
		sw->context->height = 0;
	    }
	}
    }

    return status;
}

static void
updateWindowSvgContext (CompWindow *w,
			SvgSource  *source)
{
    int x1, y1, x2, y2;

    SVG_WINDOW (w);

    if (sw->context)
    {
	finiSvgTexture (w->screen, &sw->context->texture[0]);
	finiSvgTexture (w->screen, &sw->context->texture[1]);
    }
    else
    {
	sw->context = malloc (sizeof (SvgContext));
	if (!sw->context)
	    return;
    }

    memset (&sw->context->rect, 0, sizeof (BoxRec));

    sw->context->width  = 0;
    sw->context->height = 0;

    initSvgTexture (w, source, &sw->context->texture[1], 0, 0);

    sw->context->source = source;

    sw->context->box.rects    = &sw->context->box.extents;
    sw->context->box.numRects = 1;

    decor_apply_gravity (source->p1.gravity,
			 source->p1.x, source->p1.y,
			 w->width, w->height,
			 &x1, &y1);

    decor_apply_gravity (source->p2.gravity,
			 source->p2.x, source->p2.y,
			 w->width, w->height,
			 &x2, &y2);

    x1 = MAX (x1, 0);
    y1 = MAX (y1, 0);
    x2 = MIN (x2, w->width);
    y2 = MIN (y2, w->height);

    if (!initSvgTexture (w, source, &sw->context->texture[0],
			 w->width, w->height))
    {
	free (sw->context);
	sw->context = NULL;
    }
    else
    {
	renderSvg (w->screen, source, &sw->context->texture[0],
		   0.0f, 0.0f, 1.0f, 1.0f, w->width, w->height);

	initSvgTexture (w, source, &sw->context->texture[1], 0, 0);

	sw->context->box.extents.x1 = x1;
	sw->context->box.extents.y1 = y1;
	sw->context->box.extents.x2 = x2;
	sw->context->box.extents.y2 = y2;

	sw->context->box.extents.x1 += w->attrib.x;
	sw->context->box.extents.y1 += w->attrib.y;
	sw->context->box.extents.x2 += w->attrib.x;
	sw->context->box.extents.y2 += w->attrib.y;

	updateWindowSvgMatrix (w);
    }
}

static Bool
svgSet (CompDisplay     *d,
	CompAction      *action,
	CompActionState state,
	CompOption      *option,
	int	        nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findWindowAtDisplay (d, xid);
    if (w)
    {
	decor_point_t p[2];
	char	      *data;
	RsvgHandle    *svg = NULL;
	GError	      *error = NULL;

	SVG_WINDOW (w);

	memset (p, 0, sizeof (p));

	p[0].gravity = getIntOptionNamed (option, nOption, "gravity0",
					  GRAVITY_NORTH | GRAVITY_WEST);

	p[0].x = getIntOptionNamed (option, nOption, "x0", 0);
	p[0].y = getIntOptionNamed (option, nOption, "y0", 0);

	p[1].gravity = getIntOptionNamed (option, nOption, "gravity1",
					  GRAVITY_SOUTH | GRAVITY_EAST);

	p[1].x = getIntOptionNamed (option, nOption, "x1", 0);
	p[1].y = getIntOptionNamed (option, nOption, "y1", 0);

	data = getStringOptionNamed (option, nOption, "data", 0);
	if (data)
	    svg = rsvg_handle_new_from_data ((guint8 *) data, strlen (data),
					     &error);

	if (sw->source)
	{
	    rsvg_handle_free (sw->source->svg);
	    sw->source->svg = svg;
	}
	else
	{
	    sw->source = malloc (sizeof (SvgSource));
	    if (sw->source)
		sw->source->svg = svg;
	}

	if (sw->source && sw->source->svg)
	{
	    sw->source->p1 = p[0];
	    sw->source->p2 = p[1];

	    sw->source->svg = svg;

	    rsvg_handle_get_dimensions (svg, &sw->source->dimension);

	    updateWindowSvgContext (w, sw->source);
	}
	else
	{
	    if (svg)
		rsvg_handle_free (svg);

	    if (sw->source)
	    {
		free (sw->source);
		sw->source = NULL;
	    }

	    if (sw->context)
	    {
		finiSvgTexture (w->screen, &sw->context->texture[0]);
		free (sw->context);
		sw->context = NULL;
	    }
	}
    }

    return FALSE;
}

static void
svgWindowMoveNotify (CompWindow *w,
		       int	  dx,
		       int	  dy,
		       Bool	  immediate)
{
    SVG_SCREEN (w->screen);
    SVG_WINDOW (w);

    if (sw->context)
    {
	sw->context->box.extents.x1 += dx;
	sw->context->box.extents.y1 += dy;
	sw->context->box.extents.x2 += dx;
	sw->context->box.extents.y2 += dy;

	updateWindowSvgMatrix (w);
    }

    UNWRAP (ss, w->screen, windowMoveNotify);
    (*w->screen->windowMoveNotify) (w, dx, dy, immediate);
    WRAP (ss, w->screen, windowMoveNotify, svgWindowMoveNotify);
}

static void
svgWindowResizeNotify (CompWindow *w,
		       int        dx,
		       int        dy,
		       int        dwidth,
		       int        dheight)
{
    SVG_SCREEN (w->screen);
    SVG_WINDOW (w);

    if (sw->source)
	updateWindowSvgContext (w, sw->source);

    UNWRAP (ss, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w, dx, dy, dwidth, dheight);
    WRAP (ss, w->screen, windowResizeNotify, svgWindowResizeNotify);
}

static void
svgHandleCompizEvent (CompDisplay *d,
		      const char  *pluginName,
		      const char  *eventName,
		      CompOption  *option,
		      int	  nOption)
{
    SVG_DISPLAY (d);

    UNWRAP (sd, d, handleCompizEvent);
    (*d->handleCompizEvent) (d, pluginName, eventName, option, nOption);
    WRAP (sd, d, handleCompizEvent, svgHandleCompizEvent);

    if (strcmp (pluginName, "zoom") == 0)
    {
	CompScreen *s;
	int	   output = getIntOptionNamed (option, nOption, "output", 0);

	s = findScreenAtDisplay (d, getIntOptionNamed (option, nOption,
						       "root", 0));
	if (s && output == 0)
	{
	    SVG_SCREEN (s);

	    if (strcmp (eventName, "in") == 0)
	    {
		ss->zoom.x1 = getIntOptionNamed (option, nOption, "x1", 0);
		ss->zoom.y1 = getIntOptionNamed (option, nOption, "y1", 0);
		ss->zoom.x2 = getIntOptionNamed (option, nOption, "x2", 0);
		ss->zoom.y2 = getIntOptionNamed (option, nOption, "y2", 0);
	    }
	    else if (strcmp (eventName, "out") == 0)
	    {
		memset (&ss->zoom, 0, sizeof (BoxRec));
	    }
	}
    }
}

static Bool
readSvgFileToImage (char *file,
		    int  *width,
		    int  *height,
		    void **data)
{
    cairo_surface_t   *surface;
    FILE	      *fp;
    GError	      *error = NULL;
    RsvgHandle	      *svgHandle;
    RsvgDimensionData svgDimension;

    fp = fopen (file, "r");
    if (!fp)
	return FALSE;

    fclose (fp);

    svgHandle = rsvg_handle_new_from_file (file, &error);
    if (!svgHandle)
	return FALSE;

    rsvg_handle_get_dimensions (svgHandle, &svgDimension);

    *width  = svgDimension.width;
    *height = svgDimension.height;

    *data = malloc (svgDimension.width * svgDimension.height * 4);
    if (!*data)
    {
	rsvg_handle_free (svgHandle);
	return FALSE;
    }

    surface = cairo_image_surface_create_for_data (*data,
						   CAIRO_FORMAT_ARGB32,
						   svgDimension.width,
						   svgDimension.height,
						   svgDimension.width * 4);
    if (surface)
    {
	cairo_t *cr;

	cr = cairo_create (surface);

	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	rsvg_handle_render_cairo (svgHandle, cr);

	cairo_destroy (cr);
	cairo_surface_destroy (surface);
    }

    rsvg_handle_free (svgHandle);

    return TRUE;
}

static char *
svgExtension (const char *name)
{

    if (strlen (name) > 4)
    {
	if (strcasecmp (name + (strlen (name) - 4), ".svg") == 0)
	    return "";
    }

    return ".svg";
}

static Bool
svgFileToImage (CompDisplay *d,
		const char  *path,
		const char  *name,
		int	    *width,
		int	    *height,
		int	    *stride,
		void	    **data)
{
    Bool status = FALSE;
    char *extension = svgExtension (name);
    char *file;
    int  len;

    SVG_DISPLAY (d);

    len = (path ? strlen (path) : 0) + strlen (name) + strlen (extension) + 2;

    file = malloc (len);
    if (file)
    {
	if (path)
	    sprintf (file, "%s/%s%s", path, name, extension);
	else
	    sprintf (file, "%s%s", name, extension);

	status = readSvgFileToImage (file, width, height, data);

	free (file);

	if (status)
	{
	    *stride = *width * 4;
	    return TRUE;
	}
    }

    UNWRAP (sd, d, fileToImage);
    status = (*d->fileToImage) (d, path, name, width, height, stride, data);
    WRAP (sd, d, fileToImage, svgFileToImage);

    return status;
}

static CompOption *
svgGetDisplayOptions (CompPlugin  *plugin,
		      CompDisplay *display,
		      int	  *count)
{
    SVG_DISPLAY (display);

    *count = NUM_OPTIONS (sd);
    return sd->opt;
}

static Bool
svgSetDisplayOption (CompPlugin      *plugin,
		     CompDisplay     *display,
		     const char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;

    SVG_DISPLAY (display);

    o = compFindOption (sd->opt, NUM_OPTIONS (sd), name, NULL);
    if (!o)
	return FALSE;

    return compSetDisplayOption (display, o, value);
}

static const CompMetadataOptionInfo svgDisplayOptionInfo[] = {
    { "set", "action", 0, svgSet, NULL }
};

static Bool
svgInitDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    SvgDisplay *sd;
    CompScreen *s;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    sd = malloc (sizeof (SvgDisplay));
    if (!sd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &svgMetadata,
					     svgDisplayOptionInfo,
					     sd->opt,
					     SVG_DISPLAY_OPTION_NUM))
    {
	free (sd);
	return FALSE;
    }

    sd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (sd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, sd->opt, SVG_DISPLAY_OPTION_NUM);
	free (sd);
	return FALSE;
    }

    WRAP (sd, d, handleCompizEvent, svgHandleCompizEvent);
    WRAP (sd, d, fileToImage, svgFileToImage);

    d->base.privates[displayPrivateIndex].ptr = sd;

    for (s = d->screens; s; s = s->next)
	updateDefaultIcon (s);

    return TRUE;
}

static void
svgFiniDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    CompScreen *s;

    SVG_DISPLAY (d);

    UNWRAP (sd, d, handleCompizEvent);
    UNWRAP (sd, d, fileToImage);

    for (s = d->screens; s; s = s->next)
	updateDefaultIcon (s);

    freeScreenPrivateIndex (d, sd->screenPrivateIndex);

    compFiniDisplayOptions (d, sd->opt, SVG_DISPLAY_OPTION_NUM);

    free (sd);
}

static Bool
svgInitScreen (CompPlugin *p,
	       CompScreen *s)
{
    SvgScreen *ss;

    SVG_DISPLAY (s->display);

    ss = malloc (sizeof (SvgScreen));
    if (!ss)
	return FALSE;

    ss->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ss->windowPrivateIndex < 0)
    {
	free (ss);
	return FALSE;
    }

    memset (&ss->zoom, 0, sizeof (BoxRec));

    WRAP (ss, s, drawWindow, svgDrawWindow);
    WRAP (ss, s, windowMoveNotify, svgWindowMoveNotify);
    WRAP (ss, s, windowResizeNotify, svgWindowResizeNotify);

    s->base.privates[sd->screenPrivateIndex].ptr = ss;

    return TRUE;
}

static void
svgFiniScreen (CompPlugin *p,
	       CompScreen *s)
{
    SVG_SCREEN (s);

    freeWindowPrivateIndex (s, ss->windowPrivateIndex);

    UNWRAP (ss, s, drawWindow);
    UNWRAP (ss, s, windowMoveNotify);
    UNWRAP (ss, s, windowResizeNotify);

    free (ss);
}

static Bool
svgInitWindow (CompPlugin *p,
	       CompWindow *w)
{
    SvgWindow *sw;

    SVG_SCREEN (w->screen);

    sw = malloc (sizeof (SvgWindow));
    if (!sw)
	return FALSE;

    sw->source  = NULL;
    sw->context = NULL;

    w->base.privates[ss->windowPrivateIndex].ptr = sw;

    return TRUE;
}

static void
svgFiniWindow (CompPlugin *p,
	       CompWindow *w)
{
    SVG_WINDOW (w);

    if (sw->source)
    {
	rsvg_handle_free (sw->source->svg);
	free (sw->source);
    }

    if (sw->context)
    {
	finiSvgTexture (w->screen, &sw->context->texture[0]);
	free (sw->context);
    }

    free (sw);
}

static CompBool
svgInitObject (CompPlugin *p,
	       CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) svgInitDisplay,
	(InitPluginObjectProc) svgInitScreen,
	(InitPluginObjectProc) svgInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
svgFiniObject (CompPlugin *p,
	       CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) svgFiniDisplay,
	(FiniPluginObjectProc) svgFiniScreen,
	(FiniPluginObjectProc) svgFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
svgGetObjectOptions (CompPlugin *plugin,
		     CompObject *object,
		     int	*count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) svgGetDisplayOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
svgSetObjectOption (CompPlugin      *plugin,
		    CompObject      *object,
		    const char      *name,
		    CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) svgSetDisplayOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
svgInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&svgMetadata,
					 p->vTable->name,
					 svgDisplayOptionInfo,
					 SVG_DISPLAY_OPTION_NUM,
					 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&svgMetadata);
	return FALSE;
    }

    rsvg_init ();

    compAddMetadataFromFile (&svgMetadata, p->vTable->name);

    return TRUE;
}

static void
svgFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&svgMetadata);

    rsvg_term ();
}

static CompMetadata *
svgGetMetadata (CompPlugin *plugin)
{
    return &svgMetadata;
}

CompPluginVTable svgVTable = {
    "svg",
    svgGetMetadata,
    svgInit,
    svgFini,
    svgInitObject,
    svgFiniObject,
    svgGetObjectOptions,
    svgSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &svgVTable;
}
