/*
 * Copyright © 2007 Novell, Inc.
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
#include <unistd.h>

#include <compiz-core.h>
#include <decoration.h>

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

/*
 * compiz composited video
 *
 * supported image formats:
 *
 * RGB - packed RGB colorspace
 *
 *   +---------------+
 *   |               | width  = image-width
 *   |               | height = image-height
 *   |      RGB      |
 *   |               | any pixmap depth with a matching
 *   |               | fb-config can be used.
 *   +---------------+

 * YV12 - planar YV12 colorspace
 *
 *   +---------------+
 *   |               | width  = image-width
 *   |               | height = image-height + image-height / 2
 *   |       Y       | depth  = 8
 *   |               |
 *   |               | alpha only fb-config with pixmap support
 *   +-------+-------+ must be available.
 *   |       |       |
 *   |   V   |   U   |
 *   |       |       |
 *   +---------------+
 *
 */

static CompMetadata videoMetadata;

typedef struct _VideoTexture {
    struct _VideoTexture *next;
    int			 refCount;
    Pixmap		 pixmap;
    int			 width;
    int			 height;
    Damage		 damage;
    CompTexture		 texture;
} VideoTexture;

typedef struct _VideoFunction {
    struct _VideoFunction *next;

    int handle;
    int target;
    int param;
} VideoFunction;

#define IMAGE_FORMAT_RGB  0
#define IMAGE_FORMAT_YV12 1
#define IMAGE_FORMAT_NUM  2

static int displayPrivateIndex;

#define VIDEO_DISPLAY_OPTION_YV12 0
#define VIDEO_DISPLAY_OPTION_NUM  1

typedef struct _VideoDisplay {
    int		     screenPrivateIndex;
    HandleEventProc  handleEvent;
    VideoTexture     *textures;
    Atom	     videoAtom;
    Atom	     videoSupportedAtom;
    Atom	     videoImageFormatAtom[IMAGE_FORMAT_NUM];

    CompOption opt[VIDEO_DISPLAY_OPTION_NUM];
} VideoDisplay;

typedef struct _VideoScreen {
    int	windowPrivateIndex;

    DrawWindowProc	  drawWindow;
    DrawWindowTextureProc drawWindowTexture;
    DamageWindowRectProc  damageWindowRect;

    WindowMoveNotifyProc   windowMoveNotify;
    WindowResizeNotifyProc windowResizeNotify;

    VideoFunction *yv12Functions;

    Bool imageFormat[IMAGE_FORMAT_NUM];
} VideoScreen;

typedef struct _VideoSource {
    VideoTexture  *texture;
    int		  format;
    decor_point_t p1;
    decor_point_t p2;
    Bool	  aspect;
    float	  aspectRatio;
    float	  panScan;
    int		  width;
    int		  height;
} VideoSource;

typedef struct _VideoContext {
    VideoSource *source;
    int		width;
    int	        height;
    REGION      box;
    CompMatrix  matrix;
    Bool        scaled;
    float       panX;
    float       panY;
    Bool        full;
} VideoContext;

typedef struct _VideoWindow {
    VideoSource  *source;
    VideoContext *context;
} VideoWindow;

