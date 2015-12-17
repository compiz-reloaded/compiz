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
#include <math.h>
#include <string.h>
#include <cairo-xlib-xrender.h>

#include <compiz-core.h>

static CompMetadata annoMetadata;

static int displayPrivateIndex;

static int annoLastPointerX = 0;
static int annoLastPointerY = 0;
static int annoInitialPointerX = 0;
static int annoInitialPointerY = 0;

#define ANNO_DISPLAY_OPTION_FREEDRAW_BUTTON  0
#define ANNO_DISPLAY_OPTION_LINE_BUTTON      1
#define ANNO_DISPLAY_OPTION_RECTANGLE_BUTTON 2
#define ANNO_DISPLAY_OPTION_ELLIPSE_BUTTON   3
#define ANNO_DISPLAY_OPTION_TEXT_BUTTON      4
#define ANNO_DISPLAY_OPTION_ERASE_BUTTON     5
#define ANNO_DISPLAY_OPTION_CLEAR_KEY        6
#define ANNO_DISPLAY_OPTION_CLEAR_BUTTON     7
#define ANNO_DISPLAY_OPTION_CENTER_KEY       8
#define ANNO_DISPLAY_OPTION_FILL_COLOR       9
#define ANNO_DISPLAY_OPTION_STROKE_COLOR     10
#define ANNO_DISPLAY_OPTION_STROKE_WIDTH     11
#define ANNO_DISPLAY_OPTION_NUM              12

typedef struct _Ellipse {
	int centerX;
	int centerY;
	double radiusX;
	double radiusY;
} Ellipse;

typedef struct _Point {
	int x;
	int y;
} Point;

typedef enum _AnnoMode {
	NoMode,
	FreeDrawMode,
	EraseMode,
	LineMode,
	RectangleMode,
	EllipseMode,
	TextMode
} AnnoToolType;

typedef struct _AnnoDisplay {
	int             screenPrivateIndex;
	HandleEventProc handleEvent;

	CompOption      opt[ANNO_DISPLAY_OPTION_NUM];
} AnnoDisplay;

typedef struct _AnnoScreen {
	PaintOutputProc paintOutput;
	int             grabIndex;

	Pixmap          pixmap;
	CompTexture     texture;
	cairo_surface_t *surface;
	cairo_t         *cairo;
	Bool            content;
	Bool            drawFromCenter;

	Damage          damage;
	AnnoToolType    drawMode;
	Ellipse         ellipse;
	Point           lineEndPoint;
	// declarations of these come from X11/XRegion.h
	Box             rectangle;
	Box             lastRectangle;
} AnnoScreen;

#define GET_ANNO_DISPLAY(d)					  \
    ((AnnoDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define ANNO_DISPLAY(d)			   \
    AnnoDisplay *ad = GET_ANNO_DISPLAY (d)

#define GET_ANNO_SCREEN(s, ad)					      \
    ((AnnoScreen *) (s)->base.privates[(ad)->screenPrivateIndex].ptr)

#define ANNO_SCREEN(s)							\
    AnnoScreen *as = GET_ANNO_SCREEN (s, GET_ANNO_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))


#define NUM_TOOLS (sizeof (tools) / sizeof (tools[0]))

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
		Screen            *screen;
		int               w, h;

		screen = ScreenOfDisplay (s->display->display, s->screenNum);

		w = s->width;
		h = s->height;

		format = XRenderFindStandardFormat (s->display->display,
		                    PictStandardARGB32);

		as->pixmap = XCreatePixmap (s->display->display, s->root, w, h, 32);

		if (!bindPixmapToTexture (s, &as->texture, as->pixmap, w, h, 32))
		{
			compLogMessage ("annotate", CompLogLevelError,
			                "Couldn't bind pixmap 0x%x to texture",
			                (int) as->pixmap);

			XFreePixmap (s->display->display, as->pixmap);

			return NULL;
		}

		as->damage = XDamageCreate(s->display->display, as->pixmap, XDamageReportRawRectangles);

		as->surface =
		    cairo_xlib_surface_create_with_xrender_format (s->display->display,
		                           as->pixmap, screen,
		                           format, w, h);

		as->cairo = cairo_create (as->surface);

		annoCairoClear (s, as->cairo);
	}

	return as->cairo;
}

static void
annoSetSourceColor (cairo_t        *cr,
                    unsigned short *color)
{
	cairo_set_source_rgba (cr,
	           (double) color[0] / 0xffff,
	           (double) color[1] / 0xffff,
	           (double) color[2] / 0xffff,
	           (double) color[3] / 0xffff);
}

