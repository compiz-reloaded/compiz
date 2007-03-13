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

#define BLUR_SATURATION_DEFAULT 100
#define BLUR_SATURATION_MIN       0
#define BLUR_SATURATION_MAX     100

#define BLUR_FOCUS_BLUR_MATCH_DEFAULT \
    "Toolbar | Menu | Utility | Normal | Dialog | ModalDialog"

#define BLUR_ALPHA_BLUR_MATCH_DEFAULT ""

#define BLUR_BLUR_OCCLUSION_DEFAULT TRUE

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
#define BLUR_SCREEN_OPTION_NUM		     11

typedef struct _BlurScreen {
    int	windowPrivateIndex;

    CompOption opt[BLUR_SCREEN_OPTION_NUM];

    PreparePaintScreenProc       preparePaintScreen;
    DonePaintScreenProc          donePaintScreen;
    PaintScreenProc	         paintScreen;
    PaintTransformedScreenProc	 paintTransformedScreen;
    PaintWindowProc	         paintWindow;
    DrawWindowProc	         drawWindow;
    DrawWindowTextureProc        drawWindowTexture;

    WindowResizeNotifyProc windowResizeNotify;
    WindowMoveNotifyProc   windowMoveNotify;

    Bool alphaBlur;

    int	 blurTime;
    Bool moreBlur;

    Bool blurOcclusion;

    BlurFilter filter;
    int        filterRadius;

    BlurFunction *srcBlurFunctions;
    BlurFunction *dstBlurFunctions;

    Region region;
    Region tmpRegion;
    Region tmpRegion2;
    Region tmpRegion3;
    Region occlusion;

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

    float amp[BLUR_GAUSSIAN_RADIUS_MAX];
    float pos[BLUR_GAUSSIAN_RADIUS_MAX];
    int	  numTexop;
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

    switch (bs->filter) {
    case BlurFilter4xBilinear:
	bs->filterRadius = 2;
	break;
    case BlurFilterGaussian: {
	int   radius   = bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_RADIUS].value.i;
	float strength = bs->opt[BLUR_SCREEN_OPTION_GAUSSIAN_STRENGTH].value.f;

	blurCreateGaussianLinearKernel (radius, strength, bs->amp, bs->pos,
					&bs->numTexop);

	bs->filterRadius = radius;
    } break;
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
	break;

    default:
	break;
    }

    return FALSE;
}

static void
blurScreenInitOptions (BlurScreen *bs)
{
    CompOption *o;

    o = &bs->opt[BLUR_SCREEN_OPTION_BLUR_SPEED];
    o->name		= "blur_speed";
    o->shortDesc	= N_("Blur Speed");
    o->longDesc		= N_("Window blur speed");
    o->type		= CompOptionTypeFloat;
    o->value.f		= BLUR_SPEED_DEFAULT;
    o->rest.f.min	= BLUR_SPEED_MIN;
    o->rest.f.max	= BLUR_SPEED_MAX;
    o->rest.f.precision = BLUR_SPEED_PRECISION;

    o = &bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR_MATCH];
    o->name	 = "focus_blur_match";
    o->shortDesc = N_("Focus blur windows");
    o->longDesc	 = N_("Windows that should be affected by focus blur");
    o->type	 = CompOptionTypeMatch;

    matchInit (&o->value.match);
    matchAddFromString (&o->value.match, BLUR_FOCUS_BLUR_MATCH_DEFAULT);

    o = &bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR];
    o->name	 = "focus_blur";
    o->shortDesc = N_("Focus Blur");
    o->longDesc	 = N_("Blur windows that doesn't have focus");
    o->type	 = CompOptionTypeBool;
    o->value.b   = BLUR_FOCUS_BLUR_DEFAULT;

    o = &bs->opt[BLUR_SCREEN_OPTION_ALPHA_BLUR_MATCH];
    o->name	 = "alpha_blur_match";
    o->shortDesc = N_("Alpha blur windows");
    o->longDesc	 = N_("Windows that should use alpha blur by default");
    o->type	 = CompOptionTypeMatch;

    matchInit (&o->value.match);
    matchAddFromString (&o->value.match, BLUR_ALPHA_BLUR_MATCH_DEFAULT);

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

    o = &bs->opt[BLUR_SCREEN_OPTION_SATURATION];
    o->name	  = "saturation";
    o->shortDesc  = N_("Blur Saturation");
    o->longDesc	  = N_("Blur saturation");
    o->type	  = CompOptionTypeInt;
    o->value.i	  = BLUR_SATURATION_DEFAULT;
    o->rest.i.min = BLUR_SATURATION_MIN;
    o->rest.i.max = BLUR_SATURATION_MAX;

    o = &bs->opt[BLUR_SCREEN_OPTION_BLUR_OCCLUSION];
    o->name	  = "occlusion";
    o->shortDesc  = N_("Blur Occlusion");
    o->longDesc	  = N_("blur occlusion");
    o->type	  = CompOptionTypeBool;
    o->value.b	  = BLUR_BLUR_OCCLUSION_DEFAULT;
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

    if (result == Success && n && propData)
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

    if (!bs->blurOcclusion)
    {
	CompWindow *w;

	XSubtractRegion (&emptyRegion, &emptyRegion, bs->occlusion);

	for (w = s->windows; w; w = w->next)
	    XSubtractRegion (&emptyRegion, &emptyRegion,
			     GET_BLUR_WINDOW (w, bs)->clip);
    }

    bs->output = output;

    UNWRAP (bs, s, paintScreen);
    status = (*s->paintScreen) (s, sAttrib, transform, region, output, mask);
    WRAP (bs, s, paintScreen, blurPaintScreen);

    return status;
}

