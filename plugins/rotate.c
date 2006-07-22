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

#define _GNU_SOURCE /* for asprintf */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <X11/Xatom.h>
#include <X11/Xproto.h>

#include <compiz.h>

#define ROTATE_POINTER_INVERT_Y_DEFAULT FALSE

#define ROTATE_POINTER_SENSITIVITY_DEFAULT   1.0f
#define ROTATE_POINTER_SENSITIVITY_MIN       0.01f
#define ROTATE_POINTER_SENSITIVITY_MAX       100.0f
#define ROTATE_POINTER_SENSITIVITY_PRECISION 0.01f

#define ROTATE_POINTER_SENSITIVITY_FACTOR 0.05f

#define ROTATE_ACCELERATION_DEFAULT   4.0f
#define ROTATE_ACCELERATION_MIN       1.0f
#define ROTATE_ACCELERATION_MAX       20.0f
#define ROTATE_ACCELERATION_PRECISION 0.1f

#define ROTATE_INITIATE_BUTTON_DEFAULT    Button1
#define ROTATE_INITIATE_MODIFIERS_DEFAULT (ControlMask | CompAltMask)

#define ROTATE_LEFT_KEY_DEFAULT       "Left"
#define ROTATE_LEFT_MODIFIERS_DEFAULT (ControlMask | CompAltMask)

#define ROTATE_RIGHT_KEY_DEFAULT       "Right"
#define ROTATE_RIGHT_MODIFIERS_DEFAULT (ControlMask | CompAltMask)

#define ROTATE_LEFT_WINDOW_KEY_DEFAULT       "Left"
#define ROTATE_LEFT_WINDOW_MODIFIERS_DEFAULT \
    (ShiftMask | ControlMask | CompAltMask)

#define ROTATE_RIGHT_WINDOW_KEY_DEFAULT       "Right"
#define ROTATE_RIGHT_WINDOW_MODIFIERS_DEFAULT \
    (ShiftMask | ControlMask | CompAltMask)

#define ROTATE_SNAP_TOP_DEFAULT FALSE

#define ROTATE_SPEED_DEFAULT   1.5f
#define ROTATE_SPEED_MIN       0.1f
#define ROTATE_SPEED_MAX       50.0f
#define ROTATE_SPEED_PRECISION 0.1f

#define ROTATE_TIMESTEP_DEFAULT   1.2f
#define ROTATE_TIMESTEP_MIN       0.1f
#define ROTATE_TIMESTEP_MAX       50.0f
#define ROTATE_TIMESTEP_PRECISION 0.1f

#define ROTATE_EDGEFLIP_POINTER_DEFAULT FALSE
#define ROTATE_EDGEFLIP_WINDOW_DEFAULT  TRUE
#define ROTATE_EDGEFLIP_DND_DEFAULT     TRUE

#define ROTATE_FLIPTIME_DEFAULT 350
#define ROTATE_FLIPTIME_MIN     0
#define ROTATE_FLIPTIME_MAX     1000

static int displayPrivateIndex;

#define ROTATE_DISPLAY_OPTION_INITIATE		  0
#define ROTATE_DISPLAY_OPTION_LEFT		  1
#define ROTATE_DISPLAY_OPTION_RIGHT		  2
#define ROTATE_DISPLAY_OPTION_LEFT_WINDOW	  3
#define ROTATE_DISPLAY_OPTION_RIGHT_WINDOW	  4
#define ROTATE_DISPLAY_OPTION_EDGEFLIP_POINTER	  5
#define ROTATE_DISPLAY_OPTION_EDGEFLIP_WINDOW	  6
#define ROTATE_DISPLAY_OPTION_EDGEFLIP_DND	  7
#define ROTATE_DISPLAY_OPTION_FLIPTIME		  8
#define ROTATE_DISPLAY_OPTION_TO_1		  9
#define ROTATE_DISPLAY_OPTION_TO_2		  10
#define ROTATE_DISPLAY_OPTION_TO_3		  11
#define ROTATE_DISPLAY_OPTION_TO_4		  12
#define ROTATE_DISPLAY_OPTION_TO_5		  13
#define ROTATE_DISPLAY_OPTION_TO_6		  14
#define ROTATE_DISPLAY_OPTION_TO_7		  15
#define ROTATE_DISPLAY_OPTION_TO_8		  16
#define ROTATE_DISPLAY_OPTION_TO_9		  17
#define ROTATE_DISPLAY_OPTION_TO_10		  18
#define ROTATE_DISPLAY_OPTION_TO_11		  19
#define ROTATE_DISPLAY_OPTION_TO_12		  20
#define ROTATE_DISPLAY_OPTION_TO_1_WINDOW	  21
#define ROTATE_DISPLAY_OPTION_TO_2_WINDOW	  22
#define ROTATE_DISPLAY_OPTION_TO_3_WINDOW	  23
#define ROTATE_DISPLAY_OPTION_TO_4_WINDOW	  24
#define ROTATE_DISPLAY_OPTION_TO_5_WINDOW	  25
#define ROTATE_DISPLAY_OPTION_TO_6_WINDOW	  26
#define ROTATE_DISPLAY_OPTION_TO_7_WINDOW	  27
#define ROTATE_DISPLAY_OPTION_TO_8_WINDOW	  28
#define ROTATE_DISPLAY_OPTION_TO_9_WINDOW	  29
#define ROTATE_DISPLAY_OPTION_TO_10_WINDOW	  30
#define ROTATE_DISPLAY_OPTION_TO_11_WINDOW	  31
#define ROTATE_DISPLAY_OPTION_TO_12_WINDOW	  32
#define ROTATE_DISPLAY_OPTION_TO		  33
#define ROTATE_DISPLAY_OPTION_WINDOW		  34
#define ROTATE_DISPLAY_OPTION_FLIP_LEFT		  35
#define ROTATE_DISPLAY_OPTION_FLIP_RIGHT	  36
#define ROTATE_DISPLAY_OPTION_NUM		  37

typedef struct _RotateDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[ROTATE_DISPLAY_OPTION_NUM];

    int	flipTime;
} RotateDisplay;

#define ROTATE_SCREEN_OPTION_POINTER_INVERT_Y	 0
#define ROTATE_SCREEN_OPTION_POINTER_SENSITIVITY 1
#define ROTATE_SCREEN_OPTION_ACCELERATION        2
#define ROTATE_SCREEN_OPTION_SNAP_TOP		 3
#define ROTATE_SCREEN_OPTION_SPEED		 4
#define ROTATE_SCREEN_OPTION_TIMESTEP		 5
#define ROTATE_SCREEN_OPTION_NUM		 6

typedef struct _RotateScreen {
    PreparePaintScreenProc	 preparePaintScreen;
    DonePaintScreenProc		 donePaintScreen;
    PaintScreenProc		 paintScreen;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;
    WindowGrabNotifyProc	 windowGrabNotify;
    WindowUngrabNotifyProc	 windowUngrabNotify;

    CompOption opt[ROTATE_SCREEN_OPTION_NUM];

    Bool  pointerInvertY;
    float pointerSensitivity;
    Bool  snapTop;
    float acceleration;

    float speed;
    float timestep;

    int grabIndex;

    GLfloat xrot, xVelocity;
    GLfloat yrot, yVelocity;

    GLfloat baseXrot;

    Bool    moving;
    GLfloat moveTo;

    int invert;

    Window moveWindow;
    int    moveWindowX;

    XPoint savedPointer;
    Bool   grabbed;

    CompTimeoutHandle rotateHandle;
    Bool	      slow;
    unsigned int      grabMask;
    CompWindow	      *grabWindow;
} RotateScreen;

