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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#ifdef USE_LIBSVG_CAIRO
#include <cairo-xlib.h>
#include <svg-cairo.h>
#endif

#include <X11/Xatom.h>
#include <X11/Xproto.h>

#include <compiz.h>

#define CUBE_COLOR_RED_DEFAULT   0xefef
#define CUBE_COLOR_GREEN_DEFAULT 0xebeb
#define CUBE_COLOR_BLUE_DEFAULT  0xe7e7

#define CUBE_IN_DEFAULT FALSE

static char *cubeImages[] = {
    "novell.png"
};
#define N_CUBE_IMAGES (sizeof (cubeImages) / sizeof (cubeImages[0]))

#define CUBE_SCALE_IMAGE_DEFAULT FALSE

#define CUBE_NEXT_KEY_DEFAULT       "space"
#define CUBE_NEXT_MODIFIERS_DEFAULT 0

#define CUBE_PREV_KEY_DEFAULT       "BackSpace"
#define CUBE_PREV_MODIFIERS_DEFAULT 0

#define CUBE_SKYDOME_DEFAULT FALSE

#define CUBE_SKYDOME_ANIMATE_DEFAULT FALSE

#define CUBE_UNFOLD_KEY_DEFAULT       "Down"
#define CUBE_UNFOLD_MODIFIERS_DEFAULT (ControlMask | CompAltMask)

#define CUBE_ACCELERATION_DEFAULT   4.0f
#define CUBE_ACCELERATION_MIN       1.0f
#define CUBE_ACCELERATION_MAX       20.0f
#define CUBE_ACCELERATION_PRECISION 0.1f

#define CUBE_SPEED_DEFAULT   1.5f
#define CUBE_SPEED_MIN       0.1f
#define CUBE_SPEED_MAX       50.0f
#define CUBE_SPEED_PRECISION 0.1f

#define CUBE_TIMESTEP_DEFAULT   1.2f
#define CUBE_TIMESTEP_MIN       0.1f
#define CUBE_TIMESTEP_MAX       50.0f
#define CUBE_TIMESTEP_PRECISION 0.1f

#define CUBE_MIPMAP_DEFAULT FALSE

static int displayPrivateIndex;

typedef struct _CubeDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
} CubeDisplay;

#define CUBE_SCREEN_OPTION_COLOR        0
#define CUBE_SCREEN_OPTION_IN           1
#define CUBE_SCREEN_OPTION_SCALE_IMAGE  2
#define CUBE_SCREEN_OPTION_IMAGES       3
#define CUBE_SCREEN_OPTION_NEXT         4
#define CUBE_SCREEN_OPTION_PREV         5
#define CUBE_SCREEN_OPTION_SKYDOME      6
#define CUBE_SCREEN_OPTION_SKYDOME_IMG  7
#define CUBE_SCREEN_OPTION_SKYDOME_ANIM 8
#define CUBE_SCREEN_OPTION_UNFOLD       9
#define CUBE_SCREEN_OPTION_ACCELERATION 10
#define CUBE_SCREEN_OPTION_SPEED	11
#define CUBE_SCREEN_OPTION_TIMESTEP	12
#define CUBE_SCREEN_OPTION_MIPMAP	13
#define CUBE_SCREEN_OPTION_NUM          14

typedef struct _CubeScreen {
    PreparePaintScreenProc     preparePaintScreen;
    DonePaintScreenProc	       donePaintScreen;
    PaintScreenProc	       paintScreen;
    PaintTransformedScreenProc paintTransformedScreen;
    PaintBackgroundProc	       paintBackground;
    SetScreenOptionProc	       setScreenOption;

    CompOption opt[CUBE_SCREEN_OPTION_NUM];

    int      invert;
    int      xrotations;
    GLfloat  distance;
    Bool     paintTopBottom;
    GLushort color[3];
    GLfloat  tc[12];

    int grabIndex;

    float acceleration;
    float speed;
    float timestep;

    Bool    unfolded;
    GLfloat unfold, unfoldVelocity;

    GLfloat  *vertices;
    int      nvertices;

    GLuint skyListId;
    Bool   animateSkyDome;

    Pixmap	    pixmap;
    int		    pw, ph;
    CompTexture     texture, sky;

    int		    imgNFile;
    int		    imgCurFile;
    CompOptionValue *imgFiles;

#ifdef USE_LIBSVG_CAIRO
    cairo_t	    *cr;
    svg_cairo_t	    *svgc;
#endif

} CubeScreen;

