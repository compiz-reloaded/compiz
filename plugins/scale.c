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

#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <compiz-scale.h>

#define EDGE_STATE (CompActionStateInitEdge)

#define WIN_X(w) ((w)->attrib.x - (w)->input.left)
#define WIN_Y(w) ((w)->attrib.y - (w)->input.top)
#define WIN_W(w) ((w)->width + (w)->input.left + (w)->input.right)
#define WIN_H(w) ((w)->height + (w)->input.top + (w)->input.bottom)

static CompMetadata scaleMetadata;

static int scaleDisplayPrivateIndex;

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
scaleGetScreenOptions (CompPlugin *plugin,
		       CompScreen *screen,
		       int	  *count)
{
    SCALE_SCREEN (screen);

    *count = NUM_OPTIONS (ss);
    return ss->opt;
}

static Bool
scaleSetScreenOption (CompPlugin      *plugin,
		      CompScreen      *screen,
		      const char      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    SCALE_SCREEN (screen);

    o = compFindOption (ss->opt, NUM_OPTIONS (ss), name, &index);

    if (!o)
	return FALSE;

    switch (index) {
    case SCALE_SCREEN_OPTION_OPACITY:
	if (compSetIntOption (o, value))
	{
	    ss->opacity = (OPAQUE * o->value.i) / 100;
	    return TRUE;
	}
	break;
    default:
	return compSetScreenOption (screen, o, value);
    }

    return FALSE;
}

static Bool
isNeverScaleWin (CompWindow *w)
{
    if (w->attrib.override_redirect)
	return TRUE;

    if (w->wmType & (CompWindowTypeDockMask | CompWindowTypeDesktopMask))
	return TRUE;

    return FALSE;
}

static Bool
isScaleWin (CompWindow *w)
{
    SCALE_SCREEN (w->screen);

    if (isNeverScaleWin (w))
	return FALSE;

    if (!ss->type || ss->type == ScaleTypeOutput)
    {
	if (!(*w->screen->focusWindow) (w))
	    return FALSE;
    }

    if (w->state & CompWindowStateSkipPagerMask)
	return FALSE;

    if (w->state & CompWindowStateShadedMask)
	return FALSE;

    if (!w->mapNum || w->attrib.map_state != IsViewable)
	return FALSE;

    switch (ss->type) {
    case ScaleTypeGroup:
	if (ss->clientLeader != w->clientLeader &&
	    ss->clientLeader != w->id)
	    return FALSE;
	break;
    case ScaleTypeOutput:
	if (outputDeviceForWindow(w) != w->screen->currentOutputDev)
	    return FALSE;
    default:
	break;
    }

    if (!matchEval (ss->currentMatch, w))
	return FALSE;

    return TRUE;
}

static void
scaleActivateEvent (CompScreen *s,
		    Bool       activating)
{
    CompOption o[2];

    o[0].type = CompOptionTypeInt;
    o[0].name = "root";
    o[0].value.i = s->root;

    o[1].type = CompOptionTypeBool;
    o[1].name = "active";
    o[1].value.b = activating;

    (*s->display->handleCompizEvent) (s->display, "scale", "activate", o, 2);
}

static void
scalePaintDecoration (CompWindow	      *w,
		      const WindowPaintAttrib *attrib,
		      const CompTransform     *transform,
		      Region		      region,
		      unsigned int	      mask)
{
    CompScreen *s = w->screen;

    SCALE_SCREEN (s);

    if (ss->opt[SCALE_SCREEN_OPTION_ICON].value.i != SCALE_ICON_NONE)
    {
	WindowPaintAttrib sAttrib = *attrib;
	CompIcon	  *icon;

	SCALE_WINDOW (w);

	icon = getWindowIcon (w, 96, 96);
	if (!icon)
	    icon = w->screen->defaultIcon;

	if (icon && (icon->texture.name || iconToTexture (w->screen, icon)))
	{
	    REGION iconReg;
	    float  scale;
	    float  x, y;
	    int    width, height;
	    int    scaledWinWidth, scaledWinHeight;
	    float  ds;

	    scaledWinWidth  = w->width  * sw->scale;
	    scaledWinHeight = w->height * sw->scale;

	    switch (ss->opt[SCALE_SCREEN_OPTION_ICON].value.i) {
	    case SCALE_ICON_NONE:
	    case SCALE_ICON_EMBLEM:
		scale = 1.0f;
		break;
	    case SCALE_ICON_BIG:
	    default:
		sAttrib.opacity /= 3;
		scale = MIN (((float) scaledWinWidth / icon->width),
			     ((float) scaledWinHeight / icon->height));
		break;
	    }

	    width  = icon->width  * scale;
	    height = icon->height * scale;

	    switch (ss->opt[SCALE_SCREEN_OPTION_ICON].value.i) {
	    case SCALE_ICON_NONE:
	    case SCALE_ICON_EMBLEM:
		x = w->attrib.x + scaledWinWidth - icon->width;
		y = w->attrib.y + scaledWinHeight - icon->height;
		break;
	    case SCALE_ICON_BIG:
	    default:
		x = w->attrib.x + scaledWinWidth / 2 - width / 2;
		y = w->attrib.y + scaledWinHeight / 2 - height / 2;
		break;
	    }

	    x += sw->tx;
	    y += sw->ty;

	    if (sw->slot)
	    {
		sw->delta =
		    fabs (sw->slot->x1 - w->attrib.x) +
		    fabs (sw->slot->y1 - w->attrib.y) +
		    fabs (1.0f - sw->slot->scale) * 500.0f;
	    }

	    if (sw->delta)
	    {
		float o;

		ds =
		    fabs (sw->tx) +
		    fabs (sw->ty) +
		    fabs (1.0f - sw->scale) * 500.0f;

		if (ds > sw->delta)
		    ds = sw->delta;

		o = ds / sw->delta;

		if (sw->slot)
		{
		    if (o < sw->lastThumbOpacity)
			o = sw->lastThumbOpacity;
		}
		else
		{
		    if (o > sw->lastThumbOpacity)
			o = 0.0f;
		}

		sw->lastThumbOpacity = o;

		sAttrib.opacity = sAttrib.opacity * o;
	    }

	    mask |= PAINT_WINDOW_BLEND_MASK;

	    iconReg.rects    = &iconReg.extents;
	    iconReg.numRects = 1;

	    iconReg.extents.x1 = 0;
	    iconReg.extents.y1 = 0;
	    iconReg.extents.x2 = iconReg.extents.x1 + width;
	    iconReg.extents.y2 = iconReg.extents.y1 + height;

	    w->vCount = w->indexCount = 0;
	    if (iconReg.extents.x1 < iconReg.extents.x2 &&
		iconReg.extents.y1 < iconReg.extents.y2)
		(*w->screen->addWindowGeometry) (w,
						 &icon->texture.matrix, 1,
						 &iconReg, &iconReg);

	    if (w->vCount)
	    {
		FragmentAttrib fragment;
		CompTransform  wTransform = *transform;

		initFragmentAttrib (&fragment, &sAttrib);

		matrixScale (&wTransform, scale, scale, 1.0f);
		matrixTranslate (&wTransform, x / scale, y / scale, 0.0f);

		glPushMatrix ();
		glLoadMatrixf (wTransform.m);

		(*w->screen->drawWindowTexture) (w,
						 &icon->texture, &fragment,
						 mask);

		glPopMatrix ();
	    }
	}
    }
}

