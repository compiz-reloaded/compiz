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

#define MULTM(x, y, z) \
z[0] = x[0] * y[0] + x[4] * y[1] + x[8] * y[2] + x[12] * y[3]; \
z[1] = x[1] * y[0] + x[5] * y[1] + x[9] * y[2] + x[13] * y[3]; \
z[2] = x[2] * y[0] + x[6] * y[1] + x[10] * y[2] + x[14] * y[3]; \
z[3] = x[3] * y[0] + x[7] * y[1] + x[11] * y[2] + x[15] * y[3]; \
z[4] = x[0] * y[4] + x[4] * y[5] + x[8] * y[6] + x[12] * y[7]; \
z[5] = x[1] * y[4] + x[5] * y[5] + x[9] * y[6] + x[13] * y[7]; \
z[6] = x[2] * y[4] + x[6] * y[5] + x[10] * y[6] + x[14] * y[7]; \
z[7] = x[3] * y[4] + x[7] * y[5] + x[11] * y[6] + x[15] * y[7]; \
z[8] = x[0] * y[8] + x[4] * y[9] + x[8] * y[10] + x[12] * y[11]; \
z[9] = x[1] * y[8] + x[5] * y[9] + x[9] * y[10] + x[13] * y[11]; \
z[10] = x[2] * y[8] + x[6] * y[9] + x[10] * y[10] + x[14] * y[11]; \
z[11] = x[3] * y[8] + x[7] * y[9] + x[11] * y[10] + x[15] * y[11]; \
z[12] = x[0] * y[12] + x[4] * y[13] + x[8] * y[14] + x[12] * y[15]; \
z[13] = x[1] * y[12] + x[5] * y[13] + x[9] * y[14] + x[13] * y[15]; \
z[14] = x[2] * y[12] + x[6] * y[13] + x[10] * y[14] + x[14] * y[15]; \
z[15] = x[3] * y[12] + x[7] * y[13] + x[11] * y[14] + x[15] * y[15];

#define MULTMV(m, v) { \
float v0 = m[0]*v[0] + m[4]*v[1] + m[8]*v[2] + m[12]*v[3]; \
float v1 = m[1]*v[0] + m[5]*v[1] + m[9]*v[2] + m[13]*v[3]; \
float v2 = m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14]*v[3]; \
float v3 = m[3]*v[0] + m[7]*v[1] + m[11]*v[2] + m[15]*v[3]; \
v[0] = v0; v[1] = v1; v[2] = v2; v[3] = v3; }

