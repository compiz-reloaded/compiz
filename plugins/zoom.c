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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <X11/cursorfont.h>

#include <compiz-core.h>

static CompMetadata zoomMetadata;

static int displayPrivateIndex;

#define ZOOM_DISPLAY_OPTION_INITIATE_BUTTON 0
#define ZOOM_DISPLAY_OPTION_IN_BUTTON	    1
#define ZOOM_DISPLAY_OPTION_OUT_BUTTON	    2
#define ZOOM_DISPLAY_OPTION_PAN_BUTTON	    3
#define ZOOM_DISPLAY_OPTION_NUM	            4

typedef struct _ZoomDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[ZOOM_DISPLAY_OPTION_NUM];
} ZoomDisplay;

typedef struct _ZoomBox {
    float x1;
    float y1;
    float x2;
    float y2;
} ZoomBox;

#define ZOOM_SCREEN_OPTION_SPEED	 0
#define ZOOM_SCREEN_OPTION_TIMESTEP	 1
#define ZOOM_SCREEN_OPTION_ZOOM_FACTOR   2
#define ZOOM_SCREEN_OPTION_FILTER_LINEAR 3
#define ZOOM_SCREEN_OPTION_NUM		 4

typedef struct _ZoomScreen {
    PreparePaintScreenProc	 preparePaintScreen;
    DonePaintScreenProc		 donePaintScreen;
    PaintOutputProc		 paintOutput;

    CompOption opt[ZOOM_SCREEN_OPTION_NUM];

    float pointerSensitivity;

    int  grabIndex;
    Bool grab;

    int zoomed;

    Bool adjust;

    int    panGrabIndex;
    Cursor panCursor;

    GLfloat velocity;
    GLfloat scale;

    ZoomBox current[16];
    ZoomBox last[16];

    int x1, y1, x2, y2;

    int zoomOutput;
} ZoomScreen;