static Bool
setScaledPaintAttributes (CompWindow        *w,
			  WindowPaintAttrib *attrib)
{
    Bool drawScaled = FALSE;

    SCALE_SCREEN (w->screen);
    SCALE_WINDOW (w);

    if (sw->adjust || sw->slot)
    {
	SCALE_DISPLAY (w->screen->display);

	if (w->id	    != sd->selectedWindow &&
	    ss->opacity != OPAQUE		  &&
	    ss->state   != SCALE_STATE_IN)
	{
	    /* modify opacity of windows that are not active */
	    attrib->opacity = (attrib->opacity * ss->opacity) >> 16;
	}

	drawScaled = TRUE;
    }
    else if (ss->state != SCALE_STATE_IN)
    {
	if (ss->opt[SCALE_SCREEN_OPTION_DARKEN_BACK].value.b)
	{
	    /* modify brightness of the other windows */
	    attrib->brightness = attrib->brightness / 2;
	}

	/* hide windows on the outputs used for scaling
	   that are not in scale mode */
	if (!isNeverScaleWin (w))
	{
	    int moMode;
	    moMode = ss->opt[SCALE_SCREEN_OPTION_MULTIOUTPUT_MODE].value.i;

	    switch (moMode) {
	    case SCALE_MOMODE_CURRENT:
		if (outputDeviceForWindow (w) == w->screen->currentOutputDev)
		    attrib->opacity = 0;
		break;
	    default:
		attrib->opacity = 0;
		break;
	    }
	}
    }

    return drawScaled;
}

static Bool
scalePaintWindow (CompWindow		  *w,
		  const WindowPaintAttrib *attrib,
		  const CompTransform	  *transform,
		  Region		  region,
		  unsigned int		  mask)
{
    CompScreen *s = w->screen;
    Bool       status;

    SCALE_SCREEN (s);

    if (ss->state != SCALE_STATE_NONE)
    {
	WindowPaintAttrib sAttrib = *attrib;
	Bool		  scaled;

	SCALE_WINDOW (w);

	scaled = (*ss->setScaledPaintAttributes) (w, &sAttrib);

	if (sw->adjust || sw->slot)
	    mask |= PAINT_WINDOW_NO_CORE_INSTANCE_MASK;

	UNWRAP (ss, s, paintWindow);
	status = (*s->paintWindow) (w, &sAttrib, transform, region, mask);
	WRAP (ss, s, paintWindow, scalePaintWindow);

	if (scaled)
	{
	    FragmentAttrib fragment;
	    CompTransform  wTransform = *transform;

	    if (mask & PAINT_WINDOW_OCCLUSION_DETECTION_MASK)
		return FALSE;

	    initFragmentAttrib (&fragment, &w->lastPaint);

	    if (w->alpha || fragment.opacity != OPAQUE)
		mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

	    matrixTranslate (&wTransform, w->attrib.x, w->attrib.y, 0.0f);
	    matrixScale (&wTransform, sw->scale, sw->scale, 1.0f);
	    matrixTranslate (&wTransform,
			     sw->tx / sw->scale - w->attrib.x,
			     sw->ty / sw->scale - w->attrib.y,
			     0.0f);

	    glPushMatrix ();
	    glLoadMatrixf (wTransform.m);

	    (*s->drawWindow) (w, &wTransform, &fragment, region,
			      mask | PAINT_WINDOW_TRANSFORMED_MASK);

	    glPopMatrix ();

	    (*ss->scalePaintDecoration) (w, &sAttrib, transform, region, mask);
	}
    }
    else
    {
	UNWRAP (ss, s, paintWindow);
	status = (*s->paintWindow) (w, attrib, transform, region, mask);
	WRAP (ss, s, paintWindow, scalePaintWindow);
    }

    return status;
}

static int
compareWindowsDistance (const void *elem1,
			const void *elem2)
{
    CompWindow *w1 = *((CompWindow **) elem1);
    CompWindow *w2 = *((CompWindow **) elem2);

    SCALE_SCREEN (w1->screen);

    return
	GET_SCALE_WINDOW (w1, ss)->distance -
	GET_SCALE_WINDOW (w2, ss)->distance;
}

static void
layoutSlotsForArea (CompScreen *s,
		    XRectangle workArea,
		    int        nWindows)
{
    int i, j;
    int x, y, width, height;
    int lines, n, nSlots;
    int spacing;

    SCALE_SCREEN (s);

    if (!nWindows)
	return;

    lines   = sqrt (nWindows + 1);
    spacing = ss->opt[SCALE_SCREEN_OPTION_SPACING].value.i;
    nSlots  = 0;

    y      = workArea.y + spacing;
    height = (workArea.height - (lines + 1) * spacing) / lines;

    for (i = 0; i < lines; i++)
    {
	n = MIN (nWindows - nSlots,
		 ceilf ((float)nWindows / lines));

	x     = workArea.x + spacing;
	width = (workArea.width - (n + 1) * spacing) / n;

	for (j = 0; j < n; j++)
	{
	    ss->slots[ss->nSlots].x1 = x;
	    ss->slots[ss->nSlots].y1 = y;
	    ss->slots[ss->nSlots].x2 = x + width;
	    ss->slots[ss->nSlots].y2 = y + height;

	    ss->slots[ss->nSlots].filled = FALSE;

	    x += width + spacing;

	    ss->nSlots++;
	    nSlots++;
	}

	y += height + spacing;
    }
}

static SlotArea *
getSlotAreas (CompScreen *s)
{
    int        i;
    XRectangle workArea;
    float      *size;
    float      sizePerWindow, sum = 0.0f;
    int        left;
    SlotArea   *slotAreas;

    SCALE_SCREEN (s);

    size = malloc (s->nOutputDev * sizeof (int));
    if (!size)
	return NULL;

    slotAreas = malloc (s->nOutputDev * sizeof (SlotArea));
    if (!slotAreas)
    {
	free (size);
	return NULL;
    }

    left = ss->nWindows;

    for (i = 0; i < s->nOutputDev; i++)
    {
	/* determine the size of the workarea for each output device */
	workArea = s->outputDev[i].workArea;

	size[i] = workArea.width * workArea.height;
	sum += size[i];

	slotAreas[i].nWindows = 0;
	slotAreas[i].workArea = workArea;
    }

    /* calculate size available for each window */
    sizePerWindow = sum / ss->nWindows;

    for (i = 0; i < s->nOutputDev && left; i++)
    {
	/* fill the areas with windows */
	int nw = floor (size[i] / sizePerWindow);

	nw = MIN (nw, left);
	size[i] -= nw * sizePerWindow;
	slotAreas[i].nWindows = nw;
	left -= nw;
    }

    /* add left windows to output devices with the biggest free space */
    while (left > 0)
    {
	int   num = 0;
	float big = 0;

	for (i = 0; i < s->nOutputDev; i++)
	{
	    if (size[i] > big)
	    {
		num = i;
		big = size[i];
	    }
	}

	size[num] -= sizePerWindow;
	slotAreas[num].nWindows++;
	left--;
    }

    free (size);

    return slotAreas;
}

