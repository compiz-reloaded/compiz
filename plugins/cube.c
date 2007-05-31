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
 *         Mirco Müller <macslow@bangang.de> (Skydome support)
 */

#include <string.h>
#include <math.h>

#include <X11/Xatom.h>
#include <X11/Xproto.h>

#include <cube.h>

static CompMetadata cubeMetadata;

static int cubeDisplayPrivateIndex;

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static void
cubeLoadImg (CompScreen *s,
	     int	n)
{
    unsigned int    width, height;
    int		    pw, ph;
    CompOptionValue *imgFiles;
    int		    imgNFile;

    CUBE_SCREEN (s);

    imgFiles = cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.value;
    imgNFile = cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.nValue;

    if (!cs->fullscreenOutput)
    {
	pw = s->width;
	ph = s->height;
    }
    else
    {
	pw = s->outputDev[0].width;
	ph = s->outputDev[0].height;
    }

    if (!imgNFile || cs->pw != pw || cs->ph != ph)
    {
	finiTexture (s, &cs->texture);
	initTexture (s, &cs->texture);

	if (!imgNFile)
	    return;
    }

    cs->imgCurFile = n % imgNFile;

    if (!readImageToTexture (s, &cs->texture,
			    imgFiles[cs->imgCurFile].s,
			    &width, &height))
    {
	fprintf (stderr, "%s: Failed to load slide: %s\n",
		 programName, imgFiles[cs->imgCurFile].s);

	finiTexture (s, &cs->texture);
	initTexture (s, &cs->texture);

	return;
    }

    cs->tc[0] = COMP_TEX_COORD_X (&cs->texture.matrix, width / 2.0f);
    cs->tc[1] = COMP_TEX_COORD_Y (&cs->texture.matrix, height / 2.0f);

    if (cs->opt[CUBE_SCREEN_OPTION_SCALE_IMAGE].value.b)
    {
	cs->tc[2] = COMP_TEX_COORD_X (&cs->texture.matrix, width);
	cs->tc[3] = COMP_TEX_COORD_Y (&cs->texture.matrix, 0.0f);

	cs->tc[4] = COMP_TEX_COORD_X (&cs->texture.matrix, 0.0f);
	cs->tc[5] = COMP_TEX_COORD_Y (&cs->texture.matrix, 0.0f);

	cs->tc[6] = COMP_TEX_COORD_X (&cs->texture.matrix, 0.0f);
	cs->tc[7] = COMP_TEX_COORD_Y (&cs->texture.matrix, height);

	cs->tc[8] = COMP_TEX_COORD_X (&cs->texture.matrix, width);
	cs->tc[9] = COMP_TEX_COORD_Y (&cs->texture.matrix, height);

	cs->tc[10] = COMP_TEX_COORD_X (&cs->texture.matrix, width);
	cs->tc[11] = COMP_TEX_COORD_Y (&cs->texture.matrix, 0.0f);
    }
    else
    {
	float x1 = width  / 2.0f - pw / 2.0f;
	float y1 = height / 2.0f - ph / 2.0f;
	float x2 = width  / 2.0f + pw / 2.0f;
	float y2 = height / 2.0f + ph / 2.0f;

	cs->tc[2] = COMP_TEX_COORD_X (&cs->texture.matrix, x2);
	cs->tc[3] = COMP_TEX_COORD_Y (&cs->texture.matrix, y1);

	cs->tc[4] = COMP_TEX_COORD_X (&cs->texture.matrix, x1);
	cs->tc[5] = COMP_TEX_COORD_Y (&cs->texture.matrix, y1);

	cs->tc[6] = COMP_TEX_COORD_X (&cs->texture.matrix, x1);
	cs->tc[7] = COMP_TEX_COORD_Y (&cs->texture.matrix, y2);

	cs->tc[8] = COMP_TEX_COORD_X (&cs->texture.matrix, x2);
	cs->tc[9] = COMP_TEX_COORD_Y (&cs->texture.matrix, y2);

	cs->tc[10] = COMP_TEX_COORD_X (&cs->texture.matrix, x2);
	cs->tc[11] = COMP_TEX_COORD_Y (&cs->texture.matrix, y1);
    }
}

static Bool
cubeUpdateGeometry (CompScreen *s,
		    int	       sides,
		    Bool       invert)
{
    GLfloat radius, distance;
    GLfloat *v;
    int     i, n;

    CUBE_SCREEN (s);

    sides *= cs->nOutput;

    distance = 0.5f / tanf (M_PI / sides);
    radius   = 0.5f / sinf (M_PI / sides);

    n = (sides + 2) * 2;

    if (cs->nvertices != n)
    {
	v = realloc (cs->vertices, sizeof (GLfloat) * n * 3);
	if (!v)
	    return FALSE;

	cs->nvertices = n;
	cs->vertices  = v;
    }
    else
	v = cs->vertices;

    *v++ = 0.0f;
    *v++ = 0.5 * invert;
    *v++ = 0.0f;

    for (i = 0; i <= sides; i++)
    {
	*v++ = radius * sinf (i * 2 * M_PI / sides + M_PI / sides);
	*v++ = 0.5 * invert;
	*v++ = radius * cosf (i * 2 * M_PI / sides + M_PI / sides);
    }

    *v++ = 0.0f;
    *v++ = -0.5 * invert;
    *v++ = 0.0f;

    for (i = sides; i >= 0; i--)
    {
	*v++ = radius * sinf (i * 2 * M_PI / sides + M_PI / sides);
	*v++ = -0.5 * invert;
	*v++ = radius * cosf (i * 2 * M_PI / sides + M_PI / sides);
    }

    cs->invert	 = invert;
    cs->distance = distance;

    return TRUE;
}

