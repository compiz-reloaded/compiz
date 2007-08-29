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

#include <compiz-core.h>

static void
setCursorMatrix (CompCursor *c)
{
    c->matrix = c->image->texture.matrix;
    c->matrix.x0 -= (c->x * c->matrix.xx);
    c->matrix.y0 -= (c->y * c->matrix.yy);
}

void
addCursor (CompScreen *s)
{
    CompCursor *c;

    c = malloc (sizeof (CompCursor));
    if (c)
    {
	c->screen = s;
	c->image  = NULL;
	c->x	  = 0;
	c->y	  = 0;

	c->next    = s->cursors;
	s->cursors = c;

	updateCursor (c, 0, 0, 0);

	/* XFixesHideCursor (s->display->display, s->root); */
    }
}

Bool
damageCursorRect (CompCursor *c,
		  Bool       initial,
		  BoxPtr     rect)
{
    return FALSE;
}

void
addCursorDamageRect (CompCursor *c,
		     BoxPtr     rect)
{
    REGION region;

    if (c->screen->damageMask & COMP_SCREEN_DAMAGE_ALL_MASK)
	return;

    region.extents = *rect;

    if (!(*c->screen->damageCursorRect) (c, FALSE, &region.extents))
    {
	region.extents.x1 += c->x;
	region.extents.y1 += c->y;
	region.extents.x2 += c->x;
	region.extents.y2 += c->y;

	region.rects = &region.extents;
	region.numRects = region.size = 1;

	damageScreenRegion (c->screen, &region);
    }
}

void
addCursorDamage (CompCursor *c)
{
    BoxRec box;

    if (c->screen->damageMask & COMP_SCREEN_DAMAGE_ALL_MASK)
	return;

    box.x1 = 0;
    box.y1 = 0;
    box.x2 = c->image->width;
    box.y2 = c->image->height;

    addCursorDamageRect (c, &box);
}

void
updateCursor (CompCursor    *c,
	      int	    x,
	      int	    y,
	      unsigned long serial)
{
    /* new current cursor */
    if (!c->image || c->image->serial != serial)
    {
	CompCursorImage *cursorImage;

	cursorImage = findCursorImageAtScreen (c->screen, serial);
	if (!cursorImage)
	{
	    Display	      *dpy = c->screen->display->display;
	    XFixesCursorImage *image;

	    image = XFixesGetCursorImage (dpy);
	    if (!image)
		return;

	    cursorImage = malloc (sizeof (CompCursorImage));
	    if (!cursorImage)
	    {
		XFree (image);
		return;
	    }

	    x = image->x;
	    y = image->y;

	    cursorImage->serial = image->cursor_serial;
	    cursorImage->xhot   = image->xhot;
	    cursorImage->yhot   = image->yhot;
	    cursorImage->width  = image->width;
	    cursorImage->height = image->height;

	    initTexture (c->screen, &cursorImage->texture);

	    if (!imageBufferToTexture (c->screen,
				       &cursorImage->texture,
				       (char *) image->pixels,
				       image->width,
				       image->height))
	    {
		free (cursorImage);
		XFree (image);
		return;
	    }

	    XFree (image);

	    cursorImage->next = c->screen->cursorImages;
	    c->screen->cursorImages = cursorImage;
	}

	if (c->image)
	    addCursorDamage (c);

	c->image = cursorImage;

	c->x = x - c->image->xhot;
	c->y = y - c->image->yhot;

	setCursorMatrix (c);

	addCursorDamage (c);
    }
    else
    {
	int newX, newY;

	newX = x - c->image->xhot;
	newY = y - c->image->yhot;

	if (c->x != newX || c->y != newY)
	{
	    addCursorDamage (c);

	    c->x = newX;
	    c->y = newY;

	    setCursorMatrix (c);

	    addCursorDamage (c);
	}
    }
}