static void
layoutSlots (CompScreen *s)
{
    int i;
    int moMode;

    SCALE_SCREEN (s);

    moMode  = ss->opt[SCALE_SCREEN_OPTION_MULTIOUTPUT_MODE].value.i;

    /* if we have only one head, we don't need the
       additional effort of the all outputs mode */
    if (s->nOutputDev == 1)
	moMode = SCALE_MOMODE_CURRENT;

    ss->nSlots = 0;

    switch (moMode)
    {
    case SCALE_MOMODE_ALL:
	{
	    SlotArea *slotAreas;
	    slotAreas = getSlotAreas (s);
	    if (slotAreas)
	    {
		for (i = 0; i < s->nOutputDev; i++)
		    layoutSlotsForArea (s,
					slotAreas[i].workArea,
					slotAreas[i].nWindows);
		free (slotAreas);
	    }
	}
	break;
    case SCALE_MOMODE_CURRENT:
    default:
	{
	    XRectangle workArea;
	    workArea = s->outputDev[s->currentOutputDev].workArea;
	    layoutSlotsForArea (s, workArea, ss->nWindows);
	}
	break;
    }
}

static void
findBestSlots (CompScreen *s)
{
    CompWindow *w;
    int        i, j, d, d0 = 0;
    float      sx, sy, cx, cy;

    SCALE_SCREEN (s);

    for (i = 0; i < ss->nWindows; i++)
    {
	w = ss->windows[i];

	SCALE_WINDOW (w);

	if (sw->slot)
	    continue;

	sw->sid      = 0;
	sw->distance = MAXSHORT;

	for (j = 0; j < ss->nSlots; j++)
	{
	    if (!ss->slots[j].filled)
	    {
		sx = (ss->slots[j].x2 + ss->slots[j].x1) / 2;
		sy = (ss->slots[j].y2 + ss->slots[j].y1) / 2;

		cx = w->serverX + w->width  / 2;
		cy = w->serverY + w->height / 2;

		cx -= sx;
		cy -= sy;

		d = sqrt (cx * cx + cy * cy);
		if (d0 + d < sw->distance)
		{
		    sw->sid      = j;
		    sw->distance = d0 + d;
		}
	    }
	}

	d0 += sw->distance;
    }
}

static Bool
fillInWindows (CompScreen *s)
{
    CompWindow *w;
    int        i, width, height;
    float      sx, sy, cx, cy;

    SCALE_SCREEN (s);

    for (i = 0; i < ss->nWindows; i++)
    {
	w = ss->windows[i];

	SCALE_WINDOW (w);

	if (!sw->slot)
	{
	    if (ss->slots[sw->sid].filled)
		return TRUE;

	    sw->slot = &ss->slots[sw->sid];

	    width  = w->width  + w->input.left + w->input.right;
	    height = w->height + w->input.top  + w->input.bottom;

	    sx = (float) (sw->slot->x2 - sw->slot->x1) / width;
	    sy = (float) (sw->slot->y2 - sw->slot->y1) / height;

	    sw->slot->scale = MIN (MIN (sx, sy), 1.0f);

	    sx = width  * sw->slot->scale;
	    sy = height * sw->slot->scale;
	    cx = (sw->slot->x1 + sw->slot->x2) / 2;
	    cy = (sw->slot->y1 + sw->slot->y2) / 2;

	    cx += w->input.left * sw->slot->scale;
	    cy += w->input.top  * sw->slot->scale;

	    sw->slot->x1 = cx - sx / 2;
	    sw->slot->y1 = cy - sy / 2;
	    sw->slot->x2 = cx + sx / 2;
	    sw->slot->y2 = cy + sy / 2;

	    sw->slot->filled = TRUE;

	    sw->lastThumbOpacity = 0.0f;

	    sw->adjust = TRUE;
	}
    }

    return FALSE;
}

static Bool
layoutSlotsAndAssignWindows (CompScreen *s)
{
    SCALE_SCREEN (s);

    /* create a grid of slots */
    layoutSlots (s);

    do
    {
	/* find most appropriate slots for windows */
	findBestSlots (s);

	/* sort windows, window with closest distance to a slot first */
	qsort (ss->windows, ss->nWindows, sizeof (CompWindow *),
	       compareWindowsDistance);

    } while (fillInWindows (s));

    return TRUE;
}

static Bool
layoutThumbs (CompScreen *s)
{
    CompWindow *w;

    SCALE_SCREEN (s);

    ss->nWindows = 0;

    /* add windows scale list, top most window first */
    for (w = s->reverseWindows; w; w = w->prev)
    {
	SCALE_WINDOW (w);

	if (sw->slot)
	    sw->adjust = TRUE;

	sw->slot = 0;

	if (!isScaleWin (w))
	    continue;

	if (ss->windowsSize <= ss->nWindows)
	{
	    ss->windows = realloc (ss->windows,
				   sizeof (CompWindow *) * (ss->nWindows + 32));
	    if (!ss->windows)
		return FALSE;

	    ss->windowsSize = ss->nWindows + 32;
	}

	ss->windows[ss->nWindows++] = w;
    }

    if (ss->nWindows == 0)
	return FALSE;

    if (ss->slotsSize < ss->nWindows)
    {
	ss->slots = realloc (ss->slots, sizeof (ScaleSlot) * ss->nWindows);
	if (!ss->slots)
	    return FALSE;

	ss->slotsSize = ss->nWindows;
    }

    return (*ss->layoutSlotsAndAssignWindows) (s);
}

static int
adjustScaleVelocity (CompWindow *w)
{
    float dx, dy, ds, adjust, amount;
    float x1, y1, scale;

    SCALE_WINDOW (w);

    if (sw->slot)
    {
	x1 = sw->slot->x1;
	y1 = sw->slot->y1;
	scale = sw->slot->scale;
    }
    else
    {
	x1 = w->attrib.x;
	y1 = w->attrib.y;
	scale = 1.0f;
    }

    dx = x1 - (w->attrib.x + sw->tx);

    adjust = dx * 0.15f;
    amount = fabs (dx) * 1.5f;
    if (amount < 0.5f)
	amount = 0.5f;
    else if (amount > 5.0f)
	amount = 5.0f;

    sw->xVelocity = (amount * sw->xVelocity + adjust) / (amount + 1.0f);

    dy = y1 - (w->attrib.y + sw->ty);

    adjust = dy * 0.15f;
    amount = fabs (dy) * 1.5f;
    if (amount < 0.5f)
	amount = 0.5f;
    else if (amount > 5.0f)
	amount = 5.0f;

    sw->yVelocity = (amount * sw->yVelocity + adjust) / (amount + 1.0f);

    ds = scale - sw->scale;

    adjust = ds * 0.1f;
    amount = fabs (ds) * 7.0f;
    if (amount < 0.01f)
	amount = 0.01f;
    else if (amount > 0.15f)
	amount = 0.15f;

    sw->scaleVelocity = (amount * sw->scaleVelocity + adjust) /
	(amount + 1.0f);

    if (fabs (dx) < 0.1f && fabs (sw->xVelocity) < 0.2f &&
	fabs (dy) < 0.1f && fabs (sw->yVelocity) < 0.2f &&
	fabs (ds) < 0.001f && fabs (sw->scaleVelocity) < 0.002f)
    {
	sw->xVelocity = sw->yVelocity = sw->scaleVelocity = 0.0f;
	sw->tx = x1 - w->attrib.x;
	sw->ty = y1 - w->attrib.y;
	sw->scale = scale;

	return 0;
    }

    return 1;
}

