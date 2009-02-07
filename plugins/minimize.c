/*
 * Copyright © 2005 Novell, Inc.
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

#include <X11/Xatom.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <compiz-core.h>

static CompMetadata minMetadata;

static int displayPrivateIndex;

typedef struct _MinDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
    Atom	    winChangeStateAtom;
} MinDisplay;

#define MIN_SCREEN_OPTION_SPEED		   0
#define MIN_SCREEN_OPTION_TIMESTEP	   1
#define MIN_SCREEN_OPTION_WINDOW_MATCH	   2
#define MIN_SCREEN_OPTION_SHADE_RESISTANCE 3
#define MIN_SCREEN_OPTION_NUM		   4

typedef struct _MinScreen {
    int	windowPrivateIndex;

    CompOption opt[MIN_SCREEN_OPTION_NUM];

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintOutputProc        paintOutput;
    PaintWindowProc        paintWindow;
    DamageWindowRectProc   damageWindowRect;
    FocusWindowProc	   focusWindow;

    int shadeStep;
    int moreAdjust;
} MinScreen;

typedef struct _MinWindow {
    GLfloat xVelocity, yVelocity, xScaleVelocity, yScaleVelocity;
    GLfloat xScale, yScale;
    GLfloat tx, ty;

    Bool adjust;

    int state, newState;

    int    shade;
    Region region;

    int unmapCnt;

    Bool ignoreDamage;
} MinWindow;

#define GET_MIN_DISPLAY(d)					 \
    ((MinDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define MIN_DISPLAY(d)			 \
    MinDisplay *md = GET_MIN_DISPLAY (d)

#define GET_MIN_SCREEN(s, md)					     \
    ((MinScreen *) (s)->base.privates[(md)->screenPrivateIndex].ptr)

#define MIN_SCREEN(s)						     \
    MinScreen *ms = GET_MIN_SCREEN (s, GET_MIN_DISPLAY (s->display))

#define GET_MIN_WINDOW(w, ms)					     \
    ((MinWindow *) (w)->base.privates[(ms)->windowPrivateIndex].ptr)

#define MIN_WINDOW(w)					   \
    MinWindow *mw = GET_MIN_WINDOW  (w,			   \
		    GET_MIN_SCREEN  (w->screen,		   \
		    GET_MIN_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
minGetScreenOptions (CompPlugin *plugin,
		     CompScreen *screen,
		     int	*count)
{
    MIN_SCREEN (screen);

    *count = NUM_OPTIONS (ms);
    return ms->opt;
}

static Bool
minSetScreenOption (CompPlugin      *plugin,
		    CompScreen      *screen,
		    const char	    *name,
		    CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    MIN_SCREEN (screen);

    o = compFindOption (ms->opt, NUM_OPTIONS (ms), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case MIN_SCREEN_OPTION_SHADE_RESISTANCE:
	if (compSetIntOption (o, value))
	{
	    if (o->value.i)
		ms->shadeStep = o->rest.i.max - o->value.i + 1;
	    else
		ms->shadeStep = 0;

	    return TRUE;
	}
	break;
    default:
	if (compSetOption (o, value))
	    return TRUE;
	break;
    }

    return FALSE;
}

static void
minSetShade (CompWindow *w,
	     int	shade)
{
    REGION rect;
    int	   h = w->attrib.height + w->attrib.border_width * 2;

    MIN_WINDOW (w);

    EMPTY_REGION (w->region);

    rect.rects = &rect.extents;
    rect.numRects = rect.size = 1;

    w->height = shade;

    rect.extents.x1 = 0;
    rect.extents.y1 = h - shade;
    rect.extents.x2 = w->width;
    rect.extents.y2 = h;

    XIntersectRegion (mw->region, &rect, w->region);
    XOffsetRegion (w->region, w->attrib.x, w->attrib.y - (h - shade));

    w->matrix = w->texture->matrix;
    w->matrix.x0 -= (w->attrib.x * w->matrix.xx);
    w->matrix.y0 -= ((w->attrib.y - (h - shade)) * w->matrix.yy);

    (*w->screen->windowResizeNotify) (w, 0, 0, 0, 0);
}

static int
minGetWindowState (CompWindow *w)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    int           retval = WithdrawnState;

    result = XGetWindowProperty (w->screen->display->display, w->id,
				 w->screen->display->wmStateAtom, 0L, 1L, FALSE,
				 w->screen->display->wmStateAtom,
				 &actual, &format, &n, &left, &data);

    if (result == Success && data)
    {
	if (n)
	    memcpy (&retval, data, sizeof (int));

	XFree ((void *) data);
    }

    return retval;
}

static int
adjustMinVelocity (CompWindow *w)
{
    float dx, dy, dxs, dys, adjust, amount;
    float x1, y1, xScale, yScale;

    MIN_WINDOW (w);

    if (mw->newState == IconicState)
    {
	x1 = w->iconGeometry.x;
	y1 = w->iconGeometry.y;
	xScale = (float) w->iconGeometry.width  / w->width;
	yScale = (float) w->iconGeometry.height / w->height;
    }
    else
    {
	x1 = w->serverX;
	y1 = w->serverY;
	xScale = yScale = 1.0f;
    }

    dx = x1 - (w->attrib.x + mw->tx);

    adjust = dx * 0.15f;
    amount = fabs (dx) * 1.5f;
    if (amount < 0.5f)
	amount = 0.5f;
    else if (amount > 5.0f)
	amount = 5.0f;

    mw->xVelocity = (amount * mw->xVelocity + adjust) / (amount + 1.0f);

    dy = y1 - (w->attrib.y + mw->ty);

    adjust = dy * 0.15f;
    amount = fabs (dy) * 1.5f;
    if (amount < 0.5f)
	amount = 0.5f;
    else if (amount > 5.0f)
	amount = 5.0f;

    mw->yVelocity = (amount * mw->yVelocity + adjust) / (amount + 1.0f);

    dxs = xScale - mw->xScale;

    adjust = dxs * 0.15f;
    amount = fabs (dxs) * 10.0f;
    if (amount < 0.01f)
	amount = 0.01f;
    else if (amount > 0.15f)
	amount = 0.15f;

    mw->xScaleVelocity = (amount * mw->xScaleVelocity + adjust) /
	(amount + 1.0f);

    dys = yScale - mw->yScale;

    adjust = dys * 0.15f;
    amount = fabs (dys) * 10.0f;
    if (amount < 0.01f)
	amount = 0.01f;
    else if (amount > 0.15f)
	amount = 0.15f;

    mw->yScaleVelocity = (amount * mw->yScaleVelocity + adjust) /
	(amount + 1.0f);

    if (fabs (dx) < 0.1f    && fabs (mw->xVelocity)      < 0.2f   &&
	fabs (dy) < 0.1f    && fabs (mw->yVelocity)      < 0.2f   &&
	fabs (dxs) < 0.001f && fabs (mw->xScaleVelocity) < 0.002f &&
	fabs (dys) < 0.001f && fabs (mw->yScaleVelocity) < 0.002f)
    {
	mw->xVelocity = mw->yVelocity = mw->xScaleVelocity =
	    mw->yScaleVelocity = 0.0f;
	mw->tx = x1 - w->attrib.x;
	mw->ty = y1 - w->attrib.y;
	mw->xScale = xScale;
	mw->yScale = yScale;

	return 0;
    }

    return 1;
}

static void
minPreparePaintScreen (CompScreen *s,
		       int	  msSinceLastPaint)
{
    MIN_SCREEN (s);

    if (ms->moreAdjust)
    {
	CompWindow *w;
	int        steps, h;
	float      amount, chunk;

	amount = msSinceLastPaint * 0.05f *
	    ms->opt[MIN_SCREEN_OPTION_SPEED].value.f;
	steps  = amount / (0.5f * ms->opt[MIN_SCREEN_OPTION_TIMESTEP].value.f);
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    ms->moreAdjust = 0;

	    for (w = s->windows; w; w = w->next)
	    {
		MIN_WINDOW (w);

		if (mw->adjust)
		{
		    mw->adjust = adjustMinVelocity (w);

		    ms->moreAdjust |= mw->adjust;

		    mw->tx += mw->xVelocity * chunk;
		    mw->ty += mw->yVelocity * chunk;
		    mw->xScale += mw->xScaleVelocity * chunk;
		    mw->yScale += mw->yScaleVelocity * chunk;

		    if (!mw->adjust)
		    {
			mw->state = mw->newState;

			mw->ignoreDamage = TRUE;
			while (mw->unmapCnt)
			{
			    unmapWindow (w);
			    mw->unmapCnt--;
			}
			mw->ignoreDamage = FALSE;
		    }
		}
		else if (mw->region && w->damaged)
		{
		    if (w->shaded)
		    {
			if (mw->shade > 0)
			{
			    mw->shade -= (chunk * ms->shadeStep) + 1;

			    if (mw->shade > 0)
			    {
				ms->moreAdjust = TRUE;
			    }
			    else
			    {
				mw->shade = 0;

				mw->ignoreDamage = TRUE;
				while (mw->unmapCnt)
				{
				    unmapWindow (w);
				    mw->unmapCnt--;
				}
				mw->ignoreDamage = FALSE;
			    }
			}
		    }
		    else
		    {
			h = w->attrib.height + w->attrib.border_width * 2;
			if (mw->shade < h)
			{
			    mw->shade += (chunk * ms->shadeStep) + 1;

			    if (mw->shade < h)
			    {
				ms->moreAdjust = TRUE;
			    }
			    else
			    {
				mw->shade = MAXSHORT;

				minSetShade (w, h);

				XDestroyRegion (mw->region);
				mw->region = NULL;

				addWindowDamage (w);
			    }
			}
		    }
		}
	    }

	    if (!ms->moreAdjust)
		break;
	}

	if (ms->moreAdjust)
	{
	    for (w = s->windows; w; w = w->next)
	    {
		MIN_WINDOW (w);

		if (mw->adjust)
		{
		    addWindowDamage (w);
		}
		else if (mw->region && w->damaged)
		{
		    h = w->attrib.height + w->attrib.border_width * 2;
		    if (mw->shade && mw->shade < h)
		    {
			minSetShade (w, mw->shade);
			addWindowDamage (w);
		    }
		}
	    }
	}
    }

    UNWRAP (ms, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (ms, s, preparePaintScreen, minPreparePaintScreen);
}

static void
minDonePaintScreen (CompScreen *s)
{
    MIN_SCREEN (s);

    if (ms->moreAdjust)
    {
	CompWindow *w;
	int	   h;

	for (w = s->windows; w; w = w->next)
	{
	    MIN_WINDOW (w);

	    if (mw->adjust)
	    {
		addWindowDamage (w);
	    }
	    else if (mw->region)
	    {
		h = w->attrib.height + w->attrib.border_width * 2;
		if (mw->shade && mw->shade < h)
		    addWindowDamage (w);
	    }
	}
    }

    UNWRAP (ms, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ms, s, donePaintScreen, minDonePaintScreen);
}

static Bool
minPaintOutput (CompScreen		*s,
		const ScreenPaintAttrib *sAttrib,
		const CompTransform	*transform,
		Region		        region,
		CompOutput		*output,
		unsigned int		mask)
{
    Bool status;

    MIN_SCREEN (s);

    if (ms->moreAdjust)
	mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

    UNWRAP (ms, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (ms, s, paintOutput, minPaintOutput);

    return status;
}

static Bool
minPaintWindow (CompWindow		*w,
		const WindowPaintAttrib *attrib,
		const CompTransform	*transform,
		Region			region,
		unsigned int		mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    MIN_SCREEN (s);
    MIN_WINDOW (w);

    if (mw->adjust)
    {
	FragmentAttrib fragment;
	CompTransform  wTransform = *transform;

	if (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK)
	    return FALSE;

	UNWRAP (ms, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, transform, region,
				    mask | PAINT_WINDOW_NO_CORE_INSTANCE_MASK);
	WRAP (ms, s, paintWindow, minPaintWindow);

	initFragmentAttrib (&fragment, &w->lastPaint);

	if (w->alpha || fragment.opacity != OPAQUE)
	    mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

	matrixTranslate (&wTransform, w->attrib.x, w->attrib.y, 0.0f);
	matrixScale (&wTransform, mw->xScale, mw->yScale, 1.0f);
	matrixTranslate (&wTransform,
			 mw->tx / mw->xScale - w->attrib.x,
			 mw->ty / mw->yScale - w->attrib.y,
			 0.0f);

	glPushMatrix ();
	glLoadMatrixf (wTransform.m);

	(*s->drawWindow) (w, &wTransform, &fragment, region,
			  mask | PAINT_WINDOW_TRANSFORMED_MASK);

	glPopMatrix ();
    }
    else
    {
	/* no core instance from windows that have been animated */
	if (mw->state == IconicState)
	    mask |= PAINT_WINDOW_NO_CORE_INSTANCE_MASK;

	UNWRAP (ms, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, transform, region, mask);
	WRAP (ms, s, paintWindow, minPaintWindow);
    }

    return status;
}