#define GET_ROTATE_DISPLAY(d)				       \
    ((RotateDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define ROTATE_DISPLAY(d)		       \
    RotateDisplay *rd = GET_ROTATE_DISPLAY (d)

#define GET_ROTATE_SCREEN(s, rd)				   \
    ((RotateScreen *) (s)->privates[(rd)->screenPrivateIndex].ptr)

#define ROTATE_SCREEN(s)						      \
    RotateScreen *rs = GET_ROTATE_SCREEN (s, GET_ROTATE_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
rotateGetScreenOptions (CompScreen *screen,
			int	   *count)
{
    ROTATE_SCREEN (screen);

    *count = NUM_OPTIONS (rs);
    return rs->opt;
}

static Bool
rotateSetScreenOption (CompScreen      *screen,
		       char	       *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    ROTATE_SCREEN (screen);

    o = compFindOption (rs->opt, NUM_OPTIONS (rs), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case ROTATE_SCREEN_OPTION_POINTER_INVERT_Y:
	if (compSetBoolOption (o, value))
	{
	    rs->pointerInvertY = o->value.b;
	    return TRUE;
	}
	break;
    case ROTATE_SCREEN_OPTION_POINTER_SENSITIVITY:
	if (compSetFloatOption (o, value))
	{
	    rs->pointerSensitivity = o->value.f *
		ROTATE_POINTER_SENSITIVITY_FACTOR;
	    return TRUE;
	}
	break;
    case ROTATE_SCREEN_OPTION_ACCELERATION:
	if (compSetFloatOption (o, value))
	{
	    rs->acceleration = o->value.f;
	    return TRUE;
	}
	break;
    case ROTATE_SCREEN_OPTION_SNAP_TOP:
	if (compSetBoolOption (o, value))
	{
	    rs->snapTop = o->value.b;
	    return TRUE;
	}
	break;
    case ROTATE_SCREEN_OPTION_SPEED:
	if (compSetFloatOption (o, value))
	{
	    rs->speed = o->value.f;
	    return TRUE;
	}
	break;
    case ROTATE_SCREEN_OPTION_TIMESTEP:
	if (compSetFloatOption (o, value))
	{
	    rs->timestep = o->value.f;
	    return TRUE;
	}
    default:
	break;
    }

    return FALSE;
}

static void
rotateScreenInitOptions (RotateScreen *rs)
{
    CompOption *o;

    o = &rs->opt[ROTATE_SCREEN_OPTION_POINTER_INVERT_Y];
    o->name      = "invert_y";
    o->shortDesc = N_("Pointer Invert Y");
    o->longDesc  = N_("Invert Y axis for pointer movement");
    o->type      = CompOptionTypeBool;
    o->value.b   = ROTATE_POINTER_INVERT_Y_DEFAULT;

    o = &rs->opt[ROTATE_SCREEN_OPTION_POINTER_SENSITIVITY];
    o->name		= "sensitivity";
    o->shortDesc	= N_("Pointer Sensitivity");
    o->longDesc		= N_("Sensitivity of pointer movement");
    o->type		= CompOptionTypeFloat;
    o->value.f		= ROTATE_POINTER_SENSITIVITY_DEFAULT;
    o->rest.f.min	= ROTATE_POINTER_SENSITIVITY_MIN;
    o->rest.f.max	= ROTATE_POINTER_SENSITIVITY_MAX;
    o->rest.f.precision = ROTATE_POINTER_SENSITIVITY_PRECISION;

    o = &rs->opt[ROTATE_SCREEN_OPTION_ACCELERATION];
    o->name		= "acceleration";
    o->shortDesc	= N_("Acceleration");
    o->longDesc		= N_("Rotation Acceleration");
    o->type		= CompOptionTypeFloat;
    o->value.f		= ROTATE_ACCELERATION_DEFAULT;
    o->rest.f.min	= ROTATE_ACCELERATION_MIN;
    o->rest.f.max	= ROTATE_ACCELERATION_MAX;
    o->rest.f.precision = ROTATE_ACCELERATION_PRECISION;

    o = &rs->opt[ROTATE_SCREEN_OPTION_SNAP_TOP];
    o->name      = "snap_top";
    o->shortDesc = N_("Snap To Top Face");
    o->longDesc  = N_("Snap Cube Rotation to Top Face");
    o->type      = CompOptionTypeBool;
    o->value.b   = ROTATE_SNAP_TOP_DEFAULT;

    o = &rs->opt[ROTATE_SCREEN_OPTION_SPEED];
    o->name		= "speed";
    o->shortDesc	= N_("Speed");
    o->longDesc		= N_("Rotation Speed");
    o->type		= CompOptionTypeFloat;
    o->value.f		= ROTATE_SPEED_DEFAULT;
    o->rest.f.min	= ROTATE_SPEED_MIN;
    o->rest.f.max	= ROTATE_SPEED_MAX;
    o->rest.f.precision = ROTATE_SPEED_PRECISION;

    o = &rs->opt[ROTATE_SCREEN_OPTION_TIMESTEP];
    o->name		= "timestep";
    o->shortDesc	= N_("Timestep");
    o->longDesc		= N_("Rotation Timestep");
    o->type		= CompOptionTypeFloat;
    o->value.f		= ROTATE_TIMESTEP_DEFAULT;
    o->rest.f.min	= ROTATE_TIMESTEP_MIN;
    o->rest.f.max	= ROTATE_TIMESTEP_MAX;
    o->rest.f.precision = ROTATE_TIMESTEP_PRECISION;
}

static int
adjustVelocity (RotateScreen *rs,
		int	     size)
{
    float xrot, yrot, adjust, amount;

    if (rs->moving)
    {
	xrot = rs->moveTo + (rs->xrot + rs->baseXrot);
    }
    else
    {
	xrot = rs->xrot;
	if (rs->xrot < -180.0f / size)
	    xrot = 360.0f / size + rs->xrot;
	else if (rs->xrot > 180.0f / size)
	    xrot = rs->xrot - 360.0f / size;
    }

    adjust = -xrot * 0.05f * rs->acceleration;
    amount = fabs (xrot);
    if (amount < 10.0f)
	amount = 10.0f;
    else if (amount > 30.0f)
	amount = 30.0f;

    if (rs->slow)
	adjust *= 0.05f;

    rs->xVelocity = (amount * rs->xVelocity + adjust) / (amount + 2.0f);

    if (rs->snapTop && rs->yrot > 50.0f)
	yrot = -(90.f - rs->yrot);
    else
	yrot = rs->yrot;

    adjust = -yrot * 0.05f * rs->acceleration;
    amount = fabs (rs->yrot);
    if (amount < 10.0f)
	amount = 10.0f;
    else if (amount > 30.0f)
	amount = 30.0f;

    rs->yVelocity = (amount * rs->yVelocity + adjust) / (amount + 2.0f);

    return (fabs (xrot) < 0.1f && fabs (rs->xVelocity) < 0.2f &&
	    fabs (yrot) < 0.1f && fabs (rs->yVelocity) < 0.2f);
}

static void
rotatePreparePaintScreen (CompScreen *s,
			  int	     msSinceLastPaint)
{
    ROTATE_SCREEN (s);

    if (rs->grabIndex || rs->moving)
    {
	int   steps;
	float amount, chunk;

	amount = msSinceLastPaint * 0.05f * rs->speed;
	steps  = amount / (0.5f * rs->timestep);
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    rs->xrot += rs->xVelocity * chunk;
	    rs->yrot += rs->yVelocity * chunk;

	    if (rs->xrot > 360.0f / s->size)
	    {
		rs->baseXrot += 360.0f / s->size;
		rs->xrot -= 360.0f / s->size;
	    }
	    else if (rs->xrot < 0.0f)
	    {
		rs->baseXrot -= 360.0f / s->size;
		rs->xrot += 360.0f / s->size;
	    }

	    if (rs->invert == -1)
	    {
		if (rs->yrot > 45.0f)
		{
		    rs->yVelocity = 0.0f;
		    rs->yrot = 45.0f;
		}
		else if (rs->yrot < -45.0f)
		{
		    rs->yVelocity = 0.0f;
		    rs->yrot = -45.0f;
		}
	    }
	    else
	    {
		if (rs->yrot > 100.0f)
		{
		    rs->yVelocity = 0.0f;
		    rs->yrot = 100.0f;
		}
		else if (rs->yrot < -100.0f)
		{
		    rs->yVelocity = 0.0f;
		    rs->yrot = -100.0f;
		}
	    }

	    if (rs->grabbed)
	    {
		rs->xVelocity /= 1.25f;
		rs->yVelocity /= 1.25f;

		if (fabs (rs->xVelocity) < 0.01f)
		    rs->xVelocity = 0.0f;
		if (fabs (rs->yVelocity) < 0.01f)
		    rs->yVelocity = 0.0f;
	    }
	    else if (adjustVelocity (rs, s->size))
	    {
		rs->xVelocity = 0.0f;
		rs->yVelocity = 0.0f;

		if (fabs (rs->yrot) < 0.1f)
		{
		    float xrot;
		    int   tx;

		    xrot = rs->baseXrot + rs->xrot;
		    if (xrot < 0.0f)
			tx = (s->size * xrot / 360.0f) - 0.5f;
		    else
			tx = (s->size * xrot / 360.0f) + 0.5f;

		    moveScreenViewport (s, tx, TRUE);

		    rs->xrot = 0.0f;
		    rs->yrot = 0.0f;
		    rs->baseXrot = rs->moveTo = 0.0f;
		    rs->moving = FALSE;

		    if (rs->grabIndex)
		    {
			removeScreenGrab (s, rs->grabIndex, &rs->savedPointer);
			rs->grabIndex = 0;
		    }

		    if (rs->moveWindow)
		    {
			CompWindow *w;

			w = findWindowAtScreen (s, rs->moveWindow);
			if (w)
			{
			    moveWindow (w, w->attrib.x - rs->moveWindowX, 0,
					TRUE, TRUE);
			    syncWindowPosition (w);
			}
		    }
		    else
			focusDefaultWindow (s->display);

		    rs->moveWindow = 0;
		}
		break;
	    }
	}

	if (rs->moveWindow)
	{
	    CompWindow *w;

	    w = findWindowAtScreen (s, rs->moveWindow);
	    if (w)
	    {
		float xrot = (s->size * (rs->baseXrot + rs->xrot)) / 360.0f;

		moveWindowToViewportPosition (w,
					      rs->moveWindowX - xrot * s->width,
					      FALSE);
	    }
	}
    }

    UNWRAP (rs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (rs, s, preparePaintScreen, rotatePreparePaintScreen);
}

static void
rotateDonePaintScreen (CompScreen *s)
{
    ROTATE_SCREEN (s);

    if (rs->grabIndex || rs->moving)
    {
	if ((!rs->grabbed && !rs->snapTop) || rs->xVelocity || rs->yVelocity)
	    damageScreen (s);
    }

    UNWRAP (rs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (rs, s, donePaintScreen, rotateDonePaintScreen);
}

static Bool
rotatePaintScreen (CompScreen		   *s,
		   const ScreenPaintAttrib *sAttrib,
		   Region		   region,
		   unsigned int		   mask)
{
    Bool status;

    ROTATE_SCREEN (s);

    if (rs->grabIndex || rs->moving)
    {
	ScreenPaintAttrib sa = *sAttrib;

	sa.xRotate += rs->baseXrot + rs->xrot;
	sa.vRotate += rs->yrot;

	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_TRANSFORMED_MASK;

	UNWRAP (rs, s, paintScreen);
	status = (*s->paintScreen) (s, &sa, region, mask);
	WRAP (rs, s, paintScreen, rotatePaintScreen);
    }
    else
    {
	UNWRAP (rs, s, paintScreen);
	status = (*s->paintScreen) (s, sAttrib, region, mask);
	WRAP (rs, s, paintScreen, rotatePaintScreen);
    }

    return status;
}

static Bool
rotateInitiate (CompDisplay     *d,
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
	ROTATE_SCREEN (s);

	if (rs->rotateHandle && rs->grabWindow)
	{
	    if (otherScreenGrabExist (s, "rotate", "move", 0))
		return FALSE;
	}
	else
	{
	    if (otherScreenGrabExist (s, "rotate", "switcher", "cube", 0))
		return FALSE;
	}

	rs->moving = FALSE;
	rs->slow   = FALSE;

	if (!rs->grabIndex)
	{
	    rs->grabIndex = pushScreenGrab (s, s->invisibleCursor, "rotate");
	    if (rs->grabIndex)
	    {
		int x, y;

		x = getIntOptionNamed (option, nOption, "x", 0);
		y = getIntOptionNamed (option, nOption, "y", 0);

		rs->savedPointer.x = x;
		rs->savedPointer.y = y;
	    }
	}

	if (rs->grabIndex)
	{
	    rs->moveTo = 0.0f;

	    rs->grabbed = TRUE;
	    rs->snapTop = rs->opt[ROTATE_SCREEN_OPTION_SNAP_TOP].value.b;

	    if (state & CompActionStateInitButton)
		action->state |= CompActionStateTermButton;

	    if (state & CompActionStateInitKey)
		action->state |= CompActionStateTermKey;
	}
    }

    return FALSE;
}

static Bool
rotateTerminate (CompDisplay     *d,
		 CompAction      *action,
		 CompActionState state,
		 CompOption      *option,
		 int		 nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	ROTATE_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (rs->grabIndex)
	{
	    rs->grabbed = FALSE;
	    damageScreen (s);
	}
    }

    action->state &= ~(CompActionStateTermButton | CompActionStateTermKey);

    return FALSE;
}

static Bool
rotate (CompDisplay     *d,
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
	int direction;

	ROTATE_SCREEN (s);

	if (otherScreenGrabExist (s, "rotate", "move", "switcher", "cube", 0))
	    return FALSE;

	direction = getIntOptionNamed (option, nOption, "direction", 0);
	if (!direction)
	    return FALSE;

	/* we allow the grab to fail here so that we can rotate on
	   drag-and-drop */
	if (!rs->grabIndex)
	{
	    CompOption o[3];

	    o[0].type    = CompOptionTypeInt;
	    o[0].name    = "x";
	    o[0].value.i = getIntOptionNamed (option, nOption, "x", 0);

	    o[1].type    = CompOptionTypeInt;
	    o[1].name    = "y";
	    o[1].value.i = getIntOptionNamed (option, nOption, "y", 0);

	    o[2].type	 = CompOptionTypeInt;
	    o[2].name	 = "root";
	    o[2].value.i = s->root;

	    rotateInitiate (d, NULL, 0, o, 3);
	}

	rs->moving  = TRUE;
	rs->moveTo += (360.0f / s->size) * direction;
	rs->grabbed = FALSE;

	damageScreen (s);
    }

    return FALSE;
}

static Bool
rotateWithWindow (CompDisplay     *d,
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
	CompWindow *w;
	int	   direction;

	ROTATE_SCREEN (s);

	direction = getIntOptionNamed (option, nOption, "direction", 0);
	if (!direction)
	    return FALSE;

	xid = getIntOptionNamed (option, nOption, "window", 0);

	w = findWindowAtScreen (s, xid);
	if (w)
	{
	    if (!(w->type & (CompWindowTypeDesktopMask |
			     CompWindowTypeDockMask)))
	    {
		if (!(w->state & CompWindowStateStickyMask))
		{
		    rs->moveWindow  = w->id;
		    rs->moveWindowX = w->attrib.x;
		}
	    }
	}

	if (!rs->grabIndex)
	{
	    CompOption o[3];

	    o[0].type    = CompOptionTypeInt;
	    o[0].name    = "x";
	    o[0].value.i = getIntOptionNamed (option, nOption, "x", 0);

	    o[1].type    = CompOptionTypeInt;
	    o[1].name    = "y";
	    o[1].value.i = getIntOptionNamed (option, nOption, "y", 0);

	    o[2].type	 = CompOptionTypeInt;
	    o[2].name	 = "root";
	    o[2].value.i = s->root;

	    rotateInitiate (d, NULL, 0, o, 3);
	}

	if (rs->grabIndex)
	{
	    rs->moving  = TRUE;
	    rs->moveTo += (360.0f / s->size) * direction;
	    rs->grabbed = FALSE;

	    damageScreen (s);
	}
    }

    return FALSE;
}

