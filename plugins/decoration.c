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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <compiz.h>

#define GRAVITY_WEST  (0)
#define GRAVITY_EAST  (1 << 0)
#define GRAVITY_NORTH (0)
#define GRAVITY_SOUTH (1 << 1)

#define ALIGN_LEFT   (0)
#define ALIGN_RIGHT  (1 << 0)
#define ALIGN_TOP    (0)
#define ALIGN_BOTTOM (1 << 1)

#define XX_MASK (1 << 6)
#define XY_MASK (1 << 7)
#define YX_MASK (1 << 8)
#define YY_MASK (1 << 9)

typedef struct _Point {
    int	x;
    int	y;
    int gravity;
} Point;

typedef struct _Vector {
    int	dx;
    int	dy;
    int	x0;
    int	y0;
} Vector;

typedef struct _Quad {
    Point      p1;
    Point      p2;
    int	       maxWidth;
    int	       maxHeight;
    int	       align;
    CompMatrix m;
} Quad;

#define DECOR_BARE   0
#define DECOR_NORMAL 1
#define DECOR_ACTIVE 2
#define DECOR_NUM    3

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
    CompWindowExtents input;
    Quad	      *quad;
    int		      nQuad;
} Decoration;

typedef struct _ScaledQuad {
    CompMatrix matrix;
    BoxRec     box;
} ScaledQuad;

typedef struct _WindowDecoration {
    Decoration *decor;
    ScaledQuad *quad;
    int	       nQuad;
} WindowDecoration;

static int displayPrivateIndex;

typedef struct _DecorDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
    DecorTexture    *textures;
    Atom	    supportingDmCheckAtom;
    Atom	    winDecorAtom;
    Atom	    winDecorBareAtom;
    Atom	    winDecorNormalAtom;
    Atom	    winDecorActiveAtom;
} DecorDisplay;

typedef struct _DecorScreen {
    int	windowPrivateIndex;

    Window dmWin;

    Decoration *decor[DECOR_NUM];

    PaintWindowProc	 paintWindow;
    DamageWindowRectProc damageWindowRect;

    WindowMoveNotifyProc   windowMoveNotify;
    WindowResizeNotifyProc windowResizeNotify;
} DecorScreen;

typedef struct _DecorWindow {
    WindowDecoration *wd;
    Decoration	     *decor;
} DecorWindow;

