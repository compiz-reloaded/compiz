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

#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <compiz.h>

#define GRAVITY_WEST  (1 << 0)
#define GRAVITY_EAST  (1 << 1)
#define GRAVITY_NORTH (1 << 2)
#define GRAVITY_SOUTH (1 << 3)

#define ALIGN_LEFT   (0)
#define ALIGN_RIGHT  (1 << 0)
#define ALIGN_TOP    (0)
#define ALIGN_BOTTOM (1 << 1)

#define CLAMP_HORZ (1 << 0)
#define CLAMP_VERT (1 << 1)

#define XX_MASK (1 << 12)
#define XY_MASK (1 << 13)
#define YX_MASK (1 << 14)
#define YY_MASK (1 << 15)

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
    int	       clamp;
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
    int		      minWidth;
    int		      minHeight;
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

#define DECOR_SHADOW_RADIUS_DEFAULT    8.0f
#define DECOR_SHADOW_RADIUS_MIN        0.0f
#define DECOR_SHADOW_RADIUS_MAX       48.0f
#define DECOR_SHADOW_RADIUS_PRECISION  0.1f

#define DECOR_SHADOW_OPACITY_DEFAULT   0.5f
#define DECOR_SHADOW_OPACITY_MIN       0.01f
#define DECOR_SHADOW_OPACITY_MAX       6.0f
#define DECOR_SHADOW_OPACITY_PRECISION 0.01f

#define DECOR_SHADOW_OFFSET_DEFAULT   1
#define DECOR_SHADOW_OFFSET_MIN     -16
#define DECOR_SHADOW_OFFSET_MAX      16

#define DECOR_DISPLAY_OPTION_SHADOW_RADIUS   0
#define DECOR_DISPLAY_OPTION_SHADOW_OPACITY  1
#define DECOR_DISPLAY_OPTION_SHADOW_OFFSET_X 2
#define DECOR_DISPLAY_OPTION_SHADOW_OFFSET_Y 3
#define DECOR_DISPLAY_OPTION_COMMAND         4
#define DECOR_DISPLAY_OPTION_NUM             5

static int displayPrivateIndex;

typedef struct _DecorDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
    DecorTexture    *textures;
    Atom	    supportingDmCheckAtom;
    Atom	    winDecorAtom;
    Atom	    decorAtom[DECOR_NUM];

    CompOption opt[DECOR_DISPLAY_OPTION_NUM];
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

#define GET_DECOR_WINDOW(w, ds)					  \
    ((DecorWindow *) (w)->privates[(ds)->windowPrivateIndex].ptr)

