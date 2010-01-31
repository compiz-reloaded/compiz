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

/*
 * Spring model implemented by Kristian Hogsberg.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <compiz-core.h>

#define WIN_X(w) ((w)->attrib.x - (w)->output.left)
#define WIN_Y(w) ((w)->attrib.y - (w)->output.top)
#define WIN_W(w) ((w)->width + (w)->output.left + (w)->output.right)
#define WIN_H(w) ((w)->height + (w)->output.top + (w)->output.bottom)

#define GRID_WIDTH  4
#define GRID_HEIGHT 4

#define MODEL_MAX_SPRINGS (GRID_WIDTH * GRID_HEIGHT * 2)

#define MASS 15.0f

typedef struct _xy_pair {
    float x, y;
} Point, Vector;

#define NorthEdgeMask (1L << 0)
#define SouthEdgeMask (1L << 1)
#define WestEdgeMask  (1L << 2)
#define EastEdgeMask  (1L << 3)

#define EDGE_DISTANCE 25.0f
#define EDGE_VELOCITY 13.0f

typedef struct _Edge {
    float next, prev;

    float start;
    float end;

    float attract;
    float velocity;

    Bool  snapped;
} Edge;

typedef struct _Object {
    Vector	 force;
    Point	 position;
    Vector	 velocity;
    float	 theta;
    Bool	 immobile;
    unsigned int edgeMask;
    Edge	 vertEdge;
    Edge	 horzEdge;
} Object;

typedef struct _Spring {
    Object *a;
    Object *b;
    Vector offset;
} Spring;

#define NORTH 0
#define SOUTH 1
#define WEST  2
#define EAST  3

typedef struct _Model {
    Object	 *objects;
    int		 numObjects;
    Spring	 springs[MODEL_MAX_SPRINGS];
    int		 numSprings;
    Object	 *anchorObject;
    float	 steps;
    Point	 topLeft;
    Point	 bottomRight;
    unsigned int edgeMask;
    unsigned int snapCnt[4];
} Model;

#define WOBBLY_EFFECT_NONE   0
#define WOBBLY_EFFECT_SHIVER 1
#define WOBBLY_EFFECT_LAST   WOBBLY_EFFECT_SHIVER

static CompMetadata wobblyMetadata;

static int displayPrivateIndex;

#define WOBBLY_DISPLAY_OPTION_SNAP_KEY      0
#define WOBBLY_DISPLAY_OPTION_SNAP_INVERTED 1
#define WOBBLY_DISPLAY_OPTION_SHIVER        2
#define WOBBLY_DISPLAY_OPTION_NUM           3

typedef struct _WobblyDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;

    CompOption opt[WOBBLY_DISPLAY_OPTION_NUM];

    Bool snapping;

    Bool yConstrained;
} WobblyDisplay;

#define WOBBLY_SCREEN_OPTION_FRICTION	        0
#define WOBBLY_SCREEN_OPTION_SPRING_K	        1
#define WOBBLY_SCREEN_OPTION_GRID_RESOLUTION    2
#define WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE      3
#define WOBBLY_SCREEN_OPTION_MAP_EFFECT	        4
#define WOBBLY_SCREEN_OPTION_FOCUS_EFFECT       5
#define WOBBLY_SCREEN_OPTION_MAP_WINDOW_MATCH   6
#define WOBBLY_SCREEN_OPTION_FOCUS_WINDOW_MATCH 7
#define WOBBLY_SCREEN_OPTION_GRAB_WINDOW_MATCH  8
#define WOBBLY_SCREEN_OPTION_MOVE_WINDOW_MATCH  9
#define WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT    10
#define WOBBLY_SCREEN_OPTION_NUM	        11

typedef struct _WobblyScreen {
    int	windowPrivateIndex;

    PreparePaintScreenProc preparePaintScreen;
    DonePaintScreenProc	   donePaintScreen;
    PaintOutputProc	   paintOutput;
    PaintWindowProc	   paintWindow;
    DamageWindowRectProc   damageWindowRect;
    AddWindowGeometryProc  addWindowGeometry;

    WindowResizeNotifyProc windowResizeNotify;
    WindowMoveNotifyProc   windowMoveNotify;
    WindowGrabNotifyProc   windowGrabNotify;
    WindowUngrabNotifyProc windowUngrabNotify;

    CompOption opt[WOBBLY_SCREEN_OPTION_NUM];

    Bool wobblyWindows;

    unsigned int grabMask;
    CompWindow	 *grabWindow;
    Bool         moveWindow;

    const XRectangle *grabWindowWorkArea;
} WobblyScreen;

#define WobblyInitial  (1L << 0)
#define WobblyForce    (1L << 1)
#define WobblyVelocity (1L << 2)

typedef struct _WobblyWindow {
    Model	 *model;
    int		 wobbly;
    Bool	 grabbed;
    Bool	 velocity;
    unsigned int state;
} WobblyWindow;

#define GET_WOBBLY_DISPLAY(d)					    \
    ((WobblyDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define WOBBLY_DISPLAY(d)		       \
    WobblyDisplay *wd = GET_WOBBLY_DISPLAY (d)

#define GET_WOBBLY_SCREEN(s, wd)					\
    ((WobblyScreen *) (s)->base.privates[(wd)->screenPrivateIndex].ptr)

#define WOBBLY_SCREEN(s)						      \
    WobblyScreen *ws = GET_WOBBLY_SCREEN (s, GET_WOBBLY_DISPLAY (s->display))

#define GET_WOBBLY_WINDOW(w, ws)					\
    ((WobblyWindow *) (w)->base.privates[(ws)->windowPrivateIndex].ptr)

#define WOBBLY_WINDOW(w)				         \
    WobblyWindow *ww = GET_WOBBLY_WINDOW  (w,		         \
		       GET_WOBBLY_SCREEN  (w->screen,	         \
		       GET_WOBBLY_DISPLAY (w->screen->display)))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
wobblyGetScreenOptions (CompPlugin *plugin,
			CompScreen *screen,
			int	   *count)
{
    WOBBLY_SCREEN (screen);

    *count = NUM_OPTIONS (ws);
    return ws->opt;
}

static Bool
wobblySetScreenOption (CompPlugin      *plugin,
		       CompScreen      *screen,
		       const char      *name,
		       CompOptionValue *value)
{
    CompOption *o;

    WOBBLY_SCREEN (screen);

    o = compFindOption (ws->opt, NUM_OPTIONS (ws), name, NULL);
    if (!o)
	return FALSE;

    return compSetScreenOption (screen, o, value);
}

static const CompMetadataOptionInfo wobblyScreenOptionInfo[] = {
    { "friction", "float", "<min>0.1</min>", 0, 0 },
    { "spring_k", "float", "<min>0.1</min>", 0, 0 },
    { "grid_resolution", "int", "<min>1</min><max>64</max>", 0, 0 },
    { "min_grid_size", "int", "<min>4</min><max>128</max>", 0, 0 },
    { "map_effect", "int", RESTOSTRING (0, WOBBLY_EFFECT_LAST), 0, 0 },
    { "focus_effect", "int", RESTOSTRING (0, WOBBLY_EFFECT_LAST), 0, 0 },
    { "map_window_match", "match", 0, 0, 0 },
    { "focus_window_match", "match", 0, 0, 0 },
    { "grab_window_match", "match", 0, 0, 0 },
    { "move_window_match", "match", 0, 0, 0 },
    { "maximize_effect", "bool", 0, 0, 0 }
};

#define SNAP_WINDOW_TYPE (CompWindowTypeNormalMask  | \
			  CompWindowTypeToolbarMask | \
			  CompWindowTypeMenuMask    | \
			  CompWindowTypeUtilMask)

static void
findNextWestEdge (CompWindow *w,
		  Object     *object)
{
    int v, v1, v2;
    int s, start;
    int e, end;
    int x;
    int output;
    const XRectangle *workArea;
    int workAreaEdge;

    start = -65535.0f;
    end   =  65535.0f;

    v1 = -65535.0f;
    v2 =  65535.0f;

    x = object->position.x + w->output.left - w->input.left;

    output = outputDeviceForPoint (w->screen, x, object->position.y);
    workArea = &w->screen->outputDev[output].workArea;
    workAreaEdge = workArea->x;

    if (x >= workAreaEdge)
    {
	CompWindow *p;

	v1 = workAreaEdge;

	for (p = w->screen->windows; p; p = p->next)
	{
	    if (w == p)
		continue;

	    if (p->mapNum && p->struts)
	    {
		s = p->struts->left.y - w->output.top;
		e = p->struts->left.y + p->struts->left.height +
		    w->output.bottom;
	    }
	    else if (!p->invisible && (p->type & SNAP_WINDOW_TYPE))
	    {
		s = p->attrib.y - p->input.top - w->output.top;
		e = p->attrib.y + p->height + p->input.bottom +
		    w->output.bottom;
	    }
	    else
	    {
		continue;
	    }

	    if (s > object->position.y)
	    {
		if (s < end)
		    end = s;
	    }
	    else if (e < object->position.y)
	    {
		if (e > start)
		    start = e;
	    }
	    else
	    {
		if (s > start)
		    start = s;

		if (e < end)
		    end = e;

		if (p->mapNum && p->struts)
		    v = p->struts->left.x + p->struts->left.width;
		else
		    v = p->attrib.x + p->width + p->input.right;

		if (v <= x)
		{
		    if (v > v1)
			v1 = v;
		}
		else
		{
		    if (v < v2)
			v2 = v;
		}
	    }
	}
    }
    else
    {
	v2 = workAreaEdge;
    }

    v1 = v1 - w->output.left + w->input.left;
    v2 = v2 - w->output.left + w->input.left;

    if (v1 != (int) object->vertEdge.next)
	object->vertEdge.snapped = FALSE;

    object->vertEdge.start = start;
    object->vertEdge.end   = end;

    object->vertEdge.next = v1;
    object->vertEdge.prev = v2;

    object->vertEdge.attract  = v1 + EDGE_DISTANCE;
    object->vertEdge.velocity = EDGE_VELOCITY;
}

static void
findNextEastEdge (CompWindow *w,
		  Object     *object)
{
    int v, v1, v2;
    int s, start;
    int e, end;
    int x;
    int output;
    const XRectangle *workArea;
    int workAreaEdge;

    start = -65535.0f;
    end   =  65535.0f;

    v1 =  65535.0f;
    v2 = -65535.0f;

    x = object->position.x - w->output.right + w->input.right;

    output = outputDeviceForPoint (w->screen, x, object->position.y);
    workArea = &w->screen->outputDev[output].workArea;
    workAreaEdge = workArea->x + workArea->width;

    if (x <= workAreaEdge)
    {
	CompWindow *p;

	v1 = workAreaEdge;

	for (p = w->screen->windows; p; p = p->next)
	{
	    if (w == p)
		continue;

	    if (p->mapNum && p->struts)
	    {
		s = p->struts->right.y - w->output.top;
		e = p->struts->right.y + p->struts->right.height +
		    w->output.bottom;
	    }
	    else if (!p->invisible && (p->type & SNAP_WINDOW_TYPE))
	    {
		s = p->attrib.y - p->input.top - w->output.top;
		e = p->attrib.y + p->height + p->input.bottom +
		    w->output.bottom;
	    }
	    else
	    {
		continue;
	    }

	    if (s > object->position.y)
	    {
		if (s < end)
		    end = s;
	    }
	    else if (e < object->position.y)
	    {
		if (e > start)
		    start = e;
	    }
	    else
	    {
		if (s > start)
		    start = s;

		if (e < end)
		    end = e;

		if (p->mapNum && p->struts)
		    v = p->struts->right.x;
		else
		    v = p->attrib.x - p->input.left;

		if (v >= x)
		{
		    if (v < v1)
			v1 = v;
		}
		else
		{
		    if (v > v2)
			v2 = v;
		}
	    }
	}
    }
    else
    {
	v2 = workAreaEdge;
    }

    v1 = v1 + w->output.right - w->input.right;
    v2 = v2 + w->output.right - w->input.right;

    if (v1 != (int) object->vertEdge.next)
	object->vertEdge.snapped = FALSE;

    object->vertEdge.start = start;
    object->vertEdge.end   = end;

    object->vertEdge.next = v1;
    object->vertEdge.prev = v2;

    object->vertEdge.attract  = v1 - EDGE_DISTANCE;
    object->vertEdge.velocity = EDGE_VELOCITY;
}

static void
findNextNorthEdge (CompWindow *w,
		   Object     *object)
{
    int v, v1, v2;
    int s, start;
    int e, end;
    int y;
    int output;
    const XRectangle *workArea;
    int workAreaEdge;

    start = -65535.0f;
    end   =  65535.0f;

    v1 = -65535.0f;
    v2 =  65535.0f;

    y = object->position.y + w->output.top - w->input.top;

    output = outputDeviceForPoint (w->screen, object->position.x, y);
    workArea = &w->screen->outputDev[output].workArea;
    workAreaEdge = workArea->y;

    if (y >= workAreaEdge)
    {
	CompWindow *p;

	v1 = workAreaEdge;

	for (p = w->screen->windows; p; p = p->next)
	{
	    if (w == p)
		continue;

	    if (p->mapNum && p->struts)
	    {
		s = p->struts->top.x - w->output.left;
		e = p->struts->top.x + p->struts->top.width + w->output.right;
	    }
	    else if (!p->invisible && (p->type & SNAP_WINDOW_TYPE))
	    {
		s = p->attrib.x - p->input.left - w->output.left;
		e = p->attrib.x + p->width + p->input.right + w->output.right;
	    }
	    else
	    {
		continue;
	    }

	    if (s > object->position.x)
	    {
		if (s < end)
		    end = s;
	    }
	    else if (e < object->position.x)
	    {
		if (e > start)
		    start = e;
	    }
	    else
	    {
		if (s > start)
		    start = s;

		if (e < end)
		    end = e;

		if (p->mapNum && p->struts)
		    v = p->struts->top.y + p->struts->top.height;
		else
		    v = p->attrib.y + p->height + p->input.bottom;

		if (v <= y)
		{
		    if (v > v1)
			v1 = v;
		}
		else
		{
		    if (v < v2)
			v2 = v;
		}
	    }
	}
    }
    else
    {
	v2 = workAreaEdge;
    }

    v1 = v1 - w->output.top + w->input.top;
    v2 = v2 - w->output.top + w->input.top;

    if (v1 != (int) object->horzEdge.next)
	object->horzEdge.snapped = FALSE;

    object->horzEdge.start = start;
    object->horzEdge.end   = end;

    object->horzEdge.next = v1;
    object->horzEdge.prev = v2;

    object->horzEdge.attract  = v1 + EDGE_DISTANCE;
    object->horzEdge.velocity = EDGE_VELOCITY;
}

static void
findNextSouthEdge (CompWindow *w,
		   Object     *object)
{
    int v, v1, v2;
    int s, start;
    int e, end;
    int y;
    int output;
    const XRectangle *workArea;
    int workAreaEdge;

    start = -65535.0f;
    end   =  65535.0f;

    v1 =  65535.0f;
    v2 = -65535.0f;

    y = object->position.y - w->output.bottom + w->input.bottom;

    output = outputDeviceForPoint (w->screen, object->position.x, y);
    workArea = &w->screen->outputDev[output].workArea;
    workAreaEdge = workArea->y + workArea->height;

    if (y <= workAreaEdge)
    {
	CompWindow *p;

	v1 = workAreaEdge;

	for (p = w->screen->windows; p; p = p->next)
	{
	    if (w == p)
		continue;

	    if (p->mapNum && p->struts)
	    {
		s = p->struts->bottom.x - w->output.left;
		e = p->struts->bottom.x + p->struts->bottom.width +
		    w->output.right;
	    }
	    else if (!p->invisible && (p->type & SNAP_WINDOW_TYPE))
	    {
		s = p->attrib.x - p->input.left - w->output.left;
		e = p->attrib.x + p->width + p->input.right + w->output.right;
	    }
	    else
	    {
		continue;
	    }

	    if (s > object->position.x)
	    {
		if (s < end)
		    end = s;
	    }
	    else if (e < object->position.x)
	    {
		if (e > start)
		    start = e;
	    }
	    else
	    {
		if (s > start)
		    start = s;

		if (e < end)
		    end = e;

		if (p->mapNum && p->struts)
		    v = p->struts->bottom.y;
		else
		    v = p->attrib.y - p->input.top;

		if (v >= y)
		{
		    if (v < v1)
			v1 = v;
		}
		else
		{
		    if (v > v2)
			v2 = v;
		}
	    }
	}
    }
    else
    {
	v2 = workAreaEdge;
    }

    v1 = v1 + w->output.bottom - w->input.bottom;
    v2 = v2 + w->output.bottom - w->input.bottom;

    if (v1 != (int) object->horzEdge.next)
	object->horzEdge.snapped = FALSE;

    object->horzEdge.start = start;
    object->horzEdge.end   = end;

    object->horzEdge.next = v1;
    object->horzEdge.prev = v2;

    object->horzEdge.attract  = v1 - EDGE_DISTANCE;
    object->horzEdge.velocity = EDGE_VELOCITY;
}

static void
objectInit (Object *object,
	    float  positionX,
	    float  positionY,
	    float  velocityX,
	    float  velocityY)
{
    object->force.x = 0;
    object->force.y = 0;

    object->position.x = positionX;
    object->position.y = positionY;

    object->velocity.x = velocityX;
    object->velocity.y = velocityY;

    object->theta    = 0;
    object->immobile = FALSE;

    object->edgeMask = 0;

    object->vertEdge.snapped = FALSE;
    object->horzEdge.snapped = FALSE;

    object->vertEdge.next = 0.0f;
    object->horzEdge.next = 0.0f;
}

static void
springInit (Spring *spring,
	    Object *a,
	    Object *b,
	    float  offsetX,
	    float  offsetY)
{
    spring->a	     = a;
    spring->b	     = b;
    spring->offset.x = offsetX;
    spring->offset.y = offsetY;
}

static void
modelCalcBounds (Model *model)
{
    int i;

    model->topLeft.x	 = MAXSHORT;
    model->topLeft.y	 = MAXSHORT;
    model->bottomRight.x = MINSHORT;
    model->bottomRight.y = MINSHORT;

    for (i = 0; i < model->numObjects; i++)
    {
	if (model->objects[i].position.x < model->topLeft.x)
	    model->topLeft.x = model->objects[i].position.x;
	else if (model->objects[i].position.x > model->bottomRight.x)
	    model->bottomRight.x = model->objects[i].position.x;

	if (model->objects[i].position.y < model->topLeft.y)
	    model->topLeft.y = model->objects[i].position.y;
	else if (model->objects[i].position.y > model->bottomRight.y)
	    model->bottomRight.y = model->objects[i].position.y;
    }
}

static void
modelAddSpring (Model  *model,
		Object *a,
		Object *b,
		float  offsetX,
		float  offsetY)
{
    Spring *spring;

    spring = &model->springs[model->numSprings];
    model->numSprings++;

    springInit (spring, a, b, offsetX, offsetY);
}

static void
modelSetMiddleAnchor (Model *model,
		      int   x,
		      int   y,
		      int   width,
		      int   height)
{
    float gx, gy;

    gx = ((GRID_WIDTH  - 1) / 2 * width)  / (float) (GRID_WIDTH  - 1);
    gy = ((GRID_HEIGHT - 1) / 2 * height) / (float) (GRID_HEIGHT - 1);

    if (model->anchorObject)
	model->anchorObject->immobile = FALSE;

    model->anchorObject = &model->objects[GRID_WIDTH *
					  ((GRID_HEIGHT - 1) / 2) +
					  (GRID_WIDTH - 1) / 2];
    model->anchorObject->position.x = x + gx;
    model->anchorObject->position.y = y + gy;

    model->anchorObject->immobile = TRUE;
}

static void
modelSetTopAnchor (Model *model,
		   int   x,
		   int   y,
		   int   width)
{
    float gx;

    gx = ((GRID_WIDTH - 1) / 2 * width)  / (float) (GRID_WIDTH - 1);

    if (model->anchorObject)
	model->anchorObject->immobile = FALSE;

    model->anchorObject = &model->objects[(GRID_WIDTH - 1) / 2];
    model->anchorObject->position.x = x + gx;
    model->anchorObject->position.y = y;

    model->anchorObject->immobile = TRUE;
}

static void
modelAddEdgeAnchors (Model *model,
		     int   x,
		     int   y,
		     int   width,
		     int   height)
{
    Object *o;

    o = &model->objects[0];
    o->position.x = x;
    o->position.y = y;
    o->immobile = TRUE;

    o = &model->objects[GRID_WIDTH - 1];
    o->position.x = x + width;
    o->position.y = y;
    o->immobile = TRUE;

    o = &model->objects[GRID_WIDTH * (GRID_HEIGHT - 1)];
    o->position.x = x;
    o->position.y = y + height;
    o->immobile = TRUE;

    o = &model->objects[model->numObjects - 1];
    o->position.x = x + width;
    o->position.y = y + height;
    o->immobile = TRUE;

    if (!model->anchorObject)
	model->anchorObject = &model->objects[0];
}

static void
modelRemoveEdgeAnchors (Model *model,
			int   x,
			int   y,
			int   width,
			int   height)
{
    Object *o;

    o = &model->objects[0];
    o->position.x = x;
    o->position.y = y;
    if (o != model->anchorObject)
	o->immobile = FALSE;

    o = &model->objects[GRID_WIDTH - 1];
    o->position.x = x + width;
    o->position.y = y;
    if (o != model->anchorObject)
	o->immobile = FALSE;

    o = &model->objects[GRID_WIDTH * (GRID_HEIGHT - 1)];
    o->position.x = x;
    o->position.y = y + height;
    if (o != model->anchorObject)
	o->immobile = FALSE;

    o = &model->objects[model->numObjects - 1];
    o->position.x = x + width;
    o->position.y = y + height;
    if (o != model->anchorObject)
	o->immobile = FALSE;
}

static void
modelAdjustObjectPosition (Model  *model,
			   Object *object,
			   int    x,
			   int    y,
			   int    width,
			   int    height)
{
    Object *o;
    int	   gridX, gridY, i = 0;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    o = &model->objects[i];
	    if (o == object)
	    {
		o->position.x = x + (gridX * width) / (GRID_WIDTH - 1);
		o->position.y = y + (gridY * height) / (GRID_HEIGHT - 1);

		return;
	    }

	    i++;
	}
    }
}

static void
modelInitObjects (Model *model,
		  int	x,
		  int   y,
		  int	width,
		  int	height)
{
    int	  gridX, gridY, i = 0;
    float gw, gh;

    gw = GRID_WIDTH  - 1;
    gh = GRID_HEIGHT - 1;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    objectInit (&model->objects[i],
			x + (gridX * width) / gw,
			y + (gridY * height) / gh,
			0, 0);
	    i++;
	}
    }

    modelSetMiddleAnchor (model, x, y, width, height);
}

static void
modelUpdateSnapping (CompWindow *window,
		     Model      *model)
{
    unsigned int edgeMask, gridMask, mask;
    int		 gridX, gridY, i = 0;

    edgeMask = model->edgeMask;

    if (model->snapCnt[NORTH])
	edgeMask &= ~SouthEdgeMask;
    else if (model->snapCnt[SOUTH])
	edgeMask &= ~NorthEdgeMask;

    if (model->snapCnt[WEST])
	edgeMask &= ~EastEdgeMask;
    else if (model->snapCnt[EAST])
	edgeMask &= ~WestEdgeMask;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	if (gridY == 0)
	    gridMask = edgeMask & NorthEdgeMask;
	else if (gridY == GRID_HEIGHT - 1)
	    gridMask = edgeMask & SouthEdgeMask;
	else
	    gridMask = 0;

	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    mask = gridMask;

	    if (gridX == 0)
		mask |= edgeMask & WestEdgeMask;
	    else if (gridX == GRID_WIDTH - 1)
		mask |= edgeMask & EastEdgeMask;

	    if (mask != model->objects[i].edgeMask)
	    {
		model->objects[i].edgeMask = mask;

		if (mask & WestEdgeMask)
		{
		    if (!model->objects[i].vertEdge.snapped)
			findNextWestEdge (window, &model->objects[i]);
		}
		else if (mask & EastEdgeMask)
		{
		    if (!model->objects[i].vertEdge.snapped)
			findNextEastEdge (window, &model->objects[i]);
		}
		else
		    model->objects[i].vertEdge.snapped = FALSE;

		if (mask & NorthEdgeMask)
		{
		    if (!model->objects[i].horzEdge.snapped)
			findNextNorthEdge (window, &model->objects[i]);
		}
		else if (mask & SouthEdgeMask)
		{
		    if (!model->objects[i].horzEdge.snapped)
			findNextSouthEdge (window, &model->objects[i]);
		}
		else
		    model->objects[i].horzEdge.snapped = FALSE;
	    }

	    i++;
	}
    }
}

static void
modelReduceEdgeEscapeVelocity (Model *model)
{
    int	gridX, gridY, i = 0;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    if (model->objects[i].vertEdge.snapped)
		model->objects[i].vertEdge.velocity *= drand48 () * 0.25f;

	    if (model->objects[i].horzEdge.snapped)
		model->objects[i].horzEdge.velocity *= drand48 () * 0.25f;

	    i++;
	}
    }
}

static Bool
modelDisableSnapping (CompWindow *window,
		      Model      *model)
{
    int	 gridX, gridY, i = 0;
    Bool snapped = FALSE;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    if (model->objects[i].vertEdge.snapped ||
		model->objects[i].horzEdge.snapped)
		snapped = TRUE;

	    model->objects[i].vertEdge.snapped = FALSE;
	    model->objects[i].horzEdge.snapped = FALSE;

	    model->objects[i].edgeMask = 0;

	    i++;
	}
    }

    memset (model->snapCnt, 0, sizeof (model->snapCnt));

    return snapped;
}

static void
modelAdjustObjectsForShiver (Model *model,
			     int   x,
			     int   y,
			     int   width,
			     int   height)
{
    int   gridX, gridY, i = 0;
    float vX, vY;
    float w, h;
    float scale;

    w = width;
    h = height;

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    if (!model->objects[i].immobile)
	    {
		vX = model->objects[i].position.x - (x + w / 2);
		vY = model->objects[i].position.y - (y + h / 2);

		vX /= w;
		vY /= h;

		scale = ((float) rand () * 7.5f) / RAND_MAX;

		model->objects[i].velocity.x += vX * scale;
		model->objects[i].velocity.y += vY * scale;
	    }

	    i++;
	}
    }
}

static void
modelInitSprings (Model *model,
		  int   x,
		  int   y,
		  int   width,
		  int   height)
{
    int   gridX, gridY, i = 0;
    float hpad, vpad;

    model->numSprings = 0;

    hpad = ((float) width) / (GRID_WIDTH  - 1);
    vpad = ((float) height) / (GRID_HEIGHT - 1);

    for (gridY = 0; gridY < GRID_HEIGHT; gridY++)
    {
	for (gridX = 0; gridX < GRID_WIDTH; gridX++)
	{
	    if (gridX > 0)
		modelAddSpring (model,
				&model->objects[i - 1],
				&model->objects[i],
				hpad, 0);

	    if (gridY > 0)
		modelAddSpring (model,
				&model->objects[i - GRID_WIDTH],
				&model->objects[i],
				0, vpad);

	    i++;
	}
    }
}

static void
modelMove (Model *model,
	   float tx,
	   float ty)
{
    int i;

    for (i = 0; i < model->numObjects; i++)
    {
	model->objects[i].position.x += tx;
	model->objects[i].position.y += ty;
    }
}

static Model *
createModel (int	  x,
	     int	  y,
	     int	  width,
	     int	  height,
	     unsigned int edgeMask)
{
    Model *model;

    model = malloc (sizeof (Model));
    if (!model)
	return 0;

    model->numObjects = GRID_WIDTH * GRID_HEIGHT;
    model->objects = malloc (sizeof (Object) * model->numObjects);
    if (!model->objects)
    {
	free (model);
	return 0;
    }

    model->anchorObject = 0;
    model->numSprings = 0;

    model->steps = 0;

    memset (model->snapCnt, 0, sizeof (model->snapCnt));

    model->edgeMask = edgeMask;

    modelInitObjects (model, x, y, width, height);
    modelInitSprings (model, x, y, width, height);

    modelCalcBounds (model);

    return model;
}

static void
objectApplyForce (Object *object,
		  float  fx,
		  float  fy)
{
    object->force.x += fx;
    object->force.y += fy;
}

static void
springExertForces (Spring *spring,
		   float  k)
{
    Vector da, db;
    Vector a, b;

    a = spring->a->position;
    b = spring->b->position;

    da.x = 0.5f * (b.x - a.x - spring->offset.x);
    da.y = 0.5f * (b.y - a.y - spring->offset.y);

    db.x = 0.5f * (a.x - b.x + spring->offset.x);
    db.y = 0.5f * (a.y - b.y + spring->offset.y);

    objectApplyForce (spring->a, k * da.x, k * da.y);
    objectApplyForce (spring->b, k * db.x, k * db.y);
}

static Bool
objectReleaseWestEdge (CompWindow *w,
		       Model	  *model,
		       Object	  *object)
{
    if (fabs (object->velocity.x) > object->vertEdge.velocity)
    {
	object->position.x += object->velocity.x * 2.0f;

	model->snapCnt[WEST]--;

	object->vertEdge.snapped = FALSE;
	object->edgeMask = 0;

	modelUpdateSnapping (w, model);

	return TRUE;
    }

    object->velocity.x = 0.0f;

    return FALSE;
}

static Bool
objectReleaseEastEdge (CompWindow *w,
		       Model	  *model,
		       Object	  *object)
{
    if (fabs (object->velocity.x) > object->vertEdge.velocity)
    {
	object->position.x += object->velocity.x * 2.0f;

	model->snapCnt[EAST]--;

	object->vertEdge.snapped = FALSE;
	object->edgeMask = 0;

	modelUpdateSnapping (w, model);

	return TRUE;
    }

    object->velocity.x = 0.0f;

    return FALSE;
}

static Bool
objectReleaseNorthEdge (CompWindow *w,
			Model	   *model,
			Object	   *object)
{
    if (fabs (object->velocity.y) > object->horzEdge.velocity)
    {
	object->position.y += object->velocity.y * 2.0f;

	model->snapCnt[NORTH]--;

	object->horzEdge.snapped = FALSE;
	object->edgeMask = 0;

	modelUpdateSnapping (w, model);

	return TRUE;
    }

    object->velocity.y = 0.0f;

    return FALSE;
}

static Bool
objectReleaseSouthEdge (CompWindow *w,
			Model	   *model,
			Object	   *object)
{
    if (fabs (object->velocity.y) > object->horzEdge.velocity)
    {
	object->position.y += object->velocity.y * 2.0f;

	model->snapCnt[SOUTH]--;

	object->horzEdge.snapped = FALSE;
	object->edgeMask = 0;

	modelUpdateSnapping (w, model);

	return TRUE;
    }

    object->velocity.y = 0.0f;

    return FALSE;
}

static float
modelStepObject (CompWindow *window,
		 Model	    *model,
		 Object	    *object,
		 float	    friction,
		 float	    *force)
{
    object->theta += 0.05f;

    if (object->immobile)
    {
	object->velocity.x = 0.0f;
	object->velocity.y = 0.0f;

	object->force.x = 0.0f;
	object->force.y = 0.0f;

	*force = 0.0f;

	return 0.0f;
    }
    else
    {
	object->force.x -= friction * object->velocity.x;
	object->force.y -= friction * object->velocity.y;

	object->velocity.x += object->force.x / MASS;
	object->velocity.y += object->force.y / MASS;

	if (object->edgeMask)
	{
	    if (object->edgeMask & WestEdgeMask)
	    {
		if (object->position.y < object->vertEdge.start ||
		    object->position.y > object->vertEdge.end)
		    findNextWestEdge (window, object);

		if (!object->vertEdge.snapped ||
		    objectReleaseWestEdge (window, model, object))
		{
		    object->position.x += object->velocity.x;

		    if (object->velocity.x < 0.0f &&
			object->position.x < object->vertEdge.attract)
		    {
			if (object->position.x < object->vertEdge.next)
			{
			    object->vertEdge.snapped = TRUE;
			    object->position.x = object->vertEdge.next;
			    object->velocity.x = 0.0f;

			    model->snapCnt[WEST]++;

			    modelUpdateSnapping (window, model);
			}
			else
			{
			    object->velocity.x -=
				object->vertEdge.attract - object->position.x;
			}
		    }

		    if (object->position.x > object->vertEdge.prev)
			findNextWestEdge (window, object);
		}
	    }
	    else if (object->edgeMask & EastEdgeMask)
	    {
		if (object->position.y < object->vertEdge.start ||
		    object->position.y > object->vertEdge.end)
		    findNextEastEdge (window, object);

		if (!object->vertEdge.snapped ||
		    objectReleaseEastEdge (window, model, object))
		{
		    object->position.x += object->velocity.x;

		    if (object->velocity.x > 0.0f &&
			object->position.x > object->vertEdge.attract)
		    {
			if (object->position.x > object->vertEdge.next)
			{
			    object->vertEdge.snapped = TRUE;
			    object->position.x = object->vertEdge.next;
			    object->velocity.x = 0.0f;

			    model->snapCnt[EAST]++;

			    modelUpdateSnapping (window, model);
			}
			else
			{
			    object->velocity.x =
				object->position.x - object->vertEdge.attract;
			}
		    }

		    if (object->position.x < object->vertEdge.prev)
			findNextEastEdge (window, object);
		}
	    }
	    else
		object->position.x += object->velocity.x;

	    if (object->edgeMask & NorthEdgeMask)
	    {
		if (object->position.x < object->horzEdge.start ||
		    object->position.x > object->horzEdge.end)
		    findNextNorthEdge (window, object);

		if (!object->horzEdge.snapped ||
		    objectReleaseNorthEdge (window, model, object))
		{
		    object->position.y += object->velocity.y;

		    if (object->velocity.y < 0.0f &&
			object->position.y < object->horzEdge.attract)
		    {
			if (object->position.y < object->horzEdge.next)
			{
			    object->horzEdge.snapped = TRUE;
			    object->position.y = object->horzEdge.next;
			    object->velocity.y = 0.0f;

			    model->snapCnt[NORTH]++;

			    modelUpdateSnapping (window, model);
			}
			else
			{
			    object->velocity.y -=
				object->horzEdge.attract - object->position.y;
			}
		    }

		    if (object->position.y > object->horzEdge.prev)
			findNextNorthEdge (window, object);
		}
	    }
	    else if (object->edgeMask & SouthEdgeMask)
	    {
		if (object->position.x < object->horzEdge.start ||
		    object->position.x > object->horzEdge.end)
		    findNextSouthEdge (window, object);

		if (!object->horzEdge.snapped ||
		    objectReleaseSouthEdge (window, model, object))
		{
		    object->position.y += object->velocity.y;

		    if (object->velocity.y > 0.0f &&
			object->position.y > object->horzEdge.attract)
		    {
			if (object->position.y > object->horzEdge.next)
			{
			    object->horzEdge.snapped = TRUE;
			    object->position.y = object->horzEdge.next;
			    object->velocity.y = 0.0f;

			    model->snapCnt[SOUTH]++;

			    modelUpdateSnapping (window, model);
			}
			else
			{
			    object->velocity.y =
				object->position.y - object->horzEdge.attract;
			}
		    }

		    if (object->position.y < object->horzEdge.prev)
			findNextSouthEdge (window, object);
		}
	    }
	    else
		object->position.y += object->velocity.y;
	}
	else
	{
	    object->position.x += object->velocity.x;
	    object->position.y += object->velocity.y;
	}

	*force = fabs (object->force.x) + fabs (object->force.y);

	object->force.x = 0.0f;
	object->force.y = 0.0f;

	return fabs (object->velocity.x) + fabs (object->velocity.y);
    }
}

static int
modelStep (CompWindow *window,
	   Model      *model,
	   float      friction,
	   float      k,
	   float      time)
{
    int   i, j, steps, wobbly = 0;
    float velocitySum = 0.0f;
    float force, forceSum = 0.0f;

    model->steps += time / 15.0f;
    steps = floor (model->steps);
    model->steps -= steps;

    if (!steps)
	return TRUE;

    for (j = 0; j < steps; j++)
    {
	for (i = 0; i < model->numSprings; i++)
	    springExertForces (&model->springs[i], k);

	for (i = 0; i < model->numObjects; i++)
	{
	    velocitySum += modelStepObject (window,
					    model,
					    &model->objects[i],
					    friction,
					    &force);
	    forceSum += force;
	}
    }

    modelCalcBounds (model);

    if (velocitySum > 0.5f)
	wobbly |= WobblyVelocity;

    if (forceSum > 20.0f)
	wobbly |= WobblyForce;

    return wobbly;
}

static void
bezierPatchEvaluate (Model *model,
		     float u,
		     float v,
		     float *patchX,
		     float *patchY)
{
    float coeffsU[4], coeffsV[4];
    float x, y;
    int   i, j;

    coeffsU[0] = (1 - u) * (1 - u) * (1 - u);
    coeffsU[1] = 3 * u * (1 - u) * (1 - u);
    coeffsU[2] = 3 * u * u * (1 - u);
    coeffsU[3] = u * u * u;

    coeffsV[0] = (1 - v) * (1 - v) * (1 - v);
    coeffsV[1] = 3 * v * (1 - v) * (1 - v);
    coeffsV[2] = 3 * v * v * (1 - v);
    coeffsV[3] = v * v * v;

    x = y = 0.0f;

    for (i = 0; i < 4; i++)
    {
	for (j = 0; j < 4; j++)
	{
	    x += coeffsU[i] * coeffsV[j] *
		model->objects[j * GRID_WIDTH + i].position.x;
	    y += coeffsU[i] * coeffsV[j] *
		model->objects[j * GRID_WIDTH + i].position.y;
	}
    }

    *patchX = x;
    *patchY = y;
}

static Bool
wobblyEnsureModel (CompWindow *w)
{
    WOBBLY_WINDOW (w);

    if (!ww->model)
    {
	unsigned int edgeMask = 0;

	if (w->type & CompWindowTypeNormalMask)
	    edgeMask = WestEdgeMask | EastEdgeMask | NorthEdgeMask |
		SouthEdgeMask;

	ww->model = createModel (WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w),
				 edgeMask);
	if (!ww->model)
	    return FALSE;
    }

    return TRUE;
}

static float
objectDistance (Object *object,
		float  x,
		float  y)
{
    float dx, dy;

    dx = object->position.x - x;
    dy = object->position.y - y;

    return sqrt (dx * dx + dy * dy);
}

static Object *
modelFindNearestObject (Model *model,
			float x,
			float y)
{
    Object *object = &model->objects[0];
    float  distance, minDistance = 0.0;
    int    i;

    for (i = 0; i < model->numObjects; i++)
    {
	distance = objectDistance (&model->objects[i], x, y);
	if (i == 0 || distance < minDistance)
	{
	    minDistance = distance;
	    object = &model->objects[i];
	}
    }

    return object;
}

static Bool
isWobblyWin (CompWindow *w)
{
    WOBBLY_WINDOW (w);

    if (ww->model)
	return TRUE;

    /* avoid tiny windows */
    if (w->width == 1 && w->height == 1)
	return FALSE;

    /* avoid fullscreen windows */
    if (w->attrib.x <= 0 &&
	w->attrib.y <= 0 &&
	w->attrib.x + w->width >= w->screen->width &&
	w->attrib.y + w->height >= w->screen->height)
	return FALSE;

    return TRUE;
}

