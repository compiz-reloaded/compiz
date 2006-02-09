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

#define CUBE_NEXT_KEY_DEFAULT       "space"
#define CUBE_NEXT_MODIFIERS_DEFAULT CompPressMask

#define CUBE_PREV_KEY_DEFAULT       "BackSpace"
#define CUBE_PREV_MODIFIERS_DEFAULT CompPressMask

static int displayPrivateIndex;

typedef struct _CubeDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
} CubeDisplay;

#define CUBE_SCREEN_OPTION_COLOR 0
#define CUBE_SCREEN_OPTION_IN    1
#define CUBE_SCREEN_OPTION_SVGS  2
#define CUBE_SCREEN_OPTION_NEXT  3
#define CUBE_SCREEN_OPTION_PREV  4
#define CUBE_SCREEN_OPTION_NUM   5

typedef struct _CubeScreen {
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

    GLfloat  *vertices;
    int      nvertices;

    Pixmap	    pixmap;
    int		    pw, ph;
    CompTexture     texture;

#ifdef USE_LIBSVG_CAIRO
    cairo_t	    *cr;
    svg_cairo_t	    *svgc;
    int		    svgNFile;
    int		    svgCurFile;
    CompOptionValue *svgFiles;
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

#ifdef USE_LIBSVG_CAIRO
static void
cubeInitSvg (CompScreen *s)

{
    CUBE_SCREEN (s);

    cs->pixmap = None;
    cs->pw = cs->ph = 0;
    cs->cr = 0;
    cs->svgc = 0;
}

static void
cubeFiniSvg (CompScreen *s)

{
    CUBE_SCREEN (s);

    if (cs->svgc)
	svg_cairo_destroy (cs->svgc);

    if (cs->cr)
	cairo_destroy (cs->cr);

    if (cs->pixmap)
	XFreePixmap (s->display->display, cs->pixmap);
}

static void
cubeLoadSvg (CompScreen *s,
	     int	n)
{
    unsigned int width, height;

    CUBE_SCREEN (s);

    if (!cs->svgNFile || cs->pw != s->width || cs->ph != s->height)
    {
	finiTexture (s, &cs->texture);
	initTexture (s, &cs->texture);
	cubeFiniSvg (s);
	cubeInitSvg (s);

	if (!cs->svgNFile)
	    return;
    }

    if (!cs->pixmap)
    {
	cairo_surface_t *surface;
	Visual		*visual;
	int		depth;

	cs->pw = s->width;
	cs->ph = s->height;

	depth = DefaultDepth (s->display->display, s->screenNum);
	cs->pixmap = XCreatePixmap (s->display->display, s->root,
				    s->width, s->height,
				    depth);

	if (!bindPixmapToTexture (s, &cs->texture, cs->pixmap,
				  s->width, s->height, depth))
	{
	    fprintf (stderr, "%s: Couldn't bind slide pixmap 0x%x to "
		     "texture\n", programName, (int) cs->pixmap);
	}

	if (cs->texture.target == GL_TEXTURE_RECTANGLE_ARB)
	{
	    cs->tc[0] = s->width / 2.0f;
	    cs->tc[1] = s->height / 2.0f;

	    cs->tc[2] = s->width;
	    cs->tc[3] = s->height;

	    cs->tc[4] = 0.0f;
	    cs->tc[5] = s->height;

	    cs->tc[6] = 0.0f;
	    cs->tc[7] = 0.0f;

	    cs->tc[8] = s->width;
	    cs->tc[9] = 0.0f;

	    cs->tc[10] = s->width;
	    cs->tc[11] = s->height;
	}
	else
	{
	    cs->tc[0] = 0.5f;
	    cs->tc[1] = 0.5f;

	    cs->tc[2] = 1.0f;
	    cs->tc[3] = 1.0f;

	    cs->tc[4] = 0.0f;
	    cs->tc[5] = 1.0f;

	    cs->tc[6] = 0.0f;
	    cs->tc[7] = 0.0f;

	    cs->tc[8] = 1.0f;
	    cs->tc[9] = 0.0f;

	    cs->tc[10] = 1.0;
	    cs->tc[11] = 1.0f;
	}

	visual = DefaultVisual (s->display->display, s->screenNum);
	surface = cairo_xlib_surface_create (s->display->display,
					     cs->pixmap, visual,
					     s->width, s->height);
	cs->cr = cairo_create (surface);
	cairo_surface_destroy (surface);
    }

    cs->svgCurFile = n % cs->svgNFile;

    if (cs->svgc)
	svg_cairo_destroy (cs->svgc);

    if (svg_cairo_create (&cs->svgc))
    {
	fprintf (stderr, "%s: Failed to create svg_cairo_t.\n",
		 programName);
	return;
    }

    svg_cairo_set_viewport_dimension (cs->svgc, s->width, s->height);

    if (svg_cairo_parse (cs->svgc, cs->svgFiles[cs->svgCurFile].s))
    {
	fprintf (stderr, "%s: Failed to load svg: %s.\n",
		 programName, cs->svgFiles[cs->svgCurFile].s);
	return;
    }

    svg_cairo_get_size (cs->svgc, &width, &height);

    cairo_save (cs->cr);
    cairo_set_source_rgb (cs->cr,
			  (double) cs->color[0] / 0xffff,
			  (double) cs->color[1] / 0xffff,
			  (double) cs->color[2] / 0xffff);
    cairo_rectangle (cs->cr, 0, 0, s->width, s->height);
    cairo_fill (cs->cr);

    cairo_scale (cs->cr,
		 (double) s->width / width,
		 (double) s->height / height);

    svg_cairo_render (cs->svgc, cs->cr);
    cairo_restore (cs->cr);
}
#endif

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
    case CUBE_SCREEN_OPTION_SVGS:
	if (compSetOptionList (o, value))
	{

#ifdef USE_LIBSVG_CAIRO
	    cs->svgFiles = cs->opt[CUBE_SCREEN_OPTION_SVGS].value.list.value;
	    cs->svgNFile = cs->opt[CUBE_SCREEN_OPTION_SVGS].value.list.nValue;

	    cubeLoadSvg (screen, cs->svgCurFile);
	    damageScreen (screen);
#endif

	    return TRUE;
	}
	break;
    case CUBE_SCREEN_OPTION_NEXT:
    case CUBE_SCREEN_OPTION_PREV:
	if (compSetBindingOption (o, value))
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

    o = &cs->opt[CUBE_SCREEN_OPTION_SVGS];
    o->name	         = "svgs";
    o->shortDesc         = "SVG files";
    o->longDesc	         = "List of SVG files rendered on top face of cube";
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = 0;
    o->value.list.value  = 0;
    o->rest.s.string     = 0;
    o->rest.s.nString    = 0;

    o = &cs->opt[CUBE_SCREEN_OPTION_NEXT];
    o->name			  = "next_slide";
    o->shortDesc		  = "Next Slide";
    o->longDesc			  = "Adavence to next slide";
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
}

