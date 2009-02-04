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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <compiz-core.h>

ScreenPaintAttrib defaultScreenPaintAttrib = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -DEFAULT_Z_CAMERA
};

WindowPaintAttrib defaultWindowPaintAttrib = {
    OPAQUE, BRIGHT, COLOR, 1.0f, 1.0f, 0.0f, 0.0f
};

void
preparePaintScreen (CompScreen *screen,
		    int	       msSinceLastPaint)
{
}

void
donePaintScreen (CompScreen *screen)
{
}

void
applyScreenTransform (CompScreen	      *screen,
		      const ScreenPaintAttrib *sAttrib,
		      CompOutput	      *output,
		      CompTransform	      *transform)
{
    matrixTranslate (transform,
		     sAttrib->xTranslate,
		     sAttrib->yTranslate,
		     sAttrib->zTranslate + sAttrib->zCamera);
    matrixRotate (transform,
		  sAttrib->xRotate, 0.0f, 1.0f, 0.0f);
    matrixRotate (transform,
		  sAttrib->vRotate,
		  cosf (sAttrib->xRotate * DEG2RAD),
		  0.0f,
		  sinf (sAttrib->xRotate * DEG2RAD));
    matrixRotate (transform,
		  sAttrib->yRotate, 0.0f, 1.0f, 0.0f);
}

void
transformToScreenSpace (CompScreen    *screen,
			CompOutput    *output,
			float         z,
			CompTransform *transform)
{
    matrixTranslate (transform, -0.5f, -0.5f, z);
    matrixScale (transform,
		 1.0f  / output->width,
		 -1.0f / output->height,
		 1.0f);
    matrixTranslate (transform,
		     -output->region.extents.x1,
		     -output->region.extents.y2,
		     0.0f);
}

void
prepareXCoords (CompScreen *screen,
		CompOutput *output,
		float      z)
{
    glTranslatef (-0.5f, -0.5f, z);
    glScalef (1.0f  / output->width,
	      -1.0f / output->height,
	      1.0f);
    glTranslatef (-output->region.extents.x1,
		  -output->region.extents.y2,
		  0.0f);
}

void
paintCursor (CompCursor		 *c,
	     const CompTransform *transform,
	     Region		 region,
	     unsigned int	 mask)
{
    int x1, y1, x2, y2;

    if (!c->image)
	return;

    x1 = c->x;
    y1 = c->y;
    x2 = c->x + c->image->width;
    y2 = c->y + c->image->height;

    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnable (GL_BLEND);

    enableTexture (c->screen, &c->image->texture, COMP_TEXTURE_FILTER_FAST);

    glBegin (GL_QUADS);

    glTexCoord2f (COMP_TEX_COORD_X (&c->matrix, x1),
		  COMP_TEX_COORD_Y (&c->matrix, y2));
    glVertex2i (x1, y2);
    glTexCoord2f (COMP_TEX_COORD_X (&c->matrix, x2),
		  COMP_TEX_COORD_Y (&c->matrix, y2));
    glVertex2i (x2, y2);
    glTexCoord2f (COMP_TEX_COORD_X (&c->matrix, x2),
		  COMP_TEX_COORD_Y (&c->matrix, y1));
    glVertex2i (x2, y1);
    glTexCoord2f (COMP_TEX_COORD_X (&c->matrix, x1),
		  COMP_TEX_COORD_Y (&c->matrix, y1));
    glVertex2i (x1, y1);

    glEnd ();

    disableTexture (c->screen, &c->image->texture);

    glDisable (GL_BLEND);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
}

