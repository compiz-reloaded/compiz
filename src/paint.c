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

#include <compiz.h>

ScreenPaintAttrib defaultScreenPaintAttrib = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -DEFAULT_Z_CAMERA
};

WindowPaintAttrib defaultWindowPaintAttrib = {
    OPAQUE, 1.0f, 1.0f
};

void
preparePaintScreen (CompScreen *screen,
		    int	       msSinceLastPaint) {}

void
donePaintScreen (CompScreen *screen) {}

void
applyScreenTransform (CompScreen	      *screen,
		      const ScreenPaintAttrib *sAttrib,
		      int		      output)
{
    glTranslatef (sAttrib->xTranslate,
		  sAttrib->yTranslate,
		  sAttrib->zTranslate + sAttrib->zCamera);

    glRotatef (sAttrib->xRotate, 0.0f, 1.0f, 0.0f);
    glRotatef (sAttrib->vRotate,
	       1.0f - sAttrib->xRotate / 90.0f,
	       0.0f,
	       sAttrib->xRotate / 90.0f);
    glRotatef (sAttrib->yRotate, 0.0f, 1.0f, 0.0f);
}

void
prepareXCoords (CompScreen *screen,
		int	   output,
		float      z)
{
    glTranslatef (-0.5f, -0.5f, z);
    glScalef (1.0f  / screen->outputDev[output].width,
	      -1.0f / screen->outputDev[output].height,
	      1.0f);
    glTranslatef (-screen->outputDev[output].region.extents.x1,
		  -screen->outputDev[output].region.extents.y2,
		  0.0f);
}

void
paintTransformedScreen (CompScreen		*screen,
			const ScreenPaintAttrib *sAttrib,
			Region			region,
			int			output,
			unsigned int		mask)
{
    CompWindow *w;
    int	       windowMask;
    int	       backgroundMask;

    if (mask & PAINT_SCREEN_CLEAR_MASK)
	clearScreenOutput (screen, output, GL_COLOR_BUFFER_BIT);

    screenLighting (screen, TRUE);

    glPushMatrix ();

    (screen->applyScreenTransform) (screen, sAttrib, output);

    prepareXCoords (screen, output, -sAttrib->zTranslate);

    if (mask & PAINT_SCREEN_TRANSFORMED_MASK)
    {
	windowMask = PAINT_WINDOW_ON_TRANSFORMED_SCREEN_MASK;
	backgroundMask = PAINT_BACKGROUND_ON_TRANSFORMED_SCREEN_MASK;

	if (mask & PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK)
	{
	    backgroundMask |= PAINT_BACKGROUND_WITH_STENCIL_MASK;

	    (*screen->paintBackground) (screen, region, backgroundMask);

	    glEnable (GL_STENCIL_TEST);

	    for (w = screen->windows; w; w = w->next)
	    {
		if (w->destroyed)
		    continue;

		if (!w->shaded)
		{
		    if (w->attrib.map_state != IsViewable || !w->damaged)
			continue;
		}

		(*screen->paintWindow) (w, &w->paint, region, windowMask);
	    }

	    glDisable (GL_STENCIL_TEST);

	    glPopMatrix ();

	    return;
	}
    }
    else
	windowMask = backgroundMask = 0;

    (*screen->paintBackground) (screen, region, backgroundMask);

    for (w = screen->windows; w; w = w->next)
    {
	if (w->destroyed)
	    continue;

	if (!w->shaded)
	{
	    if (w->attrib.map_state != IsViewable || !w->damaged)
		continue;
	}

	(*screen->paintWindow) (w, &w->paint, region, windowMask);
    }

    glPopMatrix ();
}