static Bool
rotateLeft (CompDisplay     *d,
	    CompAction      *action,
	    CompActionState state,
	    CompOption      *option,
	    int		    nOption)
{
    CompOption o[4];

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "x";
    o[0].value.i = getIntOptionNamed (option, nOption, "x", 0);

    o[1].type    = CompOptionTypeInt;
    o[1].name    = "y";
    o[1].value.i = getIntOptionNamed (option, nOption, "y", 0);

    o[2].type	 = CompOptionTypeInt;
    o[2].name	 = "root";
    o[2].value.i = getIntOptionNamed (option, nOption, "root", 0);

    o[3].type	 = CompOptionTypeInt;
    o[3].name	 = "direction";
    o[3].value.i = -1;

    rotate (d, NULL, 0, o, 4);

    return FALSE;
}

static Bool
rotateRight (CompDisplay     *d,
	     CompAction      *action,
	     CompActionState state,
	     CompOption      *option,
	     int	     nOption)
{
    CompOption o[4];

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "x";
    o[0].value.i = getIntOptionNamed (option, nOption, "x", 0);

    o[1].type    = CompOptionTypeInt;
    o[1].name    = "y";
    o[1].value.i = getIntOptionNamed (option, nOption, "y", 0);

    o[2].type	 = CompOptionTypeInt;
    o[2].name	 = "root";
    o[2].value.i = getIntOptionNamed (option, nOption, "root", 0);

    o[3].type	 = CompOptionTypeInt;
    o[3].name	 = "direction";
    o[3].value.i = 1;

    rotate (d, NULL, 0, o, 4);

    return FALSE;
}