#define GET_ZOOM_DISPLAY(d)					  \
    ((ZoomDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define ZOOM_DISPLAY(d)		           \
    ZoomDisplay *zd = GET_ZOOM_DISPLAY (d)

#define GET_ZOOM_SCREEN(s, zd)					      \
    ((ZoomScreen *) (s)->base.privates[(zd)->screenPrivateIndex].ptr)

#define ZOOM_SCREEN(s)						        \
    ZoomScreen *zs = GET_ZOOM_SCREEN (s, GET_ZOOM_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
zoomGetScreenOptions (CompPlugin *plugin,
		      CompScreen *screen,
		      int	 *count)
{
    ZOOM_SCREEN (screen);

    *count = NUM_OPTIONS (zs);
    return zs->opt;
}

static Bool
zoomSetScreenOption (CompPlugin      *plugin,
		     CompScreen      *screen,
		     const char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;

    ZOOM_SCREEN (screen);

    o = compFindOption (zs->opt, NUM_OPTIONS (zs), name, NULL);
    if (!o)
	return FALSE;

    return compSetScreenOption (screen, o, value);
}

static int
adjustZoomVelocity (ZoomScreen *zs)
{
    float d, adjust, amount;

    d = (1.0f - zs->scale) * 10.0f;

    adjust = d * 0.002f;
    amount = fabs (d);
    if (amount < 1.0f)
	amount = 1.0f;
    else if (amount > 5.0f)
	amount = 5.0f;

    zs->velocity = (amount * zs->velocity + adjust) / (amount + 1.0f);

    return (fabs (d) < 0.02f && fabs (zs->velocity) < 0.005f);
}

static void
zoomInEvent (CompScreen *s)
{
    CompOption o[6];

    ZOOM_SCREEN (s);

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "root";
    o[0].value.i = s->root;

    o[1].type    = CompOptionTypeInt;
    o[1].name    = "output";
    o[1].value.i = zs->zoomOutput;

    o[2].type    = CompOptionTypeInt;
    o[2].name    = "x1";
    o[2].value.i = zs->current[zs->zoomOutput].x1;

    o[3].type    = CompOptionTypeInt;
    o[3].name    = "y1";
    o[3].value.i = zs->current[zs->zoomOutput].y1;

    o[4].type    = CompOptionTypeInt;
    o[4].name    = "x2";
    o[4].value.i = zs->current[zs->zoomOutput].x2;

    o[5].type    = CompOptionTypeInt;
    o[5].name    = "y2";
    o[5].value.i = zs->current[zs->zoomOutput].y2;

    (*s->display->handleCompizEvent) (s->display, "zoom", "in", o, 6);
}

static void
zoomOutEvent (CompScreen *s)
{
    CompOption o[2];

    ZOOM_SCREEN (s);

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "root";
    o[0].value.i = s->root;

    o[1].type    = CompOptionTypeInt;
    o[1].name    = "output";
    o[1].value.i = zs->zoomOutput;

    (*s->display->handleCompizEvent) (s->display, "zoom", "out", o, 2);
}

static void
zoomPreparePaintScreen (CompScreen *s,
			int	   msSinceLastPaint)
{
    ZOOM_SCREEN (s);

    if (zs->adjust)
    {
	int   steps;
	float amount;

	amount = msSinceLastPaint * 0.35f *
	    zs->opt[ZOOM_SCREEN_OPTION_SPEED].value.f;
	steps  = amount / (0.5f * zs->opt[ZOOM_SCREEN_OPTION_TIMESTEP].value.f);
	if (!steps) steps = 1;

	while (steps--)
	{
	    if (adjustZoomVelocity (zs))
	    {
		BoxPtr pBox = &s->outputDev[zs->zoomOutput].region.extents;

		zs->scale = 1.0f;
		zs->velocity = 0.0f;
		zs->adjust = FALSE;

		if (zs->current[zs->zoomOutput].x1 == pBox->x1 &&
		    zs->current[zs->zoomOutput].y1 == pBox->y1 &&
		    zs->current[zs->zoomOutput].x2 == pBox->x2 &&
		    zs->current[zs->zoomOutput].y2 == pBox->y2)
		{
		    zs->zoomed &= ~(1 << zs->zoomOutput);
		    zoomOutEvent (s);
		}
		else
		{
		    zoomInEvent (s);
		}

		break;
	    }
	    else
	    {
		zs->scale += (zs->velocity * msSinceLastPaint) / s->redrawTime;
	    }
	}
    }

    UNWRAP (zs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (zs, s, preparePaintScreen, zoomPreparePaintScreen);
}

static void
zoomGetCurrentZoom (CompScreen *s,
		    int	       output,
		    ZoomBox    *pBox)
{
    ZOOM_SCREEN (s);

    if (output == zs->zoomOutput)
    {
	float inverse;

	inverse = 1.0f - zs->scale;

	pBox->x1 = zs->scale * zs->current[output].x1 +
	    inverse * zs->last[output].x1;
	pBox->y1 = zs->scale * zs->current[output].y1 +
	    inverse * zs->last[output].y1;
	pBox->x2 = zs->scale * zs->current[output].x2 +
	    inverse * zs->last[output].x2;
	pBox->y2 = zs->scale * zs->current[output].y2 +
	    inverse * zs->last[output].y2;
    }
    else
    {
	pBox->x1 = zs->current[output].x1;
	pBox->y1 = zs->current[output].y1;
	pBox->x2 = zs->current[output].x2;
	pBox->y2 = zs->current[output].y2;
    }
}

static void
zoomDonePaintScreen (CompScreen *s)
{
    ZOOM_SCREEN (s);

    if (zs->adjust)
	damageScreen (s);

    UNWRAP (zs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (zs, s, donePaintScreen, zoomDonePaintScreen);
}

static Bool
zoomPaintOutput (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 const CompTransform	 *transform,
		 Region		         region,
		 CompOutput		 *output,
		 unsigned int		 mask)
{
    CompTransform zTransform = *transform;
    Bool	  status;

    ZOOM_SCREEN (s);

    if (output->id != ~0 && (zs->zoomed & (1 << output->id)))
    {
	int	saveFilter;
	ZoomBox	box;
	float	scale, x, y, x1, y1;
	float	oWidth = output->width;
	float	oHeight = output->height;

	mask &= ~PAINT_SCREEN_REGION_MASK;

	zoomGetCurrentZoom (s, output->id, &box);

	x1 = box.x1 - output->region.extents.x1;
	y1 = box.y1 - output->region.extents.y1;

	scale = oWidth / (box.x2 - box.x1);

	x = ((oWidth  / 2.0f) - x1) / oWidth;
	y = ((oHeight / 2.0f) - y1) / oHeight;

	x = 0.5f - x * scale;
	y = 0.5f - y * scale;

	matrixTranslate (&zTransform, -x, y, 0.0f);
	matrixScale (&zTransform, scale, scale, 1.0f);

	mask |= PAINT_SCREEN_TRANSFORMED_MASK;

	saveFilter = s->filter[SCREEN_TRANS_FILTER];

	if ((zs->zoomOutput != output->id || !zs->adjust) && scale > 3.9f &&
	    !zs->opt[ZOOM_SCREEN_OPTION_FILTER_LINEAR].value.b)
	    s->filter[SCREEN_TRANS_FILTER] = COMP_TEXTURE_FILTER_FAST;

	UNWRAP (zs, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, &zTransform, region, output,
				    mask);
	WRAP (zs, s, paintOutput, zoomPaintOutput);

	s->filter[SCREEN_TRANS_FILTER] = saveFilter;
    }
    else
    {
	UNWRAP (zs, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, transform, region, output,
				    mask);
	WRAP (zs, s, paintOutput, zoomPaintOutput);
    }

    if (status && zs->grab)
    {
	int x1, x2, y1, y2;

	x1 = MIN (zs->x1, zs->x2);
	y1 = MIN (zs->y1, zs->y2);
	x2 = MAX (zs->x1, zs->x2);
	y2 = MAX (zs->y1, zs->y2);

	if (zs->grabIndex)
	{
	    transformToScreenSpace (s, output, -DEFAULT_Z_CAMERA, &zTransform);

	    glPushMatrix ();
	    glLoadMatrixf (zTransform.m);
	    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	    glEnable (GL_BLEND);
	    glColor4us (0x2fff, 0x2fff, 0x4fff, 0x4fff);
	    glRecti (x1, y2, x2, y1);
	    glColor4us (0x2fff, 0x2fff, 0x4fff, 0x9fff);
	    glBegin (GL_LINE_LOOP);
	    glVertex2i (x1, y1);
	    glVertex2i (x2, y1);
	    glVertex2i (x2, y2);
	    glVertex2i (x1, y2);
	    glEnd ();
	    glColor4usv (defaultColor);
	    glDisable (GL_BLEND);
	    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	    glPopMatrix ();
	}
    }

    return status;
}

static void
zoomInitiateForSelection (CompScreen *s,
			  int	     output)
{
    int tmp;

    ZOOM_SCREEN (s);

    if (zs->x1 > zs->x2)
    {
	tmp = zs->x1;
	zs->x1 = zs->x2;
	zs->x2 = tmp;
    }

    if (zs->y1 > zs->y2)
    {
	tmp = zs->y1;
	zs->y1 = zs->y2;
	zs->y2 = tmp;
    }

    if (zs->x1 < zs->x2 && zs->y1 < zs->y2)
    {
	float  oWidth, oHeight;
	float  xScale, yScale, scale;
	BoxRec box;
	int    cx, cy;
	int    width, height;

	oWidth  = s->outputDev[output].width;
	oHeight = s->outputDev[output].height;

	cx = (int) ((zs->x1 + zs->x2) / 2.0f + 0.5f);
	cy = (int) ((zs->y1 + zs->y2) / 2.0f + 0.5f);

	width  = zs->x2 - zs->x1;
	height = zs->y2 - zs->y1;

	xScale = oWidth  / width;
	yScale = oHeight / height;

	scale = MAX (MIN (xScale, yScale), 1.0f);

	box.x1 = cx - (oWidth  / scale) / 2.0f;
	box.y1 = cy - (oHeight / scale) / 2.0f;
	box.x2 = cx + (oWidth  / scale) / 2.0f;
	box.y2 = cy + (oHeight / scale) / 2.0f;

	if (box.x1 < s->outputDev[output].region.extents.x1)
	{
	    box.x2 += s->outputDev[output].region.extents.x1 - box.x1;
	    box.x1 = s->outputDev[output].region.extents.x1;
	}
	else if (box.x2 > s->outputDev[output].region.extents.x2)
	{
	    box.x1 -= box.x2 - s->outputDev[output].region.extents.x2;
	    box.x2 = s->outputDev[output].region.extents.x2;
	}

	if (box.y1 < s->outputDev[output].region.extents.y1)
	{
	    box.y2 += s->outputDev[output].region.extents.y1 - box.y1;
	    box.y1 = s->outputDev[output].region.extents.y1;
	}
	else if (box.y2 > s->outputDev[output].region.extents.y2)
	{
	    box.y1 -= box.y2 - s->outputDev[output].region.extents.y2;
	    box.y2 = s->outputDev[output].region.extents.y2;
	}

	if (zs->zoomed & (1 << output))
	{
	    zoomGetCurrentZoom (s, output, &zs->last[output]);
	}
	else
	{
	    zs->last[output].x1 = s->outputDev[output].region.extents.x1;
	    zs->last[output].y1 = s->outputDev[output].region.extents.y1;
	    zs->last[output].x2 = s->outputDev[output].region.extents.x2;
	    zs->last[output].y2 = s->outputDev[output].region.extents.y2;
	}

	zs->current[output].x1 = box.x1;
	zs->current[output].y1 = box.y1;
	zs->current[output].x2 = box.x2;
	zs->current[output].y2 = box.y2;

	zs->scale = 0.0f;
	zs->adjust = TRUE;
	zs->zoomOutput = output;
	zs->zoomed |= (1 << output);

	damageScreen (s);
    }
}

static Bool
zoomIn (CompDisplay     *d,
	CompAction      *action,
	CompActionState state,
	CompOption      *option,
	int		nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	float   w, h, x0, y0;
	int     output;
	ZoomBox box;

	ZOOM_SCREEN (s);

	output = outputDeviceForPoint (s, pointerX, pointerY);

	if (!zs->grabIndex)
	    zs->grabIndex = pushScreenGrab (s, None, "zoom");

	if (zs->zoomed & (1 << output))
	{
	    zoomGetCurrentZoom (s, output, &box);
	}
	else
	{
	    box.x1 = s->outputDev[output].region.extents.x1;
	    box.y1 = s->outputDev[output].region.extents.y1;
	    box.x2 = s->outputDev[output].region.extents.x2;
	    box.y2 = s->outputDev[output].region.extents.y2;
	}

	w = (box.x2 - box.x1) /
	    zs->opt[ZOOM_SCREEN_OPTION_ZOOM_FACTOR].value.f;
	h = (box.y2 - box.y1) /
	    zs->opt[ZOOM_SCREEN_OPTION_ZOOM_FACTOR].value.f;

	x0 = (pointerX - s->outputDev[output].region.extents.x1) / (float)
	    s->outputDev[output].width;
	y0 = (pointerY - s->outputDev[output].region.extents.y1) / (float)
	    s->outputDev[output].height;

	zs->x1 = box.x1 + (x0 * (box.x2 - box.x1) - x0 * w + 0.5f);
	zs->y1 = box.y1 + (y0 * (box.y2 - box.y1) - y0 * h + 0.5f);
	zs->x2 = zs->x1 + w;
	zs->y2 = zs->y1 + h;

	zoomInitiateForSelection (s, output);

	return TRUE;
    }

    return FALSE;
}

static Bool
zoomInitiate (CompDisplay     *d,
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
	int   output, x1, y1;
	float scale;

	ZOOM_SCREEN (s);

	if (otherScreenGrabExist (s, "zoom", NULL))
	    return FALSE;

	if (!zs->grabIndex)
	    zs->grabIndex = pushScreenGrab (s, None, "zoom");

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	/* start selection zoom rectangle */

	output = outputDeviceForPoint (s, pointerX, pointerY);

	if (zs->zoomed & (1 << output))
	{
	    ZoomBox box;
	    float   oWidth;

	    zoomGetCurrentZoom (s, output, &box);

	    oWidth = s->outputDev[output].width;
	    scale = oWidth / (box.x2 - box.x1);

	    x1 = box.x1;
	    y1 = box.y1;
	}
	else
	{
	    scale = 1.0f;
	    x1 = s->outputDev[output].region.extents.x1;
	    y1 = s->outputDev[output].region.extents.y1;
	}

	zs->x1 = zs->x2 = x1 +
	    ((pointerX - s->outputDev[output].region.extents.x1) /
	     scale + 0.5f);
	zs->y1 = zs->y2 = y1 +
	    ((pointerY - s->outputDev[output].region.extents.y1) /
	     scale + 0.5f);

	zs->zoomOutput = output;

	zs->grab = TRUE;

	damageScreen (s);

	return TRUE;
    }

    return FALSE;
}

static Bool
zoomOut (CompDisplay     *d,
	 CompAction      *action,
	 CompActionState state,
	 CompOption      *option,
	 int	         nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	int output;

	ZOOM_SCREEN (s);

	output = outputDeviceForPoint (s, pointerX, pointerY);

	zoomGetCurrentZoom (s, output, &zs->last[output]);

	zs->current[output].x1 = s->outputDev[output].region.extents.x1;
	zs->current[output].y1 = s->outputDev[output].region.extents.y1;
	zs->current[output].x2 = s->outputDev[output].region.extents.x2;
	zs->current[output].y2 = s->outputDev[output].region.extents.y2;

	zs->zoomOutput = output;
	zs->scale = 0.0f;
	zs->adjust = TRUE;
	zs->grab = FALSE;

	if (zs->grabIndex)
	{
	    removeScreenGrab (s, zs->grabIndex, NULL);
	    zs->grabIndex = 0;
	}

	damageScreen (s);

	return TRUE;
    }

    return FALSE;
}

static Bool
zoomTerminate (CompDisplay     *d,
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
	ZOOM_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (zs->grab)
	{
	    int output;

	    output = outputDeviceForPoint (s, zs->x1, zs->y1);

	    if (zs->x2 > s->outputDev[output].region.extents.x2)
		zs->x2 = s->outputDev[output].region.extents.x2;

	    if (zs->y2 > s->outputDev[output].region.extents.y2)
		zs->y2 = s->outputDev[output].region.extents.y2;

	    zoomInitiateForSelection (s, output);

	    zs->grab = FALSE;
	}
	else
	{
	    CompOption o;

	    o.type    = CompOptionTypeInt;
	    o.name    = "root";
	    o.value.i = s->root;

	    zoomOut (d, action, state, &o, 1);
	}
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

static Bool
zoomInitiatePan (CompDisplay     *d,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int	         nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	int output;

	ZOOM_SCREEN (s);

	output = outputDeviceForPoint (s, pointerX, pointerY);

	if (!(zs->zoomed & (1 << output)))
	    return FALSE;

	if (otherScreenGrabExist (s, "zoom", NULL))
	    return FALSE;

	if (state & CompActionStateInitButton)
	    action->state |= CompActionStateTermButton;

	if (!zs->panGrabIndex)
	    zs->panGrabIndex = pushScreenGrab (s, zs->panCursor, "zoom-pan");

	zs->zoomOutput = output;

	return TRUE;
    }

    return FALSE;
}

static Bool
zoomTerminatePan (CompDisplay     *d,
		  CompAction      *action,
		  CompActionState state,
		  CompOption      *option,
		  int	          nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	ZOOM_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (zs->panGrabIndex)
	{
	    removeScreenGrab (s, zs->panGrabIndex, NULL);
	    zs->panGrabIndex = 0;

	    zoomInEvent (s);
	}

	return TRUE;
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

static void
zoomHandleMotionEvent (CompScreen *s,
		       int	  xRoot,
		       int	  yRoot)
{
    ZOOM_SCREEN (s);

    if (zs->grabIndex)
    {
	int     output = zs->zoomOutput;
	ZoomBox box;
	float   scale, oWidth = s->outputDev[output].width;

	zoomGetCurrentZoom (s, output, &box);

	if (zs->zoomed & (1 << output))
	    scale = oWidth / (box.x2 - box.x1);
	else
	    scale = 1.0f;

	if (zs->panGrabIndex)
	{
	    float dx, dy;

	    dx = (xRoot - lastPointerX) / scale;
	    dy = (yRoot - lastPointerY) / scale;

	    box.x1 -= dx;
	    box.y1 -= dy;
	    box.x2 -= dx;
	    box.y2 -= dy;

	    if (box.x1 < s->outputDev[output].region.extents.x1)
	    {
		box.x2 += s->outputDev[output].region.extents.x1 - box.x1;
		box.x1 = s->outputDev[output].region.extents.x1;
	    }
	    else if (box.x2 > s->outputDev[output].region.extents.x2)
	    {
		box.x1 -= box.x2 - s->outputDev[output].region.extents.x2;
		box.x2 = s->outputDev[output].region.extents.x2;
	    }

	    if (box.y1 < s->outputDev[output].region.extents.y1)
	    {
		box.y2 += s->outputDev[output].region.extents.y1 - box.y1;
		box.y1 = s->outputDev[output].region.extents.y1;
	    }
	    else if (box.y2 > s->outputDev[output].region.extents.y2)
	    {
		box.y1 -= box.y2 - s->outputDev[output].region.extents.y2;
		box.y2 = s->outputDev[output].region.extents.y2;
	    }

	    zs->current[output] = box;

	    damageScreen (s);
	}
	else
	{
	    int x1, y1;

	    if (zs->zoomed & (1 << output))
	    {
		x1 = box.x1;
		y1 = box.y1;
	    }
	    else
	    {
		x1 = s->outputDev[output].region.extents.x1;
		y1 = s->outputDev[output].region.extents.y1;
	    }

	    zs->x2 = x1 +
		((xRoot - s->outputDev[output].region.extents.x1) /
		 scale + 0.5f);
	    zs->y2 = y1 +
		((yRoot - s->outputDev[output].region.extents.y1) /
		 scale + 0.5f);

	    damageScreen (s);
	}
    }
}

static void
zoomHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompScreen *s;

    ZOOM_DISPLAY (d);

    switch (event->type) {
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	    zoomHandleMotionEvent (s, pointerX, pointerY);
	break;
    case EnterNotify:
    case LeaveNotify:
	s = findScreenAtDisplay (d, event->xcrossing.root);
	if (s)
	    zoomHandleMotionEvent (s, pointerX, pointerY);
    default:
	break;
    }

    UNWRAP (zd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (zd, d, handleEvent, zoomHandleEvent);
}

static CompOption *
zoomGetDisplayOptions (CompPlugin  *plugin,
		       CompDisplay *display,
		       int	   *count)
{
    ZOOM_DISPLAY (display);

    *count = NUM_OPTIONS (zd);
    return zd->opt;
}

static Bool
zoomSetDisplayOption (CompPlugin      *plugin,
		      CompDisplay     *display,
		      const char      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    ZOOM_DISPLAY (display);

    o = compFindOption (zd->opt, NUM_OPTIONS (zd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case ZOOM_DISPLAY_OPTION_OUT_BUTTON:
	if (compSetActionOption (o, value))
	    return TRUE;
	break;
    default:
	return compSetDisplayOption (display, o, value);
    }

    return FALSE;
}

static const CompMetadataOptionInfo zoomDisplayOptionInfo[] = {
    { "initiate_button", "button", 0, zoomInitiate, zoomTerminate },
    { "zoom_in_button", "button", 0, zoomIn, 0 },
    { "zoom_out_button", "button", 0, zoomOut, 0 },
    { "zoom_pan_button", "button", 0, zoomInitiatePan, zoomTerminatePan }
};

static Bool
zoomInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    ZoomDisplay *zd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    zd = malloc (sizeof (ZoomDisplay));
    if (!zd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &zoomMetadata,
					     zoomDisplayOptionInfo,
					     zd->opt,
					     ZOOM_DISPLAY_OPTION_NUM))
    {
	free (zd);
	return FALSE;
    }

    zd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (zd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, zd->opt, ZOOM_DISPLAY_OPTION_NUM);
	free (zd);
	return FALSE;
    }

    WRAP (zd, d, handleEvent, zoomHandleEvent);

    d->base.privates[displayPrivateIndex].ptr = zd;

    return TRUE;
}

static void
zoomFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    ZOOM_DISPLAY (d);

    freeScreenPrivateIndex (d, zd->screenPrivateIndex);

    UNWRAP (zd, d, handleEvent);

    compFiniDisplayOptions (d, zd->opt, ZOOM_DISPLAY_OPTION_NUM);

    free (zd);
}

