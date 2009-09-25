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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <compiz-core.h>
#include <decoration.h>

#include <X11/Xatom.h>
#include <GL/glu.h>

static CompMetadata blurMetadata;

#define BLUR_GAUSSIAN_RADIUS_MAX 15

#define BLUR_FILTER_4X_BILINEAR 0
#define BLUR_FILTER_GAUSSIAN    1
#define BLUR_FILTER_MIPMAP      2
#define BLUR_FILTER_LAST	BLUR_FILTER_MIPMAP

typedef struct _BlurFunction {
    struct _BlurFunction *next;

    int handle;
    int target;
    int param;
    int unit;
    int startTC;
    int numITC;
} BlurFunction;

typedef struct _BlurBox {
    decor_point_t p1;
    decor_point_t p2;
} BlurBox;

#define BLUR_STATE_CLIENT 0
#define BLUR_STATE_DECOR  1
#define BLUR_STATE_NUM    2

typedef struct _BlurState {
    int     threshold;
    BlurBox *box;
    int	    nBox;
    Bool    active;
    Bool    clipped;
} BlurState;

static int corePrivateIndex;

typedef struct _BlurCore {
    ObjectAddProc objectAdd;
} BlurCore;

static int displayPrivateIndex;

#define BLUR_DISPLAY_OPTION_PULSE 0
#define BLUR_DISPLAY_OPTION_NUM   1

typedef struct _BlurDisplay {
    int			       screenPrivateIndex;
    HandleEventProc	       handleEvent;
    MatchExpHandlerChangedProc matchExpHandlerChanged;
    MatchPropertyChangedProc   matchPropertyChanged;

    CompOption opt[BLUR_DISPLAY_OPTION_NUM];

    Atom blurAtom[BLUR_STATE_NUM];
} BlurDisplay;

#define BLUR_SCREEN_OPTION_BLUR_SPEED        0
#define BLUR_SCREEN_OPTION_FOCUS_BLUR_MATCH  1
#define BLUR_SCREEN_OPTION_FOCUS_BLUR        2
#define BLUR_SCREEN_OPTION_ALPHA_BLUR_MATCH  3
#define BLUR_SCREEN_OPTION_ALPHA_BLUR        4
#define BLUR_SCREEN_OPTION_FILTER            5
#define BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS   6
#define BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH 7
#define BLUR_SCREEN_OPTION_MIPMAP_LOD        8
#define BLUR_SCREEN_OPTION_SATURATION        9
#define BLUR_SCREEN_OPTION_BLUR_OCCLUSION    10
#define BLUR_SCREEN_OPTION_INDEPENDENT_TEX   11
#define BLUR_SCREEN_OPTION_NUM		     12

typedef struct _BlurScreen {
    int	windowPrivateIndex;

    CompOption opt[BLUR_SCREEN_OPTION_NUM];

    PreparePaintScreenProc       preparePaintScreen;
    DonePaintScreenProc          donePaintScreen;
    PaintOutputProc	         paintOutput;
    PaintTransformedOutputProc	 paintTransformedOutput;
    PaintWindowProc	         paintWindow;
    DrawWindowProc	         drawWindow;
    DrawWindowTextureProc        drawWindowTexture;

    WindowResizeNotifyProc windowResizeNotify;
    WindowMoveNotifyProc   windowMoveNotify;

    Bool alphaBlur;

    int	 blurTime;
    Bool moreBlur;

    Bool blurOcclusion;

    int filterRadius;

    BlurFunction *srcBlurFunctions;
    BlurFunction *dstBlurFunctions;

    Region region;
    Region tmpRegion;
    Region tmpRegion2;
    Region tmpRegion3;
    Region occlusion;

    BoxRec stencilBox;
    GLint  stencilBits;

    CompOutput *output;
    int count;

    GLuint texture[2];

    GLenum target;
    float  tx;
    float  ty;
    int    width;
    int    height;

    GLuint program;
    int    maxTemp;
    GLuint fbo;
    Bool   fboStatus;

    float amp[BLUR_GAUSSIAN_RADIUS_MAX];
    float pos[BLUR_GAUSSIAN_RADIUS_MAX];
    int	  numTexop;

    CompTransform mvp;
} BlurScreen;

typedef struct _BlurWindow {
    int  blur;
    Bool pulse;
    Bool focusBlur;

    BlurState state[BLUR_STATE_NUM];
    Bool      propSet[BLUR_STATE_NUM];

    Region region;
    Region clip;
} BlurWindow;

#define GET_BLUR_CORE(c)				    \
    ((BlurCore *) (c)->base.privates[corePrivateIndex].ptr)

#define BLUR_CORE(c)		     \
    BlurCore *bc = GET_BLUR_CORE (c)