static Bool
rotateLeftWithWindow (CompDisplay     *d,
		      CompAction      *action,
		      CompActionState state,
		      CompOption      *option,
		      int	      nOption)
{
    CompOption o[5];

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "x";
    o[0].value.i = getIntOptionNamed (option, nOption, "x", 0);

    o[1].type    = CompOptionTypeInt;
    o[1].name    = "y";
    o[1].value.i = getIntOptionNamed (option, nOption, "y", 0);

    o[2].type	 = CompOptionTypeInt;
    o[2].name	 = "root";
    o[2].value.i = getIntOptionNamed (option, nOption, "root", 0);

    o[3].type	 = CompOptionTypeInt;
    o[3].name	 = "direction";
    o[3].value.i = -1;

    o[4].type	 = CompOptionTypeInt;
    o[4].name	 = "window";
    o[4].value.i = getIntOptionNamed (option, nOption, "window", 0);

    rotateWithWindow (d, NULL, 0, o, 5);

    return FALSE;
}

static Bool
rotateRightWithWindow (CompDisplay     *d,
		       CompAction      *action,
		       CompActionState state,
		       CompOption      *option,
		       int	       nOption)
{
    CompOption o[5];

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "x";
    o[0].value.i = getIntOptionNamed (option, nOption, "x", 0);

    o[1].type    = CompOptionTypeInt;
    o[1].name    = "y";
    o[1].value.i = getIntOptionNamed (option, nOption, "y", 0);

    o[2].type	 = CompOptionTypeInt;
    o[2].name	 = "root";
    o[2].value.i = getIntOptionNamed (option, nOption, "root", 0);

    o[3].type	 = CompOptionTypeInt;
    o[3].name	 = "direction";
    o[3].value.i = 1;

    o[4].type	 = CompOptionTypeInt;
    o[4].name	 = "window";
    o[4].value.i = getIntOptionNamed (option, nOption, "window", 0);

    rotateWithWindow (d, NULL, 0, o, 5);

    return FALSE;
}

static Bool
rotateFlipLeft (void *closure)
{
    CompScreen *s = closure;
    int        warpX;
    CompOption o[4];

    ROTATE_SCREEN (s);

    rs->moveTo = 0.0f;
    rs->slow = FALSE;

    if (otherScreenGrabExist (s, "rotate", "move", 0))
	return FALSE;

    warpX = pointerX + s->width;
    warpPointer (s->display, s->width - 10, 0);
    lastPointerX = warpX;

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "x";
    o[0].value.i = 0;

    o[1].type    = CompOptionTypeInt;
    o[1].name    = "y";
    o[1].value.i = pointerY;

    o[2].type	 = CompOptionTypeInt;
    o[2].name	 = "root";
    o[2].value.i = s->root;

    o[3].type	 = CompOptionTypeInt;
    o[3].name	 = "direction";
    o[3].value.i = -1;

    rotate (s->display, NULL, 0, o, 4);

    XWarpPointer (s->display->display, None, None, 0, 0, 0, 0, -1, 0);
    rs->savedPointer.x = lastPointerX - 9;

    rs->rotateHandle = 0;

    return FALSE;
}

static Bool
rotateFlipRight (void *closure)
{
    CompScreen *s = closure;
    int        warpX;
    CompOption o[4];

    ROTATE_SCREEN (s);

    rs->moveTo = 0.0f;
    rs->slow = FALSE;

    if (otherScreenGrabExist (s, "rotate", "move", 0))
	return FALSE;

    warpX = pointerX - s->width;
    warpPointer (s->display, 10 - s->width, 0);
    lastPointerX = warpX;

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "x";
    o[0].value.i = 0;

    o[1].type    = CompOptionTypeInt;
    o[1].name    = "y";
    o[1].value.i = pointerY;

    o[2].type	 = CompOptionTypeInt;
    o[2].name	 = "root";
    o[2].value.i = s->root;

    o[3].type	 = CompOptionTypeInt;
    o[3].name	 = "direction";
    o[3].value.i = 1;

    rotate (s->display, NULL, 0, o, 4);

    XWarpPointer (s->display->display, None, None, 0, 0, 0, 0, 1, 0);

    rs->savedPointer.x = lastPointerX + 9;

    rs->rotateHandle = 0;

    return FALSE;
}