#define GET_VIDEO_DISPLAY(d)					   \
    ((VideoDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define VIDEO_DISPLAY(d)		     \
    VideoDisplay *vd = GET_VIDEO_DISPLAY (d)

#define GET_VIDEO_SCREEN(s, vd)					       \
    ((VideoScreen *) (s)->base.privates[(vd)->screenPrivateIndex].ptr)

#define VIDEO_SCREEN(s)							   \
    VideoScreen *vs = GET_VIDEO_SCREEN (s, GET_VIDEO_DISPLAY (s->display))

#define GET_VIDEO_WINDOW(w, vs)					       \
    ((VideoWindow *) (w)->base.privates[(vs)->windowPrivateIndex].ptr)

#define VIDEO_WINDOW(w)					       \
    VideoWindow *vw = GET_VIDEO_WINDOW  (w,		       \
		      GET_VIDEO_SCREEN  (w->screen,	       \
		      GET_VIDEO_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static void
videoSetSupportedHint (CompScreen *s)
{
    Atom data[16];
    int  i, n = 0;

    VIDEO_DISPLAY (s->display);
    VIDEO_SCREEN (s);

    for (i = 0; i < IMAGE_FORMAT_NUM; i++)
    {
	if (!vs->imageFormat[i])
	    continue;

	if (i == 0 || vd->opt[i - 1].value.b)
	    data[n++] = vd->videoImageFormatAtom[i];
    }

    XChangeProperty (s->display->display, s->root,
		     vd->videoSupportedAtom, XA_ATOM, 32,
		     PropModeReplace, (unsigned char *) data, n);
}

static CompOption *
videoGetDisplayOptions (CompPlugin  *plugin,
			CompDisplay *display,
			int	    *count)
{
    VIDEO_DISPLAY (display);

    *count = NUM_OPTIONS (vd);
    return vd->opt;
}

static Bool
videoSetDisplayOption (CompPlugin      *plugin,
		       CompDisplay     *display,
		       const char      *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    VIDEO_DISPLAY (display);

    o = compFindOption (vd->opt, NUM_OPTIONS (vd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case VIDEO_DISPLAY_OPTION_YV12:
	if (compSetBoolOption (o, value))
	{
	    CompScreen *s;

	    for (s = display->screens; s; s = s->next)
		videoSetSupportedHint (s);
	}
    default:
	break;
    }

    return FALSE;
}

static int
getYV12FragmentFunction (CompScreen  *s,
			 CompTexture *texture,
			 int	     param)
{
    VideoFunction    *function;
    CompFunctionData *data;
    int		     target;

    VIDEO_SCREEN (s);

    if (texture->target == GL_TEXTURE_2D)
	target = COMP_FETCH_TARGET_2D;
    else
	target = COMP_FETCH_TARGET_RECT;

    for (function = vs->yv12Functions; function; function = function->next)
	if (function->param == param && function->target == target)
	    return function->handle;

    data = createFunctionData ();
    if (data)
    {
	static char *temp[] = { "uv", "tmp", "position" };
	int	    i, handle = 0;
	char	    str[1024];
	Bool	    ok = TRUE;

	for (i = 0; i < sizeof (temp) / sizeof (temp[0]); i++)
	    ok &= addTempHeaderOpToFunctionData (data, temp[i]);

	snprintf (str, 1024,
		  "MOV position, fragment.texcoord[0];"
		  "MAX position, position, program.env[%d];"
		  "MIN position, position, program.env[%d].zwww;",
		  param, param);

	ok &= addDataOpToFunctionData (data, str);

	if (target == COMP_FETCH_TARGET_RECT)
	{
	    snprintf (str, 1024,
		      "TEX output, position, texture[0], RECT;"
		      "MOV output, output.a;");

	    ok &= addDataOpToFunctionData (data, str);

	    if (s->glxPixmapFBConfigs[8].yInverted)
	    {
		snprintf (str, 1024,
			  "MAD position, position, 0.5, program.env[%d].xy;",
			  param + 1);
	    }
	    else
	    {
		snprintf (str, 1024,
			  "ADD position, position, program.env[%d].xy;"
			  "MUL position, position, 0.5;",
			  param + 1);
	    }

	    ok &= addDataOpToFunctionData (data, str);

	    snprintf (str, 1024,
		      "TEX tmp, position, texture[0], RECT;"
		      "MOV uv, tmp.a;"
		      "MAD output, output, 1.164, -0.073;"
		      "ADD position.x, position.x, program.env[%d].z;"
		      "TEX tmp, position, texture[0], RECT;"
		      "MOV uv.y, tmp.a;",
		      param + 1);
	}
	else
	{
	    snprintf (str, 1024,
		      "TEX output, position, texture[0], 2D;"
		      "MOV output, output.a;");

	    ok &= addDataOpToFunctionData (data, str);

	    if (s->glxPixmapFBConfigs[8].yInverted)
	    {
		snprintf (str, 1024,
			  "MAD position, position, 0.5, { 0.0, %f };",
			  2.0f / 3.0f);
	    }
	    else
	    {
		snprintf (str, 1024,
			  "SUB position, position, { 0.0, %f };"
			  "MUL position, position, 0.5;",
			  1.0f / 3.0f);
	    }

	    ok &= addDataOpToFunctionData (data, str);

	    snprintf (str, 1024,
		      "TEX tmp, position, texture[0], 2D;"
		      "MOV uv, tmp.a;"
		      "MAD output, output, 1.164, -0.073;"
		      "ADD position.x, position.x, 0.5;"
		      "TEX tmp, position, texture[0], 2D;"
		      "MOV uv.y, tmp.a;");
	}

	ok &= addDataOpToFunctionData (data, str);

	snprintf (str, 1024,
		  "SUB uv, uv, { 0.5, 0.5 };"
		  "MAD output.xyz, { 1.596, -0.813,   0.0 }, uv.xxxw, output;"
		  "MAD output.xyz, {   0.0, -0.391, 2.018 }, uv.yyyw, output;"
		  "MOV output.a, 1.0;");

	ok &= addDataOpToFunctionData (data, str);

	if (!ok)
	{
	    destroyFunctionData (data);
	    return 0;
	}

	function = malloc (sizeof (VideoFunction));
	if (function)
	{
	    handle = createFragmentFunction (s, "video", data);

	    function->handle = handle;
	    function->target = target;
	    function->param  = param;

	    function->next = vs->yv12Functions;
	    vs->yv12Functions = function;
	}

	destroyFunctionData (data);

	return handle;
    }

    return 0;
}

static void
videoDestroyFragmentFunctions (CompScreen    *s,
			       VideoFunction **videoFunctions)
{
    VideoFunction *function, *next;

    function = *videoFunctions;
    while (function)
    {
	destroyFragmentFunction (s, function->handle);

	next = function->next;
	free (function);
	function = next;
    }

    *videoFunctions = NULL;
}

static void
videoDrawWindowTexture (CompWindow	     *w,
			CompTexture	     *texture,
			const FragmentAttrib *attrib,
			unsigned int	     mask)
{
    CompScreen *s = w->screen;

    VIDEO_SCREEN (s);
    VIDEO_WINDOW (w);

    if (vw->context)
    {
	VideoSource *src = vw->context->source;

	if (src->format == IMAGE_FORMAT_YV12 &&
	    &src->texture->texture == texture)
	{
	    FragmentAttrib fa = *attrib;
	    int		   param, function;

	    param = allocFragmentParameters (&fa, 2);

	    function = getYV12FragmentFunction (s, texture, param);
	    if (function)
	    {
		float minX, minY, maxX, maxY, y1, y2;

		addFragmentFunction (&fa, function);

		minX = COMP_TEX_COORD_X (&texture->matrix, 1.0f);
		maxX = COMP_TEX_COORD_X (&texture->matrix, src->width - 1.0f);

		y1 = COMP_TEX_COORD_Y (&texture->matrix, 1.0f);
		y2 = COMP_TEX_COORD_Y (&texture->matrix, src->height - 1.0f);

		minY = MIN (y1, y2);
		maxY = MAX (y1, y2);

		(*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB, param,
					     minX, minY, maxX, maxY);

		/* need to provide plane offsets when texture coordinates
		   are not normalized */
		if (texture->target != GL_TEXTURE_2D)
		{
		    float offsetX, offsetY;

		    offsetX = COMP_TEX_COORD_X (&texture->matrix,
						src->width / 2);

		    if (s->glxPixmapFBConfigs[8].yInverted)
			offsetY = COMP_TEX_COORD_Y (&texture->matrix,
						    src->height);
		    else
			offsetY = COMP_TEX_COORD_Y (&texture->matrix,
						    -src->height / 2);

		    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
						 param + 1,
						 0.0f, offsetY, offsetX, 0.0f);
		}
	    }

	    UNWRAP (vs, s, drawWindowTexture);
	    (*s->drawWindowTexture) (w, texture, &fa, mask);
	    WRAP (vs, s, drawWindowTexture, videoDrawWindowTexture);
	}
	else
	{
	    if (!(mask & PAINT_WINDOW_BLEND_MASK))
	    {
		/* we don't have to draw client window texture when
		   video cover the full window and blending isn't used */
		if (vw->context->full && texture == w->texture)
		    return;
	    }

	    UNWRAP (vs, s, drawWindowTexture);
	    (*s->drawWindowTexture) (w, texture, attrib, mask);
	    WRAP (vs, s, drawWindowTexture, videoDrawWindowTexture);
	}
    }
    else
    {
	UNWRAP (vs, s, drawWindowTexture);
	(*s->drawWindowTexture) (w, texture, attrib, mask);
	WRAP (vs, s, drawWindowTexture, videoDrawWindowTexture);
    }
}

static Bool
videoDrawWindow (CompWindow	      *w,
		 const CompTransform  *transform,
		 const FragmentAttrib *attrib,
		 Region		      region,
		 unsigned int	      mask)
{
    Bool status;

    VIDEO_SCREEN (w->screen);

    UNWRAP (vs, w->screen, drawWindow);
    status = (*w->screen->drawWindow) (w, transform, attrib, region, mask);
    WRAP (vs, w->screen, drawWindow, videoDrawWindow);

    if (status)
    {
	VIDEO_WINDOW (w);

	if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	    region = &infiniteRegion;

	if (vw->context && region->numRects)
	{
	    CompTexture *texture = &vw->context->source->texture->texture;
	    int		saveFilter;

	    w->vCount = w->indexCount = 0;

	    if (vw->context->box.extents.x1 < vw->context->box.extents.x2 &&
		vw->context->box.extents.y1 < vw->context->box.extents.y2)
	    {
		(*w->screen->addWindowGeometry) (w,
						 &vw->context->matrix, 1,
						 &vw->context->box,
						 region);
	    }

	    if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
		mask |= PAINT_WINDOW_BLEND_MASK;

	    saveFilter = w->screen->filter[NOTHING_TRANS_FILTER];

	    if (vw->context->scaled)
		w->screen->filter[NOTHING_TRANS_FILTER] =
		    COMP_TEXTURE_FILTER_GOOD;

	    if (w->vCount)
		(*w->screen->drawWindowTexture) (w, texture, attrib, mask);

	    w->screen->filter[NOTHING_TRANS_FILTER] = saveFilter;
	}
    }

    return status;
}

static VideoTexture *
videoGetTexture (CompScreen *screen,
		 Pixmap	    pixmap)
{
    VideoTexture *texture;
    unsigned int width, height, depth, ui;
    Window	 root;
    int		 i;

    VIDEO_DISPLAY (screen->display);

    for (texture = vd->textures; texture; texture = texture->next)
    {
	if (texture->pixmap == pixmap)
	{
	    texture->refCount++;
	    return texture;
	}
    }

    texture = malloc (sizeof (VideoTexture));
    if (!texture)
	return NULL;

    initTexture (screen, &texture->texture);

    if (!XGetGeometry (screen->display->display, pixmap, &root,
		       &i, &i, &width, &height, &ui, &depth))
    {
	finiTexture (screen, &texture->texture);
	free (texture);
	return NULL;
    }

    if (!bindPixmapToTexture (screen, &texture->texture, pixmap,
			      width, height, depth))
    {
	finiTexture (screen, &texture->texture);
	free (texture);
	return NULL;
    }

    texture->damage = XDamageCreate (screen->display->display, pixmap,
				     XDamageReportRawRectangles);

    texture->refCount = 1;
    texture->pixmap   = pixmap;
    texture->width    = width;
    texture->height   = height;
    texture->next     = vd->textures;

    vd->textures = texture;

    return texture;
}

static void
videoReleaseTexture (CompScreen   *screen,
		     VideoTexture *texture)
{
    VIDEO_DISPLAY (screen->display);

    texture->refCount--;
    if (texture->refCount)
	return;

    if (texture == vd->textures)
    {
	vd->textures = texture->next;
    }
    else
    {
	VideoTexture *t;

	for (t = vd->textures; t; t = t->next)
	{
	    if (t->next == texture)
	    {
		t->next = texture->next;
		break;
	    }
	}
    }

    finiTexture (screen, &texture->texture);
    free (texture);
}

static void
updateWindowVideoMatrix (CompWindow *w)
{
    VIDEO_WINDOW (w);

    if (!vw->context)
	return;

    vw->context->matrix = vw->context->source->texture->texture.matrix;

    vw->context->matrix.yy /= (float)
	vw->context->height / vw->context->source->height;

    if (vw->context->width  != vw->context->source->width ||
	vw->context->height != vw->context->source->height)
    {
	vw->context->matrix.xx /= (float)
	    vw->context->width / vw->context->source->width;

	vw->context->scaled = TRUE;
    }
    else
    {
	vw->context->scaled = FALSE;
    }

    vw->context->matrix.x0 -=
	(vw->context->box.extents.x1 * vw->context->matrix.xx);
    vw->context->matrix.y0 -=
	(vw->context->box.extents.y1 * vw->context->matrix.yy);

    vw->context->matrix.x0 += (vw->context->panX * vw->context->matrix.xx);
    vw->context->matrix.y0 += (vw->context->panY * vw->context->matrix.yy);
}

static void
updateWindowVideoContext (CompWindow  *w,
			  VideoSource *source)
{
    int x1, y1, x2, y2;

    VIDEO_WINDOW (w);

    if (!vw->context)
    {
	vw->context = malloc (sizeof (VideoContext));
	if (!vw->context)
	    return;
    }

    vw->context->source = source;

    vw->context->box.rects    = &vw->context->box.extents;
    vw->context->box.numRects = 1;

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

    vw->context->width  = x2 - x1;
    vw->context->height = y2 - y1;

    vw->context->panX = 0.0f;
    vw->context->panY = 0.0f;

    if (source->aspect)
    {
	float aspect, width, height, v;

	width  = vw->context->width;
	height = vw->context->height;

	aspect = width / height;

	if (aspect < source->aspectRatio)
	{
	    v = (width + width * source->panScan) / source->aspectRatio;
	    height = MIN (vw->context->height, v);
	    width  = height * source->aspectRatio;
	}
	else
	{
	    v = (height + height * source->panScan) * source->aspectRatio;
	    width  = MIN (vw->context->width, v);
	    height = width / source->aspectRatio;
	}

	x1 = (vw->context->width  / 2.0f) - (width  / 2.0f);
	y1 = (vw->context->height / 2.0f) - (height / 2.0f);
	x2 = ((vw->context->width  / 2.0f) + (width  / 2.0f) + 0.5f);
	y2 = ((vw->context->height / 2.0f) + (height / 2.0f) + 0.5f);

	vw->context->width  = x2 - x1;
	vw->context->height = y2 - y1;

	if (x1 < 0)
	    vw->context->panX = -x1;

	if (y1 < 0)
	    vw->context->panY = -y1;

	x1 = MAX (x1, 0);
	y1 = MAX (y1, 0);
	x2 = MIN (x2, w->width);
	y2 = MIN (y2, w->height);
    }

    if (x1 == 0	       &&
	y1 == 0	       &&
	x2 == w->width &&
	y2 == w->height)
    {
	vw->context->full = TRUE;
    }
    else
    {
	vw->context->full = FALSE;
    }

    vw->context->box.extents.x1 = x1;
    vw->context->box.extents.y1 = y1;
    vw->context->box.extents.x2 = x2;
    vw->context->box.extents.y2 = y2;

    vw->context->box.extents.x1 += w->attrib.x;
    vw->context->box.extents.y1 += w->attrib.y;
    vw->context->box.extents.x2 += w->attrib.x;
    vw->context->box.extents.y2 += w->attrib.y;

    updateWindowVideoMatrix (w);
}

static void
videoWindowUpdate (CompWindow *w)
{
    Atom	  actual;
    int		  result, format, i;
    unsigned long n, left;
    unsigned char *propData;
    VideoTexture  *texture = NULL;
    Pixmap	  pixmap = None;
    Atom	  imageFormat = 0;
    decor_point_t p[2];
    int		  aspectX = 0;
    int		  aspectY = 0;
    int		  panScan = 0;
    int		  width = 0;
    int		  height = 0;

    VIDEO_DISPLAY (w->screen->display);
    VIDEO_SCREEN (w->screen);
    VIDEO_WINDOW (w);

    memset (p, 0, sizeof (p));

    result = XGetWindowProperty (w->screen->display->display, w->id,
				 vd->videoAtom, 0L, 13L, FALSE,
				 XA_INTEGER, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && propData)
    {
	if (n == 13)
	{
	    long *data = (long *) propData;

	    pixmap	= *data++;
	    imageFormat = *data++;

	    width  = *data++;
	    height = *data++;

	    aspectX = *data++;
	    aspectY = *data++;
	    panScan = *data++;

	    p[0].gravity = *data++;
	    p[0].x       = *data++;
	    p[0].y       = *data++;
	    p[1].gravity = *data++;
	    p[1].x       = *data++;
	    p[1].y       = *data++;
	}

	XFree (propData);
    }

    for (i = 0; i < IMAGE_FORMAT_NUM; i++)
	if (vd->videoImageFormatAtom[i] == imageFormat)
	    break;

    if (i < IMAGE_FORMAT_NUM)
    {
	if (!vs->imageFormat[i])
	{
	    compLogMessage ("video", CompLogLevelWarn,
			    "Image format not supported");
	    i = IMAGE_FORMAT_NUM;
	}
    }

    if (i < IMAGE_FORMAT_NUM)
    {
	texture = videoGetTexture (w->screen, pixmap);
	if (!texture)
	{
	    compLogMessage ("video", CompLogLevelWarn,
			    "Bad pixmap 0x%x", (int) pixmap);
	}
    }

    if (vw->source)
    {
	videoReleaseTexture (w->screen, vw->source->texture);
    }
    else
    {
	vw->source = malloc (sizeof (VideoSource));
    }

    if (texture && vw->source)
    {
	vw->source->texture = texture;
	vw->source->format  = i;
	vw->source->p1	    = p[0];
	vw->source->p2	    = p[1];
	vw->source->width   = width;
	vw->source->height  = height;
	vw->source->aspect  = aspectX && aspectY;
	vw->source->panScan = panScan / 65536.0f;

	if (vw->source->aspect)
	    vw->source->aspectRatio = (float) aspectX / aspectY;

	updateWindowVideoContext (w, vw->source);
    }
    else
    {
	if (texture)
	    videoReleaseTexture (w->screen, texture);

	if (vw->source)
	{
	    free (vw->source);
	    vw->source = NULL;
	}

	if (vw->context)
	{
	    free (vw->context);
	    vw->context = NULL;
	}
    }
}

static void
videoHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    CompWindow *w;

    VIDEO_DISPLAY (d);

    switch (event->type) {
    case PropertyNotify:
	if (event->xproperty.atom == vd->videoAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		videoWindowUpdate (w);
	}
	break;
    default:
	if (event->type == d->damageEvent + XDamageNotify)
	{
	    XDamageNotifyEvent *de = (XDamageNotifyEvent *) event;
	    VideoTexture       *t;

	    for (t = vd->textures; t; t = t->next)
	    {
		if (t->pixmap == de->drawable)
		{
		    VideoWindow *vw;
		    VideoScreen *vs;
		    CompScreen  *s;
		    BoxRec	box;
		    int		bw;

		    t->texture.oldMipmaps = TRUE;

		    for (s = d->screens; s; s = s->next)
		    {
			vs = GET_VIDEO_SCREEN (s, vd);

			for (w = s->windows; w; w = w->next)
			{
			    if (w->shaded || w->mapNum)
			    {
				vw = GET_VIDEO_WINDOW (w, vs);

				if (vw->context &&
				    vw->context->source->texture == t)
				{
				    box = vw->context->box.extents;

				    bw = w->attrib.border_width;

				    box.x1 -= w->attrib.x + bw;
				    box.y1 -= w->attrib.y + bw;
				    box.x2 -= w->attrib.x + bw;
				    box.y2 -= w->attrib.y + bw;

				    addWindowDamageRect (w, &box);
				}
			    }
			}
		    }
		    return;
		}
	    }
	}
	break;
    }

    UNWRAP (vd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (vd, d, handleEvent, videoHandleEvent);
}

static Bool
videoDamageWindowRect (CompWindow *w,
		       Bool	  initial,
		       BoxPtr     rect)
{
    Bool status;

    VIDEO_SCREEN (w->screen);

    if (initial)
	videoWindowUpdate (w);

    UNWRAP (vs, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (vs, w->screen, damageWindowRect, videoDamageWindowRect);

    return status;
}

static void
videoWindowMoveNotify (CompWindow *w,
		       int	  dx,
		       int	  dy,
		       Bool	  immediate)
{
    VIDEO_SCREEN (w->screen);
    VIDEO_WINDOW (w);

    if (vw->context)
    {
	vw->context->box.extents.x1 += dx;
	vw->context->box.extents.y1 += dy;
	vw->context->box.extents.x2 += dx;
	vw->context->box.extents.y2 += dy;

	updateWindowVideoMatrix (w);
    }

    UNWRAP (vs, w->screen, windowMoveNotify);
    (*w->screen->windowMoveNotify) (w, dx, dy, immediate);
    WRAP (vs, w->screen, windowMoveNotify, videoWindowMoveNotify);
}

static void
videoWindowResizeNotify (CompWindow *w,
			 int        dx,
			 int        dy,
			 int        dwidth,
			 int        dheight)
{
    VIDEO_SCREEN (w->screen);
    VIDEO_WINDOW (w);

    if (vw->source)
	updateWindowVideoContext (w, vw->source);

    UNWRAP (vs, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w, dx, dy, dwidth, dheight);
    WRAP (vs, w->screen, windowResizeNotify, videoWindowResizeNotify);
}

static const CompMetadataOptionInfo videoDisplayOptionInfo[] = {
    { "yv12", "bool", 0, 0, 0 }
};

static Bool
videoInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    VideoDisplay *vd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    vd = malloc (sizeof (VideoDisplay));
    if (!vd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &videoMetadata,
					     videoDisplayOptionInfo,
					     vd->opt,
					     VIDEO_DISPLAY_OPTION_NUM))
    {
	free (vd);
	return FALSE;
    }

    vd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (vd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, vd->opt, VIDEO_DISPLAY_OPTION_NUM);
	free (vd);
	return FALSE;
    }

    vd->textures = 0;

    vd->videoAtom	   =
	XInternAtom (d->display, "_COMPIZ_VIDEO", 0);
    vd->videoSupportedAtom =
	XInternAtom (d->display, "_COMPIZ_VIDEO_SUPPORTED", 0);

    vd->videoImageFormatAtom[IMAGE_FORMAT_RGB]  =
	XInternAtom (d->display, "_COMPIZ_VIDEO_IMAGE_FORMAT_RGB", 0);
    vd->videoImageFormatAtom[IMAGE_FORMAT_YV12] =
	XInternAtom (d->display, "_COMPIZ_VIDEO_IMAGE_FORMAT_YV12", 0);

    WRAP (vd, d, handleEvent, videoHandleEvent);

    d->base.privates[displayPrivateIndex].ptr = vd;

    return TRUE;
}

static void
videoFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    VIDEO_DISPLAY (d);

    freeScreenPrivateIndex (d, vd->screenPrivateIndex);

    UNWRAP (vd, d, handleEvent);

    compFiniDisplayOptions (d, vd->opt, VIDEO_DISPLAY_OPTION_NUM);

    free (vd);
}

static Bool
videoInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    VideoScreen *vs;

    VIDEO_DISPLAY (s->display);

    vs = malloc (sizeof (VideoScreen));
    if (!vs)
	return FALSE;

    vs->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (vs->windowPrivateIndex < 0)
    {
	free (vs);
	return FALSE;
    }

    vs->yv12Functions = NULL;

    memset (vs->imageFormat, 0, sizeof (vs->imageFormat));

    vs->imageFormat[IMAGE_FORMAT_RGB] = TRUE;
    if (s->fragmentProgram)
    {
	if (s->glxPixmapFBConfigs[8].fbConfig)
	{
	    vs->imageFormat[IMAGE_FORMAT_YV12] = TRUE;
	}
	else
	{
	    compLogMessage ("video", CompLogLevelWarn,
			    "No 8 bit GLX pixmap format, "
			    "disabling YV12 image format");
	}
    }

    WRAP (vs, s, drawWindow, videoDrawWindow);
    WRAP (vs, s, drawWindowTexture, videoDrawWindowTexture);
    WRAP (vs, s, damageWindowRect, videoDamageWindowRect);
    WRAP (vs, s, windowMoveNotify, videoWindowMoveNotify);
    WRAP (vs, s, windowResizeNotify, videoWindowResizeNotify);

    s->base.privates[vd->screenPrivateIndex].ptr = vs;

    videoSetSupportedHint (s);

    return TRUE;
}