static void
paintBackground (CompScreen   *s,
		 Region	      region,
		 Bool	      transformed)
{
    CompTexture *bg = &s->backgroundTexture;
    BoxPtr      pBox = region->rects;
    int	        n, nBox = region->numRects;
    GLfloat     *d, *data;

    if (!nBox)
	return;

    if (s->desktopWindowCount)
    {
	if (bg->name)
	{
	    finiTexture (s, bg);
	    initTexture (s, bg);
	}

	s->backgroundLoaded = FALSE;

	return;
    }
    else
    {
	if (!s->backgroundLoaded)
	    updateScreenBackground (s, bg);

	s->backgroundLoaded = TRUE;
    }

    data = malloc (sizeof (GLfloat) * nBox * 16);
    if (!data)
	return;

    d = data;
    n = nBox;
    while (n--)
    {
	*d++ = COMP_TEX_COORD_X (&bg->matrix, pBox->x1);
	*d++ = COMP_TEX_COORD_Y (&bg->matrix, pBox->y2);

	*d++ = pBox->x1;
	*d++ = pBox->y2;

	*d++ = COMP_TEX_COORD_X (&bg->matrix, pBox->x2);
	*d++ = COMP_TEX_COORD_Y (&bg->matrix, pBox->y2);

	*d++ = pBox->x2;
	*d++ = pBox->y2;

	*d++ = COMP_TEX_COORD_X (&bg->matrix, pBox->x2);
	*d++ = COMP_TEX_COORD_Y (&bg->matrix, pBox->y1);

	*d++ = pBox->x2;
	*d++ = pBox->y1;

	*d++ = COMP_TEX_COORD_X (&bg->matrix, pBox->x1);
	*d++ = COMP_TEX_COORD_Y (&bg->matrix, pBox->y1);

	*d++ = pBox->x1;
	*d++ = pBox->y1;

	pBox++;
    }

    glTexCoordPointer (2, GL_FLOAT, sizeof (GLfloat) * 4, data);
    glVertexPointer (2, GL_FLOAT, sizeof (GLfloat) * 4, data + 2);

    if (bg->name)
    {
	if (transformed)
	    enableTexture (s, bg, COMP_TEXTURE_FILTER_GOOD);
	else
	    enableTexture (s, bg, COMP_TEXTURE_FILTER_FAST);

	glDrawArrays (GL_QUADS, 0, nBox * 4);

	disableTexture (s, bg);
    }
    else
    {
	glColor4us (0, 0, 0, 0);
	glDrawArrays (GL_QUADS, 0, nBox * 4);
	glColor4usv (defaultColor);
    }

    free (data);
}


/* This function currently always performs occlusion detection to
   minimize paint regions. OpenGL precision requirements are no good
   enough to guarantee that the results from using occlusion detection
   is the same as without. It's likely not possible to see any
   difference with most hardware but occlusion detection in the
   transformed screen case should be made optional for those who do
   see a difference. */