static void
cubePaintTransformedScreen (CompScreen		    *s,
			    const ScreenPaintAttrib *sAttrib,
			    unsigned int	    mask)
{
    ScreenPaintAttrib sa = *sAttrib;
    int		      xMove = 0;

    CUBE_SCREEN (s);

    if (mask & PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK)
	glClear (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    else
	glClear (GL_COLOR_BUFFER_BIT);

    if (sa.vRotate > 100.0f)
	sa.vRotate = 100.0f;
    else if (sAttrib->vRotate < -100.0f)
	sa.vRotate = -100.0f;
    else
	sa.vRotate = sAttrib->vRotate;

    UNWRAP (cs, s, paintTransformedScreen);

    sa.xTranslate = sAttrib->xTranslate;
    sa.yTranslate = sAttrib->yTranslate;
    sa.zTranslate = -cs->invert * cs->distance;

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

    if (cs->invert != 1 || sa.vRotate != 0.0f || sa.yTranslate != 0.0f)
    {
	glColor3usv (cs->color);

	glPushMatrix ();

	if (sAttrib->xRotate > 0.0f)
	{
	    sa.yRotate += 360.0f / s->size;
	    translateRotateScreen (&sa);
	    sa.yRotate -= 360.0f / s->size;
	}
	else
	    translateRotateScreen (&sa);

	glVertexPointer (3, GL_FLOAT, 0, cs->vertices);

	if (cs->invert == 1 && s->size == 4 && cs->texture.name)
	{
	    enableTexture (s, &cs->texture, COMP_TEXTURE_FILTER_GOOD);
	    glTexCoordPointer (2, GL_FLOAT, 0, cs->tc);
	    glDrawArrays (GL_TRIANGLE_FAN, 0, cs->nvertices >> 1);
	    disableTexture (&cs->texture);
	}
	else
	    glDrawArrays (GL_TRIANGLE_FAN, 0, cs->nvertices >> 1);

	glDrawArrays (GL_TRIANGLE_FAN, cs->nvertices >> 1,
		      cs->nvertices >> 1);

	glPopMatrix ();
    }

    /* outside 4 side cube */
    if (s->size == 4 && cs->invert == 1)
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

	sa.yRotate -= 360.0f / s->size;

	(*s->paintTransformedScreen) (s, &sa, mask);

	moveScreenViewport (s, -xMove, FALSE);
    }
    else
    {
	if (sa.xRotate > 180.0f / s->size)
	{
	    sa.yRotate -= 360.0f / s->size;
	    cs->xrotations++;
	}

	sa.yRotate -= 360.0f / s->size;
	xMove = -1 - cs->xrotations;

	moveScreenViewport (s, xMove, FALSE);
	(*s->paintTransformedScreen) (s, &sa, mask);
	moveScreenViewport (s, -xMove, FALSE);

	sa.yRotate += 360.0f / s->size;
	xMove = -cs->xrotations;

	moveScreenViewport (s, xMove, FALSE);
	(*s->paintTransformedScreen) (s, &sa, mask);
	moveScreenViewport (s, -xMove, FALSE);

	sa.yRotate += 360.0f / s->size;
	xMove = 1 - cs->xrotations;

	moveScreenViewport (s, xMove, FALSE);
	(*s->paintTransformedScreen) (s, &sa, mask);
	moveScreenViewport (s, -xMove, FALSE);
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

#ifdef USE_LIBSVG_CAIRO
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

	    if (EV_KEY (&cs->opt[CUBE_SCREEN_OPTION_NEXT], event))
	    {
		cubeLoadSvg (s, (cs->svgCurFile + 1) % cs->svgNFile);
		damageScreen (s);
	    }

	    if (EV_KEY (&cs->opt[CUBE_SCREEN_OPTION_PREV], event))
	    {
		cubeLoadSvg (s, (cs->svgCurFile - 1 + cs->svgNFile) %
			     cs->svgNFile);
		damageScreen (s);
	    }
	}
	break;
    case ButtonPress:
    case ButtonRelease:
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s)
	{
	    CUBE_SCREEN (s);

	    if (EV_BUTTON (&cs->opt[CUBE_SCREEN_OPTION_NEXT], event))
	    {
		cubeLoadSvg (s, (cs->svgCurFile + 1) % cs->svgNFile);
		damageScreen (s);
	    }

	    if (EV_BUTTON (&cs->opt[CUBE_SCREEN_OPTION_PREV], event))
	    {
		cubeLoadSvg (s, (cs->svgCurFile - 1 + cs->svgNFile) %
			     cs->svgNFile);
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
#endif

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

#ifdef USE_LIBSVG_CAIRO
    WRAP (cd, d, handleEvent, cubeHandleEvent);
#endif

    d->privates[displayPrivateIndex].ptr = cd;

    return TRUE;
}

static void
cubeFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    CUBE_DISPLAY (d);

    freeScreenPrivateIndex (d, cd->screenPrivateIndex);

#ifdef USE_LIBSVG_CAIRO
    UNWRAP (cd, d, handleEvent);
#endif

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

    s->privates[cd->screenPrivateIndex].ptr = cs;

    cs->paintTopBottom = FALSE;

    initTexture (s, &cs->texture);

#ifdef USE_LIBSVG_CAIRO
    cubeInitSvg (s);

    cs->svgFiles   = 0;
    cs->svgNFile   = 0;
    cs->svgCurFile = 0;
#endif

    cubeScreenInitOptions (cs, s->display->display);

    WRAP (cs, s, paintTransformedScreen, cubePaintTransformedScreen);
    WRAP (cs, s, paintBackground, cubePaintBackground);
    WRAP (cs, s, setScreenOption, cubeSetGlobalScreenOption);

    if (!cubeUpdateGeometry (s, s->size, cs->invert))
	return FALSE;

    return TRUE;
}

static void
cubeFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    CUBE_SCREEN (s);

    UNWRAP (cs, s, paintTransformedScreen);
    UNWRAP (cs, s, paintBackground);
    UNWRAP (cs, s, setScreenOption);

    finiTexture (s, &cs->texture);

#ifdef USE_LIBSVG_CAIRO
    cubeFiniSvg (s);
#endif

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
    { CompPluginRuleBefore, "expose" }
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