static void
wobblyPreparePaintScreen (CompScreen *s,
			  int	     msSinceLastPaint)
{
    WobblyWindow *ww;
    CompWindow   *w;

    WOBBLY_SCREEN (s);

    if (ws->wobblyWindows & (WobblyInitial | WobblyVelocity))
    {
	BoxRec box;
	Point  topLeft, bottomRight;
	float  friction, springK;
	Model  *model;

	friction = ws->opt[WOBBLY_SCREEN_OPTION_FRICTION].value.f;
	springK  = ws->opt[WOBBLY_SCREEN_OPTION_SPRING_K].value.f;

	ws->wobblyWindows = 0;
	for (w = s->windows; w; w = w->next)
	{
	    ww = GET_WOBBLY_WINDOW (w, ws);

	    if (ww->wobbly)
	    {
		if (ww->wobbly & (WobblyInitial | WobblyVelocity))
		{
		    model = ww->model;

		    topLeft     = model->topLeft;
		    bottomRight = model->bottomRight;

		    ww->wobbly = modelStep (w, model, friction, springK,
					    (ww->wobbly & WobblyVelocity) ?
					    msSinceLastPaint :
					    s->redrawTime);

		    if ((ww->state & MAXIMIZE_STATE) && ww->grabbed)
			ww->wobbly |= WobblyForce;

		    if (ww->wobbly)
		    {
			/* snapped to more than one edge, we have to reduce
			   edge escape velocity until only one edge is snapped */
			if (ww->wobbly == WobblyForce && !ww->grabbed)
			{
			    modelReduceEdgeEscapeVelocity (ww->model);
			    ww->wobbly |= WobblyInitial;
			}

			if (!ww->grabbed && ws->grabWindowWorkArea)
			{
			    float topmostYPos    = MAXSHORT;
			    float bottommostYPos = MINSHORT;
			    int   decorTop;
			    int   decorTitleBottom;
			    int   i;

			    for (i = 0; i < GRID_WIDTH; i++)
			    {
				int modelY = model->objects[i].position.y;

				/* find the bottommost top-row object */
				bottommostYPos = MAX (modelY, bottommostYPos);

				/* find the topmost top-row object */
				topmostYPos = MIN (modelY, topmostYPos);
			    }

			    decorTop = bottommostYPos +
				       w->output.top - w->input.top;
			    decorTitleBottom = topmostYPos + w->output.top;

			    if (ws->grabWindowWorkArea->y > decorTop)
			    {
				/* constrain to work area box top edge */
				modelMove (model, 0,
					   ws->grabWindowWorkArea->y -
					   decorTop);
				modelCalcBounds (model);
			    }
			    else if (ws->grabWindowWorkArea->y +
			    	     ws->grabWindowWorkArea->height <
			    	     decorTitleBottom)
			    {
				/* constrain to work area box bottom edge */
				modelMove (model, 0,
					   ws->grabWindowWorkArea->y +
					   ws->grabWindowWorkArea->height -
					   decorTitleBottom);
				modelCalcBounds (model);
			    }
			}
		    }
		    else
		    {
			ww->model = 0;

			if (w->attrib.x == w->serverX &&
			    w->attrib.y == w->serverY)
			{
			    moveWindow (w,
					model->topLeft.x + w->output.left -
					w->attrib.x,
					model->topLeft.y + w->output.top -
					w->attrib.y,
					TRUE, TRUE);
			    syncWindowPosition (w);
			}

			ww->model = model;
		    }

		    if (!(s->damageMask & COMP_SCREEN_DAMAGE_ALL_MASK))
		    {
			if (ww->wobbly)
			{
			    if (ww->model->topLeft.x < topLeft.x)
				topLeft.x = ww->model->topLeft.x;
			    if (ww->model->topLeft.y < topLeft.y)
				topLeft.y = ww->model->topLeft.y;
			    if (ww->model->bottomRight.x > bottomRight.x)
				bottomRight.x = ww->model->bottomRight.x;
			    if (ww->model->bottomRight.y > bottomRight.y)
				bottomRight.y = ww->model->bottomRight.y;
			}
			else
			    addWindowDamage (w);

			box.x1 = topLeft.x;
			box.y1 = topLeft.y;
			box.x2 = bottomRight.x + 0.5f;
			box.y2 = bottomRight.y + 0.5f;

			box.x1 -= w->attrib.x + w->attrib.border_width;
			box.y1 -= w->attrib.y + w->attrib.border_width;
			box.x2 -= w->attrib.x + w->attrib.border_width;
			box.y2 -= w->attrib.y + w->attrib.border_width;

			addWindowDamageRect (w, &box);
		    }
		}

		ws->wobblyWindows |= ww->wobbly;
	    }
	}
	if (!ws->wobblyWindows)
	    ws->grabWindowWorkArea = NULL;
    }

    UNWRAP (ws, s, preparePaintScreen);
    (*s->preparePaintScreen) (s, msSinceLastPaint);
    WRAP (ws, s, preparePaintScreen, wobblyPreparePaintScreen);
}

