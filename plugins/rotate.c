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
#include <X11/Xproto.h>

#include <compiz-cube.h>

static int cubeDisplayPrivateIndex;

#define ROTATE_POINTER_SENSITIVITY_FACTOR 0.05f

static CompMetadata rotateMetadata;

static int displayPrivateIndex;

#define ROTATE_DISPLAY_OPTION_INITIATE_BUTTON	  0
#define ROTATE_DISPLAY_OPTION_LEFT_KEY		  1
#define ROTATE_DISPLAY_OPTION_LEFT_BUTTON	  2
#define ROTATE_DISPLAY_OPTION_RIGHT_KEY		  3
#define ROTATE_DISPLAY_OPTION_RIGHT_BUTTON	  4
#define ROTATE_DISPLAY_OPTION_LEFT_WINDOW_KEY	  5
#define ROTATE_DISPLAY_OPTION_LEFT_WINDOW_BUTTON  6
#define ROTATE_DISPLAY_OPTION_RIGHT_WINDOW_KEY	  7
#define ROTATE_DISPLAY_OPTION_RIGHT_WINDOW_BUTTON 8
#define ROTATE_DISPLAY_OPTION_EDGEFLIP_POINTER	  9
#define ROTATE_DISPLAY_OPTION_EDGEFLIP_WINDOW	  10
#define ROTATE_DISPLAY_OPTION_EDGEFLIP_DND	  11
#define ROTATE_DISPLAY_OPTION_FLIPTIME		  12
#define ROTATE_DISPLAY_OPTION_TO_1_KEY		  13
#define ROTATE_DISPLAY_OPTION_TO_2_KEY		  14
#define ROTATE_DISPLAY_OPTION_TO_3_KEY		  15
#define ROTATE_DISPLAY_OPTION_TO_4_KEY		  16
#define ROTATE_DISPLAY_OPTION_TO_5_KEY		  17
#define ROTATE_DISPLAY_OPTION_TO_6_KEY		  18
#define ROTATE_DISPLAY_OPTION_TO_7_KEY		  19
#define ROTATE_DISPLAY_OPTION_TO_8_KEY		  20
#define ROTATE_DISPLAY_OPTION_TO_9_KEY		  21
#define ROTATE_DISPLAY_OPTION_TO_10_KEY		  22
#define ROTATE_DISPLAY_OPTION_TO_11_KEY		  23
#define ROTATE_DISPLAY_OPTION_TO_12_KEY		  24
#define ROTATE_DISPLAY_OPTION_TO_1_WINDOW_KEY	  25
#define ROTATE_DISPLAY_OPTION_TO_2_WINDOW_KEY	  26
#define ROTATE_DISPLAY_OPTION_TO_3_WINDOW_KEY	  27
#define ROTATE_DISPLAY_OPTION_TO_4_WINDOW_KEY	  28
#define ROTATE_DISPLAY_OPTION_TO_5_WINDOW_KEY	  29
#define ROTATE_DISPLAY_OPTION_TO_6_WINDOW_KEY	  30
#define ROTATE_DISPLAY_OPTION_TO_7_WINDOW_KEY	  31
#define ROTATE_DISPLAY_OPTION_TO_8_WINDOW_KEY	  32
#define ROTATE_DISPLAY_OPTION_TO_9_WINDOW_KEY	  33
#define ROTATE_DISPLAY_OPTION_TO_10_WINDOW_KEY	  34
#define ROTATE_DISPLAY_OPTION_TO_11_WINDOW_KEY	  35
#define ROTATE_DISPLAY_OPTION_TO_12_WINDOW_KEY	  36
#define ROTATE_DISPLAY_OPTION_TO_KEY		  37
#define ROTATE_DISPLAY_OPTION_WINDOW_KEY	  38
#define ROTATE_DISPLAY_OPTION_FLIP_LEFT_EDGE	  39
#define ROTATE_DISPLAY_OPTION_FLIP_RIGHT_EDGE	  40
#define ROTATE_DISPLAY_OPTION_RAISE_ON_ROTATE	  41
#define ROTATE_DISPLAY_OPTION_NUM		  42

typedef struct _RotateDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[ROTATE_DISPLAY_OPTION_NUM];
} RotateDisplay;

#define ROTATE_SCREEN_OPTION_POINTER_INVERT_Y	 0
#define ROTATE_SCREEN_OPTION_POINTER_SENSITIVITY 1
#define ROTATE_SCREEN_OPTION_ACCELERATION        2
#define ROTATE_SCREEN_OPTION_SNAP_TOP		 3
#define ROTATE_SCREEN_OPTION_SNAP_BOTTOM	 4
#define ROTATE_SCREEN_OPTION_SPEED		 5
#define ROTATE_SCREEN_OPTION_TIMESTEP		 6
#define ROTATE_SCREEN_OPTION_ZOOM		 7
#define ROTATE_SCREEN_OPTION_NUM		 8