static void
paintOutputRegion (CompScreen	       *screen,
		   const CompTransform *transform,
		   Region	       region,
		   CompOutput	       *output,
		   unsigned int	       mask)
{
    static Region tmpRegion = NULL;
    CompWindow    *w;
    CompCursor	  *c;
    int		  count, windowMask, odMask, i;
    CompWindow	  *fullscreenWindow = NULL;
    CompWalker    walk;
    Bool          status;
    Bool          withOffset = FALSE;
    CompTransform vTransform;
    int           offX, offY;
    Region        clip = region;

    if (!tmpRegion)
    {
	tmpRegion = XCreateRegion ();
	if (!tmpRegion)
	    return;
    }

    if (mask & PAINT_SCREEN_TRANSFORMED_MASK)
    {
	windowMask     = PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK;
	count	       = 1;
    }
    else
    {
	windowMask     = 0;
	count	       = 0;
    }

    XSubtractRegion (region, &emptyRegion, tmpRegion);

    (*screen->initWindowWalker) (screen, &walk);

    if (!(mask & PAINT_SCREEN_NO_OCCLUSION_DETECTION_MASK))
    {
	/* detect occlusions */
	for (w = (*walk.last) (screen); w; w = (*walk.prev) (w))
	{
	    if (w->destroyed)
		continue;

	    if (!w->shaded)
	    {
		if (w->attrib.map_state != IsViewable || !w->damaged)
		    continue;
	    }

	    /* copy region */
	    XSubtractRegion (tmpRegion, &emptyRegion, w->clip);

	    odMask = PAINT_WINDOW_OCCLUSION_DETECTION_MASK;
		
	    if ((screen->windowOffsetX != 0 || screen->windowOffsetY != 0) &&
		!windowOnAllViewports (w))
	    {
		withOffset = TRUE;

		getWindowMovementForOffset (w, screen->windowOffsetX,
					    screen->windowOffsetY,
					    &offX, &offY);

		vTransform = *transform;
		matrixTranslate (&vTransform, offX, offY, 0);

		XOffsetRegion (w->clip, -offX, -offY);

		odMask |= PAINT_WINDOW_WITH_OFFSET_MASK;
		status = (*screen->paintWindow) (w, &w->paint, &vTransform,
						 tmpRegion, odMask);
	    }
	    else
	    {
		withOffset = FALSE;
		status = (*screen->paintWindow) (w, &w->paint, transform, tmpRegion,
						 odMask);
	    }

	    if (status)
	    {
		if (withOffset)
		{
		    XOffsetRegion (w->region, offX, offY);
		    XSubtractRegion (tmpRegion, w->region, tmpRegion);
		    XOffsetRegion (w->region, -offX, -offY);
		}
		else
		    XSubtractRegion (tmpRegion, w->region, tmpRegion);

		/* unredirect top most fullscreen windows. */
		if (count == 0 &&
		    screen->opt[COMP_SCREEN_OPTION_UNREDIRECT_FS].value.b)
		{
		    if (XEqualRegion (w->region, &screen->region) &&
			!REGION_NOT_EMPTY (tmpRegion))
		    {
			fullscreenWindow = w;
		    }
		    else
		    {
			for (i = 0; i < screen->nOutputDev; i++)
			    if (XEqualRegion (w->region,
					      &screen->outputDev[i].region))
				fullscreenWindow = w;
		    }
		}
	    }

	    if (!w->invisible)
		count++;
	}
    }

    if (fullscreenWindow)
	unredirectWindow (fullscreenWindow);

    if (!(mask & PAINT_SCREEN_NO_BACKGROUND_MASK))
	paintBackground (screen, tmpRegion,
			 (mask & PAINT_SCREEN_TRANSFORMED_MASK));

    /* paint all windows from bottom to top */
    for (w = (*walk.first) (screen); w; w = (*walk.next) (w))
    {
	if (w->destroyed)
	    continue;

	if (w == fullscreenWindow)
	    continue;

	if (!w->shaded)
	{
	    if (w->attrib.map_state != IsViewable || !w->damaged)
		continue;
	}

	if (!(mask & PAINT_SCREEN_NO_OCCLUSION_DETECTION_MASK))
	    clip = w->clip;

	if ((screen->windowOffsetX != 0 || screen->windowOffsetY != 0) &&
	    !windowOnAllViewports (w))
	{
	    getWindowMovementForOffset (w, screen->windowOffsetX,
					screen->windowOffsetY, &offX, &offY);

	    vTransform = *transform;
	    matrixTranslate (&vTransform, offX, offY, 0);
	    (*screen->paintWindow) (w, &w->paint, &vTransform, clip,
				    windowMask | PAINT_WINDOW_WITH_OFFSET_MASK);
	}
	else
	{
	    (*screen->paintWindow) (w, &w->paint, transform, clip,
				    windowMask);
	}
    }

    if (walk.fini)
	(*walk.fini) (screen, &walk);

    /* paint cursors */
    for (c = screen->cursors; c; c = c->next)
	(*screen->paintCursor) (c, transform, tmpRegion, 0);
}

void
enableOutputClipping (CompScreen	  *screen,
		      const CompTransform *transform,
		      Region		  region,
		      CompOutput	  *output)
{
    GLdouble h = screen->height;

    GLdouble p1[2] = { region->extents.x1, h - region->extents.y2 };
    GLdouble p2[2] = { region->extents.x2, h - region->extents.y1 };

    GLdouble halfW = output->width / 2.0;
    GLdouble halfH = output->height / 2.0;

    GLdouble cx = output->region.extents.x1 + halfW;
    GLdouble cy = (h - output->region.extents.y2) + halfH;

    GLdouble top[4]    = { 0.0, halfH / (cy - p1[1]), 0.0, 0.5 };
    GLdouble bottom[4] = { 0.0, halfH / (cy - p2[1]), 0.0, 0.5 };
    GLdouble left[4]   = { halfW / (cx - p1[0]), 0.0, 0.0, 0.5 };
    GLdouble right[4]  = { halfW / (cx - p2[0]), 0.0, 0.0, 0.5 };

    glPushMatrix ();
    glLoadMatrixf (transform->m);

    glClipPlane (GL_CLIP_PLANE0, top);
    glClipPlane (GL_CLIP_PLANE1, bottom);
    glClipPlane (GL_CLIP_PLANE2, left);
    glClipPlane (GL_CLIP_PLANE3, right);

    glEnable (GL_CLIP_PLANE0);
    glEnable (GL_CLIP_PLANE1);
    glEnable (GL_CLIP_PLANE2);
    glEnable (GL_CLIP_PLANE3);

    glPopMatrix ();
}

