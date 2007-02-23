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

#include <compiz.h>
#include <decoration.h>

#include <X11/Xatom.h>
#include <GL/glu.h>

#define BLUR_SPEED_DEFAULT    3.5f
#define BLUR_SPEED_MIN        0.1f
#define BLUR_SPEED_MAX       10.0f
#define BLUR_SPEED_PRECISION  0.1f

#define BLUR_FOCUS_BLUR_DEFAULT FALSE

#define BLUR_ALPHA_BLUR_DEFAULT TRUE

#define BLUR_PULSE_DEFAULT FALSE

#define BLUR_GAUSSIAN_RADIUS_DEFAULT  3
#define BLUR_GAUSSIAN_RADIUS_MIN      1
#define BLUR_GAUSSIAN_RADIUS_MAX     15

#define BLUR_GAUSSIAN_STRENGTH_DEFAULT   1.0f
#define BLUR_GAUSSIAN_STRENGTH_MIN       0.0f
#define BLUR_GAUSSIAN_STRENGTH_MAX       1.0f
#define BLUR_GAUSSIAN_STRENGTH_PRECISION 0.01f

#define BLUR_MIPMAP_LOD_DEFAULT   2.5f
#define BLUR_MIPMAP_LOD_MIN       0.1f
#define BLUR_MIPMAP_LOD_MAX       5.0f
#define BLUR_MIPMAP_LOD_PRECISION 0.1f

static char *winType[] = {
    N_("Toolbar"),
    N_("Menu"),
    N_("Utility"),
    N_("Normal"),
    N_("Dialog"),
    N_("ModalDialog")
};
#define N_WIN_TYPE (sizeof (winType) / sizeof (winType[0]))

static char *filterString[] = {
    N_("4xBilinear"),
    N_("Gaussian"),
    N_("Mipmap")
};
static int  nFilterString = sizeof (filterString) / sizeof (filterString[0]);

#define BLUR_FILTER_DEFAULT (filterString[0])

typedef enum {
    BlurFilter4xBilinear,
    BlurFilterGaussian,
    BlurFilterMipmap
} BlurFilter;

typedef struct _BlurFunction {
    struct _BlurFunction *next;

    int handle;
    int target;
    int param;
    int unit;
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

static int displayPrivateIndex;

#define BLUR_DISPLAY_OPTION_PULSE 0
#define BLUR_DISPLAY_OPTION_NUM   1

typedef struct _BlurDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[BLUR_DISPLAY_OPTION_NUM];

    Atom blurAtom[BLUR_STATE_NUM];
} BlurDisplay;

#define BLUR_SCREEN_OPTION_BLUR_SPEED        0
#define BLUR_SCREEN_OPTION_WINDOW_TYPE       1
#define BLUR_SCREEN_OPTION_FOCUS_BLUR        2
#define BLUR_SCREEN_OPTION_ALPHA_BLUR        3
#define BLUR_SCREEN_OPTION_FILTER            4
#define BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS   5
#define BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH 6
#define BLUR_SCREEN_OPTION_MIPMAP_LOD        7
#define BLUR_SCREEN_OPTION_NUM		     8

typedef struct _BlurScreen {
    int	windowPrivateIndex;

    CompOption opt[BLUR_SCREEN_OPTION_NUM];

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintScreenProc	   paintScreen;
    DrawWindowProc	   drawWindow;
    DrawWindowTextureProc  drawWindowTexture;

    WindowResizeNotifyProc windowResizeNotify;
    WindowMoveNotifyProc   windowMoveNotify;

    int  wMask;
    Bool alphaBlur;

    int	 blurTime;
    Bool moreBlur;

    BlurFilter filter;
    int        filterRadius;

    BlurFunction *srcBlurFunctions;
    BlurFunction *dstBlurFunctions;

    Region region;
    Region tmpRegion;

    BoxRec stencilBox;
    GLint  stencilBits;

    int output;
    int count;

    GLuint texture[2];

    GLenum target;
    float  tx;
    float  ty;
    int    width;
    int    height;

    GLuint program;
    GLuint fbo;
    Bool   fboStatus;
} BlurScreen;

typedef struct _BlurWindow {
    int  blur;
    Bool pulse;

    BlurState state[BLUR_STATE_NUM];

    Region region;
} BlurWindow;