static void
wobblyDonePaintScreen (CompScreen *s)
{
    WOBBLY_SCREEN (s);

    if (ws->wobblyWindows & (WobblyVelocity | WobblyInitial))
	damagePendingOnScreen (s);

    UNWRAP (ws, s, donePaintScreen);
    (*s->donePaintScreen) (s);
    WRAP (ws, s, donePaintScreen, wobblyDonePaintScreen);
}

static void
wobblyDrawWindowGeometry (CompWindow *w)
{
    int     texUnit = w->texUnits;
    int     currentTexUnit = 0;
    int     stride = w->vertexStride;
    GLfloat *vertices = w->vertices + (stride - 3);

    stride *= sizeof (GLfloat);

    glVertexPointer (3, GL_FLOAT, stride, vertices);

    while (texUnit--)
    {
	if (texUnit != currentTexUnit)
	{
	    w->screen->clientActiveTexture (GL_TEXTURE0_ARB + texUnit);
	    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	    currentTexUnit = texUnit;
	}
	vertices -= w->texCoordSize;
	glTexCoordPointer (w->texCoordSize, GL_FLOAT, stride, vertices);
    }

    glDrawElements (GL_QUADS, w->indexCount, GL_UNSIGNED_SHORT, w->indices);

    /* disable all texture coordinate arrays except 0 */
    texUnit = w->texUnits;
    if (texUnit > 1)
    {
	while (--texUnit)
	{
	    (*w->screen->clientActiveTexture) (GL_TEXTURE0_ARB + texUnit);
	    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
	}

	(*w->screen->clientActiveTexture) (GL_TEXTURE0_ARB);
    }
}