#define GET_BLUR_DISPLAY(d)					  \
    ((BlurDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define BLUR_DISPLAY(d)			   \
    BlurDisplay *bd = GET_BLUR_DISPLAY (d)

#define GET_BLUR_SCREEN(s, bd)					      \
    ((BlurScreen *) (s)->base.privates[(bd)->screenPrivateIndex].ptr)

#define BLUR_SCREEN(s)							\
    BlurScreen *bs = GET_BLUR_SCREEN (s, GET_BLUR_DISPLAY (s->display))

#define GET_BLUR_WINDOW(w, bs)					      \
    ((BlurWindow *) (w)->base.privates[(bs)->windowPrivateIndex].ptr)

#define BLUR_WINDOW(w)					     \
    BlurWindow *bw = GET_BLUR_WINDOW  (w,		     \
		     GET_BLUR_SCREEN  (w->screen,	     \
		     GET_BLUR_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

/* pascal triangle based kernel generator */
static int
blurCreateGaussianLinearKernel (int   radius,
				float strength,
				float *amp,
				float *pos,
				int   *optSize)
{
    float factor = 0.5f + (strength / 2.0f);
    float buffer1[BLUR_GAUSSIAN_RADIUS_MAX * 3];
    float buffer2[BLUR_GAUSSIAN_RADIUS_MAX * 3];
    float *ar1 = buffer1;
    float *ar2 = buffer2;
    float *tmp;
    float sum = 0;
    int   size = (radius * 2) + 1;
    int   mySize = ceil (radius / 2.0f);
    int   i, j;

    ar1[0] = 1.0;
    ar1[1] = 1.0;

    for (i = 3; i <= size; i++)
    {
	ar2[0] = 1;

	for (j = 1; j < i - 1; j++)
	    ar2[j] = (ar1[j - 1] + ar1[j]) * factor;

	ar2[i - 1] = 1;

	tmp = ar1;
	ar1 = ar2;
	ar2 = tmp;
    }

    /* normalize */
    for (i = 0; i < size; i++)
	sum += ar1[i];

    if (sum != 0.0f)
	sum = 1.0f / sum;

    for (i = 0; i < size; i++)
	ar1[i] *= sum;

    i = 0;
    j = 0;

    if (radius & 1)
    {
	pos[i] = radius;
	amp[i] = ar1[i];
	i = 1;
	j = 1;
    }

    for (; i < mySize; i++)
    {
	pos[i]  = radius - j;
	pos[i] -= ar1[j + 1] / (ar1[j] + ar1[j + 1]);
	amp[i]  = ar1[j] + ar1[j + 1];

	j += 2;
    }

    pos[mySize] = 0.0;
    amp[mySize] = ar1[radius];

    *optSize = mySize;

    return radius;
}

static void
blurUpdateFilterRadius (CompScreen *s)
{
    BLUR_SCREEN (s);

    switch (bs->opt[BLUR_SCREEN_OPTION_FILTER].value.i) {
    case BLUR_FILTER_4X_BILINEAR:
	bs->filterRadius = 2;
	break;
    case BLUR_FILTER_GAUSSIAN: {
	int   radius   = bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS].value.i;
	float strength = bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH].value.f;

	blurCreateGaussianLinearKernel (radius, strength, bs->amp, bs->pos,
					&bs->numTexop);

	bs->filterRadius = radius;
    } break;
    case BLUR_FILTER_MIPMAP: {
	float lod = bs->opt[BLUR_SCREEN_OPTION_MIPMAP_LOD].value.f;

	bs->filterRadius = powf (2.0f, ceilf (lod));
    } break;
    }
}

static void
blurDestroyFragmentFunctions (CompScreen   *s,
			      BlurFunction **blurFunctions)
{
    BlurFunction *function, *next;

    function = *blurFunctions;
    while (function)
    {
	destroyFragmentFunction (s, function->handle);

	next = function->next;
	free (function);
	function = next;
    }

    *blurFunctions = NULL;
}

static void
blurReset (CompScreen *s)
{
    BLUR_SCREEN (s);

    blurUpdateFilterRadius (s);
    blurDestroyFragmentFunctions (s, &bs->srcBlurFunctions);
    blurDestroyFragmentFunctions (s, &bs->dstBlurFunctions);

    bs->width = bs->height = 0;

    if (bs->program)
    {
	(*s->deletePrograms) (1, &bs->program);
	bs->program = 0;
    }
}

static Region
regionFromBoxes (BlurBox *box,
		 int	 nBox,
		 int	 width,
		 int	 height)
{
    Region region;
    REGION r;
    int    x, y;

    region = XCreateRegion ();
    if (!region)
	return NULL;

    r.rects = &r.extents;
    r.numRects = r.size = 1;

    while (nBox--)
    {
	decor_apply_gravity (box->p1.gravity, box->p1.x, box->p1.y,
			     width, height,
			     &x, &y);

	r.extents.x1 = x;
	r.extents.y1 = y;

	decor_apply_gravity (box->p2.gravity, box->p2.x, box->p2.y,
			     width, height,
			     &x, &y);

	r.extents.x2 = x;
	r.extents.y2 = y;

	if (r.extents.x2 > r.extents.x1 && r.extents.y2 > r.extents.y1)
	    XUnionRegion (region, &r, region);

	box++;
    }

    return region;
}

static void
blurWindowUpdateRegion (CompWindow *w)
{
    Region region, q;
    REGION r;

    BLUR_WINDOW (w);

    region = XCreateRegion ();
    if (!region)
	return;

    r.rects = &r.extents;
    r.numRects = r.size = 1;

    if (bw->state[BLUR_STATE_DECOR].threshold)
    {
	r.extents.x1 = -w->output.left;
	r.extents.y1 = -w->output.top;
	r.extents.x2 = w->width + w->output.right;
	r.extents.y2 = w->height + w->output.bottom;

	XUnionRegion (&r, region, region);

	r.extents.x1 = 0;
	r.extents.y1 = 0;
	r.extents.x2 = w->width;
	r.extents.y2 = w->height;

	XSubtractRegion (region, &r, region);

	bw->state[BLUR_STATE_DECOR].clipped = FALSE;

	if (bw->state[BLUR_STATE_DECOR].nBox)
	{
	    q = regionFromBoxes (bw->state[BLUR_STATE_DECOR].box,
				 bw->state[BLUR_STATE_DECOR].nBox,
				 w->width, w->height);
	    if (q)
	    {
		XIntersectRegion (q, region, q);
		if (!XEqualRegion (q, region))
		{
		    XSubtractRegion (q, &emptyRegion, region);
		    bw->state[BLUR_STATE_DECOR].clipped = TRUE;
		}

		XDestroyRegion (q);
	    }
	}
    }

    if (bw->state[BLUR_STATE_CLIENT].threshold)
    {
	r.extents.x1 = 0;
	r.extents.y1 = 0;
	r.extents.x2 = w->width;
	r.extents.y2 = w->height;

	bw->state[BLUR_STATE_CLIENT].clipped = FALSE;

	if (bw->state[BLUR_STATE_CLIENT].nBox)
	{
	    q = regionFromBoxes (bw->state[BLUR_STATE_CLIENT].box,
				 bw->state[BLUR_STATE_CLIENT].nBox,
				 w->width, w->height);
	    if (q)
	    {
		XIntersectRegion (q, &r, q);
		if (!XEqualRegion (q, &r))
		    bw->state[BLUR_STATE_CLIENT].clipped = TRUE;

		XUnionRegion (q, region, region);
		XDestroyRegion (q);
	    }
	}
	else
	{
	    XUnionRegion (&r, region, region);
	}
    }

    if (bw->region)
	XDestroyRegion (bw->region);

    if (XEmptyRegion (region))
    {
	bw->region = NULL;
	XDestroyRegion (region);
    }
    else
    {
	bw->region = region;
	XOffsetRegion (bw->region, w->attrib.x, w->attrib.y);
    }
}

static void
blurSetWindowBlur (CompWindow *w,
		   int	      state,
		   int	      threshold,
		   BlurBox    *box,
		   int	      nBox)
{
    BLUR_WINDOW (w);

    if (bw->state[state].box)
	free (bw->state[state].box);

    bw->state[state].threshold = threshold;
    bw->state[state].box       = box;
    bw->state[state].nBox      = nBox;

    blurWindowUpdateRegion (w);

    addWindowDamage (w);
}

static void
blurUpdateAlphaWindowMatch (BlurScreen *bs,
			    CompWindow *w)
{
    BLUR_WINDOW (w);

    if (!bw->propSet[BLUR_STATE_CLIENT])
    {
	CompMatch *match;

	match = &bs->opt[BLUR_SCREEN_OPTION_ALPHA_BLUR_MATCH].value.match;
	if (matchEval (match, w))
	{
	    if (!bw->state[BLUR_STATE_CLIENT].threshold)
		blurSetWindowBlur (w, BLUR_STATE_CLIENT, 4, NULL, 0);
	}
	else
	{
	    if (bw->state[BLUR_STATE_CLIENT].threshold)
		blurSetWindowBlur (w, BLUR_STATE_CLIENT, 0, NULL, 0);
	}
    }
}

static void
blurUpdateWindowMatch (BlurScreen *bs,
		       CompWindow *w)
{
    CompMatch *match;
    Bool      focus;

    BLUR_WINDOW (w);

    blurUpdateAlphaWindowMatch (bs, w);

    match = &bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR_MATCH].value.match;

    focus = w->screen->fragmentProgram && matchEval (match, w);
    if (focus != bw->focusBlur)
    {
	bw->focusBlur = focus;
	addWindowDamage (w);
    }
}

static CompOption *
blurGetScreenOptions (CompPlugin *plugin,
		      CompScreen *screen,
		      int	 *count)
{
    BLUR_SCREEN (screen);

    *count = NUM_OPTIONS (bs);
    return bs->opt;
}

static Bool
blurSetScreenOption (CompPlugin      *plugin,
		     CompScreen      *screen,
		     const char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index, filter;

    BLUR_SCREEN (screen);

    o = compFindOption (bs->opt, NUM_OPTIONS (bs), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case BLUR_SCREEN_OPTION_BLUR_SPEED:
	if (compSetFloatOption (o, value))
	{
	    bs->blurTime = 1000.0f / o->value.f;
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_FOCUS_BLUR_MATCH:
    case BLUR_SCREEN_OPTION_ALPHA_BLUR_MATCH:
	if (compSetMatchOption (o, value))
	{
	    CompWindow *w;

	    for (w = screen->windows; w; w = w->next)
		blurUpdateWindowMatch (bs, w);

	    bs->moreBlur = TRUE;
	    damageScreen (screen);

	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_FOCUS_BLUR:
	if (compSetBoolOption (o, value))
	{
	    bs->moreBlur = TRUE;
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_ALPHA_BLUR:
	if (compSetBoolOption (o, value))
	{
	    if (screen->fragmentProgram && o->value.b)
		bs->alphaBlur = TRUE;
	    else
		bs->alphaBlur = FALSE;

	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_FILTER:
	if (compSetIntOption (o, value))
	{
	    blurReset (screen);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS:
	if (compSetIntOption (o, value))
	{
	    filter = bs->opt[BLUR_SCREEN_OPTION_FILTER].value.i;
	    if (filter == BLUR_FILTER_GAUSSIAN)
	    {
		blurReset (screen);
		damageScreen (screen);
	    }
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH:
	if (compSetFloatOption (o, value))
	{
	    filter = bs->opt[BLUR_SCREEN_OPTION_FILTER].value.i;
	    if (filter == BLUR_FILTER_GAUSSIAN)
	    {
		blurReset (screen);
		damageScreen (screen);
	    }
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_MIPMAP_LOD:
	if (compSetFloatOption (o, value))
	{
	    filter = bs->opt[BLUR_SCREEN_OPTION_FILTER].value.i;
	    if (filter == BLUR_FILTER_MIPMAP)
	    {
		blurReset (screen);
		damageScreen (screen);
	    }
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_SATURATION:
	if (compSetIntOption (o, value))
	{
	    blurReset (screen);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_BLUR_OCCLUSION:
	if (compSetBoolOption (o, value))
	{
	    bs->blurOcclusion = o->value.b;
	    blurReset (screen);
	    damageScreen (screen);
	    return TRUE;
	}
    case BLUR_SCREEN_OPTION_INDEPENDENT_TEX:
	if (compSetBoolOption (o, value))
	{
	    filter = bs->opt[BLUR_SCREEN_OPTION_FILTER].value.i;
	    if (filter == BLUR_FILTER_GAUSSIAN)
	    {
		blurReset (screen);
		damageScreen (screen);
	    }
	    return TRUE;
	}
    default:
	break;
    }

    return FALSE;
}

static void
blurWindowUpdate (CompWindow *w,
		  int	     state)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *propData;
    int		  threshold = 0;
    BlurBox	  *box = NULL;
    int		  nBox = 0;

    BLUR_DISPLAY (w->screen->display);
    BLUR_SCREEN (w->screen);
    BLUR_WINDOW (w);

    result = XGetWindowProperty (w->screen->display->display, w->id,
				 bd->blurAtom[state], 0L, 8192L, FALSE,
				 XA_INTEGER, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && propData)
    {
	bw->propSet[state] = TRUE;

	if (n >= 2)
	{
	    long *data = (long *) propData;

	    threshold = data[0];

	    nBox = (n - 2) / 6;
	    if (nBox)
	    {
		box = malloc (sizeof (BlurBox) * nBox);
		if (box)
		{
		    int i;

		    data += 2;

		    for (i = 0; i < nBox; i++)
		    {
			box[i].p1.gravity = *data++;
			box[i].p1.x       = *data++;
			box[i].p1.y       = *data++;
			box[i].p2.gravity = *data++;
			box[i].p2.x       = *data++;
			box[i].p2.y       = *data++;
		    }
		}
	    }
	}

	XFree (propData);
    }
    else
    {
	bw->propSet[state] = FALSE;
    }

    blurSetWindowBlur (w,
		       state,
		       threshold,
		       box,
		       nBox);

    blurUpdateAlphaWindowMatch (bs, w);
}

static void
blurPreparePaintScreen (CompScreen *s,
			int	   msSinceLastPaint)
{
    BLUR_SCREEN (s);

    if (bs->moreBlur)
    {
	CompWindow  *w;
	int	    steps;
	Bool        focus = bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR].value.b;
	Bool        focusBlur;

	steps = (msSinceLastPaint * 0xffff) / bs->blurTime;
	if (steps < 12)
	    steps = 12;

	bs->moreBlur = FALSE;

	for (w = s->windows; w; w = w->next)
	{
	    BLUR_WINDOW (w);

	    focusBlur = bw->focusBlur && focus;

	    if (!bw->pulse && (!focusBlur || w->id == s->display->activeWindow))
	    {
		if (bw->blur)
		{
		    bw->blur -= steps;
		    if (bw->blur > 0)
			bs->moreBlur = TRUE;
		    else
			bw->blur = 0;
		}
	    }
	    else
	    {
		if (bw->blur < 0xffff)
		{
		    if (bw->pulse)
		    {
			bw->blur += steps * 2;

			if (bw->blur >= 0xffff)
			{
			    bw->blur = 0xffff - 1;
			    bw->pulse = FALSE;
			}

			bs->moreBlur = TRUE;
		    }
		    else
		    {
			bw->blur += steps;
			if (bw->blur < 0xffff)
			    bs->moreBlur = TRUE;
			else
			    bw->blur = 0xffff;
		    }
		}
	    }
	}
    }

    UNWRAP (bs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (bs, s, preparePaintScreen, blurPreparePaintScreen);

    if (s->damageMask & COMP_SCREEN_DAMAGE_REGION_MASK)
    {
	/* walk from bottom to top and expand damage */
	if (bs->alphaBlur)
	{
	    CompWindow *w;
	    int	       x1, y1, x2, y2;
	    int	       count = 0;

	    for (w = s->windows; w; w = w->next)
	    {
		BLUR_WINDOW (w);

		if (w->attrib.map_state != IsViewable || !w->damaged)
		    continue;

		if (bw->region)
		{
		    x1 = bw->region->extents.x1 - bs->filterRadius;
		    y1 = bw->region->extents.y1 - bs->filterRadius;
		    x2 = bw->region->extents.x2 + bs->filterRadius;
		    y2 = bw->region->extents.y2 + bs->filterRadius;

		    if (x1 < s->damage->extents.x2 &&
			y1 < s->damage->extents.y2 &&
			x2 > s->damage->extents.x1 &&
			y2 > s->damage->extents.y1)
		    {
			XShrinkRegion (s->damage,
				       -bs->filterRadius,
				       -bs->filterRadius);

			count++;
		    }
		}
	    }

	    bs->count = count;
	}
    }
}

static Bool
blurPaintOutput (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 const CompTransform	 *transform,
		 Region			 region,
		 CompOutput		 *output,
		 unsigned int		 mask)
{
    Bool status;

    BLUR_SCREEN (s);

    if (bs->alphaBlur)
    {
	bs->stencilBox = region->extents;
	XSubtractRegion (region, &emptyRegion, bs->region);

	if (mask & PAINT_SCREEN_REGION_MASK)
	{
	    /* we need to redraw more than the screen region being updated */
	    if (bs->count)
	    {
		XShrinkRegion (bs->region,
			       -bs->filterRadius * 2,
			       -bs->filterRadius * 2);
		XIntersectRegion (bs->region, &s->region, bs->region);

		region = bs->region;
	    }
	}
    }

    if (!bs->blurOcclusion)
    {
	CompWindow *w;

	XSubtractRegion (&emptyRegion, &emptyRegion, bs->occlusion);

	for (w = s->windows; w; w = w->next)
	    XSubtractRegion (&emptyRegion, &emptyRegion,
			     GET_BLUR_WINDOW (w, bs)->clip);
    }

    bs->output = output;

    UNWRAP (bs, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (bs, s, paintOutput, blurPaintOutput);

    return status;
}

static void
blurPaintTransformedOutput (CompScreen		    *s,
			    const ScreenPaintAttrib *sAttrib,
			    const CompTransform	    *transform,
			    Region		    region,
			    CompOutput		    *output,
			    unsigned int	    mask)
{
    BLUR_SCREEN (s);

    if (!bs->blurOcclusion)
    {
	CompWindow *w;

	XSubtractRegion (&emptyRegion, &emptyRegion, bs->occlusion);

	for (w = s->windows; w; w = w->next)
	    XSubtractRegion (&emptyRegion, &emptyRegion,
			     GET_BLUR_WINDOW (w, bs)->clip);
    }

    UNWRAP (bs, s, paintTransformedOutput);
    (*s->paintTransformedOutput) (s, sAttrib, transform,
				   region, output, mask);
    WRAP (bs, s, paintTransformedOutput, blurPaintTransformedOutput);
}

static void
blurDonePaintScreen (CompScreen *s)
{
    BLUR_SCREEN (s);

    if (bs->moreBlur)
    {
	CompWindow *w;

	for (w = s->windows; w; w = w->next)
	{
	    BLUR_WINDOW (w);

	    if (bw->blur > 0 && bw->blur < 0xffff)
		addWindowDamage (w);
	}
    }

    UNWRAP (bs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (bs, s, donePaintScreen, blurDonePaintScreen);
}

static Bool
blurPaintWindow (CompWindow		 *w,
		 const WindowPaintAttrib *attrib,
		 const CompTransform	 *transform,
		 Region			 region,
		 unsigned int		 mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    BLUR_SCREEN (s);
    BLUR_WINDOW (w);

    UNWRAP (bs, s, paintWindow);
    status = (*s->paintWindow) (w, attrib, transform, region, mask);
    WRAP (bs, s, paintWindow, blurPaintWindow);

    if (!bs->blurOcclusion && (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK))
    {
	XSubtractRegion (bs->occlusion, &emptyRegion, bw->clip);

	if (!(w->lastMask & PAINT_WINDOW_NO_CORE_INSTANCE_MASK) &&
	    !(w->lastMask & PAINT_WINDOW_TRANSFORMED_MASK) && bw->region)
	    XUnionRegion (bs->occlusion, bw->region, bs->occlusion);
    }

    return status;
}

static int
getSrcBlurFragmentFunction (CompScreen  *s,
			    CompTexture *texture,
			    int	        param)
{
    BlurFunction     *function;
    CompFunctionData *data;
    int		     target;

    BLUR_SCREEN (s);

    if (texture->target == GL_TEXTURE_2D)
	target = COMP_FETCH_TARGET_2D;
    else
	target = COMP_FETCH_TARGET_RECT;

    for (function = bs->srcBlurFunctions; function; function = function->next)
	if (function->param == param && function->target == target)
	    return function->handle;

    data = createFunctionData ();
    if (data)
    {
	static char *temp[] = { "offset0", "offset1", "sum" };
	int	    i, handle = 0;
	char	    str[1024];
	Bool	    ok = TRUE;

	for (i = 0; i < sizeof (temp) / sizeof (temp[0]); i++)
	    ok &= addTempHeaderOpToFunctionData (data, temp[i]);

	snprintf (str, 1024,
		  "MUL offset0, program.env[%d].xyzw, { 1.0, 1.0, 0.0, 0.0 };"
		  "MUL offset1, program.env[%d].zwww, { 1.0, 1.0, 0.0, 0.0 };",
		  param, param);

	ok &= addDataOpToFunctionData (data, str);

	switch (bs->opt[BLUR_SCREEN_OPTION_FILTER].value.i) {
	case BLUR_FILTER_4X_BILINEAR:
	default:
	    ok &= addFetchOpToFunctionData (data, "output", "offset0", target);
	    ok &= addDataOpToFunctionData (data, "MUL sum, output, 0.25;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset0", target);
	    ok &= addDataOpToFunctionData (data, "MAD sum, output, 0.25, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "offset1", target);
	    ok &= addDataOpToFunctionData (data, "MAD sum, output, 0.25, sum;");
	    ok &= addFetchOpToFunctionData (data, "output", "-offset1", target);
	    ok &= addDataOpToFunctionData (data,
					   "MAD output, output, 0.25, sum;");
	    break;
	}

	if (!ok)
	{
	    destroyFunctionData (data);
	    return 0;
	}

	function = malloc (sizeof (BlurFunction));
	if (function)
	{
	    handle = createFragmentFunction (s, "blur", data);

	    function->handle = handle;
	    function->target = target;
	    function->param  = param;
	    function->unit   = 0;

	    function->next = bs->srcBlurFunctions;
	    bs->srcBlurFunctions = function;
	}

	destroyFunctionData (data);

	return handle;
    }

    return 0;
}

static int
getDstBlurFragmentFunction (CompScreen  *s,
			    CompTexture *texture,
			    int	        param,
			    int		unit,
			    int         numITC,
			    int         startTC)
{
    BlurFunction     *function;
    CompFunctionData *data;
    int		     target;
    char	     *targetString;

    BLUR_SCREEN (s);

    if (texture->target == GL_TEXTURE_2D)
    {
	target	     = COMP_FETCH_TARGET_2D;
	targetString = "2D";
    }
    else
    {
	target	     = COMP_FETCH_TARGET_RECT;
	targetString = "RECT";
    }

    for (function = bs->dstBlurFunctions; function; function = function->next)
	if (function->param   == param  &&
	    function->target  == target &&
	    function->unit    == unit   &&
	    function->numITC  == numITC &&
	    function->startTC == startTC)
	    return function->handle;

    data = createFunctionData ();
    if (data)
    {
	static char *temp[] = { "fCoord", "mask", "sum", "dst" };
	int	    i, j, handle = 0;
	char	    str[1024];
	int	    saturation = bs->opt[BLUR_SCREEN_OPTION_SATURATION].value.i;
	Bool	    ok = TRUE;
	int	    numIndirect;
	int	    numIndirectOp;
	int	    base, end, ITCbase;

	for (i = 0; i < sizeof (temp) / sizeof (temp[0]); i++)
	    ok &= addTempHeaderOpToFunctionData (data, temp[i]);

	if (saturation < 100)
	    ok &= addTempHeaderOpToFunctionData (data, "sat");

	switch (bs->opt[BLUR_SCREEN_OPTION_FILTER].value.i) {
	case BLUR_FILTER_4X_BILINEAR: {
	    static char *filterTemp[] = {
		"t0", "t1", "t2", "t3",
		"s0", "s1", "s2", "s3"
	    };

	    for (i = 0; i < sizeof (filterTemp) / sizeof (filterTemp[0]); i++)
		ok &= addTempHeaderOpToFunctionData (data, filterTemp[i]);

	    ok &= addFetchOpToFunctionData (data, "output", NULL, target);
	    ok &= addColorOpToFunctionData (data, "output", "output");

	    snprintf (str, 1024,
		      "MUL fCoord, fragment.position, program.env[%d];",
		      param);

	    ok &= addDataOpToFunctionData (data, str);

	    snprintf (str, 1024,
		      "ADD t0, fCoord, program.env[%d];"
		      "TEX s0, t0, texture[%d], %s;"

		      "SUB t1, fCoord, program.env[%d];"
		      "TEX s1, t1, texture[%d], %s;"

		      "MAD t2, program.env[%d], { -1.0, 1.0, 0.0, 0.0 }, fCoord;"
		      "TEX s2, t2, texture[%d], %s;"

		      "MAD t3, program.env[%d], { 1.0, -1.0, 0.0, 0.0 }, fCoord;"
		      "TEX s3, t3, texture[%d], %s;"

		      "MUL_SAT mask, output.a, program.env[%d];"

		      "MUL sum, s0, 0.25;"
		      "MAD sum, s1, 0.25, sum;"
		      "MAD sum, s2, 0.25, sum;"
		      "MAD sum, s3, 0.25, sum;",

		      param + 2, unit, targetString,
		      param + 2, unit, targetString,
		      param + 2, unit, targetString,
		      param + 2, unit, targetString,
		      param + 1);

	    ok &= addDataOpToFunctionData (data, str);
	} break;
	case BLUR_FILTER_GAUSSIAN: {

	    /* try to use only half of the available temporaries to keep
	       other plugins working */
	    if ((bs->maxTemp / 2) - 4 >
		 (bs->numTexop + (bs->numTexop - numITC)) * 2)
	    {
		numIndirect   = 1;
		numIndirectOp = bs->numTexop;
	    }
	    else
	    {
		i = MAX(((bs->maxTemp / 2) - 4) / 4, 1);
		numIndirect = ceil ((float)bs->numTexop / (float)i);
		numIndirectOp = ceil ((float)bs->numTexop / (float)numIndirect);
	    }

	    /* we need to define all coordinate temporaries if we have
	       multiple indirection steps */
	    j = (numIndirect > 1) ? 0 : numITC;

	    for (i = 0; i < numIndirectOp * 2; i++)
	    {
		snprintf (str, 1024, "pix_%d", i);
		ok &= addTempHeaderOpToFunctionData (data, str);
	    }

	    for (i = j * 2; i < numIndirectOp * 2; i++)
	    {
		snprintf (str, 1024, "coord_%d", i);
		ok &= addTempHeaderOpToFunctionData (data, str);
	    }

	    ok &= addFetchOpToFunctionData (data, "output", NULL, target);
	    ok &= addColorOpToFunctionData (data, "output", "output");

	    snprintf (str, 1024,
		      "MUL fCoord, fragment.position, program.env[%d];",
		      param);

	    ok &= addDataOpToFunctionData (data, str);

	    snprintf (str, 1024,
		      "TEX sum, fCoord, texture[%d], %s;",
		       unit + 1, targetString);

	    ok &= addDataOpToFunctionData (data, str);

	    snprintf (str, 1024,
		      "MUL_SAT mask, output.a, program.env[%d];"
		      "MUL sum, sum, %f;",
		      param + 1, bs->amp[bs->numTexop]);

	    ok &= addDataOpToFunctionData (data, str);

	    for (j = 0; j < numIndirect; j++)
	    {
		base = j * numIndirectOp;
		end  = MIN ((j + 1) * numIndirectOp, bs->numTexop) - base;
		
		ITCbase = MAX (numITC - base, 0);

		for (i = ITCbase; i < end; i++)
		{
		    snprintf (str, 1024,
			      "ADD coord_%d, fCoord, {0.0, %g, 0.0, 0.0};"
			      "SUB coord_%d, fCoord, {0.0, %g, 0.0, 0.0};",
			      i * 2, bs->pos[base + i] * bs->ty,
			      (i * 2) + 1, bs->pos[base + i] * bs->ty);

		    ok &= addDataOpToFunctionData (data, str);
		}

		for (i = 0; i < ITCbase; i++)
		{
		    snprintf (str, 1024,
			"TXP pix_%d, fragment.texcoord[%d], texture[%d], %s;"
			"TXP pix_%d, fragment.texcoord[%d], texture[%d], %s;",
			i * 2, startTC + ((i + base) * 2),
			unit + 1, targetString,
			(i * 2) + 1, startTC + 1 + ((i + base) * 2),
			unit + 1, targetString);

		    ok &= addDataOpToFunctionData (data, str);
		}

		for (i = ITCbase; i < end; i++)
		{
		    snprintf (str, 1024,
			      "TEX pix_%d, coord_%d, texture[%d], %s;"
			      "TEX pix_%d, coord_%d, texture[%d], %s;",
			      i * 2, i * 2,
			      unit + 1, targetString,
			      (i * 2) + 1, (i * 2) + 1,
			      unit + 1, targetString);

		    ok &= addDataOpToFunctionData (data, str);
		}

		for (i = 0; i < end * 2; i++)
		{
		    snprintf (str, 1024,
			      "MAD sum, pix_%d, %f, sum;",
			      i, bs->amp[base + (i / 2)]);

		    ok &= addDataOpToFunctionData (data, str);
		}
	    }

	} break;
	case BLUR_FILTER_MIPMAP:
	    ok &= addFetchOpToFunctionData (data, "output", NULL, target);
	    ok &= addColorOpToFunctionData (data, "output", "output");

	    snprintf (str, 1024,
		      "MUL fCoord, fragment.position, program.env[%d].xyzz;"
		      "MOV fCoord.w, program.env[%d].w;"
		      "TXB sum, fCoord, texture[%d], %s;"
		      "MUL_SAT mask, output.a, program.env[%d];",
		      param, param, unit, targetString,
		      param + 1);

	    ok &= addDataOpToFunctionData (data, str);
	    break;
	}

	if (saturation < 100)
	{
	    snprintf (str, 1024,
		      "MUL sat, sum, { 1.0, 1.0, 1.0, 0.0 };"
		      "DP3 sat, sat, { %f, %f, %f, %f };"
		      "LRP sum.xyz, %f, sum, sat;",
		      RED_SATURATION_WEIGHT, GREEN_SATURATION_WEIGHT,
		      BLUE_SATURATION_WEIGHT, 0.0f, saturation / 100.0f);

	    ok &= addDataOpToFunctionData (data, str);
	}

	snprintf (str, 1024,
		  "MAD dst, mask, -output.a, mask;"
		  "MAD output.rgb, sum, dst.a, output;"
		  "ADD output.a, output.a, dst.a;");

	ok &= addDataOpToFunctionData (data, str);

	if (!ok)
	{
	    destroyFunctionData (data);
	    return 0;
	}

	function = malloc (sizeof (BlurFunction));
	if (function)
	{
	    handle = createFragmentFunction (s, "blur", data);

	    function->handle  = handle;
	    function->target  = target;
	    function->param   = param;
	    function->unit    = unit;
	    function->numITC  = numITC;
	    function->startTC = startTC;

	    function->next = bs->dstBlurFunctions;
	    bs->dstBlurFunctions = function;
	}

	destroyFunctionData (data);

	return handle;
    }

    return 0;
}

static Bool
projectVertices (CompScreen	     *s,
		 CompOutput	     *output,
		 const CompTransform *transform,
		 const float	     *object,
		 float		     *screen,
		 int		     n)
{
    GLdouble dProjection[16];
    GLdouble dModel[16];
    GLint    viewport[4];
    double   x, y, z;
    int	     i;

    viewport[0] = output->region.extents.x1;
    viewport[1] = s->height - output->region.extents.y2;
    viewport[2] = output->width;
    viewport[3] = output->height;

    for (i = 0; i < 16; i++)
    {
	dModel[i]      = transform->m[i];
	dProjection[i] = s->projection[i];
    }

    while (n--)
    {
	if (!gluProject (object[0], object[1], object[2],
			 dModel, dProjection, viewport,
			 &x, &y, &z))
	    return FALSE;

	screen[0] = x;
	screen[1] = y;

	object += 3;
	screen += 2;
    }

    return TRUE;
}

static Bool
loadFragmentProgram (CompScreen *s,
		     GLuint	*program,
		     const char *string)
{
    GLint errorPos;

    /* clear errors */
    glGetError ();

    if (!*program)
	(*s->genPrograms) (1, program);

    (*s->bindProgram) (GL_FRAGMENT_PROGRAM_ARB, *program);
    (*s->programString) (GL_FRAGMENT_PROGRAM_ARB,
			 GL_PROGRAM_FORMAT_ASCII_ARB,
			 strlen (string), string);

    glGetIntegerv (GL_PROGRAM_ERROR_POSITION_ARB, &errorPos);
    if (glGetError () != GL_NO_ERROR || errorPos != -1)
    {
	compLogMessage ("blur", CompLogLevelError,
			"Failed to load blur program %s", string);

	(*s->deletePrograms) (1, program);
	*program = 0;

	return FALSE;
    }

    return TRUE;
}

static Bool
loadFilterProgram (CompScreen *s, int numITC)
{
    char  buffer[4096];
    char  *targetString;
    char  *str = buffer;
    int   i, j;
    int   numIndirect;
    int   numIndirectOp;
    int   base, end, ITCbase;

    BLUR_SCREEN (s);

    if (bs->target == GL_TEXTURE_2D)
	targetString = "2D";
    else
	targetString = "RECT";

    str += sprintf (str,
		    "!!ARBfp1.0"
		    "ATTRIB texcoord = fragment.texcoord[0];"
		    "TEMP sum;");

    if (bs->maxTemp - 1 > (bs->numTexop + (bs->numTexop - numITC)) * 2)
    {
	numIndirect   = 1;
	numIndirectOp = bs->numTexop;
    }
    else
    {
	i = (bs->maxTemp - 1) / 4;
	numIndirect = ceil ((float)bs->numTexop / (float)i);
	numIndirectOp = ceil ((float)bs->numTexop / (float)numIndirect);
    }

    /* we need to define all coordinate temporaries if we have
       multiple indirection steps */
    j = (numIndirect > 1) ? 0 : numITC;

    for (i = 0; i < numIndirectOp; i++)
	str += sprintf (str,"TEMP pix_%d, pix_%d;", i * 2, (i * 2) + 1);

    for (i = j; i < numIndirectOp; i++)
	str += sprintf (str,"TEMP coord_%d, coord_%d;", i * 2, (i * 2) + 1);

    str += sprintf (str,
		    "TEX sum, texcoord, texture[0], %s;",
		    targetString);

    str += sprintf (str,
		    "MUL sum, sum, %f;",
		    bs->amp[bs->numTexop]);

    for (j = 0; j < numIndirect; j++)
    {
	base = j * numIndirectOp;
	end  = MIN ((j + 1) * numIndirectOp, bs->numTexop) - base;
	
	ITCbase = MAX (numITC - base, 0);

	for (i = ITCbase; i < end; i++)
	    str += sprintf (str,
			    "ADD coord_%d, texcoord, {%g, 0.0, 0.0, 0.0};"
			    "SUB coord_%d, texcoord, {%g, 0.0, 0.0, 0.0};",
			    i * 2, bs->pos[base + i] * bs->tx,
			    (i * 2) + 1, bs->pos[base + i] * bs->tx);

	for (i = 0; i < ITCbase; i++)
	    str += sprintf (str,
		"TEX pix_%d, fragment.texcoord[%d], texture[0], %s;"
		"TEX pix_%d, fragment.texcoord[%d], texture[0], %s;",
		i * 2, ((i + base) * 2) + 1, targetString,
		(i * 2) + 1, ((i + base) * 2) + 2, targetString);

	for (i = ITCbase; i < end; i++)
	    str += sprintf (str,
			    "TEX pix_%d, coord_%d, texture[0], %s;"
			    "TEX pix_%d, coord_%d, texture[0], %s;",
			    i * 2, i * 2, targetString,
			    (i * 2) + 1, (i * 2) + 1, targetString);

	for (i = 0; i < end * 2; i++)
	    str += sprintf (str,
			    "MAD sum, pix_%d, %f, sum;",
			    i, bs->amp[base + (i / 2)]);
    }

    str += sprintf (str,
		    "MOV result.color, sum;"
		    "END");

    return loadFragmentProgram (s, &bs->program, buffer);
}

static int
fboPrologue (CompScreen *s)
{
    BLUR_SCREEN (s);

    if (!bs->fbo)
	return FALSE;

    (*s->bindFramebuffer) (GL_FRAMEBUFFER_EXT, bs->fbo);

    /* bind texture and check status the first time */
    if (!bs->fboStatus)
    {
	(*s->framebufferTexture2D) (GL_FRAMEBUFFER_EXT,
				    GL_COLOR_ATTACHMENT0_EXT,
				    bs->target, bs->texture[1],
				    0);

	bs->fboStatus = (*s->checkFramebufferStatus) (GL_FRAMEBUFFER_EXT);
	if (bs->fboStatus != GL_FRAMEBUFFER_COMPLETE_EXT)
	{
	    compLogMessage ("blur", CompLogLevelError,
			    "Framebuffer incomplete");

	    (*s->bindFramebuffer) (GL_FRAMEBUFFER_EXT, 0);
	    (*s->deleteFramebuffers) (1, &bs->fbo);

	    bs->fbo = 0;

	    return 0;
	}
    }

    glPushAttrib (GL_VIEWPORT_BIT | GL_ENABLE_BIT);

    glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
    glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);

    glDisable (GL_CLIP_PLANE0);
    glDisable (GL_CLIP_PLANE1);
    glDisable (GL_CLIP_PLANE2);
    glDisable (GL_CLIP_PLANE3);

    glViewport (0, 0, bs->width, bs->height);
    glMatrixMode (GL_PROJECTION);
    glPushMatrix ();
    glLoadIdentity ();
    glOrtho (0.0, bs->width, 0.0, bs->height, -1.0, 1.0);
    glMatrixMode (GL_MODELVIEW);
    glPushMatrix ();
    glLoadIdentity ();

    return TRUE;
}

static void
fboEpilogue (CompScreen *s)
{
    (*s->bindFramebuffer) (GL_FRAMEBUFFER_EXT, 0);

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
    glDepthRange (0, 1);
    glViewport (-1, -1, 2, 2);
    glRasterPos2f (0, 0);

    s->rasterX = s->rasterY = 0;

    glMatrixMode (GL_PROJECTION);
    glPopMatrix ();
    glMatrixMode (GL_MODELVIEW);
    glPopMatrix ();

    glDrawBuffer (GL_BACK);
    glReadBuffer (GL_BACK);

    glPopAttrib ();
}

static Bool
fboUpdate (CompScreen *s,
	   BoxPtr     pBox,
	   int	      nBox)
{
    int  i, y, iTC = 0;
    Bool wasCulled = glIsEnabled (GL_CULL_FACE);

    BLUR_SCREEN (s);

    if (s->maxTextureUnits &&
	bs->opt[BLUR_SCREEN_OPTION_INDEPENDENT_TEX].value.b)
	iTC = MIN ((s->maxTextureUnits - 1) / 2, bs->numTexop);

    if (!bs->program)
	if (!loadFilterProgram (s, iTC))
	    return FALSE;

    if (!fboPrologue (s))
	return FALSE;

    glDisable (GL_CULL_FACE);

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);

    glBindTexture (bs->target, bs->texture[0]);

    glEnable (GL_FRAGMENT_PROGRAM_ARB);
    (*s->bindProgram) (GL_FRAGMENT_PROGRAM_ARB, bs->program);

    glBegin (GL_QUADS);

    while (nBox--)
    {
	y = s->height - pBox->y2;

	for (i = 0; i < iTC; i++)
	{
	    (*s->multiTexCoord2f) (GL_TEXTURE1_ARB + (i * 2),
				   bs->tx * (pBox->x1 + bs->pos[i]),
				   bs->ty * y);
	    (*s->multiTexCoord2f) (GL_TEXTURE1_ARB + (i * 2) + 1,
				   bs->tx * (pBox->x1 - bs->pos[i]),
				   bs->ty * y);
	}

	glTexCoord2f (bs->tx * pBox->x1, bs->ty * y);
	glVertex2i   (pBox->x1, y);

	for (i = 0; i < iTC; i++)
	{
	    (*s->multiTexCoord2f) (GL_TEXTURE1_ARB + (i * 2),
				   bs->tx * (pBox->x2 + bs->pos[i]),
				   bs->ty * y);
	    (*s->multiTexCoord2f) (GL_TEXTURE1_ARB + (i * 2) + 1,
				   bs->tx * (pBox->x2 - bs->pos[i]),
				   bs->ty * y);
	}

	glTexCoord2f (bs->tx * pBox->x2, bs->ty * y);
	glVertex2i   (pBox->x2, y);

	y = s->height - pBox->y1;

	for (i = 0; i < iTC; i++)
	{
	    (*s->multiTexCoord2f) (GL_TEXTURE1_ARB + (i * 2),
				   bs->tx * (pBox->x2 + bs->pos[i]),
				   bs->ty * y);
	    (*s->multiTexCoord2f) (GL_TEXTURE1_ARB + (i * 2) + 1,
				   bs->tx * (pBox->x2 - bs->pos[i]),
				   bs->ty * y);
	}

	glTexCoord2f (bs->tx * pBox->x2, bs->ty * y);
	glVertex2i   (pBox->x2, y);

	for (i = 0; i < iTC; i++)
	{
	    (*s->multiTexCoord2f) (GL_TEXTURE1_ARB + (i * 2),
				   bs->tx * (pBox->x1 + bs->pos[i]),
				   bs->ty * y);
	    (*s->multiTexCoord2f) (GL_TEXTURE1_ARB + (i * 2) + 1,
				   bs->tx * (pBox->x1 - bs->pos[i]),
				   bs->ty * y);
	}

	glTexCoord2f (bs->tx * pBox->x1, bs->ty * y);
	glVertex2i   (pBox->x1, y);

	pBox++;
    }

    glEnd ();

    glDisable (GL_FRAGMENT_PROGRAM_ARB);

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

    if (wasCulled)
	glEnable (GL_CULL_FACE);

    fboEpilogue (s);

    return TRUE;
}

#define MAX_VERTEX_PROJECT_COUNT 20

static void
blurProjectRegion (CompWindow	       *w,
		   CompOutput	       *output,
		   const CompTransform *transform)
{
    CompScreen *s = w->screen;
    float      screen[MAX_VERTEX_PROJECT_COUNT * 2];
    float      vertices[MAX_VERTEX_PROJECT_COUNT * 3];
    int	       nVertices, nQuadCombine;
    int        i, j, stride;
    float      *v, *vert;
    float      minX, maxX, minY, maxY, minZ, maxZ;
    float      *scr;
    REGION     region;

    BLUR_SCREEN(s);

    w->vCount = w->indexCount = 0;
    (*w->screen->addWindowGeometry) (w, NULL, 0, bs->tmpRegion2,
				     &infiniteRegion);

    if (!w->vCount)
	return;

    nVertices    = (w->indexCount) ? w->indexCount: w->vCount;
    nQuadCombine = 1;

    stride = w->vertexStride;
    vert = w->vertices + (stride - 3);

    /* we need to find the best value here */
    if (nVertices <= MAX_VERTEX_PROJECT_COUNT)
    {
	for (i = 0; i < nVertices; i++)
	{
	    if (w->indexCount)
	    {
		v = vert + (stride * w->indices[i]);
	    }
	    else
	    {
		v = vert + (stride * i);
	    }

	    vertices[i * 3] = v[0];
	    vertices[(i * 3) + 1] = v[1];
	    vertices[(i * 3) + 2] = v[2];
	}
    }
    else
    {
	minX = s->width;
	maxX = 0;
	minY = s->height;
	maxY = 0;
	minZ = 1000000;
	maxZ = -1000000;

	for (i = 0; i < w->vCount; i++)
	{
	    v = vert + (stride * i);

	    if (v[0] < minX)
		minX = v[0];

	    if (v[0] > maxX)
		maxX = v[0];

	    if (v[1] < minY)
		minY = v[1];

	    if (v[1] > maxY)
		maxY = v[1];

	    if (v[2] < minZ)
		minZ = v[2];

	    if (v[2] > maxZ)
		maxZ = v[2];
	}

	vertices[0] = vertices[9]  = minX;
	vertices[1] = vertices[4]  = minY;
	vertices[3] = vertices[6]  = maxX;
	vertices[7] = vertices[10] = maxY;
	vertices[2] = vertices[5]  = maxZ;
	vertices[8] = vertices[11] = maxZ;

	nVertices = 4;

	if (maxZ != minZ)
	{
	    vertices[12] = vertices[21] = minX;
	    vertices[13] = vertices[16] = minY;
	    vertices[15] = vertices[18] = maxX;
	    vertices[19] = vertices[22] = maxY;
	    vertices[14] = vertices[17] = minZ;
	    vertices[20] = vertices[23] = minZ;
	    nQuadCombine = 2;
	}
    }


    if (!projectVertices (w->screen, output, transform, vertices, screen,
			  nVertices * nQuadCombine))
	return;

    region.rects    = &region.extents;
    region.numRects = 1;

    for (i = 0; i < nVertices / 4; i++)
    {
	scr = screen + (i * 4 * 2);

	minX = s->width;
	maxX = 0;
	minY = s->height;
	maxY = 0;

	for (j = 0; j < 8 * nQuadCombine; j += 2)
	{
	    if (scr[j] < minX)
		minX = scr[j];

	    if (scr[j] > maxX)
		maxX = scr[j];

	    if (scr[j + 1] < minY)
		minY = scr[j + 1];

	    if (scr[j + 1] > maxY)
		maxY = scr[j + 1];
	}

	region.extents.x1 = minX - bs->filterRadius;
	region.extents.y1 = (s->height - maxY - bs->filterRadius);
	region.extents.x2 = maxX + bs->filterRadius + 0.5f;
	region.extents.y2 = (s->height - minY + bs->filterRadius + 0.5f);

	XUnionRegion (&region, bs->tmpRegion3, bs->tmpRegion3);
    }
}

static Bool
blurUpdateDstTexture (CompWindow	  *w,
		      const CompTransform *transform,
		      BoxPtr		  pExtents,
		      int                 clientThreshold)
{
    CompScreen *s = w->screen;
    BoxPtr     pBox;
    int	       nBox;
    int        y;
    int        filter;

    BLUR_SCREEN (s);
    BLUR_WINDOW (w);

    filter = bs->opt[BLUR_SCREEN_OPTION_FILTER].value.i;

    /* create empty region */
    XSubtractRegion (&emptyRegion, &emptyRegion, bs->tmpRegion3);

    if (filter == BLUR_FILTER_GAUSSIAN)
    {
	REGION region;

	region.rects    = &region.extents;
	region.numRects = 1;

	if (bw->state[BLUR_STATE_DECOR].threshold)
	{
	    /* top */
	    region.extents.x1 = w->attrib.x - w->output.left;
	    region.extents.y1 = w->attrib.y - w->output.top;
	    region.extents.x2 = w->attrib.x + w->width + w->output.right;
	    region.extents.y2 = w->attrib.y;

	    XIntersectRegion (bs->tmpRegion, &region, bs->tmpRegion2);
	    if (bs->tmpRegion2->numRects)
		blurProjectRegion (w, bs->output, transform);

	    /* bottom */
	    region.extents.x1 = w->attrib.x - w->output.left;
	    region.extents.y1 = w->attrib.y + w->height;
	    region.extents.x2 = w->attrib.x + w->width + w->output.right;
	    region.extents.y2 = w->attrib.y + w->height + w->output.bottom;

	    XIntersectRegion (bs->tmpRegion, &region, bs->tmpRegion2);
	    if (bs->tmpRegion2->numRects)
		blurProjectRegion (w, bs->output, transform);

	    /* left */
	    region.extents.x1 = w->attrib.x - w->output.left;
	    region.extents.y1 = w->attrib.y;
	    region.extents.x2 = w->attrib.x;
	    region.extents.y2 = w->attrib.y + w->height;

	    XIntersectRegion (bs->tmpRegion, &region, bs->tmpRegion2);
	    if (bs->tmpRegion2->numRects)
		blurProjectRegion (w, bs->output, transform);

	    /* right */
	    region.extents.x1 = w->attrib.x + w->width;
	    region.extents.y1 = w->attrib.y;
	    region.extents.x2 = w->attrib.x + w->width + w->output.right;
	    region.extents.y2 = w->attrib.y + w->height;

	    XIntersectRegion (bs->tmpRegion, &region, bs->tmpRegion2);
	    if (bs->tmpRegion2->numRects)
		blurProjectRegion (w, bs->output, transform);
	}

	if (clientThreshold)
	{
	    /* center */
	    region.extents.x1 = w->attrib.x;
	    region.extents.y1 = w->attrib.y;
	    region.extents.x2 = w->attrib.x + w->width;
	    region.extents.y2 = w->attrib.y + w->height;

	    XIntersectRegion (bs->tmpRegion, &region, bs->tmpRegion2);
	    if (bs->tmpRegion2->numRects)
		blurProjectRegion (w, bs->output, transform);
	}
    }
    else
    {
	/* get region that needs blur */
	XSubtractRegion (bs->tmpRegion, &emptyRegion, bs->tmpRegion2);

	if (bs->tmpRegion2->numRects)
	    blurProjectRegion (w, bs->output, transform);
    }

    XIntersectRegion (bs->tmpRegion3, bs->region, bs->tmpRegion);

    if (XEmptyRegion (bs->tmpRegion))
	return FALSE;

    pBox = &bs->tmpRegion->extents;
    nBox = 1;

    *pExtents = bs->tmpRegion->extents;

    if (!bs->texture[0] || bs->width != s->width || bs->height != s->height)
    {
	int i, textures = 1;

	bs->width  = s->width;
	bs->height = s->height;

	if (s->textureNonPowerOfTwo ||
	    (POWER_OF_TWO (bs->width) && POWER_OF_TWO (bs->height)))
	{
	    bs->target = GL_TEXTURE_2D;
	    bs->tx = 1.0f / bs->width;
	    bs->ty = 1.0f / bs->height;
	}
	else
	{
	    bs->target = GL_TEXTURE_RECTANGLE_NV;
	    bs->tx = 1;
	    bs->ty = 1;
	}

	if (filter == BLUR_FILTER_GAUSSIAN)
	{
	    if (s->fbo && !bs->fbo)
		(*s->genFramebuffers) (1, &bs->fbo);

	    if (!bs->fbo)
		compLogMessage ("blur", CompLogLevelError,
				"Failed to create framebuffer object");

	    textures = 2;
	}

	bs->fboStatus = FALSE;

	for (i = 0; i < textures; i++)
	{
	    if (!bs->texture[i])
		glGenTextures (1, &bs->texture[i]);

	    glBindTexture (bs->target, bs->texture[i]);

	    glTexImage2D (bs->target, 0, GL_RGB,
			  bs->width,
			  bs->height,
			  0, GL_BGRA,

#if IMAGE_BYTE_ORDER == MSBFirst
			  GL_UNSIGNED_INT_8_8_8_8_REV,
#else
			  GL_UNSIGNED_BYTE,
#endif

			  NULL);

	    glTexParameteri (bs->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	    glTexParameteri (bs->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	    if (filter == BLUR_FILTER_MIPMAP)
	    {
		if (!s->fbo)
		{
		    compLogMessage ("blur", CompLogLevelWarn,
				    "GL_EXT_framebuffer_object extension "
				    "is required for mipmap filter");
		}
		else if (bs->target != GL_TEXTURE_2D)
		{
		    compLogMessage ("blur", CompLogLevelWarn,
				    "GL_ARB_texture_non_power_of_two "
				    "extension is required for mipmap filter");
		}
		else
		{
		    glTexParameteri (bs->target, GL_TEXTURE_MIN_FILTER,
				     GL_LINEAR_MIPMAP_LINEAR);
		    glTexParameteri (bs->target, GL_TEXTURE_MAG_FILTER,
				     GL_LINEAR_MIPMAP_LINEAR);
		}
	    }

	    glTexParameteri (bs->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	    glTexParameteri (bs->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	    glCopyTexSubImage2D (bs->target, 0, 0, 0, 0, 0,
				 bs->width, bs->height);
	}
    }
    else
    {
	glBindTexture (bs->target, bs->texture[0]);

	while (nBox--)
	{
	    y = s->height - pBox->y2;

	    glCopyTexSubImage2D (bs->target, 0,
				 pBox->x1, y,
				 pBox->x1, y,
				 pBox->x2 - pBox->x1,
				 pBox->y2 - pBox->y1);

	    pBox++;
	}
    }

    switch (filter) {
    case BLUR_FILTER_GAUSSIAN:
	return fboUpdate (s, bs->tmpRegion->rects, bs->tmpRegion->numRects);
    case BLUR_FILTER_MIPMAP:
	if (s->generateMipmap)
	    (*s->generateMipmap) (bs->target);
	break;
    case BLUR_FILTER_4X_BILINEAR:
	break;
    }

    glBindTexture (bs->target, 0);

    return TRUE;
}

static Bool
blurDrawWindow (CompWindow	     *w,
		const CompTransform  *transform,
		const FragmentAttrib *attrib,
		Region		     region,
		unsigned int	     mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    BLUR_SCREEN (s);
    BLUR_WINDOW (w);

    if (bs->alphaBlur && bw->region)
    {
	int clientThreshold;

	/* only care about client window blurring when it's translucent */
	if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	    clientThreshold = bw->state[BLUR_STATE_CLIENT].threshold;
	else
	    clientThreshold = 0;

	if (bw->state[BLUR_STATE_DECOR].threshold || clientThreshold)
	{
	    Bool   clipped = FALSE;
	    BoxRec box = { 0, 0, 0, 0 };
	    Region reg;
	    int    i;

	    for (i = 0; i < 16; i++)
		bs->mvp.m[i] = s->projection[i];

	    matrixMultiply (&bs->mvp, &bs->mvp, transform);

	    if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
		reg = &infiniteRegion;
	    else
		reg = region;

	    XIntersectRegion (bw->region, reg, bs->tmpRegion);
	    if (!bs->blurOcclusion && !(mask & PAINT_WINDOW_TRANSFORMED_MASK))
		XSubtractRegion(bs->tmpRegion, bw->clip, bs->tmpRegion);

	    if (blurUpdateDstTexture (w, transform, &box, clientThreshold))
	    {
		if (clientThreshold)
		{
		    if (bw->state[BLUR_STATE_CLIENT].clipped)
		    {
			if (bs->stencilBits)
			{
			    bw->state[BLUR_STATE_CLIENT].active = TRUE;
			    clipped = TRUE;
			}
		    }
		    else
		    {
			bw->state[BLUR_STATE_CLIENT].active = TRUE;
		    }
		}

		if (bw->state[BLUR_STATE_DECOR].threshold)
		{
		    if (bw->state[BLUR_STATE_DECOR].clipped)
		    {
			if (bs->stencilBits)
			{
			    bw->state[BLUR_STATE_DECOR].active = TRUE;
			    clipped = TRUE;
			}
		    }
		    else
		    {
			bw->state[BLUR_STATE_DECOR].active = TRUE;
		    }
		}

		if (!bs->blurOcclusion && bw->clip->numRects)
		    clipped = TRUE;
	    }

	    if (!bs->blurOcclusion)
		XSubtractRegion (bw->region, bw->clip, bs->tmpRegion);
	    else
		XSubtractRegion (bw->region, &emptyRegion, bs->tmpRegion);

	    if (!clientThreshold)
	    {
		REGION wRegion;
		wRegion.numRects   = 1;
		wRegion.rects      = &wRegion.extents;
		wRegion.extents.x1 = w->attrib.x;
		wRegion.extents.y1 = w->attrib.y;
		wRegion.extents.x2 = w->attrib.x + w->width;
		wRegion.extents.y2 = w->attrib.y + w->height;
		XSubtractRegion (bs->tmpRegion, &wRegion, bs->tmpRegion);
	    }

	    if (clipped)
	    {
		w->vCount = w->indexCount = 0;
		(*w->screen->addWindowGeometry) (w, NULL, 0, bs->tmpRegion, reg);
		if (w->vCount)
		{
		    BoxRec clearBox = bs->stencilBox;

		    bs->stencilBox = box;

		    glEnable (GL_STENCIL_TEST);
		    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		    if (clearBox.x2 > clearBox.x1 && clearBox.y2 > clearBox.y1)
		    {
			glPushAttrib (GL_SCISSOR_BIT);
			glEnable (GL_SCISSOR_TEST);
			glScissor (clearBox.x1,
				   s->height - clearBox.y2,
				   clearBox.x2 - clearBox.x1,
				   clearBox.y2 - clearBox.y1);
			glClear (GL_STENCIL_BUFFER_BIT);
			glPopAttrib ();
		    }

		    glStencilFunc (GL_ALWAYS, 0x1, ~0);
		    glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE);

		    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
		    (*w->drawWindowGeometry) (w);
		    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

		    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		    glDisable (GL_STENCIL_TEST);
		}
	    }
	}
    }

    UNWRAP (bs, s, drawWindow);
    status = (*s->drawWindow) (w, transform, attrib, region, mask);
    WRAP (bs, s, drawWindow, blurDrawWindow);

    bw->state[BLUR_STATE_CLIENT].active = FALSE;
    bw->state[BLUR_STATE_DECOR].active  = FALSE;

    return status;
}

static void
blurDrawWindowTexture (CompWindow	    *w,
		       CompTexture	    *texture,
		       const FragmentAttrib *attrib,
		       unsigned int	    mask)
{
    CompScreen *s = w->screen;
    int	       state;

    BLUR_SCREEN (s);
    BLUR_WINDOW (w);

    if (texture == w->texture)
	state = BLUR_STATE_CLIENT;
    else
	state = BLUR_STATE_DECOR;

    if (bw->blur || bw->state[state].active)
    {
	FragmentAttrib fa = *attrib;
	int	       param, function;
	int	       unit = 0;
	GLfloat	       dx, dy;
	int            iTC = 0;

	if (bw->blur)
	{
	    param = allocFragmentParameters (&fa, 1);

	    function = getSrcBlurFragmentFunction (s, texture, param);
	    if (function)
	    {
		addFragmentFunction (&fa, function);

		dx = ((texture->matrix.xx / 2.1f) * bw->blur) / 65535.0f;
		dy = ((texture->matrix.yy / 2.1f) * bw->blur) / 65535.0f;

		(*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
					     param, dx, dy, dx, -dy);

		/* bi-linear filtering is required */
		mask |= PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK;
	    }
	}

	if (bw->state[state].active)
	{
	    FragmentAttrib dstFa = fa;
	    float	   threshold = (float) bw->state[state].threshold;

	    switch (bs->opt[BLUR_SCREEN_OPTION_FILTER].value.i) {
	    case BLUR_FILTER_4X_BILINEAR:
		dx = bs->tx / 2.1f;
		dy = bs->ty / 2.1f;

		param = allocFragmentParameters (&dstFa, 3);
		unit  = allocFragmentTextureUnits (&dstFa, 1);

		function =
		    getDstBlurFragmentFunction (s, texture, param, unit, 0, 0);
		if (function)
		{
		    addFragmentFunction (&dstFa, function);

		    (*s->activeTexture) (GL_TEXTURE0_ARB + unit);
		    glBindTexture (bs->target, bs->texture[0]);
		    (*s->activeTexture) (GL_TEXTURE0_ARB);

		    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB, param,
						 bs->tx, bs->ty, 0.0f, 0.0f);

		    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
						 param + 1,
						 threshold, threshold,
						 threshold, threshold);

		    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
						 param + 2,
						 dx, dy, 0.0f, 0.0f);
		}
		break;
	    case BLUR_FILTER_GAUSSIAN:
		if (bs->opt[BLUR_SCREEN_OPTION_INDEPENDENT_TEX].value.b)
		{
		    /* leave one free texture unit for fragment position */
		    iTC = MAX (0, s->maxTextureUnits - (w->texUnits + 1));
		    if (iTC)
			iTC = MIN (iTC / 2, bs->numTexop);
		}

		param = allocFragmentParameters (&dstFa, 2);
		unit  = allocFragmentTextureUnits (&dstFa, 2);

		function =
		    getDstBlurFragmentFunction (s, texture, param, unit,
						iTC, w->texUnits);
		if (function)
		{
		    int           i;

		    addFragmentFunction (&dstFa, function);

		    (*s->activeTexture) (GL_TEXTURE0_ARB + unit);
		    glBindTexture (bs->target, bs->texture[0]);
		    (*s->activeTexture) (GL_TEXTURE0_ARB + unit + 1);
		    glBindTexture (bs->target, bs->texture[1]);
		    (*s->activeTexture) (GL_TEXTURE0_ARB);

		    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
						 param,
						 bs->tx, bs->ty,
						 0.0f, 0.0f);

		    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
						 param + 1,
						 threshold, threshold,
						 threshold, threshold);

		    if (iTC)
		    {
			CompTransform tm, rm;
		        float s_gen[4], t_gen[4], q_gen[4];

			for (i = 0; i < 16; i++)
			    tm.m[i] = 0;
			tm.m[0] = (bs->output->width / 2.0) * bs->tx;
			tm.m[5] = (bs->output->height / 2.0) * bs->ty;
			tm.m[10] = 1;

			tm.m[12] = (bs->output->width / 2.0 +
			bs->output->region.extents.x1) * bs->tx;
			tm.m[13] = (bs->output->height / 2.0 + s->height -
				bs->output->region.extents.y2) * bs->ty;
			tm.m[14] = 1;
			tm.m[15] = 1;

			matrixMultiply (&tm, &tm, &bs->mvp);

			for (i = 0; i < iTC; i++)
			{
			    (*s->activeTexture) (GL_TEXTURE0_ARB + w->texUnits
						 + (i * 2));

			    matrixGetIdentity (&rm);
			    rm.m[13] = bs->ty * bs->pos[i];
			    matrixMultiply (&rm, &rm, &tm);
			    s_gen[0] = rm.m[0];
			    s_gen[1] = rm.m[4];
			    s_gen[2] = rm.m[8];
			    s_gen[3] = rm.m[12];
			    t_gen[0] = rm.m[1];
			    t_gen[1] = rm.m[5];
			    t_gen[2] = rm.m[9];
			    t_gen[3] = rm.m[13];
			    q_gen[0] = rm.m[3];
			    q_gen[1] = rm.m[7];
			    q_gen[2] = rm.m[11];
			    q_gen[3] = rm.m[15];

			    glTexGenfv(GL_T, GL_OBJECT_PLANE, t_gen);
			    glTexGenfv(GL_S, GL_OBJECT_PLANE, s_gen);
			    glTexGenfv(GL_Q, GL_OBJECT_PLANE, q_gen);

			    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE,
				      GL_OBJECT_LINEAR);
			    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE,
				      GL_OBJECT_LINEAR);
			    glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE,
				      GL_OBJECT_LINEAR);

			    glEnable(GL_TEXTURE_GEN_S);
			    glEnable(GL_TEXTURE_GEN_T);
			    glEnable(GL_TEXTURE_GEN_Q);

			    (*s->activeTexture) (GL_TEXTURE0_ARB + w->texUnits
						 + 1 + (i * 2));

			    matrixGetIdentity (&rm);
			    rm.m[13] = -bs->ty * bs->pos[i];
			    matrixMultiply (&rm, &rm, &tm);
			    s_gen[0] = rm.m[0];
			    s_gen[1] = rm.m[4];
			    s_gen[2] = rm.m[8];
			    s_gen[3] = rm.m[12];
			    t_gen[0] = rm.m[1];
			    t_gen[1] = rm.m[5];
			    t_gen[2] = rm.m[9];
			    t_gen[3] = rm.m[13];
			    q_gen[0] = rm.m[3];
			    q_gen[1] = rm.m[7];
			    q_gen[2] = rm.m[11];
			    q_gen[3] = rm.m[15];

			    glTexGenfv(GL_T, GL_OBJECT_PLANE, t_gen);
			    glTexGenfv(GL_S, GL_OBJECT_PLANE, s_gen);
			    glTexGenfv(GL_Q, GL_OBJECT_PLANE, q_gen);

			    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE,
				      GL_OBJECT_LINEAR);
			    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE,
				      GL_OBJECT_LINEAR);
			    glTexGeni(GL_Q, GL_TEXTURE_GEN_MODE,
				      GL_OBJECT_LINEAR);

			    glEnable(GL_TEXTURE_GEN_S);
			    glEnable(GL_TEXTURE_GEN_T);
			    glEnable(GL_TEXTURE_GEN_Q);
			}
			
			(*s->activeTexture) (GL_TEXTURE0_ARB);
		    }

		}
		break;
	    case BLUR_FILTER_MIPMAP:
		param = allocFragmentParameters (&dstFa, 2);
		unit  = allocFragmentTextureUnits (&dstFa, 1);

		function =
		    getDstBlurFragmentFunction (s, texture, param, unit, 0, 0);
		if (function)
		{
		    float lod = bs->opt[BLUR_SCREEN_OPTION_MIPMAP_LOD].value.f;

		    addFragmentFunction (&dstFa, function);

		    (*s->activeTexture) (GL_TEXTURE0_ARB + unit);
		    glBindTexture (bs->target, bs->texture[0]);
		    (*s->activeTexture) (GL_TEXTURE0_ARB);

		    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
						 param,
						 bs->tx, bs->ty,
						 0.0f, lod);

		    (*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
						 param + 1,
						 threshold, threshold,
						 threshold, threshold);
		}
		break;
	    }

	    if (bw->state[state].clipped ||
		(!bs->blurOcclusion && bw->clip->numRects))
	    {
		glEnable (GL_STENCIL_TEST);

		glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
		glStencilFunc (GL_EQUAL, 0x1, ~0);

		/* draw region with destination blur */
		UNWRAP (bs, s, drawWindowTexture);
		(*s->drawWindowTexture) (w, texture, &dstFa, mask);

		glStencilFunc (GL_EQUAL, 0, ~0);

		/* draw region without destination blur */
		(*s->drawWindowTexture) (w, texture, &fa, mask);
		WRAP (bs, s, drawWindowTexture, blurDrawWindowTexture);

		glDisable (GL_STENCIL_TEST);
	    }
	    else
	    {
		/* draw with destination blur */
		UNWRAP (bs, s, drawWindowTexture);
		(*s->drawWindowTexture) (w, texture, &dstFa, mask);
		WRAP (bs, s, drawWindowTexture, blurDrawWindowTexture);
	    }
	}
	else
	{
	    UNWRAP (bs, s, drawWindowTexture);
	    (*s->drawWindowTexture) (w, texture, &fa, mask);
	    WRAP (bs, s, drawWindowTexture, blurDrawWindowTexture);
	}

	if (unit)
	{
	    (*s->activeTexture) (GL_TEXTURE0_ARB + unit);
	    glBindTexture (bs->target, 0);
	    (*s->activeTexture) (GL_TEXTURE0_ARB + unit + 1);
	    glBindTexture (bs->target, 0);
	    (*s->activeTexture) (GL_TEXTURE0_ARB);
	}
	
	if (iTC)
	{
	    int i;
	    for (i = w->texUnits; i < w->texUnits + (2 * iTC); i++)
	    {
		(*s->activeTexture) (GL_TEXTURE0_ARB + i);
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_Q);
	    }
	    (*s->activeTexture) (GL_TEXTURE0_ARB);
	}
    }
    else
    {
	UNWRAP (bs, s, drawWindowTexture);
	(*s->drawWindowTexture) (w, texture, attrib, mask);
	WRAP (bs, s, drawWindowTexture, blurDrawWindowTexture);
    }
}

static void
blurHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    Window activeWindow = d->activeWindow;

    BLUR_DISPLAY (d);

    UNWRAP (bd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (bd, d, handleEvent, blurHandleEvent);

    if (d->activeWindow != activeWindow)
    {
	CompWindow *w;

	w = findWindowAtDisplay (d, activeWindow);
	if (w)
	{
	    BLUR_SCREEN (w->screen);

	    if (bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR].value.b)
	    {
		addWindowDamage (w);
		bs->moreBlur = TRUE;
	    }
	}

	w = findWindowAtDisplay (d, d->activeWindow);
	if (w)
	{
	    BLUR_SCREEN (w->screen);

	    if (bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR].value.b)
	    {
		addWindowDamage (w);
		bs->moreBlur = TRUE;
	    }
	}
    }

    if (event->type == PropertyNotify)
    {
	int i;

	for (i = 0; i < BLUR_STATE_NUM; i++)
	{
	    if (event->xproperty.atom == bd->blurAtom[i])
	    {
		CompWindow *w;

		w = findWindowAtDisplay (d, event->xproperty.window);
		if (w)
		    blurWindowUpdate (w, i);
	    }
	}
    }
}