static Bool
scalePaintOutput (CompScreen		  *s,
		  const ScreenPaintAttrib *sAttrib,
		  const CompTransform	  *transform,
		  Region		  region,
		  CompOutput		  *output,
		  unsigned int		  mask)
{
    Bool status;

    SCALE_SCREEN (s);

    if (ss->state != SCALE_STATE_NONE)
	mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

    UNWRAP (ss, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (ss, s, paintOutput, scalePaintOutput);

    return status;
}

static void
scalePreparePaintScreen (CompScreen *s,
			 int	     msSinceLastPaint)
{
    SCALE_SCREEN (s);

    if (ss->state != SCALE_STATE_NONE && ss->state != SCALE_STATE_WAIT)
    {
	CompWindow *w;
	int        steps;
	float      amount, chunk;

	amount = msSinceLastPaint * 0.05f *
	    ss->opt[SCALE_SCREEN_OPTION_SPEED].value.f;
	steps  = amount /
	    (0.5f * ss->opt[SCALE_SCREEN_OPTION_TIMESTEP].value.f);
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    ss->moreAdjust = 0;

	    for (w = s->windows; w; w = w->next)
	    {
		SCALE_WINDOW (w);

		if (sw->adjust)
		{
		    sw->adjust = adjustScaleVelocity (w);

		    ss->moreAdjust |= sw->adjust;

		    sw->tx += sw->xVelocity * chunk;
		    sw->ty += sw->yVelocity * chunk;
		    sw->scale += sw->scaleVelocity * chunk;
		}
	    }

	    if (!ss->moreAdjust)
		break;
	}
    }

    UNWRAP (ss, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (ss, s, preparePaintScreen, scalePreparePaintScreen);
}

static void
scaleDonePaintScreen (CompScreen *s)
{
    SCALE_SCREEN (s);

    if (ss->state != SCALE_STATE_NONE)
    {
	if (ss->moreAdjust)
	{
	    damageScreen (s);
	}
	else
	{
	    if (ss->state == SCALE_STATE_IN)
	    {
		/* The FALSE activate event is sent when scale state
		   goes back to normal, to avoid animation conflicts
		   with other plugins. */
		scaleActivateEvent (s, FALSE);
		ss->state = SCALE_STATE_NONE;
	    }
	    else if (ss->state == SCALE_STATE_OUT)
		ss->state = SCALE_STATE_WAIT;
	}
    }

    UNWRAP (ss, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ss, s, donePaintScreen, scaleDonePaintScreen);
}

static CompWindow *
scaleCheckForWindowAt (CompScreen *s,
		       int        x,
		       int        y)
{
    int        x1, y1, x2, y2;
    CompWindow *w;

    for (w = s->reverseWindows; w; w = w->prev)
    {
	SCALE_WINDOW (w);

	if (sw->slot)
	{
	    x1 = w->attrib.x - w->input.left * sw->scale;
	    y1 = w->attrib.y - w->input.top  * sw->scale;
	    x2 = w->attrib.x + (w->width  + w->input.right)  * sw->scale;
	    y2 = w->attrib.y + (w->height + w->input.bottom) * sw->scale;

	    x1 += sw->tx;
	    y1 += sw->ty;
	    x2 += sw->tx;
	    y2 += sw->ty;

	    if (x1 <= x && y1 <= y && x2 > x && y2 > y)
		return w;
	}
    }

    return 0;
}

static void
sendDndStatusMessage (CompScreen *s,
		      Window	 source)
{
    XEvent xev;

    SCALE_SCREEN (s);

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = s->display->display;
    xev.xclient.format  = 32;

    xev.xclient.message_type = s->display->xdndStatusAtom;
    xev.xclient.window	     = source;

    xev.xclient.data.l[0] = ss->dndTarget;
    xev.xclient.data.l[1] = 2;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = None;

    XSendEvent (s->display->display, source, FALSE, 0, &xev);
}

static Bool
scaleActionShouldToggle (CompDisplay     *d,
			 CompAction      *action,
			 CompActionState state)
{
    SCALE_DISPLAY (d);

    if (state & EDGE_STATE)
	return TRUE;

    if (state & (CompActionStateInitKey | CompActionStateTermKey))
    {
	if (sd->opt[SCALE_DISPLAY_OPTION_KEY_BINDINGS_TOGGLE].value.b)
	    return TRUE;
	else if (!action->key.modifiers)
	    return TRUE;
    }

    if (state & (CompActionStateInitButton | CompActionStateTermButton))
	if (sd->opt[SCALE_DISPLAY_OPTION_BUTTON_BINDINGS_TOGGLE].value.b)
	    return TRUE;

    return FALSE;
}

static Bool
scaleTerminate (CompDisplay     *d,
		CompAction      *action,
		CompActionState state,
		CompOption      *option,
		int	        nOption)
{
    CompScreen *s;
    Window     xid;

    SCALE_DISPLAY (d);

    if (!scaleActionShouldToggle (d, action, state))
    {
	xid = getIntOptionNamed (option, nOption, "root", 0);

	for (s = d->screens; s; s = s->next)
	{
	    SCALE_SCREEN (s);

	    if (xid && s->root != xid)
		continue;

	    if (!ss->grab)
		continue;

	    if (ss->grabIndex)
	    {
		removeScreenGrab (s, ss->grabIndex, 0);
		ss->grabIndex = 0;
	    }

	    if (ss->dndTarget)
		XUnmapWindow (d->display, ss->dndTarget);

	    ss->grab = FALSE;

	    if (ss->state != SCALE_STATE_NONE)
	    {
		CompWindow *w;

		for (w = s->windows; w; w = w->next)
		{
		    SCALE_WINDOW (w);

		    if (sw->slot)
		    {
			sw->slot = 0;
			sw->adjust = TRUE;
		    }
		}

		if (state & CompActionStateCancel)
		{
		    if (d->activeWindow != sd->previousActiveWindow)
		    {
			w = findWindowAtScreen (s, sd->previousActiveWindow);
			if (w)
			    moveInputFocusToWindow (w);
		    }
		}
		else if (ss->state != SCALE_STATE_IN)
		{
		    w = findWindowAtScreen (s, sd->selectedWindow);
		    if (w)
			(*s->activateWindow) (w);
		}

		ss->state = SCALE_STATE_IN;

		damageScreen (s);
	    }

	    sd->lastActiveNum = 0;
	}
    }

    action->state &= ~(CompActionStateTermKey | CompActionStateTermButton);

    return FALSE;
}

static Bool
scaleEnsureDndRedirectWindow (CompScreen *s)
{
    SCALE_SCREEN (s);

    if (!ss->dndTarget)
    {
	XSetWindowAttributes attr;
	long		     xdndVersion = 3;

	attr.override_redirect = TRUE;

	ss->dndTarget = XCreateWindow (s->display->display,
				       s->root,
				       0, 0, 1, 1, 0,
				       CopyFromParent,
				       InputOnly,
				       CopyFromParent,
				       CWOverrideRedirect, &attr);

	XChangeProperty (s->display->display, ss->dndTarget,
			 s->display->xdndAwareAtom,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) &xdndVersion, 1);
    }

    XMoveResizeWindow (s->display->display, ss->dndTarget,
		       0, 0, s->width, s->height);
    XMapRaised (s->display->display, ss->dndTarget);

    return TRUE;
}

