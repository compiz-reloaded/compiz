/*
 * Copyright © 2006 Novell, Inc.
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
#include <cairo-xlib-xrender.h>

#include <compiz.h>

#define ANNO_INITIATE_BUTTON_DEFAULT	       Button1
#define ANNO_INITIATE_BUTTON_MODIFIERS_DEFAULT (CompSuperMask | CompAltMask)

#define ANNO_ERASE_BUTTON_DEFAULT	    Button3
#define ANNO_ERASE_BUTTON_MODIFIERS_DEFAULT (CompSuperMask | CompAltMask)

#define ANNO_CLEAR_KEY_DEFAULT	         "k"
#define ANNO_CLEAR_KEY_MODIFIERS_DEFAULT (CompSuperMask | CompAltMask)

#define ANNO_COLOR_RED_DEFAULT   0xffff
#define ANNO_COLOR_GREEN_DEFAULT 0x0000
#define ANNO_COLOR_BLUE_DEFAULT  0x0000

static int displayPrivateIndex;

static int annoLastPointerX = 0;
static int annoLastPointerY = 0;

#define ANNO_DISPLAY_OPTION_INITIATE 0
#define ANNO_DISPLAY_OPTION_ERASE    1
#define ANNO_DISPLAY_OPTION_CLEAR    2
#define ANNO_DISPLAY_OPTION_COLOR    3
#define ANNO_DISPLAY_OPTION_NUM	     4

typedef struct _AnnoDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[ANNO_DISPLAY_OPTION_NUM];
} AnnoDisplay;

typedef struct _AnnoScreen {
    PaintScreenProc paintScreen;
    int		    grabIndex;

    Pixmap	    pixmap;
    CompTexture	    texture;
    cairo_surface_t *surface;
    cairo_t	    *cairo;
    Bool            content;
} AnnoScreen;

#define GET_ANNO_DISPLAY(d)				     \
    ((AnnoDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define ANNO_DISPLAY(d)			   \
    AnnoDisplay *ad = GET_ANNO_DISPLAY (d)

#define GET_ANNO_SCREEN(s, ad)					 \
    ((AnnoScreen *) (s)->privates[(ad)->screenPrivateIndex].ptr)

#define ANNO_SCREEN(s)							\
    AnnoScreen *as = GET_ANNO_SCREEN (s, GET_ANNO_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))


static void
annoCairoClear (CompScreen *s,
		cairo_t    *cr)
{
    ANNO_SCREEN (s);

    cairo_save (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_restore (cr);

    as->content = FALSE;
}

static cairo_t *
annoCairoContext (CompScreen *s)
{
    ANNO_SCREEN (s);

    if (!as->cairo)
    {
	XRenderPictFormat *format;
	Screen		  *screen;
	int		  w, h;

	screen = ScreenOfDisplay (s->display->display, s->screenNum);

	w = s->width;
	h = s->height;

	format = XRenderFindStandardFormat (s->display->display,
					    PictStandardARGB32);

	as->pixmap = XCreatePixmap (s->display->display, s->root, w, h, 32);

	if (!bindPixmapToTexture (s, &as->texture, as->pixmap, w, h, 32))
	{
	    fprintf (stderr, "%s: Couldn't bind annotate pixmap 0x%x to "
		     "texture\n", programName, (int) as->pixmap);

	    XFreePixmap (s->display->display, as->pixmap);

	    return NULL;
	}

	as->surface =
	    cairo_xlib_surface_create_with_xrender_format (s->display->display,
							   as->pixmap, screen,
							   format, w, h);

	as->cairo = cairo_create (as->surface);

	annoCairoClear (s, as->cairo);
    }

    return as->cairo;
}

static Bool
annoInitiate (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int	      nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	cairo_t *cr;

	ANNO_SCREEN (s);

	if (otherScreenGrabExist (s, 0))
	    return FALSE;

	if (!as->grabIndex)
	    as->grabIndex = pushScreenGrab (s, None, "annotate");

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	if (state & CompActionStateInitKey)
	    action->state |= CompActionStateTermKey;

	annoLastPointerX = pointerX;
	annoLastPointerY = pointerY;

	cr = annoCairoContext (s);
	if (cr)
	{
	    unsigned short *color;
	    
	    ANNO_DISPLAY (s->display);

	    color = ad->opt[ANNO_DISPLAY_OPTION_COLOR].value.c;

	    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	    cairo_set_source_rgb (cr,
				  (double) color[0] / 0xffff,
				  (double) color[1] / 0xffff,
				  (double) color[2] / 0xffff);
	    cairo_set_line_width (cr, 4.0);
	    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	}
    }

    return FALSE;
}

static Bool
annoTerminate (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int	       nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	ANNO_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (as->grabIndex)
	{
	    removeScreenGrab (s, as->grabIndex, NULL);
	    as->grabIndex = 0;
	}
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

static Bool
annoEraseInitiate (CompDisplay     *d,
		   CompAction      *action,
		   CompActionState state,
		   CompOption      *option,
		   int	           nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	cairo_t *cr;

	ANNO_SCREEN (s);

	if (otherScreenGrabExist (s, 0))
	    return FALSE;

	if (!as->grabIndex)
	    as->grabIndex = pushScreenGrab (s, None, "annotate");

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	if (state & CompActionStateInitKey)
	    action->state |= CompActionStateTermKey;

	annoLastPointerX = pointerX;
	annoLastPointerY = pointerY;

	cr = annoCairoContext (s);
	if (cr)
	{
	    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
	    cairo_set_line_width (cr, 20.0);
	    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	}
    }

    return FALSE;
}

static Bool
annoClear (CompDisplay     *d,
	   CompAction      *action,
	   CompActionState state,
	   CompOption      *option,
	   int		   nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	ANNO_SCREEN (s);

	if (as->content)
	{
	    cairo_t *cr;

	    cr = annoCairoContext (s);
	    if (cr)
		annoCairoClear (s, as->cairo);

	    damageScreen (s);
	}

	return TRUE;
    }

    return FALSE;
}

static Bool
annoPaintScreen (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 Region			 region,
		 int			 output,
		 unsigned int		 mask)
{
    Bool status;

    ANNO_SCREEN (s);

    UNWRAP (as, s, paintScreen);
    status = (*s->paintScreen) (s, sAttrib, region, output, mask);
    WRAP (as, s, paintScreen, annoPaintScreen);

    if (status && as->content && region->numRects)
    {
	BoxPtr pBox;
	int    nBox;

	glPushMatrix ();

	prepareXCoords (s, output, -DEFAULT_Z_CAMERA);

	glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	glEnable (GL_BLEND);

	enableTexture (s, &as->texture, COMP_TEXTURE_FILTER_FAST);

	pBox = region->rects;
	nBox = region->numRects;

	glBegin (GL_QUADS);

	while (nBox--)
	{
	    glTexCoord2f (COMP_TEX_COORD_X (&as->texture.matrix, pBox->x1),
			  COMP_TEX_COORD_Y (&as->texture.matrix, pBox->y2));
	    glVertex2i (pBox->x1, pBox->y2);
	    glTexCoord2f (COMP_TEX_COORD_X (&as->texture.matrix, pBox->x2),
			  COMP_TEX_COORD_Y (&as->texture.matrix, pBox->y2));
	    glVertex2i (pBox->x2, pBox->y2);
	    glTexCoord2f (COMP_TEX_COORD_X (&as->texture.matrix, pBox->x2),
			  COMP_TEX_COORD_Y (&as->texture.matrix, pBox->y1));
	    glVertex2i (pBox->x2, pBox->y1);
	    glTexCoord2f (COMP_TEX_COORD_X (&as->texture.matrix, pBox->x1),
			  COMP_TEX_COORD_Y (&as->texture.matrix, pBox->y1));
	    glVertex2i (pBox->x1, pBox->y1);

	    pBox++;
	}

	glEnd ();

	disableTexture (s, &as->texture);

	glDisable (GL_BLEND);
	glEnableClientState (GL_TEXTURE_COORD_ARRAY);

	glPopMatrix ();
    }

    return status;
}

static void
annoHandleMotionEvent (CompScreen *s,
		       int	  xRoot,
		       int	  yRoot)
{
    ANNO_SCREEN (s);

    if (as->grabIndex)
    {
	cairo_t *cr;
	double  x1, y1, x2, y2;
	REGION  reg;

	cr = annoCairoContext (s);
	if (!cr)
	    return;

	cairo_move_to (cr, annoLastPointerX, annoLastPointerY);
	cairo_line_to (cr, xRoot, yRoot);
	cairo_stroke_extents (cr, &x1, &y1, &x2, &y2);
	cairo_stroke (cr);

	as->content = TRUE;

	annoLastPointerX = xRoot;
	annoLastPointerY = yRoot;

	reg.rects    = &reg.extents;
	reg.numRects = 1;

	reg.extents.x1 = x1;
	reg.extents.y1 = y1;
	reg.extents.x2 = (x2 + 0.5);
	reg.extents.y2 = (y2 + 0.5);

	damageScreenRegion (s, &reg);
    }
}

static void
annoHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompScreen *s;

    ANNO_DISPLAY (d);

    switch (event->type) {
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	    annoHandleMotionEvent (s, pointerX, pointerY);
	break;
    case EnterNotify:
    case LeaveNotify:
	s = findScreenAtDisplay (d, event->xcrossing.root);
	if (s)
	    annoHandleMotionEvent (s, pointerX, pointerY);
    default:
	break;
    }

    UNWRAP (ad, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (ad, d, handleEvent, annoHandleEvent);
}

static void
annoDisplayInitOptions (AnnoDisplay *ad,
			Display	    *display)
{
    CompOption *o;

    o = &ad->opt[ANNO_DISPLAY_OPTION_INITIATE];
    o->name			     = "initiate";
    o->shortDesc		     = N_("Initiate");
    o->longDesc			     = N_("Initiate annotate drawing");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = annoInitiate;
    o->value.action.terminate	     = annoTerminate;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.state	     = CompActionStateInitButton;
    o->value.action.state	    |= CompActionStateInitKey;
    o->value.action.button.modifiers = ANNO_INITIATE_BUTTON_MODIFIERS_DEFAULT;
    o->value.action.button.button    = ANNO_INITIATE_BUTTON_DEFAULT;

    o = &ad->opt[ANNO_DISPLAY_OPTION_ERASE];
    o->name			     = "erase";
    o->shortDesc		     = N_("Initiate erase");
    o->longDesc			     = N_("Initiate annotate erasing");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = annoEraseInitiate;
    o->value.action.terminate	     = annoTerminate;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.state	     = CompActionStateInitButton;
    o->value.action.state	    |= CompActionStateInitKey;
    o->value.action.button.modifiers = ANNO_ERASE_BUTTON_MODIFIERS_DEFAULT;
    o->value.action.button.button    = ANNO_ERASE_BUTTON_DEFAULT;

    o = &ad->opt[ANNO_DISPLAY_OPTION_CLEAR];
    o->name			  = "clear";
    o->shortDesc		  = N_("Clear");
    o->longDesc			  = N_("Clear");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = annoClear;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.key.modifiers = ANNO_CLEAR_KEY_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (ANNO_CLEAR_KEY_DEFAULT));

    o             = &ad->opt[ANNO_DISPLAY_OPTION_COLOR];
    o->name       = "color";
    o->shortDesc  = N_("Annotate Color");
    o->longDesc   = N_("Line color for annotations");
    o->type       = CompOptionTypeColor;
    o->value.c[0] = ANNO_COLOR_RED_DEFAULT;
    o->value.c[1] = ANNO_COLOR_GREEN_DEFAULT;
    o->value.c[2] = ANNO_COLOR_BLUE_DEFAULT;
    o->value.c[3] = 0xffff;
}

static CompOption *
annoGetDisplayOptions (CompDisplay *display,
		       int	   *count)
{
    ANNO_DISPLAY (display);

    *count = NUM_OPTIONS (ad);
    return ad->opt;
}

static Bool
annoSetDisplayOption (CompDisplay    *display,
		      char	     *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    ANNO_DISPLAY (display);

    o = compFindOption (ad->opt, NUM_OPTIONS (ad), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case ANNO_DISPLAY_OPTION_INITIATE:
    case ANNO_DISPLAY_OPTION_ERASE:
    case ANNO_DISPLAY_OPTION_CLEAR:
	if (setDisplayAction (display, o, value))
	    return TRUE;
	break;
    case ANNO_DISPLAY_OPTION_COLOR:
	if (compSetColorOption (o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static Bool
annoInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    AnnoDisplay *ad;

    ad = malloc (sizeof (AnnoDisplay));
    if (!ad)
	return FALSE;

    ad->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (ad->screenPrivateIndex < 0)
    {
	free (ad);
	return FALSE;
    }

    WRAP (ad, d, handleEvent, annoHandleEvent);

    annoDisplayInitOptions (ad, d->display);

    d->privates[displayPrivateIndex].ptr = ad;

    return TRUE;
}

static void
annoFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    ANNO_DISPLAY (d);

    freeScreenPrivateIndex (d, ad->screenPrivateIndex);

    UNWRAP (ad, d, handleEvent);

    free (ad);
}

static Bool
annoInitScreen (CompPlugin *p,
		CompScreen *s)
{
    AnnoScreen *as;

    ANNO_DISPLAY (s->display);

    as = malloc (sizeof (AnnoScreen));
    if (!as)
	return FALSE;

    as->grabIndex = 0;
    as->surface   = NULL;
    as->pixmap    = None;
    as->cairo     = NULL;
    as->content   = FALSE;

    initTexture (s, &as->texture);

    addScreenAction (s, &ad->opt[ANNO_DISPLAY_OPTION_INITIATE].value.action);
    addScreenAction (s, &ad->opt[ANNO_DISPLAY_OPTION_ERASE].value.action);
    addScreenAction (s, &ad->opt[ANNO_DISPLAY_OPTION_CLEAR].value.action);

    WRAP (as, s, paintScreen, annoPaintScreen);

    s->privates[ad->screenPrivateIndex].ptr = as;

    return TRUE;
}

static void
annoFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    ANNO_SCREEN (s);

    if (as->cairo)
	cairo_destroy (as->cairo);

    if (as->surface)
	cairo_surface_destroy (as->surface);

    finiTexture (s, &as->texture);

    if (as->pixmap)
	XFreePixmap (s->display->display, as->pixmap);

    UNWRAP (as, s, paintScreen);

    free (as);
}

static Bool
annoInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
annoFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
annoGetVersion (CompPlugin *plugin,
		int	   version)
{
    return ABIVERSION;
}

static CompPluginVTable annoVTable = {
    "annotate",
    N_("Annotate"),
    N_("Annotate plugin"),
    annoGetVersion,
    annoInit,
    annoFini,
    annoInitDisplay,
    annoFiniDisplay,
    annoInitScreen,
    annoFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    annoGetDisplayOptions,
    annoSetDisplayOption,
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &annoVTable;
}
