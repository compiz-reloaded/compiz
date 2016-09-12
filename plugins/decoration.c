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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <compiz-core.h>
#include <decoration.h>

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

static CompMetadata decorMetadata;

typedef struct _Vector {
    int	dx;
    int	dy;
    int	x0;
    int	y0;
} Vector;

/* FIXME: Remove */
#define DECOR_BARE   0
#define DECOR_ACTIVE 1
#define DECOR_NUM    2

typedef struct _DecorTexture {
    struct _DecorTexture *next;
    int			 refCount;
    Pixmap		 pixmap;
    Damage		 damage;
    CompTexture		 texture;
} DecorTexture;

typedef struct _Decoration {
    int		      refCount;
    DecorTexture      *texture;
    CompWindowExtents output;
    CompWindowExtents frame;
    CompWindowExtents border;
    CompWindowExtents maxFrame;
    CompWindowExtents maxBorder;
    int		      minWidth;
    int		      minHeight;
    unsigned int      frameType;
    unsigned int      frameState;
    unsigned int      frameActions;
    decor_quad_t      *quad;
    int		      nQuad;
} Decoration;

typedef struct _ScaledQuad {
    CompMatrix matrix;
    BoxRec     box;
    float      sx;
    float      sy;
} ScaledQuad;

typedef struct _WindowDecoration {
    Decoration *decor;
    ScaledQuad *quad;
    int	       nQuad;
} WindowDecoration;

static int corePrivateIndex;

typedef struct _DecorCore {
    ObjectAddProc    objectAdd;
    ObjectRemoveProc objectRemove;
} DecorCore;

#define DECOR_DISPLAY_OPTION_SHADOW_RADIUS   0
#define DECOR_DISPLAY_OPTION_SHADOW_OPACITY  1
#define DECOR_DISPLAY_OPTION_SHADOW_COLOR    2
#define DECOR_DISPLAY_OPTION_SHADOW_OFFSET_X 3
#define DECOR_DISPLAY_OPTION_SHADOW_OFFSET_Y 4
#define DECOR_DISPLAY_OPTION_COMMAND         5
#define DECOR_DISPLAY_OPTION_MIPMAP          6
#define DECOR_DISPLAY_OPTION_DECOR_MATCH     7
#define DECOR_DISPLAY_OPTION_SHADOW_MATCH    8
#define DECOR_DISPLAY_OPTION_NUM             9

static int displayPrivateIndex;

typedef struct _DecorDisplay {
    int			     screenPrivateIndex;
    HandleEventProc	     handleEvent;
    MatchPropertyChangedProc matchPropertyChanged;
    DecorTexture	     *textures;
    Atom		     supportingDmCheckAtom;
    Atom		     winDecorAtom;
    Atom		     requestFrameExtentsAtom;
    Atom		     decorAtom[DECOR_NUM];
    Atom		     inputFrameAtom;

    CompOption opt[DECOR_DISPLAY_OPTION_NUM];
} DecorDisplay;

typedef struct _DecorScreen {
    int	windowPrivateIndex;

    Window dmWin;

    Decoration   **decors[DECOR_NUM];
    unsigned int decorNum[DECOR_NUM];

    DrawWindowProc		  drawWindow;
    DamageWindowRectProc	  damageWindowRect;
    GetOutputExtentsForWindowProc getOutputExtentsForWindow;
    AddSupportedAtomsProc         addSupportedAtoms;

    WindowMoveNotifyProc   windowMoveNotify;
    WindowResizeNotifyProc windowResizeNotify;

    WindowStateChangeNotifyProc windowStateChangeNotify;

    CompTimeoutHandle decoratorStartHandle;
} DecorScreen;

typedef struct _DecorWindow {
    WindowDecoration *wd;
    Decoration	     **decors;
    unsigned int     decorNum;

    Window	     inputFrame;

    CompTimeoutHandle resizeUpdateHandle;
} DecorWindow;

#define GET_DECOR_CORE(c)				     \
    ((DecorCore *) (c)->base.privates[corePrivateIndex].ptr)

#define DECOR_CORE(c)		       \
    DecorCore *dc = GET_DECOR_CORE (c)