void
disableOutputClipping (CompScreen *screen)
{
    glDisable (GL_CLIP_PLANE0);
    glDisable (GL_CLIP_PLANE1);
    glDisable (GL_CLIP_PLANE2);
    glDisable (GL_CLIP_PLANE3);
}

#define CLIP_PLANE_MASK (PAINT_SCREEN_TRANSFORMED_MASK | \
			 PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK)

void
paintTransformedOutput (CompScreen		*screen,
			const ScreenPaintAttrib *sAttrib,
			const CompTransform	*transform,
			Region			region,
			CompOutput		*output,
			unsigned int		mask)
{
    CompTransform sTransform = *transform;

    if (mask & PAINT_SCREEN_CLEAR_MASK)
	clearTargetOutput (screen->display, GL_COLOR_BUFFER_BIT);

    screenLighting (screen, TRUE);

    (*screen->applyScreenTransform) (screen, sAttrib, output, &sTransform);

    if ((mask & CLIP_PLANE_MASK) == CLIP_PLANE_MASK)
    {
	screen->enableOutputClipping (screen, &sTransform, region, output);

	transformToScreenSpace (screen, output, -sAttrib->zTranslate,
				&sTransform);

	glPushMatrix ();
	glLoadMatrixf (sTransform.m);

	paintOutputRegion (screen, &sTransform, region, output, mask);

	glPopMatrix ();

	screen->disableOutputClipping (screen);
    }
    else
    {
	transformToScreenSpace (screen, output, -sAttrib->zTranslate,
				&sTransform);

	glPushMatrix ();
	glLoadMatrixf (sTransform.m);

	paintOutputRegion (screen, &sTransform, region, output, mask);

	glPopMatrix ();
    }
}

Bool
paintOutput (CompScreen		     *screen,
	     const ScreenPaintAttrib *sAttrib,
	     const CompTransform     *transform,
	     Region		     region,
	     CompOutput		     *output,
	     unsigned int	     mask)
{
    CompTransform sTransform = *transform;

    if (mask & PAINT_SCREEN_REGION_MASK)
    {
	if (mask & PAINT_SCREEN_TRANSFORMED_MASK)
	{
	    if (mask & PAINT_SCREEN_FULL_MASK)
	    {
		region = &output->region;

		(*screen->paintTransformedOutput) (screen, sAttrib,
						   &sTransform, region,
						   output, mask);

		return TRUE;
	    }

	    return FALSE;
	}

	/* fall through and redraw region */
    }
    else if (mask & PAINT_SCREEN_FULL_MASK)
    {
	(*screen->paintTransformedOutput) (screen, sAttrib, &sTransform,
					   &output->region,
					   output, mask);

	return TRUE;
    }
    else
	return FALSE;

    screenLighting (screen, FALSE);

    transformToScreenSpace (screen, output, -DEFAULT_Z_CAMERA, &sTransform);

    glPushMatrix ();
    glLoadMatrixf (sTransform.m);

    paintOutputRegion (screen, &sTransform, region, output, mask);

    glPopMatrix ();

    return TRUE;
}

#define ADD_RECT(data, m, n, x1, y1, x2, y2)	   \
    for (it = 0; it < n; it++)			   \
    {						   \
	*(data)++ = COMP_TEX_COORD_X (&m[it], x1); \
	*(data)++ = COMP_TEX_COORD_Y (&m[it], y1); \
    }						   \
    *(data)++ = (x1);				   \
    *(data)++ = (y1);				   \
    *(data)++ = 0.0;				   \
    for (it = 0; it < n; it++)			   \
    {						   \
	*(data)++ = COMP_TEX_COORD_X (&m[it], x1); \
	*(data)++ = COMP_TEX_COORD_Y (&m[it], y2); \
    }						   \
    *(data)++ = (x1);				   \
    *(data)++ = (y2);				   \
    *(data)++ = 0.0;				   \
    for (it = 0; it < n; it++)			   \
    {						   \
	*(data)++ = COMP_TEX_COORD_X (&m[it], x2); \
	*(data)++ = COMP_TEX_COORD_Y (&m[it], y2); \
    }						   \
    *(data)++ = (x2);				   \
    *(data)++ = (y2);				   \
    *(data)++ = 0.0;				   \
    for (it = 0; it < n; it++)			   \
    {						   \
	*(data)++ = COMP_TEX_COORD_X (&m[it], x2); \
	*(data)++ = COMP_TEX_COORD_Y (&m[it], y1); \
    }						   \
    *(data)++ = (x2);				   \
    *(data)++ = (y1);				   \
    *(data)++ = 0.0