static Bool
scaleInitiateCommon (CompScreen      *s,
		     CompAction      *action,
		     CompActionState state,
		     CompOption      *option,
		     int	     nOption)
{
    CompMatch *match;

    SCALE_DISPLAY (s->display);
    SCALE_SCREEN (s);

    if (otherScreenGrabExist (s, "scale", NULL))
	return FALSE;

    ss->currentMatch = &ss->opt[SCALE_SCREEN_OPTION_WINDOW_MATCH].value.match;

    match = getMatchOptionNamed (option, nOption, "match", NULL);
    if (match)
    {
	matchFini (&ss->match);
	matchInit (&ss->match);
	if (matchCopy (&ss->match, match))
	{
	    matchUpdate (s->display, &ss->match);
	    ss->currentMatch = &ss->match;
	}
    }

    if (!layoutThumbs (s))
	return FALSE;

    if (state & CompActionStateInitEdgeDnd)
    {
	if (scaleEnsureDndRedirectWindow (s))
	    ss->grab = TRUE;
    }
    else if (!ss->grabIndex)
    {
	ss->grabIndex = pushScreenGrab (s, ss->cursor, "scale");
	if (ss->grabIndex)
	    ss->grab = TRUE;
    }

    if (ss->grab)
    {
	if (!sd->lastActiveNum)
	    sd->lastActiveNum = s->activeNum - 1;

	sd->previousActiveWindow = s->display->activeWindow;
	sd->lastActiveWindow     = s->display->activeWindow;
	sd->selectedWindow       = s->display->activeWindow;
	sd->hoveredWindow        = None;

	ss->state = SCALE_STATE_OUT;

	scaleActivateEvent (s, TRUE);

	damageScreen (s);
    }

    if ((state & (CompActionStateInitButton | EDGE_STATE)) ==
	CompActionStateInitButton)
    {
	action->state |= CompActionStateTermButton;
    }

    if (state & CompActionStateInitKey)
	action->state |= CompActionStateTermKey;

    return FALSE;
}

static Bool
scaleInitiate (CompDisplay     *d,
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
	SCALE_SCREEN (s);

	if (ss->state != SCALE_STATE_WAIT && ss->state != SCALE_STATE_OUT)
	{
	    ss->type = ScaleTypeNormal;
	    return scaleInitiateCommon (s, action, state, option, nOption);
	}
	else if (scaleActionShouldToggle (d, action, state))
	{
	    if (ss->type == ScaleTypeNormal)
		return scaleTerminate (s->display, action,
				       CompActionStateCancel, option, nOption);
	}
    }

    return FALSE;
}

static Bool
scaleInitiateAll (CompDisplay     *d,
		  CompAction      *action,
		  CompActionState state,
		  CompOption      *option,
		  int		  nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SCALE_SCREEN (s);

	if (ss->state != SCALE_STATE_WAIT && ss->state != SCALE_STATE_OUT)
	{
	    ss->type = ScaleTypeAll;
	    return scaleInitiateCommon (s, action, state, option, nOption);
	}
	else if (scaleActionShouldToggle (d, action, state))
	{
	    if (ss->type == ScaleTypeAll)
		return scaleTerminate (s->display, action,
				       CompActionStateCancel, option, nOption);
	}
    }

    return FALSE;
}

static Bool
scaleInitiateGroup (CompDisplay     *d,
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
	SCALE_SCREEN (s);

	if (ss->state != SCALE_STATE_WAIT && ss->state != SCALE_STATE_OUT)
	{
	    CompWindow *w;

	    w = findWindowAtDisplay (d, getIntOptionNamed (option, nOption,
							   "window", 0));
	    if (w)
	    {
		ss->type	 = ScaleTypeGroup;
		ss->clientLeader = (w->clientLeader) ? w->clientLeader : w->id;

		return scaleInitiateCommon (s, action, state, option, nOption);
	    }
	}
	else if (scaleActionShouldToggle (d, action, state))
	{
	    if (ss->type == ScaleTypeGroup)
		return scaleTerminate (s->display, action,
				       CompActionStateCancel, option, nOption);
	}
    }

    return FALSE;
}

static Bool
scaleInitiateOutput (CompDisplay     *d,
		     CompAction      *action,
		     CompActionState state,
		     CompOption      *option,
		     int	     nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SCALE_SCREEN (s);

	if (ss->state != SCALE_STATE_WAIT && ss->state != SCALE_STATE_OUT)
	{
	    ss->type = ScaleTypeOutput;
	    return scaleInitiateCommon (s, action, state, option, nOption);
	}
	else if (scaleActionShouldToggle (d, action, state))
	{
	    if (ss->type == ScaleTypeOutput)
		return scaleTerminate (s->display, action,
				       CompActionStateCancel, option, nOption);
	}
    }

    return FALSE;
}

static void
scaleSelectWindow (CompWindow *w)

{
    SCALE_DISPLAY (w->screen->display);

    if (sd->selectedWindow != w->id)
    {
	CompWindow *old, *new;

	old = findWindowAtScreen (w->screen, sd->selectedWindow);
	new = findWindowAtScreen (w->screen, w->id);

	sd->selectedWindow = w->id;

	if (old)
	    addWindowDamage (old);

	if (new)
	    addWindowDamage (new);
    }
}

static Bool
scaleSelectWindowAt (CompScreen *s,
		     int	 x,
		     int	 y,
		     Bool	 moveInputFocus)

{
    CompWindow *w;

    SCALE_DISPLAY (s->display);

    w = scaleCheckForWindowAt (s, x, y);
    if (w && isScaleWin (w))
    {
	SCALE_SCREEN (s);

	(*ss->selectWindow) (w);

	if (moveInputFocus)
	{
	    sd->lastActiveNum    = w->activeNum;
	    sd->lastActiveWindow = w->id;

	    moveInputFocusToWindow (w);
	}

	sd->hoveredWindow = w->id;

	return TRUE;
    }

    sd->hoveredWindow = None;

    return FALSE;
}

static void
scaleMoveFocusWindow (CompScreen *s,
		      int	 dx,
		      int	 dy)