#define GET_CUBE_DISPLAY(d)				     \
    ((CubeDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define CUBE_DISPLAY(d)			   \
    CubeDisplay *cd = GET_CUBE_DISPLAY (d)

#define GET_CUBE_SCREEN(s, cd)					 \
    ((CubeScreen *) (s)->privates[(cd)->screenPrivateIndex].ptr)

#define CUBE_SCREEN(s)							\
    CubeScreen *cs = GET_CUBE_SCREEN (s, GET_CUBE_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static void
cubeInitSvg (CompScreen *s)

{
    CUBE_SCREEN (s);

    cs->pixmap = None;
    cs->pw = cs->ph = 0;

#ifdef USE_LIBSVG_CAIRO
    cs->cr = 0;
    cs->svgc = 0;
#endif

}

static void
cubeFiniSvg (CompScreen *s)

{
    CUBE_SCREEN (s);

#ifdef USE_LIBSVG_CAIRO
    if (cs->svgc)
	svg_cairo_destroy (cs->svgc);

    if (cs->cr)
	cairo_destroy (cs->cr);
#endif

    if (cs->pixmap)
	XFreePixmap (s->display->display, cs->pixmap);
}

static Bool
readSvgToTexture (CompScreen   *s,
		  CompTexture  *texture,
		  const char   *svgFileName,
		  unsigned int *returnWidth,
		  unsigned int *returnHeight)
{

#ifdef USE_LIBSVG_CAIRO
    unsigned int  width, height, pw, ph;
    char	 *name;

    CUBE_SCREEN (s);

    if (cs->svgc)
	svg_cairo_destroy (cs->svgc);

    if (svg_cairo_create (&cs->svgc))
	return FALSE;

    if (!openImageFile (svgFileName, &name, NULL))
	return FALSE;

    if (svg_cairo_parse (cs->svgc, name) != SVG_CAIRO_STATUS_SUCCESS)
	return FALSE;

    free (name);

    svg_cairo_get_size (cs->svgc, &width, &height);

    if (cs->opt[CUBE_SCREEN_OPTION_SCALE_IMAGE].value.b)
    {
	pw = s->width;
	ph = s->height;
    }
    else
    {
	pw = width;
	ph = height;
    }

    svg_cairo_set_viewport_dimension (cs->svgc, pw, ph);

    if (!cs->pixmap || cs->pw != pw || cs->ph != ph)
    {
	cairo_surface_t *surface;
	Visual		*visual;
	int		depth;

	if (cs->pixmap)
	    XFreePixmap (s->display->display, cs->pixmap);

	cs->pw = pw;
	cs->ph = ph;

	depth = DefaultDepth (s->display->display, s->screenNum);
	cs->pixmap = XCreatePixmap (s->display->display, s->root,
				    cs->pw, cs->ph,
				    depth);

	if (!bindPixmapToTexture (s, texture, cs->pixmap,
				  cs->pw, cs->ph, depth))
	{
	    fprintf (stderr, "%s: Couldn't bind slide pixmap 0x%x to "
		     "texture\n", programName, (int) cs->pixmap);

	    return FALSE;
	}

	visual = DefaultVisual (s->display->display, s->screenNum);
	surface = cairo_xlib_surface_create (s->display->display,
					     cs->pixmap, visual,
					     cs->pw, cs->ph);
	cs->cr = cairo_create (surface);
	cairo_surface_destroy (surface);
    }

    cairo_save (cs->cr);
    cairo_set_source_rgb (cs->cr,
			  (double) cs->color[0] / 0xffff,
			  (double) cs->color[1] / 0xffff,
			  (double) cs->color[2] / 0xffff);
    cairo_rectangle (cs->cr, 0, 0, cs->pw, cs->ph);
    cairo_fill (cs->cr);

    cairo_scale (cs->cr, (double) cs->pw / width, (double) cs->ph / height);

    svg_cairo_render (cs->svgc, cs->cr);
    cairo_restore (cs->cr);

    *returnWidth  = cs->pw;
    *returnHeight = cs->ph;

    return TRUE;
#else
    return FALSE;
#endif

}

static void
cubeLoadImg (CompScreen *s,
	     int	n)
{
    unsigned int width, height;

    CUBE_SCREEN (s);

    if (!cs->imgNFile || cs->pw != s->width || cs->ph != s->height)
    {
	finiTexture (s, &cs->texture);
	initTexture (s, &cs->texture);
	cubeFiniSvg (s);
	cubeInitSvg (s);

	if (!cs->imgNFile)
	    return;
    }

    cs->imgCurFile = n % cs->imgNFile;

    if (readImageToTexture (s, &cs->texture,
			    cs->imgFiles[cs->imgCurFile].s,
			    &width, &height))
    {
	cubeFiniSvg (s);
	cubeInitSvg (s);
    }
    else if (!readSvgToTexture (s, &cs->texture,
				cs->imgFiles[cs->imgCurFile].s,
				&width, &height))
    {
	fprintf (stderr, "%s: Failed to load slide: %s\n",
		 programName, cs->imgFiles[cs->imgCurFile].s);

	finiTexture (s, &cs->texture);
	initTexture (s, &cs->texture);
	cubeFiniSvg (s);
	cubeInitSvg (s);

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
	float x1 = width  / 2.0f - s->width  / 2.0f;
	float y1 = height / 2.0f - s->height / 2.0f;
	float x2 = width  / 2.0f + s->width  / 2.0f;
	float y2 = height / 2.0f + s->height / 2.0f;

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

static CompOption *
cubeGetScreenOptions (CompScreen *screen,
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

	GLfloat fRStart = 13.0f / 255.0f;
	GLfloat fGStart = 177.0f / 255.0f;
	GLfloat fBStart = 253.0f / 255.0f;
	GLfloat fREnd = 254.0f / 255.0f;
	GLfloat fGEnd = 255.0f / 255.0f;
	GLfloat fBEnd = 199.0f / 255.0f;

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

    if (cs->animateSkyDome)
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

static Bool
cubeSetScreenOption (CompScreen      *screen,
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
	    if (cubeUpdateGeometry (screen, screen->size, o->value.b ? -1 : 1))
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
	    cs->imgFiles = cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.value;
	    cs->imgNFile = cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.nValue;

	    cubeLoadImg (screen, cs->imgCurFile);
	    damageScreen (screen);

	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_NEXT:
    case CUBE_SCREEN_OPTION_PREV:
	if (compSetBindingOption (o, value))
	    return TRUE;
	break;
    case CUBE_SCREEN_OPTION_UNFOLD:
	if (addScreenBinding (screen, &value->bind))
	{
	    removeScreenBinding (screen, &o->value.bind);

	    if (compSetBindingOption (o, value))
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
	    cs->animateSkyDome = o->value.b;
	    cubeUpdateSkyDomeTexture (screen);
	    cubeUpdateSkyDomeList (screen, 1.0f);
	    damageScreen (screen);
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_ACCELERATION:
	if (compSetFloatOption (o, value))
	{
	    cs->acceleration = o->value.f;
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_SPEED:
	if (compSetFloatOption (o, value))
	{
	    cs->speed = o->value.f;
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_TIMESTEP:
	if (compSetFloatOption (o, value))
	{
	    cs->timestep = o->value.f;
	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_MIPMAP:
	if (compSetBoolOption (o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
cubeScreenInitOptions (CubeScreen *cs,
		       Display    *display)
{
    CompOption *o;
    int	       i;

    o = &cs->opt[CUBE_SCREEN_OPTION_COLOR];
    o->name	  = "color";
    o->shortDesc  = "Cube Color";
    o->longDesc	  = "Color of top and bottom sides of the cube";
    o->type	  = CompOptionTypeColor;
    o->value.c[0] = CUBE_COLOR_RED_DEFAULT;
    o->value.c[1] = CUBE_COLOR_GREEN_DEFAULT;
    o->value.c[2] = CUBE_COLOR_BLUE_DEFAULT;
    o->value.c[3] = 0xffff;

    o = &cs->opt[CUBE_SCREEN_OPTION_IN];
    o->name	  = "in";
    o->shortDesc  = "Inside Cube";
    o->longDesc	  = "Inside cube";
    o->type	  = CompOptionTypeBool;
    o->value.b    = CUBE_IN_DEFAULT;

    o = &cs->opt[CUBE_SCREEN_OPTION_SCALE_IMAGE];
    o->name	  = "scale_image";
    o->shortDesc  = "Scale image";
    o->longDesc	  = "Scale images to cover top face of cube";
    o->type	  = CompOptionTypeBool;
    o->value.b    = CUBE_SCALE_IMAGE_DEFAULT;

    o = &cs->opt[CUBE_SCREEN_OPTION_IMAGES];
    o->name	         = "images";
    o->shortDesc         = "Image files";
    o->longDesc	         = "List of PNG and SVG files that should be rendered "
	"on top face of cube";
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_CUBE_IMAGES;
    o->value.list.value  = malloc (sizeof (CompOptionValue) * N_CUBE_IMAGES);
    for (i = 0; i < N_CUBE_IMAGES; i++)
	o->value.list.value[i].s = strdup (cubeImages[i]);
    o->rest.s.string     = 0;
    o->rest.s.nString    = 0;

    o = &cs->opt[CUBE_SCREEN_OPTION_NEXT];
    o->name			  = "next_slide";
    o->shortDesc		  = "Next Slide";
    o->longDesc			  = "Advance to next slide";
    o->type			  = CompOptionTypeBinding;
    o->value.bind.type		  = CompBindingTypeKey;
    o->value.bind.u.key.modifiers = CUBE_NEXT_MODIFIERS_DEFAULT;
    o->value.bind.u.key.keycode   =
	XKeysymToKeycode (display, XStringToKeysym (CUBE_NEXT_KEY_DEFAULT));

    o = &cs->opt[CUBE_SCREEN_OPTION_PREV];
    o->name			  = "prev_slide";
    o->shortDesc		  = "Previous Slide";
    o->longDesc			  = "Go back to previous slide";
    o->type			  = CompOptionTypeBinding;
    o->value.bind.type		  = CompBindingTypeKey;
    o->value.bind.u.key.modifiers = CUBE_PREV_MODIFIERS_DEFAULT;
    o->value.bind.u.key.keycode   =
	XKeysymToKeycode (display, XStringToKeysym (CUBE_PREV_KEY_DEFAULT));

    o = &cs->opt[CUBE_SCREEN_OPTION_SKYDOME];
    o->name	  = "skydome";
    o->shortDesc  = "Skydome";
    o->longDesc	  = "Render skydome";
    o->type	  = CompOptionTypeBool;
    o->value.b    = CUBE_SKYDOME_DEFAULT;

    o = &cs->opt[CUBE_SCREEN_OPTION_SKYDOME_IMG];
    o->name	      = "skydome_image";
    o->shortDesc      = "Skydome Image";
    o->longDesc	      = "Image to use as texture for the skydome";
    o->type	      = CompOptionTypeString;
    o->value.s	      = strdup ("");
    o->rest.s.string  = 0;
    o->rest.s.nString = 0;

    o = &cs->opt[CUBE_SCREEN_OPTION_SKYDOME_ANIM];
    o->name	  = "skydome_animated";
    o->shortDesc  = "Animate Skydome";
    o->longDesc	  = "Animate skydome when rotating cube";
    o->type	  = CompOptionTypeBool;
    o->value.b    = CUBE_SKYDOME_ANIMATE_DEFAULT;

    o = &cs->opt[CUBE_SCREEN_OPTION_UNFOLD];
    o->name			  = "unfold";
    o->shortDesc		  = "Unfold";
    o->longDesc			  = "Unfold cube";
    o->type			  = CompOptionTypeBinding;
    o->value.bind.type		  = CompBindingTypeKey;
    o->value.bind.u.key.modifiers = CUBE_UNFOLD_MODIFIERS_DEFAULT;
    o->value.bind.u.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (CUBE_UNFOLD_KEY_DEFAULT));

    o = &cs->opt[CUBE_SCREEN_OPTION_ACCELERATION];
    o->name		= "acceleration";
    o->shortDesc	= "Acceleration";
    o->longDesc		= "Fold Acceleration";
    o->type		= CompOptionTypeFloat;
    o->value.f		= CUBE_ACCELERATION_DEFAULT;
    o->rest.f.min	= CUBE_ACCELERATION_MIN;
    o->rest.f.max	= CUBE_ACCELERATION_MAX;
    o->rest.f.precision = CUBE_ACCELERATION_PRECISION;

    o = &cs->opt[CUBE_SCREEN_OPTION_SPEED];
    o->name		= "speed";
    o->shortDesc	= "Speed";
    o->longDesc		= "Fold Speed";
    o->type		= CompOptionTypeFloat;
    o->value.f		= CUBE_SPEED_DEFAULT;
    o->rest.f.min	= CUBE_SPEED_MIN;
    o->rest.f.max	= CUBE_SPEED_MAX;
    o->rest.f.precision = CUBE_SPEED_PRECISION;

    o = &cs->opt[CUBE_SCREEN_OPTION_TIMESTEP];
    o->name		= "timestep";
    o->shortDesc	= "Timestep";
    o->longDesc		= "Fold Timestep";
    o->type		= CompOptionTypeFloat;
    o->value.f		= CUBE_TIMESTEP_DEFAULT;
    o->rest.f.min	= CUBE_TIMESTEP_MIN;
    o->rest.f.max	= CUBE_TIMESTEP_MAX;
    o->rest.f.precision = CUBE_TIMESTEP_PRECISION;

    o = &cs->opt[CUBE_SCREEN_OPTION_MIPMAP];
    o->name	  = "mipmap";
    o->shortDesc  = "Mipmap";
    o->longDesc	  = "Generate mipmaps when possible for higher quality scaling";
    o->type	  = CompOptionTypeBool;
    o->value.b    = CUBE_MIPMAP_DEFAULT;
}

static int
adjustVelocity (CubeScreen *cs)
{
    float unfold, adjust, amount;

    if (cs->unfolded)
	unfold = 1.0f - cs->unfold;
    else
	unfold = 0.0f - cs->unfold;

    adjust = unfold * 0.02f * cs->acceleration;
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

	amount = msSinceLastPaint * 0.2f * cs->speed;
	steps  = amount / (0.5f * cs->timestep);
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

    UNWRAP (cs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (cs, s, preparePaintScreen, cubePreparePaintScreen);

}

static Bool
cubePaintScreen (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 Region			 region,
		 unsigned int		 mask)
{
    Bool status;

    CUBE_SCREEN (s);

    if (cs->grabIndex)
    {
	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_TRANSFORMED_MASK;
    }

    UNWRAP (cs, s, paintScreen);
    status = (*s->paintScreen) (s, sAttrib, region, mask);
    WRAP (cs, s, paintScreen, cubePaintScreen);

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
cubePaintTransformedScreen (CompScreen		    *s,
			    const ScreenPaintAttrib *sAttrib,
			    unsigned int	    mask)
{
    ScreenPaintAttrib sa = *sAttrib;
    int		      xMove = 0;
    float	      size = s->size;

    CUBE_SCREEN (s);

    if (cs->sky.name)
    {
	if (mask & PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK)
	    glClear (GL_STENCIL_BUFFER_BIT);

	screenLighting (s, FALSE);

	glPushMatrix ();

	if (cs->animateSkyDome)
	{
	    glRotatef (sAttrib->xRotate, 0.0f, 1.0f, 0.0f);
	    glRotatef (sAttrib->vRotate / 5.0f + 90.0f, 1.0f, 0.0f, 0.0f);
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
	if (mask & PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK)
	    glClear (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	else
	    glClear (GL_COLOR_BUFFER_BIT);
    }

    UNWRAP (cs, s, paintTransformedScreen);

    sa.xTranslate = sAttrib->xTranslate;
    sa.yTranslate = sAttrib->yTranslate;

    if (cs->grabIndex)
    {
	sa.vRotate = 0.0f;

	size += cs->unfold * 8.0f;
	size += powf (cs->unfold, 6) * 64.0;
	size += powf (cs->unfold, 16) * 2048.0;

	sa.zTranslate = -cs->invert * (0.5f / tanf (M_PI / size));

	/* distance we move the camera back when unfolding the cube.
	   currently hardcoded to 1.5 but it should probably be optional. */
	sa.zCamera -= cs->unfold * 1.5f;

	sa.xRotate = sAttrib->xRotate * cs->invert;
	if (sa.xRotate > 0.0f)
	{
	    cs->xrotations = (int) (s->size * sa.xRotate) / 360;
	    sa.xRotate = sa.xRotate - (360.0f * cs->xrotations) / s->size;
	}
	else
	{
	    cs->xrotations = (int) (s->size * sa.xRotate) / 360;
	    sa.xRotate = sa.xRotate -
		(360.0f * cs->xrotations) / s->size + 360.0f / s->size;
	    cs->xrotations--;
	}

	sa.xRotate = sa.xRotate / size * s->size;
    }
    else
    {
	if (sAttrib->vRotate > 100.0f)
	    sa.vRotate = 100.0f;
	else if (sAttrib->vRotate < -100.0f)
	    sa.vRotate = -100.0f;
	else
	    sa.vRotate = sAttrib->vRotate;

	sa.zTranslate = -cs->invert * cs->distance;
	sa.xRotate = sAttrib->xRotate * cs->invert;
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

    if (cs->grabIndex == 0 &&
	(cs->invert != 1 || sa.vRotate != 0.0f || sa.yTranslate != 0.0f))
    {
	screenLighting (s, TRUE);

	glColor3usv (cs->color);

	glPushMatrix ();

	if (sAttrib->xRotate > 0.0f)
	{
	    sa.yRotate += 360.0f / size;
	    translateRotateScreen (&sa);
	    sa.yRotate -= 360.0f / size;
	}
	else
	    translateRotateScreen (&sa);

	glVertexPointer (3, GL_FLOAT, 0, cs->vertices);

	glNormal3f (0.0f, -1.0f, 0.0f);

	if (cs->invert == 1 && s->size == 4 && cs->texture.name)
	{
	    enableTexture (s, &cs->texture, COMP_TEXTURE_FILTER_GOOD);
	    glTexCoordPointer (2, GL_FLOAT, 0, cs->tc);
	    glDrawArrays (GL_TRIANGLE_FAN, 0, cs->nvertices >> 1);
	    disableTexture (s, &cs->texture);
	}
	else
	    glDrawArrays (GL_TRIANGLE_FAN, 0, cs->nvertices >> 1);

	glNormal3f (0.0f, 1.0f, 0.0f);

	glDrawArrays (GL_TRIANGLE_FAN, cs->nvertices >> 1,
		      cs->nvertices >> 1);

	glNormal3f (0.0f, 0.0f, -1.0f);

	glPopMatrix ();

	glColor4usv (defaultColor);
    }

    /* outside cube */
    if (cs->invert == 1)
    {
	if (cs->grabIndex)
	{
	    GLenum filter;
	    int    i;

	    xMove = cs->xrotations - ((s->size >> 1) - 1);
	    sa.yRotate += (360.0f / size) * ((s->size >> 1) - 1);

	    filter = s->display->textureFilter;
	    if (cs->opt[CUBE_SCREEN_OPTION_MIPMAP].value.b)
		s->display->textureFilter = GL_LINEAR_MIPMAP_LINEAR;

	    for (i = 0; i < s->size; i++)
	    {
		moveScreenViewport (s, xMove, FALSE);
		(*s->paintTransformedScreen) (s, &sa, mask);
		moveScreenViewport (s, -xMove, FALSE);

		sa.yRotate -= 360.0f / size;
		xMove++;
	    }

	    s->display->textureFilter = filter;
	}
	else
	{
	    if (sAttrib->xRotate != 0.0f)
	    {
		xMove = cs->xrotations;

		moveScreenViewport (s, xMove, FALSE);
		(*s->paintTransformedScreen) (s, &sa, mask);
		moveScreenViewport (s, -xMove, FALSE);

		xMove++;

		moveScreenViewport (s, xMove, FALSE);
	    }

	    sa.yRotate -= 360.0f / size;

	    (*s->paintTransformedScreen) (s, &sa, mask);
	    moveScreenViewport (s, -xMove, FALSE);
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
		xMove -= ((s->size >> 1) - 2);
		sa.yRotate -= (360.0f / size) * ((s->size >> 1) - 2);
	    }
	    else
	    {
		xMove -= ((s->size >> 1) - 1);
		sa.yRotate -= (360.0f / size) * ((s->size >> 1) - 1);
	    }

	    for (i = 0; i < s->size; i++)
	    {
		moveScreenViewport (s, xMove, FALSE);
		(*s->paintTransformedScreen) (s, &sa, mask);
		moveScreenViewport (s, -xMove, FALSE);

		sa.yRotate += 360.0f / size;
		xMove++;
	    }

	    s->display->textureFilter = filter;
	}
	else
	{
	    moveScreenViewport (s, xMove, FALSE);
	    (*s->paintTransformedScreen) (s, &sa, mask);
	    moveScreenViewport (s, -xMove, FALSE);

	    sa.yRotate += 360.0f / size;
	    xMove = -cs->xrotations;

	    moveScreenViewport (s, xMove, FALSE);
	    (*s->paintTransformedScreen) (s, &sa, mask);
	    moveScreenViewport (s, -xMove, FALSE);

	    sa.yRotate += 360.0f / size;
	    xMove = 1 - cs->xrotations;

	    moveScreenViewport (s, xMove, FALSE);
	    (*s->paintTransformedScreen) (s, &sa, mask);
	    moveScreenViewport (s, -xMove, FALSE);
	}
    }

    WRAP (cs, s, paintTransformedScreen, cubePaintTransformedScreen);
}

static void
cubePaintBackground (CompScreen   *s,
		     Region	  region,
		     unsigned int mask)
{
    CUBE_SCREEN (s);

    s->stencilRef++;

    UNWRAP (cs, s, paintBackground);
    (*s->paintBackground) (s, region, mask);
    WRAP (cs, s, paintBackground, cubePaintBackground);
}

static void
cubeUnfold (CompScreen *s)
{
    CUBE_SCREEN (s);

    if (!cs->grabIndex)
	cs->grabIndex = pushScreenGrab (s, s->invisibleCursor, "cube");

    cs->unfolded = TRUE;
    damageScreen (s);
}

static void
cubeFold (CompScreen *s)
{
    CUBE_SCREEN (s);

    if (cs->grabIndex)
    {
	cs->unfolded = FALSE;
	damageScreen (s);
    }
}

static void
cubeHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompScreen *s;

    CUBE_DISPLAY (d);

    switch (event->type) {
    case KeyPress:
    case KeyRelease:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    CUBE_SCREEN (s);

	    if (eventMatches (d, event, &cs->opt[CUBE_SCREEN_OPTION_UNFOLD]))
		cubeUnfold (s);

	    if (eventMatches (d, event, &cs->opt[CUBE_SCREEN_OPTION_NEXT]))
	    {
		if (cs->imgNFile)
		{
		    cubeLoadImg (s, (cs->imgCurFile + 1) % cs->imgNFile);
		    damageScreen (s);
		}
	    }

	    if (eventMatches (d, event, &cs->opt[CUBE_SCREEN_OPTION_PREV]))
	    {
		if (cs->imgNFile)
		{
		    cubeLoadImg (s, (cs->imgCurFile - 1 + cs->imgNFile) %
				 cs->imgNFile);
		    damageScreen (s);
		}
	    }

	    if (eventTerminates (d, event,
				 &cs->opt[CUBE_SCREEN_OPTION_UNFOLD]))
		cubeFold (s);
	}
	break;
    case ButtonPress:
    case ButtonRelease:
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s)
	{
	    CUBE_SCREEN (s);

	    if (eventMatches (d, event, &cs->opt[CUBE_SCREEN_OPTION_NEXT]))
	    {
		cubeLoadImg (s, (cs->imgCurFile + 1) % cs->imgNFile);
		damageScreen (s);
	    }

	    if (eventMatches (d, event, &cs->opt[CUBE_SCREEN_OPTION_PREV]))
	    {
		cubeLoadImg (s, (cs->imgCurFile - 1 + cs->imgNFile) %
			     cs->imgNFile);
		damageScreen (s);
	    }
	}
    default:
	break;
    }

    UNWRAP (cd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (cd, d, handleEvent, cubeHandleEvent);
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

    if (status && strcmp (name, "size") == 0)
	cubeUpdateGeometry (s, s->size, cs->invert);

    return status;
}

static Bool
cubeInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    CubeDisplay *cd;

    cd = malloc (sizeof (CubeDisplay));
    if (!cd)
	return FALSE;

    cd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (cd->screenPrivateIndex < 0)
    {
	free (cd);
	return FALSE;
    }

    WRAP (cd, d, handleEvent, cubeHandleEvent);

    d->privates[displayPrivateIndex].ptr = cd;

    return TRUE;
}

static void
cubeFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    CUBE_DISPLAY (d);

    freeScreenPrivateIndex (d, cd->screenPrivateIndex);

    UNWRAP (cd, d, handleEvent);

    free (cd);
}

static Bool
cubeInitScreen (CompPlugin *p,
		CompScreen *s)
{
    CubeScreen *cs;

    CUBE_DISPLAY (s->display);

    cs = malloc (sizeof (CubeScreen));
    if (!cs)
	return FALSE;

    cs->invert = 1;

    cs->tc[0] = cs->tc[1] = cs->tc[2] = cs->tc[3] = 0.0f;
    cs->tc[4] = cs->tc[5] = cs->tc[6] = cs->tc[7] = 0.0f;

    cs->color[0] = CUBE_COLOR_RED_DEFAULT;
    cs->color[1] = CUBE_COLOR_GREEN_DEFAULT;
    cs->color[2] = CUBE_COLOR_BLUE_DEFAULT;

    cs->nvertices = 0;
    cs->vertices  = NULL;

    cs->grabIndex = 0;

    cs->skyListId      = 0;
    cs->animateSkyDome = CUBE_SKYDOME_ANIMATE_DEFAULT;

    s->privates[cd->screenPrivateIndex].ptr = cs;

    cs->paintTopBottom = FALSE;

    initTexture (s, &cs->texture);
    initTexture (s, &cs->sky);

    cubeInitSvg (s);

    cs->imgFiles   = 0;
    cs->imgNFile   = 0;
    cs->imgCurFile = 0;

    cs->acceleration = CUBE_ACCELERATION_DEFAULT;
    cs->speed        = CUBE_SPEED_DEFAULT;
    cs->timestep     = CUBE_TIMESTEP_DEFAULT;

    cs->unfolded = FALSE;
    cs->unfold   = 0.0f;

    cs->unfoldVelocity = 0.0f;

    cubeScreenInitOptions (cs, s->display->display);

    cs->imgFiles = cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.value;
    cs->imgNFile = cs->opt[CUBE_SCREEN_OPTION_IMAGES].value.list.nValue;

    addScreenBinding (s, &cs->opt[CUBE_SCREEN_OPTION_UNFOLD].value.bind);

    WRAP (cs, s, preparePaintScreen, cubePreparePaintScreen);
    WRAP (cs, s, donePaintScreen, cubeDonePaintScreen);
    WRAP (cs, s, paintScreen, cubePaintScreen);
    WRAP (cs, s, paintTransformedScreen, cubePaintTransformedScreen);
    WRAP (cs, s, paintBackground, cubePaintBackground);
    WRAP (cs, s, setScreenOption, cubeSetGlobalScreenOption);

    if (!cubeUpdateGeometry (s, s->size, cs->invert))
	return FALSE;

    if (cs->imgNFile)
    {
	cubeLoadImg (s, cs->imgCurFile);
	damageScreen (s);
    }

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
    UNWRAP (cs, s, paintScreen);
    UNWRAP (cs, s, paintTransformedScreen);
    UNWRAP (cs, s, paintBackground);
    UNWRAP (cs, s, setScreenOption);

    finiTexture (s, &cs->texture);
    finiTexture (s, &cs->sky);

    cubeFiniSvg (s);

    free (cs);
}

static Bool
cubeInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
cubeFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginDep cubeDeps[] = {
    { CompPluginRuleBefore, "scale" }
};

CompPluginVTable cubeVTable = {
    "cube",
    "Desktop Cube",
    "Place windows on cube",
    cubeInit,
    cubeFini,
    cubeInitDisplay,
    cubeFiniDisplay,
    cubeInitScreen,
    cubeFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    cubeGetScreenOptions,
    cubeSetScreenOption,
    cubeDeps,
    sizeof (cubeDeps) / sizeof (cubeDeps[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &cubeVTable;
}