#define ADD_QUAD(data, m, n, x1, y1, x2, y2)		\
    for (it = 0; it < n; it++)				\
    {							\
	*(data)++ = COMP_TEX_COORD_XY (&m[it], x1, y1);	\
	*(data)++ = COMP_TEX_COORD_YX (&m[it], x1, y1);	\
    }							\
    *(data)++ = (x1);					\
    *(data)++ = (y1);					\
    *(data)++ = 0.0;					\
    for (it = 0; it < n; it++)				\
    {							\
	*(data)++ = COMP_TEX_COORD_XY (&m[it], x1, y2);	\
	*(data)++ = COMP_TEX_COORD_YX (&m[it], x1, y2);	\
    }							\
    *(data)++ = (x1);					\
    *(data)++ = (y2);					\
    *(data)++ = 0.0;					\
    for (it = 0; it < n; it++)				\
    {							\
	*(data)++ = COMP_TEX_COORD_XY (&m[it], x2, y2);	\
	*(data)++ = COMP_TEX_COORD_YX (&m[it], x2, y2);	\
    }							\
    *(data)++ = (x2);					\
    *(data)++ = (y2);					\
    *(data)++ = 0.0;					\
    for (it = 0; it < n; it++)				\
    {							\
	*(data)++ = COMP_TEX_COORD_XY (&m[it], x2, y1);	\
	*(data)++ = COMP_TEX_COORD_YX (&m[it], x2, y1);	\
    }							\
    *(data)++ = (x2);					\
    *(data)++ = (y1);					\
    *(data)++ = 0.0;


Bool
moreWindowVertices (CompWindow *w,
		    int        newSize)
{
    if (newSize > w->vertexSize)
    {
	GLfloat *vertices;

	vertices = realloc (w->vertices, sizeof (GLfloat) * newSize);
	if (!vertices)
	    return FALSE;

	w->vertices = vertices;
	w->vertexSize = newSize;
    }

    return TRUE;
}

Bool
moreWindowIndices (CompWindow *w,
		   int        newSize)
{
    if (newSize > w->indexSize)
    {
	GLushort *indices;

	indices = realloc (w->indices, sizeof (GLushort) * newSize);
	if (!indices)
	    return FALSE;

	w->indices = indices;
	w->indexSize = newSize;
    }

    return TRUE;
}

static void
drawWindowGeometry (CompWindow *w)
{
    int     texUnit = w->texUnits;
    int     currentTexUnit = 0;
    int     stride = w->vertexStride;
    GLfloat *vertices = w->vertices + (stride - 3);

    stride *= sizeof (GLfloat);

    glVertexPointer (3, GL_FLOAT, stride, vertices);

    while (texUnit--)
    {
	if (texUnit != currentTexUnit)
	{
	    (*w->screen->clientActiveTexture) (GL_TEXTURE0_ARB + texUnit);
	    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	    currentTexUnit = texUnit;
	}
	vertices -= w->texCoordSize;
	glTexCoordPointer (w->texCoordSize, GL_FLOAT, stride, vertices);
    }

    glDrawArrays (GL_QUADS, 0, w->vCount);

    /* disable all texture coordinate arrays except 0 */
    texUnit = w->texUnits;
    if (texUnit > 1)
    {
	while (--texUnit)
	{
	    (*w->screen->clientActiveTexture) (GL_TEXTURE0_ARB + texUnit);
	    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	}

	(*w->screen->clientActiveTexture) (GL_TEXTURE0_ARB);
    }
}

