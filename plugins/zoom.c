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

#include <compiz.h>

#define ZOOM_POINTER_INVERT_Y_DEFAULT FALSE

#define ZOOM_POINTER_SENSITIVITY_DEFAULT   1.0f
#define ZOOM_POINTER_SENSITIVITY_MIN       0.01f
#define ZOOM_POINTER_SENSITIVITY_MAX       100.0f
#define ZOOM_POINTER_SENSITIVITY_PRECISION 0.01f

#define ZOOM_POINTER_SENSITIVITY_FACTOR 0.001f

#define ZOOM_INITIATE_BUTTON_DEFAULT    Button3
#define ZOOM_INITIATE_MODIFIERS_DEFAULT (CompPressMask | CompSuperMask)

#define ZOOM_TERMINATE_BUTTON_DEFAULT    Button3
#define ZOOM_TERMINATE_MODIFIERS_DEFAULT CompReleaseMask

#define ZOOM_IN_BUTTON_DEFAULT    Button4
#define ZOOM_IN_MODIFIERS_DEFAULT (CompPressMask | CompSuperMask)

#define ZOOM_OUT_BUTTON_DEFAULT    Button5
#define ZOOM_OUT_MODIFIERS_DEFAULT (CompPressMask | CompSuperMask)

#define ZOOM_SPEED_DEFAULT   1.5f
#define ZOOM_SPEED_MIN       0.1f
#define ZOOM_SPEED_MAX       50.0f
#define ZOOM_SPEED_PRECISION 0.1f

#define ZOOM_TIMESTEP_DEFAULT   1.2f
#define ZOOM_TIMESTEP_MIN       0.1f
#define ZOOM_TIMESTEP_MAX       50.0f
#define ZOOM_TIMESTEP_PRECISION 0.1f

#define ZOOM_FILTER_LINEAR_DEFAULT FALSE

static int displayPrivateIndex;

typedef struct _ZoomDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
} ZoomDisplay;

#define ZOOM_SCREEN_OPTION_POINTER_INVERT_Y    0
#define ZOOM_SCREEN_OPTION_POINTER_SENSITIVITY 1
#define ZOOM_SCREEN_OPTION_INITIATE	       2
#define ZOOM_SCREEN_OPTION_TERMINATE	       3
#define ZOOM_SCREEN_OPTION_IN		       4
#define ZOOM_SCREEN_OPTION_OUT		       5
#define ZOOM_SCREEN_OPTION_SPEED	       6
#define ZOOM_SCREEN_OPTION_TIMESTEP	       7
#define ZOOM_SCREEN_OPTION_FILTER_LINEAR       8
#define ZOOM_SCREEN_OPTION_NUM		       9

typedef struct _ZoomScreen {
    PreparePaintScreenProc	 preparePaintScreen;
    DonePaintScreenProc		 donePaintScreen;
    PaintScreenProc		 paintScreen;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;

    CompOption opt[ZOOM_SCREEN_OPTION_NUM];

    Bool  pointerInvertY;
    float pointerSensitivity;

    float speed;
    float timestep;

    int grabIndex;

    GLfloat currentZoom;
    GLfloat newZoom;

    GLfloat xVelocity;
    GLfloat yVelocity;
    GLfloat zVelocity;

    GLfloat xTranslate;
    GLfloat yTranslate;

    GLfloat xtrans;
    GLfloat ytrans;
    GLfloat ztrans;

    int    prevPointerX;
    int    prevPointerY;
    XPoint savedPointer;
    Bool   grabbed;

    float maxTranslate;
} ZoomScreen;