Bool
paintScreen (CompScreen		     *screen,
	     const ScreenPaintAttrib *sAttrib,
	     Region		     region,
	     int		     output,
	     unsigned int	     mask)
{
    static Region tmpRegion = NULL;
    CompWindow	  *w;

    if (mask & PAINT_SCREEN_REGION_MASK)
    {
	if (mask & PAINT_SCREEN_TRANSFORMED_MASK)
	{
	    if (mask & PAINT_SCREEN_FULL_MASK)
	    {
		region = &screen->outputDev[output].region;

		(*screen->paintTransformedScreen) (screen, sAttrib, region,
						   output, mask);

		return TRUE;
	    }

	    return FALSE;
	}

	/* fall through and redraw region */
    }
    else if (mask & PAINT_SCREEN_FULL_MASK)
    {
	(*screen->paintTransformedScreen) (screen, sAttrib,
					   &screen->outputDev[output].region,
					   output, mask);

	return TRUE;
    }
    else
	return FALSE;

    screenLighting (screen, FALSE);

    glPushMatrix ();

    prepareXCoords (screen, output, -DEFAULT_Z_CAMERA);

    if (mask & PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK)
    {
	(*screen->paintBackground) (screen, region, 0);

	/* paint all windows from bottom to top */
	for (w = screen->windows; w; w = w->next)
	{
	    if (w->destroyed)
		continue;

	    if (!w->shaded)
	    {
		if (w->attrib.map_state != IsViewable || !w->damaged)
		    continue;
	    }

	    (*screen->paintWindow) (w, &w->paint, region, 0);
	}
    }
    else
    {
	int cnt = 0;

	if (!tmpRegion)
	{
	    tmpRegion = XCreateRegion ();
	    if (!tmpRegion)
		return FALSE;
	}

	XSubtractRegion (region, &emptyRegion, tmpRegion);

	/* paint solid windows */
	for (w = screen->reverseWindows; w; w = w->prev)
	{
	    if (w->destroyed)
		continue;

	    if (!w->shaded)
	    {
		if (w->attrib.map_state != IsViewable || !w->damaged)
		    continue;
	    }

	    if ((*screen->paintWindow) (w, &w->paint, tmpRegion,
					PAINT_WINDOW_SOLID_MASK))
	    {
		XSubtractRegion (tmpRegion, w->region, tmpRegion);

		/* unredirect top most fullscreen windows. */
		if (cnt == 0						  &&
		    !REGION_NOT_EMPTY (tmpRegion)			  &&
		    screen->opt[COMP_SCREEN_OPTION_UNREDIRECT_FS].value.b &&
		    XEqualRegion (w->region, &screen->region))
		{
		    unredirectWindow (w);
		}
	    }

	    /* copy region */
	    XSubtractRegion (tmpRegion, &emptyRegion, w->clip);

	    cnt++;
	}

	if (tmpRegion->numRects)
	    (*screen->paintBackground) (screen, tmpRegion, 0);

	/* paint translucent windows */
	for (w = screen->windows; w; w = w->next)
	{
	    if (w->destroyed)
		continue;

	    if (!w->shaded)
	    {
		if (w->attrib.map_state != IsViewable || !w->damaged)
		    continue;
	    }

	    (*screen->paintWindow) (w, &w->paint, w->clip,
				    PAINT_WINDOW_TRANSLUCENT_MASK);
	}
    }

    glPopMatrix ();

    return TRUE;
}

#define ADD_RECT(data, m, n, x1, y1, x2, y2)	   \
    for (it = 0; it < n; it++)			   \
    {						   \
	*(data)++ = COMP_TEX_COORD_X (&m[it], x1); \
	*(data)++ = COMP_TEX_COORD_Y (&m[it], y2); \
    }						   \
    *(data)++ = (x1);				   \
    *(data)++ = (y2);				   \
    for (it = 0; it < n; it++)			   \
    {						   \
	*(data)++ = COMP_TEX_COORD_X (&m[it], x2); \
	*(data)++ = COMP_TEX_COORD_Y (&m[it], y2); \
    }						   \
    *(data)++ = (x2);				   \
    *(data)++ = (y2);				   \
    for (it = 0; it < n; it++)			   \
    {						   \
	*(data)++ = COMP_TEX_COORD_X (&m[it], x2); \
	*(data)++ = COMP_TEX_COORD_Y (&m[it], y1); \
    }						   \
    *(data)++ = (x2);				   \
    *(data)++ = (y1);				   \
    for (it = 0; it < n; it++)			   \
    {						   \
	*(data)++ = COMP_TEX_COORD_X (&m[it], x1); \
	*(data)++ = COMP_TEX_COORD_Y (&m[it], y1); \
    }						   \
    *(data)++ = (x1);				   \
    *(data)++ = (y1)