void
addWindowGeometry (CompWindow *w,
		   CompMatrix *matrix,
		   int	      nMatrix,
		   Region     region,
		   Region     clip)
{
    BoxRec full;

    w->texUnits = nMatrix;

    full = clip->extents;
    if (region->extents.x1 > full.x1)
	full.x1 = region->extents.x1;
    if (region->extents.y1 > full.y1)
	full.y1 = region->extents.y1;
    if (region->extents.x2 < full.x2)
	full.x2 = region->extents.x2;
    if (region->extents.y2 < full.y2)
	full.y2 = region->extents.y2;

    if (full.x1 < full.x2 && full.y1 < full.y2)
    {
	BoxPtr  pBox;
	int     nBox;
	BoxPtr  pClip;
	int     nClip;
	BoxRec  cbox;
	int     vSize;
	int     n, it, x1, y1, x2, y2;
	GLfloat *d;
	Bool    rect = TRUE;

	for (it = 0; it < nMatrix; it++)
	{
	    if (matrix[it].xy != 0.0f || matrix[it].yx != 0.0f)
	    {
		rect = FALSE;
		break;
	    }
	}

	pBox = region->rects;
	nBox = region->numRects;

	vSize = 3 + nMatrix * 2;

	n = w->vCount / 4;

	if ((n + nBox) * vSize * 4 > w->vertexSize)
	{
	    if (!moreWindowVertices (w, (n + nBox) * vSize * 4))
		return;
	}

	d = w->vertices + (w->vCount * vSize);

	while (nBox--)
	{
	    x1 = pBox->x1;
	    y1 = pBox->y1;
	    x2 = pBox->x2;
	    y2 = pBox->y2;

	    pBox++;

	    if (x1 < full.x1)
		x1 = full.x1;
	    if (y1 < full.y1)
		y1 = full.y1;
	    if (x2 > full.x2)
		x2 = full.x2;
	    if (y2 > full.y2)
		y2 = full.y2;

	    if (x1 < x2 && y1 < y2)
	    {
		nClip = clip->numRects;

		if (nClip == 1)
		{
		    if (rect)
		    {
			ADD_RECT (d, matrix, nMatrix, x1, y1, x2, y2);
		    }
		    else
		    {
			ADD_QUAD (d, matrix, nMatrix, x1, y1, x2, y2);
		    }

		    n++;
		}
		else
		{
		    pClip = clip->rects;

		    if (((n + nClip) * vSize * 4) > w->vertexSize)
		    {
			if (!moreWindowVertices (w, (n + nClip) * vSize * 4))
			    return;

			d = w->vertices + (n * vSize * 4);
		    }

		    while (nClip--)
		    {
			cbox = *pClip;

			pClip++;

			if (cbox.x1 < x1)
			    cbox.x1 = x1;
			if (cbox.y1 < y1)
			    cbox.y1 = y1;
			if (cbox.x2 > x2)
			    cbox.x2 = x2;
			if (cbox.y2 > y2)
			    cbox.y2 = y2;

			if (cbox.x1 < cbox.x2 && cbox.y1 < cbox.y2)
			{
			    if (rect)
			    {
				ADD_RECT (d, matrix, nMatrix,
					  cbox.x1, cbox.y1, cbox.x2, cbox.y2);
			    }
			    else
			    {
				ADD_QUAD (d, matrix, nMatrix,
					  cbox.x1, cbox.y1, cbox.x2, cbox.y2);
			    }

			    n++;
			}
		    }
		}
	    }
	}

	w->vCount	      = n * 4;
	w->vertexStride       = vSize;
	w->texCoordSize       = 2;
	w->drawWindowGeometry = drawWindowGeometry;
    }
}