static void
annoDrawEllipse (CompScreen     *s,
                 double         xc,
                 double         yc,
                 double         radiusX,
                 double         radiusY,
                 unsigned short *fillColor,
                 unsigned short *strokeColor,
                 double         strokeWidth)
{
	cairo_t *cr;

	ANNO_SCREEN (s);

	cr = annoCairoContext (s);
	if (cr)
	{
		annoSetSourceColor (cr, fillColor);
		cairo_save(cr);
		cairo_translate(cr, xc, yc);

		if (radiusX > radiusY)
		{
			cairo_scale (cr, 1.0, radiusY/radiusX);
			cairo_arc (cr, 0, 0, radiusX, 0, 2 * M_PI);
		}
		else
		{
			cairo_scale (cr,  radiusX/radiusY, 1.0);
			cairo_arc (cr, 0, 0, radiusY, 0, 2 * M_PI);
		}

		cairo_fill_preserve (cr);
		cairo_set_line_width (cr, strokeWidth);
		annoSetSourceColor (cr, strokeColor);
		cairo_stroke (cr);
		cairo_restore (cr);

		as->content = TRUE;
	}
}

static void
annoDrawRectangle (CompScreen     *s,
                   double         x,
                   double         y,
                   double         w,
                   double         h,
                   unsigned short *fillColor,
                   unsigned short *strokeColor,
                   double         strokeWidth)
{
	cairo_t *cr;

	ANNO_SCREEN (s);

	cr = annoCairoContext (s);
	if (cr)
	{
		cairo_save(cr);
		annoSetSourceColor (cr, fillColor);
		cairo_rectangle (cr, x, y, w, h);
		cairo_fill_preserve (cr);
		cairo_set_line_width (cr, strokeWidth);
		annoSetSourceColor (cr, strokeColor);
		cairo_stroke (cr);
		cairo_restore(cr);

		as->content = TRUE;
	}
}

static void
annoDrawLine (CompScreen     *s,
              double         x1,
              double         y1,
              double         x2,
              double         y2,
              double         width,
              unsigned short *color)
{
	cairo_t *cr;

	ANNO_SCREEN (s);

	cr = annoCairoContext (s);
	if (cr)
	{
		cairo_save(cr);
		cairo_set_line_width (cr, width);
		cairo_move_to (cr, x1, y1);
		cairo_line_to (cr, x2, y2);
		cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
		annoSetSourceColor (cr, color);
		cairo_stroke (cr);
		cairo_restore(cr);

		as->content = TRUE;
	}
}

static void
annoDrawText (CompScreen     *s,
              double         x,
              double         y,
              char           *text,
              char           *fontFamily,
              double         fontSize,
              int            fontSlant,
              int            fontWeight,
              unsigned short *fillColor,
              unsigned short *strokeColor,
              double         strokeWidth)
{
	cairo_t *cr;

	ANNO_SCREEN (s);

	cr = annoCairoContext (s);
	if (cr)
	{
		cairo_save(cr);
		cairo_set_line_width (cr, strokeWidth);
		annoSetSourceColor (cr, fillColor);
		cairo_select_font_face (cr, fontFamily, fontSlant, fontWeight);
		cairo_set_font_size (cr, fontSize);
		cairo_move_to (cr, x, y);
		cairo_text_path (cr, text);
		cairo_fill_preserve (cr);
		annoSetSourceColor (cr, strokeColor);
		cairo_stroke (cr);
		cairo_restore (cr);

		as->content = TRUE;
	}
}

