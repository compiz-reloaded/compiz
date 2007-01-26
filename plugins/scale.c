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

#include <compiz.h>

#define WIN_X(w) ((w)->attrib.x - (w)->input.left)
#define WIN_Y(w) ((w)->attrib.y - (w)->input.top)
#define WIN_W(w) ((w)->width + (w)->input.left + (w)->input.right)
#define WIN_H(w) ((w)->height + (w)->input.top + (w)->input.bottom)

#define SCALE_SPACING_DEFAULT 10
#define SCALE_SPACING_MIN     0
#define SCALE_SPACING_MAX     250

#define SCALE_INITIATE_KEY_DEFAULT       "Up"
#define SCALE_INITIATE_MODIFIERS_DEFAULT (ControlMask | CompAltMask)

#define SCALE_SPEED_DEFAULT   1.5f
#define SCALE_SPEED_MIN       0.1f
#define SCALE_SPEED_MAX       50.0f
#define SCALE_SPEED_PRECISION 0.1f

#define SCALE_TIMESTEP_DEFAULT   1.2f
#define SCALE_TIMESTEP_MIN       0.1f
#define SCALE_TIMESTEP_MAX       50.0f
#define SCALE_TIMESTEP_PRECISION 0.1f

#define SCALE_STATE_NONE 0
#define SCALE_STATE_OUT  1
#define SCALE_STATE_WAIT 2
#define SCALE_STATE_IN   3

#define SCALE_DARKEN_BACK_DEFAULT TRUE

#define SCALE_OPACITY_DEFAULT 75
#define SCALE_OPACITY_MIN     0
#define SCALE_OPACITY_MAX     100

static char *winType[] = {
    N_("Toolbar"),
    N_("Utility"),
    N_("Dialog"),
    N_("ModalDialog"),
    N_("Fullscreen"),
    N_("Normal")
};
#define N_WIN_TYPE (sizeof (winType) / sizeof (winType[0]))

typedef enum {
    ScaleIconNone = 0,
    ScaleIconEmblem,
    ScaleIconBig
} IconOverlay;

static char *iconOverlayString[] = {
    N_("None"),
    N_("Emblem"),
    N_("Big")
};

static IconOverlay iconOverlay[] = {
    ScaleIconNone,
    ScaleIconEmblem,
    ScaleIconBig
};
#define N_ICON_TYPE (sizeof (iconOverlayString) / sizeof (iconOverlayString[0]))
#define SCALE_ICON_DEFAULT (iconOverlayString[1])

#define SCALE_HOVER_TIME_DEFAULT 750
#define SCALE_HOVER_TIME_MIN     50
#define SCALE_HOVER_TIME_MAX     10000

static int displayPrivateIndex;

typedef struct _ScaleSlot {
    int   x1, y1, x2, y2;
    int   filled;
    float scale;
} ScaleSlot;

#define SCALE_DISPLAY_OPTION_INITIATE        0
#define SCALE_DISPLAY_OPTION_INITIATE_ALL    1
#define SCALE_DISPLAY_OPTION_INITIATE_GROUP  2
#define SCALE_DISPLAY_OPTION_INITIATE_OUTPUT 3
#define SCALE_DISPLAY_OPTION_NUM             4

typedef struct _ScaleDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[SCALE_DISPLAY_OPTION_NUM];

    unsigned int lastActiveNum;
    Window       lastActiveWindow;
    Window       selectedWindow;
    KeyCode	 leftKeyCode, rightKeyCode, upKeyCode, downKeyCode;
} ScaleDisplay;

#define SCALE_SCREEN_OPTION_SPACING      0
#define SCALE_SCREEN_OPTION_SPEED	 1
#define SCALE_SCREEN_OPTION_TIMESTEP	 2
#define SCALE_SCREEN_OPTION_WINDOW_TYPE  3
#define SCALE_SCREEN_OPTION_DARKEN_BACK  4
#define SCALE_SCREEN_OPTION_OPACITY      5
#define SCALE_SCREEN_OPTION_ICON         6
#define SCALE_SCREEN_OPTION_HOVER_TIME   7
#define SCALE_SCREEN_OPTION_NUM          8