static void
blurWindowResizeNotify (CompWindow *w,
			int	   dx,
			int	   dy,
			int	   dwidth,
			int	   dheight)
{
    BLUR_SCREEN (w->screen);

    if (bs->alphaBlur)
    {
	BLUR_WINDOW (w);

	if (bw->state[BLUR_STATE_CLIENT].threshold ||
	    bw->state[BLUR_STATE_DECOR].threshold)
	    blurWindowUpdateRegion (w);
    }

    UNWRAP (bs, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w, dx, dy, dwidth, dheight);
    WRAP (bs, w->screen, windowResizeNotify, blurWindowResizeNotify);
}

static void
blurWindowMoveNotify (CompWindow *w,
		      int	 dx,
		      int	 dy,
		      Bool	 immediate)
{
    BLUR_SCREEN (w->screen);
    BLUR_WINDOW (w);

    if (bw->region)
	XOffsetRegion (bw->region, dx, dy);

    UNWRAP (bs, w->screen, windowMoveNotify);
    (*w->screen->windowMoveNotify) (w, dx, dy, immediate);
    WRAP (bs, w->screen, windowMoveNotify, blurWindowMoveNotify);
}

static CompOption *
blurGetDisplayOptions (CompPlugin  *plugin,
		       CompDisplay *display,
		       int	   *count)
{
    BLUR_DISPLAY (display);

    *count = NUM_OPTIONS (bd);
    return bd->opt;
}