#define GET_BLUR_DISPLAY(d)				     \
    ((BlurDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define BLUR_DISPLAY(d)			   \
    BlurDisplay *bd = GET_BLUR_DISPLAY (d)

#define GET_BLUR_SCREEN(s, bd)					 \
    ((BlurScreen *) (s)->privates[(bd)->screenPrivateIndex].ptr)

#define BLUR_SCREEN(s)							\
    BlurScreen *bs = GET_BLUR_SCREEN (s, GET_BLUR_DISPLAY (s->display))

#define GET_BLUR_WINDOW(w, bs)					 \
    ((BlurWindow *) (w)->privates[(bs)->windowPrivateIndex].ptr)

#define BLUR_WINDOW(w)					     \
    BlurWindow *bw = GET_BLUR_WINDOW  (w,		     \
		     GET_BLUR_SCREEN  (w->screen,	     \
		     GET_BLUR_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static BlurFilter
blurFilterFromString (CompOptionValue *value)
{
    if (strcmp (value->s, N_("Gaussian")) == 0)
	return BlurFilterGaussian;
    else if (strcmp (value->s, N_("Mipmap")) == 0)
	return BlurFilterMipmap;
    else
	return BlurFilter4xBilinear;
}

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

    for (i = 3; i <= size;i++)
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

    switch (bs->filter) {
    case BlurFilter4xBilinear:
	bs->filterRadius = 2;
	break;
    case BlurFilterGaussian:
	bs->filterRadius = bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS].value.i;
	break;
    case BlurFilterMipmap: {
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

static CompOption *
blurGetScreenOptions (CompScreen *screen,
		      int	 *count)
{
    BLUR_SCREEN (screen);

    *count = NUM_OPTIONS (bs);
    return bs->opt;
}

static Bool
blurSetScreenOption (CompScreen      *screen,
		     char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index;

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
    case BLUR_SCREEN_OPTION_WINDOW_TYPE:
	if (compSetOptionList (o, value))
	{
	    if (screen->fragmentProgram)
		bs->wMask = compWindowTypeMaskFromStringList (&o->value);
	    else
		bs->wMask = 0;

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
	if (compSetStringOption (o, value))
	{
	    bs->filter = blurFilterFromString (&o->value);
	    blurReset (screen);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS:
	if (compSetIntOption (o, value))
	{
	    if (bs->filter == BlurFilterGaussian)
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
	    if (bs->filter == BlurFilterGaussian)
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
	    if (bs->filter == BlurFilterMipmap)
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
blurScreenInitOptions (BlurScreen *bs)
{
    CompOption *o;
    int	       i;

    o = &bs->opt[BLUR_SCREEN_OPTION_BLUR_SPEED];
    o->name		= "blur_speed";
    o->shortDesc	= N_("Blur Speed");
    o->longDesc		= N_("Window blur speed");
    o->type		= CompOptionTypeFloat;
    o->value.f		= BLUR_SPEED_DEFAULT;
    o->rest.f.min	= BLUR_SPEED_MIN;
    o->rest.f.max	= BLUR_SPEED_MAX;
    o->rest.f.precision = BLUR_SPEED_PRECISION;

    o = &bs->opt[BLUR_SCREEN_OPTION_WINDOW_TYPE];
    o->name	         = "window_types";
    o->shortDesc         = N_("Window Types");
    o->longDesc	         = N_("Window types that should be blurred");
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_WIN_TYPE;
    o->value.list.value  = malloc (sizeof (CompOptionValue) * N_WIN_TYPE);
    for (i = 0; i < N_WIN_TYPE; i++)
	o->value.list.value[i].s = strdup (winType[i]);
    o->rest.s.string     = windowTypeString;
    o->rest.s.nString    = nWindowTypeString;

    bs->wMask = compWindowTypeMaskFromStringList (&o->value);

    o = &bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR];
    o->name	 = "focus_blur";
    o->shortDesc = N_("Focus Blur");
    o->longDesc	 = N_("Blur windows that doesn't have focus");
    o->type	 = CompOptionTypeBool;
    o->value.b   = BLUR_FOCUS_BLUR_DEFAULT;

    o = &bs->opt[BLUR_SCREEN_OPTION_ALPHA_BLUR];
    o->name	 = "alpha_blur";
    o->shortDesc = N_("Alpha Blur");
    o->longDesc	 = N_("Blur behind translucent parts of windows");
    o->type	 = CompOptionTypeBool;
    o->value.b   = BLUR_ALPHA_BLUR_DEFAULT;

    bs->alphaBlur = o->value.b;

    o = &bs->opt[BLUR_SCREEN_OPTION_FILTER];
    o->name	         = "filter";
    o->shortDesc         = N_("Blur Filter");
    o->longDesc	         = N_("Filter method used for blurring");
    o->type	         = CompOptionTypeString;
    o->value.s		 = strdup (BLUR_FILTER_DEFAULT);
    o->rest.s.string     = filterString;
    o->rest.s.nString    = nFilterString;

    bs->filter = blurFilterFromString (&o->value);

    o = &bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS];
    o->name	  = "gaussian_radius";
    o->shortDesc  = N_("Gaussian Radius");
    o->longDesc	  = N_("Gaussian radius");
    o->type	  = CompOptionTypeInt;
    o->value.i	  = BLUR_GAUSSIAN_RADIUS_DEFAULT;
    o->rest.i.min = BLUR_GAUSSIAN_RADIUS_MIN;
    o->rest.i.max = BLUR_GAUSSIAN_RADIUS_MAX;

    o = &bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH];
    o->name		= "gaussian_strength";
    o->shortDesc	= N_("Gaussian Strength");
    o->longDesc		= N_("Gaussian strength");
    o->type		= CompOptionTypeFloat;
    o->value.f		= BLUR_GAUSSIAN_STRENGTH_DEFAULT;
    o->rest.f.min	= BLUR_GAUSSIAN_STRENGTH_MIN;
    o->rest.f.max	= BLUR_GAUSSIAN_STRENGTH_MAX;
    o->rest.f.precision = BLUR_GAUSSIAN_STRENGTH_PRECISION;

    o = &bs->opt[BLUR_SCREEN_OPTION_MIPMAP_LOD];
    o->name		= "mipmap_lod";
    o->shortDesc	= N_("Mipmap LOD");
    o->longDesc		= N_("Mipmap level-of-detail");
    o->type		= CompOptionTypeFloat;
    o->value.f		= BLUR_MIPMAP_LOD_DEFAULT;
    o->rest.f.min	= BLUR_MIPMAP_LOD_MIN;
    o->rest.f.max	= BLUR_MIPMAP_LOD_MAX;
    o->rest.f.precision = BLUR_MIPMAP_LOD_PRECISION;
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
blurWindowUpdate (CompWindow *w,
		  int	 state)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *propData;
    int		  threshold = 0;
    BlurBox	  *box = NULL;
    int		  nBox = 0;

    BLUR_DISPLAY (w->screen->display);

    result = XGetWindowProperty (w->screen->display->display, w->id,
				 bd->blurAtom[state], 0L, 8192L, FALSE,
				 XA_INTEGER, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && n && propData)
    {
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

    blurSetWindowBlur (w,
		       state,
		       threshold,
		       box,
		       nBox);
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

	steps = (msSinceLastPaint * 0xffff) / bs->blurTime;
	if (steps < 12)
	    steps = 12;

	bs->moreBlur = FALSE;

	for (w = s->windows; w; w = w->next)
	{
	    if (bs->wMask & w->type)
	    {
		BLUR_WINDOW (w);

		if (!bw->pulse && (!focus || w->id == s->display->activeWindow))
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
blurPaintScreen (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 const CompTransform	 *transform,
		 Region			 region,
		 int			 output,
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

    bs->output = output;

    UNWRAP (bs, s, paintScreen);
    status = (*s->paintScreen) (s, sAttrib, transform, region, output, mask);
    WRAP (bs, s, paintScreen, blurPaintScreen);

    return status;
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
	    if (bs->wMask & w->type)
	    {
		BLUR_WINDOW (w);

		if (bw->blur > 0 && bw->blur < 0xffff)
		    addWindowDamage (w);
	    }
	}
    }

    UNWRAP (bs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (bs, s, donePaintScreen, blurDonePaintScreen);
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

	switch (bs->filter) {
	case BlurFilter4xBilinear:
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
			    int		unit)
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
	if (function->param  == param  &&
	    function->target == target &&
	    function->unit   == unit)
	    return function->handle;

    data = createFunctionData ();
    if (data)
    {
	static char *temp[] = { "coord", "mask", "sum", "dst" };
	int	    i, handle = 0;
	char	    str[1024];
	Bool	    ok = TRUE;

	for (i = 0; i < sizeof (temp) / sizeof (temp[0]); i++)
	    ok &= addTempHeaderOpToFunctionData (data, temp[i]);

	switch (bs->filter) {
	case BlurFilter4xBilinear: {
	    static char *filterTemp[] = {
		"t0", "t1", "t2", "t3",
		"s0", "s1", "s2", "s3"
	    };

	    for (i = 0; i < sizeof (filterTemp) / sizeof (filterTemp[0]); i++)
		ok &= addTempHeaderOpToFunctionData (data, filterTemp[i]);

	    ok &= addFetchOpToFunctionData (data, "output", NULL, target);
	    ok &= addColorOpToFunctionData (data, "output", "output");

	    snprintf (str, 1024,
		      "MUL coord, fragment.position, program.env[%d];",
		      param);

	    ok &= addDataOpToFunctionData (data, str);

	    snprintf (str, 1024,
		      "ADD t0, coord, program.env[%d];"
		      "TEX s0, t0, texture[%d], %s;"

		      "SUB t1, coord, program.env[%d];"
		      "TEX s1, t1, texture[%d], %s;"

		      "MAD t2, coord, program.env[%d], { -1.0, 1.0, 0.0, 0.0 };"
		      "TEX s2, t2, texture[%d], %s;"

		      "MAD t2, coord, program.env[%d], { 1.0, -1.0, 0.0, 0.0 };"
		      "TEX s3, t3, texture[%d], %s;"

		      "TEX dst, coord, texture[%d], %s;"
		      "MUL_SAT mask, output.a, program.env[%d];"

		      "MUL sum, s0, 0.25;"
		      "MAD sum, s1, 0.25, sum;"
		      "MAD sum, s2, 0.25, sum;"
		      "MAD sum, s3, 0.25, sum;",

		      param + 2, unit, targetString,
		      param + 2, unit, targetString,
		      param + 2, unit, targetString,
		      param + 2, unit, targetString,
		      unit, targetString,
		      param + 1);

	    ok &= addDataOpToFunctionData (data, str);
	} break;
	case BlurFilterGaussian: {
	    static char *filterTemp[] = {
		"tCoord", "pix"
	    };
	    int		radius =
		bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS].value.i;
	    float	strength =
		bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH].value.f;
	    float	amp[BLUR_GAUSSIAN_RADIUS_MAX];
	    float	pos[BLUR_GAUSSIAN_RADIUS_MAX];
	    int		numTexop;

	    blurCreateGaussianLinearKernel (radius, strength, amp, pos,
					    &numTexop);

	    for (i = 0; i < sizeof (filterTemp) / sizeof (filterTemp[0]); i++)
		ok &= addTempHeaderOpToFunctionData (data, filterTemp[i]);

	    ok &= addFetchOpToFunctionData (data, "output", NULL, target);
	    ok &= addColorOpToFunctionData (data, "output", "output");

	    snprintf (str, 1024,
		      "MUL coord, fragment.position, program.env[%d];",
		      param);

	    ok &= addDataOpToFunctionData (data, str);

	    snprintf (str, 1024,
		      "TEX sum, coord, texture[%d], %s;",
		      unit + 1, targetString);

	    ok &= addDataOpToFunctionData (data, str);

	    snprintf (str, 1024,
		      "TEX dst, coord, texture[%d], %s;"
		      "MUL_SAT mask, output.a, program.env[%d];"
		      "MUL sum, sum, %f;",
		      unit, targetString, param + 1, amp[numTexop]);

	    ok &= addDataOpToFunctionData (data, str);

	    for (i = 0; i < numTexop; i++)
	    {
		snprintf (str, 1024,
			  "ADD tCoord, coord, program.env[%d];"
			  "TEX pix, tCoord, texture[%d], %s;"
			  "MAD sum, pix, %f, sum;"
			  "SUB tCoord, coord, program.env[%d];"
			  "TEX pix, tCoord, texture[%d], %s;"
			  "MAD sum, pix, %f, sum;",
			  param + 2 + i,
			  unit + 1, targetString,
			  amp[i],
			  param + 2 + i,
			  unit + 1, targetString,
			  amp[i]);

		ok &= addDataOpToFunctionData (data, str);
	    }
	} break;
	case BlurFilterMipmap:
	    ok &= addFetchOpToFunctionData (data, "output", NULL, target);
	    ok &= addColorOpToFunctionData (data, "output", "output");

	    snprintf (str, 1024,
		      "MUL coord, fragment.position, program.env[%d].xyzz;"
		      "TEX dst, coord, texture[%d], %s;"
		      "MOV coord.w, program.env[%d].w;"
		      "TXB sum, coord, texture[%d], %s;"
		      "MUL_SAT mask, output.a, program.env[%d];",
		      param, unit, targetString,
		      param, unit, targetString,
		      param + 1);

	    ok &= addDataOpToFunctionData (data, str);
	    break;
	}

	snprintf (str, 1024,
		  "LRP sum, mask, sum, dst;"
		  "SUB output.a, 1.0, output.a;"
		  "MAD output.rgb, sum, output.a, output;"
		  "MOV output.a, 1.0;");

	ok &= addBlendOpToFunctionData (data, str);

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
	    function->unit   = unit;

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
		 int		     output,
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

    viewport[0] = s->outputDev[output].region.extents.x1;
    viewport[1] = s->height - s->outputDev[output].region.extents.y2;
    viewport[2] = s->outputDev[output].width;
    viewport[3] = s->outputDev[output].height;

    for (i = 0; i < 16; i++)
    {
	dModel[i]      = transform->m[i];
	dProjection[i] = s->projection[i];
    }

    while (n--)
    {
	if (!gluProject (object[0], object[1], 0.0,
			 dModel, dProjection, viewport,
			 &x, &y, &z))
	    return FALSE;

	screen[0] = x;
	screen[1] = y;

	object += 2;
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
	fprintf (stderr, "%s: blur: failed to load blur program\n",
		 programName);

	(*s->deletePrograms) (1, program);
	*program = 0;

	return FALSE;
    }

    return TRUE;
}

static Bool
loadFilterProgram (CompScreen *s)
{
    char  buffer[2048];
    char  *targetString;
    char  *str = buffer;
    float amp[BLUR_GAUSSIAN_RADIUS_MAX];
    float pos[BLUR_GAUSSIAN_RADIUS_MAX];
    int   numTexop;
    int   radius;
    float strength;
    int   i;

    BLUR_SCREEN (s);

    radius = bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS].value.i;
    strength = bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH].value.f;

    blurCreateGaussianLinearKernel (radius, strength, amp, pos, &numTexop);

    if (bs->target == GL_TEXTURE_2D)
	targetString = "2D";
    else
	targetString = "RECT";

    str += sprintf (str,
		    "!!ARBfp1.0"
		    "ATTRIB texcoord = fragment.texcoord[0];"
		    "TEMP sum;");

    str += sprintf (str, "TEMP tCoord, pix;");

    str += sprintf (str,
		    "TEX sum, texcoord, texture[0], %s;",
		    targetString);

    str += sprintf (str,
		    "MUL sum, sum, %f;",
		    amp[numTexop]);

    for (i = 0; i < numTexop; i++)
	str += sprintf (str,
			"ADD tCoord, texcoord, program.local[%d];"
			"TEX pix, tCoord, texture[0], %s;"
			"MAD sum, pix, %f, sum;"
			"SUB tCoord, texcoord, program.local[%d];"
			"TEX pix, tCoord, texture[0], %s;"
			"MAD sum, pix, %f, sum;",
			i, targetString, amp[i],
			i, targetString, amp[i]);

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
	    fprintf (stderr, "%s: blur: framebuffer incomplete\n",
		     programName);

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
    int   i, y;
    float amp[BLUR_GAUSSIAN_RADIUS_MAX];
    float pos[BLUR_GAUSSIAN_RADIUS_MAX];
    int   numTexop;
    float radius;
    float strength;

    BLUR_SCREEN (s);

    radius = bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS].value.i;
    strength = bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH].value.f;

    if (!bs->program)
	if (!loadFilterProgram (s))
	    return FALSE;

    if (!fboPrologue (s))
	return FALSE;

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);

    glBindTexture (bs->target, bs->texture[0]);

    glEnable (GL_FRAGMENT_PROGRAM_ARB);
    (*s->bindProgram) (GL_FRAGMENT_PROGRAM_ARB, bs->program);

    blurCreateGaussianLinearKernel (radius, strength, amp, pos, &numTexop);

    for (i = 0; i < numTexop; i++)
	(*s->programLocalParameter4f) (GL_FRAGMENT_PROGRAM_ARB, i,
				       bs->tx * pos[i],
				       0.0f, 0.0f, 0.0f);

    glBegin (GL_QUADS);

    while (nBox--)
    {
	y = s->height - pBox->y2;

	glTexCoord2f (bs->tx * pBox->x1, bs->ty * y);
	glVertex2i   (pBox->x1, y);
	glTexCoord2f (bs->tx * pBox->x2, bs->ty * y);
	glVertex2i   (pBox->x2, y);

	y = s->height - pBox->y1;

	glTexCoord2f (bs->tx * pBox->x2, bs->ty * y);
	glVertex2i   (pBox->x2, y);
	glTexCoord2f (bs->tx * pBox->x1, bs->ty * y);
	glVertex2i   (pBox->x1, y);

	pBox++;
    }

    glEnd ();

    glDisable (GL_FRAGMENT_PROGRAM_ARB);

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

    fboEpilogue (s);

    return TRUE;
}

static Bool
blurUpdateDstTexture (CompWindow	  *w,
		      const CompTransform *transform,
		      BoxPtr		  pExtents)
{
    CompScreen *s = w->screen;
    BoxPtr     pBox;
    int	       nBox;
    REGION     region;
    int        y, i, stride = (1 + w->texUnits) * 2;
    float      *v, *vertices = w->vertices + (stride - 2);
    float      screen[8];
    float      extents[8];
    float      minX, maxX, minY, maxY;

    BLUR_SCREEN (s);

    minX = s->width;
    maxX = 0;
    minY = s->height;
    maxY = 0;

    for (i = 0; i < w->vCount; i++)
    {
	v = vertices + stride * i;

	if (v[0] < minX)
	    minX = v[0];

	if (v[0] > maxX)
	    maxX = v[0];

	if (v[1] < minY)
	    minY = v[1];

	if (v[1] > maxY)
	    maxY = v[1];
    }

    extents[0] = extents[6] = minX;
    extents[2] = extents[4] = maxX;
    extents[1] = extents[3] = minY;
    extents[5] = extents[7] = maxY;

    /* Project extents and calculate a bounding box in screen space. It
       might make sense to move this into the core so that the result
       can be reused by other plugins. */
    if (!projectVertices (w->screen, bs->output, transform, extents, screen, 4))
	return FALSE;

    minX = s->width;
    maxX = 0;
    minY = s->height;
    maxY = 0;

    for (i = 0; i < 8; i += 2)
    {
	if (screen[i] < minX)
	    minX = screen[i];

	if (screen[i] > maxX)
	    maxX = screen[i];

	if (screen[i + 1] < minY)
	    minY = screen[i + 1];

	if (screen[i + 1] > maxY)
	    maxY = screen[i + 1];
    }

    region.extents.x1 = minX - bs->filterRadius;
    region.extents.y1 = (s->height - maxY - bs->filterRadius);
    region.extents.x2 = maxX + bs->filterRadius + 0.5f;
    region.extents.y2 = (s->height - minY + bs->filterRadius + 0.5f);

    region.rects    = &region.extents;
    region.numRects = 1;

    XIntersectRegion (&region, bs->region, bs->tmpRegion);

    pBox = bs->tmpRegion->rects;
    nBox = bs->tmpRegion->numRects;

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

	if (bs->filter == BlurFilterGaussian)
	{
	    if (s->fbo && !bs->fbo)
		(*s->genFramebuffers) (1, &bs->fbo);

	    if (!bs->fbo)
		fprintf (stderr,
			 "%s: blur: failed to create framebuffer object\n",
			 programName);

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

	    if (bs->filter == BlurFilterMipmap)
	    {
		if (!s->fbo)
		{
		    fprintf (stderr,
			     "%s: blur: GL_EXT_framebuffer_object extension "
			     "is required for mipmap filter\n",
			     programName);
		}
		else if (bs->target != GL_TEXTURE_2D)
		{
		    fprintf (stderr,
			     "%s: blur: GL_ARB_texture_non_power_of_two "
			     "extension is required for mipmap filter\n",
			     programName);
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

    switch (bs->filter) {
    case BlurFilterGaussian:
	return fboUpdate (s, bs->tmpRegion->rects, bs->tmpRegion->numRects);
    case BlurFilterMipmap:
	(*s->generateMipmap) (bs->target);
	break;
    case BlurFilter4xBilinear:
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

    if (bs->alphaBlur && bw->region && !(mask & PAINT_WINDOW_CLIP_OPAQUE_MASK))
    {
	int clientThreshold;

	/* only care about client window blurring when it's translucent */
	if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	    clientThreshold = bw->state[BLUR_STATE_CLIENT].threshold;
	else
	    clientThreshold = 0;

	if (bw->state[BLUR_STATE_DECOR].threshold || clientThreshold)
	{
	    Region reg;

	    if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
		reg = &infiniteRegion;
	    else
		reg = region;

	    w->vCount = w->indexCount = 0;
	    (*w->screen->addWindowGeometry) (w, NULL, 0, bw->region, reg);
	    if (w->vCount)
	    {
		Bool   clipped = FALSE;
		BoxRec box;

		if (blurUpdateDstTexture (w, transform, &box))
		{
		    if (bw->state[BLUR_STATE_CLIENT].threshold)
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
		}

		if (clipped)
		{
		    BoxRec clearBox = bs->stencilBox;

		    bs->stencilBox = box;

		    glPushAttrib (GL_STENCIL_BUFFER_BIT);
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

		    glPopAttrib ();
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

	    switch (bs->filter) {
	    case BlurFilter4xBilinear:
		dx = bs->tx / 2.1f;
		dy = bs->ty / 2.1f;

		param = allocFragmentParameters (&dstFa, 3);
		unit  = allocFragmentTextureUnits (&dstFa, 1);

		function = getDstBlurFragmentFunction (s, texture, param, unit);
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
	    case BlurFilterGaussian:
		param = allocFragmentParameters (&dstFa, 5);
		unit  = allocFragmentTextureUnits (&dstFa, 2);

		function = getDstBlurFragmentFunction (s, texture, param, unit);
		if (function)
		{
		    float amp[BLUR_GAUSSIAN_RADIUS_MAX];
		    float pos[BLUR_GAUSSIAN_RADIUS_MAX];
		    int   numTexop;
		    int   radius =
			bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS].value.i;
		    float strength =
			bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH].value.f;
		    int   i;

		    blurCreateGaussianLinearKernel (radius, strength, amp,
						    pos, &numTexop);

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


		    for (i = 0; i < numTexop; i++)
			(*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
						     param + 2 + i,
						     0.0f, bs->ty * pos[i],
						     0.0f, 0.0f);
		}
		break;
	    case BlurFilterMipmap:
		param = allocFragmentParameters (&dstFa, 2);
		unit  = allocFragmentTextureUnits (&dstFa, 1);

		function = getDstBlurFragmentFunction (s, texture, param, unit);
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

	    if (bw->state[state].clipped)
	    {
		glPushAttrib (GL_STENCIL_BUFFER_BIT);
		glEnable (GL_STENCIL_TEST);
		glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);

		glStencilFunc (GL_EQUAL, 0, ~0);

		/* draw region without destination blur */
		UNWRAP (bs, s, drawWindowTexture);
		(*s->drawWindowTexture) (w, texture, &fa, mask);

		glStencilFunc (GL_EQUAL, 0x1, ~0);

		/* draw region with destination blur */
		(*s->drawWindowTexture) (w, texture, &dstFa, mask);
		WRAP (bs, s, drawWindowTexture, blurDrawWindowTexture);

		glPopAttrib ();
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
    Window activeWindow = 0;

    BLUR_DISPLAY (d);

    if (event->type == PropertyNotify &&
	event->xproperty.atom == d->winActiveAtom)
	activeWindow = d->activeWindow;

    UNWRAP (bd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (bd, d, handleEvent, blurHandleEvent);

    if (event->type == PropertyNotify)
    {
	if (event->xproperty.atom == d->winActiveAtom)
	{
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
	}
	else
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
}

static void
blurWindowResizeNotify (CompWindow *w)
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
    (*w->screen->windowResizeNotify) (w);
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
blurGetDisplayOptions (CompDisplay *display,
		       int	   *count)
{
    BLUR_DISPLAY (display);

    *count = NUM_OPTIONS (bd);
    return bd->opt;
}

static Bool
blurSetDisplayOption (CompDisplay     *display,
		      char	      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    BLUR_DISPLAY (display);

    o = compFindOption (bd->opt, NUM_OPTIONS (bd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case BLUR_DISPLAY_OPTION_PULSE:
	if (setDisplayAction (display, o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
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
    if (w)
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
blurDisplayInitOptions (BlurDisplay *bd)
{
    CompOption *o;

    o = &bd->opt[BLUR_DISPLAY_OPTION_PULSE];
    o->name		      = "pulse";
    o->shortDesc	      = N_("Pulse");
    o->longDesc		      = N_("Pulse effect");
    o->type		      = CompOptionTypeAction;
    o->value.action.initiate  = blurPulse;
    o->value.action.terminate = 0;
    o->value.action.bell      = BLUR_PULSE_DEFAULT;
    o->value.action.edgeMask  = 0;
    o->value.action.type      = CompBindingTypeNone;
    o->value.action.state     = CompActionStateInitBell;
}

static Bool
blurInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    BlurDisplay *bd;

    bd = malloc (sizeof (BlurDisplay));
    if (!bd)
	return FALSE;

    bd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (bd->screenPrivateIndex < 0)
    {
	free (bd);
	return FALSE;
    }

    bd->blurAtom[BLUR_STATE_CLIENT] =
	XInternAtom (d->display, "_COMPIZ_WM_WINDOW_BLUR", 0);
    bd->blurAtom[BLUR_STATE_DECOR] =
	XInternAtom (d->display, "_COMPIZ_WM_WINDOW_BLUR_DECOR", 0);

    WRAP (bd, d, handleEvent, blurHandleEvent);

    blurDisplayInitOptions (bd);

    d->privates[displayPrivateIndex].ptr = bd;

    return TRUE;
}

static void
blurFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    BLUR_DISPLAY (d);

    freeScreenPrivateIndex (d, bd->screenPrivateIndex);

    UNWRAP (bd, d, handleEvent);

    free (bd);
}

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

    bs->region = XCreateRegion ();
    if (!bs->region)
    {
	free (bs);
	return FALSE;
    }

    bs->tmpRegion = XCreateRegion ();
    if (!bs->tmpRegion)
    {
	XDestroyRegion (bs->region);
	free (bs);
	return FALSE;
    }

    bs->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (bs->windowPrivateIndex < 0)
    {
	XDestroyRegion (bs->region);
	XDestroyRegion (bs->tmpRegion);
	free (bs);
	return FALSE;
    }

    bs->output = 0;
    bs->count  = 0;

    bs->filterRadius = 0;

    bs->srcBlurFunctions = NULL;
    bs->dstBlurFunctions = NULL;
    bs->blurTime	 = 1000.0f / BLUR_SPEED_DEFAULT;
    bs->moreBlur	 = FALSE;

    for (i = 0; i < 2; i++)
	bs->texture[i] = 0;

    bs->program   = 0;
    bs->fbo	  = 0;
    bs->fboStatus = FALSE;

    glGetIntegerv (GL_STENCIL_BITS, &bs->stencilBits);
    if (!bs->stencilBits)
	fprintf (stderr, "%s: No stencil buffer. Region based blur disabled\n",
		 programName);

    blurScreenInitOptions (bs);

    /* We need GL_ARB_fragment_program for blur */
    if (!s->fragmentProgram)
    {
	bs->alphaBlur = FALSE;
	bs->wMask     = 0;
    }

    WRAP (bs, s, preparePaintScreen, blurPreparePaintScreen);
    WRAP (bs, s, donePaintScreen, blurDonePaintScreen);
    WRAP (bs, s, paintScreen, blurPaintScreen);
    WRAP (bs, s, drawWindow, blurDrawWindow);
    WRAP (bs, s, drawWindowTexture, blurDrawWindowTexture);
    WRAP (bs, s, windowResizeNotify, blurWindowResizeNotify);
    WRAP (bs, s, windowMoveNotify, blurWindowMoveNotify);

    s->privates[bd->screenPrivateIndex].ptr = bs;

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

    if (bs->fbo)
	(*s->deleteFramebuffers) (1, &bs->fbo);

    for (i = 0; i < 2; i++)
	if (bs->texture[i])
	    glDeleteTextures (1, &bs->texture[i]);

    freeWindowPrivateIndex (s, bs->windowPrivateIndex);

    UNWRAP (bs, s, preparePaintScreen);
    UNWRAP (bs, s, donePaintScreen);
    UNWRAP (bs, s, paintScreen);
    UNWRAP (bs, s, drawWindow);
    UNWRAP (bs, s, drawWindowTexture);
    UNWRAP (bs, s, windowResizeNotify);
    UNWRAP (bs, s, windowMoveNotify);

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

    bw->blur  = 0;
    bw->pulse = FALSE;

    for (i = 0; i < BLUR_STATE_NUM; i++)
    {
	bw->state[i].threshold = 0;
	bw->state[i].box       = NULL;
	bw->state[i].nBox      = 0;
	bw->state[i].clipped   = FALSE;
	bw->state[i].active    = FALSE;
    }

    bw->region = NULL;

    w->privates[bs->windowPrivateIndex].ptr = bw;

    blurWindowUpdate (w, BLUR_STATE_CLIENT);
    blurWindowUpdate (w, BLUR_STATE_DECOR);

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

    free (bw);
}

static Bool
blurInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
blurFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
blurGetVersion (CompPlugin *plugin,
		int	   version)
{
    return ABIVERSION;
}

static CompPluginVTable blurVTable = {
    "blur",
    N_("Blur Windows"),
    N_("Blur windows"),
    blurGetVersion,
    blurInit,
    blurFini,
    blurInitDisplay,
    blurFiniDisplay,
    blurInitScreen,
    blurFiniScreen,
    blurInitWindow,
    blurFiniWindow,
    blurGetDisplayOptions,
    blurSetDisplayOption,
    blurGetScreenOptions,
    blurSetScreenOption,
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &blurVTable;
}