static Bool
annoDraw (CompDisplay     *d,
          CompAction      *action,
          CompActionState state,
          CompOption      *option,
          int             nOption)
{
	CompScreen *s;
	Window     xid;

	xid  = getIntOptionNamed (option, nOption, "root", 0);

	s = findScreenAtDisplay (d, xid);
	if (s)
	{
		cairo_t *cr;

		cr = annoCairoContext (s);
		if (cr)
		{
			char           *tool;
			unsigned short *fillColor, *strokeColor;
			double         strokeWidth;

			ANNO_DISPLAY (d);

			tool = getStringOptionNamed (option, nOption, "tool", "line");

			cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
			cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

			fillColor = ad->opt[ANNO_DISPLAY_OPTION_FILL_COLOR].value.c;
			fillColor = getColorOptionNamed (option, nOption, "fill_color",
			                 fillColor);

			strokeColor = ad->opt[ANNO_DISPLAY_OPTION_STROKE_COLOR].value.c;
			strokeColor = getColorOptionNamed (option, nOption,
			                   "stroke_color", strokeColor);

			strokeWidth = ad->opt[ANNO_DISPLAY_OPTION_STROKE_WIDTH].value.f;
			strokeWidth = getFloatOptionNamed (option, nOption, "stroke_width",
			                   strokeWidth);

			if (strcasecmp (tool, "rectangle") == 0)
			{
				double x, y, w, h;

				x = getFloatOptionNamed (option, nOption, "x", 0);
				y = getFloatOptionNamed (option, nOption, "y", 0);
				w = getFloatOptionNamed (option, nOption, "w", 100);
				h = getFloatOptionNamed (option, nOption, "h", 100);

				annoDrawRectangle (s, x, y, w, h, fillColor, strokeColor,
				           strokeWidth);
			}
			else if (strcasecmp (tool, "ellipse") == 0)
			{
				double xc, yc, xr, yr;

				xc = getFloatOptionNamed (option, nOption, "xc", 0);
				yc = getFloatOptionNamed (option, nOption, "yc", 0);
				xr = getFloatOptionNamed (option, nOption, "radiusX", 100);
				yr = getFloatOptionNamed (option, nOption, "radiusY", 100);

				annoDrawEllipse (s, xc, yc, xr, yr, fillColor, strokeColor,
				        strokeWidth);
			}
			else if (strcasecmp (tool, "line") == 0)
			{
				double x1, y1, x2, y2;

				x1 = getFloatOptionNamed (option, nOption, "x1", 0);
				y1 = getFloatOptionNamed (option, nOption, "y1", 0);
				x2 = getFloatOptionNamed (option, nOption, "x2", 100);
				y2 = getFloatOptionNamed (option, nOption, "y2", 100);

				annoDrawLine (s, x1, y1, x2, y2, strokeWidth, strokeColor);
			}
			else if (strcasecmp (tool, "text") == 0)
			{
				double       x, y, size;
				char         *text, *family;
				unsigned int slant, weight;
				char         *str;

				str = getStringOptionNamed (option, nOption, "slant", "");
				if (strcasecmp (str, "oblique") == 0)
				    slant = CAIRO_FONT_SLANT_OBLIQUE;
				else if (strcasecmp (str, "italic") == 0)
				    slant = CAIRO_FONT_SLANT_ITALIC;
				else
				    slant = CAIRO_FONT_SLANT_NORMAL;

				str = getStringOptionNamed (option, nOption, "weight", "");
				if (strcasecmp (str, "bold") == 0)
				    weight = CAIRO_FONT_WEIGHT_BOLD;
				else
				    weight = CAIRO_FONT_WEIGHT_NORMAL;

				x      = getFloatOptionNamed (option, nOption, "x", 0);
				y      = getFloatOptionNamed (option, nOption, "y", 0);
				text   = getStringOptionNamed (option, nOption, "text", "");
				family = getStringOptionNamed (option, nOption, "family",
				                   "Sans");
				size   = getFloatOptionNamed (option, nOption, "size", 36.0);

				annoDrawText (s, x, y, text, family, size, slant, weight,
				          fillColor, strokeColor, strokeWidth);
			}
		}
    }

    return FALSE;
}

static Bool
annoFreeDrawInitiate (CompDisplay     *d,
                      CompAction      *action,
                      CompActionState state,
                      CompOption      *option,
                      int             nOption)
{
	CompScreen *s;
	Window     xid;

	xid = getIntOptionNamed (option, nOption, "root", 0);

	s = findScreenAtDisplay (d, xid);
	if (s)
	{
		ANNO_SCREEN (s);

		if (otherScreenGrabExist (s, NULL))
			return FALSE;

		if (!as->grabIndex)
			as->grabIndex = pushScreenGrab (s, None, "annotate");

		if (state & CompActionStateInitButton)
			action->state |= CompActionStateTermButton;

		if (state & CompActionStateInitKey)
			action->state |= CompActionStateTermKey;

		annoLastPointerX = pointerX;
		annoLastPointerY = pointerY;

		as->drawMode = FreeDrawMode;
	}

	return TRUE;
}

static Bool
annoLineInitiate (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int             nOption)
{
	CompScreen *s;
	Window     xid;

	xid = getIntOptionNamed (option, nOption, "root", 0);

	s = findScreenAtDisplay (d, xid);
	if (s)
	{
		ANNO_SCREEN (s);

		if (otherScreenGrabExist (s, NULL))
			return FALSE;

		if (!as->grabIndex)
			as->grabIndex = pushScreenGrab (s, None, "annotate");

		if (state & CompActionStateInitButton)
			action->state |= CompActionStateTermButton;

		if (state & CompActionStateInitKey)
			action->state |= CompActionStateTermKey;

		annoInitialPointerX = pointerX;
		annoInitialPointerY = pointerY;

		as->drawMode = LineMode;
	}

	return TRUE;
}