static void
rotateEdgeFlip (CompScreen      *s,
		int		edge,
		CompAction      *action,
		CompActionState state,
		CompOption      *option,
		int		nOption)
{
    CompOption o[4];

    ROTATE_DISPLAY (s->display);

    if (otherScreenGrabExist (s, "rotate", "move", 0))
	return;

    if (state & CompActionStateInitEdgeDnd)
    {
	if (!rd->opt[ROTATE_DISPLAY_OPTION_EDGEFLIP_DND].value.b)
	    return;

	if (otherScreenGrabExist (s, "rotate", 0))
	    return;
    }
    else if (otherScreenGrabExist (s, "rotate", 0))
    {
	ROTATE_SCREEN (s);

	if (!rd->opt[ROTATE_DISPLAY_OPTION_EDGEFLIP_WINDOW].value.b)
	    return;

	if (!rs->grabWindow)
	    return;

	/* bail out if window is horizontally maximized or fullscreen */
	if (rs->grabWindow->state & (CompWindowStateMaximizedHorzMask |
				     CompWindowStateFullscreenMask))
	    return;
    }
    else
    {
	if (!rd->opt[ROTATE_DISPLAY_OPTION_EDGEFLIP_POINTER].value.b)
	    return;
    }

    o[0].type    = CompOptionTypeInt;
    o[0].name    = "x";
    o[0].value.i = 0;

    o[1].type    = CompOptionTypeInt;
    o[1].name    = "y";
    o[1].value.i = pointerY;

    o[2].type	 = CompOptionTypeInt;
    o[2].name	 = "root";
    o[2].value.i = s->root;

    o[3].type	 = CompOptionTypeInt;
    o[3].name	 = "direction";

    if (edge == SCREEN_EDGE_LEFT)
    {
	ROTATE_SCREEN (s);

	if (rd->flipTime == 0 || (rs->moving && !rs->slow))
	{
	    int pointerDx = pointerX - lastPointerX;
	    int warpX;

	    warpX = pointerX + s->width;
	    warpPointer (s->display, s->width - 10, 0);
	    lastPointerX = warpX - pointerDx;

	    o[3].value.i = -1;

	    rotate (s->display, NULL, 0, o, 4);

	    XWarpPointer (s->display->display, None, None,
			  0, 0, 0, 0, -1, 0);
	    rs->savedPointer.x = lastPointerX - 9;
	}
	else
	{
	    if (!rs->rotateHandle)
		rs->rotateHandle =
		    compAddTimeout (rd->flipTime, rotateFlipLeft, s);

	    rs->moving  = TRUE;
	    rs->moveTo -= 360.0f / s->size;
	    rs->slow    = TRUE;

	    if (state & CompActionStateInitEdge)
		action->state |= CompActionStateTermEdge;

	    if (state & CompActionStateInitEdgeDnd)
		action->state |= CompActionStateTermEdgeDnd;

	    damageScreen (s);
	}
    }
    else
    {
	ROTATE_SCREEN (s);

	if (rd->flipTime == 0 || (rs->moving && !rs->slow))
	{
	    int pointerDx = pointerX - lastPointerX;
	    int warpX;

	    warpX = pointerX - s->width;
	    warpPointer (s->display, 10 - s->width, 0);
	    lastPointerX = warpX - pointerDx;

	    o[3].value.i = 1;

	    rotate (s->display, NULL, 0, o, 4);

	    XWarpPointer (s->display->display, None, None,
			  0, 0, 0, 0, 1, 0);
	    rs->savedPointer.x = lastPointerX + 9;
	}
	else
	{
	    if (!rs->rotateHandle)
		rs->rotateHandle =
		    compAddTimeout (rd->flipTime, rotateFlipRight, s);

	    rs->moving  = TRUE;
	    rs->moveTo += 360.0f / s->size;
	    rs->slow    = TRUE;

	    if (state & CompActionStateInitEdge)
		action->state |= CompActionStateTermEdge;

	    if (state & CompActionStateInitEdgeDnd)
		action->state |= CompActionStateTermEdgeDnd;

	    damageScreen (s);
	}
    }
}

static Bool
rotateFlipTerminate (CompDisplay     *d,
		     CompAction      *action,
		     CompActionState state,
		     CompOption      *option,
		     int	     nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);

    for (s = d->screens; s; s = s->next)
    {
	ROTATE_SCREEN (s);

	if (xid && s->root != xid)
	    continue;

	if (rs->rotateHandle)
	{
	    compRemoveTimeout (rs->rotateHandle);
	    rs->rotateHandle = 0;

	    if (rs->slow)
	    {
		rs->moveTo = 0.0f;
		rs->slow = FALSE;
	    }

	    damageScreen (s);
	}

	action->state &= ~(CompActionStateTermEdge |
			   CompActionStateTermEdgeDnd);
    }

    return FALSE;
}

static Bool
rotateEdgeFlipLeft (CompDisplay     *d,
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
	rotateEdgeFlip (s, SCREEN_EDGE_LEFT, action, state, option, nOption);

    return FALSE;
}

static Bool
rotateEdgeFlipRight (CompDisplay     *d,
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
	rotateEdgeFlip (s, SCREEN_EDGE_RIGHT, action, state, option, nOption);

    return FALSE;
}

static int
rotateRotationTo (CompScreen *s,
		  int	     face)
{
    int delta;

    ROTATE_SCREEN (s);

    delta = face - s->x - (rs->moveTo / (360.0f / s->size));
    if (delta > s->size / 2)
	delta -= s->size;
    else if (delta < -(s->size / 2))
	delta += s->size;

    return delta;
}

static Bool
rotateTo (CompDisplay     *d,
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
	CompOption o[4];
	int	   face = -1;
	int	   i = ROTATE_DISPLAY_OPTION_TO_1;

	ROTATE_DISPLAY (s->display);

	while (i < ROTATE_DISPLAY_OPTION_TO_12)
	{
	    if (action == &rd->opt[i].value.action)
	    {
		face = i - ROTATE_DISPLAY_OPTION_TO_1;
		break;
	    }

	    i++;
	}

	if (face < 0)
	    face = getIntOptionNamed (option, nOption, "face", s->x);

	o[0].type    = CompOptionTypeInt;
	o[0].name    = "x";
	o[0].value.i = getIntOptionNamed (option, nOption, "x", pointerX);

	o[1].type    = CompOptionTypeInt;
	o[1].name    = "y";
	o[1].value.i = getIntOptionNamed (option, nOption, "y", pointerY);

	o[2].type    = CompOptionTypeInt;
	o[2].name    = "root";
	o[2].value.i = s->root;

	o[3].type    = CompOptionTypeInt;
	o[3].name    = "direction";
	o[3].value.i = rotateRotationTo (s, face);

	rotate (d, NULL, 0, o, 4);
    }

    return FALSE;
}

static Bool
rotateToWithWindow (CompDisplay     *d,
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
	CompOption o[5];
	int	   face = -1;
	int	   i = ROTATE_DISPLAY_OPTION_TO_1_WINDOW;

	ROTATE_DISPLAY (s->display);

	while (i < ROTATE_DISPLAY_OPTION_TO_12_WINDOW)
	{
	    if (action == &rd->opt[i].value.action)
	    {
		face = i - ROTATE_DISPLAY_OPTION_TO_1_WINDOW;
		break;
	    }

	    i++;
	}

	if (face < 0)
	    face = getIntOptionNamed (option, nOption, "face", s->x);

	o[0].type    = CompOptionTypeInt;
	o[0].name    = "x";
	o[0].value.i = getIntOptionNamed (option, nOption, "x", pointerX);

	o[1].type    = CompOptionTypeInt;
	o[1].name    = "y";
	o[1].value.i = getIntOptionNamed (option, nOption, "y", pointerY);

	o[2].type    = CompOptionTypeInt;
	o[2].name    = "root";
	o[2].value.i = s->root;

	o[3].type    = CompOptionTypeInt;
	o[3].name    = "direction";
	o[3].value.i = rotateRotationTo (s, face);

	o[4].type    = CompOptionTypeInt;
	o[4].name    = "window";
	o[4].value.i = getIntOptionNamed (option, nOption, "window", 0);

	rotate (d, NULL, 0, o, 5);
    }

    return FALSE;
}