#define ADD_QUAD(data, m, n, x1, y1, x2, y2)		\
    for (it = 0; it < n; it++)				\
    {							\
	*(data)++ = COMP_TEX_COORD_XY (&m[it], x1, y2);	\
	*(data)++ = COMP_TEX_COORD_YX (&m[it], x1, y2);	\
    }							\
    *(data)++ = (x1);					\
    *(data)++ = (y2);					\
    for (it = 0; it < n; it++)				\
    {							\
	*(data)++ = COMP_TEX_COORD_XY (&m[it], x2, y2);	\
	*(data)++ = COMP_TEX_COORD_YX (&m[it], x2, y2);	\
    }							\
    *(data)++ = (x2);					\
    *(data)++ = (y2);					\
    for (it = 0; it < n; it++)				\
    {							\
	*(data)++ = COMP_TEX_COORD_XY (&m[it], x2, y1);	\
	*(data)++ = COMP_TEX_COORD_YX (&m[it], x2, y1);	\
    }							\
    *(data)++ = (x2);					\
    *(data)++ = (y1);					\
    for (it = 0; it < n; it++)				\
    {							\
	*(data)++ = COMP_TEX_COORD_XY (&m[it], x1, y1);	\
	*(data)++ = COMP_TEX_COORD_YX (&m[it], x1, y1);	\
    }							\
    *(data)++ = (x1);					\
    *(data)++ = (y1)


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
	    if (matrix[it].xy != 0.0f && matrix[it].yx != 0.0f)
	    {
		rect = FALSE;
		break;
	    }
	}

	pBox = region->rects;
	nBox = region->numRects;

	vSize = 2 + nMatrix * 2;

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
	w->vCount = n * 4;
    }
}