#define GET_DECOR_DISPLAY(d)					   \
    ((DecorDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define DECOR_DISPLAY(d)		     \
    DecorDisplay *dd = GET_DECOR_DISPLAY (d)

#define GET_DECOR_SCREEN(s, dd)					       \
    ((DecorScreen *) (s)->base.privates[(dd)->screenPrivateIndex].ptr)

#define DECOR_SCREEN(s)							   \
    DecorScreen *ds = GET_DECOR_SCREEN (s, GET_DECOR_DISPLAY (s->display))

#define GET_DECOR_WINDOW(w, ds)					       \
    ((DecorWindow *) (w)->base.privates[(ds)->windowPrivateIndex].ptr)

#define DECOR_WINDOW(w)					       \
    DecorWindow *dw = GET_DECOR_WINDOW  (w,		       \
		      GET_DECOR_SCREEN  (w->screen,	       \
		      GET_DECOR_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static Bool
decorDrawWindow (CompWindow	      *w,
		 const CompTransform  *transform,
		 const FragmentAttrib *attrib,
		 Region		      region,
		 unsigned int	      mask)
{
    Bool status;

    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    UNWRAP (ds, w->screen, drawWindow);
    status = (*w->screen->drawWindow) (w, transform, attrib, region, mask);
    WRAP (ds, w->screen, drawWindow, decorDrawWindow);

    /* we wait to draw dock shadows until we get to the lowest
       desktop window in the stack */
    if (w->type & CompWindowTypeDockMask)
	return status;

    if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	region = &infiniteRegion;

    if (dw->wd && region->numRects)
    {
	WindowDecoration *wd = dw->wd;
	REGION	     box;
	int		     i;

	mask |= PAINT_WINDOW_BLEND_MASK;

	box.rects	 = &box.extents;
	box.numRects = 1;

	w->vCount = w->indexCount = 0;

	for (i = 0; i < wd->nQuad; i++)
	{
	    box.extents = wd->quad[i].box;

	    if (box.extents.x1 < box.extents.x2 &&
		box.extents.y1 < box.extents.y2)
	    {
		(*w->screen->addWindowGeometry) (w,
						 &wd->quad[i].matrix, 1,
						 &box,
						 region);
	    }
	}

	if (w->vCount)
	    (*w->screen->drawWindowTexture) (w,
					     &wd->decor->texture->texture,
					     attrib, mask);
    }

    if (w->type & CompWindowTypeDesktopMask)
    {
	/* we only want to draw on the lowest desktop window, find it and see
	   if we the window we have is it */
	CompWindow *window = w->screen->windows;
	for (window = w->screen->windows; window; window = window->next)
	{
	    if (window->type & CompWindowTypeDesktopMask)
	    {
		if (window == w)
		    break;
		else
		    return status;
	    }
	}

	/* drawing dock shadows now */
	for (window = w->screen->windows; window; window = window->next)
	{
	    if (window->type & CompWindowTypeDockMask && !window->destroyed && !window->invisible)
	    {
		DECOR_WINDOW (window);

		if (dw->wd && region->numRects)
		{
		    WindowDecoration *wd = dw->wd;
		    REGION	     box;
		    int		     i;

		    mask |= PAINT_WINDOW_BLEND_MASK;

		    box.rects	 = &box.extents;
		    box.numRects = 1;

		    window->vCount = window->indexCount = 0;

		    for (i = 0; i < wd->nQuad; i++)
		    {
			box.extents = wd->quad[i].box;

			if (box.extents.x1 < box.extents.x2 &&
			    box.extents.y1 < box.extents.y2)
			{
			    (*window->screen->addWindowGeometry) (window,
								  &wd->quad[i].matrix, 1,
								  &box,
								  region);
			}
		    }

		    if (window->vCount)
			(*window->screen->drawWindowTexture) (window,
							      &wd->decor->texture->texture,
							      attrib, mask);
		}
	    }
	}
    }

    return status;
}

static DecorTexture *
decorGetTexture (CompScreen *screen,
		 Pixmap	    pixmap)
{
    DecorTexture *texture;
    unsigned int width, height, depth, ui;
    Window	 root;
    int		 i;

    DECOR_DISPLAY (screen->display);

    for (texture = dd->textures; texture; texture = texture->next)
    {
	if (texture->pixmap == pixmap)
	{
	    texture->refCount++;
	    return texture;
	}
    }

    texture = malloc (sizeof (DecorTexture));
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

    if (!dd->opt[DECOR_DISPLAY_OPTION_MIPMAP].value.b)
	texture->texture.mipmap = FALSE;

    texture->damage = XDamageCreate (screen->display->display, pixmap,
				     XDamageReportRawRectangles);

    texture->refCount = 1;
    texture->pixmap   = pixmap;
    texture->next     = dd->textures;

    dd->textures = texture;

    return texture;
}

static void
decorReleaseTexture (CompScreen   *screen,
		     DecorTexture *texture)
{
    DECOR_DISPLAY (screen->display);

    texture->refCount--;
    if (texture->refCount)
	return;

    if (texture == dd->textures)
    {
	dd->textures = texture->next;
    }
    else
    {
	DecorTexture *t;

	for (t = dd->textures; t; t = t->next)
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
computeQuadBox (decor_quad_t *q,
		int	     width,
		int	     height,
		int	     *return_x1,
		int	     *return_y1,
		int	     *return_x2,
		int	     *return_y2,
		float        *return_sx,
		float        *return_sy)
{
    int   x1, y1, x2, y2;
    float sx = 1.0f;
    float sy = 1.0f;

    decor_apply_gravity (q->p1.gravity, q->p1.x, q->p1.y, width, height,
			 &x1, &y1);
    decor_apply_gravity (q->p2.gravity, q->p2.x, q->p2.y, width, height,
			 &x2, &y2);

    if (q->clamp & CLAMP_HORZ)
    {
	if (x1 < 0)
	    x1 = 0;
	if (x2 > width)
	    x2 = width;
    }

    if (q->clamp & CLAMP_VERT)
    {
	if (y1 < 0)
	    y1 = 0;
	if (y2 > height)
	    y2 = height;
    }

    if (q->stretch & STRETCH_X)
    {
	sx = (float)q->max_width / ((float)(x2 - x1));
    }
    else if (q->max_width < x2 - x1)
    {
	if (q->align & ALIGN_RIGHT)
	    x1 = x2 - q->max_width;
	else
	    x2 = x1 + q->max_width;
    }

    if (q->stretch & STRETCH_Y)
    {
	sy = (float)q->max_height / ((float)(y2 - y1));
    }
    else if (q->max_height < y2 - y1)
    {
	if (q->align & ALIGN_BOTTOM)
	    y1 = y2 - q->max_height;
	else
	    y2 = y1 + q->max_height;
    }

    *return_x1 = x1;
    *return_y1 = y1;
    *return_x2 = x2;
    *return_y2 = y2;

    if (return_sx)
	*return_sx = sx;
    if (return_sy)
	*return_sy = sy;
}

static Decoration *
decorCreateDecoration (CompScreen   *screen,
		       Window	    id,
		       long	    *prop,
		       unsigned int size,
		       unsigned int type,
		       unsigned int nOffset)
{
    Decoration	    *decoration;
    unsigned int    frameType, frameState, frameActions;
    Pixmap	    pixmap;
    decor_extents_t frame, border;
    decor_extents_t maxFrame, maxBorder;
    decor_quad_t    *quad;
    int		    nQuad;
    int		    minWidth;
    int		    minHeight;
    int		    left, right, top, bottom;
    int		    x1, y1, x2, y2;

    nQuad = N_QUADS_MAX;

    quad = calloc (nQuad, sizeof (decor_quad_t));
    if (!quad)
    {
	compLogMessage ("decoration",
			CompLogLevelWarn,
			"Failed to allocate %i quads\n", nQuad);
	return NULL;
    }

    nQuad = decor_pixmap_property_to_quads (prop,
					    nOffset,
					    size,
					    &pixmap,
					    &frame,
					    &border,
					    &maxFrame,
					    &maxBorder,
					    &minWidth,
					    &minHeight,
					    &frameType,
					    &frameState,
					    &frameActions,
					    quad);

    if (!nQuad)
    {
	free (quad);
	return NULL;
    }

    decoration = malloc (sizeof (Decoration));
    if (!decoration)
    {
	free (quad);
	return NULL;
    }

    decoration->texture = decorGetTexture (screen, pixmap);
    if (!decoration->texture)
    {
	free (decoration);
	free (quad);
	return NULL;
    }

    decoration->minWidth  = minWidth;
    decoration->minHeight = minHeight;
    decoration->quad	  = quad;
    decoration->nQuad	  = nQuad;

    left   = 0;
    right  = minWidth;
    top    = 0;
    bottom = minHeight;

    while (nQuad--)
    {
	computeQuadBox (quad, minWidth, minHeight, &x1, &y1, &x2, &y2,
			NULL, NULL);

	if (x1 < left)
	    left = x1;
	if (y1 < top)
	    top = y1;
	if (x2 > right)
	    right = x2;
	if (y2 > bottom)
	    bottom = y2;

	quad++;
    }

    decoration->output.left   = -left;
    decoration->output.right  = right - minWidth;
    decoration->output.top    = -top;
    decoration->output.bottom = bottom - minHeight;

    /* Extents of actual frame window */
    decoration->frame.left   = frame.left;
    decoration->frame.right  = frame.right;
    decoration->frame.top    = frame.top;
    decoration->frame.bottom = frame.bottom;

    /* Border extents */
    decoration->border.left   = border.left;
    decoration->border.right  = border.right;
    decoration->border.top    = border.top;
    decoration->border.bottom = border.bottom;

    /* Extents of actual frame window */
    decoration->maxFrame.left   = maxFrame.left;
    decoration->maxFrame.right  = maxFrame.right;
    decoration->maxFrame.top    = maxFrame.top;
    decoration->maxFrame.bottom = maxFrame.bottom;

    /* Border extents */
    decoration->maxBorder.left   = maxBorder.left;
    decoration->maxBorder.right  = maxBorder.right;
    decoration->maxBorder.top    = maxBorder.top;
    decoration->maxBorder.bottom = maxBorder.bottom;

    /* Decoration info */
    decoration->frameType    = frameType;
    decoration->frameState   = frameState;
    decoration->frameActions = frameActions;

    decoration->refCount = 1;

    return decoration;
}

static void
decorReleaseDecoration (CompScreen *screen,
			Decoration *decoration)
{
    if (!decoration)
	return;

    decoration->refCount--;
    if (decoration->refCount > 0)
	return;

    decorReleaseTexture (screen, decoration->texture);

    free (decoration->quad);
    free (decoration);
}

static void
decorReleaseDecorations (CompScreen   *screen,
			 Decoration   **decors,
			 unsigned int *decorNum)
{
    int i;

    if (decorNum && *decorNum > 0)
    {
	if (decors)
	{
	    for (i = 0; i < *decorNum; i++)
	    {
		if (decors[i])
		    decorReleaseDecoration (screen, decors[i]);
	    }
	    free (decors);
	}
	*decorNum = 0;
    }
}

static Decoration **
decorUpdateDecorations (CompScreen   *screen,
			Window       id,
			Atom	     decorAtom,
			Decoration   **decors,
			unsigned int *decorNum)
{
    int		    i;
    unsigned long   n, nleft;
    unsigned char   *data;
    long	    *prop;
    Atom	    actual;
    int		    result, format;
    unsigned int    type;

    /* null pointer as decorNum is unacceptable */
    if (!decorNum)
	return NULL;

    /* FIXME: should really check the property to see
       if the decoration changed, and /then/ release
       and re-update it, but for now just releasing all */
    if (decors && *decorNum > 0)
	decorReleaseDecorations (screen, decors, decorNum);
    decors = NULL;
    *decorNum = 0;

    result = XGetWindowProperty (screen->display->display, id,
				 decorAtom, 0L,
				 PROP_HEADER_SIZE + 6 * (BASE_PROP_SIZE +
				   QUAD_PROP_SIZE * N_QUADS_MAX),
				 FALSE, XA_INTEGER, &actual, &format,
				 &n, &nleft, &data);

    if (result != Success || !data)
	return NULL;

    if (!n)
    {
	XFree (data);
	return NULL;
    }

    /* attempted to read the reasonable amount of
     * around 6 pixmap decorations, if there are more, we need
     * an additional roundtrip to read everything else
     */
    if (nleft)
    {
	XFree (data);

	result = XGetWindowProperty (screen->display->display, id, decorAtom, 0L,
				     n + nleft, FALSE, XA_INTEGER,
				     &actual, &format,
				     &n, &nleft, &data);

	if (result != Success || !data)
	    return NULL;

	if (!n)
	{
	    XFree (data);
	    return NULL;
	}
    }

    prop = (long *) data;

    if (decor_property_get_version (prop) != decor_version ())
    {
	compLogMessage ("decoration", CompLogLevelWarn,
			"Property ignored because "
			"version is %d and decoration plugin version is %d\n",
			decor_property_get_version (prop), decor_version ());

	XFree (data);
	return NULL;
    }

    type = decor_property_get_type (prop);

    *decorNum = decor_property_get_num (prop);
    if (*decorNum == 0)
    {
	XFree (data);
	return NULL;
    }

    /* create a new decoration for each individual item on the property */
    decors = calloc (*decorNum, sizeof (Decoration *));
    for (i = 0; i < *decorNum; i++)
	decors[i] = NULL;
    for (i = 0; i < *decorNum; i++)
    {
	Decoration *d = decorCreateDecoration (screen, id, prop, n, type, i);

	if (d)
	    decors[i] = d;
	else
	{
	    unsigned int realDecorNum = i;
	    decorReleaseDecorations (screen, decors, &realDecorNum);
	    *decorNum = realDecorNum;
	    XFree (data);
	    return NULL;
	}
    }

    XFree (data);

    return decors;
}

static void
decorWindowUpdateDecoration (CompWindow *w)
{
    DECOR_DISPLAY (w->screen->display);
    DECOR_WINDOW (w);

    dw->decors = decorUpdateDecorations (w->screen, w->id,
					 dd->winDecorAtom,
					 dw->decors, &dw->decorNum);
}

static WindowDecoration *
createWindowDecoration (Decoration *d)
{
    WindowDecoration *wd;

    wd = malloc (sizeof (WindowDecoration) +
		 sizeof (ScaledQuad) * d->nQuad);
    if (!wd)
	return NULL;

    d->refCount++;

    wd->decor = d;
    wd->quad  = (ScaledQuad *) (wd + 1);
    wd->nQuad = d->nQuad;

    return wd;
}

static void
destroyWindowDecoration (CompScreen	  *screen,
			 WindowDecoration *wd)
{
    decorReleaseDecoration (screen, wd->decor);
    free (wd);
}

static void
setDecorationMatrices (CompWindow *w)
{
    WindowDecoration *wd;
    int		     i;
    float	     x0, y0;
    decor_matrix_t   a;
    CompMatrix       b;


    DECOR_WINDOW (w);

    wd = dw->wd;
    if (!wd)
	return;

    for (i = 0; i < wd->nQuad; i++)
    {
	wd->quad[i].matrix = wd->decor->texture->texture.matrix;

	x0 = wd->decor->quad[i].m.x0;
	y0 = wd->decor->quad[i].m.y0;

	a = wd->decor->quad[i].m;
	b = wd->quad[i].matrix;

	wd->quad[i].matrix.xx = a.xx * b.xx + a.yx * b.xy;
	wd->quad[i].matrix.yx = a.xx * b.yx + a.yx * b.yy;
	wd->quad[i].matrix.xy = a.xy * b.xx + a.yy * b.xy;
	wd->quad[i].matrix.yy = a.xy * b.yx + a.yy * b.yy;
	wd->quad[i].matrix.x0 = x0 * b.xx + y0 * b.xy + b.x0;
	wd->quad[i].matrix.y0 = x0 * b.yx + y0 * b.yy + b.y0;

	wd->quad[i].matrix.xx *= wd->quad[i].sx;
	wd->quad[i].matrix.yx *= wd->quad[i].sx;
	wd->quad[i].matrix.xy *= wd->quad[i].sy;
	wd->quad[i].matrix.yy *= wd->quad[i].sy;

	if (wd->decor->quad[i].align & ALIGN_RIGHT)
	    x0 = wd->quad[i].box.x2 - wd->quad[i].box.x1;
	else
	    x0 = 0.0f;

	if (wd->decor->quad[i].align & ALIGN_BOTTOM)
	    y0 = wd->quad[i].box.y2 - wd->quad[i].box.y1;
	else
	    y0 = 0.0f;

	wd->quad[i].matrix.x0 -=
	    x0 * wd->quad[i].matrix.xx +
	    y0 * wd->quad[i].matrix.xy;

	wd->quad[i].matrix.y0 -=
	    y0 * wd->quad[i].matrix.yy +
	    x0 * wd->quad[i].matrix.yx;

	wd->quad[i].matrix.x0 -=
	    wd->quad[i].box.x1 * wd->quad[i].matrix.xx +
	    wd->quad[i].box.y1 * wd->quad[i].matrix.xy;

	wd->quad[i].matrix.y0 -=
	    wd->quad[i].box.y1 * wd->quad[i].matrix.yy +
	    wd->quad[i].box.x1 * wd->quad[i].matrix.yx;
    }
}

static void
updateWindowDecorationScale (CompWindow *w)
{
    WindowDecoration *wd;
    int		     x1, y1, x2, y2;
    float	     sx, sy;
    int		     i;

    DECOR_WINDOW (w);

    wd = dw->wd;
    if (!wd)
	return;

    for (i = 0; i < wd->nQuad; i++)
    {
	computeQuadBox (&wd->decor->quad[i], w->width, w->height,
			&x1, &y1, &x2, &y2, &sx, &sy);

	wd->quad[i].box.x1 = x1 + w->attrib.x;
	wd->quad[i].box.y1 = y1 + w->attrib.y;
	wd->quad[i].box.x2 = x2 + w->attrib.x;
	wd->quad[i].box.y2 = y2 + w->attrib.y;
	wd->quad[i].sx     = sx;
	wd->quad[i].sy     = sy;
    }

    setDecorationMatrices (w);
}

static Bool
decorCheckSize (CompWindow *w,
		Decoration *decor)
{
    return (decor->minWidth <= w->width && decor->minHeight <= w->height);
}

static int
decorWindowShiftX (CompWindow *w)
{
    switch (w->sizeHints.win_gravity) {
    case WestGravity:
    case NorthWestGravity:
    case SouthWestGravity:
	return w->input.left;
    case EastGravity:
    case NorthEastGravity:
    case SouthEastGravity:
	return -w->input.right;
    }

    return 0;
}

static int
decorWindowShiftY (CompWindow *w)
{
    switch (w->sizeHints.win_gravity) {
    case NorthGravity:
    case NorthWestGravity:
    case NorthEastGravity:
	return w->input.top;
    case SouthGravity:
    case SouthWestGravity:
    case SouthEastGravity:
	return -w->input.bottom;
    }

    return 0;
}

static void
decorWindowUpdateFrame (CompWindow *w)
{
    CompDisplay	     *d = w->screen->display;
    WindowDecoration *wd;

    DECOR_DISPLAY (w->screen->display);
    DECOR_WINDOW (w);

    wd = dw->wd;

    if (wd && (w->frameInput.left || w->frameInput.right ||
	w->frameInput.top || w->frameInput.bottom ||
	w->input.left || w->input.right || w->input.top || w->input.bottom))
    {
	XRectangle           rects[4];
	int	             x, y, width, height;
	int	             i = 0;
	int                  bw = w->serverBorderWidth * 2;
	CompWindowExtents    frame;

	if ((w->state & MAXIMIZE_STATE) == MAXIMIZE_STATE)
	    frame = wd->decor->maxFrame;
	else
	    frame = wd->decor->frame;

	x      = w->frameInput.left - frame.left;
	y      = w->frameInput.top - frame.top;
	width  = w->serverWidth + frame.left + frame.right + bw;
	height = w->serverHeight + frame.top  + frame.bottom + bw;

	if (w->shaded)
	    height = frame.top + frame.bottom;

	if (!dw->inputFrame)
	{
	    XSetWindowAttributes attr;

	    attr.event_mask	   = StructureNotifyMask;
	    attr.override_redirect = TRUE;

	    dw->inputFrame = XCreateWindow (d->display, w->frame,
					    x, y, width, height, 0, CopyFromParent,
					    InputOnly, CopyFromParent,
					    CWOverrideRedirect | CWEventMask,
					    &attr);

	    XGrabButton (d->display, AnyButton, AnyModifier, dw->inputFrame,
			 TRUE, ButtonPressMask | ButtonReleaseMask |
			 ButtonMotionMask, GrabModeSync, GrabModeSync, None,
			 None);

	    XMapWindow (d->display, dw->inputFrame);

	    XChangeProperty (d->display, w->id,
			     dd->inputFrameAtom, XA_WINDOW, 32,
			     PropModeReplace, (unsigned char *) &dw->inputFrame, 1);
	}

	XMoveResizeWindow (d->display, dw->inputFrame, x, y, width, height);
	XLowerWindow (d->display, dw->inputFrame);

	rects[i].x	= 0;
	rects[i].y	= 0;
	rects[i].width  = width;
	rects[i].height = frame.top;

	if (rects[i].width && rects[i].height)
	    i++;

	rects[i].x	= 0;
	rects[i].y	= frame.top;
	rects[i].width  = frame.left;
	rects[i].height = height - frame.top - frame.bottom;

	if (rects[i].width && rects[i].height)
	    i++;

	rects[i].x	= width - frame.right;
	rects[i].y	= frame.top;
	rects[i].width  = frame.right;
	rects[i].height = height - frame.top - frame.bottom;

	if (rects[i].width && rects[i].height)
	    i++;

	rects[i].x	= 0;
	rects[i].y	= height - frame.bottom;
	rects[i].width  = width;
	rects[i].height = frame.bottom;

	if (rects[i].width && rects[i].height)
	    i++;

	XShapeCombineRectangles (d->display, dw->inputFrame, ShapeInput,
				 0, 0, rects, i, ShapeSet, YXBanded);
    }
    else
    {
	if (dw->inputFrame)
	{
	    XDeleteProperty (d->display, w->id,
			     dd->inputFrameAtom);
	    XDestroyWindow (d->display, dw->inputFrame);
	    dw->inputFrame = None;
	}
    }
}

static Bool
decorWindowMatchType (CompWindow *w,
		      unsigned int decorType)
{
    int i;
    unsigned int nTypeStates = 5;
    struct typestate {
	unsigned int compFlag;
	unsigned int decorFlag;
    } typeStates[] =
    {
	{ CompWindowTypeNormalMask, DECOR_WINDOW_TYPE_NORMAL },
	{ CompWindowTypeDialogMask, DECOR_WINDOW_TYPE_DIALOG },
	{ CompWindowTypeModalDialogMask, DECOR_WINDOW_TYPE_MODAL_DIALOG },
	{ CompWindowTypeMenuMask, DECOR_WINDOW_TYPE_MENU },
	{ CompWindowTypeUtilMask, DECOR_WINDOW_TYPE_UTILITY}
    };

    for (i = 0; i < nTypeStates; i++)
    {
	if ((decorType & typeStates[i].decorFlag) &&
	    (w->type & typeStates[i].compFlag))
	    return TRUE;
    }

    return FALSE;
}

static Bool
decorWindowMatchState (CompWindow   *w,
		       unsigned int decorState)
{
    int i;
    unsigned int nStateStates = 3;
    struct statestate {
	unsigned int compFlag;
	unsigned int decorFlag;
    } stateStates[] =
    {
	{ CompWindowStateMaximizedVertMask, DECOR_WINDOW_STATE_MAXIMIZED_VERT },
	{ CompWindowStateMaximizedHorzMask, DECOR_WINDOW_STATE_MAXIMIZED_HORZ },
	{ CompWindowStateShadedMask, DECOR_WINDOW_STATE_SHADED }
    };

    /* active is a separate check */
    if (w->screen->display->activeWindow == w->id)
	decorState &= ~(DECOR_WINDOW_STATE_FOCUS);

    for (i = 0; i < nStateStates; i++)
    {
	if ((decorState & stateStates[i].decorFlag) &&
	    (w->state & stateStates[i].compFlag))
	    decorState &= ~(stateStates[i].decorFlag);
    }

    return (decorState == 0);
}

static Bool
decorWindowMatchActions (CompWindow   *w,
			 unsigned int decorActions)
{
    int i;
    unsigned int nActionStates = 16;
    struct actionstate {
	unsigned int compFlag;
	unsigned int decorFlag;
    } actionStates[] =
    {
	{ DECOR_WINDOW_ACTION_RESIZE_HORZ, CompWindowActionResizeMask },
	{ DECOR_WINDOW_ACTION_RESIZE_VERT, CompWindowActionResizeMask },
	{ DECOR_WINDOW_ACTION_CLOSE, CompWindowActionCloseMask },
	{ DECOR_WINDOW_ACTION_MINIMIZE, CompWindowActionMinimizeMask },
	{ DECOR_WINDOW_ACTION_UNMINIMIZE, CompWindowActionMinimizeMask },
	{ DECOR_WINDOW_ACTION_MAXIMIZE_HORZ, CompWindowActionMaximizeHorzMask },
	{ DECOR_WINDOW_ACTION_MAXIMIZE_VERT, CompWindowActionMaximizeVertMask },
	{ DECOR_WINDOW_ACTION_UNMAXIMIZE_HORZ, CompWindowActionMaximizeHorzMask },
	{ DECOR_WINDOW_ACTION_UNMAXIMIZE_VERT, CompWindowActionMaximizeVertMask },
	{ DECOR_WINDOW_ACTION_SHADE, CompWindowActionShadeMask },
	{ DECOR_WINDOW_ACTION_UNSHADE, CompWindowActionShadeMask },
	{ DECOR_WINDOW_ACTION_STICK, CompWindowActionStickMask },
	{ DECOR_WINDOW_ACTION_UNSTICK, CompWindowActionStickMask },
	{ DECOR_WINDOW_ACTION_FULLSCREEN, CompWindowActionFullscreenMask },
	{ DECOR_WINDOW_ACTION_ABOVE, CompWindowActionAboveMask },
	{ DECOR_WINDOW_ACTION_BELOW, CompWindowActionBelowMask },
    };

    for (i = 0; i < nActionStates; i++)
    {
	if ((decorActions & actionStates[i].decorFlag) &&
	    (w->type & actionStates[i].compFlag))
	    decorActions &= ~(actionStates[i].decorFlag);
    }

    return (decorActions == 0);
}

static Decoration *
decorFindMatchingDecoration (CompWindow   *w,
			     Decoration   **decors,
			     unsigned int decorNum,
			     Bool	  sizeCheck)
{
    int	       i;
    Decoration *decoration = NULL;

    if (!decors)
       return NULL;

    if (decorNum > 0)
    {
	int typeMatch = (1 << 0);
	int stateMatch = (1 << 1);
	int actionsMatch = (1 << 2);

	int currentDecorState = 0;

	if (sizeCheck ? decorCheckSize (w, decors[decorNum - 1]) : TRUE)
	    decoration = decors[decorNum - 1];
	else
	    decoration = NULL;

	for (i = 0; i < decorNum; i++)
	{
	    Decoration *d = decors[i];

	    /* must always match type */
	    if (decorWindowMatchType (w, d->frameType))
	    {
		/* use this decoration if the type matched */
		if (!(typeMatch & currentDecorState) &&
		    (!sizeCheck || decorCheckSize (w, d)))
		{
		    decoration = d;
		    currentDecorState |= typeMatch;
		}

		/* must always match state if type is already matched */
		if (decorWindowMatchState (w, d->frameState) &&
		    (!sizeCheck || decorCheckSize (w, d)))
		{
		    /* use this decoration if the type and state match */
		    if (!(stateMatch & currentDecorState))
		    {
			decoration = d;
			currentDecorState |= stateMatch;
		    }

		    /* must always match actions if
		       state and type are already matched */
		    if (decorWindowMatchActions (w, d->frameActions) &&
			(!sizeCheck || decorCheckSize (w, d)))
		    {
			/* use this decoration if the requested actions match */
			if (!(actionsMatch & currentDecorState))
			{
			    decoration = d;
			    currentDecorState |= actionsMatch;

			    /* perfect match, no need to continue searching */
			    break;
			}
		    }
		}
	    }
	}
    }

    return decoration;
}

static Bool
decorWindowUpdate (CompWindow *w,
		   Bool	      allowDecoration)
{
    WindowDecoration *wd;
    Decoration	     *old = NULL, *decoration = NULL;
    Bool	     decorate = FALSE;
    CompMatch	     *match;
    int		     moveDx, moveDy;
    int		     oldShiftX = 0;
    int		     oldShiftY = 0;

    DECOR_DISPLAY (w->screen->display);
    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    wd = dw->wd;
    old = (wd) ? wd->decor : NULL;

    switch (w->type) {
    case CompWindowTypeDialogMask:
    case CompWindowTypeModalDialogMask:
    case CompWindowTypeUtilMask:
    case CompWindowTypeMenuMask:
    case CompWindowTypeNormalMask:
	if (w->mwmDecor & (MwmDecorAll | MwmDecorTitle))
	    decorate = TRUE;
    default:
	break;
    }

    if (w->wmType & (CompWindowTypeDockMask | CompWindowTypeDesktopMask))
	decorate = FALSE;

    if (w->attrib.override_redirect)
	decorate = FALSE;

    if (decorate)
    {
	match = &dd->opt[DECOR_DISPLAY_OPTION_DECOR_MATCH].value.match;
	if (!matchEval (match, w))
	    decorate = FALSE;
    }

    if (decorate)
    {
	/* attempt to find a matching decoration */
	decoration = decorFindMatchingDecoration (w,
						  dw->decors,
						  dw->decorNum,
						  TRUE);

	if (!decoration)
	{
	    /* find an appropriate default decoration to use */
	    decoration = decorFindMatchingDecoration (w,
						      ds->decors[DECOR_ACTIVE],
						      ds->decorNum[DECOR_ACTIVE],
						      FALSE);

	    if (!decoration)
		decorate = FALSE;
	}
    }

    if (!decorate)
    {
	match = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_MATCH].value.match;
	if (matchEval (match, w))
	{
	    if (ds->decors[DECOR_BARE] && ds->decorNum[DECOR_BARE] > 0 &&
		w->region->numRects == 1)
		decoration = ds->decors[DECOR_BARE][0];

	    if (decoration)
	    {
		if (!decorCheckSize (w, decoration))
		    decoration = NULL;
	    }
	}
    }

    if (!ds->dmWin || !allowDecoration)
	decoration = NULL;

    if (decoration == old)
	return FALSE;

    damageWindowOutputExtents (w);

    if (old)
    {
	oldShiftX = decorWindowShiftX (w);
	oldShiftY = decorWindowShiftY (w);

	destroyWindowDecoration (w->screen, wd);
    }

    if (decoration)
    {
	dw->wd = createWindowDecoration (decoration);
	if (!dw->wd)
	    return FALSE;

	if ((w->state & MAXIMIZE_STATE) == MAXIMIZE_STATE)
	{
	    setWindowFrameExtents (w, &decoration->maxFrame);
	    setWindowBorderExtents (w, &decoration->maxBorder);
	}
	else
	{
	    setWindowFrameExtents (w, &decoration->frame);
	    setWindowBorderExtents (w, &decoration->border);
	}

	moveDx = decorWindowShiftX (w) - oldShiftX;
	moveDy = decorWindowShiftY (w) - oldShiftY;

	decorWindowUpdateFrame (w);
	updateWindowOutputExtents (w);
	damageWindowOutputExtents (w);
	updateWindowDecorationScale (w);
    }
    else
    {
	CompWindowExtents emptyInput;

	memset (&emptyInput, 0, sizeof (emptyInput));
	setWindowFrameExtents (w, &emptyInput);

	dw->wd = NULL;

	moveDx = -oldShiftX;
	moveDy = -oldShiftY;

	decorWindowUpdateFrame (w);
    }

    if (w->placed && !w->attrib.override_redirect && (moveDx || moveDy))
    {
	XWindowChanges xwc;
	unsigned int   mask = CWX | CWY;

	xwc.x = w->serverX + moveDx;
	xwc.y = w->serverY + moveDy;

	if (w->state & CompWindowStateFullscreenMask)
	    mask &= ~(CWX | CWY);

	if (w->state & CompWindowStateMaximizedHorzMask)
	    mask &= ~CWX;

	if (w->state & CompWindowStateMaximizedVertMask)
	    mask &= ~CWY;

	if (w->saveMask & CWX)
	    w->saveWc.x += moveDx;

	if (w->saveMask & CWY)
	    w->saveWc.y += moveDy;

	if (mask)
	    configureXWindow (w, mask, &xwc);
    }

    return TRUE;
}

static void
decorCheckForDmOnScreen (CompScreen *s,
			 Bool	    updateWindows)
{
    CompDisplay   *d = s->display;
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    Window	  dmWin = None;

    DECOR_DISPLAY (s->display);
    DECOR_SCREEN (s);

    result = XGetWindowProperty (d->display, s->root,
				 dd->supportingDmCheckAtom, 0L, 1L, FALSE,
				 XA_WINDOW, &actual, &format,
				 &n, &left, &data);

    if (result == Success && data)
    {
	if (n)
	{
	    XWindowAttributes attr;

	    memcpy (&dmWin, data, sizeof (Window));

	    compCheckForError (d->display);

	    XGetWindowAttributes (d->display, dmWin, &attr);

	    if (compCheckForError (d->display))
		dmWin = None;
	}

	XFree (data);
    }

    /* different decorator became active, update all decorations */
    if (dmWin != ds->dmWin)
    {
	CompWindow *w;
	int	   i;

	/* create new default decorations */
	if (dmWin)
	{
	    for (i = 0; i < DECOR_NUM; i++)
		ds->decors[i] = decorUpdateDecorations (s,
							s->root,
							dd->decorAtom[i],
							ds->decors[i],
							&ds->decorNum[i]);
	}
	else
	{
	    /* no decorator active, destroy all decorations */
	    for (i = 0; i < DECOR_NUM; i++)
	    {
		if (ds->decors[i] && ds->decorNum[i] > 0)
		    decorReleaseDecorations (s, ds->decors[i], &ds->decorNum[i]);
		ds->decors[i] = NULL;
	    }

	    for (w = s->windows; w; w = w->next)
	    {
		DECOR_WINDOW (w);

		if (dw->decors && dw->decorNum > 0)
		    decorReleaseDecorations (s, dw->decors, &dw->decorNum);
		dw->decors = NULL;
	    }
	}

	ds->dmWin = dmWin;

	if (updateWindows)
	{
	    for (w = s->windows; w; w = w->next)
		decorWindowUpdate (w, TRUE);
	}
    }
}

static void
decorHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    Window     activeWindow = d->activeWindow;
    CompWindow *w;

    DECOR_DISPLAY (d);

    switch (event->type) {
    case DestroyNotify:
	w = findWindowAtDisplay (d, event->xdestroywindow.window);
	if (w)
	{
	    DECOR_SCREEN (w->screen);

	    if (w->id == ds->dmWin)
		decorCheckForDmOnScreen (w->screen, TRUE);
	}
	break;
    case MapRequest:
	w = findWindowAtDisplay (d, event->xmaprequest.window);
	if (w)
	    decorWindowUpdate (w, TRUE);
	break;
    case ClientMessage:
	if (event->xclient.message_type == dd->requestFrameExtentsAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
		decorWindowUpdate (w, TRUE);
	}
	break;
    default:
	if (event->type == d->damageEvent + XDamageNotify)
	{
	    XDamageNotifyEvent *de = (XDamageNotifyEvent *) event;
	    DecorTexture       *t;

	    for (t = dd->textures; t; t = t->next)
	    {
		if (t->pixmap == de->drawable)
		{
		    DecorWindow *dw;
		    DecorScreen *ds;
		    CompScreen  *s;

		    t->texture.oldMipmaps = TRUE;

		    for (s = d->screens; s; s = s->next)
		    {
			ds = GET_DECOR_SCREEN (s, dd);

			for (w = s->windows; w; w = w->next)
			{
			    if (w->shaded || w->mapNum)
			    {
				dw = GET_DECOR_WINDOW (w, ds);

				if (dw->wd && dw->wd->decor->texture == t)
				    damageWindowOutputExtents (w);
			    }
			}
		    }
		    return;
		}
	    }
	}
	break;
    }

    UNWRAP (dd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (dd, d, handleEvent, decorHandleEvent);

    if (d->activeWindow != activeWindow)
    {
	w = findWindowAtDisplay (d, activeWindow);
	if (w)
	    decorWindowUpdate (w, TRUE);

	w = findWindowAtDisplay (d, d->activeWindow);
	if (w)
	    decorWindowUpdate (w, TRUE);
    }

    switch (event->type) {
    case PropertyNotify:
	if (event->xproperty.atom == dd->winDecorAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
	    {
		decorWindowUpdateDecoration (w);
		decorWindowUpdate (w, TRUE);
	    }
	}
	else if (event->xproperty.atom == d->mwmHintsAtom)
	{
	    w = findWindowAtDisplay (d, event->xproperty.window);
	    if (w)
		decorWindowUpdate (w, TRUE);
	}
	else
	{
	    CompScreen *s;

	    s = findScreenAtDisplay (d, event->xproperty.window);
	    if (s)
	    {
		if (event->xproperty.atom == dd->supportingDmCheckAtom)
		{
		    decorCheckForDmOnScreen (s, TRUE);
		}
		else
		{
		    int i;

		    for (i = 0; i < DECOR_NUM; i++)
		    {
			if (event->xproperty.atom == dd->decorAtom[i])
			{
			    DECOR_SCREEN (s);

			    if (ds->decors[i] && ds->decorNum[i] > 0)
				decorReleaseDecorations (s,
							 ds->decors[i],
							 &ds->decorNum[i]);

			    ds->decors[i] =
			      decorUpdateDecorations (s,
						      s->root,
						      dd->decorAtom[i],
						      ds->decors[i],
						      &ds->decorNum[i]);

			    for (w = s->windows; w; w = w->next)
				decorWindowUpdate (w, TRUE);
			}
		    }
		}
	    }
	}
	break;
    case ConfigureNotify:
	w = findTopLevelWindowAtDisplay (d, event->xproperty.window);
	if (w)
	{
	    DECOR_WINDOW (w);
	    if (dw->wd && dw->wd->decor)
		decorWindowUpdateFrame (w);
	}
	break;
    case DestroyNotify:
	w = findTopLevelWindowAtDisplay (d, event->xproperty.window);
	if (w)
	{
	    DECOR_WINDOW (w);
	    if (dw->inputFrame &&
		dw->inputFrame == event->xdestroywindow.window)
	    {
		XDeleteProperty (d->display, w->id,
				 dd->inputFrameAtom);
		dw->inputFrame = None;
	    }
	}
	break;
    default:
	if (d->shapeExtension && event->type == d->shapeEvent + ShapeNotify)
	{
	    w = findWindowAtDisplay (d, ((XShapeEvent *) event)->window);
	    if (w)
		decorWindowUpdate (w, TRUE);
	}
	break;
    }
}

static Bool
decorDamageWindowRect (CompWindow *w,
		       Bool	  initial,
		       BoxPtr     rect)
{
    Bool status;

    DECOR_SCREEN (w->screen);

    if (initial)
	decorWindowUpdate (w, TRUE);

    UNWRAP (ds, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ds, w->screen, damageWindowRect, decorDamageWindowRect);

    return status;
}

static unsigned int
decorAddSupportedAtoms (CompScreen   *s,
			Atom         *atoms,
			unsigned int size)
{
    unsigned int count;

    DECOR_DISPLAY (s->display);
    DECOR_SCREEN (s);

    UNWRAP (ds, s, addSupportedAtoms);
    count = (*s->addSupportedAtoms) (s, atoms, size);
    WRAP (ds, s, addSupportedAtoms, decorAddSupportedAtoms);

    if (count < size)
	atoms[count++] = dd->requestFrameExtentsAtom;

    return count;
}

static void
decorGetOutputExtentsForWindow (CompWindow	  *w,
				CompWindowExtents *output)
{
    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    UNWRAP (ds, w->screen, getOutputExtentsForWindow);
    (*w->screen->getOutputExtentsForWindow) (w, output);
    WRAP (ds, w->screen, getOutputExtentsForWindow,
	  decorGetOutputExtentsForWindow);

    if (dw->wd)
    {
	CompWindowExtents *e = &dw->wd->decor->output;

	if (e->left > output->left)
	    output->left = e->left;
	if (e->right > output->right)
	    output->right = e->right;
	if (e->top > output->top)
	    output->top = e->top;
	if (e->bottom > output->bottom)
	    output->bottom = e->bottom;
    }
}

static CompBool
decorStartDecorator (void *closure)
{
    CompScreen *s = (CompScreen *) closure;

    DECOR_DISPLAY (s->display);
    DECOR_SCREEN (s);

    ds->decoratorStartHandle = 0;

    if (!ds->dmWin)
	runCommand (s, dd->opt[DECOR_DISPLAY_OPTION_COMMAND].value.s);

    return FALSE;
}

static CompOption *
decorGetDisplayOptions (CompPlugin  *plugin,
			CompDisplay *display,
			int	    *count)
{
    DECOR_DISPLAY (display);

    *count = NUM_OPTIONS (dd);
    return dd->opt;
}

static Bool
decorSetDisplayOption (CompPlugin      *plugin,
		       CompDisplay     *display,
		       const char      *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    DECOR_DISPLAY (display);

    o = compFindOption (dd->opt, NUM_OPTIONS (dd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case DECOR_DISPLAY_OPTION_COMMAND:
	if (compSetStringOption (o, value))
	{
	    CompScreen *s;

	    for (s = display->screens; s; s = s->next)
	    {
		DECOR_SCREEN (s);

		if (!ds->dmWin)
		    runCommand (s, o->value.s);
	    }

	    return TRUE;
	}
	break;
    case DECOR_DISPLAY_OPTION_SHADOW_MATCH:
	{
	    char *matchString;

	    /*
	       Make sure RGBA matching is always present and disable shadows
	       for RGBA windows by default if the user didn't specify an
	       RGBA match.
	       Reasoning for that is that shadows are desired for some RGBA
	       windows (e.g. rectangular windows that just happen to have an
	       RGBA colormap), while it's absolutely undesired for others
	       (especially shaped ones) ... by enforcing no shadows for RGBA
	       windows by default, we are flexible to user desires while still
	       making sure we don't show ugliness by default
	     */

	    matchString = matchToString (&value->match);
	    if (matchString)
	    {
		if (!strstr (matchString, "rgba="))
		{
		    CompMatch rgbaMatch;

		    matchInit (&rgbaMatch);
		    matchAddFromString (&rgbaMatch, "rgba=0");
		    matchAddGroup (&value->match, MATCH_OP_AND_MASK,
				   &rgbaMatch);
		    matchFini (&rgbaMatch);
		}
		free (matchString);
	    }
	}
	/* fall-through intended */
    case DECOR_DISPLAY_OPTION_DECOR_MATCH:
	if (compSetMatchOption (o, value))
	{
	    CompScreen *s;
	    CompWindow *w;

	    for (s = display->screens; s; s = s->next)
		for (w = s->windows; w; w = w->next)
		    decorWindowUpdate (w, TRUE);
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
decorWindowMoveNotify (CompWindow *w,
		       int	  dx,
		       int	  dy,
		       Bool	  immediate)
{
    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    if (dw->wd)
    {
	WindowDecoration *wd = dw->wd;
	int		 i;

	for (i = 0; i < wd->nQuad; i++)
	{
	    wd->quad[i].box.x1 += dx;
	    wd->quad[i].box.y1 += dy;
	    wd->quad[i].box.x2 += dx;
	    wd->quad[i].box.y2 += dy;
	}

	setDecorationMatrices (w);
    }

    UNWRAP (ds, w->screen, windowMoveNotify);
    (*w->screen->windowMoveNotify) (w, dx, dy, immediate);
    WRAP (ds, w->screen, windowMoveNotify, decorWindowMoveNotify);
}

static Bool
decorResizeUpdateTimeout (void *closure)
{
    CompWindow *w = (CompWindow *) closure;

    DECOR_WINDOW (w);

    decorWindowUpdate (w, TRUE);

    dw->resizeUpdateHandle = 0;

    return FALSE;
}

static void
decorWindowResizeNotify (CompWindow *w,
			 int	    dx,
			 int	    dy,
			 int	    dwidth,
			 int	    dheight)
{
    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    /* FIXME: we should not need a timer for calling decorWindowUpdate,
       and only call updateWindowDecorationScale if decorWindowUpdate
       returns FALSE. Unfortunately, decorWindowUpdate may call
       updateWindowOutputExtents, which may call WindowResizeNotify. As
       we never should call a wrapped function that's currently
       processed, we need the timer for the moment. updateWindowOutputExtents
       should be fixed so that it does not emit a resize notification. */
    dw->resizeUpdateHandle = compAddTimeout (0, 0, decorResizeUpdateTimeout, w);
    updateWindowDecorationScale (w);

    UNWRAP (ds, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w, dx, dy, dwidth, dheight);
    WRAP (ds, w->screen, windowResizeNotify, decorWindowResizeNotify);
}

static void
decorWindowStateChangeNotify (CompWindow   *w,
			      unsigned int lastState)
{
    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    if (!decorWindowUpdate (w, TRUE))
    {
	if (dw->wd && dw->wd->decor)
	{
	    int oldShiftX = decorWindowShiftX (w);
	    int oldShiftY = decorWindowShiftY (w);
	    int moveDx, moveDy;

	    if ((w->state & MAXIMIZE_STATE) == MAXIMIZE_STATE)
	    {
		setWindowFrameExtents (w, &dw->wd->decor->maxFrame);
		setWindowFrameExtents (w, &dw->wd->decor->maxBorder);
	    }
	    else
	    {
		setWindowFrameExtents (w, &dw->wd->decor->frame);
		setWindowFrameExtents (w, &dw->wd->decor->border);
	    }

	    /* since we immediately update the frame extents, we must
	       also update the stored saved window geometry in order
	       to prevent the window from shifting back too far once
	       unmaximized */

	    moveDx = decorWindowShiftX (w) - oldShiftX;
	    moveDy = decorWindowShiftY (w) - oldShiftY;

	    if (w->saveMask & CWX)
	        w->saveWc.x += moveDx;

	    if (w->saveMask & CWY)
	        w->saveWc.y += moveDy;

	    decorWindowUpdateFrame (w);
	}
    }

    UNWRAP (ds, w->screen, windowStateChangeNotify);
    (*w->screen->windowStateChangeNotify) (w, lastState);
    WRAP (ds, w->screen, windowStateChangeNotify, decorWindowStateChangeNotify);
}

static void
decorMatchPropertyChanged (CompDisplay *d,
			   CompWindow  *w)
{
    DECOR_DISPLAY (d);

    decorWindowUpdate (w, TRUE);

    UNWRAP (dd, d, matchPropertyChanged);
    (*d->matchPropertyChanged) (d, w);
    WRAP (dd, d, matchPropertyChanged, decorMatchPropertyChanged);
}

static void
decorWindowAdd (CompScreen *s,
		CompWindow *w)
{
    if (w->shaded || w->attrib.map_state == IsViewable)
	decorWindowUpdate (w, TRUE);
}

static void
decorWindowRemove (CompScreen *s,
		   CompWindow *w)
{
    if (!w->destroyed)
	decorWindowUpdate (w, FALSE);
}

static void
decorObjectAdd (CompObject *parent,
		CompObject *object)
{
    static ObjectAddProc dispTab[] = {
	(ObjectAddProc) 0, /* CoreAdd */
	(ObjectAddProc) 0, /* DisplayAdd */
	(ObjectAddProc) 0, /* ScreenAdd */
	(ObjectAddProc) decorWindowAdd
    };

    DECOR_CORE (&core);

    UNWRAP (dc, &core, objectAdd);
    (*core.objectAdd) (parent, object);
    WRAP (dc, &core, objectAdd, decorObjectAdd);

    DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), (parent, object));
}

static void
decorObjectRemove (CompObject *parent,
		   CompObject *object)
{
    static ObjectRemoveProc dispTab[] = {
	(ObjectRemoveProc) 0, /* CoreRemove */
	(ObjectRemoveProc) 0, /* DisplayRemove */
	(ObjectRemoveProc) 0, /* ScreenRemove */
	(ObjectRemoveProc) decorWindowRemove
    };

    DECOR_CORE (&core);

    DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), (parent, object));

    UNWRAP (dc, &core, objectRemove);
    (*core.objectRemove) (parent, object);
    WRAP (dc, &core, objectRemove, decorObjectRemove);
}

static Bool
decorInitCore (CompPlugin *p,
	       CompCore   *c)
{
    DecorCore *dc;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    dc = malloc (sizeof (DecorCore));
    if (!dc)
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	free (dc);
	return FALSE;
    }

    WRAP (dc, c, objectAdd, decorObjectAdd);
    WRAP (dc, c, objectRemove, decorObjectRemove);

    c->base.privates[corePrivateIndex].ptr = dc;

    return TRUE;
}

static void
decorFiniCore (CompPlugin *p,
	       CompCore   *c)
{
    DECOR_CORE (c);

    freeDisplayPrivateIndex (displayPrivateIndex);

    UNWRAP (dc, c, objectAdd);
    UNWRAP (dc, c, objectRemove);

    free (dc);
}

static const CompMetadataOptionInfo decorDisplayOptionInfo[] = {
    { "shadow_radius", "float", "<min>0.0</min><max>48.0</max>", 0, 0 },
    { "shadow_opacity", "float", "<min>0.0</min>", 0, 0 },
    { "shadow_color", "color", 0, 0, 0 },
    { "shadow_x_offset", "int", "<min>-16</min><max>16</max>", 0, 0 },
    { "shadow_y_offset", "int", "<min>-16</min><max>16</max>", 0, 0 },
    { "command", "string", 0, 0, 0 },
    { "mipmap", "bool", 0, 0, 0 },
    { "decoration_match", "match", 0, 0, 0 },
    { "shadow_match", "match", 0, 0, 0 }
};

static Bool
decorInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    DecorDisplay *dd;

    dd = malloc (sizeof (DecorDisplay));
    if (!dd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &decorMetadata,
					     decorDisplayOptionInfo,
					     dd->opt,
					     DECOR_DISPLAY_OPTION_NUM))
    {
	free (dd);
	return FALSE;
    }

    dd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (dd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, dd->opt, DECOR_DISPLAY_OPTION_NUM);
	free (dd);
	return FALSE;
    }

    dd->textures = 0;

    dd->supportingDmCheckAtom =
	XInternAtom (d->display, DECOR_SUPPORTING_DM_CHECK_ATOM_NAME, 0);
    dd->winDecorAtom =
	XInternAtom (d->display, DECOR_WINDOW_ATOM_NAME, 0);
    dd->decorAtom[DECOR_BARE] =
	XInternAtom (d->display, DECOR_BARE_ATOM_NAME, 0);
    dd->decorAtom[DECOR_ACTIVE] =
	XInternAtom (d->display, DECOR_ACTIVE_ATOM_NAME, 0);
    dd->inputFrameAtom =
	XInternAtom (d->display, DECOR_INPUT_FRAME_ATOM_NAME, 0);
    dd->requestFrameExtentsAtom =
	XInternAtom (d->display, "_NET_REQUEST_FRAME_EXTENTS", 0);

    WRAP (dd, d, handleEvent, decorHandleEvent);
    WRAP (dd, d, matchPropertyChanged, decorMatchPropertyChanged);

    d->base.privates[displayPrivateIndex].ptr = dd;

    return TRUE;
}

static void
decorFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    DECOR_DISPLAY (d);

    freeScreenPrivateIndex (d, dd->screenPrivateIndex);

    UNWRAP (dd, d, handleEvent);
    UNWRAP (dd, d, matchPropertyChanged);

    compFiniDisplayOptions (d, dd->opt, DECOR_DISPLAY_OPTION_NUM);

    free (dd);
}