typedef struct _RotateScreen {
    PreparePaintScreenProc	 preparePaintScreen;
    DonePaintScreenProc		 donePaintScreen;
    PaintOutputProc		 paintOutput;
    WindowGrabNotifyProc	 windowGrabNotify;
    WindowUngrabNotifyProc	 windowUngrabNotify;
    ActivateWindowProc           activateWindow;

    CubeGetRotationProc getRotation;

    CompOption opt[ROTATE_SCREEN_OPTION_NUM];

    float pointerSensitivity;

    Bool snapTop;
    Bool snapBottom;

    int grabIndex;

    GLfloat xrot, xVelocity;
    GLfloat yrot, yVelocity;

    GLfloat baseXrot;

    Bool    moving;
    GLfloat moveTo;

    Window moveWindow;
    int    moveWindowX;

    XPoint savedPointer;
    Bool   grabbed;
    Bool   focusDefault;

    CompTimeoutHandle rotateHandle;
    Bool	      slow;
    unsigned int      grabMask;
    CompWindow	      *grabWindow;

    float progress;
    float progressVelocity;

    GLfloat zoomTranslate;
} RotateScreen;

#define GET_ROTATE_DISPLAY(d)					    \
    ((RotateDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define ROTATE_DISPLAY(d)		       \
    RotateDisplay *rd = GET_ROTATE_DISPLAY (d)

#define GET_ROTATE_SCREEN(s, rd)					\
    ((RotateScreen *) (s)->base.privates[(rd)->screenPrivateIndex].ptr)

#define ROTATE_SCREEN(s)						      \
    RotateScreen *rs = GET_ROTATE_SCREEN (s, GET_ROTATE_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
rotateGetScreenOptions (CompPlugin *plugin,
			CompScreen *screen,
			int	   *count)
{
    ROTATE_SCREEN (screen);

    *count = NUM_OPTIONS (rs);
    return rs->opt;
}

static Bool
rotateSetScreenOption (CompPlugin      *plugin,
		       CompScreen      *screen,
		       const char      *name,
		       CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    ROTATE_SCREEN (screen);

    o = compFindOption (rs->opt, NUM_OPTIONS (rs), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case ROTATE_SCREEN_OPTION_POINTER_SENSITIVITY:
	if (compSetFloatOption (o, value))
	{
	    rs->pointerSensitivity = o->value.f *
		ROTATE_POINTER_SENSITIVITY_FACTOR;
	    return TRUE;
	}
	break;
    default:
	return compSetScreenOption (screen, o, value);
    }

    return FALSE;
}

static int
adjustVelocity (RotateScreen *rs,
		int	     size,
		int	     invert)
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

    adjust = -xrot * 0.05f * rs->opt[ROTATE_SCREEN_OPTION_ACCELERATION].value.f;
    amount = fabs (xrot);
    if (amount < 10.0f)
	amount = 10.0f;
    else if (amount > 30.0f)
	amount = 30.0f;

    if (rs->slow)
	adjust *= 0.05f;

    rs->xVelocity = (amount * rs->xVelocity + adjust) / (amount + 2.0f);

    yrot = rs->yrot;
    /* Only snap if more than 2 viewports */
    if (size > 2)
    {
	if (rs->yrot > 50.0f && ((rs->snapTop && invert == 1) ||
				 (rs->snapBottom && invert != 1)))
	    yrot -= 90.f;
	else if (rs->yrot < -50.0f && ((rs->snapTop && invert != 1) ||
				       (rs->snapBottom && invert == 1)))
	    yrot += 90.f;
    }

    adjust = -yrot * 0.05f * rs->opt[ROTATE_SCREEN_OPTION_ACCELERATION].value.f;
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
rotateReleaseMoveWindow (CompScreen *s)
{
    CompWindow *w;

    ROTATE_SCREEN (s);

    w = findWindowAtScreen (s, rs->moveWindow);
    if (w)
	syncWindowPosition (w);

    rs->moveWindow = None;
}

static void
rotatePreparePaintScreen (CompScreen *s,
			  int	     msSinceLastPaint)
{
    ROTATE_SCREEN (s);
    CUBE_SCREEN (s);

    float oldXrot = rs->xrot + rs->baseXrot;

    if (rs->grabIndex || rs->moving)
    {
	int   steps;
	float amount, chunk;

	amount = msSinceLastPaint * 0.05f *
	    rs->opt[ROTATE_SCREEN_OPTION_SPEED].value.f;
	steps  = amount /
	    (0.5f * rs->opt[ROTATE_SCREEN_OPTION_TIMESTEP].value.f);
	if (!steps) steps = 1;
	chunk  = amount / (float) steps;

	while (steps--)
	{
	    rs->xrot += rs->xVelocity * chunk;
	    rs->yrot += rs->yVelocity * chunk;

	    if (rs->xrot > 360.0f / s->hsize)
	    {
		rs->baseXrot += 360.0f / s->hsize;
		rs->xrot -= 360.0f / s->hsize;
	    }
	    else if (rs->xrot < 0.0f)
	    {
		rs->baseXrot -= 360.0f / s->hsize;
		rs->xrot += 360.0f / s->hsize;
	    }

	    if (cs->invert == -1)
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
	    else if (adjustVelocity (rs, s->hsize, cs->invert))
	    {
		rs->xVelocity = 0.0f;
		rs->yVelocity = 0.0f;

		if (fabs (rs->yrot) < 0.1f)
		{
		    float xrot;
		    int   tx;

		    xrot = rs->baseXrot + rs->xrot;
		    if (xrot < 0.0f)
			tx = (s->hsize * xrot / 360.0f) - 0.5f;
		    else
			tx = (s->hsize * xrot / 360.0f) + 0.5f;

		    /* flag end of rotation */
		    cs->rotationState = RotationNone;

		    moveScreenViewport (s, tx, 0, TRUE);

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
			    moveWindow (w, rs->moveWindowX - w->attrib.x, 0,
					TRUE, TRUE);
			    syncWindowPosition (w);
			}
		    }
		    else if (rs->focusDefault)
		    {
			int i;

			for (i = 0; i < s->maxGrab; i++)
			    if (s->grabs[i].active &&
				strcmp ("switcher", s->grabs[i].name) == 0)
				break;

			/* only focus default window if switcher isn't active */
			if (i == s->maxGrab)
			    focusDefaultWindow (s);
		    }

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
		float xrot = (s->hsize * (rs->baseXrot + rs->xrot)) / 360.0f;

		moveWindowToViewportPosition (w,
					      rs->moveWindowX - xrot * s->width,
					      w->attrib.y,
					      FALSE);
	    }
	}
    }

    if (rs->moving)
    {
	if (fabs (rs->xrot + rs->baseXrot + rs->moveTo) <=
	    (360.0 / (s->hsize * 2.0)))
	{
	    rs->progress = fabs (rs->xrot + rs->baseXrot + rs->moveTo) /
			   (360.0 / (s->hsize * 2.0));
	}
	else if (fabs (rs->xrot + rs->baseXrot) <= (360.0 / (s->hsize * 2.0)))
	{
	    rs->progress = fabs (rs->xrot + rs->baseXrot) /
			   (360.0 / (s->hsize * 2.0));
	}
	else
	{
	    rs->progress += fabs (rs->xrot + rs->baseXrot - oldXrot) /
			    (360.0 / (s->hsize * 2.0));
	    rs->progress = MIN (rs->progress, 1.0);
	}
    }
    else if (rs->progress != 0.0f || rs->grabbed)
    {
	int   steps;
	float amount, chunk;

	amount = msSinceLastPaint * 0.05f *
	    rs->opt[ROTATE_SCREEN_OPTION_SPEED].value.f;
	steps = amount /
	    (0.5f * rs->opt[ROTATE_SCREEN_OPTION_TIMESTEP].value.f);
	if (!steps)
	    steps = 1;

	chunk = amount / (float) steps;

	while (steps--)
	{
	    float dt, adjust, tamount;

	    if (rs->grabbed)
		dt = 1.0 - rs->progress;
	    else
		dt = 0.0f - rs->progress;

	    adjust = dt * 0.15f;
	    tamount = fabs (dt) * 1.5f;
	    if (tamount < 0.2f)
		tamount = 0.2f;
	    else if (tamount > 2.0f)
		tamount = 2.0f;

	    rs->progressVelocity = (tamount * rs->progressVelocity + adjust) /
				   (tamount + 1.0f);

	    rs->progress += rs->progressVelocity * chunk;

	    if (fabs (dt) < 0.01f && fabs (rs->progressVelocity) < 0.0001f)
	    {
		if (rs->grabbed)
		    rs->progress = 1.0f;
		else
		    rs->progress = 0.0f;

		break;
	    }
	}
    }

    if (cs->invert == 1 && !cs->unfolded)
    {
	rs->zoomTranslate = rs->opt[ROTATE_SCREEN_OPTION_ZOOM].value.f *
			    rs->progress;
    }
    else
    {
	rs->zoomTranslate = 0.0;
    }

    UNWRAP (rs, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (rs, s, preparePaintScreen, rotatePreparePaintScreen);
}

static void
rotateDonePaintScreen (CompScreen *s)
{
    ROTATE_SCREEN (s);

    if (rs->grabIndex || rs->moving ||
	(rs->progress != 0.0 && rs->progress != 1.0))
    {
	if ((!rs->grabbed && !rs->snapTop && !rs->snapBottom) ||
	    rs->xVelocity || rs->yVelocity || rs->progressVelocity)
	{
	    damageScreen (s);
	}
    }

    UNWRAP (rs, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (rs, s, donePaintScreen, rotateDonePaintScreen);
}

static void
rotateGetRotation (CompScreen *s,
		   float      *x,
		   float      *v,
		   float      *progress)
{
    CUBE_SCREEN (s);
    ROTATE_SCREEN (s);

    UNWRAP (rs, cs, getRotation);
    (*cs->getRotation) (s, x, v, progress);
    WRAP (rs, cs, getRotation, rotateGetRotation);

    *x += rs->baseXrot + rs->xrot;
    *v += rs->yrot;
    *progress = MAX (*progress, rs->progress);
}

static Bool
rotatePaintOutput (CompScreen		   *s,
		   const ScreenPaintAttrib *sAttrib,
		   const CompTransform	   *transform,
		   Region		   region,
		   CompOutput		   *output,
		   unsigned int		   mask)
{
    Bool status;

    ROTATE_SCREEN (s);

    if (rs->grabIndex || rs->moving || rs->progress != 0.0f)
    {
	CompTransform sTransform = *transform;

	matrixTranslate (&sTransform, 0.0f, 0.0f, -rs->zoomTranslate);

	mask &= ~PAINT_SCREEN_REGION_MASK;
	mask |= PAINT_SCREEN_TRANSFORMED_MASK;

	UNWRAP (rs, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, &sTransform, region,
				    output, mask);
	WRAP (rs, s, paintOutput, rotatePaintOutput);
    }
    else
    {
	UNWRAP (rs, s, paintOutput);
	status = (*s->paintOutput) (s, sAttrib, transform, region,
				    output, mask);
	WRAP (rs, s, paintOutput, rotatePaintOutput);
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
	CUBE_SCREEN (s);

	if (s->hsize < 2)
	    return FALSE;

	if (rs->rotateHandle && rs->grabWindow)
	{
	    if (otherScreenGrabExist (s, "rotate", "move", NULL))
		return FALSE;
	}
	else
	{
	    if (otherScreenGrabExist (s, "rotate", "switcher", "cube", NULL))
		return FALSE;
	}

	rs->moving = FALSE;
	rs->slow   = FALSE;

	/* Set the rotation state for cube - if action is non-NULL,
	   we set it to manual (as we were called from the 'Initiate
	   Rotation' binding. Otherwise, we set it to Change. */
	if (action)
	    cs->rotationState = RotationManual;
	else
	    cs->rotationState = RotationChange;

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
	    rs->snapBottom = rs->opt[ROTATE_SCREEN_OPTION_SNAP_BOTTOM].value.b;

	    if (state & CompActionStateInitButton)
		action->state |= CompActionStateTermButton;

	    if (state & CompActionStateInitKey)
		action->state |= CompActionStateTermKey;
	}
    }

    return TRUE;
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
	    if (!xid)
	    {
		rs->snapTop = FALSE;
		rs->snapBottom = FALSE;
	    }

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

	if (s->hsize < 2)
	    return FALSE;

	if (otherScreenGrabExist (s, "rotate", "move", "switcher",
				  "group-drag", "cube", NULL))
	    return FALSE;

	direction = getIntOptionNamed (option, nOption, "direction", 0);
	if (!direction)
	    return FALSE;

	if (rs->moveWindow)
	    rotateReleaseMoveWindow (s);

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

	rs->focusDefault = getBoolOptionNamed (option, nOption,
					       "focus_default", TRUE);
	rs->moving  = TRUE;
	rs->moveTo += (360.0f / s->hsize) * direction;
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

    ROTATE_DISPLAY (d);

    xid = getIntOptionNamed (option, nOption, "root", 0);

    s = findScreenAtDisplay (d, xid);
    if (s)
    {
	Bool raise = rd->opt[ROTATE_DISPLAY_OPTION_RAISE_ON_ROTATE].value.b;
	int  direction;

	ROTATE_SCREEN (s);

	if (s->hsize < 2)
	    return FALSE;

	direction = getIntOptionNamed (option, nOption, "direction", 0);
	if (!direction)
	    return FALSE;

	xid = getIntOptionNamed (option, nOption, "window", 0);

	if (rs->moveWindow != xid)
	{
	    CompWindow *w;

	    rotateReleaseMoveWindow (s);

	    if (!rs->grabIndex && !rs->moving)
	    {
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

			    if (raise)
				raiseWindow (w);
			}
		    }
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
	    rs->moveTo += (360.0f / s->hsize) * direction;
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

    if (otherScreenGrabExist (s, "rotate", "move", "group-drag", NULL))
	return FALSE;

    warpX = pointerX + s->width;
    warpPointer (s, s->width - 10, 0);
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

    if (otherScreenGrabExist (s, "rotate", "move", "group-drag", NULL))
	return FALSE;

    warpX = pointerX - s->width;
    warpPointer (s, 10 - s->width, 0);
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

    if (s->hsize < 2)
	return;

    if (otherScreenGrabExist (s, "rotate", "move", "group-drag", NULL))
	return;

    if (state & CompActionStateInitEdgeDnd)
    {
	if (!rd->opt[ROTATE_DISPLAY_OPTION_EDGEFLIP_DND].value.b)
	    return;

	if (otherScreenGrabExist (s, "rotate", NULL))
	    return;
    }
    else if (otherScreenGrabExist (s, "rotate", "group-drag", NULL))
    {
	ROTATE_SCREEN (s);

	if (!rd->opt[ROTATE_DISPLAY_OPTION_EDGEFLIP_WINDOW].value.b)
	    return;

	if (!rs->grabWindow)
	    return;

	/* bail out if window is horizontally maximized, fullscreen,
	   or sticky */
	if (rs->grabWindow->state & (CompWindowStateMaximizedHorzMask |
				     CompWindowStateFullscreenMask |
				     CompWindowStateStickyMask))
	    return;
    }
    else if (otherScreenGrabExist (s, "rotate", NULL))
    {
	/* in that case, 'group-drag' must be the active screen grab */
	if (!rd->opt[ROTATE_DISPLAY_OPTION_EDGEFLIP_WINDOW].value.b)
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
	int flipTime = rd->opt[ROTATE_DISPLAY_OPTION_FLIPTIME].value.i;

	ROTATE_SCREEN (s);

	if (flipTime == 0 || (rs->moving && !rs->slow))
	{
	    int pointerDx = pointerX - lastPointerX;
	    int warpX;

	    warpX = pointerX + s->width;
	    warpPointer (s, s->width - 10, 0);
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
	    {
		int flipTime = rd->opt[ROTATE_DISPLAY_OPTION_FLIPTIME].value.i;

		rs->rotateHandle = compAddTimeout (flipTime,
						   (float) flipTime * 1.2,
						   rotateFlipLeft, s);
	    }

	    rs->moving  = TRUE;
	    rs->moveTo -= 360.0f / s->hsize;
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
	int flipTime = rd->opt[ROTATE_DISPLAY_OPTION_FLIPTIME].value.i;

	ROTATE_SCREEN (s);

	if (flipTime == 0 || (rs->moving && !rs->slow))
	{
	    int pointerDx = pointerX - lastPointerX;
	    int warpX;

	    warpX = pointerX - s->width;
	    warpPointer (s, 10 - s->width, 0);
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
	    {
		int flipTime = rd->opt[ROTATE_DISPLAY_OPTION_FLIPTIME].value.i;

		rs->rotateHandle =
		    compAddTimeout (flipTime, (float) flipTime * 1.2,
				    rotateFlipRight, s);
	    }

	    rs->moving  = TRUE;
	    rs->moveTo += 360.0f / s->hsize;
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

    delta = face - s->x - (rs->moveTo / (360.0f / s->hsize));
    if (delta > s->hsize / 2)
	delta -= s->hsize;
    else if (delta < -(s->hsize / 2))
	delta += s->hsize;

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
	int	   i = ROTATE_DISPLAY_OPTION_TO_1_KEY;

	ROTATE_DISPLAY (s->display);

	while (i <= ROTATE_DISPLAY_OPTION_TO_12_KEY)
	{
	    if (action == &rd->opt[i].value.action)
	    {
		face = i - ROTATE_DISPLAY_OPTION_TO_1_KEY;
		break;
	    }

	    i++;
	}

	if (face < 0)
	    face = getIntOptionNamed (option, nOption, "face", s->x);

	if (face > s->hsize)
	    return FALSE;

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
	int	   i = ROTATE_DISPLAY_OPTION_TO_1_WINDOW_KEY;

	ROTATE_DISPLAY (s->display);

	while (i <= ROTATE_DISPLAY_OPTION_TO_12_WINDOW_KEY)
	{
	    if (action == &rd->opt[i].value.action)
	    {
		face = i - ROTATE_DISPLAY_OPTION_TO_1_WINDOW_KEY;
		break;
	    }

	    i++;
	}

	if (face < 0)
	    face = getIntOptionNamed (option, nOption, "face", s->x);

	if (face > s->hsize)
	    return FALSE;

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

	rotateWithWindow (d, NULL, 0, o, 5);
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
	    CUBE_SCREEN (s);

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
			warpPointer (s,
				     (s->width  / 2) - pointerX,
				     (s->height / 2) - pointerY);
		    }

		    if (rs->opt[ROTATE_SCREEN_OPTION_POINTER_INVERT_Y].value.b)
			pointerDy = -pointerDy;

		    rs->xVelocity += pointerDx * rs->pointerSensitivity *
			cs->invert;
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
	if (event->xclient.message_type == d->desktopViewportAtom)
	{
	    s = findScreenAtDisplay (d, event->xclient.window);
	    if (s)
	    {
		int dx;

		ROTATE_SCREEN (s);

		if (otherScreenGrabExist (s, "rotate", "switcher", "cube", NULL))
		    break;

		/* reset movement */
		rs->moveTo = 0.0f;

		dx = event->xclient.data.l[0] / s->width - s->x;
		if (dx)
		{
		    Window	 win;
		    int		 i, x, y;
		    unsigned int ui;
		    CompOption   o[4];

		    XQueryPointer (d->display, s->root,
				   &win, &win, &x, &y, &i, &i, &ui);

		    if (dx * 2 > s->hsize)
			dx -= s->hsize;
		    else if (dx * 2 < -s->hsize)
			dx += s->hsize;

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
rotateActivateWindow (CompWindow *w)
{
    CompScreen *s = w->screen;

    ROTATE_SCREEN (s);

    if (w->placed &&
	!otherScreenGrabExist (s, "rotate", "switcher", "cube", NULL))
    {
	int dx;

	/* reset movement */
	rs->moveTo = 0.0f;

	defaultViewportForWindow (w, &dx, NULL);
	dx -= s->x;
	if (dx)
	{
	    Window	 win;
	    int		 i, x, y;
	    unsigned int ui;
	    CompOption   o[5];

	    XQueryPointer (s->display->display, s->root,
			   &win, &win, &x, &y, &i, &i, &ui);

	    if (dx * 2 > s->hsize)
		dx -= s->hsize;
	    else if (dx * 2 < -s->hsize)
		dx += s->hsize;

	    o[0].type    = CompOptionTypeInt;
	    o[0].name    = "x";
	    o[0].value.i = x;

	    o[1].type    = CompOptionTypeInt;
	    o[1].name    = "y";
	    o[1].value.i = y;

	    o[2].type    = CompOptionTypeInt;
	    o[2].name    = "root";
	    o[2].value.i = s->root;

	    o[3].type    = CompOptionTypeInt;
	    o[3].name    = "direction";
	    o[3].value.i = dx;

	    o[4].type    = CompOptionTypeBool;
	    o[4].name    = "focus_default";
	    o[4].value.b = FALSE;

	    rotate (s->display, NULL, 0, o, 5);
	}
    }

    UNWRAP (rs, s, activateWindow);
    (*s->activateWindow) (w);
    WRAP (rs, s, activateWindow, rotateActivateWindow);
}

static void
rotateWindowGrabNotify (CompWindow   *w,
			int	     x,
			int	     y,
			unsigned int state,
			unsigned int mask)
{
    ROTATE_SCREEN (w->screen);

    if (!rs->grabWindow)
    {
	rs->grabMask   = mask;
	rs->grabWindow = w;
    }

    UNWRAP (rs, w->screen, windowGrabNotify);
    (*w->screen->windowGrabNotify) (w, x, y, state, mask);
    WRAP (rs, w->screen, windowGrabNotify, rotateWindowGrabNotify);
}

static void
rotateWindowUngrabNotify (CompWindow *w)
{
    ROTATE_SCREEN (w->screen);

    if (w == rs->grabWindow)
    {
	rs->grabMask   = 0;
	rs->grabWindow = NULL;
    }

    UNWRAP (rs, w->screen, windowUngrabNotify);
    (*w->screen->windowUngrabNotify) (w);
    WRAP (rs, w->screen, windowUngrabNotify, rotateWindowUngrabNotify);
}

static CompOption *
rotateGetDisplayOptions (CompPlugin  *plugin,
			 CompDisplay *display,
			 int	     *count)
{
    ROTATE_DISPLAY (display);

    *count = NUM_OPTIONS (rd);
    return rd->opt;
}

static Bool
rotateSetDisplayOption (CompPlugin      *plugin,
			CompDisplay     *display,
			const char	*name,
			CompOptionValue *value)
{
    CompOption *o;

    ROTATE_DISPLAY (display);

    o = compFindOption (rd->opt, NUM_OPTIONS (rd), name, NULL);
    if (!o)
	return FALSE;

    return compSetDisplayOption (display, o, value);
}

static const CompMetadataOptionInfo rotateDisplayOptionInfo[] = {
    { "initiate_button", "button", 0, rotateInitiate, rotateTerminate },
    { "rotate_left_key", "key", 0, rotateLeft, 0 },
    { "rotate_left_button", "button", 0, rotateLeft, 0 },
    { "rotate_right_key", "key", 0, rotateRight, 0 },
    { "rotate_right_button", "button", 0, rotateRight, 0 },
    { "rotate_left_window_key", "key", 0, rotateLeftWithWindow, 0 },
    { "rotate_left_window_button", "button", 0, rotateLeftWithWindow, 0 },
    { "rotate_right_window_key", "key", 0, rotateRightWithWindow, 0 },
    { "rotate_right_window_button", "button", 0, rotateRightWithWindow, 0 },
    { "edge_flip_pointer", "bool", 0, 0, 0 },
    { "edge_flip_window", "bool", 0, 0, 0 },
    { "edge_flip_dnd", "bool", 0, 0, 0 },
    { "flip_time", "int", "<min>0</min><max>1000</max>", 0, 0 },
    { "rotate_to_1_key", "key", 0, rotateTo, 0 },
    { "rotate_to_2_key", "key", 0, rotateTo, 0 },
    { "rotate_to_3_key", "key", 0, rotateTo, 0 },
    { "rotate_to_4_key", "key", 0, rotateTo, 0 },
    { "rotate_to_5_key", "key", 0, rotateTo, 0 },
    { "rotate_to_6_key", "key", 0, rotateTo, 0 },
    { "rotate_to_7_key", "key", 0, rotateTo, 0 },
    { "rotate_to_8_key", "key", 0, rotateTo, 0 },
    { "rotate_to_9_key", "key", 0, rotateTo, 0 },
    { "rotate_to_10_key", "key", 0, rotateTo, 0 },
    { "rotate_to_11_key", "key", 0, rotateTo, 0 },
    { "rotate_to_12_key", "key", 0, rotateTo, 0 },
    { "rotate_to_1_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_2_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_3_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_4_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_5_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_6_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_7_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_8_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_9_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_10_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_11_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_12_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_to_key", "key", 0, rotateTo, 0 },
    { "rotate_window_key", "key", 0, rotateToWithWindow, 0 },
    { "rotate_flip_left_edge", "edge", 0, rotateEdgeFlipLeft,
      rotateFlipTerminate },
    { "rotate_flip_right_edge", "edge", 0, rotateEdgeFlipRight,
      rotateFlipTerminate },
    { "raise_on_rotate", "bool", 0, 0, 0 }
};

static Bool
rotateInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    RotateDisplay *rd;

    if (!checkPluginABI ("core", CORE_ABIVERSION) ||
	!checkPluginABI ("cube", CUBE_ABIVERSION))
	return FALSE;

    if (!getPluginDisplayIndex (d, "cube", &cubeDisplayPrivateIndex))
	return FALSE;

    rd = malloc (sizeof (RotateDisplay));
    if (!rd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &rotateMetadata,
					     rotateDisplayOptionInfo,
					     rd->opt,
					     ROTATE_DISPLAY_OPTION_NUM))
    {
	free (rd);
	return FALSE;
    }

    rd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (rd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, rd->opt, ROTATE_DISPLAY_OPTION_NUM);
	free (rd);
	return FALSE;
    }

    WRAP (rd, d, handleEvent, rotateHandleEvent);

    d->base.privates[displayPrivateIndex].ptr = rd;

    return TRUE;
}

static void
rotateFiniDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    ROTATE_DISPLAY (d);

    freeScreenPrivateIndex (d, rd->screenPrivateIndex);

    UNWRAP (rd, d, handleEvent);

    compFiniDisplayOptions (d, rd->opt, ROTATE_DISPLAY_OPTION_NUM);

    free (rd);
}

static const CompMetadataOptionInfo rotateScreenOptionInfo[] = {
    { "invert_y", "bool", 0, 0, 0 },
    { "sensitivity", "float", 0, 0, 0 },
    { "acceleration", "float", "<min>1.0</min>", 0, 0 },
    { "snap_top", "bool", 0, 0, 0 },
    { "snap_bottom", "bool", 0, 0, 0 },
    { "speed", "float", "<min>0.1</min>", 0, 0 },
    { "timestep", "float", "<min>0.1</min>", 0, 0 },
    { "zoom", "float", 0, 0, 0 }
};

static Bool
rotateInitScreen (CompPlugin *p,
		  CompScreen *s)
{
    RotateScreen *rs;

    ROTATE_DISPLAY (s->display);
    CUBE_SCREEN (s);

    rs = malloc (sizeof (RotateScreen));
    if (!rs)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &rotateMetadata,
					    rotateScreenOptionInfo,
					    rs->opt,
					    ROTATE_SCREEN_OPTION_NUM))
    {
	free (rs);
	return FALSE;
    }

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

    rs->focusDefault = TRUE;
    rs->grabbed	     = FALSE;
    rs->snapTop	     = FALSE;
    rs->snapBottom   = FALSE;

    rs->slow       = FALSE;
    rs->grabMask   = FALSE;
    rs->grabWindow = NULL;

    rs->pointerSensitivity =
	rs->opt[ROTATE_SCREEN_OPTION_POINTER_SENSITIVITY].value.f *
	ROTATE_POINTER_SENSITIVITY_FACTOR;

    rs->rotateHandle = 0;

    rs->progress          = 0.0;
    rs->progressVelocity  = 0.0;

    rs->zoomTranslate = 0.0;

    WRAP (rs, s, preparePaintScreen, rotatePreparePaintScreen);
    WRAP (rs, s, donePaintScreen, rotateDonePaintScreen);
    WRAP (rs, s, paintOutput, rotatePaintOutput);
    WRAP (rs, s, windowGrabNotify, rotateWindowGrabNotify);
    WRAP (rs, s, windowUngrabNotify, rotateWindowUngrabNotify);
    WRAP (rs, s, activateWindow, rotateActivateWindow);

    WRAP (rs, cs, getRotation, rotateGetRotation);

    s->base.privates[rd->screenPrivateIndex].ptr = rs;

    return TRUE;
}

static void
rotateFiniScreen (CompPlugin *p,
		  CompScreen *s)
{
    CUBE_SCREEN (s);
    ROTATE_SCREEN (s);

    if (rs->rotateHandle)
	compRemoveTimeout (rs->rotateHandle);

    UNWRAP (rs, cs, getRotation);

    UNWRAP (rs, s, preparePaintScreen);
    UNWRAP (rs, s, donePaintScreen);
    UNWRAP (rs, s, paintOutput);
    UNWRAP (rs, s, windowGrabNotify);
    UNWRAP (rs, s, windowUngrabNotify);
    UNWRAP (rs, s, activateWindow);

    compFiniScreenOptions (s, rs->opt, ROTATE_SCREEN_OPTION_NUM);

    free (rs);
}
static CompBool
rotateInitObject (CompPlugin *p,
		  CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) rotateInitDisplay,
	(InitPluginObjectProc) rotateInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
rotateFiniObject (CompPlugin *p,
		  CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) rotateFiniDisplay,
	(FiniPluginObjectProc) rotateFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
rotateGetObjectOptions (CompPlugin *plugin,
			CompObject *object,
			int	   *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) rotateGetDisplayOptions,
	(GetPluginObjectOptionsProc) rotateGetScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
rotateSetObjectOption (CompPlugin      *plugin,
		       CompObject      *object,
		       const char      *name,
		       CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) rotateSetDisplayOption,
	(SetPluginObjectOptionProc) rotateSetScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
rotateInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&rotateMetadata,
					 p->vTable->name,
					 rotateDisplayOptionInfo,
					 ROTATE_DISPLAY_OPTION_NUM,
					 rotateScreenOptionInfo,
					 ROTATE_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&rotateMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&rotateMetadata, p->vTable->name);

    return TRUE;
}

static void
rotateFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&rotateMetadata);
}

static CompMetadata *
rotateGetMetadata (CompPlugin *plugin)
{
    return &rotateMetadata;
}

CompPluginVTable rotateVTable = {
    "rotate",
    rotateGetMetadata,
    rotateInit,
    rotateFini,
    rotateInitObject,
    rotateFiniObject,
    rotateGetObjectOptions,
    rotateSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &rotateVTable;
}