static void
wobblyAddWindowGeometry (CompWindow *w,
			 CompMatrix *matrix,
			 int	    nMatrix,
			 Region     region,
			 Region     clip)
{
    WOBBLY_WINDOW (w);
    WOBBLY_SCREEN (w->screen);

    if (ww->wobbly)
    {
	BoxPtr   pClip;
	int      nClip, nVertices, nIndices;
	GLushort *i;
	GLfloat  *v;
	int      x1, y1, x2, y2;
	float    width, height;
	float    deformedX, deformedY;
	int      x, y, iw, ih, wx, wy;
	int      vSize, it;
	int      gridW, gridH;
	Bool     rect = TRUE;

	for (it = 0; it < nMatrix; it++)
	{
	    if (matrix[it].xy != 0.0f || matrix[it].yx != 0.0f)
	    {
		rect = FALSE;
		break;
	    }
	}

	wx     = WIN_X (w);
	wy     = WIN_Y (w);
	width  = WIN_W (w);
	height = WIN_H (w);

	gridW = width / ws->opt[WOBBLY_SCREEN_OPTION_GRID_RESOLUTION].value.i;
	if (gridW < ws->opt[WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE].value.i)
	    gridW = ws->opt[WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE].value.i;

	gridH = height / ws->opt[WOBBLY_SCREEN_OPTION_GRID_RESOLUTION].value.i;
	if (gridH < ws->opt[WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE].value.i)
	    gridH = ws->opt[WOBBLY_SCREEN_OPTION_MIN_GRID_SIZE].value.i;

	nClip = region->numRects;
	pClip = region->rects;

	w->texUnits = nMatrix;

	vSize = 3 + nMatrix * 2;

	nVertices = w->vCount;
	nIndices  = w->indexCount;

	v = w->vertices + (nVertices * vSize);
	i = w->indices  + nIndices;

	while (nClip--)
	{
	    x1 = pClip->x1;
	    y1 = pClip->y1;
	    x2 = pClip->x2;
	    y2 = pClip->y2;

	    iw = ((x2 - x1 - 1) / gridW) + 1;
	    ih = ((y2 - y1 - 1) / gridH) + 1;

	    if (nIndices + (iw * ih * 4) > w->indexSize)
	    {
		if (!moreWindowIndices (w, nIndices + (iw * ih * 4)))
		    return;

		i = w->indices + nIndices;
	    }

	    iw++;
	    ih++;

	    for (y = 0; y < ih - 1; y++)
	    {
		for (x = 0; x < iw - 1; x++)
		{
		    *i++ = nVertices + iw * (y + 1) + x;
		    *i++ = nVertices + iw * (y + 1) + x + 1;
		    *i++ = nVertices + iw * y + x + 1;
		    *i++ = nVertices + iw * y + x;

		    nIndices += 4;
		}
	    }

	    if (((nVertices + iw * ih) * vSize) > w->vertexSize)
	    {
		if (!moreWindowVertices (w, (nVertices + iw * ih) * vSize))
		    return;

		v = w->vertices + (nVertices * vSize);
	    }

	    for (y = y1;; y += gridH)
	    {
		if (y > y2)
		    y = y2;

		for (x = x1;; x += gridW)
		{
		    if (x > x2)
			x = x2;

		    bezierPatchEvaluate (ww->model,
					 (x - wx) / width,
					 (y - wy) / height,
					 &deformedX,
					 &deformedY);

		    if (rect)
		    {
			for (it = 0; it < nMatrix; it++)
			{
			    *v++ = COMP_TEX_COORD_X (&matrix[it], x);
			    *v++ = COMP_TEX_COORD_Y (&matrix[it], y);
			}
		    }
		    else
		    {
			for (it = 0; it < nMatrix; it++)
			{
			    *v++ = COMP_TEX_COORD_XY (&matrix[it], x, y);
			    *v++ = COMP_TEX_COORD_YX (&matrix[it], x, y);
			}
		    }

		    *v++ = deformedX;
		    *v++ = deformedY;
		    *v++ = 0.0;

		    nVertices++;

		    if (x == x2)
			break;
		}

		if (y == y2)
		    break;
	    }

	    pClip++;
	}

	w->vCount	      = nVertices;
	w->vertexStride       = vSize;
	w->texCoordSize       = 2;
	w->indexCount	      = nIndices;
	w->drawWindowGeometry = wobblyDrawWindowGeometry;
    }
    else
    {
	UNWRAP (ws, w->screen, addWindowGeometry);
	(*w->screen->addWindowGeometry) (w, matrix, nMatrix, region, clip);
	WRAP (ws, w->screen, addWindowGeometry, wobblyAddWindowGeometry);
    }
}