static void
blurPaintTransformedScreen (CompScreen		    *s,
			    const ScreenPaintAttrib *sAttrib,
			    const CompTransform	    *transform,
			    Region		    region,
			    int			    output,
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

    UNWRAP (bs, s, paintTransformedScreen);
    (*s->paintTransformedScreen) (s, sAttrib, transform,
				   region, output, mask);
    WRAP (bs, s, paintTransformedScreen, blurPaintTransformedScreen);
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
	int	    saturation = bs->opt[BLUR_SCREEN_OPTION_SATURATION].value.i;
	Bool	    ok = TRUE;

	for (i = 0; i < sizeof (temp) / sizeof (temp[0]); i++)
	    ok &= addTempHeaderOpToFunctionData (data, temp[i]);

	if (saturation < 100)
	    ok &= addTempHeaderOpToFunctionData (data, "sat");

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

		      "MAD t2, program.env[%d], { -1.0, 1.0, 0.0, 0.0 }, coord;"
		      "TEX s2, t2, texture[%d], %s;"

		      "MAD t3, program.env[%d], { 1.0, -1.0, 0.0, 0.0 }, coord;"
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
		      unit, targetString, param + 1, bs->amp[bs->numTexop]);

	    ok &= addDataOpToFunctionData (data, str);

	    for (i = 0; i < bs->numTexop; i++)
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
			  bs->amp[i],
			  param + 2 + i,
			  unit + 1, targetString,
			  bs->amp[i]);

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
    int   i;

    BLUR_SCREEN (s);

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
		    bs->amp[bs->numTexop]);

    for (i = 0; i < bs->numTexop; i++)
	str += sprintf (str,
			"ADD tCoord, texcoord, program.local[%d];"
			"TEX pix, tCoord, texture[0], %s;"
			"MAD sum, pix, %f, sum;"
			"SUB tCoord, texcoord, program.local[%d];"
			"TEX pix, tCoord, texture[0], %s;"
			"MAD sum, pix, %f, sum;",
			i, targetString, bs->amp[i],
			i, targetString, bs->amp[i]);

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
    int i, y;

    BLUR_SCREEN (s);

    if (!bs->program)
	if (!loadFilterProgram (s))
	    return FALSE;

    if (!fboPrologue (s))
	return FALSE;

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);

    glBindTexture (bs->target, bs->texture[0]);

    glEnable (GL_FRAGMENT_PROGRAM_ARB);
    (*s->bindProgram) (GL_FRAGMENT_PROGRAM_ARB, bs->program);

    for (i = 0; i < bs->numTexop; i++)
	(*s->programLocalParameter4f) (GL_FRAGMENT_PROGRAM_ARB, i,
				       bs->tx * bs->pos[i],
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

#define MAX_VERTEX_PROJECT_COUNT 20

static void
blurProjectRegion (CompWindow	       *w,
		   int		       output,
		   const CompTransform *transform)
{
    CompScreen *s = w->screen;
    float      screen[MAX_VERTEX_PROJECT_COUNT * 2];
    float      vertices[MAX_VERTEX_PROJECT_COUNT * 2];
    int	       nVertices;
    int        i, j, stride;
    float      *v, *vert;
    float      minX, maxX, minY, maxY;
    float      *scr;
    REGION     region;

    BLUR_SCREEN(s);

    w->vCount = w->indexCount = 0;
    (*w->screen->addWindowGeometry) (w, NULL, 0, bs->tmpRegion2,
				     &infiniteRegion);

    if (!w->vCount)
	return;

    nVertices = (w->indexCount) ? w->indexCount: w->vCount;

    stride = (1 + w->texUnits) * 2;
    vert = w->vertices + (stride - 2);

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

	    vertices[i * 2] = v[0];
	    vertices[(i * 2) + 1] = v[1];
	}
    }
    else
    {
	minX = s->width;
	maxX = 0;
	minY = s->height;
	maxY = 0;

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
	}

	vertices[0] = vertices[6] = minX;
	vertices[1] = vertices[3] = minY;
	vertices[2] = vertices[4] = maxX;
	vertices[5] = vertices[7] = maxY;

	nVertices = 4;
    }

    if (!projectVertices (w->screen, output, transform, vertices, screen,
			  nVertices))
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

	for (j = 0; j < 8; j += 2)
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

    BLUR_SCREEN (s);
    BLUR_WINDOW (w);

    /* create empty region */
    XSubtractRegion (&emptyRegion, &emptyRegion, bs->tmpRegion3);

    if (bs->filter == BlurFilterGaussian)
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
		    int i;

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


		    for (i = 0; i < bs->numTexop; i++)
			(*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB,
						     param + 2 + i,
						     0.0f, bs->ty * bs->pos[i],
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

	    if (bw->state[state].clipped ||
		(!bs->blurOcclusion && bw->clip->numRects))
	    {
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
    WRAP (bd, d, matchExpHandlerChanged, blurMatchExpHandlerChanged);
    WRAP (bd, d, matchPropertyChanged, blurMatchPropertyChanged);

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
    UNWRAP (bd, d, matchExpHandlerChanged);
    UNWRAP (bd, d, matchPropertyChanged);

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

    bs->tmpRegion2 = XCreateRegion ();
    if (!bs->tmpRegion2)
    {
	XDestroyRegion (bs->region);
	XDestroyRegion (bs->tmpRegion);
	free (bs);
	return FALSE;
    }

    bs->tmpRegion3 = XCreateRegion ();
    if (!bs->tmpRegion3)
    {
	XDestroyRegion (bs->region);
	XDestroyRegion (bs->tmpRegion);
	XDestroyRegion (bs->tmpRegion2);
	free (bs);
	return FALSE;
    }

    bs->occlusion = XCreateRegion ();
    if (!bs->occlusion)
    {
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
	XDestroyRegion (bs->region);
	XDestroyRegion (bs->tmpRegion);
	XDestroyRegion (bs->tmpRegion2);
	XDestroyRegion (bs->tmpRegion3);
	XDestroyRegion (bs->occlusion);
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
    bs->blurOcclusion    = BLUR_BLUR_OCCLUSION_DEFAULT;

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

    matchUpdate (s->display,
		 &bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR_MATCH].value.match);
    matchUpdate (s->display,
		 &bs->opt[BLUR_SCREEN_OPTION_ALPHA_BLUR_MATCH].value.match);

    /* We need GL_ARB_fragment_program for blur */
    if (!s->fragmentProgram)
	bs->alphaBlur = FALSE;

    WRAP (bs, s, preparePaintScreen, blurPreparePaintScreen);
    WRAP (bs, s, donePaintScreen, blurDonePaintScreen);
    WRAP (bs, s, paintScreen, blurPaintScreen);
    WRAP (bs, s, paintTransformedScreen, blurPaintTransformedScreen);
    WRAP (bs, s, paintWindow, blurPaintWindow);
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
    XDestroyRegion (bs->tmpRegion2);
    XDestroyRegion (bs->tmpRegion3);
    XDestroyRegion (bs->occlusion);

    if (bs->fbo)
	(*s->deleteFramebuffers) (1, &bs->fbo);

    for (i = 0; i < 2; i++)
	if (bs->texture[i])
	    glDeleteTextures (1, &bs->texture[i]);

    freeWindowPrivateIndex (s, bs->windowPrivateIndex);

    matchFini (&bs->opt[BLUR_SCREEN_OPTION_FOCUS_BLUR_MATCH].value.match);
    matchFini (&bs->opt[BLUR_SCREEN_OPTION_ALPHA_BLUR_MATCH].value.match);

    UNWRAP (bs, s, preparePaintScreen);
    UNWRAP (bs, s, donePaintScreen);
    UNWRAP (bs, s, paintScreen);
    UNWRAP (bs, s, paintTransformedScreen);
    UNWRAP (bs, s, paintWindow);
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

    bw->clip = XCreateRegion();
    if (!bw->clip)
	return FALSE;

    w->privates[bs->windowPrivateIndex].ptr = bw;

    blurWindowUpdate (w, BLUR_STATE_CLIENT);
    blurWindowUpdate (w, BLUR_STATE_DECOR);

    blurUpdateWindowMatch (bs, w);

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

    XDestroyRegion(bw->clip);

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

CompPluginDep blurDeps[] = {
    { CompPluginRuleBefore, "video" }
};

CompPluginFeature blurFeatures[] = {
    { "blur" }
};

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
    blurDeps,
    sizeof (blurDeps) / sizeof (blurDeps[0]),
    blurFeatures,
    sizeof (blurFeatures) / sizeof (blurFeatures[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &blurVTable;
}