static void
minHandleEvent (CompDisplay *d,
		XEvent      *event)
{
    CompWindow *w;

    MIN_DISPLAY (d);

    switch (event->type) {
    case MapNotify:
	w = findWindowAtDisplay (d, event->xmap.window);
	if (w)
	{
	    MIN_WINDOW (w);

	    if (mw->adjust)
		mw->state = mw->newState;

	    if (mw->region)
		w->height = 0;

	    mw->ignoreDamage = TRUE;
	    while (mw->unmapCnt)
	    {
		unmapWindow (w);
		mw->unmapCnt--;
	    }
	    mw->ignoreDamage = FALSE;
	}
	break;
    case UnmapNotify:
	w = findWindowAtDisplay (d, event->xunmap.window);
	if (w)
	{
	    MIN_SCREEN (w->screen);

	    if (w->pendingUnmaps && onCurrentDesktop (w)) /* Normal -> Iconic */
	    {
		CompMatch *match =
		    &ms->opt[MIN_SCREEN_OPTION_WINDOW_MATCH].value.match;

		MIN_WINDOW (w);

		if (w->shaded)
		{
		    if (!mw->region)
			mw->region = XCreateRegion ();

		    if (mw->region && ms->shadeStep)
		    {
			XSubtractRegion (w->region, &emptyRegion, mw->region);
			XOffsetRegion (mw->region, -w->attrib.x,
				       w->attrib.height +
				       w->attrib.border_width * 2 -
				       w->height - w->attrib.y);

			mw->shade = w->height;

			mw->adjust     = FALSE;
			ms->moreAdjust = TRUE;

			mw->unmapCnt++;
			w->unmapRefCnt++;

			addWindowDamage (w);
		    }
		}
		else if (!w->invisible && matchEval (match, w))
		{
		    if (w->iconGeometrySet)
		    {
			mw->newState = IconicState;

			mw->xScale = w->paint.xScale;
			mw->yScale = w->paint.yScale;
			mw->tx	   = w->attrib.x - w->serverX;
			mw->ty	   = w->attrib.y - w->serverY;

			if (mw->region)
			{
			    XDestroyRegion (mw->region);
			    mw->region = NULL;
			}

			mw->shade = MAXSHORT;

			mw->adjust     = TRUE;
			ms->moreAdjust = TRUE;

			mw->unmapCnt++;
			w->unmapRefCnt++;

			addWindowDamage (w);
		    }
		}
	    }
	    else  /* X -> Withdrawn */
	    {
		MIN_WINDOW (w);

		if (mw->adjust)
		{
		    mw->adjust = FALSE;
		    mw->xScale = mw->yScale = 1.0f;
		    mw->tx = mw->ty = 0.0f;
		    mw->xVelocity = mw->yVelocity = 0.0f;
		    mw->xScaleVelocity = mw->yScaleVelocity = 1.0f;
		    mw->shade = MAXSHORT;

		    if (mw->region)
		    {
			XDestroyRegion (mw->region);
			mw->region = NULL;
		    }
		}

		mw->state = NormalState;
	    }
	}
    default:
	break;
    }

    UNWRAP (md, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (md, d, handleEvent, minHandleEvent);
}

static Bool
minDamageWindowRect (CompWindow *w,
		     Bool	initial,
		     BoxPtr     rect)
{
    Bool status = FALSE;

    MIN_SCREEN (w->screen);
    MIN_WINDOW (w);

    if (mw->ignoreDamage)
	return TRUE;

    if (initial)
    {
	if (mw->state == IconicState)
	{
	    CompMatch *match =
		&ms->opt[MIN_SCREEN_OPTION_WINDOW_MATCH].value.match;

	    mw->state = NormalState;

	    if (!w->invisible	   &&
		w->iconGeometrySet &&
		matchEval (match, w))
	    {
		if (!mw->adjust)
		{
		    mw->adjust     = TRUE;
		    ms->moreAdjust = TRUE;

		    mw->tx     = w->iconGeometry.x - w->serverX;
		    mw->ty     = w->iconGeometry.y - w->serverY;
		    mw->xScale = (float) w->iconGeometry.width  / w->width;
		    mw->yScale = (float) w->iconGeometry.height / w->height;

		    addWindowDamage (w);
		}
	    }
	}
	else if (mw->region && mw->shade < w->height)
	{
	    if (ms->shadeStep && !w->invisible)
	    {
		XSubtractRegion (w->region, &emptyRegion, mw->region);
		XOffsetRegion (mw->region, -w->attrib.x, -w->attrib.y);

		/* bind pixmap here so we have something to unshade with */
		if (!w->texture->pixmap && !w->bindFailed)
		    bindWindow (w);

		ms->moreAdjust = TRUE;
	    }
	    else
	    {
		mw->shade = MAXSHORT;
	    }
	}

	mw->newState = NormalState;
    }
    else if (mw->adjust)
    {
	damageTransformedWindowRect (w,
				     mw->xScale,
				     mw->yScale,
				     mw->tx,
				     mw->ty,
				     rect);

	status = TRUE;
    }

    UNWRAP (ms, w->screen, damageWindowRect);
    status |= (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ms, w->screen, damageWindowRect, minDamageWindowRect);

    return status;
}

static Bool
minFocusWindow (CompWindow *w)
{
    Bool status;

    MIN_SCREEN (w->screen);
    MIN_WINDOW (w);

    if (mw->unmapCnt)
	return FALSE;

    UNWRAP (ms, w->screen, focusWindow);
    status = (*w->screen->focusWindow) (w);
    WRAP (ms, w->screen, focusWindow, minFocusWindow);

    return status;
}

static Bool
minInitDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    MinDisplay *md;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    md = malloc (sizeof (MinDisplay));
    if (!md)
	return FALSE;

    md->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (md->screenPrivateIndex < 0)
    {
	free (md);
	return FALSE;
    }

    md->winChangeStateAtom  = XInternAtom (d->display, "WM_CHANGE_STATE", 0);

    WRAP (md, d, handleEvent, minHandleEvent);

    d->base.privates[displayPrivateIndex].ptr = md;

    return TRUE;
}