static Bool
wobblyPaintWindow (CompWindow		   *w,
		   const WindowPaintAttrib *attrib,
		   const CompTransform	   *transform,
		   Region		   region,
		   unsigned int		   mask)
{
    Bool status;

    WOBBLY_SCREEN (w->screen);
    WOBBLY_WINDOW (w);

    if (ww->wobbly)
	mask |= PAINT_WINDOW_TRANSFORMED_MASK;

    UNWRAP (ws, w->screen, paintWindow);
    status = (*w->screen->paintWindow) (w, attrib, transform, region, mask);
    WRAP (ws, w->screen, paintWindow, wobblyPaintWindow);

    return status;
}

static Bool
wobblyEnableSnapping (CompDisplay     *d,
		      CompAction      *action,
		      CompActionState state,
		      CompOption      *option,
		      int	      nOption)
{
    CompScreen *s;
    CompWindow *w;

    WOBBLY_DISPLAY (d);

    for (s = d->screens; s; s = s->next)
    {
	for (w = s->windows; w; w = w->next)
	{
	    WOBBLY_WINDOW (w);

	    if (ww->grabbed && ww->model)
		modelUpdateSnapping (w, ww->model);
	}
    }

    wd->snapping = TRUE;

    return FALSE;
}

static Bool
wobblyDisableSnapping (CompDisplay     *d,
		       CompAction      *action,
		       CompActionState state,
		       CompOption      *option,
		       int	       nOption)
{
    CompScreen *s;
    CompWindow *w;

    WOBBLY_DISPLAY (d);

    if (!wd->snapping)
	return FALSE;

    for (s = d->screens; s; s = s->next)
    {
	for (w = s->windows; w; w = w->next)
	{
	    WOBBLY_WINDOW (w);

	    if (ww->grabbed && ww->model)
	    {
		if (modelDisableSnapping (w, ww->model))
		{
		    WOBBLY_SCREEN (w->screen);

		    ww->wobbly |= WobblyInitial;
		    ws->wobblyWindows |= ww->wobbly;

		    damagePendingOnScreen (w->screen);
		}
	    }
	}
    }

    wd->snapping = FALSE;

    return FALSE;
}