#define DIVV(v) \
v[0] /= v[3]; \
v[1] /= v[3]; \
v[2] /= v[3]; \
v[3] /= v[3];

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
	compLogMessage (s->display, "cube", CompLogLevelWarn,
			"Failed to load slide: %s",
			imgFiles[cs->imgCurFile].s);

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

    if (cs->nVertices != n)
    {
	v = realloc (cs->vertices, sizeof (GLfloat) * n * 3);
	if (!v)
	    return FALSE;

	cs->nVertices = n;
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
			     &cs->skyW,
			     &cs->skyH))
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

	cs->sky.matrix.xx = 1.0 / 128.0;
	cs->sky.matrix.yy = -1.0 / 128.0;
	cs->sky.matrix.xy = 0;
	cs->sky.matrix.yx = 0;
	cs->sky.matrix.x0 = 0;
	cs->sky.matrix.y0 = 1.0;

	cs->skyW = 128;
	cs->skyH = 128;

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
    afTexCoordY[0] = 1.0f - fStepY;
    afTexCoordX[1] = 1.0f - fStepX;
    afTexCoordY[1] = 1.0f - fStepY;
    afTexCoordX[2] = 1.0f - fStepX;
    afTexCoordY[2] = 1.0f;
    afTexCoordX[3] = 1.0f;
    afTexCoordY[3] = 1.0f;

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

	    glTexCoord2f (
		COMP_TEX_COORD_X( &cs->sky.matrix, afTexCoordX[3] * cs->skyW),
		COMP_TEX_COORD_Y( &cs->sky.matrix, afTexCoordY[3] * cs->skyH));
	    glVertex3f (x * r * fRadius, y * r * fRadius, z * fRadius);

	    /* top-right */
	    z = cost2[i + 1];
	    r = sint2[i + 1];
	    x = cost1[j];
	    y = sint1[j];

	    glTexCoord2f (
		COMP_TEX_COORD_X( &cs->sky.matrix, afTexCoordX[0] * cs->skyW),
		COMP_TEX_COORD_Y( &cs->sky.matrix, afTexCoordY[0] * cs->skyH));
	    glVertex3f (x * r * fRadius, y * r * fRadius, z * fRadius);

	    /* top-left */
	    z = cost2[i + 1];
	    r = sint2[i + 1];
	    x = cost1[j + 1];
	    y = sint1[j + 1];

	    glTexCoord2f (
		COMP_TEX_COORD_X( &cs->sky.matrix, afTexCoordX[1] * cs->skyW),
		COMP_TEX_COORD_Y( &cs->sky.matrix, afTexCoordY[1] * cs->skyH));
	    glVertex3f (x * r * fRadius, y * r * fRadius, z * fRadius);

	    /* bottom-left */
	    z = cost2[i];
	    r = sint2[i];
	    x = cost1[j + 1];
	    y = sint1[j + 1];

	    glTexCoord2f (
		COMP_TEX_COORD_X( &cs->sky.matrix, afTexCoordX[2] * cs->skyW),
		COMP_TEX_COORD_Y( &cs->sky.matrix, afTexCoordY[2] * cs->skyH));
	    glVertex3f (x * r * fRadius, y * r * fRadius, z * fRadius);

	    afTexCoordX[0] -= fStepX;
	    afTexCoordX[1] -= fStepX;
	    afTexCoordX[2] -= fStepX;
	    afTexCoordX[3] -= fStepX;
	}

	afTexCoordY[0] -= fStepY;
	afTexCoordY[1] -= fStepY;
	afTexCoordY[2] -= fStepY;
	afTexCoordY[3] -= fStepY;
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
    int opt;

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

    /* Transparency handling */
    if (cs->rotationState == RotationManual ||
	(cs->rotationState == RotationChange &&
	 cs->opt[CUBE_SCREEN_OPTION_TRANSPARENT_MANUAL_ONLY].value.b))
    {
	opt = CUBE_SCREEN_OPTION_ACTIVE_OPACITY;
    }
    else
    {
	opt = CUBE_SCREEN_OPTION_INACTIVE_OPACITY;
    }

    cs->toOpacity = (cs->opt[opt].value.f / 100.0f) * OPAQUE;

    if (cs->opt[CUBE_SCREEN_OPTION_FADE_TIME].value.f == 0.0f)
	cs->desktopOpacity = cs->toOpacity;
    else if (cs->desktopOpacity != cs->toOpacity)
    {
	float steps = (msSinceLastPaint * OPAQUE / 1000.0) /
	              cs->opt[CUBE_SCREEN_OPTION_FADE_TIME].value.f;
	if (steps < 12)
	    steps = 12;

	if (cs->toOpacity > cs->desktopOpacity)
	{
	    cs->desktopOpacity += steps;
	    cs->desktopOpacity = MIN (cs->toOpacity, cs->desktopOpacity);
	}
	if (cs->toOpacity < cs->desktopOpacity)
	{
	    cs->desktopOpacity -= steps;
	    cs->desktopOpacity = MAX (cs->toOpacity, cs->desktopOpacity);
	}
    }

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

    CUBE_SCREEN (s);

    if (cs->grabIndex || cs->desktopOpacity != OPAQUE)
    {
	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_TRANSFORMED_MASK;
    }

    cs->srcOutput = (output->id != ~0) ? output->id : 0;
    /* Always use BTF painting on non-transformed screen */
    cs->paintOrder = BTF;

    UNWRAP (cs, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (cs, s, paintOutput, cubePaintOutput);

    return status;
}