void
drawWindowGeometry (CompWindow *w)
{
    int     texUnit = w->texUnits;
    int     currentTexUnit = 0;
    int     stride = (1 + texUnit) * 2;
    GLfloat *vertices = w->vertices + (stride - 2);

    stride *= sizeof (GLfloat);

    glVertexPointer (2, GL_FLOAT, stride, vertices);

    while (texUnit--)
    {
	if (texUnit != currentTexUnit)
	{
	    (*w->screen->clientActiveTexture) (GL_TEXTURE0_ARB + texUnit);
	    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	    currentTexUnit = texUnit;
	}
	vertices -= 2;
	glTexCoordPointer (2, GL_FLOAT, stride, vertices);
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
drawWindowTexture (CompWindow		   *w,
		   CompTexture		   *texture,
		   const WindowPaintAttrib *attrib,
		   unsigned int		   mask)
{
    int filter;

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

    if (w->screen->canDoSaturated && attrib->saturation != COLOR)
    {
	GLfloat constant[4];

	if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	    glEnable (GL_BLEND);

	enableTexture (w->screen, texture, filter);

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

	w->screen->activeTexture (GL_TEXTURE1_ARB);

	enableTexture (w->screen, texture, filter);

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);

	glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
	glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

	if (w->screen->canDoSlightlySaturated && attrib->saturation > 0)
	{
	    glTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	    glTexEnvf (GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
	    glTexEnvf (GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

	    constant[0] = 0.5f + 0.5f * RED_SATURATION_WEIGHT;
	    constant[1] = 0.5f + 0.5f * GREEN_SATURATION_WEIGHT;
	    constant[2] = 0.5f + 0.5f * BLUE_SATURATION_WEIGHT;
	    constant[3] = 1.0;

	    glTexEnvfv (GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant);

	    w->screen->activeTexture (GL_TEXTURE2_ARB);

	    enableTexture (w->screen, texture, filter);

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
		w->screen->activeTexture (GL_TEXTURE3_ARB);

		enableTexture (w->screen, texture, filter);

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

		(*w->screen->drawWindowGeometry) (w);

		disableTexture (w->screen, texture);

		glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		w->screen->activeTexture (GL_TEXTURE2_ARB);
	    }
	    else
	    {
		(*w->screen->drawWindowGeometry) (w);
	    }

	    disableTexture (w->screen, texture);

	    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	    w->screen->activeTexture (GL_TEXTURE1_ARB);
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

	    (*w->screen->drawWindowGeometry) (w);
	}

	disableTexture (w->screen, texture);

	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	w->screen->activeTexture (GL_TEXTURE0_ARB);

	disableTexture (w->screen, texture);

	glColor4usv (defaultColor);
	screenTexEnvMode (w->screen, GL_REPLACE);

	if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	    glDisable (GL_BLEND);
    }
    else
    {
	enableTexture (w->screen, texture, filter);

	if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
	{
	    glEnable (GL_BLEND);
	    if (attrib->opacity != OPAQUE || attrib->brightness != BRIGHT)
	    {
		GLushort color;

		color = (attrib->opacity * attrib->brightness) >> 16;

		screenTexEnvMode (w->screen, GL_MODULATE);
		glColor4us (color, color, color, attrib->opacity);

		(*w->screen->drawWindowGeometry) (w);

		glColor4usv (defaultColor);
		screenTexEnvMode (w->screen, GL_REPLACE);
	    }
	    else
	    {
		(*w->screen->drawWindowGeometry) (w);
	    }

	    glDisable (GL_BLEND);
	}
	else if (attrib->brightness != BRIGHT)
	{
	    screenTexEnvMode (w->screen, GL_MODULATE);
	    glColor4us (attrib->brightness, attrib->brightness,
			attrib->brightness, BRIGHT);

	    (*w->screen->drawWindowGeometry) (w);

	    glColor4usv (defaultColor);
	    screenTexEnvMode (w->screen, GL_REPLACE);
	}
	else
	{
	    (*w->screen->drawWindowGeometry) (w);
	}

	disableTexture (w->screen, texture);
    }

    glPopMatrix ();
}

Bool
paintWindow (CompWindow		     *w,
	     const WindowPaintAttrib *attrib,
	     Region		     region,
	     unsigned int	     mask)
{
    if (mask & PAINT_WINDOW_TRANSFORMED_MASK)
	region = &infiniteRegion;

    if (!region->numRects)
	return TRUE;

    if (mask & PAINT_WINDOW_SOLID_MASK)
    {
	if (w->attrib.map_state != IsViewable)
	    return FALSE;

	if (w->alpha)
	    return FALSE;

	if (attrib->opacity != OPAQUE)
	    return FALSE;
    }
    else if (mask & PAINT_WINDOW_TRANSLUCENT_MASK)
    {
	if (!w->alpha && attrib->opacity == OPAQUE)
	    return FALSE;
    }
    else
    {
	if (w->attrib.map_state != IsViewable)
	    return TRUE;

	if (w->alpha || attrib->opacity != OPAQUE)
	    mask |= PAINT_WINDOW_TRANSLUCENT_MASK;
	else
	    mask |= PAINT_WINDOW_SOLID_MASK;
    }

    if (!w->texture->pixmap)
    {
	bindWindow (w);
	if (!w->mapNum)
	    return FALSE;
    }

    w->vCount = 0;
    (*w->screen->addWindowGeometry) (w, &w->matrix, 1, w->region, region);
    if (w->vCount)
    {
	w->lastPaint = *attrib;
	(*w->screen->drawWindowTexture) (w, w->texture, attrib, mask);
    }

    return TRUE;
}

void
paintBackground (CompScreen   *s,
		 Region	      region,
		 unsigned int mask)
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

	if (!(mask & PAINT_BACKGROUND_WITH_STENCIL_MASK))
	    return;
    }
    else
    {
	if (!bg->name)
	    updateScreenBackground (s, bg);
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

    if (mask & PAINT_BACKGROUND_WITH_STENCIL_MASK)
    {
	glEnable (GL_STENCIL_TEST);
	glStencilFunc (GL_ALWAYS, s->stencilRef, ~0);
	glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE);
    }

    glTexCoordPointer (2, GL_FLOAT, sizeof (GLfloat) * 4, data);
    glVertexPointer (2, GL_FLOAT, sizeof (GLfloat) * 4, data + 2);

    if (s->desktopWindowCount)
    {
	glDrawArrays (GL_QUADS, 0, nBox * 4);
    }
    else
    {
	if (mask & PAINT_BACKGROUND_ON_TRANSFORMED_SCREEN_MASK)
	    enableTexture (s, bg, COMP_TEXTURE_FILTER_GOOD);
	else
	    enableTexture (s, bg, COMP_TEXTURE_FILTER_FAST);

	glDrawArrays (GL_QUADS, 0, nBox * 4);

	disableTexture (s, bg);
    }

    if (mask & PAINT_BACKGROUND_WITH_STENCIL_MASK)
    {
	glStencilFunc (GL_EQUAL, s->stencilRef, ~0);
	glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
	glDisable (GL_STENCIL_TEST);
    }

    free (data);
}