static Bool
wobblyShiver (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int	      nOption)
{
    CompWindow *w;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "window", 0);

    w = findWindowAtDisplay (d, xid);
    if (w && isWobblyWin (w) && wobblyEnsureModel (w))
    {
	WOBBLY_SCREEN (w->screen);
	WOBBLY_WINDOW (w);

	modelSetMiddleAnchor (ww->model,
			      WIN_X (w), WIN_Y (w),
			      WIN_W (w), WIN_H (w));
	modelAdjustObjectsForShiver (ww->model,
				     WIN_X (w), WIN_Y (w),
				     WIN_W (w), WIN_H (w));

	ww->wobbly |= WobblyInitial;
	ws->wobblyWindows |= ww->wobbly;

	damagePendingOnScreen (w->screen);
    }

    return FALSE;
}

static void
wobblyHandleEvent (CompDisplay *d,
		   XEvent      *event)
{
    Window     activeWindow = d->activeWindow;
    CompWindow *w;
    CompScreen *s;

    WOBBLY_DISPLAY (d);

    switch (event->type) {
    case MapNotify:
	w = findWindowAtDisplay (d, event->xmap.window);
	if (w)
	{
	    WOBBLY_WINDOW (w);

	    if (ww->model)
	    {
		modelInitObjects (ww->model,
				  WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));

		modelInitSprings (ww->model,
				  WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));
	    }
	}
	break;
    default:
	if (event->type == d->xkbEvent)
	{
	    XkbAnyEvent *xkbEvent = (XkbAnyEvent *) event;

	    if (xkbEvent->xkb_type == XkbStateNotify)
	    {
		XkbStateNotifyEvent *stateEvent = (XkbStateNotifyEvent *) event;
		CompAction	    *action;
		Bool		    inverted;
		unsigned int	    mods = 0xffffffff;

		action   =
		    &wd->opt[WOBBLY_DISPLAY_OPTION_SNAP_KEY].value.action;
		inverted = wd->opt[WOBBLY_DISPLAY_OPTION_SNAP_INVERTED].value.b;

		if (action->type & CompBindingTypeKey)
		    mods = action->key.modifiers;

		if ((stateEvent->mods & mods) == mods)
		{
		    if (inverted)
			wobblyDisableSnapping (d, NULL, 0, NULL, 0);
		    else
			wobblyEnableSnapping (d, NULL, 0, NULL, 0);
		}
		else
		{
		    if (inverted)
			wobblyEnableSnapping (d, NULL, 0, NULL, 0);
		    else
			wobblyDisableSnapping (d, NULL, 0, NULL, 0);
		}
	    }
	}
	break;
    }

    UNWRAP (wd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (wd, d, handleEvent, wobblyHandleEvent);

    switch (event->type) {
    case MotionNotify:
	s = findScreenAtDisplay (d, event->xmotion.root);
	if (s)
	{
	    WOBBLY_SCREEN (s);

	    if (ws->grabWindow &&
		ws->moveWindow &&
		ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
	    {
		WOBBLY_WINDOW (ws->grabWindow);

		if (ww->state & MAXIMIZE_STATE)
		{
		    if (ww->model && ww->grabbed)
		    {
			int dx, dy;

			if (ww->state & CompWindowStateMaximizedHorzMask)
			    dx = pointerX - lastPointerX;
			else
			    dx = 0;

			if (ww->state & CompWindowStateMaximizedVertMask)
			    dy = pointerY - lastPointerY;
			else
			    dy = 0;

			ww->model->anchorObject->position.x += dx;
			ww->model->anchorObject->position.y += dy;

			ww->wobbly |= WobblyInitial;
			ws->wobblyWindows |= ww->wobbly;

			damagePendingOnScreen (s);
		    }
		}
	    }
	}
    default:
	break;
    }

    if (d->activeWindow != activeWindow)
    {
	w = findWindowAtDisplay (d, d->activeWindow);
	if (w && isWobblyWin (w))
	{
	    int mIndex;
	    int focusEffect;

	    WOBBLY_WINDOW (w);
	    WOBBLY_SCREEN (w->screen);

	    mIndex      = WOBBLY_SCREEN_OPTION_FOCUS_WINDOW_MATCH;
	    focusEffect = ws->opt[WOBBLY_SCREEN_OPTION_FOCUS_EFFECT].value.i;

	    if ((focusEffect != WOBBLY_EFFECT_NONE)         &&
		matchEval (&ws->opt[mIndex].value.match, w) &&
		wobblyEnsureModel (w))
	    {
		switch (focusEffect) {
		case WOBBLY_EFFECT_SHIVER:
		    modelAdjustObjectsForShiver (ww->model,
						 WIN_X (w),
						 WIN_Y (w),
						 WIN_W (w),
						 WIN_H (w));
		default:
		    break;
		}

		ww->wobbly |= WobblyInitial;
		ws->wobblyWindows |= ww->wobbly;

		damagePendingOnScreen (w->screen);
	    }
	}
    }
}

static Bool
wobblyDamageWindowRect (CompWindow *w,
			Bool	   initial,
			BoxPtr     rect)
{
    Bool status;

    WOBBLY_SCREEN (w->screen);

    if (!initial)
    {
	WOBBLY_WINDOW (w);

	if (ww->wobbly == WobblyForce)
	{
	    REGION region;

	    region.rects = &region.extents;
	    region.numRects = region.size = 1;

	    region.extents.x1 = ww->model->topLeft.x;
	    region.extents.y1 = ww->model->topLeft.y;
	    region.extents.x2 = ww->model->bottomRight.x + 0.5f;
	    region.extents.y2 = ww->model->bottomRight.y + 0.5f;

	    damageScreenRegion (w->screen, &region);

	    return TRUE;
	}
    }

    UNWRAP (ws, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ws, w->screen, damageWindowRect, wobblyDamageWindowRect);

    if (initial)
    {
	if (isWobblyWin (w))
	{
	    int mIndex;
	    int mapEffect;

	    WOBBLY_WINDOW (w);
	    WOBBLY_SCREEN (w->screen);

	    mIndex    = WOBBLY_SCREEN_OPTION_MAP_WINDOW_MATCH;
	    mapEffect = ws->opt[WOBBLY_SCREEN_OPTION_MAP_EFFECT].value.i;

	    if (ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
		wobblyEnsureModel (w);

	    if ((mapEffect != WOBBLY_EFFECT_NONE)           &&
		matchEval (&ws->opt[mIndex].value.match, w) &&
		wobblyEnsureModel (w))
	    {
		switch (mapEffect) {
		case WOBBLY_EFFECT_SHIVER:
		    modelAdjustObjectsForShiver (ww->model,
						 WIN_X (w), WIN_Y (w),
						 WIN_W (w), WIN_H (w));
		default:
		    break;
		}

		ww->wobbly |= WobblyInitial;
		ws->wobblyWindows |= ww->wobbly;

		damagePendingOnScreen (w->screen);
	    }
	}
    }

    return status;
}

static void
wobblyWindowResizeNotify (CompWindow *w,
			  int	     dx,
			  int        dy,
			  int	     dwidth,
			  int	     dheight)
{
    WOBBLY_SCREEN (w->screen);
    WOBBLY_WINDOW (w);

    if (ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b &&
	isWobblyWin (w)					      &&
	/* prevent wobbling when shading maximized windows - assuming that
	   the height difference shaded - non-shaded will hardly be -1 and
	   a lack of wobbly animation in that corner case is tolerable */
	(dheight != -1)                                       &&
	((w->state | ww->state) & MAXIMIZE_STATE))
    {
	ww->state &= ~MAXIMIZE_STATE;
	ww->state |= w->state & MAXIMIZE_STATE;

	if (wobblyEnsureModel (w))
	{
	    if (w->state & MAXIMIZE_STATE)
	    {
		if (!ww->grabbed && ww->model->anchorObject)
		{
		    ww->model->anchorObject->immobile = FALSE;
		    ww->model->anchorObject = NULL;
		}

		modelAddEdgeAnchors (ww->model,
				     WIN_X (w), WIN_Y (w),
				     WIN_W (w), WIN_H (w));
	    }
	    else
	    {
		modelRemoveEdgeAnchors (ww->model,
					WIN_X (w), WIN_Y (w),
					WIN_W (w), WIN_H (w));
		modelSetMiddleAnchor (ww->model,
				      WIN_X (w), WIN_Y (w),
				      WIN_W (w), WIN_H (w));
	    }

	    modelInitSprings (ww->model,
			      WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));

	    ww->wobbly |= WobblyInitial;
	    ws->wobblyWindows |= ww->wobbly;

	    damagePendingOnScreen (w->screen);
	}
    }
    else if (ww->model)
    {
	if (ww->wobbly)
	{
	    if (!(ww->state & MAXIMIZE_STATE))
		modelSetTopAnchor (ww->model, WIN_X (w), WIN_Y (w), WIN_W (w));
	}
	else
	{
	    modelInitObjects (ww->model,
			      WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));
	}

	modelInitSprings (ww->model,
			  WIN_X (w), WIN_Y (w), WIN_W (w), WIN_H (w));
    }

    /* update grab */
    if (ww->model && ww->grabbed)
    {
	if (ww->model->anchorObject)
	    ww->model->anchorObject->immobile = FALSE;

	ww->model->anchorObject = modelFindNearestObject (ww->model,
							  pointerX,
							  pointerY);
	ww->model->anchorObject->immobile = TRUE;

	modelAdjustObjectPosition (ww->model,
				   ww->model->anchorObject,
				   WIN_X (w), WIN_Y (w),
				   WIN_W (w), WIN_H (w));
    }

    UNWRAP (ws, w->screen, windowResizeNotify);
    (*w->screen->windowResizeNotify) (w, dx, dy, dwidth, dheight);
    WRAP (ws, w->screen, windowResizeNotify, wobblyWindowResizeNotify);
}