static Bool
annoRectangleInitiate (CompDisplay     *d,
                       CompAction      *action,
                       CompActionState state,
                       CompOption      *option,
                       int             nOption)
{
	CompScreen *s;
	Window     xid;

	xid = getIntOptionNamed (option, nOption, "root", 0);

	s = findScreenAtDisplay (d, xid);
	if (s)
	{
		ANNO_SCREEN (s);

		if (otherScreenGrabExist (s, NULL))
			return FALSE;

		if (!as->grabIndex)
			as->grabIndex = pushScreenGrab (s, None, "annotate");

		if (state & CompActionStateInitButton)
			action->state |= CompActionStateTermButton;

		if (state & CompActionStateInitKey)
			action->state |= CompActionStateTermKey;

		annoInitialPointerX = pointerX;
		annoInitialPointerY = pointerY;

		as->rectangle.x1 = annoInitialPointerX;
		as->rectangle.y1 = annoInitialPointerY;
		as->rectangle.x2 = 0;
		as->rectangle.y2 = 0;
		as->lastRectangle = as->rectangle;

		as->drawMode = RectangleMode;
	}

	return TRUE;
}

static Bool
annoEllipseInitiate (CompDisplay     *d,
                     CompAction      *action,
                     CompActionState state,
                     CompOption      *option,
                     int             nOption)
{
	CompScreen *s;
	Window     xid;

	xid = getIntOptionNamed (option, nOption, "root", 0);

	s = findScreenAtDisplay (d, xid);
	if (s)
	{
		ANNO_SCREEN (s);

		if (otherScreenGrabExist (s, NULL))
			return FALSE;

		if (!as->grabIndex)
			as->grabIndex = pushScreenGrab (s, None, "annotate");

		if (state & CompActionStateInitButton)
			action->state |= CompActionStateTermButton;

		if (state & CompActionStateInitKey)
			action->state |= CompActionStateTermKey;

		annoInitialPointerX = pointerX;
		annoInitialPointerY = pointerY;

		as->ellipse.radiusX = 0;
		as->ellipse.radiusY = 0;

		as->lastRectangle.x1 = annoInitialPointerX;
		as->lastRectangle.y1 = annoInitialPointerY;
		as->lastRectangle.x2 = 0;
		as->lastRectangle.y2 = 0;

		as->drawMode = EllipseMode;
	}

	return TRUE;
}

static Bool
annoTerminate (CompDisplay     *d,
               CompAction      *action,
               CompActionState state,
               CompOption      *option,
               int             nOption)
{
	CompScreen *s;
	Window     xid;

	unsigned short *fillColor, *strokeColor;
	double strokeWidth;

	xid = getIntOptionNamed (option, nOption, "root", 0);

	for (s = d->screens; s; s = s->next)
	{
		ANNO_SCREEN (s);
		ANNO_DISPLAY (s->display);

		if (xid && s->root != xid)
			continue;

		if (as->grabIndex)
		{
			removeScreenGrab (s, as->grabIndex, NULL);
			as->grabIndex = 0;
		}

		fillColor = ad->opt[ANNO_DISPLAY_OPTION_FILL_COLOR].value.c;
		fillColor = getColorOptionNamed (option, nOption, "fill_color",
		                 fillColor);

		strokeColor = ad->opt[ANNO_DISPLAY_OPTION_STROKE_COLOR].value.c;
		strokeColor = getColorOptionNamed (option, nOption,
		                   "stroke_color", strokeColor);

		strokeWidth = ad->opt[ANNO_DISPLAY_OPTION_STROKE_WIDTH].value.f;
		strokeWidth = getFloatOptionNamed (option, nOption, "stroke_width",
		               strokeWidth);

		if (as->drawMode == LineMode) {
			annoDrawLine(s, annoInitialPointerX, annoInitialPointerY,
			             as->lineEndPoint.x, as->lineEndPoint.y,
			             strokeWidth, strokeColor);
		}
		else if (as->drawMode == RectangleMode) {
			annoDrawRectangle(s, as->rectangle.x1, as->rectangle.y1,
			                  as->rectangle.x2 - as->rectangle.x1,
			                  as->rectangle.y2 - as->rectangle.y1,
			                  fillColor, strokeColor, strokeWidth);
		}
		else if (as->drawMode == EllipseMode) {
			annoDrawEllipse(s, as->ellipse.centerX, as->ellipse.centerY,
			                as->ellipse.radiusX, as->ellipse.radiusY,
			                fillColor, strokeColor, strokeWidth);
		}

		as->drawMode = NoMode;
	}

	action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

	return FALSE;
}