static Bool
enableFragmentProgramAndDrawGeometry (CompWindow	   *w,
				      CompTexture	   *texture,
				      const FragmentAttrib *attrib,
				      int		   filter,
				      unsigned int	   mask)
{
    FragmentAttrib fa = *attrib;
    CompScreen     *s = w->screen;
    Bool	   blending;

    if (s->canDoSaturated && attrib->saturation != COLOR)
    {
	int param, function;

	param    = allocFragmentParameters (&fa, 1);
	function = getSaturateFragmentFunction (s, texture, param);

	addFragmentFunction (&fa, function);

	(*s->programEnvParameter4f) (GL_FRAGMENT_PROGRAM_ARB, param,
				     RED_SATURATION_WEIGHT,
				     GREEN_SATURATION_WEIGHT,
				     BLUE_SATURATION_WEIGHT,
				     attrib->saturation / 65535.0f);
    }

    if (!enableFragmentAttrib (s, &fa, &blending))
	return FALSE;

    enableTexture (s, texture, filter);

    if (mask & PAINT_WINDOW_BLEND_MASK)
    {
	if (blending)
	    glEnable (GL_BLEND);

	if (attrib->opacity != OPAQUE || attrib->brightness != BRIGHT)
	{
	    GLushort color;

	    color = (attrib->opacity * attrib->brightness) >> 16;

	    screenTexEnvMode (s, GL_MODULATE);
	    glColor4us (color, color, color, attrib->opacity);

	    (*w->drawWindowGeometry) (w);

	    glColor4usv (defaultColor);
	    screenTexEnvMode (s, GL_REPLACE);
	}
	else
	{
	    (*w->drawWindowGeometry) (w);
	}

	if (blending)
	    glDisable (GL_BLEND);
    }
    else if (attrib->brightness != BRIGHT)
    {
	screenTexEnvMode (s, GL_MODULATE);
	glColor4us (attrib->brightness, attrib->brightness,
		    attrib->brightness, BRIGHT);

	(*w->drawWindowGeometry) (w);

	glColor4usv (defaultColor);
	screenTexEnvMode (s, GL_REPLACE);
    }
    else
    {
	(*w->drawWindowGeometry) (w);
    }

    disableTexture (w->screen, texture);

    disableFragmentAttrib (s, &fa);

    return TRUE;
}

static void
enableFragmentOperationsAndDrawGeometry (CompWindow	      *w,
					 CompTexture	      *texture,
					 const FragmentAttrib *attrib,
					 int		      filter,
					 unsigned int	      mask)
{
    CompScreen *s = w->screen;

    if (s->canDoSaturated && attrib->saturation != COLOR)
    {
	GLfloat constant[4];

	if (mask & PAINT_WINDOW_BLEND_MASK)
	    glEnable (GL_BLEND);

	enableTexture (s, texture, filter);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_PRIMARY_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);

	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	glColor4f (1.0f, 1.0f, 1.0f, 0.5f);

	s->activeTexture (GL_TEXTURE1_ARB);

	enableTexture (s, texture, filter);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

	if (s->canDoSlightlySaturated && attrib->saturation > 0)
	{
	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	    constant[0] = 0.5f + 0.5f * RED_SATURATION_WEIGHT;
	    constant[1] = 0.5f + 0.5f * GREEN_SATURATION_WEIGHT;
	    constant[2] = 0.5f + 0.5f * BLUE_SATURATION_WEIGHT;
	    constant[3] = 1.0;

	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);

	    s->activeTexture (GL_TEXTURE2_ARB);

	    enableTexture (s, texture, filter);

	    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);

	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	    constant[3] = attrib->saturation / 65535.0f;

	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);

	    if (attrib->opacity < OPAQUE || attrib->brightness != BRIGHT)
	    {
		s->activeTexture (GL_TEXTURE3_ARB);

		enableTexture (s, texture, filter);

		constant[3] = attrib->opacity / 65535.0f;
		constant[0] = constant[1] = constant[2] = constant[3] *
		    attrib->brightness / 65535.0f;

		glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);

		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
		glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_CONSTANT);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

		(*w->drawWindowGeometry) (w);

		disableTexture (s, texture);

		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		s->activeTexture (GL_TEXTURE2_ARB);
	    }
	    else
	    {
		(*w->drawWindowGeometry) (w);
	    }

	    disableTexture (s, texture);

	    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	    s->activeTexture (GL_TEXTURE1_ARB);
	}
	else
	{
	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_CONSTANT);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);

	    constant[3] = attrib->opacity / 65535.0f;
	    constant[0] = constant[1] = constant[2] = constant[3] *
		attrib->brightness / 65535.0f;

	    constant[0] = 0.5f + 0.5f * RED_SATURATION_WEIGHT   * constant[0];
	    constant[1] = 0.5f + 0.5f * GREEN_SATURATION_WEIGHT * constant[1];
	    constant[2] = 0.5f + 0.5f * BLUE_SATURATION_WEIGHT  * constant[2];

	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);

	    (*w->drawWindowGeometry) (w);
	}

	disableTexture (s, texture);

	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	s->activeTexture (GL_TEXTURE0_ARB);

	disableTexture (s, texture);

	glColor4usv (defaultColor);
	screenTexEnvMode (s, GL_REPLACE);

	if (mask & PAINT_WINDOW_BLEND_MASK)
	    glDisable (GL_BLEND);
    }
    else
    {
	enableTexture (s, texture, filter);

	if (mask & PAINT_WINDOW_BLEND_MASK)
	{
	    glEnable (GL_BLEND);
	    if (attrib->opacity != OPAQUE || attrib->brightness != BRIGHT)
	    {
		GLushort color;

		color = (attrib->opacity * attrib->brightness) >> 16;

		screenTexEnvMode (s, GL_MODULATE);
		glColor4us (color, color, color, attrib->opacity);

		(*w->drawWindowGeometry) (w);

		glColor4usv (defaultColor);
		screenTexEnvMode (s, GL_REPLACE);
	    }
	    else
	    {
		(*w->drawWindowGeometry) (w);
	    }

	    glDisable (GL_BLEND);
	}
	else if (attrib->brightness != BRIGHT)
	{
	    screenTexEnvMode (s, GL_MODULATE);
	    glColor4us (attrib->brightness, attrib->brightness,
			attrib->brightness, BRIGHT);

	    (*w->drawWindowGeometry) (w);

	    glColor4usv (defaultColor);
	    screenTexEnvMode (s, GL_REPLACE);
	}
	else
	{
	    (*w->drawWindowGeometry) (w);
	}

	disableTexture (w->screen, texture);
    }
}