static Bool
blurSetDisplayOption (CompPlugin      *plugin,
		      CompDisplay     *display,
		      const char      *name,
		      CompOptionValue *value)
{
    CompOption *o;

    BLUR_DISPLAY (display);

    o = compFindOption (bd->opt, NUM_OPTIONS (bd), name, NULL);
    if (!o)
	return FALSE;

    return compSetDisplayOption (display, o, value);
}

static Bool
blurPulse (CompDisplay     *d,
	   CompAction      *action,
	   CompActionState state,
	   CompOption      *option,
	   int	           nOption)
{
    CompWindow *w;
    int	       xid;

    xid = getIntOptionNamed (option, nOption, "window", d->activeWindow);

    w = findWindowAtDisplay (d, xid);
    if (w && w->screen->fragmentProgram)
    {
	BLUR_SCREEN (w->screen);
	BLUR_WINDOW (w);

	bw->pulse    = TRUE;
	bs->moreBlur = TRUE;

	addWindowDamage (w);
    }

    return FALSE;
}

static void
blurMatchExpHandlerChanged (CompDisplay *d)
{
    CompScreen *s;
    CompWindow *w;

    BLUR_DISPLAY (d);

    UNWRAP (bd, d, matchExpHandlerChanged);
    (*d->matchExpHandlerChanged) (d);
    WRAP (bd, d, matchExpHandlerChanged, blurMatchExpHandlerChanged);

    /* match options are up to date after the call to matchExpHandlerChanged */
    for (s = d->screens; s; s = s->next)
    {
	BLUR_SCREEN (s);

	for (w = s->windows; w; w = w->next)
	    blurUpdateWindowMatch (bs, w);
    }
}