#define GET_ZOOM_DISPLAY(d)				      \
    ((ZoomDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define ZOOM_DISPLAY(d)		           \
    ZoomDisplay *zd = GET_ZOOM_DISPLAY (d)

#define GET_ZOOM_SCREEN(s, zd)				         \
    ((ZoomScreen *) (s)->privates[(zd)->screenPrivateIndex].ptr)

#define ZOOM_SCREEN(s)						        \
    ZoomScreen *zs = GET_ZOOM_SCREEN (s, GET_ZOOM_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
zoomGetScreenOptions (CompScreen *screen,
		      int	 *count)
{
    ZOOM_SCREEN (screen);

    *count = NUM_OPTIONS (zs);
    return zs->opt;
}

static Bool
zoomSetScreenOption (CompScreen      *screen,
		     char	     *name,
		     CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    ZOOM_SCREEN (screen);

    o = compFindOption (zs->opt, NUM_OPTIONS (zs), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case ZOOM_SCREEN_OPTION_POINTER_INVERT_Y:
	if (compSetBoolOption (o, value))
	{
	    zs->pointerInvertY = o->value.b;
	    return TRUE;
	}
	break;
    case ZOOM_SCREEN_OPTION_POINTER_SENSITIVITY:
	if (compSetFloatOption (o, value))
	{
	    zs->pointerSensitivity = o->value.f *
		ZOOM_POINTER_SENSITIVITY_FACTOR;
	    return TRUE;
	}
	break;
    case ZOOM_SCREEN_OPTION_INITIATE:
    case ZOOM_SCREEN_OPTION_IN:
	if (addScreenBinding (screen, &value->bind))
	{
	    removeScreenBinding (screen, &o->value.bind);

	    if (compSetBindingOption (o, value))
		return TRUE;
	}
	break;
    case ZOOM_SCREEN_OPTION_TERMINATE:
    case ZOOM_SCREEN_OPTION_OUT:
	if (compSetBindingOption (o, value))
	    return TRUE;
	break;
    case ZOOM_SCREEN_OPTION_SPEED:
	if (compSetFloatOption (o, value))
	{
	    zs->speed = o->value.f;
	    return TRUE;
	}
	break;
    case ZOOM_SCREEN_OPTION_TIMESTEP:
	if (compSetFloatOption (o, value))
	{
	    zs->timestep = o->value.f;
	    return TRUE;
	}
	break;
    case ZOOM_SCREEN_OPTION_FILTER_LINEAR:
	if (compSetBoolOption (o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
zoomScreenInitOptions (ZoomScreen *zs,
		       Display    *display)
{
    CompOption *o;

    o = &zs->opt[ZOOM_SCREEN_OPTION_POINTER_INVERT_Y];
    o->name      = "invert_y";
    o->shortDesc = "Pointer Invert Y";
    o->longDesc  = "Invert Y axis for pointer movement";
    o->type      = CompOptionTypeBool;
    o->value.b   = ZOOM_POINTER_INVERT_Y_DEFAULT;

    o = &zs->opt[ZOOM_SCREEN_OPTION_POINTER_SENSITIVITY];
    o->name		= "sensitivity";
    o->shortDesc	= "Pointer Sensitivity";
    o->longDesc		= "Sensitivity of pointer movement";
    o->type		= CompOptionTypeFloat;
    o->value.f		= ZOOM_POINTER_SENSITIVITY_DEFAULT;
    o->rest.f.min	= ZOOM_POINTER_SENSITIVITY_MIN;
    o->rest.f.max	= ZOOM_POINTER_SENSITIVITY_MAX;
    o->rest.f.precision = ZOOM_POINTER_SENSITIVITY_PRECISION;

    o = &zs->opt[ZOOM_SCREEN_OPTION_INITIATE];
    o->name			     = "initiate";
    o->shortDesc		     = "Initiate";
    o->longDesc			     = "Zoom In";
    o->type			     = CompOptionTypeBinding;
    o->value.bind.type		     = CompBindingTypeButton;
    o->value.bind.u.button.modifiers = ZOOM_INITIATE_MODIFIERS_DEFAULT;
    o->value.bind.u.button.button    = ZOOM_INITIATE_BUTTON_DEFAULT;

    o = &zs->opt[ZOOM_SCREEN_OPTION_TERMINATE];
    o->name			     = "terminate";
    o->shortDesc		     = "Terminate";
    o->longDesc			     = "Zoom to Normal View";
    o->type			     = CompOptionTypeBinding;
    o->value.bind.type		     = CompBindingTypeButton;
    o->value.bind.u.button.modifiers = ZOOM_TERMINATE_MODIFIERS_DEFAULT;
    o->value.bind.u.button.button    = ZOOM_TERMINATE_BUTTON_DEFAULT;

    o = &zs->opt[ZOOM_SCREEN_OPTION_IN];
    o->name			     = "zoom_in";
    o->shortDesc		     = "Zoom In";
    o->longDesc			     = "Zoom In";
    o->type			     = CompOptionTypeBinding;
    o->value.bind.type		     = CompBindingTypeButton;
    o->value.bind.u.button.modifiers = ZOOM_IN_MODIFIERS_DEFAULT;
    o->value.bind.u.button.button    = ZOOM_IN_BUTTON_DEFAULT;

    o = &zs->opt[ZOOM_SCREEN_OPTION_OUT];
    o->name			     = "zoom_out";
    o->shortDesc		     = "Zoom Out";
    o->longDesc			     = "Zoom Out";
    o->type			     = CompOptionTypeBinding;
    o->value.bind.type		     = CompBindingTypeButton;
    o->value.bind.u.button.modifiers = ZOOM_OUT_MODIFIERS_DEFAULT;
    o->value.bind.u.button.button    = ZOOM_OUT_BUTTON_DEFAULT;

    o = &zs->opt[ZOOM_SCREEN_OPTION_SPEED];
    o->name		= "speed";
    o->shortDesc	= "Speed";
    o->longDesc		= "Zoom Speed";
    o->type		= CompOptionTypeFloat;
    o->value.f		= ZOOM_SPEED_DEFAULT;
    o->rest.f.min	= ZOOM_SPEED_MIN;
    o->rest.f.max	= ZOOM_SPEED_MAX;
    o->rest.f.precision = ZOOM_SPEED_PRECISION;

    o = &zs->opt[ZOOM_SCREEN_OPTION_TIMESTEP];
    o->name		= "timestep";
    o->shortDesc	= "Timestep";
    o->longDesc		= "Zoom Timestep";
    o->type		= CompOptionTypeFloat;
    o->value.f		= ZOOM_TIMESTEP_DEFAULT;
    o->rest.f.min	= ZOOM_TIMESTEP_MIN;
    o->rest.f.max	= ZOOM_TIMESTEP_MAX;
    o->rest.f.precision = ZOOM_TIMESTEP_PRECISION;

    o = &zs->opt[ZOOM_SCREEN_OPTION_FILTER_LINEAR];
    o->name	  = "filter_linear";
    o->shortDesc  = "Filter Linear";
    o->longDesc	  = "USe linear filter when zoomed in";
    o->type	  = CompOptionTypeBool;
    o->value.b    = ZOOM_FILTER_LINEAR_DEFAULT;
}

static int
adjustZoomVelocity (ZoomScreen *zs)
{
    float d, adjust, amount;

    d = (zs->newZoom - zs->currentZoom) * 75.0f;

    adjust = d * 0.002f;
    amount = fabs (d);
    if (amount < 1.0f)
	amount = 1.0f;
    else if (amount > 5.0f)
	amount = 5.0f;

    zs->zVelocity = (amount * zs->zVelocity + adjust) / (amount + 1.0f);

    return (fabs (d) < 0.1f && fabs (zs->zVelocity) < 0.005f);
}

static void
zoomPreparePaintScreen (CompScreen *s,
			int	   msSinceLastPaint)
{
    ZOOM_SCREEN (s);

    if (zs->grabIndex)
    {
	int   steps;
	float amount, chunk;

	amount = msSinceLastPaint * 0.05f * zs->speed;
	steps  = amount / (0.5f * zs->timestep);
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    zs->xVelocity /= 1.25f;
	    zs->yVelocity /= 1.25f;

	    if (fabs (zs->xVelocity) < 0.001f)
		zs->xVelocity = 0.0f;
	    if (fabs (zs->yVelocity) < 0.001f)
		zs->yVelocity = 0.0f;

	    zs->xTranslate += zs->xVelocity * chunk;
	    if (zs->xTranslate < -zs->maxTranslate)
	    {
		zs->xTranslate = -zs->maxTranslate;
		zs->xVelocity  = 0.0f;
	    }
	    else if (zs->xTranslate > zs->maxTranslate)
	    {
		zs->xTranslate = zs->maxTranslate;
		zs->xVelocity  = 0.0f;
	    }

	    zs->yTranslate += zs->yVelocity * chunk;
	    if (zs->yTranslate < -zs->maxTranslate)
	    {
		zs->yTranslate = -zs->maxTranslate;
		zs->yVelocity  = 0.0f;
	    }
	    else if (zs->yTranslate > zs->maxTranslate)
	    {
		zs->yTranslate = zs->maxTranslate;
		zs->yVelocity  = 0.0f;
	    }

	    if (adjustZoomVelocity (zs))
	    {
		zs->currentZoom = zs->newZoom;
		zs->zVelocity = 0.0f;
	    }
	    else
	    {
		zs->currentZoom += (zs->zVelocity * msSinceLastPaint) /
		    s->redrawTime;
	    }

	    zs->ztrans = DEFAULT_Z_CAMERA * zs->currentZoom;
	    if (zs->ztrans <= 0.1f)
	    {
		zs->zVelocity = 0.0f;
		zs->ztrans = 0.1f;
	    }

	    zs->xtrans = -zs->xTranslate * (1.0f - zs->currentZoom);
	    zs->ytrans = zs->yTranslate * (1.0f - zs->currentZoom);

	    if (!zs->grabbed)
	    {
		if (zs->currentZoom == 1.0f && zs->zVelocity == 0.0f)
		{
		    zs->xVelocity = zs->yVelocity = 0.0f;

		    removeScreenGrab (s, zs->grabIndex, &zs->savedPointer);
		    zs->grabIndex = FALSE;
		    break;
		}
	    }
	}
    }

    UNWRAP (zs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (zs, s, preparePaintScreen, zoomPreparePaintScreen);
}

static void
zoomDonePaintScreen (CompScreen *s)
{
    ZOOM_SCREEN (s);

    if (zs->grabIndex)
    {
	if (zs->currentZoom != zs->newZoom ||
	    zs->xVelocity || zs->yVelocity || zs->zVelocity)
	    damageScreen (s);
    }

    UNWRAP (zs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (zs, s, donePaintScreen, zoomDonePaintScreen);
}

static Bool
zoomPaintScreen (CompScreen		 *s,
		 const ScreenPaintAttrib *sAttrib,
		 Region		         region,
		 unsigned int		 mask)
{
    Bool status;

    ZOOM_SCREEN (s);

    if (zs->grabIndex)
    {
	ScreenPaintAttrib sa = *sAttrib;
	int		  saveFilter;

	sa.xTranslate += zs->xtrans;
	sa.yTranslate += zs->ytrans;

	sa.zCamera = -zs->ztrans;

	/* hack to get sides rendered correctly */
	if (zs->xtrans > 0.0f)
	    sa.xRotate += 0.000001f;
	else
	    sa.xRotate -= 0.000001f;

	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_TRANSFORMED_MASK;

	saveFilter = s->filter[SCREEN_TRANS_FILTER];

	if (zs->opt[ZOOM_SCREEN_OPTION_FILTER_LINEAR].value.b ||
	    zs->zVelocity != 0.0f)
	    s->filter[SCREEN_TRANS_FILTER] = COMP_TEXTURE_FILTER_GOOD;
	else
	    s->filter[SCREEN_TRANS_FILTER] = COMP_TEXTURE_FILTER_FAST;

	UNWRAP (zs, s, paintScreen);
	status = (*s->paintScreen) (s, &sa, region, mask);
	WRAP (zs, s, paintScreen, zoomPaintScreen);

	s->filter[SCREEN_TRANS_FILTER] = saveFilter;
    }
    else
    {
	UNWRAP (zs, s, paintScreen);
	status = (*s->paintScreen) (s, sAttrib, region, mask);
	WRAP (zs, s, paintScreen, zoomPaintScreen);
    }

    return status;
}

static void
zoomIn (CompScreen *s,
	int	   x,
	int	   y)
{
    ZOOM_SCREEN (s);

    /* some other plugin has already grabbed the screen */
    if (s->maxGrab - zs->grabIndex)
	return;

    zs->prevPointerX = x;
    zs->prevPointerY = y;

    if (!zs->grabIndex)
    {
	zs->grabIndex = pushScreenGrab (s, s->invisibleCursor);

	zs->savedPointer.x = zs->prevPointerX;
	zs->savedPointer.y = zs->prevPointerY;
    }

    if (zs->grabIndex)
    {
	zs->grabbed = TRUE;

	if (zs->newZoom / 2.0f * DEFAULT_Z_CAMERA >= 0.1f)
	{
	    zs->newZoom /= 2.0f;

	    damageScreen (s);

	    if (zs->currentZoom == 1.0f)
	    {
		zs->xTranslate = (x - s->width / 2) / (s->width * 2.0f);
		zs->yTranslate = (y - s->height / 2) / (s->height * 2.0f);

		zs->xTranslate /= zs->newZoom;
		zs->yTranslate /= zs->newZoom;
	    }
	}
    }
}

static void
zoomOut (CompScreen *s)
{
    ZOOM_SCREEN (s);

    if (zs->grabIndex)
    {
	zs->newZoom *= 2.0f;
	if (zs->newZoom > DEFAULT_Z_CAMERA - (DEFAULT_Z_CAMERA / 10.0f))
	{
	    zs->grabbed = FALSE;
	    zs->newZoom = 1.0f;
	}

	damageScreen (s);
    }
}

static void
zoomTerminate (CompScreen *s)
{
    ZOOM_SCREEN (s);

    if (zs->grabIndex)
    {
	zs->newZoom = 1.0f;
	zs->grabbed = FALSE;

	damageScreen (s);
    }
}

static void
zoomHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    CompScreen *s;

    ZOOM_DISPLAY (d);

    switch (event->type) {
    case KeyPress:
    case KeyRelease:
	s = findScreenAtDisplay (d, event->xkey.root);
	if (s)
	{
	    ZOOM_SCREEN (s);

	    if (EV_KEY (&zs->opt[ZOOM_SCREEN_OPTION_INITIATE], event) ||
		EV_KEY (&zs->opt[ZOOM_SCREEN_OPTION_IN], event))
		zoomIn (s,
			event->xkey.x_root,
			event->xkey.y_root);

	    if (EV_KEY (&zs->opt[ZOOM_SCREEN_OPTION_OUT], event))
		zoomOut (s);

	    if (EV_KEY (&zs->opt[ZOOM_SCREEN_OPTION_TERMINATE], event) ||
		(event->type	     == KeyPress &&
		 event->xkey.keycode == s->escapeKeyCode))
		zoomTerminate (s);
	}
	break;
    case ButtonPress:
    case ButtonRelease:
	s = findScreenAtDisplay (d, event->xbutton.root);
	if (s)
	{
	    ZOOM_SCREEN (s);

	    if (EV_BUTTON (&zs->opt[ZOOM_SCREEN_OPTION_INITIATE], event) ||
		EV_BUTTON (&zs->opt[ZOOM_SCREEN_OPTION_IN], event))
		zoomIn (s,
			event->xbutton.x_root,
			event->xbutton.y_root);

	    if (EV_BUTTON (&zs->opt[ZOOM_SCREEN_OPTION_OUT], event))
		zoomOut (s);

	    if (EV_BUTTON (&zs->opt[ZOOM_SCREEN_OPTION_TERMINATE], event))
		zoomTerminate (s);
	}
	break;
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	{
	    ZOOM_SCREEN (s);

	    if (zs->grabIndex && zs->grabbed)
	    {
		GLfloat pointerDx;
		GLfloat pointerDy;

		pointerDx = event->xmotion.x_root - zs->prevPointerX;
		pointerDy = event->xmotion.y_root - zs->prevPointerY;
		zs->prevPointerX = event->xmotion.x_root;
		zs->prevPointerY = event->xmotion.y_root;

		if (event->xmotion.x_root < 50	           ||
		    event->xmotion.y_root < 50	           ||
		    event->xmotion.x_root > s->width  - 50 ||
		    event->xmotion.y_root > s->height - 50)
		{
		    XEvent ev;

		    zs->prevPointerX = s->width / 2;
		    zs->prevPointerY = s->height / 2;

		    XSync (d->display, FALSE);
		    while (XCheckTypedEvent (d->display, MotionNotify, &ev));

		    XWarpPointer (d->display, None, s->root, 0, 0, 0, 0,
				  zs->prevPointerX, zs->prevPointerY);
		}

		if (zs->pointerInvertY)
		    pointerDy = -pointerDy;

		zs->xVelocity += pointerDx * zs->pointerSensitivity;
		zs->yVelocity += pointerDy * zs->pointerSensitivity;

		damageScreen (s);
	    }
	}
    default:
	break;
    }

    UNWRAP (zd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (zd, d, handleEvent, zoomHandleEvent);
}

static void
zoomUpdateCubeOptions (CompScreen *s)
{
    CompPlugin *p;

    ZOOM_SCREEN (s);

    p = findActivePlugin ("cube");
    if (p && p->vTable->getScreenOptions)
    {
	CompOption *options, *option;
	int	   nOptions;

	options = (*p->vTable->getScreenOptions) (s, &nOptions);
	option = compFindOption (options, nOptions, "in", 0);
	if (option)
	    zs->maxTranslate = option->value.b ? 0.85f : 1.5f;
    }
}

static Bool
zoomSetScreenOptionForPlugin (CompScreen      *s,
			      char	      *plugin,
			      char	      *name,
			      CompOptionValue *value)
{
    Bool status;

    ZOOM_SCREEN (s);

    UNWRAP (zs, s, setScreenOptionForPlugin);
    status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
    WRAP (zs, s, setScreenOptionForPlugin, zoomSetScreenOptionForPlugin);

    if (status && strcmp (plugin, "cube") == 0)
	zoomUpdateCubeOptions (s);

    return status;
}

static Bool
zoomInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    ZoomDisplay *zd;

    zd = malloc (sizeof (ZoomDisplay));
    if (!zd)
	return FALSE;

    zd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (zd->screenPrivateIndex < 0)
    {
	free (zd);
	return FALSE;
    }

    WRAP (zd, d, handleEvent, zoomHandleEvent);

    d->privates[displayPrivateIndex].ptr = zd;

    return TRUE;
}

static void
zoomFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    ZOOM_DISPLAY (d);

    freeScreenPrivateIndex (d, zd->screenPrivateIndex);

    UNWRAP (zd, d, handleEvent);

    free (zd);
}

static Bool
zoomInitScreen (CompPlugin *p,
		CompScreen *s)
{
    ZoomScreen *zs;

    ZOOM_DISPLAY (s->display);

    zs = malloc (sizeof (ZoomScreen));
    if (!zs)
	return FALSE;

    zs->grabIndex = 0;

    zs->currentZoom = 1.0f;
    zs->newZoom = 1.0f;

    zs->xVelocity = 0.0f;
    zs->yVelocity = 0.0f;
    zs->zVelocity = 0.0f;

    zs->xTranslate = 0.0f;
    zs->yTranslate = 0.0f;

    zs->maxTranslate = 0.0f;

    zs->savedPointer.x = 0;
    zs->savedPointer.y = 0;
    zs->prevPointerX = 0;
    zs->prevPointerY = 0;

    zs->grabbed = FALSE;

    zs->pointerInvertY     = ZOOM_POINTER_INVERT_Y_DEFAULT;
    zs->pointerSensitivity = ZOOM_POINTER_SENSITIVITY_DEFAULT *
	ZOOM_POINTER_SENSITIVITY_FACTOR;

    zs->speed    = ZOOM_SPEED_DEFAULT;
    zs->timestep = ZOOM_TIMESTEP_DEFAULT;

    zoomScreenInitOptions (zs, s->display->display);

    addScreenBinding (s, &zs->opt[ZOOM_SCREEN_OPTION_INITIATE].value.bind);
    addScreenBinding (s, &zs->opt[ZOOM_SCREEN_OPTION_IN].value.bind);

    WRAP (zs, s, preparePaintScreen, zoomPreparePaintScreen);
    WRAP (zs, s, donePaintScreen, zoomDonePaintScreen);
    WRAP (zs, s, paintScreen, zoomPaintScreen);
    WRAP (zs, s, setScreenOptionForPlugin, zoomSetScreenOptionForPlugin);

    s->privates[zd->screenPrivateIndex].ptr = zs;

    zoomUpdateCubeOptions (s);

    return TRUE;
}

static void
zoomFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    ZOOM_SCREEN (s);

    UNWRAP (zs, s, preparePaintScreen);
    UNWRAP (zs, s, donePaintScreen);
    UNWRAP (zs, s, paintScreen);
    UNWRAP (zs, s, setScreenOptionForPlugin);

    free (zs);
}

static Bool
zoomInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
zoomFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginDep zoomDeps[] = {
    { CompPluginRuleAfter, "cube" }
};

CompPluginVTable zoomVTable = {
    "zoom",
    "Zoom Desktop",
    "Zoom and pan desktop cube",
    zoomInit,
    zoomFini,
    zoomInitDisplay,
    zoomFiniDisplay,
    zoomInitScreen,
    zoomFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    zoomGetScreenOptions,
    zoomSetScreenOption,
    zoomDeps,
    sizeof (zoomDeps) / sizeof (zoomDeps[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &zoomVTable;
}