static void
cubeUpdateOutputs (CompScreen *s)
{
    BoxPtr pBox0, pBox1;
    int    i, j, k, x;

    CUBE_SCREEN (s);

    k = 0;

    cs->fullscreenOutput = TRUE;

    for (i = 0; i < s->nOutputDev; i++)
    {
	cs->outputMask[i] = -1;

	/* dimensions must match first output */
	if (s->outputDev[i].width  != s->outputDev[0].width ||
	    s->outputDev[i].height != s->outputDev[0].height)
	    continue;

	pBox0 = &s->outputDev[0].region.extents;
	pBox1 = &s->outputDev[i].region.extents;

	/* top and bottom line must match first output */
	if (pBox0->y1 != pBox1->y1 || pBox0->y2 != pBox1->y2)
	    continue;

	k++;

	for (j = 0; j < s->nOutputDev; j++)
	{
	    pBox0 = &s->outputDev[j].region.extents;

	    /* must not intersect other output region */
	    if (i != j && pBox0->x2 > pBox1->x1 && pBox0->x1 < pBox1->x2)
	    {
		k--;
		break;
	    }
	}
    }

    if (k != s->nOutputDev)
    {
	cs->fullscreenOutput = FALSE;
	cs->nOutput = 1;
	return;
    }

    /* add output indices from left to right */
    j = 0;
    for (;;)
    {
	x = MAXSHORT;
	k = -1;

	for (i = 0; i < s->nOutputDev; i++)
	{
	    if (cs->outputMask[i] != -1)
		continue;

	    if (s->outputDev[i].region.extents.x1 < x)
	    {
		x = s->outputDev[i].region.extents.x1;
		k = i;
	    }
	}

	if (k < 0)
	    break;

	cs->outputMask[k] = j;
	cs->output[j]     = k;

	j++;
    }

    cs->nOutput = j;

    if (cs->nOutput == 1)
    {
	if (s->outputDev[0].width  != s->width ||
	    s->outputDev[0].height != s->height)
	    cs->fullscreenOutput = FALSE;
    }
}

static CompOption *
cubeGetScreenOptions (CompPlugin *plugin,
		      CompScreen *screen,
		      int	 *count)
{
    CUBE_SCREEN (screen);

    *count = NUM_OPTIONS (cs);
    return cs->opt;
}