{
    CompWindow *active;
    CompWindow *focus = NULL;

    active = findWindowAtScreen (s, s->display->activeWindow);
    if (active)
    {
	SCALE_WINDOW (active);

	if (sw->slot)
	{
	    SCALE_SCREEN (s);

	    CompWindow *w;
	    ScaleSlot  *slot;
	    int	       x, y, cx, cy, d, min = MAXSHORT;

	    cx = (sw->slot->x1 + sw->slot->x2) / 2;
	    cy = (sw->slot->y1 + sw->slot->y2) / 2;

	    for (w = s->windows; w; w = w->next)
	    {
		slot = GET_SCALE_WINDOW (w, ss)->slot;
		if (!slot)
		    continue;

		x = (slot->x1 + slot->x2) / 2;
		y = (slot->y1 + slot->y2) / 2;

		d = abs (x - cx) + abs (y - cy);
		if (d < min)
		{
		    if ((dx > 0 && slot->x1 < sw->slot->x2) ||
			(dx < 0 && slot->x2 > sw->slot->x1) ||
			(dy > 0 && slot->y1 < sw->slot->y2) ||
			(dy < 0 && slot->y2 > sw->slot->y1))
			continue;

		    min   = d;
		    focus = w;
		}
	    }
	}
    }

    /* move focus to the last focused window if no slot window is currently
       focused */
    if (!focus)
    {
	CompWindow *w;

	SCALE_SCREEN (s);

	for (w = s->windows; w; w = w->next)
	{
	    if (!GET_SCALE_WINDOW (w, ss)->slot)
		continue;

	    if (!focus || focus->activeNum < w->activeNum)
		focus = w;
	}
    }

    if (focus)
    {
	SCALE_DISPLAY (s->display);
	SCALE_SCREEN (s);

	(*ss->selectWindow) (focus);

	sd->lastActiveNum    = focus->activeNum;
	sd->lastActiveWindow = focus->id;

	moveInputFocusToWindow (focus);
    }
}

static Bool
scaleRelayoutSlots (CompDisplay     *d,
		    CompAction      *action,
		    CompActionState state,
		    CompOption      *option,
		    int	            nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	SCALE_SCREEN (s);

	if (ss->state != SCALE_STATE_NONE && ss->state != SCALE_STATE_IN)
	{
	    if (layoutThumbs (s))
	    {
		ss->state = SCALE_STATE_OUT;
		scaleMoveFocusWindow (s, 0, 0);
		damageScreen (s);
	    }
	}

	return TRUE;
    }

    return FALSE;
}

static void
scaleWindowRemove (CompDisplay *d,
		   CompWindow  *w)
{
    if (w)
    {
	SCALE_SCREEN (w->screen);

	if (ss->state != SCALE_STATE_NONE && ss->state != SCALE_STATE_IN)
	{
	    int i;

	    for (i = 0; i < ss->nWindows; i++)
	    {
		if (ss->windows[i] == w)
		{
		    if (layoutThumbs (w->screen))
		    {
			ss->state = SCALE_STATE_OUT;
			damageScreen (w->screen);
			break;
		    }
		    else
		    {
			CompOption o;
			CompAction *action;
			int        opt;

			SCALE_DISPLAY (d);

			/* terminate scale mode if the recently closed
			 * window was the last scaled window */

			o.type    = CompOptionTypeInt;
			o.name    = "root";
			o.value.i = w->screen->root;

			opt = SCALE_DISPLAY_OPTION_INITIATE_EDGE;
			action = &sd->opt[opt].value.action;
			scaleTerminate (d, action,
					CompActionStateCancel, &o, 1);

			opt = SCALE_DISPLAY_OPTION_INITIATE_KEY;
			action = &sd->opt[opt].value.action;
			scaleTerminate (d, action,
					CompActionStateCancel, &o, 1);
			break;
		    }
		}
	    }
	}
    }
}

static Bool
scaleHoverTimeout (void *closure)
{
    CompScreen *s = closure;

    SCALE_SCREEN (s);
    SCALE_DISPLAY (s->display);

    if (ss->grab && ss->state != SCALE_STATE_IN)
    {
	CompWindow *w;
	CompOption o;
	int	   option;

	w = findWindowAtDisplay (s->display, sd->selectedWindow);
	if (w)
	{
	    sd->lastActiveNum    = w->activeNum;
	    sd->lastActiveWindow = w->id;

	    moveInputFocusToWindow (w);
	}

	o.type    = CompOptionTypeInt;
	o.name    = "root";
	o.value.i = s->root;

	option = SCALE_DISPLAY_OPTION_INITIATE_EDGE;
	scaleTerminate (s->display, &sd->opt[option].value.action, 0, &o, 1);
	option = SCALE_DISPLAY_OPTION_INITIATE_KEY;
	scaleTerminate (s->display, &sd->opt[option].value.action, 0, &o, 1);
    }

    ss->hoverHandle = 0;

    return FALSE;
}