#define GET_DECOR_DISPLAY(d)				      \
    ((DecorDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define DECOR_DISPLAY(d)		     \
    DecorDisplay *dd = GET_DECOR_DISPLAY (d)

#define GET_DECOR_SCREEN(s, dd)				          \
    ((DecorScreen *) (s)->privates[(dd)->screenPrivateIndex].ptr)

#define DECOR_SCREEN(s)							   \
    DecorScreen *ds = GET_DECOR_SCREEN (s, GET_DECOR_DISPLAY (s->display))

#define GET_DECOR_WINDOW(w, ds)				   \
    ((DecorWindow *) (w)->privates[(ds)->windowPrivateIndex].ptr)

#define DECOR_WINDOW(w)					       \
    DecorWindow *dw = GET_DECOR_WINDOW  (w,		       \
		      GET_DECOR_SCREEN  (w->screen,	       \
		      GET_DECOR_DISPLAY (w->screen->display)))


static Bool
decorPaintWindow (CompWindow		  *w,
		  const WindowPaintAttrib *attrib,
		  Region		  region,
		  unsigned int		  mask)
{
    Bool status;

    DECOR_SCREEN (w->screen);

    if (!(mask & PAINT_WINDOW_SOLID_MASK))
    {
	DECOR_WINDOW (w);

	if (dw->wd)
	{
	    WindowDecoration *wd = dw->wd;
	    REGION	     box;
	    int		     i;

	    if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
		region = &infiniteRegion;

	    box.rects	 = &box.extents;
	    box.numRects = 1;

	    if (w->alpha && wd->decor == ds->decor[DECOR_BARE])
	    {
		if (attrib->saturation == COLOR)
		{
		    CompMatrix matrix[2];

		    if (!w->texture.pixmap)
			bindWindow (w);

		    matrix[1] = w->texture.matrix;

		    w->vCount = 0;

		    for (i = 0; i < wd->nQuad; i++)
		    {
			box.extents = wd->quad[i].box;

			if (box.extents.x1 < box.extents.x2 &&
			    box.extents.y1 < box.extents.y2)
			{
			    matrix[0] = wd->quad[i].matrix;

			    (*w->screen->addWindowGeometry) (w,
							     matrix, 2,
							     &box,
							     region);
			}
		    }

		    if (w->vCount)
		    {
			int filter;

			glEnable (GL_BLEND);

			glPushMatrix ();

			if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
			{
			    glTranslatef (w->attrib.x, w->attrib.y, 0.0f);
			    glScalef (attrib->xScale, attrib->yScale, 0.0f);
			    glTranslatef (-w->attrib.x, -w->attrib.y, 0.0f);

			    filter = w->screen->filter[WINDOW_TRANS_FILTER];
			}
			else if (mask & PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK)
			{
			    filter = w->screen->filter[SCREEN_TRANS_FILTER];
			}
			else
			{
			    filter = w->screen->filter[NOTHING_TRANS_FILTER];
			}

			enableTexture (w->screen, &wd->decor->texture->texture,
				       filter);

			if (attrib->opacity    != OPAQUE ||
			    attrib->brightness != BRIGHT)
			{
			    GLushort color;

			    color = (attrib->opacity *
				     attrib->brightness) >> 16;

			    screenTexEnvMode (w->screen, GL_MODULATE);
			    glColor4us (color, color, color, attrib->opacity);
			}

			w->screen->activeTexture (GL_TEXTURE1_ARB);

			enableTexture (w->screen, &w->texture, filter);

			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
				   GL_MODULATE);

			w->screen->clientActiveTexture (GL_TEXTURE1_ARB);
			glEnableClientState (GL_TEXTURE_COORD_ARRAY);
			w->screen->clientActiveTexture (GL_TEXTURE0_ARB);

			(*w->screen->drawWindowGeometry) (w);

			disableTexture (&w->texture);
			w->screen->activeTexture (GL_TEXTURE0_ARB);

			w->screen->clientActiveTexture (GL_TEXTURE1_ARB);
			glDisableClientState (GL_TEXTURE_COORD_ARRAY);
			w->screen->clientActiveTexture (GL_TEXTURE0_ARB);

			disableTexture (&wd->decor->texture->texture);

			glPopMatrix ();

			if (attrib->opacity    != OPAQUE ||
			    attrib->brightness != BRIGHT)
			{
			    glColor4usv (defaultColor);
			    screenTexEnvMode (w->screen, GL_REPLACE);
			}

			glDisable (GL_BLEND);
		    }
		}
	    }
	    else
	    {
		w->vCount = 0;

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
		    drawWindowTexture (w, &wd->decor->texture->texture, attrib,
				       mask | PAINT_WINDOW_TRANSLUCENT_MASK);
	    }
	}
    }

    UNWRAP (ds, w->screen, paintWindow);
    status = (*w->screen->paintWindow) (w, attrib, region, mask);
    WRAP (ds, w->screen, paintWindow, decorPaintWindow);

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

/*
  decoration property
  -------------------

  data[0] = pixmap

  data[1] = input left
  data[2] = input right
  data[3] = input top
  data[4] = input bottom

  flags

  1st and 2nd bit p1 gravity, 3rd and 4th bit p2 gravity,
  5rd and 6th bit alignment, 7th bit XX, 8th bit XY, 9th bit YX, 10th bit YY.

  data[4 + n * 9 + 1] = flags
  data[4 + n * 9 + 2] = p1 x
  data[4 + n * 9 + 3] = p1 y
  data[4 + n * 9 + 4] = p2 x
  data[4 + n * 9 + 5] = p2 y
  data[4 + n * 9 + 6] = widthMax
  data[4 + n * 9 + 7] = heightMax
  data[4 + n * 9 + 8] = x0
  data[4 + n * 9 + 9] = y0
 */
static Decoration *
decorCreateDecoration (CompScreen *screen,
		       Window	  id,
		       Atom	  decorAtom)
{
    Decoration	  *decoration;
    Atom	  actual;
    int		  result, format;
    unsigned long n, nleft;
    unsigned char *data;
    long	  *prop;
    Pixmap	  pixmap;
    Quad	  *quad;
    int		  nQuad;
    int		  left, right, top, bottom;
    int		  flags;

    result = XGetWindowProperty (screen->display->display, id,
				 decorAtom, 0L, 1024L, FALSE,
				 XA_INTEGER, &actual, &format,
				 &n, &nleft, &data);

    if (result != Success || !n || !data)
	return NULL;

    if (n < 9 + 14)
    {
	XFree (data);
	return NULL;
    }

    decoration = malloc (sizeof (Decoration));
    if (!decoration)
    {
	XFree (data);
	return NULL;
    }

    prop = (long *) data;

    memcpy (&pixmap, prop++, sizeof (Pixmap));

    decoration->texture = decorGetTexture (screen, pixmap);
    if (!decoration->texture)
    {
	free (decoration);
	XFree (data);
	return NULL;
    }

    decoration->input.left   = *prop++;
    decoration->input.right  = *prop++;
    decoration->input.top    = *prop++;
    decoration->input.bottom = *prop++;

    nQuad = (n - 5) / 9;

    quad = malloc (sizeof (Quad) * nQuad);
    if (!quad)
    {
	decorReleaseTexture (screen, decoration->texture);
	free (decoration);
	XFree (data);
	return NULL;
    }

    decoration->quad  = quad;
    decoration->nQuad = nQuad;

    left = right = top = bottom = 0;

    while (nQuad--)
    {
	flags = *prop++;

	quad->p1.gravity = (flags >> 0) & 0x3;
	quad->p2.gravity = (flags >> 2) & 0x3;

	quad->align = (flags >> 4) & 0x3;

	quad->m.xx = (flags & XX_MASK) ? 1.0f : 0.0f;
	quad->m.xy = (flags & XY_MASK) ? 1.0f : 0.0f;
	quad->m.yx = (flags & YX_MASK) ? 1.0f : 0.0f;
	quad->m.yy = (flags & YY_MASK) ? 1.0f : 0.0f;

	quad->p1.x = *prop++;
	quad->p1.y = *prop++;
	quad->p2.x = *prop++;
	quad->p2.y = *prop++;

	quad->maxWidth  = *prop++;
	quad->maxHeight = *prop++;

	quad->m.x0 = *prop++;
	quad->m.y0 = *prop++;

	if (quad->p1.x < left)
	    left = quad->p1.x;
	if (quad->p1.y < top)
	    top = quad->p1.y;
	if (quad->p2.x > right)
	    right = quad->p2.x;
	if (quad->p2.y > bottom)
	    bottom = quad->p2.y;

	quad++;
    }

    XFree (data);

    decoration->output.left   = -left;
    decoration->output.right  = right;
    decoration->output.top    = -top;
    decoration->output.bottom = bottom;

    decoration->refCount = 1;

    return decoration;
}

static void
decorReleaseDecoration (CompScreen *screen,
			Decoration *decoration)
{
    decoration->refCount--;
    if (decoration->refCount)
	return;

    decorReleaseTexture (screen, decoration->texture);
    free (decoration->quad);
}

static void
decorWindowUpdateDecoration (CompWindow *w)
{
    Decoration *decoration;

    DECOR_DISPLAY (w->screen->display);
    DECOR_WINDOW (w);

    decoration = decorCreateDecoration (w->screen, w->id, dd->winDecorAtom);

    if (dw->decor)
	decorReleaseDecoration (w->screen, dw->decor);

    dw->decor = decoration;
}

static WindowDecoration *
createWindowDecoration (Decoration *d)
{
    WindowDecoration *wd;

    wd = malloc (sizeof (WindowDecoration) +
		 sizeof (ScaledQuad) * d->nQuad);
    if (!wd)
	return NULL;

    wd->decor = d;
    wd->quad  = (ScaledQuad *) (wd + 1);
    wd->nQuad = d->nQuad;

    return wd;
}

static void
destroyWindowDecoration (WindowDecoration *wd)
{
    free (wd);
}

static void
setDecorationMatrices (CompWindow *w)
{
    WindowDecoration *wd;
    int		     i;

    DECOR_WINDOW (w);

    wd = dw->wd;
    if (!wd)
	return;

    for (i = 0; i < wd->nQuad; i++)
    {
	wd->quad[i].matrix = wd->decor->texture->texture.matrix;

	wd->quad[i].matrix.x0 += wd->decor->quad[i].m.x0 * wd->quad[i].matrix.xx;
	wd->quad[i].matrix.y0 += wd->decor->quad[i].m.y0 * wd->quad[i].matrix.yy;

	wd->quad[i].matrix.xx *= wd->decor->quad[i].m.xx;
	wd->quad[i].matrix.yy *= wd->decor->quad[i].m.yy;
	wd->quad[i].matrix.xy *= wd->decor->quad[i].m.xy;
	wd->quad[i].matrix.yx *= wd->decor->quad[i].m.yx;

	wd->quad[i].matrix.x0 -=
	    wd->quad[i].box.x1 * wd->quad[i].matrix.xx +
	    wd->quad[i].box.y1 * wd->quad[i].matrix.xy;

	wd->quad[i].matrix.y0 -=
	    wd->quad[i].box.y1 * wd->quad[i].matrix.yy +
	    wd->quad[i].box.x1 * wd->quad[i].matrix.yx;
    }
}

static void
applyGravity (int gravity,
	      int x,
	      int y,
	      int width,
	      int height,
	      int *return_x,
	      int *return_y)
{
    if (gravity & GRAVITY_EAST)
	*return_x = x + width;
    else
	*return_x = x;

    if (gravity & GRAVITY_SOUTH)
	*return_y = y + height;
    else
	*return_y = y;
}

static void
updateWindowDecorationScale (CompWindow *w)
{
    WindowDecoration *wd;
    int		     x1, y1, x2, y2;
    int		     maxWidth, maxHeight;
    int		     align;
    int		     i;

    DECOR_WINDOW (w);

    wd = dw->wd;
    if (!wd)
	return;

    for (i = 0; i < wd->nQuad; i++)
    {
	applyGravity (wd->decor->quad[i].p1.gravity,
		      wd->decor->quad[i].p1.x, wd->decor->quad[i].p1.y,
		      w->width, w->height,
		      &x1, &y1);

	applyGravity (wd->decor->quad[i].p2.gravity,
		      wd->decor->quad[i].p2.x, wd->decor->quad[i].p2.y,
		      w->width, w->height,
		      &x2, &y2);

	maxWidth  = wd->decor->quad[i].maxWidth;
	maxHeight = wd->decor->quad[i].maxHeight;
	align	  = wd->decor->quad[i].align;

	if (maxWidth < x2 - x1)
	{
	    if (align & ALIGN_RIGHT)
		x1 = x2 - maxWidth;
	    else
		x2 = x1 + maxWidth;
	}

	if (maxHeight < y2 - y1)
	{
	    if (align & ALIGN_BOTTOM)
		y1 = y2 - maxHeight;
	    else
		y2 = y1 + maxHeight;
	}

	wd->quad[i].box.x1 = x1 + w->attrib.x;
	wd->quad[i].box.y1 = y1 + w->attrib.y;
	wd->quad[i].box.x2 = x2 + w->attrib.x;
	wd->quad[i].box.y2 = y2 + w->attrib.y;
    }

    setDecorationMatrices (w);
}

static Bool
decorWindowUpdate (CompWindow *w,
		   Bool	      move)
{
    WindowDecoration *wd;
    Decoration	     *old, *decor = NULL;
    int		     dx, dy;

    DECOR_SCREEN (w->screen);
    DECOR_WINDOW (w);

    wd = dw->wd;
    old = (wd) ? wd->decor : NULL;

    if (dw->decor)
    {
	decor = dw->decor;
    }
    else
    {
	if (w->attrib.override_redirect)
	{
	    if (w->region->numRects == 1)
		decor = ds->decor[DECOR_BARE];
	}
	else
	{
	    switch (w->type) {
	    case CompWindowTypeDialogMask:
	    case CompWindowTypeModalDialogMask:
	    case CompWindowTypeUtilMask:
	    case CompWindowTypeNormalMask:
		if (w->mwmDecor & (MwmDecorAll | MwmDecorTitle))
		{
		    if (w->id == w->screen->display->activeWindow)
			decor = ds->decor[DECOR_ACTIVE];
		    else
			decor = ds->decor[DECOR_NORMAL];

		    break;
		}
		/* fall-through */
	    case CompWindowTypeSplashMask:
	    case CompWindowTypeToolbarMask:
	    case CompWindowTypeMenuMask:
	    case CompWindowTypeUnknownMask:
	    case CompWindowTypeDockMask:
		if (w->region->numRects == 1)
		    decor = ds->decor[DECOR_BARE];
		break;
	    default:
		break;
	    }
	}
    }

    if (!ds->dmWin)
	decor = NULL;

    if (decor == old)
	return FALSE;

    if (old)
    {
	damageWindowOutputExtents (w);

	if (wd->decor == dw->decor)
	{
	    decorReleaseDecoration (w->screen, dw->decor);
	    dw->decor = NULL;
	}

	destroyWindowDecoration (wd);
    }

    if (decor)
    {
	dx = decor->input.left;
	dy = decor->input.top;
    }
    else
	dx = dy = 0;

    dx -= w->input.left;
    dy -= w->input.top;

    /* if (dx == 0 && dy == 0) */
	move = FALSE;

    if (decor)
    {
	dw->wd = createWindowDecoration (decor);
	if (!dw->wd)
	    return FALSE;

	setWindowFrameExtents (w, &decor->input, &decor->output);

	if (!move)
	    damageWindowOutputExtents (w);

	updateWindowDecorationScale (w);
    }
    else
    {
	dw->wd = NULL;
    }

    if (move)
	moveWindow (w, dx, dy, TRUE);

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

    if (result == Success && n && data)
    {
	XWindowAttributes attr;

	memcpy (&dmWin, data, sizeof (Window));
	XFree (data);

	compCheckForError (d->display);

	XGetWindowAttributes (d->display, dmWin, &attr);

	if (compCheckForError (d->display))
	    dmWin = None;
    }

    if (dmWin != ds->dmWin)
    {
	CompWindow *w;

	if (dmWin)
	{
	    ds->decor[DECOR_BARE] =
		decorCreateDecoration (s, s->root, dd->winDecorBareAtom);

	    ds->decor[DECOR_NORMAL] =
		decorCreateDecoration (s, s->root, dd->winDecorNormalAtom);

	    ds->decor[DECOR_ACTIVE] =
		decorCreateDecoration (s, s->root, dd->winDecorActiveAtom);
	}
	else
	{
	    int i;

	    for (i = 0; i < DECOR_NUM; i++)
	    {
		if (ds->decor[i])
		{
		    decorReleaseDecoration (s, ds->decor[i]);
		    ds->decor[i] = 0;
		}
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
    Window     activeWindow = 0;
    CompWindow *w;

    DECOR_DISPLAY (d);

    switch (event->type) {
    case PropertyNotify:
	if (event->xproperty.atom == d->winActiveAtom)
	    activeWindow = d->activeWindow;
	break;
    case DestroyNotify:
	w = findWindowAtDisplay (d, event->xdestroywindow.window);
	if (w)
	{
	    DECOR_SCREEN (w->screen);

	    if (w->id == ds->dmWin)
		decorCheckForDmOnScreen (w->screen, TRUE);
	}
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
			    if (!w->attrib.override_redirect && w->mapNum)
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

    switch (event->type) {
    case PropertyNotify:
	if (event->xproperty.atom == d->winActiveAtom)
	{
	    if (d->activeWindow != activeWindow)
	    {
		w = findWindowAtDisplay (d, activeWindow);
		if (w)
		    decorWindowUpdate (w, FALSE);

		w = findWindowAtDisplay (d, d->activeWindow);
		if (w)
		    decorWindowUpdate (w, FALSE);
	    }
	}
	else if (event->xproperty.atom == dd->winDecorAtom)
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
	else if (event->xproperty.atom == dd->supportingDmCheckAtom)
	{
	    CompScreen *s;

	    s = findScreenAtDisplay (d, event->xproperty.window);
	    if (s)
		decorCheckForDmOnScreen (s, TRUE);
	}
	break;
    case MapRequest:
	w = findWindowAtDisplay (d, event->xmaprequest.window);
	if (w)
	    decorWindowUpdate (w, TRUE);
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
	decorWindowUpdate (w, FALSE);

    UNWRAP (ds, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ds, w->screen, damageWindowRect, decorDamageWindowRect);

    return status;
}

static void
decorWindowMoveNotify (CompWindow *w,
		       int	  dx,
		       int	  dy)
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
    (*w->screen->windowMoveNotify) (w, dx, dy);
    WRAP (ds, w->screen, windowMoveNotify, decorWindowMoveNotify);
}

static void
decorWindowResizeNotify (CompWindow *w)
{
    DECOR_SCREEN (w->screen);

    if (!decorWindowUpdate (w, FALSE))
	updateWindowDecorationScale (w);

    UNWRAP (ds, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w);
    WRAP (ds, w->screen, windowResizeNotify, decorWindowResizeNotify);
}

static Bool
decorInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    DecorDisplay *dd;

    dd = malloc (sizeof (DecorDisplay));
    if (!dd)
	return FALSE;

    dd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (dd->screenPrivateIndex < 0)
    {
	free (dd);
	return FALSE;
    }

    dd->textures = 0;

    dd->supportingDmCheckAtom =
	XInternAtom (d->display, "_NET_SUPPORTING_DM_CHECK", 0);
    dd->winDecorAtom = XInternAtom (d->display, "_NET_WINDOW_DECOR", 0);
    dd->winDecorBareAtom =
	XInternAtom (d->display, "_NET_WINDOW_DECOR_BARE", 0);
    dd->winDecorNormalAtom =
	XInternAtom (d->display, "_NET_WINDOW_DECOR_NORMAL", 0);
    dd->winDecorActiveAtom =
	XInternAtom (d->display, "_NET_WINDOW_DECOR_ACTIVE", 0);

    WRAP (dd, d, handleEvent, decorHandleEvent);

    d->privates[displayPrivateIndex].ptr = dd;

    return TRUE;
}

static void
decorFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    DECOR_DISPLAY (d);

    freeScreenPrivateIndex (d, dd->screenPrivateIndex);

    UNWRAP (dd, d, handleEvent);

    free (dd);
}

static Bool
decorInitScreen (CompPlugin *p,
		 CompScreen *s)
{
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

    memset (ds->decor, 0, sizeof (ds->decor));

    ds->dmWin = None;

    WRAP (ds, s, paintWindow, decorPaintWindow);
    WRAP (ds, s, damageWindowRect, decorDamageWindowRect);
    WRAP (ds, s, windowMoveNotify, decorWindowMoveNotify);
    WRAP (ds, s, windowResizeNotify, decorWindowResizeNotify);

    s->privates[dd->screenPrivateIndex].ptr = ds;

    decorCheckForDmOnScreen (s, FALSE);

    return TRUE;
}

static void
decorFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    int i;

    DECOR_SCREEN (s);

    for (i = 0; i < DECOR_NUM; i++)
	if (ds->decor[i])
	    decorReleaseDecoration (s, ds->decor[i]);

    UNWRAP (ds, s, paintWindow);
    UNWRAP (ds, s, damageWindowRect);
    UNWRAP (ds, s, windowMoveNotify);
    UNWRAP (ds, s, windowResizeNotify);

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

    dw->wd    = NULL;
    dw->decor = NULL;

    w->privates[ds->windowPrivateIndex].ptr = dw;

    if (w->attrib.map_state == IsViewable)
    {
	if (!w->attrib.override_redirect)
	    decorWindowUpdateDecoration (w);

	decorWindowUpdate (w, FALSE);
    }

    return TRUE;
}

static void
decorFiniWindow (CompPlugin *p,
		 CompWindow *w)
{
    DECOR_WINDOW (w);

    if (dw->wd)
	destroyWindowDecoration (dw->wd);

    if (dw->decor)
	decorReleaseDecoration (w->screen, dw->decor);

    free (dw);
}

static Bool
decorInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
decorFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginDep decorDeps[] = {
    { CompPluginRuleBefore, "wobbly" },
    { CompPluginRuleBefore, "fade" },
    { CompPluginRuleBefore, "cube" },
    { CompPluginRuleBefore, "expose" }
};

static CompPluginVTable decorVTable = {
    "decoration",
    "Window Decoration",
    "Window decorations",
    decorInit,
    decorFini,
    decorInitDisplay,
    decorFiniDisplay,
    decorInitScreen,
    decorFiniScreen,
    decorInitWindow,
    decorFiniWindow,
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    decorDeps,
    sizeof (decorDeps) / sizeof (decorDeps[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &decorVTable;
}