static void
blurMatchPropertyChanged (CompDisplay *d,
			  CompWindow  *w)
{
    BLUR_DISPLAY (d);
    BLUR_SCREEN (w->screen);

    blurUpdateWindowMatch (bs, w);

    UNWRAP (bd, d, matchPropertyChanged);
    (*d->matchPropertyChanged) (d, w);
    WRAP (bd, d, matchPropertyChanged, blurMatchPropertyChanged);
}

static void
blurWindowAdd (CompScreen *s,
	       CompWindow *w)
{
    BLUR_SCREEN (s);

    blurWindowUpdate (w, BLUR_STATE_CLIENT);
    blurWindowUpdate (w, BLUR_STATE_DECOR);

    blurUpdateWindowMatch (bs, w);
}

static void
blurObjectAdd (CompObject *parent,
	       CompObject *object)
{
    static ObjectAddProc dispTab[] = {
	(ObjectAddProc) 0, /* CoreAdd */
	(ObjectAddProc) 0, /* DisplayAdd */
	(ObjectAddProc) 0, /* ScreenAdd */
	(ObjectAddProc) blurWindowAdd
    };

    BLUR_CORE (&core);

    UNWRAP (bc, &core, objectAdd);
    (*core.objectAdd) (parent, object);
    WRAP (bc, &core, objectAdd, blurObjectAdd);

    DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), (parent, object));
}