static Bool
annoEraseInitiate (CompDisplay     *d,
                   CompAction      *action,
                   CompActionState state,
                   CompOption      *option,
                   int             nOption)
{
	CompScreen *s;
	Window     xid;

	xid = getIntOptionNamed (option, nOption, "root", 0);

	s = findScreenAtDisplay (d, xid);
	if (s)
	{
		ANNO_SCREEN (s);

		if (otherScreenGrabExist (s, NULL))
			return FALSE;

		if (!as->grabIndex)
			as->grabIndex = pushScreenGrab (s, None, "annotate");

		if (state & CompActionStateInitButton)
			action->state |= CompActionStateTermButton;

		if (state & CompActionStateInitKey)
			action->state |= CompActionStateTermKey;

		annoLastPointerX = pointerX;
		annoLastPointerY = pointerY;

		as->drawMode = EraseMode;
	}

	return FALSE;
}

static Bool
annoClear (CompDisplay     *d,
           CompAction      *action,
           CompActionState state,
           CompOption      *option,
           int             nOption)
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
annoToggleCenter (CompDisplay     *d,
                  CompAction      *action,
                  CompActionState state,
                  CompOption      *option,
                  int             nOption)
{
	CompScreen *s;
	Window     xid;

	xid = getIntOptionNamed (option, nOption, "root", 0);

	s = findScreenAtDisplay (d, xid);
	if (s)
	{
		ANNO_SCREEN (s);
		as->drawFromCenter = !(as->drawFromCenter);
		return TRUE;
	}

	return FALSE;
}

static Bool
annoPaintOutput (CompScreen              *s,
                 const ScreenPaintAttrib *sAttrib,
                 const CompTransform     *transform,
                 Region                  region,
                 CompOutput              *output,
                 unsigned int            mask)
{
	Bool status;

	ANNO_SCREEN (s);
	ANNO_DISPLAY (s->display);

	UNWRAP (as, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
	WRAP (as, s, paintOutput, annoPaintOutput);

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

		unsigned short *fillColor, *strokeColor;
		double strokeWidth, offset;
		int angle;
		double vectorX, vectorY;

		fillColor   = ad->opt[ANNO_DISPLAY_OPTION_FILL_COLOR].value.c;
		strokeColor = ad->opt[ANNO_DISPLAY_OPTION_STROKE_COLOR].value.c;

		strokeWidth = ad->opt[ANNO_DISPLAY_OPTION_STROKE_WIDTH].value.f;
		offset = strokeWidth / 2;

		//put draw code here
		switch(as->drawMode) {
			case LineMode:
				glColor4usv (strokeColor);
				glLineWidth (strokeWidth);
				glBegin (GL_LINES);
				glVertex2i (annoInitialPointerX, annoInitialPointerY);
				glVertex2i (as->lineEndPoint.x, as->lineEndPoint.y);
				glEnd ();
				break;

			case RectangleMode:
				/* fill rectangle */
				glColor4usv (fillColor);
				// normally this would be glRecti(x1, y1, x2, y2), but this would render
				// the rectangle with a winding opposite to what's on screen.
				// That would undo setting the fill. Instead, specify the coordinates in the order
				// glRecti(x1, y1, x2, y2) to make sure that it has the proper winding
				glRecti (as->rectangle.x1, as->rectangle.y2,
				         as->rectangle.x2, as->rectangle.y1);

				/* draw rectangle outline */
				glColor4usv (strokeColor);
				//left edge
				glRecti (as->rectangle.x1 - offset, as->rectangle.y2,
				         as->rectangle.x1 + offset, as->rectangle.y1);
				//right edge
				glRecti (as->rectangle.x2 - offset, as->rectangle.y2,
				         as->rectangle.x2 + offset, as->rectangle.y1);
				//top left, top right, top
				glRecti (as->rectangle.x1 - offset, as->rectangle.y1 + offset,
				         as->rectangle.x2 + offset, as->rectangle.y1 - offset);
				//bottom left, bottom right, bottom
				glRecti (as->rectangle.x1 - offset, as->rectangle.y2 + offset,
				         as->rectangle.x2 + offset, as->rectangle.y2 - offset);
				break;

			case EllipseMode:
				/* fill ellipse */
				glColor4usv (fillColor);

				glBegin (GL_TRIANGLE_FAN);
				glVertex2d (as->ellipse.centerX, as->ellipse.centerY);
				for (angle = 0; angle <= 360; angle += 1)
				{
					vectorX = as->ellipse.centerX +
					         (as->ellipse.radiusX * sinf (angle * DEG2RAD));
					vectorY = as->ellipse.centerY +
					         (as->ellipse.radiusY * cosf (angle * DEG2RAD));
					glVertex2d (vectorX, vectorY);
				}
				glVertex2d (as->ellipse.centerX, as->ellipse.centerY +
					        as->ellipse.radiusY);
				glEnd();

				/* draw ellipse outline */
				glColor4usv (strokeColor);
				glLineWidth (strokeWidth);

				glBegin (GL_TRIANGLE_STRIP);
				glVertex2d (as->ellipse.centerX, as->ellipse.centerY +
				            as->ellipse.radiusY - offset);
				for (angle = 360; angle >= 0; angle -= 1)
				{
					vectorX = as->ellipse.centerX + ((as->ellipse.radiusX -
					          offset) * sinf (angle * DEG2RAD));
					vectorY = as->ellipse.centerY + ((as->ellipse.radiusY -
					          offset) * cosf (angle * DEG2RAD));
					glVertex2d (vectorX, vectorY);
					vectorX = as->ellipse.centerX + ((as->ellipse.radiusX +
					          offset) * sinf (angle * DEG2RAD));
					vectorY = as->ellipse.centerY + ((as->ellipse.radiusY +
					          offset) * cosf (angle * DEG2RAD));
					glVertex2d (vectorX, vectorY);
				}
				glVertex2d (as->ellipse.centerX, as->ellipse.centerY +
					        as->ellipse.radiusY + offset);
				glEnd();
				break;

			default:
				break;
		}

		/* clean up */
		glColor4usv (defaultColor);
		glDisable (GL_BLEND);
		glEnableClientState (GL_TEXTURE_COORD_ARRAY);

		glPopMatrix ();
    }

    return status;
}