static void
scaleHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    CompScreen *s;
    Bool       consumeEvent = FALSE;
    CompWindow *w = NULL;

    SCALE_DISPLAY (d);

    switch (event->type) {
    case KeyPress:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    SCALE_SCREEN (s);

	    if (ss->grabIndex)
	    {
		if (event->xkey.keycode == sd->leftKeyCode)
		{
		    scaleMoveFocusWindow (s, -1, 0);
		    consumeEvent = TRUE;
		}
		else if (event->xkey.keycode == sd->rightKeyCode)
		{
		    scaleMoveFocusWindow (s, 1, 0);
		    consumeEvent = TRUE;
		}
		else if (event->xkey.keycode == sd->upKeyCode)
		{
		    scaleMoveFocusWindow (s, 0, -1);
		    consumeEvent = TRUE;
		}
		else if (event->xkey.keycode == sd->downKeyCode)
		{
		    scaleMoveFocusWindow (s, 0, 1);
		    consumeEvent = TRUE;
		}
	    }
	}
	break;
    case ButtonPress:
	if (event->xbutton.button == Button1)
	{
	    s = findScreenAtDisplay (d, event->xbutton.root);
	    if (s)
	    {
		int option;

		SCALE_SCREEN (s);

		if (ss->grabIndex && ss->state != SCALE_STATE_IN)
		{
		    CompOption o;

		    o.type    = CompOptionTypeInt;
		    o.name    = "root";
		    o.value.i = s->root;

		    if (scaleSelectWindowAt (s,
					     event->xbutton.x_root,
					     event->xbutton.y_root,
					     TRUE))
		    {
			option = SCALE_DISPLAY_OPTION_INITIATE_EDGE;
			scaleTerminate (s->display,
					&sd->opt[option].value.action,
					0, &o, 1);
			option = SCALE_DISPLAY_OPTION_INITIATE_KEY;
			scaleTerminate (s->display,
					&sd->opt[option].value.action,
					0, &o, 1);
		    }
		    else if (event->xbutton.x_root > s->workArea.x &&
			     event->xbutton.x_root < (s->workArea.x +
						      s->workArea.width) &&
			     event->xbutton.y_root > s->workArea.y &&
			     event->xbutton.y_root < (s->workArea.y +
						      s->workArea.height))
		    {
			if (sd->opt[SCALE_DISPLAY_OPTION_SHOW_DESKTOP].value.b)
			{
			    option = SCALE_DISPLAY_OPTION_INITIATE_EDGE;
			    scaleTerminate (s->display,
					    &sd->opt[option].value.action,
					    0, &o, 1);
			    option = SCALE_DISPLAY_OPTION_INITIATE_KEY;
			    scaleTerminate (s->display,
					    &sd->opt[option].value.action,
					    0, &o, 1);
			    (*s->enterShowDesktopMode) (s);
			}
		    }
		}
	    }
	}
	break;
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	{
	    SCALE_SCREEN (s);

	    if (ss->grabIndex && ss->state != SCALE_STATE_IN)
	    {
		Bool focus;

		focus = !d->opt[COMP_DISPLAY_OPTION_CLICK_TO_FOCUS].value.b;

		scaleSelectWindowAt (s,
				     event->xmotion.x_root,
				     event->xmotion.y_root,
				     focus);
	    }
	}
	break;
    case DestroyNotify:
	/* We need to get the CompWindow * for event->xdestroywindow.window
	   here because in the (*d->handleEvent) call below, that CompWindow's
	   id will become 1, so findWindowAtDisplay won't be able to find the
	   CompWindow after that. */
	w = findWindowAtDisplay (d, event->xdestroywindow.window);
	break;
    case ClientMessage:
	if (event->xclient.message_type == d->xdndPositionAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		Bool focus;

		SCALE_SCREEN (w->screen);

		s = w->screen;

		focus = !d->opt[COMP_DISPLAY_OPTION_CLICK_TO_FOCUS].value.b;

		if (w->id == ss->dndTarget)
		    sendDndStatusMessage (w->screen, event->xclient.data.l[0]);

		if (ss->grab			&&
		    ss->state != SCALE_STATE_IN &&
		    w->id == ss->dndTarget)
		{
		    int x = event->xclient.data.l[2] >> 16;
		    int y = event->xclient.data.l[2] & 0xffff;

		    w = scaleCheckForWindowAt (s, x, y);
		    if (w && isScaleWin (w))
		    {
			int time;

			time = ss->opt[SCALE_SCREEN_OPTION_HOVER_TIME].value.i;

			if (ss->hoverHandle)
			{
			    if (w->id != sd->selectedWindow)
			    {
				compRemoveTimeout (ss->hoverHandle);
				ss->hoverHandle = 0;
			    }
			}

			if (!ss->hoverHandle)
			    ss->hoverHandle =
				compAddTimeout (time, (float) time * 1.2,
						scaleHoverTimeout,
						s);

			scaleSelectWindowAt (s, x, y, focus);
		    }
		    else
		    {
			if (ss->hoverHandle)
			    compRemoveTimeout (ss->hoverHandle);

			ss->hoverHandle = 0;
		    }
		}
	    }
	}
	else if (event->xclient.message_type == d->xdndDropAtom ||
		 event->xclient.message_type == d->xdndLeaveAtom)
	{
	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		int option;

		SCALE_SCREEN (w->screen);

		if (ss->grab			&&
		    ss->state != SCALE_STATE_IN &&
		    w->id == ss->dndTarget)
		{
		    CompOption o;

		    o.type    = CompOptionTypeInt;
		    o.name    = "root";
		    o.value.i = w->screen->root;

		    option = SCALE_DISPLAY_OPTION_INITIATE_EDGE;
		    scaleTerminate (d, &sd->opt[option].value.action,
				    0, &o, 1);
		    option = SCALE_DISPLAY_OPTION_INITIATE_KEY;
		    scaleTerminate (d, &sd->opt[option].value.action,
				    0, &o, 1);
		}
	    }
	}
    default:
	break;
    }

    if (!consumeEvent)
    {
	UNWRAP (sd, d, handleEvent);
	(*d->handleEvent) (d, event);
	WRAP (sd, d, handleEvent, scaleHandleEvent);
    }

    switch (event->type) {
    case UnmapNotify:
	w = findWindowAtDisplay (d, event->xunmap.window);
	scaleWindowRemove (d, w);
	break;
    case DestroyNotify:
	scaleWindowRemove (d, w);
	break;
    }

}

static Bool
scaleDamageWindowRect (CompWindow *w,
		       Bool	  initial,
		       BoxPtr     rect)
{
    Bool status = FALSE;

    SCALE_SCREEN (w->screen);

    if (initial)
    {
	if (ss->grab && isScaleWin (w))
	{
	    if (layoutThumbs (w->screen))
	    {
		ss->state = SCALE_STATE_OUT;
		damageScreen (w->screen);
	    }
	}
    }
    else if (ss->state == SCALE_STATE_WAIT)
    {
	SCALE_WINDOW (w);

	if (sw->slot)
	{
	    damageTransformedWindowRect (w,
					 sw->scale,
					 sw->scale,
					 sw->tx,
					 sw->ty,
					 rect);

	    status = TRUE;
	}
    }

    UNWRAP (ss, w->screen, damageWindowRect);
    status |= (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ss, w->screen, damageWindowRect, scaleDamageWindowRect);

    return status;
}

static CompOption *
scaleGetDisplayOptions (CompPlugin  *plugin,
			CompDisplay *display,
			int	    *count)
{
    SCALE_DISPLAY (display);

    *count = NUM_OPTIONS (sd);
    return sd->opt;
}

static Bool
scaleSetDisplayOption (CompPlugin      *plugin,
		       CompDisplay     *display,
		       const char      *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    SCALE_DISPLAY (display);

    o = compFindOption (sd->opt, NUM_OPTIONS (sd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case SCALE_DISPLAY_OPTION_ABI:
    case SCALE_DISPLAY_OPTION_INDEX:
	break;
    default:
	return compSetDisplayOption (display, o, value);
    }

    return FALSE;
}

static const CompMetadataOptionInfo scaleDisplayOptionInfo[] = {
    { "abi", "int", 0, 0, 0 },
    { "index", "int", 0, 0, 0 },
    { "initiate_edge", "edge", 0, scaleInitiate, scaleTerminate },
    { "initiate_button", "button", 0, scaleInitiate, scaleTerminate },
    { "initiate_key", "key", 0, scaleInitiate, scaleTerminate },
    { "initiate_all_edge", "edge", 0, scaleInitiateAll, scaleTerminate },
    { "initiate_all_button", "button", 0, scaleInitiateAll, scaleTerminate },
    { "initiate_all_key", "key", 0, scaleInitiateAll, scaleTerminate },
    { "initiate_group_edge", "edge", 0, scaleInitiateGroup, scaleTerminate },
    { "initiate_group_button", "button", 0,
      scaleInitiateGroup, scaleTerminate },
    { "initiate_group_key", "key", 0, scaleInitiateGroup, scaleTerminate },
    { "initiate_output_edge", "edge", 0, scaleInitiateOutput, scaleTerminate },
    { "initiate_output_button", "button", 0,
      scaleInitiateOutput, scaleTerminate },
    { "initiate_output_key", "key", 0, scaleInitiateOutput, scaleTerminate },
    { "show_desktop", "bool", 0, 0, 0 },
    { "relayout_slots", "action", 0, scaleRelayoutSlots, 0 },
    { "key_bindings_toggle", "bool", 0, 0, 0 },
    { "button_bindings_toggle", "bool", 0, 0, 0 }
};

static Bool
scaleInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    ScaleDisplay *sd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    sd = malloc (sizeof (ScaleDisplay));
    if (!sd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &scaleMetadata,
					     scaleDisplayOptionInfo,
					     sd->opt,
					     SCALE_DISPLAY_OPTION_NUM))
    {
	free (sd);
	return FALSE;
    }

    sd->opt[SCALE_DISPLAY_OPTION_ABI].value.i   = SCALE_ABIVERSION;
    sd->opt[SCALE_DISPLAY_OPTION_INDEX].value.i = scaleDisplayPrivateIndex;

    sd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (sd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, sd->opt, SCALE_DISPLAY_OPTION_NUM);
	free (sd);
	return FALSE;
    }

    sd->lastActiveNum = None;
    sd->selectedWindow = None;
    sd->hoveredWindow = None;

    sd->leftKeyCode  = XKeysymToKeycode (d->display, XStringToKeysym ("Left"));
    sd->rightKeyCode = XKeysymToKeycode (d->display, XStringToKeysym ("Right"));
    sd->upKeyCode    = XKeysymToKeycode (d->display, XStringToKeysym ("Up"));
    sd->downKeyCode  = XKeysymToKeycode (d->display, XStringToKeysym ("Down"));

    WRAP (sd, d, handleEvent, scaleHandleEvent);

    d->base.privates[scaleDisplayPrivateIndex].ptr = sd;

    return TRUE;
}