void
drawWindowTexture (CompWindow		*w,
		   CompTexture		*texture,
		   const FragmentAttrib	*attrib,
		   unsigned int		mask)
{
    int filter;

    if (mask & (PAINT_WINDOW_TRANSFORMED_MASK |
		PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK))
	filter = w->screen->filter[SCREEN_TRANS_FILTER];
    else
	filter = w->screen->filter[NOTHING_TRANS_FILTER];

    if ((!attrib->nFunction && (!w->screen->lighting ||
	 attrib->saturation == COLOR || attrib->saturation == 0)) ||
	!enableFragmentProgramAndDrawGeometry (w,
					       texture,
					       attrib,
					       filter,
					       mask))
    {
	enableFragmentOperationsAndDrawGeometry (w,
						 texture,
						 attrib,
						 filter,
						 mask);
    }
}

Bool
drawWindow (CompWindow		 *w,
	    const CompTransform  *transform,
	    const FragmentAttrib *fragment,
	    Region		 region,
	    unsigned int	 mask)
{
    if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	region = &infiniteRegion;

    if (!region->numRects)
	return TRUE;

    if (w->attrib.map_state != IsViewable)
	return TRUE;

    if (!w->texture->pixmap && !bindWindow (w))
	return FALSE;

    if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	mask |= PAINT_WINDOW_BLEND_MASK;

    w->vCount = w->indexCount = 0;
    (*w->screen->addWindowGeometry) (w, &w->matrix, 1, w->region, region);
    if (w->vCount)
	(*w->screen->drawWindowTexture) (w, w->texture, fragment, mask);

    return TRUE;
}

Bool
paintWindow (CompWindow		     *w,
	     const WindowPaintAttrib *attrib,
	     const CompTransform     *transform,
	     Region		     region,
	     unsigned int	     mask)
{
    FragmentAttrib fragment;
    Bool	   status;

    w->lastPaint = *attrib;

    if (w->alpha || attrib->opacity != OPAQUE)
	mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

    w->lastMask = mask;

    if (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK)
    {
	if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	    return FALSE;

	if (mask & PAINT_WINDOW_NO_CORE_INSTANCE_MASK)
	    return FALSE;

	if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	    return FALSE;

	if (w->shaded)
	    return FALSE;

	return TRUE;
    }

    if (mask & PAINT_WINDOW_NO_CORE_INSTANCE_MASK)
	return TRUE;

    initFragmentAttrib (&fragment, attrib);

    if (mask & PAINT_WINDOW_TRANSFORMED_MASK ||
        mask & PAINT_WINDOW_WITH_OFFSET_MASK)
    {
	glPushMatrix ();
	glLoadMatrixf (transform->m);
    }

    status = (*w->screen->drawWindow) (w, transform, &fragment, region, mask);

    if (mask & PAINT_WINDOW_TRANSFORMED_MASK ||
        mask & PAINT_WINDOW_WITH_OFFSET_MASK)
	glPopMatrix ();

    return status;
}