typedef enum {
    ScaleTypeNormal = 0,
    ScaleTypeOutput,
    ScaleTypeGroup,
    ScaleTypeAll
} ScaleType;

typedef struct _ScaleScreen {
    int windowPrivateIndex;

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintScreenProc        paintScreen;
    PaintWindowProc        paintWindow;
    DamageWindowRectProc   damageWindowRect;

    CompOption opt[SCALE_SCREEN_OPTION_NUM];

    int spacing;

    float speed;
    float timestep;

    unsigned int wMask;

    Bool grab;
    int  grabIndex;

    Window dndTarget;

    CompTimeoutHandle hoverHandle;

    int state;
    int moreAdjust;

    Cursor cursor;

    ScaleSlot *slots;
    int        slotsSize;
    int        nSlots;

    /* only used for sorting */
    CompWindow **windows;
    int        windowsSize;
    int        nWindows;

    Bool     darkenBack;
    GLushort opacity;

    IconOverlay iconOverlay;

    ScaleType type;

    Window clientLeader;
} ScaleScreen;

typedef struct _ScaleWindow {
    ScaleSlot *slot;

    int sid;
    int distance;

    GLfloat xVelocity, yVelocity, scaleVelocity;
    GLfloat scale;
    GLfloat tx, ty;
    float   delta;
    Bool    adjust;

    float lastThumbOpacity;
} ScaleWindow;