static void
cubeDonePaintScreen (CompScreen *s)
{
    CUBE_SCREEN (s);

    if (cs->grabIndex || cs->desktopOpacity != cs->toOpacity)
	damageScreen (s);

    UNWRAP (cs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (cs, s, donePaintScreen, cubeDonePaintScreen);
}

static Bool
cubeCheckFTB (CompScreen              *s,
              const ScreenPaintAttrib *sAttrib,
              const CompTransform     *transform,
              CompOutput              *outputPtr)
{
    CompTransform sTransform = *transform;
    float         mvp[16];
    float         pntA[4], pntB[4], pntC[4];
    float         vecA[3], vecB[3];
    float         ortho[3];

    (*s->applyScreenTransform) (s, sAttrib, outputPtr, &sTransform);
    transformToScreenSpace (s, outputPtr, -sAttrib->zTranslate, &sTransform);

    MULTM (s->projection, sTransform.m, mvp);

    pntA[0] = outputPtr->region.extents.x1;
    pntA[1] = outputPtr->region.extents.y1,
    pntA[2] = 0.0f;
    pntA[3] = 1.0f;

    pntB[0] = outputPtr->region.extents.x2;
    pntB[1] = outputPtr->region.extents.y1;
    pntB[2] = 0.0f;
    pntB[3] = 1.0f;

    pntC[0] = outputPtr->region.extents.x1 + outputPtr->width / 2.0f;
    pntC[1] = outputPtr->region.extents.y1 + outputPtr->height / 2.0f;
    pntC[2] = 0.0f;
    pntC[3] = 1.0f;

    MULTMV (mvp, pntA);
    DIVV (pntA);

    MULTMV (mvp, pntB);
    DIVV (pntB);

    MULTMV (mvp, pntC);
    DIVV (pntC);

    vecA[0] = pntC[0] - pntA[0];
    vecA[1] = pntC[1] - pntA[1];
    vecA[2] = pntC[2] - pntA[2];

    vecB[0] = pntC[0] - pntB[0];
    vecB[1] = pntC[1] - pntB[1];
    vecB[2] = pntC[2] - pntB[2];

    ortho[0] = vecA[1] * vecB[2] - vecA[2] * vecB[1];
    ortho[1] = vecA[2] * vecB[0] - vecA[0] * vecB[2];
    ortho[2] = vecA[0] * vecB[1] - vecA[1] * vecB[0];

    if (ortho[2] > 0.0f && pntC[2] > DEFAULT_Z_CAMERA)
    {
	/* The viewport is reversed, should be painted front to back. */
	return TRUE;
    }

    return FALSE;
}

static void
cubeMoveViewportAndPaint (CompScreen		  *s,
			  const ScreenPaintAttrib *sAttrib,
			  const CompTransform	  *transform,
			  CompOutput		  *outputPtr,
			  unsigned int		  mask,
			  PaintOrder              paintOrder,
			  int			  dx)
{
    Bool ftb;
    int  output;

    CUBE_SCREEN (s);

    ftb = cubeCheckFTB (s, sAttrib, transform, outputPtr);

    if ((paintOrder == FTB && !ftb) ||
        (paintOrder == BTF && ftb))
	return;

    output = (outputPtr->id != ~0) ? outputPtr->id : 0;

    cs->paintOrder = paintOrder;

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
cubePaintAllViewports (CompScreen          *s,
                       ScreenPaintAttrib   *sAttrib,
	               const CompTransform *transform,
                       Region              region,
                       CompOutput          *outputPtr,
                       unsigned int        mask,
                       int                 xMove,
                       float               size,
                       int                 hsize,
                       PaintOrder          paintOrder)
{
    CUBE_SCREEN(s);

    ScreenPaintAttrib sa = *sAttrib;

    int i;
    int xMoveAdd;
    int origXMoveAdd = 0; /* dx for the viewport we start
			     painting with (back-most). */
    int iFirstSign;       /* 1 if we do xMove += i first and
			     -1 if we do xMove -= i first. */

    if (cs->invert == 1)
    {
	/* xMove ==> dx for the viewport which is the
	   nearest to the viewer in z axis.
	   xMove +/- hsize / 2 ==> dx for the viewport
	   which is the farthest to the viewer in z axis. */

	if ((sa.xRotate < 0.0f && hsize % 2 == 1) ||
	    (sa.xRotate > 0.0f && hsize % 2 == 0))
	{
	    origXMoveAdd = hsize / 2;
	    iFirstSign = 1;
	}
	else
	{
	    origXMoveAdd = -hsize / 2;
	    iFirstSign = -1;
	}
    }
    else
    {
	/* xMove is already the dx for farthest viewport. */
	if (sa.xRotate > 0.0f)
	    iFirstSign = -1;
	else
	    iFirstSign = 1;
    }

    for (i = 0; i <= hsize / 2; i++)
    {
	/* move to the correct viewport (back to front). */
	xMoveAdd = origXMoveAdd;	/* move to farthest viewport. */
	xMoveAdd += iFirstSign * i;	/* move i more viewports to
					   the right / left. */

	/* Needed especially for unfold.
	   We paint the viewports around xMove viewport.
	   Adding or subtracting hsize from xMove has no effect on
	   what viewport we paint, but can make shorter paths. */
	if (xMoveAdd < -hsize / 2)
	    xMoveAdd += hsize;
	else if (xMoveAdd > hsize / 2)
	    xMoveAdd -= hsize;

	/* Paint the viewport. */
	xMove += xMoveAdd;

	sa.yRotate -= cs->invert * xMoveAdd * 360.0f / size;
	cubeMoveViewportAndPaint (s, &sa, transform, outputPtr, mask,
	                          paintOrder, xMove);
	sa.yRotate += cs->invert * xMoveAdd * 360.0f / size;

	xMove -= xMoveAdd;

	/* do the same for an equally far viewport. */
	if (i == 0 || i * 2 == hsize)
	    continue;

	xMoveAdd = origXMoveAdd;	/* move to farthest viewport. */
	xMoveAdd -= iFirstSign * i;	/* move i more viewports to the
					   left / right (opposite side
					   from the one chosen first) */

	if (xMoveAdd < -hsize / 2)
	    xMoveAdd += hsize;
	else if (xMoveAdd > hsize / 2)
	    xMoveAdd -= hsize;

	xMove += xMoveAdd;

	sa.yRotate -= cs->invert * xMoveAdd * 360.0f / size;
	cubeMoveViewportAndPaint (s, &sa, transform, outputPtr, mask,
	                          paintOrder, xMove);
	sa.yRotate += cs->invert * xMoveAdd * 360.0f / size;

	xMove -= xMoveAdd;
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
    int               i;

    CUBE_SCREEN (s);

    screenLighting (s, TRUE);

    glColor4us (cs->color[0], cs->color[1], cs->color[2], cs->desktopOpacity);

    glPushMatrix ();

    sa.yRotate += (360.0f / size) * (cs->xRotations + 1);
    if (!cs->opt[CUBE_SCREEN_OPTION_ADJUST_IMAGE].value.b)
	sa.yRotate -= (360.0f / size) * s->x;

    (*s->applyScreenTransform) (s, &sa, output, &sTransform);

    glLoadMatrixf (sTransform.m);
    glTranslatef (cs->outputXOffset, -cs->outputYOffset, 0.0f);
    glScalef (cs->outputXScale, cs->outputYScale, 1.0f);

    if (cs->desktopOpacity != OPAQUE)
    {
	screenTexEnvMode (s, GL_MODULATE);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glVertexPointer (3, GL_FLOAT, 0, cs->vertices);

    for (i = 0; i < 2; i++)
    {
	if ((i == 0 && sAttrib->vRotate <= 0.0f) ||
	    (i == 1 && sAttrib->vRotate > 0.0f))
	{
	    glNormal3f (0.0f, -1.0f, 0.0f);
	    if (cs->invert == 1 && size == 4 && cs->texture.name)
	    {
		enableTexture (s, &cs->texture, COMP_TEXTURE_FILTER_GOOD);
		glTexCoordPointer (2, GL_FLOAT, 0, cs->tc);
		glDrawArrays (GL_TRIANGLE_FAN, 0, cs->nVertices >> 1);
		disableTexture (s, &cs->texture);
		glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	    }
	    else
	    {
		glDisableClientState (GL_TEXTURE_COORD_ARRAY);
		glDrawArrays (GL_TRIANGLE_FAN, 0, cs->nVertices >> 1);
	    }
	}
	else
	{
	    glNormal3f (0.0f, 1.0f, 0.0f);
	    glDrawArrays (GL_TRIANGLE_FAN, cs->nVertices >> 1,
			  cs->nVertices >> 1);
	}
    }

    glNormal3f (0.0f, 0.0f, -1.0f);

    glPopMatrix ();

    glColor4usv (defaultColor);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

    screenTexEnvMode (s, GL_REPLACE);
    glDisable (GL_BLEND);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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
    GLenum            filter = s->display->textureFilter;
    PaintOrder        paintOrder;
    Bool	      clear;
    Bool              wasCulled = FALSE;
    int output = 0;

    CUBE_SCREEN (s);

    output = (outputPtr->id != ~0) ? outputPtr->id : 0;

    hsize = s->hsize * cs->nOutput;
    size  = hsize;

    if (cs->desktopOpacity != OPAQUE)
    {
	wasCulled = glIsEnabled (GL_CULL_FACE);
	if (wasCulled)
	    glDisable (GL_CULL_FACE);
    }

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
    }

    if (sa.xRotate > 0.0f)
	cs->xRotations = (int) (hsize * sa.xRotate + 180.0f) / 360.0f;
    else
	cs->xRotations = (int) (hsize * sa.xRotate - 180.0f) / 360.0f;

    sa.xRotate -= (360.0f * cs->xRotations) / hsize;
    sa.xRotate *= cs->invert;

    sa.xRotate = sa.xRotate / size * hsize;

    xMove = cs->xRotations;

    if (cs->grabIndex && cs->opt[CUBE_SCREEN_OPTION_MIPMAP].value.b)
	s->display->textureFilter = GL_LINEAR_MIPMAP_LINEAR;

    if (cs->invert == 1)
    {
	/* Outside cube - start with FTB faces */
	paintOrder = FTB;
    }
    else
    {
	/* Inside cube - start with BTF faces */
	paintOrder = BTF;
    }

    if (cs->invert == -1 || cs->desktopOpacity != OPAQUE)
	cubePaintAllViewports (s, &sa,transform, region,
			       outputPtr, mask, xMove,
			       size, hsize, paintOrder);

    if (cs->grabIndex == 0 && hsize > 2 &&
	(cs->invert != 1 || cs->desktopOpacity != OPAQUE ||
	 sa.vRotate != 0.0f || sa.yTranslate != 0.0f))
    {
	(*cs->paintTopBottom) (s, &sa, transform, outputPtr, hsize);
    }

    if (cs->invert == 1)
    {
	/* Outside cube - continue with BTF faces */
	paintOrder = BTF;
    }
    else
    {
	/* Inside cube - continue with FTB faces */
	paintOrder = FTB;
    }

    if (cs->invert == 1 || cs->desktopOpacity != OPAQUE)
	cubePaintAllViewports (s, &sa, transform, region,
			       outputPtr, mask, xMove,
			       size, hsize, paintOrder);

    s->display->textureFilter = filter;

    if (wasCulled)
	glEnable (GL_CULL_FACE);

    WRAP (cs, s, paintTransformedOutput, cubePaintTransformedOutput);
}

static void
cubeSetBackgroundOpacity (CompScreen* s)
{
    CUBE_SCREEN (s);

    if (cs->desktopOpacity != OPAQUE)
    {
	if (s->desktopWindowCount)
	{
	    glColor4us (0, 0, 0, 0);
	    glEnable (GL_BLEND);
	}
	else
	{
	    glColor4us (0xffff, 0xffff, 0xffff, cs->desktopOpacity);
	    glEnable (GL_BLEND);
	    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	    glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
    }
}

static void
cubeUnSetBackgroundOpacity (CompScreen* s)
{
    CUBE_SCREEN (s);

    if (cs->desktopOpacity != OPAQUE)
    {
	if (s->desktopWindowCount)
	{
	    glColor3usv (defaultColor);
	    glDisable (GL_BLEND);
	}
	else
	{
	    glColor3usv (defaultColor);
	    glDisable (GL_BLEND);
	    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	    screenTexEnvMode(s, GL_REPLACE);
	}
    }
}

static void
cubePaintBackground (CompScreen   *s,
		     Region	  region,
		     unsigned int mask)
{
    int n;

    CUBE_SCREEN (s);

    cubeSetBackgroundOpacity(s);

    n = cs->opt[CUBE_SCREEN_OPTION_BACKGROUNDS].value.list.nValue;
    if (n)
    {
	CompTexture *bg;
	CompMatrix  matrix;
	BoxPtr      pBox = region->rects;
	int	    nBox = region->numRects;
	GLfloat     *d, *data;

	if (!nBox)
	{
	    cubeUnSetBackgroundOpacity(s);
	    return;
	}

	n = (s->x * cs->nOutput + cs->srcOutput) % n;

	if (s->desktopWindowCount)
	{
	    cubeUnloadBackgrounds (s);
	    cubeUnSetBackgroundOpacity(s);
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
	{
	    cubeUnSetBackgroundOpacity(s);
	    return;
	}

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

    cubeUnSetBackgroundOpacity(s);
}

static Bool
cubePaintWindow (CompWindow		  *w,
		 const WindowPaintAttrib  *attrib,
		 const CompTransform	  *transform,
		 Region			  region,
		 unsigned int		  mask)
{
    Bool status;
    CompScreen* s = w->screen;
    CUBE_SCREEN(s);

    WindowPaintAttrib wa = *attrib;

    if (w->type & CompWindowTypeDesktopMask)
	wa.opacity = cs->desktopOpacity;

    UNWRAP (cs, s, paintWindow);
    status = (*s->paintWindow) (w, &wa, transform, region, mask);
    WRAP (cs, s, paintWindow, cubePaintWindow);

    return status;
}

static void
cubeInitWindowWalker (CompScreen *s, CompWalker* walker)
{
    CUBE_SCREEN (s);

    UNWRAP (cs, s, initWindowWalker);
    (*s->initWindowWalker) (s, walker);
    WRAP (cs, s, initWindowWalker, cubeInitWindowWalker);

    if (cs->paintOrder == FTB)
    {
	WalkInitProc tmpInit = walker->first;
	WalkStepProc tmpStep = walker->next;

	walker->first = walker->last;
	walker->last = tmpInit;

	walker->next = walker->prev;
	walker->prev = tmpStep;
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
    { "adjust_image", "bool", 0, 0, 0 },
    { "active_opacity", "float", "<min>0.0</min><max>100.0</max>", 0, 0 },
    { "inactive_opacity", "float", "<min>0.0</min><max>100.0</max>", 0, 0 },
    { "fade_time", "float", "<min>0.0</min>", 0, 0 },
    { "transparent_manual_only", "bool", 0, 0, 0 }
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

    cs->nVertices = 0;
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

    cs->outputXScale  = 1.0f;
    cs->outputYScale  = 1.0f;
    cs->outputXOffset = 0.0f;
    cs->outputYOffset = 0.0f;

    cs->rotationState = RotationNone;

    cs->desktopOpacity = OPAQUE;

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
    WRAP (cs, s, paintWindow, cubePaintWindow);
    WRAP (cs, s, applyScreenTransform, cubeApplyScreenTransform);
    WRAP (cs, s, setScreenOption, cubeSetGlobalScreenOption);
    WRAP (cs, s, outputChangeNotify, cubeOutputChangeNotify);
    WRAP (cs, s, initWindowWalker, cubeInitWindowWalker);

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
    UNWRAP (cs, s, paintWindow);
    UNWRAP (cs, s, applyScreenTransform);
    UNWRAP (cs, s, setScreenOption);
    UNWRAP (cs, s, outputChangeNotify);
    UNWRAP (cs, s, initWindowWalker);

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