static const CompMetadataOptionInfo zoomScreenOptionInfo[] = {
    { "speed", "float", "<min>0.1</min>", 0, 0 },
    { "timestep", "float", "<min>0.1</min>", 0, 0 },
    { "zoom_factor", "float", "<min>1.01</min>", 0, 0 },
    { "filter_linear", "bool", 0, 0, 0 }
};

static Bool
zoomInitScreen (CompPlugin *p,
		CompScreen *s)
{
    ZoomScreen *zs;

    ZOOM_DISPLAY (s->display);

    zs = malloc (sizeof (ZoomScreen));
    if (!zs)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &zoomMetadata,
					    zoomScreenOptionInfo,
					    zs->opt,
					    ZOOM_SCREEN_OPTION_NUM))
    {
	free (zs);
	return FALSE;
    }

    zs->grabIndex = 0;
    zs->grab = FALSE;

    zs->velocity = 0.0f;

    zs->zoomOutput = 0;

    zs->zoomed = 0;
    zs->adjust = FALSE;

    zs->panGrabIndex = 0;
    zs->panCursor = XCreateFontCursor (s->display->display, XC_fleur);

    zs->scale = 0.0f;

    memset (&zs->current, 0, sizeof (zs->current));
    memset (&zs->last, 0, sizeof (zs->last));

    WRAP (zs, s, preparePaintScreen, zoomPreparePaintScreen);
    WRAP (zs, s, donePaintScreen, zoomDonePaintScreen);
    WRAP (zs, s, paintOutput, zoomPaintOutput);

    s->base.privates[zd->screenPrivateIndex].ptr = zs;

    return TRUE;
}