static void
wobblyWindowMoveNotify (CompWindow *w,
			int	   dx,
			int	   dy,
			Bool	   immediate)
{
    WOBBLY_SCREEN (w->screen);
    WOBBLY_WINDOW (w);

    if (ww->model)
    {
	if (ww->grabbed && !immediate)
	{
	    if (ww->state & MAXIMIZE_STATE)
	    {
		int i;

		for (i = 0; i < ww->model->numObjects; i++)
		{
		    if (ww->model->objects[i].immobile)
		    {
			ww->model->objects[i].position.x += dx;
			ww->model->objects[i].position.y += dy;
		    }
		}
	    }
	    else
	    {
		ww->model->anchorObject->position.x += dx;
		ww->model->anchorObject->position.y += dy;
	    }

	    ww->wobbly |= WobblyInitial;
	    ws->wobblyWindows |= ww->wobbly;

	    damagePendingOnScreen (w->screen);
	}
	else
	    modelMove (ww->model, dx, dy);
    }

    UNWRAP (ws, w->screen, windowMoveNotify);
    (*w->screen->windowMoveNotify) (w, dx, dy, immediate);
    WRAP (ws, w->screen, windowMoveNotify, wobblyWindowMoveNotify);
}

static void
wobblyWindowGrabNotify (CompWindow   *w,
			int	     x,
			int	     y,
			unsigned int state,
			unsigned int mask)
{
    int mIndex;

    WOBBLY_SCREEN (w->screen);

    mIndex = WOBBLY_SCREEN_OPTION_MOVE_WINDOW_MATCH;

    if (!ws->grabWindow)
    {
	ws->grabMask   = mask;
	ws->grabWindow = w;
    }
    ws->moveWindow = FALSE;

    if ((mask & CompWindowGrabButtonMask)	    &&
	matchEval (&ws->opt[mIndex].value.match, w) &&
	isWobblyWin (w))
    {
	WOBBLY_WINDOW (w);

	ws->moveWindow = TRUE;

	if (wobblyEnsureModel (w))
	{
	    Spring *s;
	    int	   i;
	    CompPlugin *pMove;

	    WOBBLY_DISPLAY (w->screen->display);

	    if (ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
	    {
		if (w->state & MAXIMIZE_STATE)
		{
		    modelAddEdgeAnchors (ww->model,
					 WIN_X (w), WIN_Y (w),
					 WIN_W (w), WIN_H (w));
		}
		else
		{
		    modelRemoveEdgeAnchors (ww->model,
					    WIN_X (w), WIN_Y (w),
					    WIN_W (w), WIN_H (w));

		    if (ww->model->anchorObject)
			ww->model->anchorObject->immobile = FALSE;
		}
	    }
	    else
	    {
		if (ww->model->anchorObject)
		    ww->model->anchorObject->immobile = FALSE;
	    }

	    ww->model->anchorObject = modelFindNearestObject (ww->model, x, y);
	    ww->model->anchorObject->immobile = TRUE;

	    ww->grabbed = TRUE;

	    /* Update yConstrained and workArea at grab time */
	    wd->yConstrained = FALSE;
	    if (mask & CompWindowGrabExternalAppMask)
	    {
		pMove = findActivePlugin ("move");
		if (pMove && pMove->vTable->getObjectOptions)
		{
		    int        nOption = 0;
		    CompOption *moveOptions;

		    moveOptions = (*pMove->vTable->getObjectOptions)
			(pMove, &core.displays->base, &nOption);

		    wd->yConstrained = getBoolOptionNamed (moveOptions, nOption,
							   "constrain_y", TRUE);
		}
	    }

	    if (wd->yConstrained)
	    {
		int output = outputDeviceForWindow (w);
		ws->grabWindowWorkArea = &w->screen->outputDev[output].workArea;
	    }

	    if (mask & CompWindowGrabMoveMask)
	    {
		modelDisableSnapping (w, ww->model);
		if (wd->snapping)
		    modelUpdateSnapping (w, ww->model);
	    }

	    mIndex = WOBBLY_SCREEN_OPTION_GRAB_WINDOW_MATCH;

	    if (matchEval (&ws->opt[mIndex].value.match, w))
	    {
		for (i = 0; i < ww->model->numSprings; i++)
		{
		    s = &ww->model->springs[i];

		    if (s->a == ww->model->anchorObject)
		    {
			s->b->velocity.x -= s->offset.x * 0.05f;
			s->b->velocity.y -= s->offset.y * 0.05f;
		    }
		    else if (s->b == ww->model->anchorObject)
		    {
			s->a->velocity.x += s->offset.x * 0.05f;
			s->a->velocity.y += s->offset.y * 0.05f;
		    }
		}

		ww->wobbly |= WobblyInitial;
		ws->wobblyWindows |= ww->wobbly;

		damagePendingOnScreen (w->screen);
	    }
	}
    }

    UNWRAP (ws, w->screen, windowGrabNotify);
    (*w->screen->windowGrabNotify) (w, x, y, state, mask);
    WRAP (ws, w->screen, windowGrabNotify, wobblyWindowGrabNotify);
}

static void
wobblyWindowUngrabNotify (CompWindow *w)
{
    WOBBLY_SCREEN (w->screen);
    WOBBLY_WINDOW (w);

    if (w == ws->grabWindow)
    {
	ws->grabMask   = 0;
	ws->grabWindow = NULL;
    }

    if (ww->grabbed)
    {
	if (ww->model)
	{
	    if (ww->model->anchorObject)
		ww->model->anchorObject->immobile = FALSE;

	    ww->model->anchorObject = NULL;

	    if (ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
	    {
		if (ww->state & MAXIMIZE_STATE)
		    modelAddEdgeAnchors (ww->model,
					 WIN_X (w), WIN_Y (w),
					 WIN_W (w), WIN_H (w));
	    }

	    ww->wobbly |= WobblyInitial;
	    ws->wobblyWindows |= ww->wobbly;

	    damagePendingOnScreen (w->screen);
	}

	ww->grabbed = FALSE;
    }

    UNWRAP (ws, w->screen, windowUngrabNotify);
    (*w->screen->windowUngrabNotify) (w);
    WRAP (ws, w->screen, windowUngrabNotify, wobblyWindowUngrabNotify);
}


static Bool
wobblyPaintOutput (CompScreen		   *s,
		   const ScreenPaintAttrib *sAttrib,
		   const CompTransform	   *transform,
		   Region		   region,
		   CompOutput		   *output,
		   unsigned int		   mask)
{
    Bool status;

    WOBBLY_SCREEN (s);

    if (ws->wobblyWindows)
	mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_MASK;

    UNWRAP (ws, s, paintOutput);
    status = (*s->paintOutput) (s, sAttrib, transform, region, output, mask);
    WRAP (ws, s, paintOutput, wobblyPaintOutput);

    return status;
}

static CompOption *
wobblyGetDisplayOptions (CompPlugin  *plugin,
			 CompDisplay *display,
			 int	     *count)
{
    WOBBLY_DISPLAY (display);

    *count = NUM_OPTIONS (wd);
    return wd->opt;
}

static Bool
wobblySetDisplayOption (CompPlugin      *plugin,
			CompDisplay     *display,
			const char	*name,
			CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    WOBBLY_DISPLAY (display);

    o = compFindOption (wd->opt, NUM_OPTIONS (wd), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case WOBBLY_DISPLAY_OPTION_SNAP_INVERTED:
	if (compSetBoolOption (o, value))
	{
	    if (value->b)
		wobblyEnableSnapping (display, NULL, 0, NULL, 0);
	    else
		wobblyDisableSnapping (display, NULL, 0, NULL, 0);
	    return TRUE;
	}
	break;
    case WOBBLY_DISPLAY_OPTION_SNAP_KEY:
	/* ignore the key */
	value->action.key.keycode = 0;

	if (compSetActionOption (o, value))
	    return TRUE;
	break;
    default:
	return compSetDisplayOption (display, o, value);
    }

    return FALSE;
}

static const CompMetadataOptionInfo wobblyDisplayOptionInfo[] = {
    { "snap_key", "key", "<passive_grab>false</passive_grab>",
      wobblyEnableSnapping, wobblyDisableSnapping },
    { "snap_inverted", "bool", 0, 0, 0 },
    { "shiver", "bell", 0, wobblyShiver, 0 }
};

static Bool
wobblyInitDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    WobblyDisplay *wd;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    wd = malloc (sizeof (WobblyDisplay));
    if (!wd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &wobblyMetadata,
					     wobblyDisplayOptionInfo,
					     wd->opt,
					     WOBBLY_DISPLAY_OPTION_NUM))
    {
	free (wd);
	return FALSE;
    }

    wd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (wd->screenPrivateIndex < 0)
    {
	compFiniDisplayOptions (d, wd->opt, WOBBLY_DISPLAY_OPTION_NUM);
	free (wd);
	return FALSE;
    }

    WRAP (wd, d, handleEvent, wobblyHandleEvent);

    wd->snapping = FALSE;
    wd->yConstrained = FALSE;

    d->base.privates[displayPrivateIndex].ptr = wd;

    return TRUE;
}