static void
videoFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    VIDEO_DISPLAY (s->display);
    VIDEO_SCREEN (s);

    freeWindowPrivateIndex (s, vs->windowPrivateIndex);

    XDeleteProperty (s->display->display, s->root, vd->videoSupportedAtom);

    videoDestroyFragmentFunctions (s, &vs->yv12Functions);

    UNWRAP (vs, s, drawWindow);
    UNWRAP (vs, s, drawWindowTexture);
    UNWRAP (vs, s, damageWindowRect);
    UNWRAP (vs, s, windowMoveNotify);
    UNWRAP (vs, s, windowResizeNotify);

    free (vs);
}

static Bool
videoInitWindow (CompPlugin *p,
		 CompWindow *w)
{
    VideoWindow *vw;

    VIDEO_SCREEN (w->screen);

    vw = malloc (sizeof (VideoWindow));
    if (!vw)
	return FALSE;

    vw->source  = NULL;
    vw->context = NULL;

    w->base.privates[vs->windowPrivateIndex].ptr = vw;

    if (w->shaded || w->attrib.map_state == IsViewable)
	videoWindowUpdate (w);

    return TRUE;
}

static void
videoFiniWindow (CompPlugin *p,
		 CompWindow *w)
{
    VIDEO_WINDOW (w);

    if (vw->source)
    {
	videoReleaseTexture (w->screen, vw->source->texture);
	free (vw->source);
    }

    if (vw->context)
	free (vw->context);

    free (vw);
}