static void
scaleFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    SCALE_DISPLAY (d);

    freeScreenPrivateIndex (d, sd->screenPrivateIndex);

    UNWRAP (sd, d, handleEvent);

    compFiniDisplayOptions (d, sd->opt, SCALE_DISPLAY_OPTION_NUM);

    free (sd);
}

static const CompMetadataOptionInfo scaleScreenOptionInfo[] = {
    { "spacing", "int", "<min>0</min>", 0, 0 },
    { "speed", "float", "<min>0.1</min>", 0, 0 },
    { "timestep", "float", "<min>0.1</min>", 0, 0 },
    { "window_match", "match", 0, 0, 0 },
    { "darken_back", "bool", 0, 0, 0 },
    { "opacity", "int", "<min>0</min><max>100</max>", 0, 0 },
    { "overlay_icon", "int", RESTOSTRING (0, SCALE_ICON_LAST), 0, 0 },
    { "hover_time", "int", "<min>50</min>", 0, 0 },
    { "multioutput_mode", "int", RESTOSTRING (0, SCALE_MOMODE_LAST), 0, 0 }
};

static Bool
scaleInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    ScaleScreen *ss;

    SCALE_DISPLAY (s->display);

    ss = malloc (sizeof (ScaleScreen));
    if (!ss)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &scaleMetadata,
					    scaleScreenOptionInfo,
					    ss->opt,
					    SCALE_SCREEN_OPTION_NUM))
    {
	free (ss);
	return FALSE;
    }

    ss->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ss->windowPrivateIndex < 0)
    {
	compFiniScreenOptions (s, ss->opt, SCALE_SCREEN_OPTION_NUM);
	free (ss);
	return FALSE;
    }

    ss->grab      = FALSE;
    ss->grabIndex = 0;

    ss->hoverHandle = 0;

    ss->dndTarget = None;

    ss->state = SCALE_STATE_NONE;

    ss->slots = 0;
    ss->slotsSize = 0;

    ss->windows = 0;
    ss->windowsSize = 0;

    ss->opacity  =
	(OPAQUE * ss->opt[SCALE_SCREEN_OPTION_OPACITY].value.i) / 100;

    matchInit (&ss->match);

    ss->layoutSlotsAndAssignWindows = layoutSlotsAndAssignWindows;
    ss->setScaledPaintAttributes    = setScaledPaintAttributes;
    ss->scalePaintDecoration	    = scalePaintDecoration;
    ss->selectWindow                = scaleSelectWindow;

    WRAP (ss, s, preparePaintScreen, scalePreparePaintScreen);
    WRAP (ss, s, donePaintScreen, scaleDonePaintScreen);
    WRAP (ss, s, paintOutput, scalePaintOutput);
    WRAP (ss, s, paintWindow, scalePaintWindow);
    WRAP (ss, s, damageWindowRect, scaleDamageWindowRect);

    ss->cursor = XCreateFontCursor (s->display->display, XC_left_ptr);

    s->base.privates[sd->screenPrivateIndex].ptr = ss;

    return TRUE;
}

static void
scaleFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    SCALE_SCREEN (s);

    UNWRAP (ss, s, preparePaintScreen);
    UNWRAP (ss, s, donePaintScreen);
    UNWRAP (ss, s, paintOutput);
    UNWRAP (ss, s, paintWindow);
    UNWRAP (ss, s, damageWindowRect);

    matchFini (&ss->match);

    if (ss->cursor)
	XFreeCursor (s->display->display, ss->cursor);

    if (ss->hoverHandle)
	compRemoveTimeout (ss->hoverHandle);

    if (ss->slotsSize)
	free (ss->slots);

    if (ss->windows)
	free (ss->windows);

    freeWindowPrivateIndex (s, ss->windowPrivateIndex);

    compFiniScreenOptions (s, ss->opt, SCALE_SCREEN_OPTION_NUM);

    free (ss);
}

static Bool
scaleInitWindow (CompPlugin *p,
		 CompWindow *w)
{
    ScaleWindow *sw;

    SCALE_SCREEN (w->screen);

    sw = malloc (sizeof (ScaleWindow));
    if (!sw)
	return FALSE;

    sw->slot = 0;
    sw->scale = 1.0f;
    sw->tx = sw->ty = 0.0f;
    sw->adjust = FALSE;
    sw->xVelocity = sw->yVelocity = 0.0f;
    sw->scaleVelocity = 1.0f;
    sw->delta = 1.0f;
    sw->lastThumbOpacity = 0.0f;

    w->base.privates[ss->windowPrivateIndex].ptr = sw;

    return TRUE;
}

static void
scaleFiniWindow (CompPlugin *p,
		 CompWindow *w)
{
    SCALE_WINDOW (w);

    free (sw);
}

static CompBool
scaleInitObject (CompPlugin *p,
		 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) scaleInitDisplay,
	(InitPluginObjectProc) scaleInitScreen,
	(InitPluginObjectProc) scaleInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
scaleFiniObject (CompPlugin *p,
		 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) scaleFiniDisplay,
	(FiniPluginObjectProc) scaleFiniScreen,
	(FiniPluginObjectProc) scaleFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
scaleGetObjectOptions (CompPlugin *plugin,
		       CompObject *object,
		       int	  *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) scaleGetDisplayOptions,
	(GetPluginObjectOptionsProc) scaleGetScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
scaleSetObjectOption (CompPlugin      *plugin,
		      CompObject      *object,
		      const char      *name,
		      CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) scaleSetDisplayOption,
	(SetPluginObjectOptionProc) scaleSetScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
scaleInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&scaleMetadata,
					 p->vTable->name,
					 scaleDisplayOptionInfo,
					 SCALE_DISPLAY_OPTION_NUM,
					 scaleScreenOptionInfo,
					 SCALE_SCREEN_OPTION_NUM))
	return FALSE;

    scaleDisplayPrivateIndex = allocateDisplayPrivateIndex ();
    if (scaleDisplayPrivateIndex < 0)
    {
	compFiniMetadata (&scaleMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&scaleMetadata, p->vTable->name);

    return TRUE;
}

static void
scaleFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (scaleDisplayPrivateIndex);
    compFiniMetadata (&scaleMetadata);
}

static CompMetadata *
scaleGetMetadata (CompPlugin *plugin)
{
    return &scaleMetadata;
}

CompPluginVTable scaleVTable = {
    "scale",
    scaleGetMetadata,
    scaleInit,
    scaleFini,
    scaleInitObject,
    scaleFiniObject,
    scaleGetObjectOptions,
    scaleSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &scaleVTable;
}