static Bool
blurInitCore (CompPlugin *p,
	      CompCore   *c)
{
    BlurCore *bc;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    bc = malloc (sizeof (BlurCore));
    if (!bc)
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	free (bc);
	return FALSE;
    }

    WRAP (bc, c, objectAdd, blurObjectAdd);

    c->base.privates[corePrivateIndex].ptr = bc;

    return TRUE;
}

static void
blurFiniCore (CompPlugin *p,
	      CompCore   *c)
{
    BLUR_CORE (c);

    freeDisplayPrivateIndex (displayPrivateIndex);

    UNWRAP (bc, c, objectAdd);

    free (bc);
}

static const CompMetadataOptionInfo blurDisplayOptionInfo[] = {
    { "pulse", "bell", 0, blurPulse, 0 }
};

static Bool
blurInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    BlurDisplay *bd;

    bd = malloc (sizeof (BlurDisplay));
    if (!bd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &blurMetadata,
					     blurDisplayOptionInfo,
					     bd->opt,
					     BLUR_DISPLAY_OPTION_NUM))
    {
	free (bd);
	return FALSE;
    }

    bd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (bd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, bd->opt, BLUR_DISPLAY_OPTION_NUM);
	free (bd);
	return FALSE;
    }

    bd->blurAtom[BLUR_STATE_CLIENT] =
	XInternAtom (d->display, "_COMPIZ_WM_WINDOW_BLUR", 0);
    bd->blurAtom[BLUR_STATE_DECOR] =
	XInternAtom (d->display, DECOR_BLUR_ATOM_NAME, 0);

    WRAP (bd, d, handleEvent, blurHandleEvent);
    WRAP (bd, d, matchExpHandlerChanged, blurMatchExpHandlerChanged);
    WRAP (bd, d, matchPropertyChanged, blurMatchPropertyChanged);

    d->base.privates[displayPrivateIndex].ptr = bd;

    return TRUE;
}

