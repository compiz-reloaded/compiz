/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2002, 2003 Red Hat, Inc.
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2005 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <math.h>
#include <stdlib.h>

#include <compiz.h>

#include <glib.h>

#define PLACE_WORKAROUND_DEFAULT TRUE

static int displayPrivateIndex;

typedef struct _PlaceDisplay {
    int		    screenPrivateIndex;
    HandleEventProc handleEvent;
} PlaceDisplay;

#define PLACE_SCREEN_OPTION_WORKAROUND 0
#define PLACE_SCREEN_OPTION_NUM        1

typedef struct _PlaceScreen {
    CompOption opt[PLACE_SCREEN_OPTION_NUM];

    DamageWindowRectProc damageWindowRect;
} PlaceScreen;

#define GET_PLACE_DISPLAY(d)				      \
    ((PlaceDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define PLACE_DISPLAY(d)		     \
    PlaceDisplay *pd = GET_PLACE_DISPLAY (d)

#define GET_PLACE_SCREEN(s, pd)					  \
    ((PlaceScreen *) (s)->privates[(pd)->screenPrivateIndex].ptr)

#define PLACE_SCREEN(s)							   \
    PlaceScreen *ps = GET_PLACE_SCREEN (s, GET_PLACE_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static CompOption *
placeGetScreenOptions (CompScreen *screen,
		       int	  *count)
{
    PLACE_SCREEN (screen);

    *count = NUM_OPTIONS (ps);
    return ps->opt;
}

static Bool
placeSetScreenOption (CompScreen      *screen,
		      char	      *name,
		      CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    PLACE_SCREEN (screen);

    o = compFindOption (ps->opt, NUM_OPTIONS (ps), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case PLACE_SCREEN_OPTION_WORKAROUND:
	if (compSetBoolOption (o, value))
	    return TRUE;
    default:
	break;
    }

    return FALSE;
}

static void
placeScreenInitOptions (PlaceScreen *ps)
{
    CompOption *o;

    o = &ps->opt[PLACE_SCREEN_OPTION_WORKAROUND];
    o->name	 = "workarounds";
    o->shortDesc = N_("Workarounds");
    o->longDesc	 = N_("Window placement workarounds");
    o->type	 = CompOptionTypeBool;
    o->value.b	 = PLACE_WORKAROUND_DEFAULT;
}

typedef enum {
    PlaceLeft,
    PlaceRight,
    PlaceTop,
    PlaceBottom
} PlaceWindowDirection;

static Bool
rectangleIntersect (XRectangle *src1,
		    XRectangle *src2,
		    XRectangle *dest)
{
    int dest_x, dest_y;
    int dest_w, dest_h;
    int return_val;

    g_return_val_if_fail (src1 != NULL, FALSE);
    g_return_val_if_fail (src2 != NULL, FALSE);
    g_return_val_if_fail (dest != NULL, FALSE);

    return_val = FALSE;

    dest_x = MAX (src1->x, src2->x);
    dest_y = MAX (src1->y, src2->y);
    dest_w = MIN (src1->x + src1->width, src2->x + src2->width) - dest_x;
    dest_h = MIN (src1->y + src1->height, src2->y + src2->height) - dest_y;

    if (dest_w > 0 && dest_h > 0)
    {
	dest->x = dest_x;
	dest->y = dest_y;
	dest->width = dest_w;
	dest->height = dest_h;
	return_val = TRUE;
    }
    else
    {
	dest->width = 0;
	dest->height = 0;
    }

    return return_val;
}

static gint
northwestcmp (gconstpointer a,
	      gconstpointer b)
{
    CompWindow *aw = (gpointer) a;
    CompWindow *bw = (gpointer) b;
    int	       from_origin_a;
    int	       from_origin_b;
    int	       ax, ay, bx, by;

    ax = aw->attrib.x - aw->input.left;
    ay = aw->attrib.y - aw->input.top;

    bx = bw->attrib.x - bw->input.left;
    by = bw->attrib.y - bw->input.top;

    /* probably there's a fast good-enough-guess we could use here. */
    from_origin_a = sqrt (ax * ax + ay * ay);
    from_origin_b = sqrt (bx * bx + by * by);

    if (from_origin_a < from_origin_b)
	return -1;
    else if (from_origin_a > from_origin_b)
	return 1;
    else
	return 0;
}


static void
get_workarea_of_current_output_device (CompScreen *s,
				       XRectangle *area)
{
    int x1, y1, x2, y2;
    int oX1, oY1, oX2, oY2;

    x1 = s->workArea.x;
    y1 = s->workArea.y;
    x2 = x1 + s->workArea.width;
    y2 = y1 + s->workArea.height;

    oX1 = s->outputDev[s->currentOutputDev].region.extents.x1;
    oY1 = s->outputDev[s->currentOutputDev].region.extents.y1;
    oX2 = s->outputDev[s->currentOutputDev].region.extents.x2;
    oY2 = s->outputDev[s->currentOutputDev].region.extents.y2;

    if (x1 < oX1)
	x1 = oX1;
    if (y1 < oY1)
	y1 = oY1;
    if (x2 > oX2)
	x2 = oX2;
    if (y2 > oY2)
	y2 = oY2;

    area->x      = x1;
    area->y      = y1;
    area->width  = x2 - x1;
    area->height = y2 - y1;
}

static void
find_next_cascade (CompWindow *window,
		   GList      *windows,
		   int        x,
		   int        y,
		   int        *new_x,
		   int        *new_y)
{
    GList      *tmp;
    GList      *sorted;
    int	       cascade_x, cascade_y;
    int	       x_threshold, y_threshold;
    int	       window_width, window_height;
    int	       cascade_stage;
    XRectangle work_area;

    sorted = g_list_copy (windows);
    sorted = g_list_sort (sorted, northwestcmp);

    /* This is a "fuzzy" cascade algorithm.
     * For each window in the list, we find where we'd cascade a
     * new window after it. If a window is already nearly at that
     * position, we move on.
     */

    /* arbitrary-ish threshold, honors user attempts to
     * manually cascade.
     */
#define CASCADE_FUZZ 15

    x_threshold = MAX (window->input.left, CASCADE_FUZZ);
    y_threshold = MAX (window->input.top, CASCADE_FUZZ);

    /* Find furthest-SE origin of all workspaces.
     * cascade_x, cascade_y are the target position
     * of NW corner of window frame.
     */

    get_workarea_of_current_output_device (window->screen, &work_area);

    cascade_x = MAX (0, work_area.x);
    cascade_y = MAX (0, work_area.y);

    /* Find first cascade position that's not used. */

    window_width = window->attrib.width + window->input.left +
	window->input.right;
    window_height = window->attrib.height + window->input.top +
	window->input.bottom;

    cascade_stage = 0;
    tmp = sorted;
    while (tmp != NULL)
    {
	CompWindow *w;
	int	   wx, wy;

	w = tmp->data;

	/* we want frame position, not window position */
	wx = w->attrib.x - w->input.left;
	wy = w->attrib.y - w->input.top;

	if (ABS (wx - cascade_x) < x_threshold &&
	    ABS (wy - cascade_y) < y_threshold)
	{
	    /* This window is "in the way", move to next cascade
	     * point. The new window frame should go at the origin
	     * of the client window we're stacking above.
	     */
	    wx = w->attrib.x;
	    wy = w->attrib.y;

	    cascade_x = wx;
	    cascade_y = wy;

	    /* If we go off the screen, start over with a new cascade */
	    if (((cascade_x + window_width) >
		 (work_area.x + work_area.width)) ||
		((cascade_y + window_height) >
		 (work_area.y + work_area.height)))
	    {
		cascade_x = MAX (0, work_area.x);
		cascade_y = MAX (0, work_area.y);

#define CASCADE_INTERVAL 50 /* space between top-left corners of cascades */

		cascade_stage += 1;
		cascade_x += CASCADE_INTERVAL * cascade_stage;

		/* start over with a new cascade translated to the right,
		 * unless we are out of space
		 */
		if ((cascade_x + window_width) <
		    (work_area.x + work_area.width))
		{
		    tmp = sorted;
		    continue;
		}
		else
		{
		    /* All out of space, this cascade_x won't work */
		    cascade_x = MAX (0, work_area.x);
		    break;
		}
	    }
	}
	else
	{
	    /* Keep searching for a further-down-the-diagonal window. */
	}

	tmp = tmp->next;
    }

    /* cascade_x and cascade_y will match the last window in the list
     * that was "in the way" (in the approximate cascade diagonal)
     */

    g_list_free (sorted);

    /* Convert coords to position of window, not position of frame. */
    *new_x = cascade_x + window->input.left;
    *new_y = cascade_y + window->input.top;
}

static void
find_most_freespace (CompWindow *window,
		     CompWindow *focus_window,
		     int        x,
		     int        y,
		     int        *new_x,
		     int        *new_y)
{
    PlaceWindowDirection side;
    int			 max_area;
    int			 max_width, max_height, left, right, top, bottom;
    int			 left_space, right_space, top_space, bottom_space;
    int			 frame_size_left, frame_size_top;
    XRectangle		 work_area;
    XRectangle		 avoid;
    XRectangle		 outer;

    frame_size_left = window->input.left;
    frame_size_top  = window->input.top;

    get_workarea_of_current_output_device (window->screen, &work_area);

    getOuterRectOfWindow (focus_window, &avoid);
    getOuterRectOfWindow (window, &outer);

    /* Find the areas of choosing the various sides of the focus window */
    max_width  = MIN (avoid.width, outer.width);
    max_height = MIN (avoid.height, outer.height);
    left_space   = avoid.x - work_area.x;
    right_space  = work_area.width - (avoid.x + avoid.width - work_area.x);
    top_space    = avoid.y - work_area.y;
    bottom_space = work_area.height - (avoid.y + avoid.height - work_area.y);
    left   = MIN (left_space,   outer.width);
    right  = MIN (right_space,  outer.width);
    top    = MIN (top_space,    outer.height);
    bottom = MIN (bottom_space, outer.height);

    /* Find out which side of the focus_window can show the most of the
     * window
     */
    side = PlaceLeft;
    max_area = left * max_height;
    if (right * max_height > max_area)
    {
	side = PlaceRight;
	max_area = right * max_height;
    }
    if (top * max_width > max_area)
    {
	side = PlaceTop;
	max_area = top * max_width;
    }
    if (bottom * max_width > max_area)
    {
	side = PlaceBottom;
	max_area = bottom * max_width;
    }

    /* Give up if there's no where to put it
     * (i.e. focus window is maximized)
     */
    if (max_area == 0)
	return;

    /* Place the window on the relevant side; if the whole window fits,
     * make it adjacent to the focus window; if not, make sure the
     * window doesn't go off the edge of the screen.
     */
    switch (side) {
    case PlaceLeft:
	*new_y = avoid.y + frame_size_top;
	if (left_space > outer.width)
	    *new_x = avoid.x - outer.width + frame_size_left;
	else
	    *new_x = work_area.x + frame_size_left;
	break;
    case PlaceRight:
	*new_y = avoid.y + frame_size_top;
	if (right_space > outer.width)
	    *new_x = avoid.x + avoid.width + frame_size_left;
	else
	    *new_x = work_area.x + work_area.width - outer.width +
		frame_size_left;
	break;
    case PlaceTop:
	*new_x = avoid.x + frame_size_left;
	if (top_space > outer.height)
	    *new_y = avoid.y - outer.height + frame_size_top;
	else
	    *new_y = work_area.y + frame_size_top;
	break;
    case PlaceBottom:
	*new_x = avoid.x + frame_size_left;
	if (bottom_space > outer.height)
	    *new_y = avoid.y + avoid.height + frame_size_top;
	else
	    *new_y = work_area.y + work_area.height - outer.height +
		frame_size_top;
	break;
    }
}

static void
avoid_being_obscured_as_second_modal_dialog (CompWindow *window,
					     int        *x,
					     int        *y)
{
    /* We can't center this dialog if it was denied focus and it
     * overlaps with the focus window and this dialog is modal and this
     * dialog is in the same app as the focus window (*phew*...please
     * don't make me say that ten times fast). See bug 307875 comment 11
     * and 12 for details, but basically it means this is probably a
     * second modal dialog for some app while the focus window is the
     * first modal dialog.  We should probably make them simultaneously
     * visible in general, but it becomes mandatory to do so due to
     * buggy apps (e.g. those using gtk+ *sigh*) because in those cases
     * this second modal dialog also happens to be modal to the first
     * dialog in addition to the main window, while it has only let us
     * know about the modal-to-the-main-window part.
     */

    CompWindow *focus_window;

    focus_window =
	findWindowAtDisplay (window->screen->display,
			     window->screen->display->activeWindow);

    if (focus_window				   &&
	(window->state & CompWindowStateModalMask) &&
	0 /* window->denied_focus_and_not_transient	       &&
	window_same_application (window, focus_window) &&
	window_intersect (window, focus_window) */
	)
    {
	find_most_freespace (window, focus_window, *x, *y, x, y);
    }
}

static gboolean
rectangle_overlaps_some_window (XRectangle *rect,
				GList      *windows)
{
    GList *tmp;
    XRectangle dest;

    tmp = windows;
    while (tmp != NULL)
    {
	CompWindow *other = tmp->data;
	XRectangle other_rect;

	switch (other->type) {
	case CompWindowTypeDockMask:
	case CompWindowTypeSplashMask:
	case CompWindowTypeDesktopMask:
	case CompWindowTypeDialogMask:
	case CompWindowTypeModalDialogMask:
	case CompWindowTypeFullscreenMask:
	case CompWindowTypeUnknownMask:
	    break;
	case CompWindowTypeNormalMask:
	case CompWindowTypeUtilMask:
	case CompWindowTypeToolbarMask:
	case CompWindowTypeMenuMask:
	    getOuterRectOfWindow (other, &other_rect);

	    if (rectangleIntersect (rect, &other_rect, &dest))
		return TRUE;
	    break;
	}

	tmp = tmp->next;
    }

    return FALSE;
}

static gint
leftmost_cmp (gconstpointer a,
	      gconstpointer b)
{
    CompWindow *aw = (gpointer) a;
    CompWindow *bw = (gpointer) b;
    int	       ax, bx;

    ax = aw->attrib.x - aw->input.left;
    bx = bw->attrib.x - bw->input.left;

    if (ax < bx)
	return -1;
    else if (ax > bx)
	return 1;
    else
	return 0;
}

static gint
topmost_cmp (gconstpointer a,
	     gconstpointer b)
{
    CompWindow *aw = (gpointer) a;
    CompWindow *bw = (gpointer) b;
    int	       ay, by;

    ay = aw->attrib.y - aw->input.top;
    by = bw->attrib.y - bw->input.top;

    if (ay < by)
	return -1;
    else if (ay > by)
	return 1;
    else
	return 0;
}

static void
center_tile_rect_in_area (XRectangle *rect,
			  XRectangle *work_area)
{
    int fluff;

    /* The point here is to tile a window such that "extra"
     * space is equal on either side (i.e. so a full screen
     * of windows tiled this way would center the windows
     * as a group)
     */

    fluff = (work_area->width % (rect->width + 1)) / 2;
    rect->x = work_area->x + fluff;
    fluff = (work_area->height % (rect->height + 1)) / 3;
    rect->y = work_area->y + fluff;
}

static gboolean
rect_fits_in_work_area (XRectangle *work_area,
			XRectangle *rect)
{
    return ((rect->x >= work_area->x) &&
	    (rect->y >= work_area->y) &&
	    (rect->x + rect->width <= work_area->x + work_area->width) &&
	    (rect->y + rect->height <= work_area->y + work_area->height));
}

/* Find the leftmost, then topmost, empty area on the workspace
 * that can contain the new window.
 *
 * Cool feature to have: if we can't fit the current window size,
 * try shrinking the window (within geometry constraints). But
 * beware windows such as Emacs with no sane minimum size, we
 * don't want to create a 1x1 Emacs.
 */
static gboolean
find_first_fit (CompWindow *window,
		GList      *windows,
		int        x,
		int        y,
		int        *new_x,
		int        *new_y)
{
    /* This algorithm is limited - it just brute-force tries
     * to fit the window in a small number of locations that are aligned
     * with existing windows. It tries to place the window on
     * the bottom of each existing window, and then to the right
     * of each existing window, aligned with the left/top of the
     * existing window in each of those cases.
     */
    int	       retval;
    GList      *below_sorted;
    GList      *right_sorted;
    GList      *tmp;
    XRectangle rect;
    XRectangle work_area;

    retval = FALSE;

    /* Below each window */
    below_sorted = g_list_copy (windows);
    below_sorted = g_list_sort (below_sorted, leftmost_cmp);
    below_sorted = g_list_sort (below_sorted, topmost_cmp);

    /* To the right of each window */
    right_sorted = g_list_copy (windows);
    right_sorted = g_list_sort (right_sorted, topmost_cmp);
    right_sorted = g_list_sort (right_sorted, leftmost_cmp);

    getOuterRectOfWindow (window, &rect);

    get_workarea_of_current_output_device (window->screen, &work_area);

    work_area.x += (window->initialViewport - window->screen->x) *
	window->screen->width;

    center_tile_rect_in_area (&rect, &work_area);

    if (rect_fits_in_work_area (&work_area, &rect) &&
	!rectangle_overlaps_some_window (&rect, windows))
    {
	*new_x = rect.x + window->input.left;
	*new_y = rect.y + window->input.top;

	retval = TRUE;

	goto out;
    }

    /* try below each window */
    tmp = below_sorted;
    while (tmp != NULL)
    {
	CompWindow *w = tmp->data;
	XRectangle outer_rect;

	getOuterRectOfWindow (w, &outer_rect);

	rect.x = outer_rect.x;
	rect.y = outer_rect.y + outer_rect.height;

	if (rect_fits_in_work_area (&work_area, &rect) &&
	    !rectangle_overlaps_some_window (&rect, below_sorted))
	{
	    *new_x = rect.x + window->input.left;
	    *new_y = rect.y + window->input.top;

	    retval = TRUE;

	    goto out;
	}

	tmp = tmp->next;
    }

    /* try to the right of each window */
    tmp = right_sorted;
    while (tmp != NULL)
    {
	CompWindow *w = tmp->data;
	XRectangle outer_rect;

	getOuterRectOfWindow (w, &outer_rect);

	rect.x = outer_rect.x + outer_rect.width;
	rect.y = outer_rect.y;

	if (rect_fits_in_work_area (&work_area, &rect) &&
	    !rectangle_overlaps_some_window (&rect, right_sorted))
	{
	    *new_x = rect.x + window->input.left;
	    *new_y = rect.y + window->input.top;

	    retval = TRUE;

	    goto out;
	}

	tmp = tmp->next;
    }

out:
    g_list_free (below_sorted);
    g_list_free (right_sorted);

    return retval;
}

static void
placeWindow (CompWindow *window,
	     int        x,
	     int        y,
	     int        *new_x,
	     int        *new_y)
{
    CompWindow *wi;
    GList      *windows;
    XRectangle work_area;
    int	       x0 = (window->initialViewport - window->screen->x) *
	window->screen->width;

    PLACE_SCREEN (window->screen);

    get_workarea_of_current_output_device (window->screen, &work_area);

    work_area.x += x0;

    windows = NULL;

    switch (window->type) {
    case CompWindowTypeSplashMask:
    case CompWindowTypeDialogMask:
    case CompWindowTypeModalDialogMask:
    case CompWindowTypeNormalMask:
	/* Run placement algorithm on these. */
	break;
    case CompWindowTypeDockMask:
    case CompWindowTypeDesktopMask:
    case CompWindowTypeUtilMask:
    case CompWindowTypeToolbarMask:
    case CompWindowTypeMenuMask:
    case CompWindowTypeFullscreenMask:
    case CompWindowTypeUnknownMask:
	/* Assume the app knows best how to place these, no placement
	 * algorithm ever (other than "leave them as-is")
	 */
	goto done_no_constraints;
	break;
    }

    /* don't run placement algorithm on windows that can't be moved */
    if (!(window->actions & CompWindowActionMoveMask))
    {
	goto done_no_constraints;
    }

    if (window->type & CompWindowTypeFullscreenMask)
    {
	x = x0;
	y = 0;
	goto done_no_constraints;
    }

    if (window->state & (CompWindowStateMaximizedVertMask |
			 CompWindowStateMaximizedHorzMask))
    {
	if (window->state & CompWindowStateMaximizedVertMask)
	    y = work_area.y + window->input.top;

	if (window->state & CompWindowStateMaximizedHorzMask)
	    x = work_area.x + window->input.left;

	goto done;
    }

    if (ps->opt[PLACE_SCREEN_OPTION_WORKAROUND].value.b)
    {
	/* workarounds enabled */

	if ((window->sizeHints.flags & PPosition) ||
	    (window->sizeHints.flags & USPosition))
	{
	    avoid_being_obscured_as_second_modal_dialog (window, &x, &y);
	    goto done;
	}
    }
    else
    {
	switch (window->type) {
	case CompWindowTypeNormalMask:
	    /* Only accept USPosition on normal windows because the app is full
	     * of shit claiming the user set -geometry for a dialog or dock
	     */
	    if (window->sizeHints.flags & USPosition)
	    {
		/* don't constrain with placement algorithm */
		goto done;
	    }
	    break;
	case CompWindowTypeSplashMask:
	case CompWindowTypeDialogMask:
	case CompWindowTypeModalDialogMask:
	    /* Ignore even USPosition on dialogs, splashscreen */
	    break;
	case CompWindowTypeDockMask:
	case CompWindowTypeDesktopMask:
	case CompWindowTypeUtilMask:
	case CompWindowTypeToolbarMask:
	case CompWindowTypeMenuMask:
	case CompWindowTypeFullscreenMask:
	case CompWindowTypeUnknownMask:
	    /* Assume the app knows best how to place these. */
	    if (window->sizeHints.flags & PPosition)
	    {
		goto done_no_constraints;
	    }
	    break;
	}
    }

    if ((window->type == CompWindowTypeDialogMask ||
	 window->type == CompWindowTypeModalDialogMask) &&
	window->transientFor != None)
    {
	/* Center horizontally, at top of parent vertically */

	CompWindow *parent;

	parent = findWindowAtDisplay (window->screen->display,
				      window->transientFor);
	if (parent)
	{
	    int	w;

	    x = parent->attrib.x;
	    y = parent->attrib.y;

	    w = parent->width;

	    /* center of parent */
	    x = x + w / 2;

	    /* center of child over center of parent */
	    x -= window->width / 2;

	    /* "visually" center window over parent, leaving twice as
	     * much space below as on top.
	     */
	    y += (parent->height - window->height) / 3;

	    /* put top of child's frame, not top of child's client */
	    y += window->input.top;

	    /* clip to screen if parent is visible in current viewport */
	    if (parent->attrib.x < parent->screen->width &&
		parent->attrib.x + parent->screen->width > 0)
	    {
		XRectangle area;

		get_workarea_of_current_output_device (window->screen, &area);

		if (x + window->width > area.x + area.width)
		    x = area.x + area.width - window->width;
		if (y + window->height > area.y + area.height)
		    y = area.y + area.height - window->height;
		if (x < area.x) x = area.x;
		if (y < area.y) y = area.y;
	    }

	    avoid_being_obscured_as_second_modal_dialog (window, &x, &y);

	    goto done_no_x_constraints;
	}
    }

    /* FIXME UTILITY with transient set should be stacked up
     * on the sides of the parent window or something.
     */
    if (window->type == CompWindowTypeDialogMask      ||
	window->type == CompWindowTypeModalDialogMask ||
	window->type == CompWindowTypeSplashMask)
    {
	/* Center on screen */
	int w, h;

	w = window->screen->width;
	h = window->screen->height;

	x = (w - window->width) / 2;
	y = (h - window->height) / 2;

	goto done_check_denied_focus;
    }

    /* Find windows that matter (not minimized, on same workspace
     * as placed window, may be shaded - if shaded we pretend it isn't
     * for placement purposes)
     */
    for (wi = window->screen->windows; wi; wi = wi->next)
    {
	if (!wi->shaded && wi->attrib.map_state != IsViewable)
	    continue;

	if (wi->attrib.x >= work_area.x + work_area.width  ||
	    wi->attrib.x + wi->width <= work_area.x	   ||
	    wi->attrib.y >= work_area.y + work_area.height ||
	    wi->attrib.y + wi->height <= work_area.y)
	    continue;

	if (wi->state & (CompWindowTypeDesktopMask    |
			 CompWindowTypeDockMask       |
			 CompWindowTypeFullscreenMask |
			 CompWindowTypeUnknownMask))
	    continue;

	if (wi != window)
	    windows = g_list_prepend (windows, wi);
    }

    /* "Origin" placement algorithm */
    x = x0;
    y = 0;

    if (find_first_fit (window, windows, x, y, &x, &y))
	goto done_check_denied_focus;

    /* if the window wasn't placed at the origin of screen,
     * cascade it onto the current screen
     */
    find_next_cascade (window, windows, x, y, &x, &y);

    /* Maximize windows if they are too big for their work area (bit of
     * a hack here). Assume undecorated windows probably don't intend to
     * be maximized.
     *
    if (window->has_maximize_func &&
	window->decorated	  &&
	!window->fullscreen)
    {
	XRectangle workarea;
	XRectangle outer;

	workarea = s->workArea;
	getOuterRectOfWindow (window, &outer);

	if (outer.width >= workarea.width &&
	    outer.height >= workarea.height)
	{
	    window->maximize_after_placement = TRUE;
	}
    }
    */

done_check_denied_focus:
    /* If the window is being denied focus and isn't a transient of the
     * focus window, we do NOT want it to overlap with the focus window
     * if at all possible.  This is guaranteed to only be called if the
     * focus_window is non-NULL, and we try to avoid that window.
     */
    if (0 /* window->denied_focus_and_not_transient */)
    {
	gboolean    found_fit = FALSE;
	CompWindow  *focus_window;

	focus_window =
	    findWindowAtDisplay (window->screen->display,
				 window->screen->display->activeWindow);
	if (focus_window)
	{
	    XRectangle wr, fwr, overlap;

	    getOuterRectOfWindow (window, &wr);
	    getOuterRectOfWindow (focus_window, &fwr);

	    /* No need to do anything if the window doesn't overlap at all */
	    found_fit = !rectangleIntersect (&wr, &fwr, &overlap);

	    /* Try to do a first fit again, this time only taking into
	     * account the focus window.
	     */
	    if (!found_fit)
	    {
		GList *focus_window_list;

		focus_window_list = g_list_prepend (NULL, focus_window);

		/* Reset x and y ("origin" placement algorithm) */
		x = 0;
		y = 0;

		found_fit = find_first_fit (window, focus_window_list,
					    x, y, &x, &y);

		g_list_free (focus_window_list);
	    }
	}

	/* If that still didn't work, just place it where we can see as much
	 * as possible.
	 */
	if (!found_fit)
	    find_most_freespace (window, focus_window, x, y, &x, &y);
    }

    g_list_free (windows);

done:
    if (x + window->width + window->input.right > work_area.x + work_area.width)
	x = work_area.x + work_area.width - window->width - window->input.right;

    if (x - window->input.left < work_area.x)
	x = work_area.x + window->input.left;

done_no_x_constraints:
    if (y + window->height + window->input.bottom >
	work_area.y + work_area.height)
	y = work_area.y + work_area.height
	    - window->height - window->input.bottom;

    if (y - window->input.top < work_area.y)
	y = work_area.y + window->input.top;

done_no_constraints:
    *new_x = x;
    *new_y = y;
}

static Bool
placeDamageWindowRect (CompWindow *w,
		       Bool	  initial,
		       BoxPtr     rect)
{
    Bool status;

    PLACE_SCREEN (w->screen);

    UNWRAP (ps, w->screen, damageWindowRect);
    status = (*w->screen->damageWindowRect) (w, initial, rect);
    WRAP (ps, w->screen, damageWindowRect, placeDamageWindowRect);

    if (initial && !w->attrib.override_redirect && !w->placed)
    {
	int newX, newY;

	placeWindow (w, w->attrib.x, w->attrib.y, &newX, &newY);

	w->placed = TRUE;

	if (newX != w->attrib.x || newY != w->attrib.y)
	{
	    moveWindow (w, newX - w->attrib.x, newY - w->attrib.y, FALSE, TRUE);
	    syncWindowPosition (w);
	}
    }

    return status;
}

static Bool
placeInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    PlaceDisplay *pd;

    pd = malloc (sizeof (PlaceDisplay));
    if (!pd)
	return FALSE;

    pd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (pd->screenPrivateIndex < 0)
    {
	free (pd);
	return FALSE;
    }

    d->privates[displayPrivateIndex].ptr = pd;

    return TRUE;
}

static void
placeFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    PLACE_DISPLAY (d);

    freeScreenPrivateIndex (d, pd->screenPrivateIndex);

    free (pd);
}

static Bool
placeInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    PlaceScreen *ps;

    PLACE_DISPLAY (s->display);

    ps = malloc (sizeof (PlaceScreen));
    if (!ps)
	return FALSE;

    placeScreenInitOptions (ps);

    WRAP (ps, s, damageWindowRect, placeDamageWindowRect);

    s->privates[pd->screenPrivateIndex].ptr = ps;

    return TRUE;
}

static void
placeFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    PLACE_SCREEN (s);

    UNWRAP (ps, s, damageWindowRect);

    free (ps);
}

static Bool
placeInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
placeFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
placeGetVersion (CompPlugin *plugin,
		 int	    version)
{
    return ABIVERSION;
}

static CompPluginVTable placeVTable = {
    "place",
    N_("Place Windows"),
    N_("Place windows at appropriate positions when mapped"),
    placeGetVersion,
    placeInit,
    placeFini,
    placeInitDisplay,
    placeFiniDisplay,
    placeInitScreen,
    placeFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    placeGetScreenOptions,
    placeSetScreenOption,
    0,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &placeVTable;
}