static void
minFiniDisplay (CompPlugin  *p,
		CompDisplay *d)
{
    MIN_DISPLAY (d);

    freeScreenPrivateIndex (d, md->screenPrivateIndex);

    UNWRAP (md, d, handleEvent);

    free (md);
}

static const CompMetadataOptionInfo minScreenOptionInfo[] = {
    { "speed", "float", "<min>0.1</min>", 0, 0 },
    { "timestep", "float", "<min>0.1</min>", 0, 0 },
    { "window_match", "match", 0, 0, 0 },
    { "shade_resistance", "int", "<min>0</min><max>100</max>", 0, 0 }
};

static Bool
minInitScreen (CompPlugin *p,
	       CompScreen *s)
{
    MinScreen *ms;

    MIN_DISPLAY (s->display);

    ms = malloc (sizeof (MinScreen));
    if (!ms)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &minMetadata,
					    minScreenOptionInfo,
					    ms->opt,
					    MIN_SCREEN_OPTION_NUM))
    {
	free (ms);
	return FALSE;
    }

    ms->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ms->windowPrivateIndex < 0)
    {
	compFiniScreenOptions (s, ms->opt, MIN_SCREEN_OPTION_NUM);
	free (ms);
	return FALSE;
    }

    ms->moreAdjust = FALSE;
    ms->shadeStep  = ms->opt[MIN_SCREEN_OPTION_SHADE_RESISTANCE].rest.i.max -
	ms->opt[MIN_SCREEN_OPTION_SHADE_RESISTANCE].value.i + 1;

    WRAP (ms, s, preparePaintScreen, minPreparePaintScreen);
    WRAP (ms, s, donePaintScreen, minDonePaintScreen);
    WRAP (ms, s, paintOutput, minPaintOutput);
    WRAP (ms, s, paintWindow, minPaintWindow);
    WRAP (ms, s, damageWindowRect, minDamageWindowRect);
    WRAP (ms, s, focusWindow, minFocusWindow);

    s->base.privates[md->screenPrivateIndex].ptr = ms;

    return TRUE;
}