static CompBool
videoInitObject (CompPlugin *p,
		 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) videoInitDisplay,
	(InitPluginObjectProc) videoInitScreen,
	(InitPluginObjectProc) videoInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
videoFiniObject (CompPlugin *p,
		 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) videoFiniDisplay,
	(FiniPluginObjectProc) videoFiniScreen,
	(FiniPluginObjectProc) videoFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
videoGetObjectOptions (CompPlugin *plugin,
		       CompObject *object,
		       int	  *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) videoGetDisplayOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
videoSetObjectOption (CompPlugin      *plugin,
		      CompObject      *object,
		      const char      *name,
		      CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) videoSetDisplayOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
videoInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&videoMetadata,
					 p->vTable->name,
					 videoDisplayOptionInfo,
					 VIDEO_DISPLAY_OPTION_NUM,
					 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&videoMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&videoMetadata, p->vTable->name);

    return TRUE;
}

static void
videoFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&videoMetadata);
}

static CompMetadata *
videoGetMetadata (CompPlugin *plugin)
{
    return &videoMetadata;
}

static CompPluginVTable videoVTable = {
    "video",
    videoGetMetadata,
    videoInit,
    videoFini,
    videoInitObject,
    videoFiniObject,
    videoGetObjectOptions,
    videoSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &videoVTable;
}