static Bool
decorInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    int         i;
    DecorScreen *ds;

    DECOR_DISPLAY (s->display);

    ds = malloc (sizeof (DecorScreen));
    if (!ds)
	return FALSE;

    ds->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ds->windowPrivateIndex < 0)
    {
	free (ds);
	return FALSE;
    }

    for (i = 0; i < DECOR_NUM; i++)
    {
	ds->decors[i]   = NULL;
	ds->decorNum[i] = 0;
    }

    ds->dmWin		     = None;
    ds->decoratorStartHandle = 0;

    WRAP (ds, s, drawWindow, decorDrawWindow);
    WRAP (ds, s, damageWindowRect, decorDamageWindowRect);
    WRAP (ds, s, getOutputExtentsForWindow, decorGetOutputExtentsForWindow);
    WRAP (ds, s, windowMoveNotify, decorWindowMoveNotify);
    WRAP (ds, s, windowResizeNotify, decorWindowResizeNotify);
    WRAP (ds, s, windowStateChangeNotify, decorWindowStateChangeNotify);
    WRAP (ds, s, addSupportedAtoms, decorAddSupportedAtoms);

    s->base.privates[dd->screenPrivateIndex].ptr = ds;

    decorCheckForDmOnScreen (s, FALSE);
    setSupportedWmHints (s);

    if (!ds->dmWin)
	ds->decoratorStartHandle = compAddTimeout (0, -1,
						   decorStartDecorator, s);

    return TRUE;
}