static void
minFiniScreen (CompPlugin *p,
	       CompScreen *s)
{
    MIN_SCREEN (s);

    freeWindowPrivateIndex (s, ms->windowPrivateIndex);

    UNWRAP (ms, s, preparePaintScreen);
    UNWRAP (ms, s, donePaintScreen);
    UNWRAP (ms, s, paintOutput);
    UNWRAP (ms, s, paintWindow);
    UNWRAP (ms, s, damageWindowRect);
    UNWRAP (ms, s, focusWindow);

    compFiniScreenOptions (s, ms->opt, MIN_SCREEN_OPTION_NUM);

    free (ms);
}

static Bool
minInitWindow (CompPlugin *p,
	       CompWindow *w)
{
    MinWindow *mw;

    MIN_SCREEN (w->screen);

    mw = malloc (sizeof (MinWindow));
    if (!mw)
	return FALSE;

    mw->xScale = mw->yScale = 1.0f;
    mw->tx = mw->ty = 0.0f;
    mw->adjust = FALSE;
    mw->xVelocity = mw->yVelocity = 0.0f;
    mw->xScaleVelocity = mw->yScaleVelocity = 1.0f;

    mw->unmapCnt = 0;

    mw->ignoreDamage = FALSE;

    if (w->state & CompWindowStateHiddenMask)
    {
	if (w->shaded)
	{
	    mw->state = mw->newState = NormalState;
	    mw->shade = 0;
	    mw->region = XCreateRegion ();
	}
	else
	{
	    mw->state = mw->newState = minGetWindowState (w);
	    mw->shade = MAXSHORT;
	    mw->region = NULL;
	}
    }
    else
    {
	    mw->state = mw->newState = NormalState;
	    mw->shade = MAXSHORT;
	    mw->region = NULL;
    }

    w->base.privates[ms->windowPrivateIndex].ptr = mw;

    return TRUE;
}