static void
zoomFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    ZOOM_SCREEN (s);

    if (zs->panCursor)
	XFreeCursor (s->display->display, zs->panCursor);

    UNWRAP (zs, s, preparePaintScreen);
    UNWRAP (zs, s, donePaintScreen);
    UNWRAP (zs, s, paintOutput);

    compFiniScreenOptions (s, zs->opt, ZOOM_SCREEN_OPTION_NUM);

    free (zs);
}

static CompBool
zoomInitObject (CompPlugin *p,
		CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) zoomInitDisplay,
	(InitPluginObjectProc) zoomInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
zoomFiniObject (CompPlugin *p,
		CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) zoomFiniDisplay,
	(FiniPluginObjectProc) zoomFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
zoomGetObjectOptions (CompPlugin *plugin,
		      CompObject *object,
		      int	 *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) zoomGetDisplayOptions,
	(GetPluginObjectOptionsProc) zoomGetScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
zoomSetObjectOption (CompPlugin      *plugin,
		     CompObject      *object,
		     const char      *name,
		     CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) zoomSetDisplayOption,
	(SetPluginObjectOptionProc) zoomSetScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
zoomInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&zoomMetadata,
					 p->vTable->name,
					 zoomDisplayOptionInfo,
					 ZOOM_DISPLAY_OPTION_NUM,
					 zoomScreenOptionInfo,
					 ZOOM_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&zoomMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&zoomMetadata, p->vTable->name);

    return TRUE;
}

static void
zoomFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&zoomMetadata);
}

static CompMetadata *
zoomGetMetadata (CompPlugin *plugin)
{
    return &zoomMetadata;
}

CompPluginVTable zoomVTable = {
    "zoom",
    zoomGetMetadata,
    zoomInit,
    zoomFini,
    zoomInitObject,
    zoomFiniObject,
    zoomGetObjectOptions,
    zoomSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &zoomVTable;
}