static void
rotateHandleEvent (CompDisplay *d,
		   XEvent      *event)
{
    CompScreen *s;

    ROTATE_DISPLAY (d);

    switch (event->type) {
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	{
	    ROTATE_SCREEN (s);

	    if (rs->grabIndex)
	    {
		if (rs->grabbed)
		{
		    GLfloat pointerDx, pointerDy;

		    pointerDx = pointerX - lastPointerX;
		    pointerDy = pointerY - lastPointerY;

		    if (event->xmotion.x_root < 50	       ||
			event->xmotion.y_root < 50	       ||
			event->xmotion.x_root > s->width  - 50 ||
			event->xmotion.y_root > s->height - 50)
		    {
			warpPointer (d,
				     (s->width  / 2) - pointerX,
				     (s->height / 2) - pointerY);
		    }

		    if (rs->pointerInvertY)
			pointerDy = -pointerDy;

		    rs->xVelocity += pointerDx * rs->pointerSensitivity *
			rs->invert;
		    rs->yVelocity += pointerDy * rs->pointerSensitivity;

		    damageScreen (s);
		}
		else
		{
		    rs->savedPointer.x += pointerX - lastPointerX;
		    rs->savedPointer.y += pointerY - lastPointerY;
		}
	    }
	}
	break;
    case ClientMessage:
	if (event->xclient.message_type == d->winActiveAtom)
	{
	    CompWindow *w;

	    w = findWindowAtDisplay (d, event->xclient.window);
	    if (w)
	    {
		int dx;

		ROTATE_SCREEN (w->screen);

		s = w->screen;

		/* window must be placed */
		if (!w->placed)
		    break;

		if (otherScreenGrabExist (s, "rotate", "switcher", "cube", 0))
		    break;

		/* reset movement */
		rs->moving = FALSE;
		rs->moveTo = 0.0f;

		dx = defaultViewportForWindow (w) - s->x;
		if (dx)
		{
		    Window	 win;
		    int		 i, x, y;
		    unsigned int ui;
		    CompOption   o[4];

		    XQueryPointer (d->display, s->root,
				   &win, &win, &x, &y, &i, &i, &ui);

		    if (dx > (s->size + 1) / 2)
			dx -= s->size;
		    else if (dx < -(s->size + 1) / 2)
			dx += s->size;

		    o[0].type    = CompOptionTypeInt;
		    o[0].name    = "x";
		    o[0].value.i = x;

		    o[1].type    = CompOptionTypeInt;
		    o[1].name    = "y";
		    o[1].value.i = y;

		    o[2].type	 = CompOptionTypeInt;
		    o[2].name	 = "root";
		    o[2].value.i = s->root;

		    o[3].type	 = CompOptionTypeInt;
		    o[3].name	 = "direction";
		    o[3].value.i = dx;

		    rotate (d, NULL, 0, o, 4);
		}
	    }
	}
	else if (event->xclient.message_type == d->desktopViewportAtom)
	{
	    s = findScreenAtDisplay (d, event->xclient.window);
	    if (s)
	    {
		int dx;

		if (otherScreenGrabExist (s, "rotate", "switcher", "cube", 0))
		    break;

		dx = event->xclient.data.l[0] / s->width - s->x;
		if (dx)
		{
		    Window	 win;
		    int		 i, x, y;
		    unsigned int ui;
		    CompOption   o[4];

		    XQueryPointer (d->display, s->root,
				   &win, &win, &x, &y, &i, &i, &ui);

		    if (dx > (s->size + 1) / 2)
			dx -= s->size;
		    else if (dx < -(s->size + 1) / 2)
			dx += s->size;

		    o[0].type    = CompOptionTypeInt;
		    o[0].name    = "x";
		    o[0].value.i = x;

		    o[1].type    = CompOptionTypeInt;
		    o[1].name    = "y";
		    o[1].value.i = y;

		    o[2].type	 = CompOptionTypeInt;
		    o[2].name	 = "root";
		    o[2].value.i = s->root;

		    o[3].type	 = CompOptionTypeInt;
		    o[3].name	 = "direction";
		    o[3].value.i = dx;

		    rotate (d, NULL, 0, o, 4);
		}
	    }
	}
    default:
	break;
    }

    UNWRAP (rd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (rd, d, handleEvent, rotateHandleEvent);
}

static void
rotateWindowGrabNotify (CompWindow   *w,
			int	     x,
			int	     y,
			unsigned int state,
			unsigned int mask)
{
    ROTATE_SCREEN (w->screen);

    rs->grabMask   = mask;
    rs->grabWindow = w;

    UNWRAP (rs, w->screen, windowGrabNotify);
    (*w->screen->windowGrabNotify) (w, x, y, state, mask);
    WRAP (rs, w->screen, windowGrabNotify, rotateWindowGrabNotify);
}

static void
rotateWindowUngrabNotify (CompWindow *w)
{
    ROTATE_SCREEN (w->screen);

    rs->grabMask   = 0;
    rs->grabWindow = NULL;

    UNWRAP (rs, w->screen, windowUngrabNotify);
    (*w->screen->windowUngrabNotify) (w);
    WRAP (rs, w->screen, windowUngrabNotify, rotateWindowUngrabNotify);
}

static void
rotateUpdateCubeOptions (CompScreen *s)
{
    CompPlugin *p;

    ROTATE_SCREEN (s);

    p = findActivePlugin ("cube");
    if (p && p->vTable->getScreenOptions)
    {
	CompOption *options, *option;
	int	   nOptions;

	options = (*p->vTable->getScreenOptions) (s, &nOptions);
	option = compFindOption (options, nOptions, "in", 0);
	if (option)
	    rs->invert = option->value.b ? -1 : 1;
    }
}

static Bool
rotateSetScreenOptionForPlugin (CompScreen      *s,
				char	        *plugin,
				char	        *name,
				CompOptionValue *value)
{
    Bool status;

    ROTATE_SCREEN (s);

    UNWRAP (rs, s, setScreenOptionForPlugin);
    status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
    WRAP (rs, s, setScreenOptionForPlugin, rotateSetScreenOptionForPlugin);

    if (status && strcmp (plugin, "cube") == 0 && strcmp (name, "in") == 0)
	rotateUpdateCubeOptions (s);

    return status;
}

static CompOption *
rotateGetDisplayOptions (CompDisplay *display,
			 int	     *count)
{
    ROTATE_DISPLAY (display);

    *count = NUM_OPTIONS (rd);
    return rd->opt;
}

static Bool
rotateSetDisplayOption (CompDisplay     *display,
			char	        *name,
			CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    ROTATE_DISPLAY (display);

    o = compFindOption (rd->opt, NUM_OPTIONS (rd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case ROTATE_DISPLAY_OPTION_INITIATE:
    case ROTATE_DISPLAY_OPTION_LEFT:
    case ROTATE_DISPLAY_OPTION_RIGHT:
    case ROTATE_DISPLAY_OPTION_TO_1:
    case ROTATE_DISPLAY_OPTION_TO_2:
    case ROTATE_DISPLAY_OPTION_TO_3:
    case ROTATE_DISPLAY_OPTION_TO_4:
    case ROTATE_DISPLAY_OPTION_TO_5:
    case ROTATE_DISPLAY_OPTION_TO_6:
    case ROTATE_DISPLAY_OPTION_TO_7:
    case ROTATE_DISPLAY_OPTION_TO_8:
    case ROTATE_DISPLAY_OPTION_TO_9:
    case ROTATE_DISPLAY_OPTION_TO_10:
    case ROTATE_DISPLAY_OPTION_TO_11:
    case ROTATE_DISPLAY_OPTION_TO_12:
    case ROTATE_DISPLAY_OPTION_LEFT_WINDOW:
    case ROTATE_DISPLAY_OPTION_RIGHT_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_1_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_2_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_3_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_4_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_5_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_6_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_7_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_8_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_9_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_10_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_11_WINDOW:
    case ROTATE_DISPLAY_OPTION_TO_12_WINDOW:
    case ROTATE_DISPLAY_OPTION_FLIP_LEFT:
    case ROTATE_DISPLAY_OPTION_FLIP_RIGHT:
	if (setDisplayAction (display, o, value))
	    return TRUE;
	break;
    case ROTATE_DISPLAY_OPTION_TO:
    case ROTATE_DISPLAY_OPTION_WINDOW:
	if (compSetActionOption (o, value))
	    return TRUE;
	break;
    case ROTATE_DISPLAY_OPTION_EDGEFLIP_POINTER:
    case ROTATE_DISPLAY_OPTION_EDGEFLIP_WINDOW:
    case ROTATE_DISPLAY_OPTION_EDGEFLIP_DND:
	if (compSetBoolOption (o, value))
	    return TRUE;
	break;
    case ROTATE_DISPLAY_OPTION_FLIPTIME:
	if (compSetIntOption (o, value))
	{
	    rd->flipTime = o->value.i;
	    return TRUE;
	}
    default:
	break;
    }

    return FALSE;
}

static void
rotateDisplayInitOptions (RotateDisplay *rd,
			  Display       *display)
{
    CompOption *o;
    char       *str;

    o = &rd->opt[ROTATE_DISPLAY_OPTION_INITIATE];
    o->name			     = "initiate";
    o->shortDesc		     = N_("Initiate");
    o->longDesc			     = N_("Start Rotation");
    o->type			     = CompOptionTypeAction;
    o->value.action.initiate	     = rotateInitiate;
    o->value.action.terminate	     = rotateTerminate;
    o->value.action.bell	     = FALSE;
    o->value.action.edgeMask	     = 0;
    o->value.action.state	     = CompActionStateInitKey;
    o->value.action.state	    |= CompActionStateInitButton;
    o->value.action.type	     = CompBindingTypeButton;
    o->value.action.button.modifiers = ROTATE_INITIATE_MODIFIERS_DEFAULT;
    o->value.action.button.button    = ROTATE_INITIATE_BUTTON_DEFAULT;

    o = &rd->opt[ROTATE_DISPLAY_OPTION_LEFT];
    o->name			  = "rotate_left";
    o->shortDesc		  = N_("Rotate Left");
    o->longDesc			  = N_("Rotate left");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = rotateLeft;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = ROTATE_LEFT_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (ROTATE_LEFT_KEY_DEFAULT));

    o = &rd->opt[ROTATE_DISPLAY_OPTION_RIGHT];
    o->name			  = "rotate_right";
    o->shortDesc		  = N_("Rotate Right");
    o->longDesc			  = N_("Rotate right");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = rotateRight;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = ROTATE_RIGHT_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (ROTATE_RIGHT_KEY_DEFAULT));

    o = &rd->opt[ROTATE_DISPLAY_OPTION_LEFT_WINDOW];
    o->name			  = "rotate_left_window";
    o->shortDesc		  = N_("Rotate Left with Window");
    o->longDesc			  = N_("Rotate left and bring active window "
				       "along");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = rotateLeftWithWindow;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = ROTATE_LEFT_WINDOW_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (ROTATE_LEFT_WINDOW_KEY_DEFAULT));

    o = &rd->opt[ROTATE_DISPLAY_OPTION_RIGHT_WINDOW];
    o->name			  = "rotate_right_window";
    o->shortDesc		  = N_("Rotate Right with Window");
    o->longDesc			  = N_("Rotate right and bring active window "
				       "along");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = rotateRightWithWindow;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeKey;
    o->value.action.key.modifiers = ROTATE_RIGHT_WINDOW_MODIFIERS_DEFAULT;
    o->value.action.key.keycode   =
	XKeysymToKeycode (display,
			  XStringToKeysym (ROTATE_RIGHT_WINDOW_KEY_DEFAULT));