static void
minFiniWindow (CompPlugin *p,
	       CompWindow *w)
{
    MIN_WINDOW (w);

    mw->ignoreDamage = TRUE;
    while (mw->unmapCnt--)
	unmapWindow (w);
    mw->ignoreDamage = FALSE;

    if (mw->region)
	XDestroyRegion (mw->region);

    free (mw);
}

static CompBool
minInitObject (CompPlugin *p,
	       CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) minInitDisplay,
	(InitPluginObjectProc) minInitScreen,
	(InitPluginObjectProc) minInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
minFiniObject (CompPlugin *p,
	       CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) minFiniDisplay,
	(FiniPluginObjectProc) minFiniScreen,
	(FiniPluginObjectProc) minFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
minGetObjectOptions (CompPlugin *plugin,
		     CompObject *object,
		     int	*count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) 0, /* GetDisplayOptions */
	(GetPluginObjectOptionsProc) minGetScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
minSetObjectOption (CompPlugin      *plugin,
		    CompObject      *object,
		    const char      *name,
		    CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) 0, /* SetDisplayOption */
	(SetPluginObjectOptionProc) minSetScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
minInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&minMetadata,
					 p->vTable->name, 0, 0,
					 minScreenOptionInfo,
					 MIN_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&minMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&minMetadata, p->vTable->name);

    return TRUE;
}

static void
minFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&minMetadata);
}

static CompMetadata *
minGetMetadata (CompPlugin *plugin)
{
    return &minMetadata;
}

static CompPluginVTable minVTable = {
    "minimize",
    minGetMetadata,
    minInit,
    minFini,
    minInitObject,
    minFiniObject,
    minGetObjectOptions,
    minSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &minVTable;
}
