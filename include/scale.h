/*
 * Copyright © 2007 Novell, Inc.
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

#include <compiz.h>

#define SCALE_ABIVERSION 20070605

#define SCALE_STATE_NONE 0
#define SCALE_STATE_OUT  1
#define SCALE_STATE_WAIT 2
#define SCALE_STATE_IN   3

#define SCALE_ICON_NONE   0
#define SCALE_ICON_EMBLEM 1
#define SCALE_ICON_BIG    2
#define SCALE_ICON_LAST   SCALE_ICON_BIG

typedef struct _ScaleSlot {
    int   x1, y1, x2, y2;
    int   filled;
    float scale;
} ScaleSlot;

#define SCALE_DISPLAY_OPTION_ABI	     0
#define SCALE_DISPLAY_OPTION_INDEX	     1
#define SCALE_DISPLAY_OPTION_INITIATE        2
#define SCALE_DISPLAY_OPTION_INITIATE_ALL    3
#define SCALE_DISPLAY_OPTION_INITIATE_GROUP  4
#define SCALE_DISPLAY_OPTION_INITIATE_OUTPUT 5
#define SCALE_DISPLAY_OPTION_SHOW_DESKTOP    6
#define SCALE_DISPLAY_OPTION_NUM             7

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
#define SCALE_SCREEN_OPTION_WINDOW_MATCH 3
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

typedef Bool (*ScaleLayoutSlotsAndAssignWindowsProc) (CompScreen *s);

typedef void (*ScalePaintDecorationProc) (CompWindow		  *w,
					  const WindowPaintAttrib *attrib,
					  const CompTransform     *transform,
					  Region		  region,
					  unsigned int		  mask);

typedef struct _ScaleScreen {
    int windowPrivateIndex;

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc    donePaintScreen;
    PaintOutputProc        paintOutput;
    PaintWindowProc        paintWindow;
    DamageWindowRectProc   damageWindowRect;

    ScaleLayoutSlotsAndAssignWindowsProc layoutSlotsAndAssignWindows;
    ScalePaintDecorationProc		 scalePaintDecoration;

    CompOption opt[SCALE_SCREEN_OPTION_NUM];

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

    GLushort opacity;

    ScaleType type;

    Window clientLeader;

    CompMatch match;
    CompMatch *currentMatch;
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