static void
decorFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    int	i;

    DECOR_SCREEN (s);

    for (i = 0; i < DECOR_NUM; i++)
    {
	if (ds->decors[i] && ds->decorNum[i] > 0)
	    decorReleaseDecorations (s, ds->decors[i], &ds->decorNum[i]);
    }

    if (ds->decoratorStartHandle)
	compRemoveTimeout (ds->decoratorStartHandle);

    freeWindowPrivateIndex (s, ds->windowPrivateIndex);

    UNWRAP (ds, s, drawWindow);
    UNWRAP (ds, s, damageWindowRect);
    UNWRAP (ds, s, getOutputExtentsForWindow);
    UNWRAP (ds, s, windowMoveNotify);
    UNWRAP (ds, s, windowResizeNotify);
    UNWRAP (ds, s, windowStateChangeNotify);
    UNWRAP (ds, s, addSupportedAtoms);

    setSupportedWmHints (s);

    free (ds);
}

static Bool
decorInitWindow (CompPlugin *p,
		 CompWindow *w)
{
    DecorWindow *dw;

    DECOR_SCREEN (w->screen);

    dw = malloc (sizeof (DecorWindow));
    if (!dw)
	return FALSE;

    dw->wd	   = NULL;
    dw->decors     = NULL;
    dw->decorNum   = 0;
    dw->inputFrame = None;

    dw->resizeUpdateHandle = 0;

    w->base.privates[ds->windowPrivateIndex].ptr = dw;

    if (!w->attrib.override_redirect)
	decorWindowUpdateDecoration (w);

    if (w->base.parent)
	decorWindowAdd (w->screen, w);

    return TRUE;
}