static void
annoHandleMotionEvent (CompScreen *s,
                       int        xRoot,
                       int        yRoot)
{
	ANNO_SCREEN (s);

	REGION  damageReg;

	if (as->grabIndex)
	{
		cairo_t *cr;

		if (as->drawMode == EraseMode)
		{
			static unsigned short color[] = { 0, 0, 0, 0 };

			annoDrawLine (s,
				  annoLastPointerX, annoLastPointerY,
				  xRoot, yRoot,
				  20.0, color);
		}
		else if (as->drawMode == FreeDrawMode)
		{
			ANNO_DISPLAY(s->display);

			annoDrawLine (s,
			      annoLastPointerX, annoLastPointerY,
			      xRoot, yRoot,
			      ad->opt[ANNO_DISPLAY_OPTION_STROKE_WIDTH].value.f,
			      ad->opt[ANNO_DISPLAY_OPTION_STROKE_COLOR].value.c);
		}
		else if (as->drawMode == LineMode)
		{
			as->lineEndPoint.x = xRoot;
			as->lineEndPoint.y = yRoot;

			damageReg.rects = &damageReg.extents;
			damageReg.numRects = 1;
			damageReg.extents.x1 = MIN(annoInitialPointerX, as->lineEndPoint.x);
			damageReg.extents.y1 = MIN(annoInitialPointerY, as->lineEndPoint.y);
			damageReg.extents.x2 = damageReg.extents.x1 + abs(as->lineEndPoint.x - annoInitialPointerX);
			damageReg.extents.y2 = damageReg.extents.y1 + abs(as->lineEndPoint.y - annoInitialPointerY);

			//manually set that we have content so the user can see it drawing the first time
			//we also need to properly initialize the cairo context before manually flagging it
			if (!as->content) {
				cr = annoCairoContext(s);
				if (cr) {
					as->content = TRUE;
				}
			}
		}
		else if (as->drawMode == RectangleMode)
		{
			//save the results of the distance between xDist and yDist
			//to avoid recalculating it
			int xDist = abs(xRoot - annoInitialPointerX);
			int yDist = abs(yRoot - annoInitialPointerY);

			if (as->drawFromCenter)
			{
				as->rectangle.x1 = annoInitialPointerX - xDist;
				as->rectangle.y1 = annoInitialPointerY - yDist;
				as->rectangle.x2 = as->rectangle.x1 + xDist * 2;
				as->rectangle.y2 = as->rectangle.y1 + yDist * 2;
			}
			else {
				as->rectangle.x1 = MIN(xRoot, annoInitialPointerX);
				as->rectangle.y1 = MIN(yRoot, annoInitialPointerY);
				as->rectangle.x2 = as->rectangle.x1 + xDist;
				as->rectangle.y2 = as->rectangle.y1 + yDist;
			}

			damageReg.rects = &damageReg.extents;
			damageReg.numRects = 1;
			damageReg.extents = as->rectangle;

			//manually set that we have content so the user can see it drawing the first time
			//we also need to properly initialize the cairo context before manually flagging it
			if (!as->content) {
				cr = annoCairoContext(s);
				if (cr) {
					as->content = TRUE;
				}
			}
		}
		else if (as->drawMode == EllipseMode)
		{
			if (as->drawFromCenter) {
				as->ellipse.centerX = annoInitialPointerX;
				as->ellipse.centerY = annoInitialPointerY;
			}
			else {
				int xDist = xRoot - annoInitialPointerX;
				int yDist = yRoot - annoInitialPointerY;
				as->ellipse.centerX = annoInitialPointerX + xDist / 2;
				as->ellipse.centerY = annoInitialPointerY + yDist / 2;
			}

			as->ellipse.radiusX = abs(as->ellipse.centerX - xRoot);
			as->ellipse.radiusY = abs(as->ellipse.centerY - yRoot);

			damageReg.rects = &damageReg.extents;
			damageReg.numRects = 1;
			damageReg.extents.x1 = as->ellipse.centerX - as->ellipse.radiusX;
			damageReg.extents.y1 = as->ellipse.centerY - as->ellipse.radiusY;
			damageReg.extents.x2 = damageReg.extents.x1 + as->ellipse.radiusX * 2;
			damageReg.extents.y2 = damageReg.extents.y1 + as->ellipse.radiusY * 2;

			//manually set that we have content so the user can see it drawing the first time
			//we also need to properly initialize the cairo context before manually flagging it
			if (!as->content) {
				cr = annoCairoContext(s);
				if (cr) {
					as->content = TRUE;
				}
			}
		}

		if (s && (as->drawMode == LineMode ||
		          as->drawMode == RectangleMode ||
		          as->drawMode == EllipseMode))
		{
			ANNO_DISPLAY(s->display);
			double strokeWidth = ad->opt[ANNO_DISPLAY_OPTION_STROKE_WIDTH].value.f;

			damageReg.extents.x1 -= (strokeWidth / 2);
			damageReg.extents.y1 -= (strokeWidth / 2);
			damageReg.extents.x2 += (strokeWidth + 1);
			damageReg.extents.y2 += (strokeWidth + 1);

			//copy last rectangle into a tmp damage region
			REGION lastRectangleDamageReg;
			lastRectangleDamageReg.rects = &lastRectangleDamageReg.extents;
			lastRectangleDamageReg.numRects = 1;
			lastRectangleDamageReg.extents = as->lastRectangle;

			//update the screen after movement to show WIP
			damageScreenRegion(s, &damageReg);
			damageScreenRegion(s, &lastRectangleDamageReg);

			as->lastRectangle = *damageReg.rects;
		}

		annoLastPointerX = xRoot;
		annoLastPointerY = yRoot;
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
			break;
		default:
			if (event->type == d->damageEvent + XDamageNotify)
			{
				XDamageNotifyEvent *de = (XDamageNotifyEvent *) event;
				int firstScreen, lastScreen;
				if (onlyCurrentScreen) {
					firstScreen = DefaultScreen (d->display);
					lastScreen  = DefaultScreen (d->display);
				}
				else
				{
					firstScreen = 0;
					lastScreen  = ScreenCount (d->display) - 1;
				}

				for (int i = firstScreen; i <= lastScreen; i++)
				{
					s = findScreenAtDisplay (d, XRootWindow (d->display, i));
					ANNO_SCREEN(s);
					if (as->pixmap == de->drawable) {
						REGION reg;
						reg.rects    = &reg.extents;
						reg.numRects = 1;
						reg.extents.x1 = de->area.x;
						reg.extents.y1 = de->area.y;
						reg.extents.x2 = de->area.x + de->area.width;
						reg.extents.y2 = de->area.y + de->area.height;

						damageScreenRegion(s, &reg);
					}
				}
			}
			break;
	}

	UNWRAP (ad, d, handleEvent);
	(*d->handleEvent) (d, event);
	WRAP (ad, d, handleEvent, annoHandleEvent);
}