static void
blurFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    BLUR_DISPLAY (d);

    freeScreenPrivateIndex (d, bd->screenPrivateIndex);

    UNWRAP (bd, d, handleEvent);
    UNWRAP (bd, d, matchExpHandlerChanged);
    UNWRAP (bd, d, matchPropertyChanged);

    compFiniDisplayOptions (d, bd->opt, BLUR_DISPLAY_OPTION_NUM);

    free (bd);
}

static const CompMetadataOptionInfo blurScreenOptionInfo[] = {
    { "blur_speed", "float", "<min>0.1</min>", 0, 0 },
    { "focus_blur_match", "match", 0, 0, 0 },
    { "focus_blur", "bool", 0, 0, 0 },
    { "alpha_blur_match", "match", 0, 0, 0 },
    { "alpha_blur", "bool", 0, 0, 0 },
    { "filter", "int", RESTOSTRING (0, BLUR_FILTER_LAST), 0, 0 },
    { "gaussian_radius", "int", "<min>1</min><max>15</max>", 0, 0 },
    { "gaussian_strength", "float", "<min>0.0</min><max>1.0</max>", 0, 0 },
    { "mipmap_lod", "float", "<min>0.1</min><max>5.0</max>", 0, 0 },
    { "saturation", "int", "<min>0</min><max>100</max>", 0, 0 },
    { "occlusion", "bool", 0, 0, 0 },
    { "independent_tex", "bool", 0, 0, 0 }
};