static void
decorFiniWindow (CompPlugin *p,
		 CompWindow *w)
{
    DECOR_WINDOW (w);

    if (dw->resizeUpdateHandle)
	compRemoveTimeout (dw->resizeUpdateHandle);

    if (w->base.parent)
	decorWindowRemove (w->screen, w);

    if (dw->wd)
	destroyWindowDecoration (w->screen, dw->wd);

    if (dw->decors && dw->decorNum > 0)
	decorReleaseDecorations (w->screen, dw->decors, &dw->decorNum);

    free (dw);
}

static CompBool
decorInitObject (CompPlugin *p,
		 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) decorInitCore,
	(InitPluginObjectProc) decorInitDisplay,
	(InitPluginObjectProc) decorInitScreen,
	(InitPluginObjectProc) decorInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
decorFiniObject (CompPlugin *p,
		 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) decorFiniCore,
	(FiniPluginObjectProc) decorFiniDisplay,
	(FiniPluginObjectProc) decorFiniScreen,
	(FiniPluginObjectProc) decorFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
decorGetObjectOptions (CompPlugin *plugin,
		       CompObject *object,
		       int	  *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) decorGetDisplayOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
decorSetObjectOption (CompPlugin      *plugin,
		      CompObject      *object,
		      const char      *name,
		      CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) decorSetDisplayOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
decorInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&decorMetadata,
					 p->vTable->name,
					 decorDisplayOptionInfo,
					 DECOR_DISPLAY_OPTION_NUM,
					 0, 0))
	return FALSE;

    corePrivateIndex = allocateCorePrivateIndex ();
    if (corePrivateIndex < 0)
    {
	compFiniMetadata (&decorMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&decorMetadata, p->vTable->name);

    return TRUE;
}

static void
decorFini (CompPlugin *p)
{
    freeCorePrivateIndex (corePrivateIndex);
    compFiniMetadata (&decorMetadata);
}

static CompMetadata *
decorGetMetadata (CompPlugin *plugin)
{
    return &decorMetadata;
}

static CompPluginVTable decorVTable = {
    "decoration",
    decorGetMetadata,
    decorInit,
    decorFini,
    decorInitObject,
    decorFiniObject,
    decorGetObjectOptions,
    decorSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &decorVTable;
}