static void
cubeUpdateSkyDomeTexture (CompScreen *screen)
{
    CUBE_SCREEN (screen);

    finiTexture (screen, &cs->sky);
    initTexture (screen, &cs->sky);

    if (!cs->opt[CUBE_SCREEN_OPTION_SKYDOME].value.b)
	return;

    if (strlen (cs->opt[CUBE_SCREEN_OPTION_SKYDOME_IMG].value.s) == 0 ||
	!readImageToTexture (screen,
			     &cs->sky,
			     cs->opt[CUBE_SCREEN_OPTION_SKYDOME_IMG].value.s,
			     NULL,
			     NULL))
    {
	GLfloat aaafTextureData[128][128][3];
	GLfloat fRStart = (GLfloat)
	    cs->opt[CUBE_SCREEN_OPTION_SKYDOME_GRAD_START].value.c[0] / 0xffff;
	GLfloat fGStart = (GLfloat)
	    cs->opt[CUBE_SCREEN_OPTION_SKYDOME_GRAD_START].value.c[1] / 0xffff;
	GLfloat fBStart = (GLfloat)
	    cs->opt[CUBE_SCREEN_OPTION_SKYDOME_GRAD_START].value.c[2] / 0xffff;
	GLfloat fREnd = (GLfloat)
	    cs->opt[CUBE_SCREEN_OPTION_SKYDOME_GRAD_END].value.c[0] / 0xffff;
	GLfloat fGEnd = (GLfloat)
	    cs->opt[CUBE_SCREEN_OPTION_SKYDOME_GRAD_END].value.c[1] / 0xffff;
	GLfloat fBEnd = (GLfloat)
	    cs->opt[CUBE_SCREEN_OPTION_SKYDOME_GRAD_END].value.c[2] / 0xffff;
	GLfloat fRStep = (fREnd - fRStart) / 128.0f;
	GLfloat fGStep = (fGEnd - fGStart) / 128.0f;
	GLfloat fBStep = (fBStart - fBEnd) / 128.0f;
	GLfloat fR = fRStart;
	GLfloat fG = fGStart;
	GLfloat fB = fBStart;

	int	iX, iY;

	for (iX = 127; iX >= 0; iX--)
	{
	    fR += fRStep;
	    fG += fGStep;
	    fB -= fBStep;

	    for (iY = 0; iY < 128; iY++)
	    {
		aaafTextureData[iX][iY][0] = fR;
		aaafTextureData[iX][iY][1] = fG;
		aaafTextureData[iX][iY][2] = fB;
	    }
	}

	cs->sky.target = GL_TEXTURE_2D;
	cs->sky.filter = GL_LINEAR;
	cs->sky.wrap   = GL_CLAMP_TO_EDGE;

	glGenTextures (1, &cs->sky.name);
	glBindTexture (cs->sky.target, cs->sky.name);

	glTexParameteri (cs->sky.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (cs->sky.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri (cs->sky.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri (cs->sky.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D (cs->sky.target,
		      0,
		      GL_RGB,
		      128,
		      128,
		      0,
		      GL_RGB,
		      GL_FLOAT,
		      aaafTextureData);

	glBindTexture (cs->sky.target, 0);
    }
}

static Bool
fillCircleTable (GLfloat   **ppSint,
		 GLfloat   **ppCost,
		 const int n)
{
    const GLfloat angle = 2 * M_PI / (GLfloat) ((n == 0) ? 1 : n);
    const int	  size = abs (n);
    int		  i;

    *ppSint = (GLfloat *) calloc (sizeof (GLfloat), size + 1);
    *ppCost = (GLfloat *) calloc (sizeof (GLfloat), size + 1);

    if (!(*ppSint) || !(*ppCost))
    {
	free (*ppSint);
	free (*ppCost);

	return FALSE;
    }

    (*ppSint)[0] = 0.0;
    (*ppCost)[0] = 1.0;

    for (i = 1; i < size; i++)
    {
	(*ppSint)[i] = sin (angle * i);
	(*ppCost)[i] = cos (angle * i);
    }

    (*ppSint)[size] = (*ppSint)[0];
    (*ppCost)[size] = (*ppCost)[0];

    return TRUE;
}

static void
cubeUpdateSkyDomeList (CompScreen *s,
		       GLfloat	  fRadius)
{
    GLint   iSlices = 128;
    GLint   iStacks = 64;
    GLfloat afTexCoordX[4];
    GLfloat afTexCoordY[4];
    GLfloat *sint1;
    GLfloat *cost1;
    GLfloat *sint2;
    GLfloat *cost2;
    GLfloat r;
    GLfloat x;
    GLfloat y;
    GLfloat z;
    int	    i;
    int	    j;
    int	    iStacksStart;
    int	    iStacksEnd;
    int	    iSlicesStart;
    int	    iSlicesEnd;
    GLfloat fStepX;
    GLfloat fStepY;

    CUBE_SCREEN (s);

    if (cs->opt[CUBE_SCREEN_OPTION_SKYDOME_ANIM].value.b)
    {
	iStacksStart = 11; /* min.   0 */
	iStacksEnd = 53;   /* max.  64 */
	iSlicesStart = 0;  /* min.   0 */
	iSlicesEnd = 128;  /* max. 128 */
    }
    else
    {
	iStacksStart = 21; /* min.   0 */
	iStacksEnd = 43;   /* max.  64 */
	iSlicesStart = 21; /* min.   0 */
	iSlicesEnd = 44;   /* max. 128 */
    }

    fStepX = 1.0 / (GLfloat) (iSlicesEnd - iSlicesStart);
    fStepY = 1.0 / (GLfloat) (iStacksEnd - iStacksStart);

    if (!fillCircleTable (&sint1, &cost1, -iSlices))
	return;

    if (!fillCircleTable (&sint2, &cost2, iStacks * 2))
    {
	free (sint1);
	free (cost1);
	return;
    }

    afTexCoordX[0] = 1.0f;
    afTexCoordY[0] = fStepY;
    afTexCoordX[1] = 1.0f - fStepX;
    afTexCoordY[1] = fStepY;
    afTexCoordX[2] = 1.0f - fStepX;
    afTexCoordY[2] = 0.0f;
    afTexCoordX[3] = 1.0f;
    afTexCoordY[3] = 0.0f;

    if (!cs->skyListId)
	cs->skyListId = glGenLists (1);

    glNewList (cs->skyListId, GL_COMPILE);

    enableTexture (s, &cs->sky, COMP_TEXTURE_FILTER_GOOD);

    glBegin (GL_QUADS);

    for (i = iStacksStart; i < iStacksEnd; i++)
    {
	afTexCoordX[0] = 1.0f;
	afTexCoordX[1] = 1.0f - fStepX;
	afTexCoordX[2] = 1.0f - fStepX;
	afTexCoordX[3] = 1.0f;

	for (j = iSlicesStart; j < iSlicesEnd; j++)
	{
	    /* bottom-right */
	    z = cost2[i];
	    r = sint2[i];
	    x = cost1[j];
	    y = sint1[j];

	    glTexCoord2f (afTexCoordX[3], afTexCoordY[3]);
	    glVertex3f (x * r * fRadius, y * r * fRadius, z * fRadius);

	    /* top-right */
	    z = cost2[i + 1];
	    r = sint2[i + 1];
	    x = cost1[j];
	    y = sint1[j];

	    glTexCoord2f (afTexCoordX[0], afTexCoordY[0]);
	    glVertex3f (x * r * fRadius, y * r * fRadius, z * fRadius);

	    /* top-left */
	    z = cost2[i + 1];
	    r = sint2[i + 1];
	    x = cost1[j + 1];
	    y = sint1[j + 1];

	    glTexCoord2f (afTexCoordX[1], afTexCoordY[1]);
	    glVertex3f (x * r * fRadius, y * r * fRadius, z * fRadius);

	    /* bottom-left */
	    z = cost2[i];
	    r = sint2[i];
	    x = cost1[j + 1];
	    y = sint1[j + 1];

	    glTexCoord2f (afTexCoordX[2], afTexCoordY[2]);
	    glVertex3f (x * r * fRadius, y * r * fRadius, z * fRadius);

	    afTexCoordX[0] -= fStepX;
	    afTexCoordX[1] -= fStepX;
	    afTexCoordX[2] -= fStepX;
	    afTexCoordX[3] -= fStepX;
	}

	afTexCoordY[0] += fStepY;
	afTexCoordY[1] += fStepY;
	afTexCoordY[2] += fStepY;
	afTexCoordY[3] += fStepY;
    }

    glEnd ();

    disableTexture (s, &cs->sky);

    glEndList ();

    free (sint1);
    free (cost1);
    free (sint2);
    free (cost2);
}

static void
cubeUnloadBackgrounds (CompScreen *s)
{
    CUBE_SCREEN (s);

    if (cs->nBg)
    {
	int i;

	for (i = 0; i < cs->nBg; i++)
	    finiTexture (s, &cs->bg[i]);

	free (cs->bg);

	cs->bg  = NULL;
	cs->nBg = 0;
    }
}

static void
cubeLoadBackground (CompScreen *s,
		    int	       n)
{
    CompOptionValue *value;
    unsigned int    width, height;
    int		    i;

    CUBE_SCREEN (s);

    value = &cs->opt[CUBE_SCREEN_OPTION_BACKGROUNDS].value;

    if (!cs->bg)
    {
	cs->bg = malloc (sizeof (CompTexture) * value->list.nValue);
	if (!cs->bg)
	    return;

	for (i = 0; i < value->list.nValue; i++)
	    initTexture (s, &cs->bg[i]);

	cs->nBg = value->list.nValue;
    }

    if (cs->bg[n].target)
    {
	if (readImageToTexture (s, &cs->bg[n], value->list.value[n].s,
				&width, &height))
	{
	    if (cs->fullscreenOutput)
	    {
		cs->bg[n].matrix.xx *= (float) width  / s->outputDev[0].width;
		cs->bg[n].matrix.yy *= (float) height / s->outputDev[0].height;
	    }
	    else
	    {
		cs->bg[n].matrix.xx *= (float) width  / s->width;
		cs->bg[n].matrix.yy *= (float) height / s->height;
	    }
	}
	else
	{
	    cs->bg[n].target = 0;
	}
    }
}

static Bool
cubeSetScreenOption (CompPlugin      *plugin,
		     CompScreen      *screen,
		     char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    CUBE_SCREEN (screen);

    o = compFindOption (cs->opt, NUM_OPTIONS (cs), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case CUBE_SCREEN_OPTION_COLOR:
	if (compSetColorOption (o, value))
	{
	    memcpy (cs->color, o->value.c, sizeof (cs->color));
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_IN:
	if (compSetBoolOption (o, value))
	{
	    if (cubeUpdateGeometry (screen, screen->hsize, o->value.b ? -1 : 1))
		return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_SCALE_IMAGE:
	if (compSetBoolOption (o, value))
	{
	    cubeLoadImg (screen, cs->imgCurFile);
	    damageScreen (screen);

	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_IMAGES:
	if (compSetOptionList (o, value))
	{
	    cubeLoadImg (screen, cs->imgCurFile);
	    damageScreen (screen);

	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_SKYDOME:
	if (compSetBoolOption (o, value))
	{
	    cubeUpdateSkyDomeTexture (screen);
	    cubeUpdateSkyDomeList (screen, 1.0f);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_SKYDOME_IMG:
	if (compSetStringOption (o, value))
	{
	    cubeUpdateSkyDomeTexture (screen);
	    cubeUpdateSkyDomeList (screen, 1.0f);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_SKYDOME_ANIM:
	if (compSetBoolOption (o, value))
	{
	    cubeUpdateSkyDomeTexture (screen);
	    cubeUpdateSkyDomeList (screen, 1.0f);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_SKYDOME_GRAD_START:
	if (compSetColorOption (o, value))
	{
	    cubeUpdateSkyDomeTexture (screen);
	    cubeUpdateSkyDomeList (screen, 1.0f);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_SKYDOME_GRAD_END:
	if (compSetColorOption (o, value))
	{
	    cubeUpdateSkyDomeTexture (screen);
	    cubeUpdateSkyDomeList (screen, 1.0f);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_BACKGROUNDS:
	if (compSetOptionList (o, value))
	{
	    cubeUnloadBackgrounds (screen);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    default:
	return compSetScreenOption (screen, o, value);
    }

    return FALSE;
}

static int
adjustVelocity (CubeScreen *cs)
{
    float unfold, adjust, amount;

    if (cs->unfolded)
	unfold = 1.0f - cs->unfold;
    else
	unfold = 0.0f - cs->unfold;

    adjust = unfold * 0.02f * cs->opt[CUBE_SCREEN_OPTION_ACCELERATION].value.f;
    amount = fabs (unfold);
    if (amount < 1.0f)
	amount = 1.0f;
    else if (amount > 3.0f)
	amount = 3.0f;

    cs->unfoldVelocity = (amount * cs->unfoldVelocity + adjust) /
	(amount + 2.0f);

    return (fabs (unfold) < 0.002f && fabs (cs->unfoldVelocity) < 0.01f);
}

static void
cubePreparePaintScreen (CompScreen *s,
			int	   msSinceLastPaint)
{
    CUBE_SCREEN (s);

    if (cs->grabIndex)
    {
	int   steps;
	float amount, chunk;

	amount = msSinceLastPaint * 0.2f *
	    cs->opt[CUBE_SCREEN_OPTION_SPEED].value.f;
	steps  = amount / (0.5f * cs->opt[CUBE_SCREEN_OPTION_TIMESTEP].value.f);
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    cs->unfold += cs->unfoldVelocity * chunk;
	    if (cs->unfold > 1.0f)
		cs->unfold = 1.0f;

	    if (adjustVelocity (cs))
	    {
		if (cs->unfold < 0.5f)
		{
		    if (cs->grabIndex)
		    {
			removeScreenGrab (s, cs->grabIndex, NULL);
			cs->grabIndex = 0;
		    }

		    cs->unfold = 0.0f;
		}
		break;
	    }
	}
    }

    memset (cs->cleared, 0, sizeof (Bool) * s->nOutputDev);

    UNWRAP (cs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (cs, s, preparePaintScreen, cubePreparePaintScreen);

}

static Bool
cubePaintOutput (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 const CompTransform	*transform,
		 Region			 region,
		 CompOutput		 *output,
		 unsigned int		 mask)
{
    Bool status;
    int i;

    CUBE_SCREEN (s);

    if (cs->grabIndex)
    {
	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_TRANSFORMED_MASK;
    }

    cs->srcOutput = 0;
    for (i = 0; i < s->nOutputDev; i++)
	if (!memcmp (output, &s->outputDev[i], sizeof (CompOutput)))
	    cs->srcOutput = i;

    UNWRAP (cs, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (cs, s, paintOutput, cubePaintOutput);

    return status;
}

static void
cubeDonePaintScreen (CompScreen *s)
{
    CUBE_SCREEN (s);

    if (cs->grabIndex)
	damageScreen (s);

    UNWRAP (cs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (cs, s, donePaintScreen, cubeDonePaintScreen);
}

static void
cubeMoveViewportAndPaint (CompScreen		  *s,
			  const ScreenPaintAttrib *sAttrib,
			  const CompTransform	  *transform,
			  CompOutput		  *outputPtr,
			  unsigned int		  mask,
			  int			  dx)
{
    int i, output = 0;
    
    CUBE_SCREEN (s);

    for (i = 0; i < s->nOutputDev; i++)
	if (!memcmp (outputPtr, &s->outputDev[i], sizeof (CompOutput)))
	    output = i;
    
    if (cs->nOutput > 1)
    {
	int cubeOutput, dView;

	/* translate to cube output */
	cubeOutput = cs->outputMask[output];

	/* convert from window movement to viewport movement */
	dView = -dx;

	cubeOutput += dView;

	dView      = cubeOutput / cs->nOutput;
	cubeOutput = cubeOutput % cs->nOutput;

	if (cubeOutput < 0)
	{
	    cubeOutput += cs->nOutput;
	    dView--;
	}

	/* translate back to compiz output */
	output = cs->srcOutput = cs->output[cubeOutput];

	moveScreenViewport (s, -dView, 0, FALSE);
	(*s->paintTransformedOutput) (s, sAttrib, transform,
				      &s->outputDev[output].region,
				      &s->outputDev[output], mask);
	moveScreenViewport (s, dView, 0, FALSE);
    }
    else
    {
	moveScreenViewport (s, dx, 0, FALSE);
	(*s->paintTransformedOutput) (s, sAttrib, transform, &s->region,
				      outputPtr, mask);
	moveScreenViewport (s, -dx, 0, FALSE);
    }
}

static void
cubeGetRotation (CompScreen *s,
		 float	    *x,
		 float	    *v)
{
    *x = 0.0f;
    *v = 0.0f;
}

static void
cubeClearTargetOutput (CompScreen *s,
		       float	  xRotate,
		       float	  vRotate)
{
    CUBE_SCREEN (s);

    if (cs->sky.name)
    {
	screenLighting (s, FALSE);

	glPushMatrix ();

	if (cs->opt[CUBE_SCREEN_OPTION_SKYDOME_ANIM].value.b &&
	    cs->grabIndex == 0)
	{
	    glRotatef (xRotate, 0.0f, 1.0f, 0.0f);
	    glRotatef (vRotate / 5.0f + 90.0f, 1.0f, 0.0f, 0.0f);
	}
	else
	{
	    glRotatef (90.0f, 1.0f, 0.0f, 0.0f);
	}

	glCallList (cs->skyListId);
	glPopMatrix ();
    }
    else
    {
	clearTargetOutput (s->display, GL_COLOR_BUFFER_BIT);
    }
}

static void
cubePaintTopBottom (CompScreen		    *s,
		    const ScreenPaintAttrib *sAttrib,
		    const CompTransform	    *transform,
		    CompOutput		    *output,
		    int			    size)
{
    ScreenPaintAttrib sa = *sAttrib;
    CompTransform     sTransform = *transform;

    CUBE_SCREEN (s);

    screenLighting (s, TRUE);

    glColor3usv (cs->color);

    glPushMatrix ();

    sa.yRotate += (360.0f / size) * (cs->xrotations + 1);
    if (!cs->opt[CUBE_SCREEN_OPTION_ADJUST_IMAGE].value.b)
	sa.yRotate -= (360.0f / size) * s->x;

    (*s->applyScreenTransform) (s, &sa, output, &sTransform);

    glLoadMatrixf (sTransform.m);
    glTranslatef (cs->outputXOffset, -cs->outputYOffset, 0.0f);
    glScalef (cs->outputXScale, cs->outputYScale, 1.0f);

    glVertexPointer (3, GL_FLOAT, 0, cs->vertices);

    glNormal3f (0.0f, -1.0f, 0.0f);

    if (cs->invert == 1 && size == 4 && cs->texture.name)
    {
	enableTexture (s, &cs->texture, COMP_TEXTURE_FILTER_GOOD);
	glTexCoordPointer (2, GL_FLOAT, 0, cs->tc);
	glDrawArrays (GL_TRIANGLE_FAN, 0, cs->nvertices >> 1);
	disableTexture (s, &cs->texture);
	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    }
    else
    {
	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	glDrawArrays (GL_TRIANGLE_FAN, 0, cs->nvertices >> 1);
    }

    glNormal3f (0.0f, 1.0f, 0.0f);

    glDrawArrays (GL_TRIANGLE_FAN, cs->nvertices >> 1, cs->nvertices >> 1);

    glNormal3f (0.0f, 0.0f, -1.0f);

    glPopMatrix ();

    glColor4usv (defaultColor);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
}

static void
cubePaintTransformedOutput (CompScreen		    *s,
			    const ScreenPaintAttrib *sAttrib,
			    const CompTransform	    *transform,
			    Region		    region,
			    CompOutput		    *outputPtr,
			    unsigned int	    mask)
{
    ScreenPaintAttrib sa = *sAttrib;
    float	      xRotate, vRotate;
    int		      hsize, xMove = 0;
    float	      size;
    Bool	      clear;
    int i, output = 0;

    CUBE_SCREEN (s);

    for (i = 0; i < s->nOutputDev; i++)
	if (!memcmp (outputPtr, &s->outputDev[i], sizeof (CompOutput)))
	    output = i;

    hsize = s->hsize * cs->nOutput;
    size  = hsize;

    if (!cs->fullscreenOutput)
    {
	cs->outputXScale = (float) s->width / s->outputDev[output].width;
	cs->outputYScale = (float) s->height / s->outputDev[output].height;

	cs->outputXOffset =
	    (s->width / 2.0f -
	     (s->outputDev[output].region.extents.x1 +
	      s->outputDev[output].region.extents.x2) / 2.0f) /
	    (float) s->outputDev[output].width;

	cs->outputYOffset =
	    (s->height / 2.0f -
	     (s->outputDev[output].region.extents.y1 +
	      s->outputDev[output].region.extents.y2) / 2.0f) /
	    (float) s->outputDev[output].height;
    }
    else
    {
	cs->outputXScale  = 1.0f;
	cs->outputYScale  = 1.0f;
	cs->outputXOffset = 0.0f;
	cs->outputYOffset = 0.0f;
    }

    (*cs->getRotation) (s, &xRotate, &vRotate);

    sa.xRotate += xRotate;
    sa.vRotate += vRotate;

    clear = cs->cleared[output];
    if (!clear)
    {
	(*cs->clearTargetOutput) (s, xRotate, vRotate);
	cs->cleared[output] = TRUE;
    }

    mask &= ~PAINT_SCREEN_CLEAR_MASK;

    UNWRAP (cs, s, paintTransformedOutput);

    sa.xTranslate = sAttrib->xTranslate;
    sa.yTranslate = sAttrib->yTranslate;

    if (cs->grabIndex)
    {
	sa.vRotate = 0.0f;

	size += cs->unfold * 8.0f;
	size += powf (cs->unfold, 6) * 64.0;
	size += powf (cs->unfold, 16) * 8192.0;

	sa.zTranslate = -cs->invert * (0.5f / tanf (M_PI / size));

	/* distance we move the camera back when unfolding the cube.
	   currently hardcoded to 1.5 but it should probably be optional. */
	sa.zCamera -= cs->unfold * 1.5f;

	sa.xRotate = xRotate * cs->invert;
	if (sa.xRotate > 0.0f)
	{
	    cs->xrotations = (int) (hsize * sa.xRotate) / 360;
	    sa.xRotate = sa.xRotate - (360.0f * cs->xrotations) / hsize;
	}
	else
	{
	    cs->xrotations = (int) (hsize * sa.xRotate) / 360;
	    sa.xRotate = sa.xRotate -
		(360.0f * cs->xrotations) / hsize + 360.0f / hsize;
	    cs->xrotations--;
	}

	sa.xRotate = sa.xRotate / size * hsize;
    }
    else
    {
	if (vRotate > 100.0f)
	    sa.vRotate = 100.0f;
	else if (vRotate < -100.0f)
	    sa.vRotate = -100.0f;
	else
	    sa.vRotate = vRotate;

	sa.zTranslate = -cs->invert * cs->distance;
	sa.xRotate = xRotate * cs->invert;
	if (sa.xRotate > 0.0f)
	{
	    cs->xrotations = (int) (size * sa.xRotate) / 360;
	    sa.xRotate = sa.xRotate - (360.0f * cs->xrotations) / size;
	}
	else
	{
	    cs->xrotations = (int) (size * sa.xRotate) / 360;
	    sa.xRotate = sa.xRotate -
		(360.0f * cs->xrotations) / size + 360.0f / size;
	    cs->xrotations--;
	}
    }

    if (!clear && cs->grabIndex == 0 && hsize > 2 &&
	(cs->invert != 1 || sa.vRotate != 0.0f || sa.yTranslate != 0.0f))
    {
	(*cs->paintTopBottom) (s, &sa, transform, outputPtr, hsize);
    }

    /* outside cube */
    if (cs->invert == 1)
    {
	if (cs->grabIndex || hsize > 4)
	{
	    GLenum filter;
	    int    i;

	    xMove = cs->xrotations - ((hsize >> 1) - 1);
	    sa.yRotate += (360.0f / size) * ((hsize >> 1) - 1);

	    filter = s->display->textureFilter;
	    if (cs->grabIndex && cs->opt[CUBE_SCREEN_OPTION_MIPMAP].value.b)
		s->display->textureFilter = GL_LINEAR_MIPMAP_LINEAR;

	    for (i = 0; i < hsize; i++)
	    {
		cubeMoveViewportAndPaint (s, &sa, transform, outputPtr, mask,
					  xMove);

		sa.yRotate -= 360.0f / size;
		xMove++;
	    }

	    s->display->textureFilter = filter;
	}
	else
	{
	    if (xRotate != 0.0f)
	    {
		xMove = cs->xrotations;

		cubeMoveViewportAndPaint (s, &sa, transform, outputPtr, mask,
					  xMove);

		xMove++;
	    }

	    sa.yRotate -= 360.0f / size;

	    cubeMoveViewportAndPaint (s, &sa, transform, outputPtr, mask,
				      xMove);
	}
    }
    else
    {
	if (sa.xRotate > 180.0f / size)
	{
	    sa.yRotate -= 360.0f / size;
	    cs->xrotations++;
	}

	sa.yRotate -= 360.0f / size;
	xMove = -1 - cs->xrotations;

	if (cs->grabIndex)
	{
	    GLenum filter;
	    int    i;

	    filter = s->display->textureFilter;
	    if (cs->opt[CUBE_SCREEN_OPTION_MIPMAP].value.b)
		s->display->textureFilter = GL_LINEAR_MIPMAP_LINEAR;

	    if (sa.xRotate > 180.0f / size)
	    {
		xMove -= ((hsize >> 1) - 2);
		sa.yRotate -= (360.0f / size) * ((hsize >> 1) - 2);
	    }
	    else
	    {
		xMove -= ((hsize >> 1) - 1);
		sa.yRotate -= (360.0f / size) * ((hsize >> 1) - 1);
	    }

	    for (i = 0; i < hsize; i++)
	    {
		cubeMoveViewportAndPaint (s, &sa, transform, outputPtr, mask,
					  xMove);

		sa.yRotate += 360.0f / size;
		xMove++;
	    }

	    s->display->textureFilter = filter;
	}
	else
	{
	    cubeMoveViewportAndPaint (s, &sa, transform, outputPtr, mask,
				      xMove);

	    sa.yRotate += 360.0f / size;
	    xMove = -cs->xrotations;

	    cubeMoveViewportAndPaint (s, &sa, transform, outputPtr, mask,
				      xMove);

	    sa.yRotate += 360.0f / size;
	    xMove = 1 - cs->xrotations;

	    cubeMoveViewportAndPaint (s, &sa, transform, outputPtr, mask,
				      xMove);
	}
    }

    WRAP (cs, s, paintTransformedOutput, cubePaintTransformedOutput);
}

static void
cubePaintBackground (CompScreen   *s,
		     Region	  region,
		     unsigned int mask)
{
    int n;

    CUBE_SCREEN (s);

    n = cs->opt[CUBE_SCREEN_OPTION_BACKGROUNDS].value.list.nValue;
    if (n)
    {
	CompTexture *bg;
	CompMatrix  matrix;
	BoxPtr      pBox = region->rects;
	int	    nBox = region->numRects;
	GLfloat     *d, *data;

	if (!nBox)
	    return;

	n = (s->x * cs->nOutput + cs->srcOutput) % n;

	if (s->desktopWindowCount)
	{
	    cubeUnloadBackgrounds (s);
	    return;
	}
	else
	{
	    if (!cs->nBg || !cs->bg[n].name)
		cubeLoadBackground (s, n);
	}

	bg = &cs->bg[n];

	matrix = bg->matrix;
	matrix.x0 -= (cs->srcOutput * s->outputDev[0].width) * matrix.xx;

	data = malloc (sizeof (GLfloat) * nBox * 16);
	if (!data)
	    return;

	d = data;
	n = nBox;
	while (n--)
	{
	    *d++ = COMP_TEX_COORD_X (&matrix, pBox->x1);
	    *d++ = COMP_TEX_COORD_Y (&matrix, pBox->y2);

	    *d++ = pBox->x1;
	    *d++ = pBox->y2;

	    *d++ = COMP_TEX_COORD_X (&matrix, pBox->x2);
	    *d++ = COMP_TEX_COORD_Y (&matrix, pBox->y2);

	    *d++ = pBox->x2;
	    *d++ = pBox->y2;

	    *d++ = COMP_TEX_COORD_X (&matrix, pBox->x2);
	    *d++ = COMP_TEX_COORD_Y (&matrix, pBox->y1);

	    *d++ = pBox->x2;
	    *d++ = pBox->y1;

	    *d++ = COMP_TEX_COORD_X (&matrix, pBox->x1);
	    *d++ = COMP_TEX_COORD_Y (&matrix, pBox->y1);

	    *d++ = pBox->x1;
	    *d++ = pBox->y1;

	    pBox++;
	}

	glTexCoordPointer (2, GL_FLOAT, sizeof (GLfloat) * 4, data);
	glVertexPointer (2, GL_FLOAT, sizeof (GLfloat) * 4, data + 2);

	if (bg->name)
	{
	    enableTexture (s, bg, COMP_TEXTURE_FILTER_GOOD);
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
    else
    {
	UNWRAP (cs, s, paintBackground);
	(*s->paintBackground) (s, region, mask);
	WRAP (cs, s, paintBackground, cubePaintBackground);
    }
}

static void
cubeApplyScreenTransform (CompScreen		  *s,
			  const ScreenPaintAttrib *sAttrib,
			  CompOutput		  *output,
			  CompTransform	          *transform)
{
    CUBE_SCREEN (s);

    matrixTranslate (transform, cs->outputXOffset, -cs->outputYOffset, 0.0f);
    matrixScale (transform, cs->outputXScale, cs->outputYScale, 1.0f);

    UNWRAP (cs, s, applyScreenTransform);
    (*s->applyScreenTransform) (s, sAttrib, output, transform);
    WRAP (cs, s, applyScreenTransform, cubeApplyScreenTransform);

    matrixScale (transform, 1.0f / cs->outputXScale,
		 1.0f / cs->outputYScale, 1.0f);
    matrixTranslate (transform, -cs->outputXOffset, cs->outputYOffset, 0.0f);
}

static Bool
cubeUnfold (CompDisplay     *d,
	    CompAction      *action,
	    CompActionState state,
	    CompOption      *option,
	    int		    nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	CUBE_SCREEN (s);

	if (s->hsize * cs->nOutput < 4)
	    return FALSE;

	if (otherScreenGrabExist (s, "rotate", "switcher", "cube", 0))
	    return FALSE;

	if (!cs->grabIndex)
	    cs->grabIndex = pushScreenGrab (s, s->invisibleCursor, "cube");

	if (cs->grabIndex)
	{
	    cs->unfolded = TRUE;
	    damageScreen (s);
	}

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	if (state & CompActionStateInitKey)
	    action->state |= CompActionStateTermKey;
    }

    return FALSE;
}

static Bool
cubeFold (CompDisplay     *d,
	  CompAction      *action,
	  CompActionState state,
	  CompOption      *option,
	  int		  nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	CUBE_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (cs->grabIndex)
	{
	    cs->unfolded = FALSE;
	    damageScreen (s);
	}
    }

    action->state &= ~(CompActionStateTermButton | CompActionStateTermKey);

    return FALSE;
}

static Bool
cubeNextImage (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	int imgNFile;

	CUBE_SCREEN (s);

	imgNFile = cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.nValue;
	if (imgNFile)
	{
	    cubeLoadImg (s, (cs->imgCurFile + 1) % imgNFile);
	    damageScreen (s);
	}
    }

    return FALSE;
}

static Bool
cubePrevImage (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	int imgNFile;

	CUBE_SCREEN (s);

	imgNFile = cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.nValue;
	if (imgNFile)
	{
	    cubeLoadImg (s, (cs->imgCurFile - 1 + imgNFile) % imgNFile);
	    damageScreen (s);
	}
    }

    return FALSE;
}

static void
cubeOutputChangeNotify (CompScreen *s)
{
    CUBE_SCREEN (s);

    cubeUpdateOutputs (s);
    cubeUnloadBackgrounds (s);
    cubeUpdateGeometry (s, s->hsize, cs->invert);

    if (cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.nValue)
	cubeLoadImg (s, cs->imgCurFile);

    UNWRAP (cs, s, outputChangeNotify);
    (*s->outputChangeNotify) (s);
    WRAP (cs, s, outputChangeNotify, cubeOutputChangeNotify);
}

static Bool
cubeSetGlobalScreenOption (CompScreen      *s,
			   char		   *name,
			   CompOptionValue *value)
{
    Bool status;

    CUBE_SCREEN (s);

    UNWRAP (cs, s, setScreenOption);
    status = (*s->setScreenOption) (s, name, value);
    WRAP (cs, s, setScreenOption, cubeSetGlobalScreenOption);

    if (status && strcmp (name, "hsize") == 0)
    {
	cubeUpdateGeometry (s, s->hsize, cs->invert);
	cubeUnloadBackgrounds (s);
    }

    return status;
}

static CompOption *
cubeGetDisplayOptions (CompPlugin  *plugin,
		       CompDisplay *display,
		       int	   *count)
{
    CUBE_DISPLAY (display);

    *count = NUM_OPTIONS (cd);
    return cd->opt;
}

static Bool
cubeSetDisplayOption (CompPlugin      *plugin,
		      CompDisplay     *display,
		      char	      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    CUBE_DISPLAY (display);

    o = compFindOption (cd->opt, NUM_OPTIONS (cd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case CUBE_DISPLAY_OPTION_ABI:
    case CUBE_DISPLAY_OPTION_INDEX:
	break;
    default:
	return compSetDisplayOption (display, o, value);
    }

    return FALSE;
}

static const CompMetadataOptionInfo cubeDisplayOptionInfo[] = {
    { "abi", "int", 0, 0, 0 },
    { "index", "int", 0, 0, 0 },
    { "unfold", "action", 0, cubeUnfold, cubeFold },
    { "next_slide", "action", "<passive_grab>false</passive_grab>",
      cubeNextImage, 0 },
    { "prev_slide", "action", "<passive_grab>false</passive_grab>",
      cubePrevImage, 0 }
};

static Bool
cubeInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    CubeDisplay *cd;

    cd = malloc (sizeof (CubeDisplay));
    if (!cd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &cubeMetadata,
					     cubeDisplayOptionInfo,
					     cd->opt,
					     CUBE_DISPLAY_OPTION_NUM))
    {
	free (cd);
	return FALSE;
    }

    cd->opt[CUBE_DISPLAY_OPTION_ABI].value.i   = CUBE_ABIVERSION;
    cd->opt[CUBE_DISPLAY_OPTION_INDEX].value.i = cubeDisplayPrivateIndex;

    cd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (cd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, cd->opt, CUBE_DISPLAY_OPTION_NUM);
	free (cd);
	return FALSE;
    }

    d->privates[cubeDisplayPrivateIndex].ptr = cd;

    return TRUE;
}

static void
cubeFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    CUBE_DISPLAY (d);

    freeScreenPrivateIndex (d, cd->screenPrivateIndex);

    compFiniDisplayOptions (d, cd->opt, CUBE_DISPLAY_OPTION_NUM);

    free (cd);
}

static const CompMetadataOptionInfo cubeScreenOptionInfo[] = {
    { "color", "color", 0, 0, 0 },
    { "in", "bool", 0, 0, 0 },
    { "scale_image", "bool", 0, 0, 0 },
    { "images", "list", "<type>string</type>", 0, 0 },
    { "skydome", "bool", 0, 0, 0 },
    { "skydome_image", "string", 0, 0, 0 },
    { "skydome_animated", "bool", 0, 0, 0 },
    { "skydome_gradient_start_color", "color", 0, 0, 0 },
    { "skydome_gradient_end_color", "color", 0, 0, 0 },
    { "acceleration", "float", "<min>1.0</min>", 0, 0 },
    { "speed", "float", "<min>0.1</min>", 0, 0 },
    { "timestep", "float", "<min>0.1</min>", 0, 0 },
    { "mipmap", "bool", 0, 0, 0 },
    { "backgrounds", "list", "<type>string</type>", 0, 0 },
    { "adjust_image", "bool", 0, 0, 0 }
};

static Bool
cubeInitScreen (CompPlugin *p,
		CompScreen *s)
{
    CubeScreen *cs;

    CUBE_DISPLAY (s->display);

    cs = malloc (sizeof (CubeScreen));
    if (!cs)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &cubeMetadata,
					    cubeScreenOptionInfo,
					    cs->opt,
					    CUBE_SCREEN_OPTION_NUM))
    {
	free (cs);
	return FALSE;
    }

    cs->pw = 0;
    cs->ph = 0;

    cs->invert = 1;

    cs->tc[0] = cs->tc[1] = cs->tc[2] = cs->tc[3] = 0.0f;
    cs->tc[4] = cs->tc[5] = cs->tc[6] = cs->tc[7] = 0.0f;

    memcpy (cs->color, cs->opt[CUBE_SCREEN_OPTION_COLOR].value.c,
	    sizeof (cs->color));

    cs->nvertices = 0;
    cs->vertices  = NULL;

    cs->grabIndex = 0;

    cs->srcOutput = 0;

    cs->skyListId = 0;

    cs->getRotation	  = cubeGetRotation;
    cs->clearTargetOutput = cubeClearTargetOutput;
    cs->paintTopBottom    = cubePaintTopBottom;

    s->privates[cd->screenPrivateIndex].ptr = cs;

    initTexture (s, &cs->texture);
    initTexture (s, &cs->sky);

    cs->imgCurFile = 0;

    cs->unfolded = FALSE;
    cs->unfold   = 0.0f;

    cs->unfoldVelocity = 0.0f;

    cs->fullscreenOutput = TRUE;

    cs->bg  = NULL;
    cs->nBg = 0;

    memset (cs->cleared, 0, sizeof (cs->cleared));

    cubeUpdateOutputs (s);

    if (!cubeUpdateGeometry (s, s->hsize, cs->invert))
    {
	compFiniScreenOptions (s, cs->opt, CUBE_SCREEN_OPTION_NUM);
	free (cs);
	return FALSE;
    }

    if (cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.nValue)
    {
	cubeLoadImg (s, cs->imgCurFile);
	damageScreen (s);
    }

    WRAP (cs, s, preparePaintScreen, cubePreparePaintScreen);
    WRAP (cs, s, donePaintScreen, cubeDonePaintScreen);
    WRAP (cs, s, paintOutput, cubePaintOutput);
    WRAP (cs, s, paintTransformedOutput, cubePaintTransformedOutput);
    WRAP (cs, s, paintBackground, cubePaintBackground);
    WRAP (cs, s, applyScreenTransform, cubeApplyScreenTransform);
    WRAP (cs, s, setScreenOption, cubeSetGlobalScreenOption);
    WRAP (cs, s, outputChangeNotify, cubeOutputChangeNotify);

    return TRUE;
}

static void
cubeFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    CUBE_SCREEN (s);

    if (cs->skyListId)
	glDeleteLists (cs->skyListId, 1);

    UNWRAP (cs, s, preparePaintScreen);
    UNWRAP (cs, s, donePaintScreen);
    UNWRAP (cs, s, paintOutput);
    UNWRAP (cs, s, paintTransformedOutput);
    UNWRAP (cs, s, paintBackground);
    UNWRAP (cs, s, applyScreenTransform);
    UNWRAP (cs, s, setScreenOption);
    UNWRAP (cs, s, outputChangeNotify);

    finiTexture (s, &cs->texture);
    finiTexture (s, &cs->sky);

    cubeUnloadBackgrounds (s);

    compFiniScreenOptions (s, cs->opt, CUBE_SCREEN_OPTION_NUM);

    free (cs);
}

static Bool
cubeInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&cubeMetadata,
					 p->vTable->name,
					 cubeDisplayOptionInfo,
					 CUBE_DISPLAY_OPTION_NUM,
					 cubeScreenOptionInfo,
					 CUBE_SCREEN_OPTION_NUM))
	return FALSE;

    cubeDisplayPrivateIndex = allocateDisplayPrivateIndex ();
    if (cubeDisplayPrivateIndex < 0)
    {
	compFiniMetadata (&cubeMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&cubeMetadata, p->vTable->name);

    return TRUE;
}

static void
cubeFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (cubeDisplayPrivateIndex);
    compFiniMetadata (&cubeMetadata);
}

static int
cubeGetVersion (CompPlugin *plugin,
		int	   version)
{
    return ABIVERSION;
}

static CompMetadata *
cubeGetMetadata (CompPlugin *plugin)
{
    return &cubeMetadata;
}

CompPluginDep cubeDeps[] = {
    { CompPluginRuleBefore, "scale" },
    { CompPluginRuleBefore, "switcher" }
};

CompPluginFeature cubeFeatures[] = {
    { "largedesktop" }
};

CompPluginVTable cubeVTable = {
    "cube",
    cubeGetVersion,
    cubeGetMetadata,
    cubeInit,
    cubeFini,
    cubeInitDisplay,
    cubeFiniDisplay,
    cubeInitScreen,
    cubeFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    cubeGetDisplayOptions,
    cubeSetDisplayOption,
    cubeGetScreenOptions,
    cubeSetScreenOption,
    cubeDeps,
    sizeof (cubeDeps) / sizeof (cubeDeps[0]),
    cubeFeatures,
    sizeof (cubeFeatures) / sizeof (cubeFeatures[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &cubeVTable;
}