static CompOption *
annoGetDisplayOptions (CompPlugin  *plugin,
                       CompDisplay *display,
                       int         *count)
{
	ANNO_DISPLAY (display);

	*count = NUM_OPTIONS (ad);
	return ad->opt;
}

static Bool
annoSetDisplayOption (CompPlugin      *plugin,
                      CompDisplay     *display,
                      const char      *name,
                      CompOptionValue *value)
{
	CompOption *o;

	ANNO_DISPLAY (display);

	o = compFindOption (ad->opt, NUM_OPTIONS (ad), name, NULL);
	if (!o)
		return FALSE;

	return compSetDisplayOption (display, o, value);
}

// Make this a lot more readable
static const CompMetadataOptionInfo annoDisplayOptionInfo[] = {
	{ "initiate_free_draw_button", "button", 0, annoFreeDrawInitiate,  annoTerminate },
	{ "initiate_line_button"     , "button", 0, annoLineInitiate,      annoTerminate },
	{ "initiate_rectangle_button", "button", 0, annoRectangleInitiate, annoTerminate },
	{ "initiate_ellipse_button"  , "button", 0, annoEllipseInitiate,   annoTerminate },
	{ "draw"                     , "action", 0, annoDraw,              0 },
	{ "erase_button"             , "button", 0, annoEraseInitiate,     annoTerminate },
	{ "clear_key"                , "key"   , 0, annoClear,             0 },
	{ "clear_button"             , "button", 0, annoClear,             0 },
	{ "center_key"               , "key"   , 0, annoToggleCenter,      0 },
	{ "fill_color"               , "color" , 0, 0,                     0 },
	{ "stroke_color"             , "color" , 0, 0,                     0 },
	{ "line_width"               , "float" , 0, 0,                     0 },
	{ "stroke_width"             , "float" , 0, 0,                     0 }
};