static Bool
blurInitScreen (CompPlugin *p,
		CompScreen *s)
{
    BlurScreen *bs;
    int	       i;

    BLUR_DISPLAY (s->display);

    bs = malloc (sizeof (BlurScreen));
    if (!bs)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &blurMetadata,
					    blurScreenOptionInfo,
					    bs->opt,
					    BLUR_SCREEN_OPTION_NUM))
    {
	free (bs);
	return FALSE;
    }

    bs->region = XCreateRegion ();
    if (!bs->region)
    {
	compFiniScreenOptions (s, bs->opt, BLUR_SCREEN_OPTION_NUM);
	free (bs);
	return FALSE;
    }

    bs->tmpRegion = XCreateRegion ();
    if (!bs->tmpRegion)
    {
	compFiniScreenOptions (s, bs->opt, BLUR_SCREEN_OPTION_NUM);
	XDestroyRegion (bs->region);
	free (bs);
	return FALSE;
    }

    bs->tmpRegion2 = XCreateRegion ();
    if (!bs->tmpRegion2)
    {
	compFiniScreenOptions (s, bs->opt, BLUR_SCREEN_OPTION_NUM);
	XDestroyRegion (bs->region);
	XDestroyRegion (bs->tmpRegion);
	free (bs);
	return FALSE;
    }

    bs->tmpRegion3 = XCreateRegion ();
    if (!bs->tmpRegion3)
    {
	compFiniScreenOptions (s, bs->opt, BLUR_SCREEN_OPTION_NUM);
	XDestroyRegion (bs->region);
	XDestroyRegion (bs->tmpRegion);
	XDestroyRegion (bs->tmpRegion2);
	free (bs);
	return FALSE;
    }

    bs->occlusion = XCreateRegion ();
    if (!bs->occlusion)
    {
	compFiniScreenOptions (s, bs->opt, BLUR_SCREEN_OPTION_NUM);
	XDestroyRegion (bs->region);
	XDestroyRegion (bs->tmpRegion);
	XDestroyRegion (bs->tmpRegion2);
	XDestroyRegion (bs->tmpRegion3);
	free (bs);
	return FALSE;
    }


    bs->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (bs->windowPrivateIndex < 0)
    {
	compFiniScreenOptions (s, bs->opt, BLUR_SCREEN_OPTION_NUM);
	XDestroyRegion (bs->region);
	XDestroyRegion (bs->tmpRegion);
	XDestroyRegion (bs->tmpRegion2);
	XDestroyRegion (bs->tmpRegion3);
	XDestroyRegion (bs->occlusion);
	free (bs);
	return FALSE;
    }

    bs->output = NULL;
    bs->count  = 0;

    bs->filterRadius = 0;

    bs->srcBlurFunctions = NULL;
    bs->dstBlurFunctions = NULL;
    bs->blurTime	 = 1000.0f /
	bs->opt[BLUR_SCREEN_OPTION_BLUR_SPEED].value.f;
    bs->moreBlur	 = FALSE;
    bs->blurOcclusion    =
	bs->opt[BLUR_SCREEN_OPTION_BLUR_OCCLUSION].value.b;

    for (i = 0; i < 2; i++)
	bs->texture[i] = 0;

    bs->program   = 0;
    bs->maxTemp   = 32;
    bs->fbo	  = 0;
    bs->fboStatus = FALSE;

    glGetIntegerv (GL_STENCIL_BITS, &bs->stencilBits);
    if (!bs->stencilBits)
	compLogMessage ("blur", CompLogLevelWarn,
			"No stencil buffer. Region based blur disabled");

    /* We need GL_ARB_fragment_program for blur */
    if (s->fragmentProgram)
	bs->alphaBlur = bs->opt[BLUR_SCREEN_OPTION_ALPHA_BLUR].value.b;
    else
	bs->alphaBlur = FALSE;

    if (s->fragmentProgram)
    {
	int tmp[4];
	s->getProgramiv (GL_FRAGMENT_PROGRAM_ARB,
			 GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB,
			 tmp);
	bs->maxTemp = tmp[0];
    }

    WRAP (bs, s, preparePaintScreen, blurPreparePaintScreen);
    WRAP (bs, s, donePaintScreen, blurDonePaintScreen);
    WRAP (bs, s, paintOutput, blurPaintOutput);
    WRAP (bs, s, paintTransformedOutput, blurPaintTransformedOutput);
    WRAP (bs, s, paintWindow, blurPaintWindow);
    WRAP (bs, s, drawWindow, blurDrawWindow);
    WRAP (bs, s, drawWindowTexture, blurDrawWindowTexture);
    WRAP (bs, s, windowResizeNotify, blurWindowResizeNotify);
    WRAP (bs, s, windowMoveNotify, blurWindowMoveNotify);

    s->base.privates[bd->screenPrivateIndex].ptr = bs;

    blurUpdateFilterRadius (s);

    return TRUE;
}

static void
blurFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    int i;

    BLUR_SCREEN (s);

    blurDestroyFragmentFunctions (s, &bs->srcBlurFunctions);
    blurDestroyFragmentFunctions (s, &bs->dstBlurFunctions);

    damageScreen (s);

    XDestroyRegion (bs->region);
    XDestroyRegion (bs->tmpRegion);
    XDestroyRegion (bs->tmpRegion2);
    XDestroyRegion (bs->tmpRegion3);
    XDestroyRegion (bs->occlusion);

    if (bs->fbo)
	(*s->deleteFramebuffers) (1, &bs->fbo);

    for (i = 0; i < 2; i++)
	if (bs->texture[i])
	    glDeleteTextures (1, &bs->texture[i]);

    freeWindowPrivateIndex (s, bs->windowPrivateIndex);

    UNWRAP (bs, s, preparePaintScreen);
    UNWRAP (bs, s, donePaintScreen);
    UNWRAP (bs, s, paintOutput);
    UNWRAP (bs, s, paintTransformedOutput);
    UNWRAP (bs, s, paintWindow);
    UNWRAP (bs, s, drawWindow);
    UNWRAP (bs, s, drawWindowTexture);
    UNWRAP (bs, s, windowResizeNotify);
    UNWRAP (bs, s, windowMoveNotify);

    compFiniScreenOptions (s, bs->opt, BLUR_SCREEN_OPTION_NUM);

    free (bs);
}

static Bool
blurInitWindow (CompPlugin *p,
		CompWindow *w)
{
    BlurWindow *bw;
    int	       i;

    BLUR_SCREEN (w->screen);

    bw = malloc (sizeof (BlurWindow));
    if (!bw)
	return FALSE;

    bw->blur      = 0;
    bw->pulse     = FALSE;
    bw->focusBlur = FALSE;

    for (i = 0; i < BLUR_STATE_NUM; i++)
    {
	bw->state[i].threshold = 0;
	bw->state[i].box       = NULL;
	bw->state[i].nBox      = 0;
	bw->state[i].clipped   = FALSE;
	bw->state[i].active    = FALSE;

	bw->propSet[i] = FALSE;
    }

    bw->region = NULL;

    bw->clip = XCreateRegion ();
    if (!bw->clip)
    {
	free (bw);
	return FALSE;
    }

    w->base.privates[bs->windowPrivateIndex].ptr = bw;

    if (w->base.parent)
	blurWindowAdd (w->screen, w);

    return TRUE;
}

static void
blurFiniWindow (CompPlugin *p,
		CompWindow *w)
{
    int i;

    BLUR_WINDOW (w);

    for (i = 0; i < BLUR_STATE_NUM; i++)
	if (bw->state[i].box)
	    free (bw->state[i].box);

    if (bw->region)
	XDestroyRegion (bw->region);

    XDestroyRegion (bw->clip);

    free (bw);
}

static CompBool
blurInitObject (CompPlugin *p,
		CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) blurInitCore,
	(InitPluginObjectProc) blurInitDisplay,
	(InitPluginObjectProc) blurInitScreen,
	(InitPluginObjectProc) blurInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
blurFiniObject (CompPlugin *p,
		CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) blurFiniCore,
	(FiniPluginObjectProc) blurFiniDisplay,
	(FiniPluginObjectProc) blurFiniScreen,
	(FiniPluginObjectProc) blurFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
blurGetObjectOptions (CompPlugin *plugin,
		      CompObject *object,
		      int	 *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) blurGetDisplayOptions,
	(GetPluginObjectOptionsProc) blurGetScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
blurSetObjectOption (CompPlugin      *plugin,
		     CompObject      *object,
		     const char      *name,
		     CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) blurSetDisplayOption,
	(SetPluginObjectOptionProc) blurSetScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
blurInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&blurMetadata,
					 p->vTable->name,
					 blurDisplayOptionInfo,
					 BLUR_DISPLAY_OPTION_NUM,
					 blurScreenOptionInfo,
					 BLUR_SCREEN_OPTION_NUM))
	return FALSE;

    corePrivateIndex = allocateCorePrivateIndex ();
    if (corePrivateIndex < 0)
    {
	compFiniMetadata (&blurMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&blurMetadata, p->vTable->name);

    return TRUE;
}

static void
blurFini (CompPlugin *p)
{
    freeCorePrivateIndex (corePrivateIndex);
    compFiniMetadata (&blurMetadata);
}

static CompMetadata *
blurGetMetadata (CompPlugin *plugin)
{
    return &blurMetadata;
}

static CompPluginVTable blurVTable = {
    "blur",
    blurGetMetadata,
    blurInit,
    blurFini,
    blurInitObject,
    blurFiniObject,
    blurGetObjectOptions,
    blurSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &blurVTable;
}