#define ROTATE_TO_SHORT        N_("Rotate To Face %d")
#define ROTATE_TO_LONG         N_("Rotate to face %d")
#define ROTATE_TO_WINDOW_SHORT N_("Rotate To Face %d with Window")
#define ROTATE_TO_WINDOW_LONG  N_("Rotate to face %d and bring active " \
				  "window along")

#define ROTATE_TO_OPTION(n)						 \
    o = &rd->opt[ROTATE_DISPLAY_OPTION_TO_ ## n];			 \
    o->name			  = "rotate_to_" #n;			 \
    asprintf (&str, ROTATE_TO_SHORT, n);				 \
    o->shortDesc		  = str;				 \
    asprintf (&str, ROTATE_TO_LONG, n);					 \
    o->longDesc			  = str;				 \
    o->type			  = CompOptionTypeAction;		 \
    o->value.action.initiate	  = rotateTo;				 \
    o->value.action.terminate	  = 0;					 \
    o->value.action.bell	  = FALSE;				 \
    o->value.action.edgeMask	  = 0;					 \
    o->value.action.state	  = CompActionStateInitKey;		 \
    o->value.action.state	 |= CompActionStateInitButton;		 \
    o->value.action.type	  = CompBindingTypeNone;		 \
									 \
    o = &rd->opt[ROTATE_DISPLAY_OPTION_TO_ ## n ## _WINDOW];		 \
    o->name			  = "rotate_to_" #n "_window";		 \
    asprintf (&str, ROTATE_TO_WINDOW_SHORT, n);				 \
    o->shortDesc		  = str;				 \
    asprintf (&str, ROTATE_TO_WINDOW_LONG, n);				 \
    o->longDesc			  = str;				 \
    o->type			  = CompOptionTypeAction;		 \
    o->value.action.initiate	  = rotateToWithWindow;			 \
    o->value.action.terminate	  = 0;					 \
    o->value.action.bell	  = FALSE;				 \
    o->value.action.edgeMask	  = 0;					 \
    o->value.action.state	  = CompActionStateInitKey;		 \
    o->value.action.state	 |= CompActionStateInitButton;		 \
    o->value.action.type	  = CompBindingTypeNone

    ROTATE_TO_OPTION (1);
    ROTATE_TO_OPTION (2);
    ROTATE_TO_OPTION (3);
    ROTATE_TO_OPTION (4);
    ROTATE_TO_OPTION (5);
    ROTATE_TO_OPTION (6);
    ROTATE_TO_OPTION (7);
    ROTATE_TO_OPTION (8);
    ROTATE_TO_OPTION (9);
    ROTATE_TO_OPTION (10);
    ROTATE_TO_OPTION (11);
    ROTATE_TO_OPTION (12);

    o = &rd->opt[ROTATE_DISPLAY_OPTION_TO];
    o->name			  = "rotate_to";
    o->shortDesc		  = N_("Rotate To");
    o->longDesc			  = N_("Rotate to viewport");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = rotateTo;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = 0;
    o->value.action.type	  = CompBindingTypeNone;

    o = &rd->opt[ROTATE_DISPLAY_OPTION_WINDOW];
    o->name			  = "rotate_window";
    o->shortDesc		  = N_("Rotate Window");
    o->longDesc			  = N_("Rotate with window");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = rotateWithWindow;
    o->value.action.terminate	  = 0;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 0;
    o->value.action.state	  = 0;
    o->value.action.type	  = CompBindingTypeNone;

    o = &rd->opt[ROTATE_DISPLAY_OPTION_FLIP_LEFT];
    o->name			  = "rotate_flip_left";
    o->shortDesc		  = N_("Rotate Flip Left");
    o->longDesc			  = N_("Flip to left viewport and warp "
				       "pointer");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = rotateEdgeFlipLeft;
    o->value.action.terminate	  = rotateFlipTerminate;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 1 << SCREEN_EDGE_LEFT;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeNone;

    o = &rd->opt[ROTATE_DISPLAY_OPTION_FLIP_RIGHT];
    o->name			  = "rotate_flip_right";
    o->shortDesc		  = N_("Rotate Flip Right");
    o->longDesc			  = N_("Flip to right viewport and warp "
				       "pointer");
    o->type			  = CompOptionTypeAction;
    o->value.action.initiate	  = rotateEdgeFlipRight;
    o->value.action.terminate	  = rotateFlipTerminate;
    o->value.action.bell	  = FALSE;
    o->value.action.edgeMask	  = 1 << SCREEN_EDGE_RIGHT;
    o->value.action.state	  = CompActionStateInitEdge;
    o->value.action.state	 |= CompActionStateInitEdgeDnd;
    o->value.action.state	 |= CompActionStateInitKey;
    o->value.action.state	 |= CompActionStateInitButton;
    o->value.action.type	  = CompBindingTypeNone;

    o = &rd->opt[ROTATE_DISPLAY_OPTION_EDGEFLIP_POINTER];
    o->name      = "edge_flip_pointer";
    o->shortDesc = N_("Edge Flip Pointer");
    o->longDesc  = N_("Flip to next viewport when moving pointer to screen "
		      "edge");
    o->type      = CompOptionTypeBool;
    o->value.b   = ROTATE_EDGEFLIP_POINTER_DEFAULT;

    o = &rd->opt[ROTATE_DISPLAY_OPTION_EDGEFLIP_WINDOW];
    o->name      = "edge_flip_move";
    o->shortDesc = N_("Edge Flip Move");
    o->longDesc  = N_("Flip to next viewport when moving window to screen "
		      "edge");
    o->type      = CompOptionTypeBool;
    o->value.b   = ROTATE_EDGEFLIP_WINDOW_DEFAULT;

    o = &rd->opt[ROTATE_DISPLAY_OPTION_EDGEFLIP_DND];
    o->name      = "edge_flip_dnd";
    o->shortDesc = N_("Edge Flip DnD");
    o->longDesc  = N_("Flip to next viewport when dragging object to screen "
		      "edge");
    o->type      = CompOptionTypeBool;
    o->value.b   = ROTATE_EDGEFLIP_DND_DEFAULT;

    o = &rd->opt[ROTATE_DISPLAY_OPTION_FLIPTIME];
    o->name	  = "flip_time";
    o->shortDesc  = N_("Flip Time");
    o->longDesc	  = N_("Timeout before flipping viewport");
    o->type	  = CompOptionTypeInt;
    o->value.i	  = ROTATE_FLIPTIME_DEFAULT;
    o->rest.i.min = ROTATE_FLIPTIME_MIN;
    o->rest.i.max = ROTATE_FLIPTIME_MAX;
}

static Bool
rotateInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    RotateDisplay *rd;

    rd = malloc (sizeof (RotateDisplay));
    if (!rd)
	return FALSE;

    rd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (rd->screenPrivateIndex < 0)
    {
	free (rd);
	return FALSE;
    }

    rd->flipTime = ROTATE_FLIPTIME_DEFAULT;

    rotateDisplayInitOptions (rd, d->display);

    WRAP (rd, d, handleEvent, rotateHandleEvent);

    d->privates[displayPrivateIndex].ptr = rd;

    return TRUE;
}