static Bool
annoInitDisplay (CompPlugin  *p,
                 CompDisplay *d)
{
	AnnoDisplay *ad;

	if (!checkPluginABI ("core", CORE_ABIVERSION))
		return FALSE;

	ad = malloc (sizeof (AnnoDisplay));
	if (!ad)
		return FALSE;

	if (!compInitDisplayOptionsFromMetadata (d,
	                     &annoMetadata,
	                     annoDisplayOptionInfo,
	                     ad->opt,
	                     ANNO_DISPLAY_OPTION_NUM))
	{
		free (ad);
		return FALSE;
	}

	ad->screenPrivateIndex = allocateScreenPrivateIndex (d);
	if (ad->screenPrivateIndex < 0)
	{
		compFiniDisplayOptions (d, ad->opt, ANNO_DISPLAY_OPTION_NUM);
		free (ad);
		return FALSE;
	}

	WRAP (ad, d, handleEvent, annoHandleEvent);

	d->base.privates[displayPrivateIndex].ptr = ad;

	return TRUE;
}

static void
annoFiniDisplay (CompPlugin  *p,
                 CompDisplay *d)
{
	ANNO_DISPLAY (d);

	freeScreenPrivateIndex (d, ad->screenPrivateIndex);

	UNWRAP (ad, d, handleEvent);

	compFiniDisplayOptions (d, ad->opt, ANNO_DISPLAY_OPTION_NUM);

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

	WRAP (as, s, paintOutput, annoPaintOutput);

	s->base.privates[ad->screenPrivateIndex].ptr = as;

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

	if (as->damage)
		XDamageDestroy(s->display->display, as->damage);

	UNWRAP (as, s, paintOutput);

	free (as);
}

static CompBool
annoInitObject (CompPlugin *p,
                CompObject *o)
{
	static InitPluginObjectProc dispTab[] = {
		(InitPluginObjectProc) 0, /* InitCore */
		(InitPluginObjectProc) annoInitDisplay,
		(InitPluginObjectProc) annoInitScreen
	};

	RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
annoFiniObject (CompPlugin *p,
                CompObject *o)
{
	static FiniPluginObjectProc dispTab[] = {
		(FiniPluginObjectProc) 0, /* FiniCore */
		(FiniPluginObjectProc) annoFiniDisplay,
		(FiniPluginObjectProc) annoFiniScreen
	};

	DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
annoGetObjectOptions (CompPlugin *plugin,
                      CompObject *object,
                      int        *count)
{
	static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) annoGetDisplayOptions
	};

	*count = 0;
	RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
	         (void *) count, (plugin, object, count));
}

static CompBool
annoSetObjectOption (CompPlugin      *plugin,
                     CompObject      *object,
                     const char      *name,
                     CompOptionValue *value)
{
	static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) annoSetDisplayOption
	};

	RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
	         (plugin, object, name, value));
}

static Bool
annoInit (CompPlugin *p)
{
	if (!compInitPluginMetadataFromInfo (&annoMetadata,
	                 p->vTable->name,
	                 annoDisplayOptionInfo,
	                 ANNO_DISPLAY_OPTION_NUM,
	                 0, 0))
	return FALSE;

	displayPrivateIndex = allocateDisplayPrivateIndex ();
	if (displayPrivateIndex < 0)
	{
		compFiniMetadata (&annoMetadata);
		return FALSE;
	}

	compAddMetadataFromFile (&annoMetadata, p->vTable->name);

	return TRUE;
}

static void
annoFini (CompPlugin *p)
{
	freeDisplayPrivateIndex (displayPrivateIndex);
	compFiniMetadata (&annoMetadata);
}

static CompMetadata *
annoGetMetadata (CompPlugin *plugin)
{
	return &annoMetadata;
}

static CompPluginVTable annoVTable = {
	"annotate",
	annoGetMetadata,
	annoInit,
	annoFini,
	annoInitObject,
	annoFiniObject,
	annoGetObjectOptions,
	annoSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
	return &annoVTable;
}