#define GET_SCALE_DISPLAY(d)				      \
    ((ScaleDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define SCALE_DISPLAY(d)		     \
    ScaleDisplay *sd = GET_SCALE_DISPLAY (d)

#define GET_SCALE_SCREEN(s, sd)					  \
    ((ScaleScreen *) (s)->privates[(sd)->screenPrivateIndex].ptr)

#define SCALE_SCREEN(s)							   \
    ScaleScreen *ss = GET_SCALE_SCREEN (s, GET_SCALE_DISPLAY (s->display))

#define GET_SCALE_WINDOW(w, ss)					  \
    ((ScaleWindow *) (w)->privates[(ss)->windowPrivateIndex].ptr)

#define SCALE_WINDOW(w)					       \
    ScaleWindow *sw = GET_SCALE_WINDOW  (w,		       \
		      GET_SCALE_SCREEN  (w->screen,	       \
		      GET_SCALE_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
scaleGetScreenOptions (CompScreen *screen,
		       int	  *count)
{
    SCALE_SCREEN (screen);

    *count = NUM_OPTIONS (ss);
    return ss->opt;
}

static Bool
scaleSetScreenOption (CompScreen      *screen,
		      char	      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    SCALE_SCREEN (screen);

    o = compFindOption (ss->opt, NUM_OPTIONS (ss), name, &index);

    if (!o)
	return FALSE;

    switch (index) {
    case SCALE_SCREEN_OPTION_SPACING:
	if (compSetIntOption (o, value))
	{
	    ss->spacing = o->value.i;
	    return TRUE;
	}
	break;
    case SCALE_SCREEN_OPTION_SPEED:
	if (compSetFloatOption (o, value))
	{
	    ss->speed = o->value.f;
	    return TRUE;
	}
	break;
    case SCALE_SCREEN_OPTION_TIMESTEP:
	if (compSetFloatOption (o, value))
	{
	    ss->timestep = o->value.f;
	    return TRUE;
	}
	break;
    case SCALE_SCREEN_OPTION_WINDOW_TYPE:
	if (compSetOptionList (o, value))
	{
	    ss->wMask = compWindowTypeMaskFromStringList (&o->value);
	    return TRUE;
	}
	break;
    case SCALE_SCREEN_OPTION_DARKEN_BACK:
	if (compSetBoolOption (o, value))
	{
	    ss->darkenBack = o->value.b;
	    return TRUE;
	}
	break;
    case SCALE_SCREEN_OPTION_OPACITY:
	if (compSetIntOption (o, value))
	{
	    ss->opacity = (OPAQUE * o->value.i) / 100;
	    return TRUE;
	}
	break;
    case SCALE_SCREEN_OPTION_ICON:
	if (compSetStringOption (o, value))
	{
	    int i;

	    for (i = 0; i < N_ICON_TYPE; i++)
	    {
		if (strcmp (o->value.s, iconOverlayString[i]) == 0)
		{
		    ss->iconOverlay = iconOverlay[i];
		    return TRUE;
		}
	    }
	}
	break;
    case SCALE_SCREEN_OPTION_HOVER_TIME:
	if (compSetIntOption (o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
scaleScreenInitOptions (ScaleScreen *ss)
{
    CompOption *o;
    int	       i;

    o = &ss->opt[SCALE_SCREEN_OPTION_SPACING];
    o->name	  = "spacing";
    o->shortDesc  = N_("Spacing");
    o->longDesc   = N_("Space between windows");
    o->type	  = CompOptionTypeInt;
    o->value.i	  = SCALE_SPACING_DEFAULT;
    o->rest.i.min = SCALE_SPACING_MIN;
    o->rest.i.max = SCALE_SPACING_MAX;

    o = &ss->opt[SCALE_SCREEN_OPTION_SPEED];
    o->name		= "speed";
    o->shortDesc	= N_("Speed");
    o->longDesc		= N_("Scale speed");
    o->type		= CompOptionTypeFloat;
    o->value.f		= SCALE_SPEED_DEFAULT;
    o->rest.f.min	= SCALE_SPEED_MIN;
    o->rest.f.max	= SCALE_SPEED_MAX;
    o->rest.f.precision = SCALE_SPEED_PRECISION;

    o = &ss->opt[SCALE_SCREEN_OPTION_TIMESTEP];
    o->name		= "timestep";
    o->shortDesc	= N_("Timestep");
    o->longDesc		= N_("Scale timestep");
    o->type		= CompOptionTypeFloat;
    o->value.f		= SCALE_TIMESTEP_DEFAULT;
    o->rest.f.min	= SCALE_TIMESTEP_MIN;
    o->rest.f.max	= SCALE_TIMESTEP_MAX;
    o->rest.f.precision = SCALE_TIMESTEP_PRECISION;

    o = &ss->opt[SCALE_SCREEN_OPTION_WINDOW_TYPE];
    o->name	         = "window_types";
    o->shortDesc         = N_("Window Types");
    o->longDesc	         = N_("Window types that should scaled in scale mode");
    o->type	         = CompOptionTypeList;
    o->value.list.type   = CompOptionTypeString;
    o->value.list.nValue = N_WIN_TYPE;
    o->value.list.value  = malloc (sizeof (CompOptionValue) * N_WIN_TYPE);
    for (i = 0; i < N_WIN_TYPE; i++)
	o->value.list.value[i].s = strdup (winType[i]);
    o->rest.s.string     = windowTypeString;
    o->rest.s.nString    = nWindowTypeString;

    ss->wMask = compWindowTypeMaskFromStringList (&o->value);

    o = &ss->opt[SCALE_SCREEN_OPTION_DARKEN_BACK];
    o->name      = "darken_back";
    o->shortDesc = N_("Darken Background");
    o->longDesc  = N_("Darken background when scaling windows");
    o->type      = CompOptionTypeBool;
    o->value.b   = SCALE_DARKEN_BACK_DEFAULT;

    o = &ss->opt[SCALE_SCREEN_OPTION_OPACITY];
    o->name	  = "opacity";
    o->shortDesc  = N_("Opacity");
    o->longDesc	  = N_("Amount of opacity in percent");
    o->type	  = CompOptionTypeInt;
    o->value.i    = SCALE_OPACITY_DEFAULT;
    o->rest.i.min = SCALE_OPACITY_MIN;
    o->rest.i.max = SCALE_OPACITY_MAX;

    o = &ss->opt[SCALE_SCREEN_OPTION_ICON];
    o->name	      = "overlay_icon";
    o->shortDesc      = N_("Overlay Icon");
    o->longDesc	      = N_("Overlay an icon on windows once they are scaled");
    o->type	      = CompOptionTypeString;
    o->value.s	      = strdup (SCALE_ICON_DEFAULT);
    o->rest.s.string  = iconOverlayString;
    o->rest.s.nString = N_ICON_TYPE;

    o = &ss->opt[SCALE_SCREEN_OPTION_HOVER_TIME];
    o->name	  = "hover_time";
    o->shortDesc  = N_("Hover Time");
    o->longDesc	  = N_("Time (in ms) before scale mode is terminated when "
		       "hovering over a window");
    o->type	  = CompOptionTypeInt;
    o->value.i    = SCALE_HOVER_TIME_DEFAULT;
    o->rest.i.min = SCALE_HOVER_TIME_MIN;
    o->rest.i.max = SCALE_HOVER_TIME_MAX;
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

    if (!ss->type)
    {
	if (!(*w->screen->focusWindow) (w))
	    return FALSE;
    }

    if (!(ss->wMask & w->type))
	return FALSE;

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

    return TRUE;
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
	Bool		  scaled = FALSE;

	SCALE_WINDOW (w);

	if (sw->adjust || sw->slot)
	{
	    SCALE_DISPLAY (s->display);

	    if (w->id	    != sd->selectedWindow &&
		ss->opacity != OPAQUE		  &&
		ss->state   != SCALE_STATE_IN)
	    {
		/* modify opacity of windows that are not active */
		sAttrib.opacity = (sAttrib.opacity * ss->opacity) >> 16;
	    }

	    scaled = TRUE;

	    mask |= PAINT_WINDOW_NO_CORE_INSTANCE_MASK;
	}
	else if (ss->state != SCALE_STATE_IN)
	{
	    if (ss->darkenBack)
	    {
		/* modify brightness of the other windows */
		sAttrib.brightness = sAttrib.brightness / 2;
	    }

	    /* hide windows on this output that are not in scale mode */
	    if (!isNeverScaleWin (w) &&
		outputDeviceForWindow (w) == s->currentOutputDev)
	    {
		sAttrib.opacity = 0;
	    }
	}

	UNWRAP (ss, s, paintWindow);
	status = (*s->paintWindow) (w, &sAttrib, transform, region, mask);
	WRAP (ss, s, paintWindow, scalePaintWindow);

	if (scaled)
	{
	    FragmentAttrib fragment;
	    CompTransform  wTransform = *transform;

	    initFragmentAttrib (&fragment, &w->lastPaint);

	    matrixTranslate (&wTransform, w->attrib.x, w->attrib.y, 0.0f);
	    matrixScale (&wTransform, sw->scale, sw->scale, 0.0f);
	    matrixTranslate (&wTransform,
			     sw->tx / sw->scale - w->attrib.x,
			     sw->ty / sw->scale - w->attrib.y,
			     0.0f);

	    glPushMatrix ();
	    glLoadMatrixf (wTransform.m);

	    (*s->drawWindow) (w, &wTransform, &fragment, region,
			      mask | PAINT_WINDOW_TRANSFORMED_MASK);

	    glPopMatrix ();
	}

	if ((ss->iconOverlay != ScaleIconNone) && scaled)
	{
	    CompIcon *icon;

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

		switch (ss->iconOverlay) {
		case ScaleIconNone:
		case ScaleIconEmblem:
		    scale = 1.0f;
		    break;
		case ScaleIconBig:
		default:
		    sAttrib.opacity /= 3;
		    scale = MIN (((float) scaledWinWidth / icon->width),
				 ((float) scaledWinHeight / icon->height));
		    break;
		}

		width  = icon->width  * scale;
		height = icon->height * scale;

		switch (ss->iconOverlay) {
		case ScaleIconNone:
		case ScaleIconEmblem:
		    x = w->attrib.x + scaledWinWidth - icon->width;
		    y = w->attrib.y + scaledWinHeight - icon->height;
		    break;
		case ScaleIconBig:
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

		mask |= PAINT_WINDOW_TRANSLUCENT_MASK;

		iconReg.rects    = &iconReg.extents;
		iconReg.numRects = 1;

		iconReg.extents.x1 = 0;
		iconReg.extents.y1 = 0;
		iconReg.extents.x2 = iconReg.extents.x1 + width;
		iconReg.extents.y2 = iconReg.extents.y1 + height;

		w->vCount = 0;
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
layoutSlots (CompScreen *s)
{
    int i, j, x, y, width, height, lines, n;
    int x1, y1, x2, y2;

    SCALE_SCREEN (s);

    ss->nSlots = 0;

    lines = sqrt (ss->nWindows + 1);

    getCurrentOutputExtents (s, &x1, &y1, &x2, &y2);

    if (s->workArea.x > x1)
	x1 = s->workArea.x;
    if (s->workArea.x + s->workArea.width < x2)
	x2 = s->workArea.x + s->workArea.width;
    if (s->workArea.y > y1)
	y1 = s->workArea.y;
    if (s->workArea.y + s->workArea.height < y2)
	y2 = s->workArea.y + s->workArea.height;

    y      = y1 + ss->spacing;
    height = ((y2 - y1) - (lines + 1) * ss->spacing) / lines;

    for (i = 0; i < lines; i++)
    {
	n = MIN (ss->nWindows - ss->nSlots,
		 ceilf ((float) ss->nWindows / lines));

	x     = x1 + ss->spacing;
	width = ((x2 - x1) - (n + 1) * ss->spacing) / n;

	for (j = 0; j < n; j++)
	{
	    ss->slots[ss->nSlots].x1 = x;
	    ss->slots[ss->nSlots].y1 = y;
	    ss->slots[ss->nSlots].x2 = x + width;
	    ss->slots[ss->nSlots].y2 = y + height;

	    ss->slots[ss->nSlots].filled = FALSE;

	    x += width + ss->spacing;

	    ss->nSlots++;
	}

	y += height + ss->spacing;
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
scalePaintScreen (CompScreen		  *s,
		  const ScreenPaintAttrib *sAttrib,
		  const CompTransform	  *transform,
		  Region		  region,
		  int			  output,
		  unsigned int		  mask)
{
    Bool status;

    SCALE_SCREEN (s);

    if (ss->state != SCALE_STATE_NONE)
	mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

    UNWRAP (ss, s, paintScreen);
    status = (*s->paintScreen) (s, sAttrib, transform, region, output, mask);
    WRAP (ss, s, paintScreen, scalePaintScreen);

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

	amount = msSinceLastPaint * 0.05f * ss->speed;
	steps  = amount / (0.5f * ss->timestep);
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
		ss->state = SCALE_STATE_NONE;
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
sendViewportMoveRequest (CompScreen *s,
			 int	    x,
			 int	    y)
{
    XEvent xev;

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = s->display->display;
    xev.xclient.format  = 32;

    xev.xclient.message_type = s->display->desktopViewportAtom;
    xev.xclient.window	     = s->root;

    xev.xclient.data.l[0] = x;
    xev.xclient.data.l[1] = y;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent (s->display->display,
		s->root,
		FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xev);
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
scaleTerminate (CompDisplay     *d,
		CompAction      *action,
		CompActionState state,
		CompOption      *option,
		int	        nOption)
{
    CompScreen *s;
    Window     xid;

    SCALE_DISPLAY (d);

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	SCALE_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (ss->grab)
	{
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

		if (ss->state != SCALE_STATE_IN)
		{
		    w = findWindowAtScreen (s, sd->lastActiveWindow);
		    if (w)
		    {
			int x, y;

			activateWindow (w);

			defaultViewportForWindow (w, &x, &y);

			if (x != s->x || y != s->y)
			    sendViewportMoveRequest (s,
						     x * s->width,
						     y * s->height);
		    }
		}

		ss->state = SCALE_STATE_IN;

		damageScreen (s);
	    }

	    sd->lastActiveNum = 0;
	}
    }

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
    SCALE_DISPLAY (s->display);
    SCALE_SCREEN (s);

    if (otherScreenGrabExist (s, "scale", 0))
	return FALSE;

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

	sd->lastActiveWindow = s->display->activeWindow;
	sd->selectedWindow   = s->display->activeWindow;

	ss->state = SCALE_STATE_OUT;

	damageScreen (s);
    }

    if (state & CompActionStateInitButton)
	action->state |= CompActionStateTermButton;

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

    w = scaleCheckForWindowAt (s, x, y);
    if (w && isScaleWin (w))
    {
	SCALE_DISPLAY (s->display);

	scaleSelectWindow (w);

	if (moveInputFocus)
	{
	    sd->lastActiveNum    = w->activeNum;
	    sd->lastActiveWindow = w->id;

	    moveInputFocusToWindow (w);
	}

	return TRUE;
    }

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

	scaleSelectWindow (focus);

	sd->lastActiveNum    = focus->activeNum;
	sd->lastActiveWindow = focus->id;

	moveInputFocusToWindow (focus);
    }
}

static void
scaleWindowRemove (CompDisplay *d,
		   Window      id)
{
    CompWindow *w;

    w = findWindowAtDisplay (d, id);
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
	CompAction *action =
	    &sd->opt[SCALE_DISPLAY_OPTION_INITIATE].value.action;

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

	scaleTerminate (s->display, action, 0, &o, 1);
    }

    ss->hoverHandle = 0;

    return FALSE;
}

static void
scaleHandleEvent (CompDisplay *d,
		  XEvent      *event)
{
    CompScreen *s;

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
		    scaleMoveFocusWindow (s, -1, 0);
		else if (event->xkey.keycode == sd->rightKeyCode)
		    scaleMoveFocusWindow (s, 1, 0);
		else if (event->xkey.keycode == sd->upKeyCode)
		    scaleMoveFocusWindow (s, 0, -1);
		else if (event->xkey.keycode == sd->downKeyCode)
		    scaleMoveFocusWindow (s, 0, 1);
	    }
	}
	break;
    case ButtonPress:
	if (event->xbutton.button == Button1)
	{
	    s = findScreenAtDisplay (d, event->xbutton.root);
	    if (s)
	    {
		CompAction *action =
		    &sd->opt[SCALE_DISPLAY_OPTION_INITIATE].value.action;

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
			scaleTerminate (d, action, 0, &o, 1);
		    }
		    else if (event->xbutton.x_root > s->workArea.x &&
			     event->xbutton.x_root < (s->workArea.x +
						      s->workArea.width) &&
			     event->xbutton.y_root > s->workArea.y &&
			     event->xbutton.y_root < (s->workArea.y +
						      s->workArea.height))
		    {
			scaleTerminate (d, action, 0, &o, 1);
			enterShowDesktopMode (s);
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
    case ClientMessage:
	if (event->xclient.message_type == d->xdndPositionAtom)
	{
	    CompWindow *w;

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
				compAddTimeout (time,
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
	    CompWindow *w;

	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		CompAction *action =
		    &sd->opt[SCALE_DISPLAY_OPTION_INITIATE].value.action;

		SCALE_SCREEN (w->screen);

		if (ss->grab			&&
		    ss->state != SCALE_STATE_IN &&
		    w->id == ss->dndTarget)
		{
		    CompOption o;

		    o.type    = CompOptionTypeInt;
		    o.name    = "root";
		    o.value.i = w->screen->root;

		    scaleTerminate (d, action, 0, &o, 1);
		}
	    }
	}
    default:
	break;
    }

    UNWRAP (sd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (sd, d, handleEvent, scaleHandleEvent);

    switch (event->type) {
    case UnmapNotify:
	scaleWindowRemove (d, event->xunmap.window);
	break;
    case DestroyNotify:
	scaleWindowRemove (d, event->xdestroywindow.window);
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
scaleGetDisplayOptions (CompDisplay *display,
			int	    *count)
{
    SCALE_DISPLAY (display);

    *count = NUM_OPTIONS (sd);
    return sd->opt;
}

static Bool
scaleSetDisplayOption (CompDisplay     *display,
		       char	       *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    SCALE_DISPLAY (display);

    o = compFindOption (sd->opt, NUM_OPTIONS (sd), name, &index);

    if (!o)
	return FALSE;

    switch (index) {
    case SCALE_DISPLAY_OPTION_INITIATE:
    case SCALE_DISPLAY_OPTION_INITIATE_ALL:
    case SCALE_DISPLAY_OPTION_INITIATE_GROUP:
    case SCALE_DISPLAY_OPTION_INITIATE_OUTPUT:
	if (setDisplayAction (display, o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
scaleDisplayInitOptions (ScaleDisplay *sd,
			 Display      *display)
{
    CompOption *o;

    o = &sd->opt[SCALE_DISPLAY_OPTION_INITIATE];
    o->name			  = "initiate";
    o->shortDesc		  = N_("Initiate Window Picker");
    o->longDesc			  = N_("Layout and start transforming windows");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = scaleInitiate;
    o->value.action.terminate	  = scaleTerminate;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = (1 << SCREEN_EDGE_TOPRIGHT);
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.key.modifiers = SCALE_INITIATE_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (SCALE_INITIATE_KEY_DEFAULT));

    o = &sd->opt[SCALE_DISPLAY_OPTION_INITIATE_ALL];
    o->name		      = "initiate_all";
    o->shortDesc	      = N_("Initiate Window Picker For All Windows");
    o->longDesc		      = N_("Layout and start transforming all windows");
    o->type		      = CompOptionTypeAction;
    o->value.action.initiate  = scaleInitiateAll;
    o->value.action.terminate = scaleTerminate;
    o->value.action.bell      = FALSE;
    o->value.action.edgeMask  = 0;
    o->value.action.state     = CompActionStateInitEdge;
    o->value.action.state    |= CompActionStateInitEdgeDnd;
    o->value.action.type      = 0;
    o->value.action.state    |= CompActionStateInitKey;
    o->value.action.state    |= CompActionStateInitButton;

    o = &sd->opt[SCALE_DISPLAY_OPTION_INITIATE_GROUP];
    o->name		      = "initiate_group";
    o->shortDesc	      = N_("Initiate Window Picker For Window Group");
    o->longDesc		      = N_("Layout and start transforming "
				   "window group");
    o->type		      = CompOptionTypeAction;
    o->value.action.initiate  = scaleInitiateGroup;
    o->value.action.terminate = scaleTerminate;
    o->value.action.bell      = FALSE;
    o->value.action.edgeMask  = 0;
    o->value.action.state     = CompActionStateInitEdge;
    o->value.action.state    |= CompActionStateInitEdgeDnd;
    o->value.action.type      = 0;
    o->value.action.state    |= CompActionStateInitKey;
    o->value.action.state    |= CompActionStateInitButton;

    o = &sd->opt[SCALE_DISPLAY_OPTION_INITIATE_OUTPUT];
    o->name		      = "initiate_output";
    o->shortDesc	      = N_("Initiate Window Picker For Windows on "
				   "Current Output");
    o->longDesc		      = N_("Layout and start transforming "
				   "windows on current output");
    o->type		      = CompOptionTypeAction;
    o->value.action.initiate  = scaleInitiateOutput;
    o->value.action.terminate = scaleTerminate;
    o->value.action.bell      = FALSE;
    o->value.action.edgeMask  = 0;
    o->value.action.state     = CompActionStateInitEdge;
    o->value.action.state    |= CompActionStateInitEdgeDnd;
    o->value.action.type      = 0;
    o->value.action.state    |= CompActionStateInitKey;
    o->value.action.state    |= CompActionStateInitButton;
}

static Bool
scaleInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    ScaleDisplay *sd;

    sd = malloc (sizeof (ScaleDisplay));
    if (!sd)
	return FALSE;

    sd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (sd->screenPrivateIndex < 0)
    {
	free (sd);
	return FALSE;
    }

    sd->lastActiveNum = None;
    sd->selectedWindow = None;

    scaleDisplayInitOptions (sd, d->display);

    sd->leftKeyCode  = XKeysymToKeycode (d->display, XStringToKeysym ("Left"));
    sd->rightKeyCode = XKeysymToKeycode (d->display, XStringToKeysym ("Right"));
    sd->upKeyCode    = XKeysymToKeycode (d->display, XStringToKeysym ("Up"));
    sd->downKeyCode  = XKeysymToKeycode (d->display, XStringToKeysym ("Down"));

    WRAP (sd, d, handleEvent, scaleHandleEvent);

    d->privates[displayPrivateIndex].ptr = sd;

    return TRUE;
}

static void
scaleFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    SCALE_DISPLAY (d);

    freeScreenPrivateIndex (d, sd->screenPrivateIndex);

    UNWRAP (sd, d, handleEvent);

    free (sd);
}

static Bool
scaleInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    ScaleScreen *ss;

    SCALE_DISPLAY (s->display);

    ss = malloc (sizeof (ScaleScreen));
    if (!ss)
	return FALSE;

    ss->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ss->windowPrivateIndex < 0)
    {
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

    ss->spacing = SCALE_SPACING_DEFAULT;

    ss->speed    = SCALE_SPEED_DEFAULT;
    ss->timestep = SCALE_TIMESTEP_DEFAULT;
    ss->opacity  = (OPAQUE * SCALE_OPACITY_DEFAULT) / 100;

    ss->darkenBack = SCALE_DARKEN_BACK_DEFAULT;

    ss->iconOverlay = ScaleIconEmblem;

    scaleScreenInitOptions (ss);

    addScreenAction (s, &sd->opt[SCALE_DISPLAY_OPTION_INITIATE].value.action);
    addScreenAction (s,
		     &sd->opt[SCALE_DISPLAY_OPTION_INITIATE_ALL].value.action);
    addScreenAction (s,
		     &sd->opt[SCALE_DISPLAY_OPTION_INITIATE_GROUP].value.action);
    addScreenAction (s,
		     &sd->opt[SCALE_DISPLAY_OPTION_INITIATE_OUTPUT].value.action);

    WRAP (ss, s, preparePaintScreen, scalePreparePaintScreen);
    WRAP (ss, s, donePaintScreen, scaleDonePaintScreen);
    WRAP (ss, s, paintScreen, scalePaintScreen);
    WRAP (ss, s, paintWindow, scalePaintWindow);
    WRAP (ss, s, damageWindowRect, scaleDamageWindowRect);

    ss->cursor = XCreateFontCursor (s->display->display, XC_left_ptr);

    s->privates[sd->screenPrivateIndex].ptr = ss;

    return TRUE;
}

static void
scaleFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    SCALE_SCREEN (s);

    UNWRAP (ss, s, preparePaintScreen);
    UNWRAP (ss, s, donePaintScreen);
    UNWRAP (ss, s, paintScreen);
    UNWRAP (ss, s, paintWindow);
    UNWRAP (ss, s, damageWindowRect);

    if (ss->slotsSize)
	free (ss->slots);

    if (ss->windowsSize)
	free (ss->windows);

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

    w->privates[ss->windowPrivateIndex].ptr = sw;

    return TRUE;
}

static void
scaleFiniWindow (CompPlugin *p,
		 CompWindow *w)
{
    SCALE_WINDOW (w);

    free (sw);
}

static Bool
scaleInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
scaleFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
scaleGetVersion (CompPlugin *plugin,
		 int	    version)
{
    return ABIVERSION;
}

CompPluginVTable scaleVTable = {
    "scale",
    N_("Scale"),
    N_("Scale windows"),
    scaleGetVersion,
    scaleInit,
    scaleFini,
    scaleInitDisplay,
    scaleFiniDisplay,
    scaleInitScreen,
    scaleFiniScreen,
    scaleInitWindow,
    scaleFiniWindow,
    scaleGetDisplayOptions,
    scaleSetDisplayOption,
    scaleGetScreenOptions,
    scaleSetScreenOption,
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &scaleVTable;
}