static void
wobblyFiniDisplay (CompPlugin  *p,
		   CompDisplay *d)
{
    WOBBLY_DISPLAY (d);

    freeScreenPrivateIndex (d, wd->screenPrivateIndex);

    UNWRAP (wd, d, handleEvent);

    compFiniDisplayOptions (d, wd->opt, WOBBLY_DISPLAY_OPTION_NUM);

    free (wd);
}

static Bool
wobblyInitScreen (CompPlugin *p,
		  CompScreen *s)
{
    WobblyScreen *ws;

    WOBBLY_DISPLAY (s->display);

    ws = malloc (sizeof (WobblyScreen));
    if (!ws)
	return FALSE;

    if (!compInitScreenOptionsFromMetadata (s,
					    &wobblyMetadata,
					    wobblyScreenOptionInfo,
					    ws->opt,
					    WOBBLY_SCREEN_OPTION_NUM))
    {
	free (ws);
	return FALSE;
    }

    ws->windowPrivateIndex = allocateWindowPrivateIndex (s);
    if (ws->windowPrivateIndex < 0)
    {
	compFiniScreenOptions (s, ws->opt, WOBBLY_SCREEN_OPTION_NUM);
	free (ws);
	return FALSE;
    }

    ws->wobblyWindows = FALSE;

    ws->grabMask   = 0;
    ws->grabWindow = NULL;
    ws->moveWindow = FALSE;

    ws->grabWindowWorkArea = NULL;

    WRAP (ws, s, preparePaintScreen, wobblyPreparePaintScreen);
    WRAP (ws, s, donePaintScreen, wobblyDonePaintScreen);
    WRAP (ws, s, paintOutput, wobblyPaintOutput);
    WRAP (ws, s, paintWindow, wobblyPaintWindow);
    WRAP (ws, s, damageWindowRect, wobblyDamageWindowRect);
    WRAP (ws, s, addWindowGeometry, wobblyAddWindowGeometry);
    WRAP (ws, s, windowResizeNotify, wobblyWindowResizeNotify);
    WRAP (ws, s, windowMoveNotify, wobblyWindowMoveNotify);
    WRAP (ws, s, windowGrabNotify, wobblyWindowGrabNotify);
    WRAP (ws, s, windowUngrabNotify, wobblyWindowUngrabNotify);

    s->base.privates[wd->screenPrivateIndex].ptr = ws;

    return TRUE;
}

static void
wobblyFiniScreen (CompPlugin *p,
		  CompScreen *s)
{
    WOBBLY_SCREEN (s);

    freeWindowPrivateIndex (s, ws->windowPrivateIndex);

    UNWRAP (ws, s, preparePaintScreen);
    UNWRAP (ws, s, donePaintScreen);
    UNWRAP (ws, s, paintOutput);
    UNWRAP (ws, s, paintWindow);
    UNWRAP (ws, s, damageWindowRect);
    UNWRAP (ws, s, addWindowGeometry);
    UNWRAP (ws, s, windowResizeNotify);
    UNWRAP (ws, s, windowMoveNotify);
    UNWRAP (ws, s, windowGrabNotify);
    UNWRAP (ws, s, windowUngrabNotify);

    compFiniScreenOptions (s, ws->opt, WOBBLY_SCREEN_OPTION_NUM);

    free (ws);
}

static Bool
wobblyInitWindow (CompPlugin *p,
		  CompWindow *w)
{
    WobblyWindow *ww;

    WOBBLY_SCREEN (w->screen);

    ww = malloc (sizeof (WobblyWindow));
    if (!ww)
	return FALSE;

    ww->model   = 0;
    ww->wobbly  = 0;
    ww->grabbed = FALSE;
    ww->state   = w->state;

    w->base.privates[ws->windowPrivateIndex].ptr = ww;

    if (w->mapNum && ws->opt[WOBBLY_SCREEN_OPTION_MAXIMIZE_EFFECT].value.b)
    {
	if (isWobblyWin (w))
	    wobblyEnsureModel (w);
    }

    return TRUE;
}

static void
wobblyFiniWindow (CompPlugin *p,
		  CompWindow *w)
{
    WOBBLY_WINDOW (w);
    WOBBLY_SCREEN (w->screen);

    if (ws->grabWindow == w)
    {
	ws->grabWindow = NULL;
	ws->grabMask   = 0;
    }

    if (ww->model)
    {
	free (ww->model->objects);
	free (ww->model);
    }

    free (ww);
}

static CompBool
wobblyInitObject (CompPlugin *p,
		 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) wobblyInitDisplay,
	(InitPluginObjectProc) wobblyInitScreen,
	(InitPluginObjectProc) wobblyInitWindow
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
wobblyFiniObject (CompPlugin *p,
		 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) wobblyFiniDisplay,
	(FiniPluginObjectProc) wobblyFiniScreen,
	(FiniPluginObjectProc) wobblyFiniWindow
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
wobblyGetObjectOptions (CompPlugin *plugin,
		       CompObject *object,
		       int	  *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) wobblyGetDisplayOptions,
	(GetPluginObjectOptionsProc) wobblyGetScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (plugin, object, count));
}

static CompBool
wobblySetObjectOption (CompPlugin      *plugin,
		      CompObject      *object,
		      const char      *name,
		      CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) wobblySetDisplayOption,
	(SetPluginObjectOptionProc) wobblySetScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static Bool
wobblyInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&wobblyMetadata,
					 p->vTable->name,
					 wobblyDisplayOptionInfo,
					 WOBBLY_DISPLAY_OPTION_NUM,
					 wobblyScreenOptionInfo,
					 WOBBLY_SCREEN_OPTION_NUM))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&wobblyMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&wobblyMetadata, p->vTable->name);

    return TRUE;
}

static void
wobblyFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&wobblyMetadata);
}

static CompMetadata *
wobblyGetMetadata (CompPlugin *plugin)
{
    return &wobblyMetadata;
}

CompPluginVTable wobblyVTable = {
    "wobbly",
    wobblyGetMetadata,
    wobblyInit,
    wobblyFini,
    wobblyInitObject,
    wobblyFiniObject,
    wobblyGetObjectOptions,
    wobblySetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &wobblyVTable;
}