static void
rotateFiniDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    ROTATE_DISPLAY (d);

    freeScreenPrivateIndex (d, rd->screenPrivateIndex);

    UNWRAP (rd, d, handleEvent);

    free (rd);
}

static Bool
rotateInitScreen (CompPlugin *p,
		  CompScreen *s)
{
    RotateScreen *rs;

    ROTATE_DISPLAY (s->display);

    rs = malloc (sizeof (RotateScreen));
    if (!rs)
	return FALSE;

    rs->grabIndex = 0;

    rs->xrot = 0.0f;
    rs->xVelocity = 0.0f;
    rs->yrot = 0.0f;
    rs->yVelocity = 0.0f;

    rs->baseXrot = 0.0f;

    rs->moving = FALSE;
    rs->moveTo = 0.0f;

    rs->moveWindow = 0;

    rs->savedPointer.x = 0;
    rs->savedPointer.y = 0;

    rs->grabbed = FALSE;
    rs->snapTop = FALSE;

    rs->slow       = FALSE;
    rs->grabMask   = FALSE;
    rs->grabWindow = NULL;

    rs->acceleration = ROTATE_ACCELERATION_DEFAULT;

    rs->pointerInvertY     = ROTATE_POINTER_INVERT_Y_DEFAULT;
    rs->pointerSensitivity = ROTATE_POINTER_SENSITIVITY_DEFAULT *
	ROTATE_POINTER_SENSITIVITY_FACTOR;

    rs->speed    = ROTATE_SPEED_DEFAULT;
    rs->timestep = ROTATE_TIMESTEP_DEFAULT;

    rs->rotateHandle = 0;

    rotateScreenInitOptions (rs);

    addScreenAction (s, &rd->opt[ROTATE_DISPLAY_OPTION_INITIATE].value.action);
    addScreenAction (s, &rd->opt[ROTATE_DISPLAY_OPTION_LEFT].value.action);
    addScreenAction (s, &rd->opt[ROTATE_DISPLAY_OPTION_RIGHT].value.action);
    addScreenAction (s,
		     &rd->opt[ROTATE_DISPLAY_OPTION_LEFT_WINDOW].value.action);
    addScreenAction (s,
		     &rd->opt[ROTATE_DISPLAY_OPTION_RIGHT_WINDOW].value.action);
    addScreenAction (s, &rd->opt[ROTATE_DISPLAY_OPTION_FLIP_LEFT].value.action);
    addScreenAction (s,
		     &rd->opt[ROTATE_DISPLAY_OPTION_FLIP_RIGHT].value.action);

    WRAP (rs, s, preparePaintScreen, rotatePreparePaintScreen);
    WRAP (rs, s, donePaintScreen, rotateDonePaintScreen);
    WRAP (rs, s, paintScreen, rotatePaintScreen);
    WRAP (rs, s, setScreenOptionForPlugin, rotateSetScreenOptionForPlugin);
    WRAP (rs, s, windowGrabNotify, rotateWindowGrabNotify);
    WRAP (rs, s, windowUngrabNotify, rotateWindowUngrabNotify);

    s->privates[rd->screenPrivateIndex].ptr = rs;

    rotateUpdateCubeOptions (s);

    return TRUE;
}

static void
rotateFiniScreen (CompPlugin *p,
		  CompScreen *s)
{
    ROTATE_SCREEN (s);

    UNWRAP (rs, s, preparePaintScreen);
    UNWRAP (rs, s, donePaintScreen);
    UNWRAP (rs, s, paintScreen);
    UNWRAP (rs, s, setScreenOptionForPlugin);
    UNWRAP (rs, s, windowGrabNotify);
    UNWRAP (rs, s, windowUngrabNotify);

    free (rs);
}

static Bool
rotateInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
rotateFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginDep rotateDeps[] = {
    { CompPluginRuleAfter, "cube" }
};

CompPluginVTable rotateVTable = {
    "rotate",
    N_("Rotate Cube"),
    N_("Rotate desktop cube"),
    rotateInit,
    rotateFini,
    rotateInitDisplay,
    rotateFiniDisplay,
    rotateInitScreen,
    rotateFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    rotateGetDisplayOptions,
    rotateSetDisplayOption,
    rotateGetScreenOptions,
    rotateSetScreenOption,
    rotateDeps,
    sizeof (rotateDeps) / sizeof (rotateDeps[0])
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &rotateVTable;
}