#define DECOR_WINDOW(w)					       \
    DecorWindow *dw = GET_DECOR_WINDOW  (w,		       \
		      GET_DECOR_SCREEN  (w->screen,	       \
		      GET_DECOR_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static CompOption *
decorGetDisplayOptions (CompDisplay *display,
			int	    *count)
{
    DECOR_DISPLAY (display);

    *count = NUM_OPTIONS (dd);
    return dd->opt;
}

static Bool
decorSetDisplayOption (CompDisplay     *display,
		       char	       *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    DECOR_DISPLAY (display);

    o = compFindOption (dd->opt, NUM_OPTIONS (dd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case DECOR_DISPLAY_OPTION_SHADOW_RADIUS:
    case DECOR_DISPLAY_OPTION_SHADOW_OPACITY:
	if (compSetFloatOption (o, value))
	    return TRUE;
	break;
    case DECOR_DISPLAY_OPTION_SHADOW_OFFSET_X:
    case DECOR_DISPLAY_OPTION_SHADOW_OFFSET_Y:
	if (compSetIntOption (o, value))
	    return TRUE;
	break;
    case DECOR_DISPLAY_OPTION_COMMAND:
	if (compSetStringOption (o, value))
	{
	    if (display->screens && *o->value.s != '\0')
	    {
		DECOR_SCREEN (display->screens);

		/* run decorator command if no decorator is present on
		   first screen */
		if (!ds->dmWin)
		{
		    if (fork () == 0)
		    {
			putenv (display->displayString);
			execl ("/bin/sh", "/bin/sh", "-c", o->value.s, NULL);
			exit (0);
		    }
		}
	    }

	    return TRUE;
	}
    default:
	break;
    }

    return FALSE;
}

static void
decorDisplayInitOptions (DecorDisplay *dd)
{
    CompOption *o;

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_RADIUS];
    o->name		= "shadow_radius";
    o->shortDesc	= N_("Shadow Radius");
    o->longDesc		= N_("Drop shadow radius");
    o->type		= CompOptionTypeFloat;
    o->value.f		= DECOR_SHADOW_RADIUS_DEFAULT;
    o->rest.f.min	= DECOR_SHADOW_RADIUS_MIN;
    o->rest.f.max	= DECOR_SHADOW_RADIUS_MAX;
    o->rest.f.precision = DECOR_SHADOW_RADIUS_PRECISION;

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_OPACITY];
    o->name		= "shadow_opacity";
    o->shortDesc	= N_("Shadow Opacity");
    o->longDesc		= N_("Drop shadow opacity");
    o->type		= CompOptionTypeFloat;
    o->value.f		= DECOR_SHADOW_OPACITY_DEFAULT;
    o->rest.f.min	= DECOR_SHADOW_OPACITY_MIN;
    o->rest.f.max	= DECOR_SHADOW_OPACITY_MAX;
    o->rest.f.precision = DECOR_SHADOW_OPACITY_PRECISION;

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_OFFSET_X];
    o->name		= "shadow_offset_x";
    o->shortDesc	= N_("Shadow Offset X");
    o->longDesc		= N_("Drop shadow X offset");
    o->type		= CompOptionTypeInt;
    o->value.i		= DECOR_SHADOW_OFFSET_DEFAULT;
    o->rest.i.min	= DECOR_SHADOW_OFFSET_MIN;
    o->rest.i.max	= DECOR_SHADOW_OFFSET_MAX;

    o = &dd->opt[DECOR_DISPLAY_OPTION_SHADOW_OFFSET_Y];
    o->name		= "shadow_offset_y";
    o->shortDesc	= N_("Shadow Offset Y");
    o->longDesc		= N_("Drop shadow Y offset");
    o->type		= CompOptionTypeInt;
    o->value.i		= DECOR_SHADOW_OFFSET_DEFAULT;
    o->rest.i.min	= DECOR_SHADOW_OFFSET_MIN;
    o->rest.i.max	= DECOR_SHADOW_OFFSET_MAX;

    o = &dd->opt[DECOR_DISPLAY_OPTION_COMMAND];
    o->name		= "command";
    o->shortDesc	= N_("Command");
    o->longDesc		= N_("Decorator command line that is executed if no "
			     "decorator is already running");
    o->type		= CompOptionTypeString;
    o->value.s		= strdup ("");
    o->rest.s.string	= NULL;
    o->rest.s.nString	= 0;
}

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

	if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	    region = &infiniteRegion;

	if (dw->wd && region->numRects)
	{
	    WindowDecoration *wd = dw->wd;
	    REGION	     box;
	    int		     i;

	    box.rects	 = &box.extents;
	    box.numRects = 1;

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
		(*w->screen->drawWindowTexture) (w,
						 &wd->decor->texture->texture,
						 attrib, mask |
						 PAINT_WINDOW_TRANSLUCENT_MASK);
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

    if (n < 12 + 9)
    {
	XFree (data);
	return NULL;
    }

    prop = (long *) data;

    if (*prop != DECOR_INTERFACE_VERSION)
    {
	fprintf (stderr, "%s: decoration: property ignored because "
		 "version is %d and decoration plugin version is %d\n",
		 programName, (int) *prop, DECOR_INTERFACE_VERSION);

	XFree (data);
	return NULL;
    }

    prop++;

    decoration = malloc (sizeof (Decoration));
    if (!decoration)
    {
	XFree (data);
	return NULL;
    }

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

    prop += 4;

    decoration->minWidth  = *prop++;
    decoration->minHeight = *prop++;

    nQuad = (n - 12) / 9;

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

	quad->p1.gravity = (flags >> 0) & 0xf;
	quad->p2.gravity = (flags >> 4) & 0xf;

	quad->align = (flags >> 8)  & 0x3;
	quad->clamp = (flags >> 10) & 0x3;

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
    free (decoration);
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

    DECOR_WINDOW (w);

    wd = dw->wd;
    if (!wd)
	return;

    for (i = 0; i < wd->nQuad; i++)
    {
	wd->quad[i].matrix = wd->decor->texture->texture.matrix;

	x0 = wd->decor->quad[i].m.x0;
	if (wd->decor->quad[i].align & ALIGN_RIGHT)
	    x0 -= wd->quad[i].box.x2 - wd->quad[i].box.x1;

	y0 = wd->decor->quad[i].m.y0;
	if (wd->decor->quad[i].align & ALIGN_BOTTOM)
	    y0 -= wd->quad[i].box.y2 - wd->quad[i].box.y1;

	wd->quad[i].matrix.x0 += x0 * wd->quad[i].matrix.xx;
	wd->quad[i].matrix.y0 += y0 * wd->quad[i].matrix.yy;

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
    else if (gravity & GRAVITY_WEST)
	*return_x = x;
    else
	*return_x = (width >> 1) + x;

    if (gravity & GRAVITY_SOUTH)
	*return_y = y + height;
    else if (gravity & GRAVITY_NORTH)
	*return_y = y;
    else
	*return_y = (height >> 1) + y;
}

static void
updateWindowDecorationScale (CompWindow *w)
{
    WindowDecoration *wd;
    int		     x1, y1, x2, y2;
    int		     maxWidth, maxHeight;
    int		     align;
    int		     clamp;
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
	clamp	  = wd->decor->quad[i].clamp;

	if (clamp & CLAMP_HORZ)
	{
	    if (x1 < 0)
		x1 = 0;
	    if (x2 > w->width)
		x2 = w->width;
	}

	if (clamp & CLAMP_VERT)
	{
	    if (y1 < 0)
		y1 = 0;
	    if (y2 > w->height)
		y2 = w->height;
	}

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
decorCheckSize (CompWindow *w,
		Decoration *decor)
{
    return (decor->minWidth <= w->width && decor->minHeight <= w->height);
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

    if (dw->decor && decorCheckSize (w, dw->decor))
    {
	if (w->type != CompWindowTypeFullscreenMask)
	    decor = dw->decor;
    }
    else
    {
	if (w->attrib.override_redirect)
	{
	    if (w->region->numRects == 1 && !w->alpha)
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
	    default:
		if (w->region->numRects == 1 && !w->alpha)
		    decor = ds->decor[DECOR_BARE];

		/* no decoration on windows with below state */
		if (w->state & CompWindowStateBelowMask)
		    decor = NULL;

		break;
	    }
	}

	if (decor)
	{
	    if (!decorCheckSize (w, decor))
		decor = NULL;
	}
    }

    if (!ds->dmWin)
	decor = NULL;

    if (decor == old)
	return FALSE;

    if (old)
    {
	damageWindowOutputExtents (w);
	destroyWindowDecoration (w->screen, wd);
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
	moveWindow (w, dx, dy, TRUE, TRUE);

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
	int	   i;

	if (dmWin)
	{
	    for (i = 0; i < DECOR_NUM; i++)
		ds->decor[i] =
		    decorCreateDecoration (s, s->root, dd->decorAtom[i]);
	}
	else
	{
	    for (i = 0; i < DECOR_NUM; i++)
	    {
		if (ds->decor[i])
		{
		    decorReleaseDecoration (s, ds->decor[i]);
		    ds->decor[i] = 0;
		}
	    }

	    for (w = s->windows; w; w = w->next)
	    {
		DECOR_WINDOW (w);

		if (dw->decor)
		{
		    decorReleaseDecoration (s, dw->decor);
		    dw->decor = 0;
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
			    if (w->attrib.override_redirect)
				continue;

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

			    if (ds->decor[i])
				decorReleaseDecoration (s, ds->decor[i]);

			    ds->decor[i] =
				decorCreateDecoration (s, s->root,
						       dd->decorAtom[i]);

			    for (w = s->windows; w; w = w->next)
				decorWindowUpdate (w, TRUE);
			}
		    }
		}
	    }
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
    dd->decorAtom[DECOR_BARE] =
	XInternAtom (d->display, "_NET_WINDOW_DECOR_BARE", 0);
    dd->decorAtom[DECOR_NORMAL] =
	XInternAtom (d->display, "_NET_WINDOW_DECOR_NORMAL", 0);
    dd->decorAtom[DECOR_ACTIVE] =
	XInternAtom (d->display, "_NET_WINDOW_DECOR_ACTIVE", 0);

    decorDisplayInitOptions (dd);

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

    if (!w->attrib.override_redirect)
	decorWindowUpdateDecoration (w);

    if (w->shaded || w->attrib.map_state == IsViewable)
	decorWindowUpdate (w, FALSE);

    return TRUE;
}

static void
decorFiniWindow (CompPlugin *p,
		 CompWindow *w)
{
    DECOR_WINDOW (w);

    if (dw->wd)
	destroyWindowDecoration (w->screen, dw->wd);

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

static int
decorGetVersion (CompPlugin *plugin,
		 int	    version)
{
    return ABIVERSION;
}

CompPluginDep decorDeps[] = {
    { CompPluginRuleBefore, "wobbly" },
    { CompPluginRuleBefore, "fade" },
    { CompPluginRuleBefore, "cube" },
    { CompPluginRuleBefore, "scale" }
};

CompPluginFeature decorFeatures[] = {
    { "decorations" }
};

static CompPluginVTable decorVTable = {
    "decoration",
    N_("Window Decoration"),
    N_("Window decorations"),
    decorGetVersion,
    decorInit,
    decorFini,
    decorInitDisplay,
    decorFiniDisplay,
    decorInitScreen,
    decorFiniScreen,
    decorInitWindow,
    decorFiniWindow,
    decorGetDisplayOptions,
    decorSetDisplayOption,
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    decorDeps,
    sizeof (decorDeps) / sizeof (decorDeps[0]),
    decorFeatures,
    sizeof (decorFeatures) / sizeof (decorFeatures[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &decorVTable;
}
