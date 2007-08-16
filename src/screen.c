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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>
#include <X11/cursorfont.h>

#include <compiz.h>

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static int
reallocScreenPrivate (int  size,
		      void *closure)
{
    CompDisplay *d = (CompDisplay *) closure;
    CompScreen  *s;
    void        *privates;

    for (s = d->screens; s; s = s->next)
    {
	privates = realloc (s->privates, size * sizeof (CompPrivate));
	if (!privates)
	    return FALSE;

	s->privates = (CompPrivate *) privates;
    }

    return TRUE;
}

int
allocateScreenPrivateIndex (CompDisplay *display)
{
    return allocatePrivateIndex (&display->screenPrivateLen,
				 &display->screenPrivateIndices,
				 reallocScreenPrivate,
				 (void *) display);
}

void
freeScreenPrivateIndex (CompDisplay *display,
			int	    index)
{
    freePrivateIndex (display->screenPrivateLen,
		      display->screenPrivateIndices,
		      index);
}

static Bool
desktopHintEqual (CompScreen	*s,
		  unsigned long *data,
		  int		size,
		  int		offset,
		  int		hintSize)
{
    if (size != s->desktopHintSize)
	return FALSE;

    if (memcmp (data + offset,
		s->desktopHintData + offset,
		hintSize * sizeof (unsigned long)) == 0)
	return TRUE;

    return FALSE;
}

static void
setDesktopHints (CompScreen *s)
{
    CompDisplay   *d = s->display;
    unsigned long *data;
    int		  size, offset, hintSize, i;

    size = s->nDesktop * 2 + s->nDesktop * 2 + s->nDesktop * 4 + 1;

    data = malloc (sizeof (unsigned long) * size);
    if (!data)
	return;

    offset   = 0;
    hintSize = s->nDesktop * 2;

    for (i = 0; i < s->nDesktop; i++)
    {
	data[offset + i * 2 + 0] = s->x * s->width;
	data[offset + i * 2 + 1] = s->y * s->height;
    }

    if (!desktopHintEqual (s, data, size, offset, hintSize))
	XChangeProperty (d->display, s->root, d->desktopViewportAtom,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &data[offset], hintSize);

    offset += hintSize;

    for (i = 0; i < s->nDesktop; i++)
    {
	data[offset + i * 2 + 0] = s->width  * s->hsize;
	data[offset + i * 2 + 1] = s->height * s->vsize;
    }

    if (!desktopHintEqual (s, data, size, offset, hintSize))
	XChangeProperty (d->display, s->root, d->desktopGeometryAtom,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &data[offset], hintSize);

    offset += hintSize;
    hintSize = s->nDesktop * 4;

    for (i = 0; i < s->nDesktop; i++)
    {
	data[offset + i * 4 + 0] = s->workArea.x;
	data[offset + i * 4 + 1] = s->workArea.y;
	data[offset + i * 4 + 2] = s->workArea.width;
	data[offset + i * 4 + 3] = s->workArea.height;
    }

    if (!desktopHintEqual (s, data, size, offset, hintSize))
	XChangeProperty (d->display, s->root, d->workareaAtom,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &data[offset], hintSize);

    offset += hintSize;

    data[offset] = s->nDesktop;
    hintSize = 1;

    if (!desktopHintEqual (s, data, size, offset, hintSize))
	XChangeProperty (d->display, s->root, d->numberOfDesktopsAtom,
			 XA_CARDINAL, 32, PropModeReplace,
			 (unsigned char *) &data[offset], hintSize);

    if (s->desktopHintData)
	free (s->desktopHintData);

    s->desktopHintData = data;
    s->desktopHintSize = size;
}

static void
setVirtualScreenSize (CompScreen *screen,
		      int	 hsize,
		      int	 vsize)
{
    screen->hsize = hsize;
    screen->vsize = vsize;

    setDesktopHints (screen);
}

static void
updateOutputDevices (CompScreen	*s)
{
    CompOutput	  *o, *output = NULL;
    CompListValue *list = &s->opt[COMP_SCREEN_OPTION_OUTPUTS].value.list;
    int		  nOutput = 0;
    int		  x, y, i, bits;
    unsigned int  width, height;
    int		  x1, y1, x2, y2;
    Region	  region;

    for (i = 0; i < list->nValue; i++)
    {
	if (!list->value[i].s)
	    continue;

	x      = 0;
	y      = 0;
	width  = s->width;
	height = s->height;

	bits = XParseGeometry (list->value[i].s, &x, &y, &width, &height);

	if (bits & XNegative)
	    x = s->width + x - width;

	if (bits & YNegative)
	    y = s->height + y - height;

	x1 = x;
	y1 = y;
	x2 = x + width;
	y2 = y + height;

	if (x1 < 0)
	    x1 = 0;
	if (y1 < 0)
	    y1 = 0;
	if (x2 > s->width)
	    x2 = s->width;
	if (y2 > s->height)
	    y2 = s->height;

	if (x1 < x2 && y1 < y2)
	{
	    o = realloc (output, sizeof (CompOutput) * (nOutput + 1));
	    if (o)
	    {
		o[nOutput].region.extents.x1 = x1;
		o[nOutput].region.extents.y1 = y1;
		o[nOutput].region.extents.x2 = x2;
		o[nOutput].region.extents.y2 = y2;

		output = o;
		nOutput++;
	    }
	}
    }

    /* make sure we have at least one output */
    if (!nOutput)
    {
	output = malloc (sizeof (CompOutput));
	if (!output)
	    return;

	output->region.extents.x1 = 0;
	output->region.extents.y1 = 0;
	output->region.extents.x2 = s->width;
	output->region.extents.y2 = s->height;

	nOutput = 1;
    }

    /* set name, width, height and update rect pointers in all regions */
    for (i = 0; i < nOutput; i++)
    {
	output[i].name = malloc (sizeof (char) * 10);
	if (output[i].name)
	    snprintf (output[i].name, 10, "Output %d", nOutput);

	output[i].region.rects = &output[i].region.extents;
	output[i].region.numRects = 1;

	output[i].width  = output[i].region.extents.x2 -
	    output[i].region.extents.x1;
	output[i].height = output[i].region.extents.y2 -
	    output[i].region.extents.y1;

	output[i].workArea.x      = output[i].region.extents.x1;
	output[i].workArea.y      = output[i].region.extents.x1;
	output[i].workArea.width  = output[i].width;
	output[i].workArea.height = output[i].height;

	output[i].id = i;
    }

    if (s->outputDev)
    {
	for (i = 0; i < s->nOutputDev; i++)
	    if (s->outputDev[i].name)
		free (s->outputDev[i].name);

	free (s->outputDev);
    }

    s->outputDev  = output;
    s->nOutputDev = nOutput;

    setCurrentOutput (s, s->currentOutputDev);

    updateWorkareaForScreen (s);

    setDefaultViewport (s);
    damageScreen (s);

    region = XCreateRegion ();
    if (region)
    {
	REGION r;

	r.rects = &r.extents;
	r.numRects = 1;

	if (s->display->nScreenInfo)
	{
	    for (i = 0; i < s->display->nScreenInfo; i++)
	    {
		r.extents.x1 = s->display->screenInfo[i].x_org;
		r.extents.y1 = s->display->screenInfo[i].y_org;
		r.extents.x2 = r.extents.x1 + s->display->screenInfo[i].width;
		r.extents.y2 = r.extents.y1 + s->display->screenInfo[i].height;

		XUnionRegion (region, &r, region);
	    }
	}
	else
	{
	    r.extents.x1 = 0;
	    r.extents.y1 = 0;
	    r.extents.x2 = s->width;
	    r.extents.y2 = s->height;

	    XUnionRegion (region, &r, region);
	}

	/* remove all output regions from visible screen region */
	for (i = 0; i < s->nOutputDev; i++)
	    XSubtractRegion (region, &s->outputDev[i].region, region);

	/* we should clear color buffers before swapping if we have visible
	   regions without output */
	s->clearBuffers = REGION_NOT_EMPTY (region);

	XDestroyRegion (region);
    }

    (*s->outputChangeNotify) (s);
}

static void
detectOutputDevices (CompScreen *s)
{
    if (!noDetection && s->opt[COMP_SCREEN_OPTION_DETECT_OUTPUTS].value.b)
    {
	char		*name;
	CompOptionValue	value;
	char		output[1024];
	int		i, size = sizeof (output);

	if (s->display->nScreenInfo)
	{
	    int n = s->display->nScreenInfo;

	    value.list.nValue = n;
	    value.list.value  = malloc (sizeof (CompOptionValue) * n);
	    if (!value.list.value)
		return;

	    for (i = 0; i < n; i++)
	    {
		snprintf (output, size, "%dx%d+%d+%d",
			  s->display->screenInfo[i].width,
			  s->display->screenInfo[i].height,
			  s->display->screenInfo[i].x_org,
			  s->display->screenInfo[i].y_org);

		value.list.value[i].s = strdup (output);
	    }
	}
	else
	{
	    value.list.nValue = 1;
	    value.list.value  = malloc (sizeof (CompOptionValue));
	    if (!value.list.value)
		return;

	    snprintf (output, size, "%dx%d+%d+%d", s->width, s->height, 0, 0);

	    value.list.value->s = strdup (output);
	}

	name = s->opt[COMP_SCREEN_OPTION_OUTPUTS].name;

	s->opt[COMP_SCREEN_OPTION_DETECT_OUTPUTS].value.b = FALSE;
	(*s->setScreenOption) (s, name, &value);
	s->opt[COMP_SCREEN_OPTION_DETECT_OUTPUTS].value.b = TRUE;

	for (i = 0; i < value.list.nValue; i++)
	    if (value.list.value[i].s)
		free (value.list.value[i].s);

	free (value.list.value);
    }
    else
    {
	updateOutputDevices (s);
    }
}

CompOption *
compGetScreenOptions (CompScreen *screen,
		      int	 *count)
{
    *count = NUM_OPTIONS (screen);
    return screen->opt;
}

static Bool
setScreenOption (CompScreen      *screen,
		 const char	 *name,
		 CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    o = compFindOption (screen->opt, NUM_OPTIONS (screen), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case COMP_SCREEN_OPTION_DETECT_REFRESH_RATE:
	if (compSetBoolOption (o, value))
	{
	    if (value->b)
		detectRefreshRateOfScreen (screen);

	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_DETECT_OUTPUTS:
	if (compSetBoolOption (o, value))
	{
	    if (value->b)
		detectOutputDevices (screen);

	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_REFRESH_RATE:
	if (screen->opt[COMP_SCREEN_OPTION_DETECT_REFRESH_RATE].value.b)
	    return FALSE;

	if (compSetIntOption (o, value))
	{
	    screen->redrawTime = 1000 / o->value.i;
	    screen->optimalRedrawTime = screen->redrawTime;
	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_HSIZE:
	if (compSetIntOption (o, value))
	{
	    CompOption *vsize;

	    vsize = compFindOption (screen->opt, NUM_OPTIONS (screen),
				    "vsize", NULL);

	    if (!vsize)
		return FALSE;

	    if (o->value.i * screen->width > MAXSHORT)
		return FALSE;

	    setVirtualScreenSize (screen, o->value.i, vsize->value.i);
	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_VSIZE:
	if (compSetIntOption (o, value))
	{
	    CompOption *hsize;

	    hsize = compFindOption (screen->opt, NUM_OPTIONS (screen),
				    "hsize", NULL);

	    if (!hsize)
		return FALSE;

	    if (o->value.i * screen->height > MAXSHORT)
		return FALSE;

	    setVirtualScreenSize (screen, hsize->value.i, o->value.i);
	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_NUMBER_OF_DESKTOPS:
	if (compSetIntOption (o, value))
	{
	    setNumberOfDesktops (screen, o->value.i);
	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_DEFAULT_ICON:
	if (compSetStringOption (o, value))
	    return updateDefaultIcon (screen);
	break;
    case COMP_SCREEN_OPTION_OUTPUTS:
	if (!noDetection &&
	    screen->opt[COMP_SCREEN_OPTION_DETECT_OUTPUTS].value.b)
	    return FALSE;

	if (compSetOptionList (o, value))
	{
	    updateOutputDevices (screen);
	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_OPACITY_MATCHES:
	if (compSetOptionList (o, value))
	{
	    CompWindow *w;
	    int	       i;

	    for (i = 0; i < o->value.list.nValue; i++)
		matchUpdate (screen->display, &o->value.list.value[i].match);

	    for (w = screen->windows; w; w = w->next)
		updateWindowOpacity (w);

	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_OPACITY_VALUES:
	if (compSetOptionList (o, value))
	{
	    CompWindow *w;

	    for (w = screen->windows; w; w = w->next)
		updateWindowOpacity (w);

	    return TRUE;
	}
	break;
    default:
	if (compSetScreenOption (screen, o, value))
	    return TRUE;
	break;
    }

    return FALSE;
}

static Bool
setScreenOptionForPlugin (CompScreen      *screen,
			  const char	  *plugin,
			  const char	  *name,
			  CompOptionValue *value)
{
    CompPlugin *p;

    p = findActivePlugin (plugin);
    if (p && p->vTable->setScreenOption)
	return (*p->vTable->setScreenOption) (p, screen, name, value);

    return FALSE;
}

const CompMetadataOptionInfo coreScreenOptionInfo[COMP_SCREEN_OPTION_NUM] = {
    { "detect_refresh_rate", "bool", 0, 0, 0 },
    { "lighting", "bool", 0, 0, 0 },
    { "refresh_rate", "int", "<min>1</min>", 0, 0 },
    { "hsize", "int", "<min>1</min><max>32</max>", 0, 0 },
    { "vsize", "int", "<min>1</min><max>32</max>", 0, 0 },
    { "opacity_step", "int", "<min>1</min>", 0, 0 },
    { "unredirect_fullscreen_windows", "bool", 0, 0, 0 },
    { "default_icon", "string", 0, 0, 0 },
    { "sync_to_vblank", "bool", 0, 0, 0 },
    { "number_of_desktops", "int", "<min>1</min>", 0, 0 },
    { "detect_outputs", "bool", 0, 0, 0 },
    { "outputs", "list", "<type>string</type>", 0, 0 },
    { "focus_prevention_match", "match", 0, 0, 0 },
    { "opacity_matches", "list", "<type>match</type>", 0, 0 },
    { "opacity_values", "list", "<type>int</type>", 0, 0 }
};

static void
updateStartupFeedback (CompScreen *s)
{
    if (s->startupSequences)
	XDefineCursor (s->display->display, s->root, s->busyCursor);
    else
	XDefineCursor (s->display->display, s->root, s->normalCursor);
}

#define STARTUP_TIMEOUT_DELAY 15000

static Bool
startupSequenceTimeout (void *data)
{
    CompScreen		*screen = data;
    CompStartupSequence *s;
    struct timeval	now, active;
    double		elapsed;

    gettimeofday (&now, NULL);

    for (s = screen->startupSequences; s; s = s->next)
    {
	sn_startup_sequence_get_last_active_time (s->sequence,
						  &active.tv_sec,
						  &active.tv_usec);

	elapsed = ((((double) now.tv_sec - active.tv_sec) * 1000000.0 +
		    (now.tv_usec - active.tv_usec))) / 1000.0;

	if (elapsed > STARTUP_TIMEOUT_DELAY)
	    sn_startup_sequence_complete (s->sequence);
    }

    return TRUE;
}

static void
addSequence (CompScreen        *screen,
	     SnStartupSequence *sequence)
{
    CompStartupSequence *s;

    s = malloc (sizeof (CompStartupSequence));
    if (!s)
	return;

    sn_startup_sequence_ref (sequence);

    s->next     = screen->startupSequences;
    s->sequence = sequence;
    s->viewportX = screen->x;
    s->viewportY = screen->y;

    screen->startupSequences = s;

    if (!screen->startupSequenceTimeoutHandle)
	compAddTimeout (1000,
			startupSequenceTimeout,
			screen);

    updateStartupFeedback (screen);
}

static void
removeSequence (CompScreen        *screen,
		SnStartupSequence *sequence)
{
    CompStartupSequence *s, *p = NULL;

    for (s = screen->startupSequences; s; s = s->next)
    {
	if (s->sequence == sequence)
	    break;

	p = s;
    }

    if (!s)
	return;

    sn_startup_sequence_unref (sequence);

    if (p)
	p->next = s->next;
    else
	screen->startupSequences = NULL;

    free (s);

    if (!screen->startupSequences && screen->startupSequenceTimeoutHandle)
    {
	compRemoveTimeout (screen->startupSequenceTimeoutHandle);
	screen->startupSequenceTimeoutHandle = 0;
    }

    updateStartupFeedback (screen);
}

static void
compScreenSnEvent (SnMonitorEvent *event,
		   void           *userData)
{
    CompScreen	      *screen = userData;
    SnStartupSequence *sequence;

    sequence = sn_monitor_event_get_startup_sequence (event);

    switch (sn_monitor_event_get_type (event)) {
    case SN_MONITOR_EVENT_INITIATED:
	addSequence (screen, sequence);
	break;
    case SN_MONITOR_EVENT_COMPLETED:
	removeSequence (screen, sn_monitor_event_get_startup_sequence (event));
	break;
    case SN_MONITOR_EVENT_CHANGED:
    case SN_MONITOR_EVENT_CANCELED:
	break;
    }
}

static void
updateScreenEdges (CompScreen *s)
{
    struct screenEdgeGeometry {
	int xw, x0;
	int yh, y0;
	int ww, w0;
	int hh, h0;
    } geometry[SCREEN_EDGE_NUM] = {
	{ 0,  0,   0,  2,   0,  2,   1, -4 }, /* left */
	{ 1, -2,   0,  2,   0,  2,   1, -4 }, /* right */
	{ 0,  2,   0,  0,   1, -4,   0,  2 }, /* top */
	{ 0,  2,   1, -2,   1, -4,   0,  2 }, /* bottom */
	{ 0,  0,   0,  0,   0,  2,   0,  2 }, /* top-left */
	{ 1, -2,   0,  0,   0,  2,   0,  2 }, /* top-right */
	{ 0,  0,   1, -2,   0,  2,   0,  2 }, /* bottom-left */
	{ 1, -2,   1, -2,   0,  2,   0,  2 }  /* bottom-right */
    };
    int i;

    for (i = 0; i < SCREEN_EDGE_NUM; i++)
    {
	if (s->screenEdge[i].id)
	    XMoveResizeWindow (s->display->display, s->screenEdge[i].id,
			       geometry[i].xw * s->width  + geometry[i].x0,
			       geometry[i].yh * s->height + geometry[i].y0,
			       geometry[i].ww * s->width  + geometry[i].w0,
			       geometry[i].hh * s->height + geometry[i].h0);
    }
}

static void
frustum (GLfloat *m,
	 GLfloat left,
	 GLfloat right,
	 GLfloat bottom,
	 GLfloat top,
	 GLfloat nearval,
	 GLfloat farval)
{
    GLfloat x, y, a, b, c, d;

    x = (2.0 * nearval) / (right - left);
    y = (2.0 * nearval) / (top - bottom);
    a = (right + left) / (right - left);
    b = (top + bottom) / (top - bottom);
    c = -(farval + nearval) / ( farval - nearval);
    d = -(2.0 * farval * nearval) / (farval - nearval);

#define M(row,col)  m[col*4+row]
    M(0,0) = x;     M(0,1) = 0.0f;  M(0,2) = a;      M(0,3) = 0.0f;
    M(1,0) = 0.0f;  M(1,1) = y;     M(1,2) = b;      M(1,3) = 0.0f;
    M(2,0) = 0.0f;  M(2,1) = 0.0f;  M(2,2) = c;      M(2,3) = d;
    M(3,0) = 0.0f;  M(3,1) = 0.0f;  M(3,2) = -1.0f;  M(3,3) = 0.0f;
#undef M

}

static void
perspective (GLfloat *m,
	     GLfloat fovy,
	     GLfloat aspect,
	     GLfloat zNear,
	     GLfloat zFar)
{
    GLfloat xmin, xmax, ymin, ymax;

    ymax = zNear * tan (fovy * M_PI / 360.0);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    frustum (m, xmin, xmax, ymin, ymax, zNear, zFar);
}

void
setCurrentOutput (CompScreen *s,
		  int	     outputNum)
{
    if (outputNum >= s->nOutputDev)
	outputNum = 0;

    s->currentOutputDev = outputNum;
}

static void
reshape (CompScreen *s,
	 int	    w,
	 int	    h)
{

#ifdef USE_COW
    if (useCow)
	XMoveResizeWindow (s->display->display, s->overlay, 0, 0, w, h);
#endif

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
    glDepthRange (0, 1);
    glViewport (-1, -1, 2, 2);
    glRasterPos2f (0, 0);

    s->rasterX = s->rasterY = 0;

    perspective (s->projection, 60.0f, 1.0f, 0.1f, 100.0f);

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glMultMatrixf (s->projection);
    glMatrixMode (GL_MODELVIEW);

    s->region.rects = &s->region.extents;
    s->region.numRects = 1;
    s->region.extents.x1 = 0;
    s->region.extents.y1 = 0;
    s->region.extents.x2 = w;
    s->region.extents.y2 = h;
    s->region.size = 1;

    s->width  = w;
    s->height = h;

    s->fullscreenOutput.name             = "fullscreen";
    s->fullscreenOutput.id               = ~0;
    s->fullscreenOutput.width            = w;
    s->fullscreenOutput.height           = h;
    s->fullscreenOutput.region           = s->region;
    s->fullscreenOutput.workArea.x       = 0;
    s->fullscreenOutput.workArea.y       = 0;
    s->fullscreenOutput.workArea.width   = w;
    s->fullscreenOutput.workArea.height  = h;
 
    updateScreenEdges (s);
}

void
configureScreen (CompScreen	 *s,
		 XConfigureEvent *ce)
{
    if (s->attrib.width  != ce->width ||
	s->attrib.height != ce->height)
    {
	s->attrib.width  = ce->width;
	s->attrib.height = ce->height;

	reshape (s, ce->width, ce->height);

	detectOutputDevices (s);

	damageScreen (s);
    }
}

static FuncPtr
getProcAddress (CompScreen *s,
		const char *name)
{
    static void *dlhand = NULL;
    FuncPtr     funcPtr = NULL;

    if (s->getProcAddress)
	funcPtr = s->getProcAddress ((GLubyte *) name);

    if (!funcPtr)
    {
	if (!dlhand)
	    dlhand = dlopen (NULL, RTLD_LAZY);

	if (dlhand)
	{
	    dlerror ();
	    funcPtr = (FuncPtr) dlsym (dlhand, name);
	    if (dlerror () != NULL)
		funcPtr = NULL;
	}
    }

    return funcPtr;
}

void
updateScreenBackground (CompScreen  *screen,
			CompTexture *texture)
{
    Display	  *dpy = screen->display->display;
    Atom	  pixmapAtom, actualType;
    int		  actualFormat, i, status;
    unsigned int  width = 1, height = 1, depth = 0;
    unsigned long nItems;
    unsigned long bytesAfter;
    unsigned char *prop;
    Pixmap	  pixmap = 0;

    pixmapAtom = XInternAtom (dpy, "PIXMAP", FALSE);

    for (i = 0; pixmap == 0 && i < 2; i++)
    {
	status = XGetWindowProperty (dpy, screen->root,
				     screen->display->xBackgroundAtom[i],
				     0, 4, FALSE, AnyPropertyType,
				     &actualType, &actualFormat, &nItems,
				     &bytesAfter, &prop);

	if (status == Success && nItems && prop)
	{
	    if (actualType   == pixmapAtom &&
		actualFormat == 32         &&
		nItems	     == 1)
	    {
		Pixmap p;

		memcpy (&p, prop, 4);

		if (p)
		{
		    unsigned int ui;
		    int		 i;
		    Window	 w;

		    if (XGetGeometry (dpy, p, &w, &i, &i,
				      &width, &height, &ui, &depth))
		    {
			if (depth == screen->attrib.depth)
			    pixmap = p;
		    }
		}
	    }

	    XFree (prop);
	}
    }

    if (pixmap)
    {
	if (pixmap == texture->pixmap)
	    return;

	finiTexture (screen, texture);
	initTexture (screen, texture);

	if (!bindPixmapToTexture (screen, texture, pixmap,
				  width, height, depth))
	{
	    compLogMessage (NULL, "core", CompLogLevelWarn,
			    "Couldn't bind background pixmap 0x%x to "
			    "texture", (int) pixmap);
	}
    }
    else
    {
	finiTexture (screen, texture);
	initTexture (screen, texture);
    }

    if (!texture->name && backgroundImage)
	readImageToTexture (screen, texture, backgroundImage, &width, &height);

    if (texture->target == GL_TEXTURE_2D)
    {
	glBindTexture (texture->target, texture->name);
	glTexParameteri (texture->target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri (texture->target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture (texture->target, 0);
    }
}

void
detectRefreshRateOfScreen (CompScreen *s)
{
    if (!noDetection && s->opt[COMP_SCREEN_OPTION_DETECT_REFRESH_RATE].value.b)
    {
	char		*name;
	CompOptionValue	value;

	value.i = 0;

	if (s->display->randrExtension)
	{
	    XRRScreenConfiguration *config;

	    config  = XRRGetScreenInfo (s->display->display, s->root);
	    value.i = (int) XRRConfigCurrentRate (config);

	    XRRFreeScreenConfigInfo (config);
	}

	if (value.i == 0)
	    value.i = defaultRefreshRate;

	name = s->opt[COMP_SCREEN_OPTION_REFRESH_RATE].name;

	s->opt[COMP_SCREEN_OPTION_DETECT_REFRESH_RATE].value.b = FALSE;
	(*s->setScreenOption) (s, name, &value);
	s->opt[COMP_SCREEN_OPTION_DETECT_REFRESH_RATE].value.b = TRUE;
    }
    else
    {
	s->redrawTime = 1000 / s->opt[COMP_SCREEN_OPTION_REFRESH_RATE].value.i;
	s->optimalRedrawTime = s->redrawTime;
    }
}

static void
setSupportingWmCheck (CompScreen *s)
{
    CompDisplay *d = s->display;

    XChangeProperty (d->display, s->grabWindow, d->supportingWmCheckAtom,
		     XA_WINDOW, 32, PropModeReplace,
		     (unsigned char *) &s->grabWindow, 1);

    XChangeProperty (d->display, s->grabWindow, d->wmNameAtom,
		     d->utf8StringAtom, 8, PropModeReplace,
		     (unsigned char *) PACKAGE, strlen (PACKAGE));
    XChangeProperty (d->display, s->grabWindow, d->winStateAtom,
		     XA_ATOM, 32, PropModeReplace,
		     (unsigned char *) &d->winStateSkipTaskbarAtom, 1);
    XChangeProperty (d->display, s->grabWindow, d->winStateAtom,
		     XA_ATOM, 32, PropModeAppend,
		     (unsigned char *) &d->winStateSkipPagerAtom, 1);
    XChangeProperty (d->display, s->grabWindow, d->winStateAtom,
		     XA_ATOM, 32, PropModeAppend,
		     (unsigned char *) &d->winStateHiddenAtom, 1);

    XChangeProperty (d->display, s->root, d->supportingWmCheckAtom,
		     XA_WINDOW, 32, PropModeReplace,
		     (unsigned char *) &s->grabWindow, 1);
}

static void
setSupported (CompScreen *s)
{
    CompDisplay *d = s->display;
    Atom	data[256];
    int		i = 0;

    data[i++] = d->supportedAtom;
    data[i++] = d->supportingWmCheckAtom;

    data[i++] = d->utf8StringAtom;

    data[i++] = d->clientListAtom;
    data[i++] = d->clientListStackingAtom;

    data[i++] = d->winActiveAtom;

    data[i++] = d->desktopViewportAtom;
    data[i++] = d->desktopGeometryAtom;
    data[i++] = d->currentDesktopAtom;
    data[i++] = d->numberOfDesktopsAtom;
    data[i++] = d->showingDesktopAtom;

    data[i++] = d->workareaAtom;

    data[i++] = d->wmNameAtom;
/*
    data[i++] = d->wmVisibleNameAtom;
*/

    data[i++] = d->wmStrutAtom;
    data[i++] = d->wmStrutPartialAtom;

/*
    data[i++] = d->wmPidAtom;
*/

    data[i++] = d->wmUserTimeAtom;
    data[i++] = d->frameExtentsAtom;
    data[i++] = d->frameWindowAtom;

    data[i++] = d->winStateModalAtom;
    data[i++] = d->winStateStickyAtom;
    data[i++] = d->winStateMaximizedVertAtom;
    data[i++] = d->winStateMaximizedHorzAtom;
    data[i++] = d->winStateShadedAtom;
    data[i++] = d->winStateSkipTaskbarAtom;
    data[i++] = d->winStateSkipPagerAtom;
    data[i++] = d->winStateHiddenAtom;
    data[i++] = d->winStateFullscreenAtom;
    data[i++] = d->winStateAboveAtom;
    data[i++] = d->winStateBelowAtom;
    data[i++] = d->winStateDemandsAttentionAtom;

    data[i++] = d->winOpacityAtom;
    data[i++] = d->winBrightnessAtom;

    if (s->canDoSaturated)
    {
	data[i++] = d->winSaturationAtom;
	data[i++] = d->winStateDisplayModalAtom;
    }

    data[i++] = d->wmAllowedActionsAtom;

    data[i++] = d->winActionMoveAtom;
    data[i++] = d->winActionResizeAtom;
    data[i++] = d->winActionStickAtom;
    data[i++] = d->winActionMinimizeAtom;
    data[i++] = d->winActionMaximizeHorzAtom;
    data[i++] = d->winActionMaximizeVertAtom;
    data[i++] = d->winActionFullscreenAtom;
    data[i++] = d->winActionCloseAtom;
    data[i++] = d->winActionShadeAtom;
    data[i++] = d->winActionChangeDesktopAtom;
    data[i++] = d->winActionAboveAtom;
    data[i++] = d->winActionBelowAtom;

    data[i++] = d->winTypeAtom;
    data[i++] = d->winTypeDesktopAtom;
    data[i++] = d->winTypeDockAtom;
    data[i++] = d->winTypeToolbarAtom;
    data[i++] = d->winTypeMenuAtom;
    data[i++] = d->winTypeSplashAtom;
    data[i++] = d->winTypeDialogAtom;
    data[i++] = d->winTypeUtilAtom;
    data[i++] = d->winTypeNormalAtom;

    data[i++] = d->wmDeleteWindowAtom;
    data[i++] = d->wmPingAtom;

    data[i++] = d->wmMoveResizeAtom;
    data[i++] = d->moveResizeWindowAtom;
    data[i++] = d->restackWindowAtom;

    XChangeProperty (d->display, s->root, d->supportedAtom, XA_ATOM, 32,
		     PropModeReplace, (unsigned char *) data, i);
}

static void
getDesktopHints (CompScreen *s)
{
    CompDisplay   *d = s->display;
    unsigned long data[2];
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *propData;

    result = XGetWindowProperty (s->display->display, s->root,
				 d->numberOfDesktopsAtom, 0L, 1L, FALSE,
				 XA_CARDINAL, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && n && propData && useDesktopHints)
    {
	memcpy (data, propData, sizeof (unsigned long));
	XFree (propData);

	if (data[0] > 0 && data[0] < 0xffffffff)
	    s->nDesktop = data[0];
    }

    result = XGetWindowProperty (s->display->display, s->root,
				 d->desktopViewportAtom, 0L, 2L,
				 FALSE, XA_CARDINAL, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && n && propData)
    {
	if (n == 2)
	{
	    memcpy (data, propData, sizeof (unsigned long) * 2);

	    if (data[0] / s->width < s->hsize - 1)
		s->x = data[0] / s->width;

	    if (data[1] / s->height < s->vsize - 1)
		s->y = data[1] / s->height;
	}

	XFree (propData);
    }

    result = XGetWindowProperty (s->display->display, s->root,
				 d->currentDesktopAtom, 0L, 1L, FALSE,
				 XA_CARDINAL, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && n && propData && useDesktopHints)
    {
	memcpy (data, propData, sizeof (unsigned long));
	XFree (propData);

	if (data[0] < s->nDesktop)
	    s->currentDesktop = data[0];
    }

    result = XGetWindowProperty (s->display->display, s->root,
				 d->showingDesktopAtom, 0L, 1L, FALSE,
				 XA_CARDINAL, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && n && propData)
    {
	memcpy (data, propData, sizeof (unsigned long));
	XFree (propData);

	if (data[0])
	    (*s->enterShowDesktopMode) (s);
    }

    data[0] = s->currentDesktop;

    XChangeProperty (d->display, s->root, d->currentDesktopAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 1);

    data[0] = s->showingDesktopMask ? TRUE : FALSE;

    XChangeProperty (d->display, s->root, d->showingDesktopAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 1);
}

void
showOutputWindow (CompScreen *s)
{

#ifdef USE_COW
    if (useCow)
    {
	Display       *dpy = s->display->display;
	XserverRegion region;

	region = XFixesCreateRegion (dpy, NULL, 0);

	XFixesSetWindowShapeRegion (dpy,
				    s->output,
				    ShapeBounding,
				    0, 0, 0);
	XFixesSetWindowShapeRegion (dpy,
				    s->output,
				    ShapeInput,
				    0, 0, region);

	XFixesDestroyRegion (dpy, region);

	damageScreen (s);
    }
#endif

}

void
hideOutputWindow (CompScreen *s)
{

#ifdef USE_COW
    if (useCow)
    {
	Display       *dpy = s->display->display;
	XserverRegion region;

	region = XFixesCreateRegion (dpy, NULL, 0);

	XFixesSetWindowShapeRegion (dpy,
				    s->output,
				    ShapeBounding,
				    0, 0, region);

	XFixesDestroyRegion (dpy, region);
    }
#endif

}

static void
makeOutputWindow (CompScreen *s)
{

#ifdef USE_COW
    if (useCow)
    {
	s->overlay = XCompositeGetOverlayWindow (s->display->display, s->root);
	s->output  = s->overlay;

	XSelectInput (s->display->display, s->output, ExposureMask);
    }
    else
#endif

	s->output = s->overlay = s->root;

    showOutputWindow (s);
}

static void
enterShowDesktopMode (CompScreen *s)
{
    CompDisplay   *d = s->display;
    CompWindow    *w;
    unsigned long data = 1;
    int		  count = 0;
    CompOption    *st = &d->opt[COMP_DISPLAY_OPTION_HIDE_SKIP_TASKBAR_WINDOWS];

    s->showingDesktopMask = ~(CompWindowTypeDesktopMask |
			      CompWindowTypeDockMask);

    for (w = s->windows; w; w = w->next)
    {
	if ((s->showingDesktopMask & w->type) &&
	    (!(w->state & CompWindowStateSkipTaskbarMask) || st->value.b))
	{
	    if (!w->inShowDesktopMode && (*s->focusWindow) (w))
	    {
		w->inShowDesktopMode = TRUE;
		hideWindow (w);
	    }
	}

	if (w->inShowDesktopMode)
	    count++;
    }

    if (!count)
    {
	s->showingDesktopMask = 0;
	data = 0;
    }

    XChangeProperty (s->display->display, s->root,
		     s->display->showingDesktopAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &data, 1);
}

static void
leaveShowDesktopMode (CompScreen *s,
		      CompWindow *window)
{
    CompWindow    *w;
    unsigned long data = 0;

    if (window)
    {
	if (!window->inShowDesktopMode)
	    return;

	window->inShowDesktopMode = FALSE;
	showWindow (window);

	/* return if some other window is still in show desktop mode */
	for (w = s->windows; w; w = w->next)
	    if (w->inShowDesktopMode)
		return;

	s->showingDesktopMask = 0;
    }
    else
    {
	s->showingDesktopMask = 0;

	for (w = s->windows; w; w = w->next)
	{
	    if (!w->inShowDesktopMode)
		continue;

	    w->inShowDesktopMode = FALSE;
	    showWindow (w);
	}

	/* focus default window - most likely this will be the window
	   which had focus before entering showdesktop mode */
	focusDefaultWindow (s->display);
    }

    XChangeProperty (s->display->display, s->root,
		     s->display->showingDesktopAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &data, 1);
}

static CompWindow *
walkFirst (CompScreen *s)
{
    return s->windows;
}

static CompWindow *
walkLast (CompScreen *s)
{
    return s->reverseWindows;
}

static CompWindow *
walkNext (CompWindow *w)
{
    return w->next;
}

static CompWindow *
walkPrev (CompWindow *w)
{
    return w->prev;
}

static void
initWindowWalker (CompScreen *screen,
		  CompWalker *walker)
{
    walker->fini  = NULL;
    walker->first = walkFirst;
    walker->last  = walkLast;
    walker->next  = walkNext;
    walker->prev  = walkPrev;
}

Bool
addScreen (CompDisplay *display,
	   int	       screenNum,
	   Window      wmSnSelectionWindow,
	   Atom	       wmSnAtom,
	   Time	       wmSnTimestamp)
{
    CompScreen		 *s;
    Display		 *dpy = display->display;
    static char		 data = 0;
    XColor		 black;
    Pixmap		 bitmap;
    XVisualInfo		 templ;
    XVisualInfo		 *visinfo;
    GLXFBConfig		 *fbConfigs;
    Window		 rootReturn, parentReturn;
    Window		 *children;
    unsigned int	 nchildren;
    int			 defaultDepth, nvisinfo, nElements, value, i;
    const char		 *glxExtensions, *glExtensions;
    XSetWindowAttributes attrib;
    GLfloat		 globalAmbient[]  = { 0.1f, 0.1f,  0.1f, 0.1f };
    GLfloat		 ambientLight[]   = { 0.0f, 0.0f,  0.0f, 0.0f };
    GLfloat		 diffuseLight[]   = { 0.9f, 0.9f,  0.9f, 0.9f };
    GLfloat		 light0Position[] = { -0.5f, 0.5f, -9.0f, 1.0f };
    CompWindow		 *w;

    s = malloc (sizeof (CompScreen));
    if (!s)
	return FALSE;

    s->windowPrivateIndices = 0;
    s->windowPrivateLen     = 0;

    if (display->screenPrivateLen)
    {
	s->privates = malloc (display->screenPrivateLen *
			      sizeof (CompPrivate));
	if (!s->privates)
	{
	    free (s);
	    return FALSE;
	}
    }
    else
	s->privates = 0;

    s->display = display;

    if (!compInitScreenOptionsFromMetadata (s,
					    &coreMetadata,
					    coreScreenOptionInfo,
					    s->opt,
					    COMP_SCREEN_OPTION_NUM))
	return FALSE;

    s->damage = XCreateRegion ();
    if (!s->damage)
	return FALSE;

    s->x     = 0;
    s->y     = 0;
    s->hsize = s->opt[COMP_SCREEN_OPTION_HSIZE].value.i;
    s->vsize = s->opt[COMP_SCREEN_OPTION_VSIZE].value.i;

    s->nDesktop	      = 1;
    s->currentDesktop = 0;

    for (i = 0; i < SCREEN_EDGE_NUM; i++)
    {
	s->screenEdge[i].id    = None;
	s->screenEdge[i].count = 0;
    }

    s->buttonGrab  = 0;
    s->nButtonGrab = 0;
    s->keyGrab     = 0;
    s->nKeyGrab    = 0;

    s->grabs    = 0;
    s->grabSize = 0;
    s->maxGrab  = 0;

    s->pendingDestroys = 0;

    s->clientList  = 0;
    s->nClientList = 0;

    s->screenNum = screenNum;
    s->colormap  = DefaultColormap (dpy, screenNum);
    s->root	 = XRootWindow (dpy, screenNum);

    s->mapNum    = 1;
    s->activeNum = 1;

    s->groups = NULL;

    s->snContext = sn_monitor_context_new (display->snDisplay,
					   screenNum,
					   compScreenSnEvent, s,
					   NULL);

    s->startupSequences		    = NULL;
    s->startupSequenceTimeoutHandle = 0;

    s->wmSnSelectionWindow = wmSnSelectionWindow;
    s->wmSnAtom		   = wmSnAtom;
    s->wmSnTimestamp	   = wmSnTimestamp;

    s->damageMask  = COMP_SCREEN_DAMAGE_ALL_MASK;
    s->next	   = 0;
    s->exposeRects = 0;
    s->sizeExpose  = 0;
    s->nExpose     = 0;

    s->rasterX = 0;
    s->rasterY = 0;

    s->outputDev	= NULL;
    s->nOutputDev	= 0;
    s->currentOutputDev = 0;

    s->windows = 0;
    s->reverseWindows = 0;

    s->nextRedraw  = 0;
    s->frameStatus = 0;
    s->timeMult    = 1;
    s->idle	   = TRUE;
    s->timeLeft    = 0;

    s->pendingCommands = TRUE;

    s->lastFunctionId = 0;

    s->fragmentFunctions = NULL;
    s->fragmentPrograms = NULL;

    memset (s->saturateFunction, 0, sizeof (s->saturateFunction));

    s->showingDesktopMask = 0;

    memset (s->history, 0, sizeof (s->history));
    s->currentHistory = 0;

    s->overlayWindowCount = 0;

    s->desktopHintData = NULL;
    s->desktopHintSize = 0;

    s->cursors = NULL;

    s->clearBuffers = TRUE;

    gettimeofday (&s->lastRedraw, 0);

    s->setScreenOption	        = setScreenOption;
    s->setScreenOptionForPlugin = setScreenOptionForPlugin;

    s->initPluginForScreen = initPluginForScreen;
    s->finiPluginForScreen = finiPluginForScreen;

    s->preparePaintScreen	  = preparePaintScreen;
    s->donePaintScreen		  = donePaintScreen;
    s->paintScreen		  = paintScreen;
    s->paintOutput		  = paintOutput;
    s->paintTransformedOutput	  = paintTransformedOutput;
    s->applyScreenTransform	  = applyScreenTransform;
    s->paintBackground		  = paintBackground;
    s->paintWindow		  = paintWindow;
    s->drawWindow		  = drawWindow;
    s->addWindowGeometry	  = addWindowGeometry;
    s->drawWindowTexture	  = drawWindowTexture;
    s->damageWindowRect		  = damageWindowRect;
    s->getOutputExtentsForWindow  = getOutputExtentsForWindow;
    s->getAllowedActionsForWindow = getAllowedActionsForWindow;
    s->focusWindow		  = focusWindow;
    s->placeWindow                = placeWindow;

    s->paintCursor      = paintCursor;
    s->damageCursorRect	= damageCursorRect;

    s->windowAddNotify    = windowAddNotify;
    s->windowResizeNotify = windowResizeNotify;
    s->windowMoveNotify	  = windowMoveNotify;
    s->windowGrabNotify   = windowGrabNotify;
    s->windowUngrabNotify = windowUngrabNotify;

    s->enterShowDesktopMode = enterShowDesktopMode;
    s->leaveShowDesktopMode = leaveShowDesktopMode;

    s->windowStateChangeNotify = windowStateChangeNotify;

    s->outputChangeNotify = outputChangeNotify;

    s->initWindowWalker = initWindowWalker;

    s->getProcAddress = 0;

    if (!XGetWindowAttributes (dpy, s->root, &s->attrib))
	return FALSE;

    s->workArea.x      = 0;
    s->workArea.y      = 0;
    s->workArea.width  = s->attrib.width;
    s->workArea.height = s->attrib.height;

    s->grabWindow = None;

    makeOutputWindow (s);

    templ.visualid = XVisualIDFromVisual (s->attrib.visual);

    visinfo = XGetVisualInfo (dpy, VisualIDMask, &templ, &nvisinfo);
    if (!nvisinfo)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"Couldn't get visual info for default visual");
	return FALSE;
    }

    defaultDepth = visinfo->depth;

    black.red = black.green = black.blue = 0;

    if (!XAllocColor (dpy, s->colormap, &black))
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"Couldn't allocate color");
	XFree(visinfo);
	return FALSE;
    }

    bitmap = XCreateBitmapFromData (dpy, s->root, &data, 1, 1);
    if (!bitmap)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"Couldn't create bitmap");
	XFree(visinfo);
	return FALSE;
    }

    s->invisibleCursor = XCreatePixmapCursor (dpy, bitmap, bitmap,
					      &black, &black, 0, 0);
    if (!s->invisibleCursor)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"Couldn't create invisible cursor");
	XFree(visinfo);
	return FALSE;
    }

    XFreePixmap (dpy, bitmap);
    XFreeColors (dpy, s->colormap, &black.pixel, 1, 0);

    glXGetConfig (dpy, visinfo, GLX_USE_GL, &value);
    if (!value)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"Root visual is not a GL visual");
	XFree(visinfo);
	return FALSE;
    }

    glXGetConfig (dpy, visinfo, GLX_DOUBLEBUFFER, &value);
    if (!value)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"Root visual is not a double buffered GL visual");
	XFree(visinfo);
	return FALSE;
    }

    s->ctx = glXCreateContext (dpy, visinfo, NULL, !indirectRendering);
    if (!s->ctx)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"glXCreateContext failed");
	XFree (visinfo);

	return FALSE;
    }

    glxExtensions = glXQueryExtensionsString (dpy, screenNum);
    if (!strstr (glxExtensions, "GLX_EXT_texture_from_pixmap"))
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"GLX_EXT_texture_from_pixmap is missing");
	XFree (visinfo);

	return FALSE;
    }

    XFree (visinfo);

    if (!strstr (glxExtensions, "GLX_SGIX_fbconfig"))
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"GLX_SGIX_fbconfig is missing");
	return FALSE;
    }

    s->getProcAddress = (GLXGetProcAddressProc)
	getProcAddress (s, "glXGetProcAddressARB");
    s->bindTexImage = (GLXBindTexImageProc)
	getProcAddress (s, "glXBindTexImageEXT");
    s->releaseTexImage = (GLXReleaseTexImageProc)
	getProcAddress (s, "glXReleaseTexImageEXT");
    s->queryDrawable = (GLXQueryDrawableProc)
	getProcAddress (s, "glXQueryDrawable");
    s->getFBConfigs = (GLXGetFBConfigsProc)
	getProcAddress (s, "glXGetFBConfigs");
    s->getFBConfigAttrib = (GLXGetFBConfigAttribProc)
	getProcAddress (s, "glXGetFBConfigAttrib");
    s->createPixmap = (GLXCreatePixmapProc)
	getProcAddress (s, "glXCreatePixmap");

    if (!s->bindTexImage)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"glXBindTexImageEXT is missing");
	return FALSE;
    }

    if (!s->releaseTexImage)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"glXReleaseTexImageEXT is missing");
	return FALSE;
    }

    if (!s->queryDrawable     ||
	!s->getFBConfigs      ||
	!s->getFBConfigAttrib ||
	!s->createPixmap)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"fbconfig functions missing");
	return FALSE;
    }

    s->copySubBuffer = NULL;
    if (strstr (glxExtensions, "GLX_MESA_copy_sub_buffer"))
	s->copySubBuffer = (GLXCopySubBufferProc)
	    getProcAddress (s, "glXCopySubBufferMESA");

    s->getVideoSync = NULL;
    s->waitVideoSync = NULL;
    if (strstr (glxExtensions, "GLX_SGI_video_sync"))
    {
	s->getVideoSync = (GLXGetVideoSyncProc)
	    getProcAddress (s, "glXGetVideoSyncSGI");

	s->waitVideoSync = (GLXWaitVideoSyncProc)
	    getProcAddress (s, "glXWaitVideoSyncSGI");
    }

    glXMakeCurrent (dpy, s->output, s->ctx);
    currentRoot = s->root;

    glExtensions = (const char *) glGetString (GL_EXTENSIONS);

    s->textureNonPowerOfTwo = 0;
    if (strstr (glExtensions, "GL_ARB_texture_non_power_of_two"))
	s->textureNonPowerOfTwo = 1;

    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &s->maxTextureSize);

    s->textureRectangle = 0;
    if (strstr (glExtensions, "GL_NV_texture_rectangle")  ||
	strstr (glExtensions, "GL_EXT_texture_rectangle") ||
	strstr (glExtensions, "GL_ARB_texture_rectangle"))
    {
	s->textureRectangle = 1;

	if (!s->textureNonPowerOfTwo)
	{
	    GLint maxTextureSize;

	    glGetIntegerv (GL_MAX_RECTANGLE_TEXTURE_SIZE_NV, &maxTextureSize);
	    if (maxTextureSize > s->maxTextureSize)
		s->maxTextureSize = maxTextureSize;
	}
    }

    if (!(s->textureRectangle || s->textureNonPowerOfTwo))
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"Support for non power of two textures missing");
	return FALSE;
    }

    s->textureEnvCombine = s->textureEnvCrossbar = 0;
    if (strstr (glExtensions, "GL_ARB_texture_env_combine"))
    {
	s->textureEnvCombine = 1;

	/* XXX: GL_NV_texture_env_combine4 need special code but it seams to
	   be working anyway for now... */
	if (strstr (glExtensions, "GL_ARB_texture_env_crossbar") ||
	    strstr (glExtensions, "GL_NV_texture_env_combine4"))
	    s->textureEnvCrossbar = 1;
    }

    s->textureBorderClamp = 0;
    if (strstr (glExtensions, "GL_ARB_texture_border_clamp") ||
	strstr (glExtensions, "GL_SGIS_texture_border_clamp"))
	s->textureBorderClamp = 1;

    s->maxTextureUnits = 1;
    if (strstr (glExtensions, "GL_ARB_multitexture"))
    {
	s->activeTexture = (GLActiveTextureProc)
	    getProcAddress (s, "glActiveTexture");
	s->clientActiveTexture = (GLClientActiveTextureProc)
	    getProcAddress (s, "glClientActiveTexture");

	if (s->activeTexture && s->clientActiveTexture)
	    glGetIntegerv (GL_MAX_TEXTURE_UNITS_ARB, &s->maxTextureUnits);
    }

    s->fragmentProgram = 0;
    if (strstr (glExtensions, "GL_ARB_fragment_program"))
    {
	s->genPrograms = (GLGenProgramsProc)
	    getProcAddress (s, "glGenProgramsARB");
	s->deletePrograms = (GLDeleteProgramsProc)
	    getProcAddress (s, "glDeleteProgramsARB");
	s->bindProgram = (GLBindProgramProc)
	    getProcAddress (s, "glBindProgramARB");
	s->programString = (GLProgramStringProc)
	    getProcAddress (s, "glProgramStringARB");
	s->programEnvParameter4f = (GLProgramParameter4fProc)
	    getProcAddress (s, "glProgramEnvParameter4fARB");
	s->programLocalParameter4f = (GLProgramParameter4fProc)
	    getProcAddress (s, "glProgramLocalParameter4fARB");

	if (s->genPrograms	     &&
	    s->deletePrograms	     &&
	    s->bindProgram	     &&
	    s->programString	     &&
	    s->programEnvParameter4f &&
	    s->programLocalParameter4f)
	    s->fragmentProgram = 1;
    }

    s->fbo = 0;
    if (strstr (glExtensions, "GL_EXT_framebuffer_object"))
    {
	s->genFramebuffers = (GLGenFramebuffersProc)
	    getProcAddress (s, "glGenFramebuffersEXT");
	s->deleteFramebuffers = (GLDeleteFramebuffersProc)
	    getProcAddress (s, "glDeleteFramebuffersEXT");
	s->bindFramebuffer = (GLBindFramebufferProc)
	    getProcAddress (s, "glBindFramebufferEXT");
	s->checkFramebufferStatus = (GLCheckFramebufferStatusProc)
	    getProcAddress (s, "glCheckFramebufferStatusEXT");
	s->framebufferTexture2D = (GLFramebufferTexture2DProc)
	    getProcAddress (s, "glFramebufferTexture2DEXT");
	s->generateMipmap = (GLGenerateMipmapProc)
	    getProcAddress (s, "glGenerateMipmapEXT");

	if (s->genFramebuffers	      &&
	    s->deleteFramebuffers     &&
	    s->bindFramebuffer	      &&
	    s->checkFramebufferStatus &&
	    s->framebufferTexture2D   &&
	    s->generateMipmap)
	    s->fbo = 1;
    }

    fbConfigs = (*s->getFBConfigs) (dpy,
				    screenNum,
				    &nElements);

    for (i = 0; i <= MAX_DEPTH; i++)
    {
	int j, db, stencil, depth, alpha, mipmap, rgba;

	s->glxPixmapFBConfigs[i].fbConfig       = NULL;
	s->glxPixmapFBConfigs[i].mipmap         = 0;
	s->glxPixmapFBConfigs[i].yInverted      = 0;
	s->glxPixmapFBConfigs[i].textureFormat  = 0;
	s->glxPixmapFBConfigs[i].textureTargets = 0;

	db      = MAXSHORT;
	stencil = MAXSHORT;
	depth   = MAXSHORT;
	mipmap  = 0;
	rgba    = 0;

	for (j = 0; j < nElements; j++)
	{
	    XVisualInfo *vi;
	    int		visualDepth;

	    vi = glXGetVisualFromFBConfig (dpy, fbConfigs[j]);
	    if (vi == NULL)
		continue;

	    visualDepth = vi->depth;

	    XFree (vi);

	    if (visualDepth != i)
		continue;

	    (*s->getFBConfigAttrib) (dpy,
				     fbConfigs[j],
				     GLX_ALPHA_SIZE,
				     &alpha);
	    (*s->getFBConfigAttrib) (dpy,
				     fbConfigs[j],
				     GLX_BUFFER_SIZE,
				     &value);
	    if (value != i && (value - alpha) != i)
		continue;

	    value = 0;
	    if (i == 32)
	    {
		(*s->getFBConfigAttrib) (dpy,
					 fbConfigs[j],
					 GLX_BIND_TO_TEXTURE_RGBA_EXT,
					 &value);

		if (value)
		{
		    rgba = 1;

		    s->glxPixmapFBConfigs[i].textureFormat =
			GLX_TEXTURE_FORMAT_RGBA_EXT;
		}
	    }

	    if (!value)
	    {
		if (rgba)
		    continue;

		(*s->getFBConfigAttrib) (dpy,
					 fbConfigs[j],
					 GLX_BIND_TO_TEXTURE_RGB_EXT,
					 &value);
		if (!value)
		    continue;

		s->glxPixmapFBConfigs[i].textureFormat =
		    GLX_TEXTURE_FORMAT_RGB_EXT;
	    }

	    (*s->getFBConfigAttrib) (dpy,
				     fbConfigs[j],
				     GLX_DOUBLEBUFFER,
				     &value);
	    if (value > db)
		continue;

	    db = value;

	    (*s->getFBConfigAttrib) (dpy,
				     fbConfigs[j],
				     GLX_STENCIL_SIZE,
				     &value);
	    if (value > stencil)
		continue;

	    stencil = value;

	    (*s->getFBConfigAttrib) (dpy,
				     fbConfigs[j],
				     GLX_DEPTH_SIZE,
				     &value);
	    if (value > depth)
		continue;

	    depth = value;

	    if (s->fbo)
	    {
		(*s->getFBConfigAttrib) (dpy,
					 fbConfigs[j],
					 GLX_BIND_TO_MIPMAP_TEXTURE_EXT,
					 &value);
		if (value < mipmap)
		    continue;

		mipmap = value;
	    }

	    (*s->getFBConfigAttrib) (dpy,
				     fbConfigs[j],
				     GLX_Y_INVERTED_EXT,
				     &value);

	    s->glxPixmapFBConfigs[i].yInverted = value;

	    (*s->getFBConfigAttrib) (dpy,
				     fbConfigs[j],
				     GLX_BIND_TO_TEXTURE_TARGETS_EXT,
				     &value);

	    s->glxPixmapFBConfigs[i].textureTargets = value;

	    s->glxPixmapFBConfigs[i].fbConfig = fbConfigs[j];
	    s->glxPixmapFBConfigs[i].mipmap   = mipmap;
	}
    }

    if (nElements)
	XFree (fbConfigs);

    if (!s->glxPixmapFBConfigs[defaultDepth].fbConfig)
    {
	compLogMessage (display, "core", CompLogLevelFatal,
			"No GLXFBConfig for default depth, "
			"this isn't going to work.");
	return FALSE;
    }

    initTexture (s, &s->backgroundTexture);
    s->backgroundLoaded = FALSE;

    s->defaultIcon = NULL;

    s->desktopWindowCount = 0;

    glClearColor (0.0, 0.0, 0.0, 1.0);
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_CULL_FACE);
    glDisable (GL_BLEND);
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4usv (defaultColor);
    glEnableClientState (GL_VERTEX_ARRAY);
    glEnableClientState (GL_TEXTURE_COORD_ARRAY);

    s->canDoSaturated = s->canDoSlightlySaturated = FALSE;
    if (s->textureEnvCombine && s->maxTextureUnits >= 2)
    {
	s->canDoSaturated = TRUE;
	if (s->textureEnvCrossbar && s->maxTextureUnits >= 4)
	    s->canDoSlightlySaturated = TRUE;
    }

    s->redrawTime = 1000 / defaultRefreshRate;
    s->optimalRedrawTime = s->redrawTime;

    reshape (s, s->attrib.width, s->attrib.height);

    detectRefreshRateOfScreen (s);
    detectOutputDevices (s);
    updateOutputDevices (s);

    glLightModelfv (GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    glEnable (GL_LIGHT0);
    glLightfv (GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv (GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv (GL_LIGHT0, GL_POSITION, light0Position);

    glColorMaterial (GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    glNormal3f (0.0f, 0.0f, -1.0f);

    s->lighting	      = FALSE;
    s->slowAnimations = FALSE;

    addScreenToDisplay (display, s);

    getDesktopHints (s);

    screenInitPlugins (s);

    XQueryTree (dpy, s->root,
		&rootReturn, &parentReturn,
		&children, &nchildren);

    for (i = 0; i < nchildren; i++)
	addWindow (s, children[i], i ? children[i - 1] : 0);

    for (w = s->windows; w; w = w->next)
    {
	if (w->attrib.map_state == IsViewable)
	{
	    w->activeNum = s->activeNum++;
	    w->damaged   = TRUE;
	    w->invisible = WINDOW_INVISIBLE (w);
	}
    }

    XFree (children);

    attrib.override_redirect = 1;
    attrib.event_mask	     = PropertyChangeMask;

    s->grabWindow = XCreateWindow (dpy, s->root, -100, -100, 1, 1, 0,
				   CopyFromParent, InputOnly, CopyFromParent,
				   CWOverrideRedirect | CWEventMask,
				   &attrib);
    XMapWindow (dpy, s->grabWindow);

    for (i = 0; i < SCREEN_EDGE_NUM; i++)
    {
	long xdndVersion = 3;

	s->screenEdge[i].id = XCreateWindow (dpy, s->root, -100, -100, 1, 1, 0,
					     CopyFromParent, InputOnly,
					     CopyFromParent, CWOverrideRedirect,
					     &attrib);

	XChangeProperty (dpy, s->screenEdge[i].id, display->xdndAwareAtom,
			 XA_ATOM, 32, PropModeReplace,
			 (unsigned char *) &xdndVersion, 1);

	XSelectInput (dpy, s->screenEdge[i].id,
		      EnterWindowMask   |
		      LeaveWindowMask   |
		      ButtonPressMask   |
		      ButtonReleaseMask |
		      PointerMotionMask);
    }

    updateScreenEdges (s);

    setDesktopHints (s);
    setSupportingWmCheck (s);
    setSupported (s);

    s->normalCursor = XCreateFontCursor (dpy, XC_left_ptr);
    s->busyCursor   = XCreateFontCursor (dpy, XC_watch);

    XDefineCursor (dpy, s->root, s->normalCursor);

    s->filter[NOTHING_TRANS_FILTER] = COMP_TEXTURE_FILTER_FAST;
    s->filter[SCREEN_TRANS_FILTER]  = COMP_TEXTURE_FILTER_GOOD;
    s->filter[WINDOW_TRANS_FILTER]  = COMP_TEXTURE_FILTER_GOOD;

    return TRUE;
}

void
damageScreenRegion (CompScreen *screen,
		    Region     region)
{
    if (screen->damageMask & COMP_SCREEN_DAMAGE_ALL_MASK)
	return;

    XUnionRegion (screen->damage, region, screen->damage);

    screen->damageMask |= COMP_SCREEN_DAMAGE_REGION_MASK;
}

void
damageScreen (CompScreen *s)
{
    s->damageMask |= COMP_SCREEN_DAMAGE_ALL_MASK;
    s->damageMask &= ~COMP_SCREEN_DAMAGE_REGION_MASK;
}

void
damagePendingOnScreen (CompScreen *s)
{
    s->damageMask |= COMP_SCREEN_DAMAGE_PENDING_MASK;
}

void
forEachWindowOnScreen (CompScreen	 *screen,
		       ForEachWindowProc proc,
		       void		 *closure)
{
    CompWindow *w;

    for (w = screen->windows; w; w = w->next)
	(*proc) (w, closure);
}

CompWindow *
findWindowAtScreen (CompScreen *s,
		    Window     id)
{
    if (lastFoundWindow && lastFoundWindow->id == id)
    {
	return lastFoundWindow;
    }
    else
    {
	CompWindow *w;

	for (w = s->windows; w; w = w->next)
	    if (w->id == id)
		return (lastFoundWindow = w);
    }

    return 0;
}

CompWindow *
findTopLevelWindowAtScreen (CompScreen *s,
			    Window     id)
{
    CompWindow *w;

    w = findWindowAtScreen (s, id);
    if (!w)
	return NULL;

    if (w->attrib.override_redirect)
    {
	/* likely a frame window */
	if (w->attrib.class == InputOnly)
	{
	    for (w = s->windows; w; w = w->next)
		if (w->frame == id)
		    return w;
	}

	return NULL;
    }

    return w;
}

void
insertWindowIntoScreen (CompScreen *s,
			CompWindow *w,
			Window	   aboveId)
{
    CompWindow *p;

    if (s->windows)
    {
	if (!aboveId)
	{
	    w->next = s->windows;
	    w->prev = NULL;
	    s->windows->prev = w;
	    s->windows = w;
	}
	else
	{
	    for (p = s->windows; p; p = p->next)
	    {
		if (p->id == aboveId)
		{
		    if (p->next)
		    {
			w->next = p->next;
			w->prev = p;
			p->next->prev = w;
			p->next = w;
		    }
		    else
		    {
			p->next = w;
			w->next = NULL;
			w->prev = p;
			s->reverseWindows = w;
		    }
		    break;
		}
	    }

#ifdef DEBUG
	    if (!p)
		abort ();
#endif

	}
    }
    else
    {
	s->reverseWindows = s->windows = w;
	w->prev = w->next = NULL;
    }
}

void
unhookWindowFromScreen (CompScreen *s,
			CompWindow *w)
{
    CompWindow *next, *prev;

    next = w->next;
    prev = w->prev;

    if (next || prev)
    {
	if (next)
	{
	    if (prev)
	    {
		next->prev = prev;
	    }
	    else
	    {
		s->windows = next;
		next->prev = NULL;
	    }
	}

	if (prev)
	{
	    if (next)
	    {
		prev->next = next;
	    }
	    else
	    {
		s->reverseWindows = prev;
		prev->next = NULL;
	    }
	}
    }
    else
    {
	s->windows = s->reverseWindows = NULL;
    }

    if (w == lastFoundWindow)
	lastFoundWindow = NULL;
    if (w == lastDamagedWindow)
	lastDamagedWindow = NULL;
}

#define POINTER_GRAB_MASK (ButtonReleaseMask | \
			   ButtonPressMask   | \
			   PointerMotionMask)
int
pushScreenGrab (CompScreen *s,
		Cursor     cursor,
		const char *name)
{
    if (s->maxGrab == 0)
    {
	int status;

	status = XGrabPointer (s->display->display, s->grabWindow, TRUE,
			       POINTER_GRAB_MASK,
			       GrabModeAsync, GrabModeAsync,
			       s->root, cursor,
			       CurrentTime);

	if (status == GrabSuccess)
	{
	    status = XGrabKeyboard (s->display->display,
				    s->grabWindow, TRUE,
				    GrabModeAsync, GrabModeAsync,
				    CurrentTime);
	    if (status != GrabSuccess)
	    {
		XUngrabPointer (s->display->display, CurrentTime);
		return 0;
	    }
	}
	else
	    return 0;
    }
    else
    {
	XChangeActivePointerGrab (s->display->display, POINTER_GRAB_MASK,
				  cursor, CurrentTime);
    }

    if (s->grabSize <= s->maxGrab)
    {
	s->grabs = realloc (s->grabs, sizeof (CompGrab) * (s->maxGrab + 1));
	if (!s->grabs)
	    return 0;

	s->grabSize = s->maxGrab + 1;
    }

    s->grabs[s->maxGrab].cursor = cursor;
    s->grabs[s->maxGrab].active = TRUE;
    s->grabs[s->maxGrab].name   = name;

    s->maxGrab++;

    return s->maxGrab;
}

void
updateScreenGrab (CompScreen *s,
		  int        index,
		  Cursor     cursor)
{
  index--;

#ifdef DEBUG
    if (index < 0 || index >= s->maxGrab)
	abort ();
#endif

  XChangeActivePointerGrab (s->display->display, POINTER_GRAB_MASK,
			    cursor, CurrentTime);

  s->grabs[index].cursor = cursor;
}

void
removeScreenGrab (CompScreen *s,
		  int	     index,
		  XPoint     *restorePointer)
{
    int maxGrab;

    index--;

#ifdef DEBUG
    if (index < 0 || index >= s->maxGrab)
	abort ();
#endif

    s->grabs[index].cursor = None;
    s->grabs[index].active = FALSE;

    for (maxGrab = s->maxGrab; maxGrab; maxGrab--)
	if (s->grabs[maxGrab - 1].active)
	    break;

    if (maxGrab != s->maxGrab)
    {
	if (maxGrab)
	{
	    XChangeActivePointerGrab (s->display->display,
				      POINTER_GRAB_MASK,
				      s->grabs[maxGrab - 1].cursor,
				      CurrentTime);
	}
	else
	{
	    if (restorePointer)
		warpPointer (s,
			     restorePointer->x - pointerX,
			     restorePointer->y - pointerY);

	    XUngrabPointer (s->display->display, CurrentTime);
	    XUngrabKeyboard (s->display->display, CurrentTime);
	}

	s->maxGrab = maxGrab;
    }
}

/* otherScreenGrabExist takes a series of strings terminated by a NULL.
   It returns TRUE if a grab exists but it is NOT held by one of the
   plugins listed, returns FALSE otherwise. */

Bool
otherScreenGrabExist (CompScreen *s, ...)
{
    va_list ap;
    char    *name;
    int	    i;

    for (i = 0; i < s->maxGrab; i++)
    {
	if (s->grabs[i].active)
	{
	    va_start (ap, s);

	    name = va_arg (ap, char *);
	    while (name)
	    {
		if (strcmp (name, s->grabs[i].name) == 0)
		    break;

		name = va_arg (ap, char *);
	    }

	    va_end (ap);

	    if (!name)
		return TRUE;
	}
    }

    return FALSE;
}

static void
grabUngrabOneKey (CompScreen   *s,
		  unsigned int modifiers,
		  int          keycode,
		  Bool         grab)
{
    if (grab)
    {
	XGrabKey (s->display->display,
		  keycode,
		  modifiers,
		  s->root,
		  TRUE,
		  GrabModeAsync,
		  GrabModeAsync);
    }
    else
    {
	XUngrabKey (s->display->display,
		    keycode,
		    modifiers,
		    s->root);
    }
}

static Bool
grabUngrabKeys (CompScreen   *s,
		unsigned int modifiers,
		int          keycode,
		Bool         grab)
{
    XModifierKeymap *modMap = s->display->modMap;
    int ignore, mod, k;

    compCheckForError (s->display->display);

    for (ignore = 0; ignore <= s->display->ignoredModMask; ignore++)
    {
	if (ignore & ~s->display->ignoredModMask)
	    continue;

	if (keycode != 0)
	{
	    grabUngrabOneKey (s, modifiers | ignore, keycode, grab);
	}
	else
	{
	    for (mod = 0; mod < 8; mod++)
	    {
		if (modifiers & (1 << mod))
		{
		    for (k = mod * modMap->max_keypermod;
			 k < (mod + 1) * modMap->max_keypermod;
			 k++)
		    {
			if (modMap->modifiermap[k])
			{
			    grabUngrabOneKey (
				s,
				(modifiers & ~(1 << mod)) | ignore,
				modMap->modifiermap[k],
				grab);
			}
		    }
		}
	    }
	}

	if (compCheckForError (s->display->display))
	    return FALSE;
    }

    return TRUE;
}

static Bool
addPassiveKeyGrab (CompScreen	  *s,
		   CompKeyBinding *key)
{
    CompKeyGrab  *keyGrab;
    unsigned int mask;
    int          i;

    mask = virtualToRealModMask (s->display, key->modifiers);

    for (i = 0; i < s->nKeyGrab; i++)
    {
	if (key->keycode == s->keyGrab[i].keycode &&
	    mask         == s->keyGrab[i].modifiers)
	{
	    s->keyGrab[i].count++;
	    return TRUE;
	}
    }

    keyGrab = realloc (s->keyGrab, sizeof (CompKeyGrab) * (s->nKeyGrab + 1));
    if (!keyGrab)
	return FALSE;

    s->keyGrab = keyGrab;

    if (!(mask & CompNoMask))
    {
	if (!grabUngrabKeys (s, mask, key->keycode, TRUE))
	    return FALSE;
    }

    s->keyGrab[s->nKeyGrab].keycode   = key->keycode;
    s->keyGrab[s->nKeyGrab].modifiers = mask;
    s->keyGrab[s->nKeyGrab].count     = 1;

    s->nKeyGrab++;

    return TRUE;
}

static void
removePassiveKeyGrab (CompScreen     *s,
		      CompKeyBinding *key)
{
    unsigned int mask;
    int          i;

    for (i = 0; i < s->nKeyGrab; i++)
    {
	mask = virtualToRealModMask (s->display, key->modifiers);
	if (key->keycode == s->keyGrab[i].keycode &&
	    mask         == s->keyGrab[i].modifiers)
	{
	    s->keyGrab[i].count--;
	    if (s->keyGrab[i].count)
		return;

	    memmove (s->keyGrab + i, s->keyGrab + i + 1, 
		     (s->nKeyGrab - (i + 1)) * sizeof (CompKeyGrab));

	    s->nKeyGrab--;
	    s->keyGrab = realloc (s->keyGrab,
				  sizeof (CompKeyGrab) * s->nKeyGrab);

	    if (!(mask & CompNoMask))
		grabUngrabKeys (s, mask, key->keycode, FALSE);
	}
    }
}

static void
updatePassiveKeyGrabs (CompScreen *s)
{
    int i;

    XUngrabKey (s->display->display, AnyKey, AnyModifier, s->root);

    for (i = 0; i < s->nKeyGrab; i++)
    {
	if (!(s->keyGrab[i].modifiers & CompNoMask))
	{
	    grabUngrabKeys (s, s->keyGrab[i].modifiers,
			    s->keyGrab[i].keycode, TRUE);
	}
    }
}

static Bool
addPassiveButtonGrab (CompScreen        *s,
		      CompButtonBinding *button)
{
    CompButtonGrab *buttonGrab;
    int            i;

    for (i = 0; i < s->nButtonGrab; i++)
    {
	if (button->button    == s->buttonGrab[i].button &&
	    button->modifiers == s->buttonGrab[i].modifiers)
	{
	    s->buttonGrab[i].count++;
	    return TRUE;
	}
    }

    buttonGrab = realloc (s->buttonGrab,
			  sizeof (CompButtonGrab) * (s->nButtonGrab + 1));
    if (!buttonGrab)
	return FALSE;

    s->buttonGrab = buttonGrab;

    s->buttonGrab[s->nButtonGrab].button    = button->button;
    s->buttonGrab[s->nButtonGrab].modifiers = button->modifiers;
    s->buttonGrab[s->nButtonGrab].count     = 1;

    s->nButtonGrab++;

    return TRUE;
}

static void
removePassiveButtonGrab (CompScreen        *s,
			 CompButtonBinding *button)
{
    int          i;

    for (i = 0; i < s->nButtonGrab; i++)
    {
	if (button->button    == s->buttonGrab[i].button &&
	    button->modifiers == s->buttonGrab[i].modifiers)
	{
	    s->buttonGrab[i].count--;
	    if (s->buttonGrab[i].count)
		return;

	    memmove (s->buttonGrab + i, s->buttonGrab + i + 1,
		     (s->nButtonGrab - (i + 1)) * sizeof (CompButtonGrab));

	    s->nButtonGrab--;
	    s->buttonGrab = realloc (s->buttonGrab,
				     sizeof (CompButtonGrab) * s->nButtonGrab);
	}
    }
}

Bool
addScreenAction (CompScreen *s,
		 CompAction *action)
{
    if (action->type & CompBindingTypeKey)
    {
	if (!addPassiveKeyGrab (s, &action->key))
	    return FALSE;
    }

    if (action->type & CompBindingTypeButton)
    {
	if (!addPassiveButtonGrab (s, &action->button))
	{
	    if (action->type & CompBindingTypeKey)
		removePassiveKeyGrab (s, &action->key);

	    return FALSE;
	}
    }

    if (action->edgeMask)
    {
	int i;

	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	    if (action->edgeMask & (1 << i))
		enableScreenEdge (s, i);
    }

    return TRUE;
}

void
removeScreenAction (CompScreen *s,
		    CompAction *action)
{
    if (action->type & CompBindingTypeKey)
	removePassiveKeyGrab (s, &action->key);

    if (action->type & CompBindingTypeButton)
	removePassiveButtonGrab (s, &action->button);

    if (action->edgeMask)
    {
	int i;

	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	    if (action->edgeMask & (1 << i))
		disableScreenEdge (s, i);
    }
}

void
updatePassiveGrabs (CompScreen *s)
{
    updatePassiveKeyGrabs (s);
}

static void
computeWorkareaForBox (CompScreen *s,
		       BoxPtr     pBox,
		       XRectangle *area)
{
    CompWindow *w;
    Region     region;
    REGION     r;
    int	       x1, y1, x2, y2;

    region = XCreateRegion ();
    if (!region)
    {
	area->x      = pBox->x1;
	area->y      = pBox->y1;
	area->width  = pBox->x1 - pBox->x1;
	area->height = pBox->y2 - pBox->y1;

	return;
    }

    r.rects    = &r.extents;
    r.numRects = r.size = 1;
    r.extents  = *pBox;

    XUnionRegion (&r, region, region);

    for (w = s->windows; w; w = w->next)
    {
	if (!w->mapNum)
	    continue;

	if (w->struts)
	{
	    r.extents.y1 = pBox->y1;
	    r.extents.y2 = pBox->y2;

	    x1 = w->struts->left.x;
	    y1 = w->struts->left.y;
	    x2 = x1 + w->struts->left.width;
	    y2 = y1 + w->struts->left.height;

	    if (y1 < pBox->y2 && y2 > pBox->y1)
	    {
		r.extents.x1 = x1;
		r.extents.x2 = x2;

		XSubtractRegion (region, &r, region);
	    }

	    x1 = w->struts->right.x;
	    y1 = w->struts->right.y;
	    x2 = x1 + w->struts->right.width;
	    y2 = y1 + w->struts->right.height;

	    if (y1 < pBox->y2 && y2 > pBox->y1)
	    {
		r.extents.x1 = x1;
		r.extents.x2 = x2;

		XSubtractRegion (region, &r, region);
	    }

	    r.extents.x1 = pBox->x1;
	    r.extents.x2 = pBox->x2;

	    x1 = w->struts->top.x;
	    y1 = w->struts->top.y;
	    x2 = x1 + w->struts->top.width;
	    y2 = y1 + w->struts->top.height;

	    if (x1 < pBox->x2 && x2 > pBox->x1)
	    {
		r.extents.y1 = y1;
		r.extents.y2 = y2;

		XSubtractRegion (region, &r, region);
	    }

	    x1 = w->struts->bottom.x;
	    y1 = w->struts->bottom.y;
	    x2 = x1 + w->struts->bottom.width;
	    y2 = y1 + w->struts->bottom.height;

	    if (x1 < pBox->x2 && x2 > pBox->x1)
	    {
		r.extents.y1 = y1;
		r.extents.y2 = y2;

		XSubtractRegion (region, &r, region);
	    }
	}
    }

    area->x      = region->extents.x1;
    area->y      = region->extents.y1;
    area->width  = region->extents.x2 - region->extents.x1;
    area->height = region->extents.y2 - region->extents.y1;

    XDestroyRegion (region);
}

void
updateWorkareaForScreen (CompScreen *s)
{
    XRectangle workArea;
    BoxRec     box;
    int        i;

    for (i = 0; i < s->nOutputDev; i++)
	computeWorkareaForBox (s,
			       &s->outputDev[i].region.extents,
			       &s->outputDev[i].workArea);

    box.x1 = 0;
    box.y1 = 0;
    box.x2 = s->width;
    box.y2 = s->height;

    computeWorkareaForBox (s, &box, &workArea);

    if (memcmp (&workArea, &s->workArea, sizeof (XRectangle)))
    {
	s->workArea = workArea;

	setDesktopHints (s);
    }
}

static Bool
isClientListWindow (CompWindow *w)
{
    /* windows with client id less than 2 have been destroyed and only exists
       because some plugin keeps a reference to them. they should not be in
       client lists */
    if (w->id < 2)
	return FALSE;

    if (w->attrib.override_redirect)
	return FALSE;

    if (w->attrib.map_state != IsViewable)
    {
	if (!(w->state & CompWindowStateHiddenMask))
	    return FALSE;
    }

    return TRUE;
}

static void
countClientListWindow (CompWindow *w,
		       void       *closure)
{
    if (isClientListWindow (w))
    {
	int *num = (int *) closure;

	*num = *num + 1;
    }
}

static void
addClientListWindow (CompWindow *w,
		     void       *closure)
{
    if (isClientListWindow (w))
    {
	int *num = (int *) closure;

	w->screen->clientList[*num] = w;
	*num = *num + 1;
    }
}

static int
compareMappingOrder (const void *w1,
		     const void *w2)
{
    return (*((CompWindow **) w1))->mapNum - (*((CompWindow **) w2))->mapNum;
}

void
updateClientListForScreen (CompScreen *s)
{
    Window *clientList;
    Window *clientListStacking;
    Bool   updateClientList = FALSE;
    Bool   updateClientListStacking = FALSE;
    int	   i, n = 0;

    forEachWindowOnScreen (s, countClientListWindow, (void *) &n);

    if (n == 0)
    {
	if (n != s->nClientList)
	{
	    free (s->clientList);

	    s->clientList  = NULL;
	    s->nClientList = 0;

	    XChangeProperty (s->display->display, s->root,
			     s->display->clientListAtom,
			     XA_WINDOW, 32, PropModeReplace,
			     (unsigned char *) &s->grabWindow, 1);
	    XChangeProperty (s->display->display, s->root,
			     s->display->clientListStackingAtom,
			     XA_WINDOW, 32, PropModeReplace,
			     (unsigned char *) &s->grabWindow, 1);
	}

	return;
    }

    if (n != s->nClientList)
    {
	CompWindow **list;

	list = realloc (s->clientList,
			(sizeof (CompWindow *) + sizeof (Window) * 2) * n);
	if (!list)
	    return;

	s->clientList  = list;
	s->nClientList = n;

	updateClientList = updateClientListStacking = TRUE;
    }

    clientList	       = (Window *) (s->clientList + n);
    clientListStacking = clientList + n;

    i = 0;
    forEachWindowOnScreen (s, addClientListWindow, (void *) &i);

    for (i = 0; i < n; i++)
    {
	if (!updateClientListStacking)
	{
	    if (clientListStacking[i] != s->clientList[i]->id)
		updateClientListStacking = TRUE;
	}

	clientListStacking[i] = s->clientList[i]->id;
    }

    /* sort window list in mapping order */
    qsort (s->clientList, n, sizeof (CompWindow *), compareMappingOrder);

    for (i = 0; i < n; i++)
    {
	if (!updateClientList)
	{
	    if (clientList[i] != s->clientList[i]->id)
		updateClientList = TRUE;
	}

	clientList[i] = s->clientList[i]->id;
    }

    if (updateClientList)
	XChangeProperty (s->display->display, s->root,
			 s->display->clientListAtom,
			 XA_WINDOW, 32, PropModeReplace,
			 (unsigned char *) clientList, s->nClientList);

    if (updateClientListStacking)
	XChangeProperty (s->display->display, s->root,
			 s->display->clientListStackingAtom,
			 XA_WINDOW, 32, PropModeReplace,
			 (unsigned char *) clientListStacking, s->nClientList);
}

Window
getActiveWindow (CompDisplay *display,
		 Window      root)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;
    Window	  w = None;

    result = XGetWindowProperty (display->display, root,
				 display->winActiveAtom, 0L, 1L, FALSE,
				 XA_WINDOW, &actual, &format,
				 &n, &left, &data);

    if (result == Success && n && data)
    {
	memcpy (&w, data, sizeof (Window));
	XFree (data);
    }

    return w;
}

void
toolkitAction (CompScreen *s,
	       Atom	  toolkitAction,
	       Time       eventTime,
	       Window	  window,
	       long	  data0,
	       long	  data1,
	       long	  data2)
{
    XEvent ev;

    ev.type		    = ClientMessage;
    ev.xclient.window	    = window;
    ev.xclient.message_type = s->display->toolkitActionAtom;
    ev.xclient.format	    = 32;
    ev.xclient.data.l[0]    = toolkitAction;
    ev.xclient.data.l[1]    = eventTime;
    ev.xclient.data.l[2]    = data0;
    ev.xclient.data.l[3]    = data1;
    ev.xclient.data.l[4]    = data2;

    XUngrabPointer (s->display->display, CurrentTime);
    XUngrabKeyboard (s->display->display, CurrentTime);

    XSendEvent (s->display->display, s->root, FALSE, StructureNotifyMask, &ev);
}

void
runCommand (CompScreen *s,
	    const char *command)
{
    if (*command == '\0')
	return;

    if (fork () == 0)
    {
	setsid ();
	putenv (s->display->displayString);
	execl ("/bin/sh", "/bin/sh", "-c", command, NULL);
	exit (0);
    }
}

void
moveScreenViewport (CompScreen *s,
		    int	       tx,
		    int	       ty,
		    Bool       sync)
{
    CompWindow *w;
    int         m, wx, wy, vWidth, vHeight;

    tx = s->x - tx;
    tx = MOD (tx, s->hsize);
    tx -= s->x;

    ty = s->y - ty;
    ty = MOD (ty, s->vsize);
    ty -= s->y;

    if (!tx && !ty)
	return;

    s->x += tx;
    s->y += ty;

    tx *= -s->width;
    ty *= -s->height;

    vWidth = s->width * s->hsize;
    vHeight = s->height * s->vsize;

    for (w = s->windows; w; w = w->next)
    {
	if (w->attrib.override_redirect)
	    continue;

	if (!w->managed && w->attrib.map_state != IsViewable)
	    continue;

	if (w->type & (CompWindowTypeDesktopMask | CompWindowTypeDockMask))
	    continue;

	if (w->state & CompWindowStateStickyMask)
	    continue;

	/* x */
	if (s->hsize == 1)
	{
	    wx = tx;
	}
	else
	{
	    m = w->attrib.x + tx;
	    if (m - w->input.left < s->width - vWidth)
		wx = tx + vWidth;
	    else if (m + w->width + w->input.right > vWidth)
		wx = tx - vWidth;
	    else
		wx = tx;
	}

	if (w->saveMask & CWX)
	    w->saveWc.x += wx;

	/* y */
	if (s->vsize == 1)
	{
	    wy = ty;
	}
	else
	{
	    m = w->attrib.y + ty;
	    if (m - w->input.top < s->height - vHeight)
		wy = ty + vHeight;
	    else if (m + w->height + w->input.bottom > vHeight)
		wy = ty - vHeight;
	    else
		wy = ty;
	}

	if (w->saveMask & CWY)
	    w->saveWc.y += wy;

	/* move */
	moveWindow (w, wx, wy, sync, TRUE);

	if (sync)
	    syncWindowPosition (w);
    }

    if (sync)
    {
	setDesktopHints (s);

	setCurrentActiveWindowHistory (s, s->x, s->y);

	w = findWindowAtDisplay (s->display, s->display->activeWindow);
	if (w)
	{
	    int x, y;

	    defaultViewportForWindow (w, &x, &y);

	    /* add window to current history if it's default viewport is
	       still the current one. */
	    if (s->x == x && s->y == y)
		addToCurrentActiveWindowHistory (s, w->id);
	}
    }
}

void
moveWindowToViewportPosition (CompWindow *w,
			      int	 x,
			      int        y,
			      Bool       sync)
{
    int	tx, vWidth = w->screen->width * w->screen->hsize;
    int ty, vHeight = w->screen->height * w->screen->vsize;

    if (w->screen->hsize != 1)
    {
	x += w->screen->x * w->screen->width;
	x = MOD (x, vWidth);
	x -= w->screen->x * w->screen->width;
    }

    if (w->screen->vsize != 1)
    {
	y += w->screen->y * w->screen->height;
	y = MOD (y, vHeight);
	y -= w->screen->y * w->screen->height;
    }

    tx = x - w->attrib.x;
    ty = y - w->attrib.y;

    if (tx || ty)
    {
	int m, wx, wy;

	if (!w->managed)
	    return;

	if (w->type & (CompWindowTypeDesktopMask | CompWindowTypeDockMask))
	    return;

	if (w->state & CompWindowStateStickyMask)
	    return;

	wx = tx;
	wy = ty;

	if (w->screen->hsize != 1)
	{
	    m = w->attrib.x + tx;

	    if (m - w->output.left < w->screen->width - vWidth)
		wx = tx + vWidth;
	    else if (m + w->width + w->output.right > vWidth)
		wx = tx - vWidth;
	}

	if (w->screen->vsize != 1)
	{
	    m = w->attrib.y + ty;

	    if (m - w->output.top < w->screen->height - vHeight)
		wy = ty + vHeight;
	    else if (m + w->height + w->output.bottom > vHeight)
		wy = ty - vHeight;
	}

	if (w->saveMask & CWX)
	    w->saveWc.x += wx;

	if (w->saveMask & CWY)
	    w->saveWc.y += wy;

	moveWindow (w, wx, wy, sync, TRUE);

	if (sync)
	    syncWindowPosition (w);
    }
}

CompGroup *
addGroupToScreen (CompScreen *s,
		  Window     id)
{
    CompGroup *group;

    group = malloc (sizeof (CompGroup));
    if (!group)
	return NULL;

    group->next   = s->groups;
    group->refCnt = 1;
    group->id     = id;

    s->groups = group;

    return group;
}

void
removeGroupFromScreen (CompScreen *s,
		       CompGroup  *group)
{
    group->refCnt--;
    if (group->refCnt)
	return;

    if (group == s->groups)
    {
	s->groups = group->next;
    }
    else
    {
	CompGroup *g;

	for (g = s->groups; g; g = g->next)
	{
	    if (g->next == group)
	    {
		g->next = group->next;
		break;
	    }
	}
    }

    free (group);
}

CompGroup *
findGroupAtScreen (CompScreen *s,
		   Window     id)
{
    CompGroup *g;

    for (g = s->groups; g; g = g->next)
	if (g->id == id)
	    return g;

    return NULL;
}

void
applyStartupProperties (CompScreen *screen,
			CompWindow *window)
{
    CompStartupSequence *s;
    const char	        *startupId = window->startupId;

    if (!startupId)
    {
	CompWindow *leader;

	leader = findWindowAtScreen (screen, window->clientLeader);
	if (leader)
	    startupId = leader->startupId;

	if (!startupId)
	    return;
    }

    for (s = screen->startupSequences; s; s = s->next)
    {
	const char *id;

	id = sn_startup_sequence_get_id (s->sequence);
	if (strcmp (id, startupId) == 0)
	    break;
    }

    if (s)
    {
	int workspace;

	window->initialViewportX = s->viewportX;
	window->initialViewportY = s->viewportY;

	workspace = sn_startup_sequence_get_workspace (s->sequence);
	if (workspace >= 0)
	    window->desktop = workspace;

	window->initialTimestamp    =
	    sn_startup_sequence_get_timestamp (s->sequence);
	window->initialTimestampSet = TRUE;
    }
}

void
sendWindowActivationRequest (CompScreen *s,
			     Window	id)
{
    XEvent xev;

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = s->display->display;
    xev.xclient.format  = 32;

    xev.xclient.message_type = s->display->winActiveAtom;
    xev.xclient.window	     = id;

    xev.xclient.data.l[0] = 2;
    xev.xclient.data.l[1] = 0;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent (s->display->display,
		s->root,
		FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xev);
}

void
screenTexEnvMode (CompScreen *s,
		  GLenum     mode)
{
    if (s->lighting)
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    else
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
}

void
screenLighting (CompScreen *s,
		Bool       lighting)
{
    if (s->lighting != lighting)
    {
	if (!s->opt[COMP_SCREEN_OPTION_LIGHTING].value.b)
	    lighting = FALSE;

	if (lighting)
	{
	    glEnable (GL_COLOR_MATERIAL);
	    glEnable (GL_LIGHTING);
	}
	else
	{
	    glDisable (GL_COLOR_MATERIAL);
	    glDisable (GL_LIGHTING);
	}

	s->lighting = lighting;

	screenTexEnvMode (s, GL_REPLACE);
    }
}

void
enableScreenEdge (CompScreen *s,
		  int	     edge)
{
    s->screenEdge[edge].count++;
    if (s->screenEdge[edge].count == 1)
	XMapRaised (s->display->display, s->screenEdge[edge].id);
}

void
disableScreenEdge (CompScreen *s,
		   int	      edge)
{
    s->screenEdge[edge].count--;
    if (s->screenEdge[edge].count == 0)
	XUnmapWindow (s->display->display, s->screenEdge[edge].id);
}

Window
getTopWindow (CompScreen *s)
{
    CompWindow *w;

    /* return first window that has not been destroyed */
    for (w = s->reverseWindows; w; w = w->prev)
    {
	if (w->id > 1)
	    return w->id;
    }

    return None;
}

void
makeScreenCurrent (CompScreen *s)
{
    if (currentRoot != s->root)
    {
	glXMakeCurrent (s->display->display, s->output, s->ctx);
	currentRoot = s->root;
    }

    s->pendingCommands = TRUE;
}

void
finishScreenDrawing (CompScreen *s)
{
    if (s->pendingCommands)
    {
	makeScreenCurrent (s);
	glFinish ();

	s->pendingCommands = FALSE;
    }
}

int
outputDeviceForPoint (CompScreen *s,
		      int	 x,
		      int	 y)
{
    int i, x1, y1, x2, y2;

    i = s->nOutputDev;
    while (i--)
    {
	x1 = s->outputDev[i].region.extents.x1;
	y1 = s->outputDev[i].region.extents.y1;
	x2 = s->outputDev[i].region.extents.x2;
	y2 = s->outputDev[i].region.extents.y2;

	if (x1 <= x && x2 > x && y1 <= y && y2 > y)
	    return i;
    }

    return s->currentOutputDev;
}

void
getCurrentOutputExtents (CompScreen *s,
			 int	    *x1,
			 int	    *y1,
			 int	    *x2,
			 int	    *y2)
{
    if (x1)
	*x1 = s->outputDev[s->currentOutputDev].region.extents.x1;

    if (y1)
	*y1 = s->outputDev[s->currentOutputDev].region.extents.y1;

    if (x2)
	*x2 = s->outputDev[s->currentOutputDev].region.extents.x2;

    if (y2)
	*y2 = s->outputDev[s->currentOutputDev].region.extents.y2;
}

void
setNumberOfDesktops (CompScreen   *s,
		     unsigned int nDesktop)
{
    CompWindow *w;

    if (nDesktop < 1 || nDesktop >= 0xffffffff)
	return;

    if (nDesktop == s->nDesktop)
	return;

    if (s->currentDesktop >= nDesktop)
	s->currentDesktop = nDesktop - 1;

    for (w = s->windows; w; w = w->next)
    {
	if (w->desktop == 0xffffffff)
	    continue;

	if (w->desktop >= nDesktop)
	    setDesktopForWindow (w, nDesktop - 1);
    }

    s->nDesktop = nDesktop;

    setDesktopHints (s);
}

void
setCurrentDesktop (CompScreen   *s,
		   unsigned int desktop)
{
    unsigned long data;
    CompWindow    *w;

    if (desktop >= s->nDesktop)
	return;

    if (desktop == s->currentDesktop)
	return;

    s->currentDesktop = desktop;

    for (w = s->windows; w; w = w->next)
    {
	if (w->desktop == 0xffffffff)
	    continue;

	if (w->desktop == desktop)
	    showWindow (w);
	else
	    hideWindow (w);
    }

    data = desktop;

    XChangeProperty (s->display->display, s->root,
		     s->display->currentDesktopAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &data, 1);
}

void
getWorkareaForOutput (CompScreen *s,
		      int	 output,
		      XRectangle *area)
{
    *area = s->outputDev[output].workArea;
}

void
setDefaultViewport (CompScreen *s)
{
    s->lastViewport.x	   = s->outputDev->region.extents.x1;
    s->lastViewport.y	   = s->height - s->outputDev->region.extents.y2;
    s->lastViewport.width  = s->outputDev->width;
    s->lastViewport.height = s->outputDev->height;

    glViewport (s->lastViewport.x,
		s->lastViewport.y,
		s->lastViewport.width,
		s->lastViewport.height);
}

void
outputChangeNotify (CompScreen *s)
{
}

void
clearScreenOutput (CompScreen	*s,
		   CompOutput	*output,
		   unsigned int mask)
{
    BoxPtr pBox = &output->region.extents;

    if (pBox->x1 != 0	     ||
	pBox->y1 != 0	     ||
	pBox->x2 != s->width ||
	pBox->y2 != s->height)
    {
	glPushAttrib (GL_SCISSOR_BIT);

	glEnable (GL_SCISSOR_TEST);
	glScissor (pBox->x1,
		   s->height - pBox->y2,
		   pBox->x2 - pBox->x1,
		   pBox->y2 - pBox->y1);
	glClear (mask);

	glPopAttrib ();
    }
    else
    {
	glClear (mask);
    }
}

/* Returns default viewport for some window geometry. If the window spans
   more than one viewport the most appropriate viewport is returned. How the
   most appropriate viewport is computed can be made optional if necessary. It
   is currently computed as the viewport where the center of the window is
   located, except for when the window is visible in the current viewport as
   the current viewport is then always returned. */
void
viewportForGeometry (CompScreen *s,
		     int	x,
		     int	y,
		     int	width,
		     int	height,
		     int	borderWidth,
		     int	*viewportX,
		     int	*viewportY)
{
    int	centerX;
    int	centerY;

    width  += borderWidth * 2;
    height += borderWidth * 2;

    if ((x < s->width  && x + width  > 0) &&
	(y < s->height && y + height > 0))
    {
	if (viewportX)
	    *viewportX = s->x;

	if (viewportY)
	    *viewportY = s->y;

	return;
    }

    if (viewportX)
    {
	centerX = x + (width >> 1);
	if (centerX < 0)
	    *viewportX = s->x + ((centerX / s->width) - 1) % s->hsize;
	else
	    *viewportX = s->x + (centerX / s->width) % s->hsize;
    }

    if (viewportY)
    {
	centerY = y + (height >> 1);
	if (centerY < 0)
	    *viewportY = s->y + ((centerY / s->height) - 1) % s->vsize;
	else
	    *viewportY = s->y + (centerY / s->height) % s->vsize;
    }
}

int
outputDeviceForGeometry (CompScreen *s,
			 int	    x,
			 int	    y,
			 int	    width,
			 int	    height,
			 int	    borderWidth)
{
    int output = s->currentOutputDev;
    int x1, y1, x2, y2;

    width  += borderWidth * 2;
    height += borderWidth * 2;

    x1 = s->outputDev[output].region.extents.x1;
    y1 = s->outputDev[output].region.extents.y1;
    x2 = s->outputDev[output].region.extents.x2;
    y2 = s->outputDev[output].region.extents.y2;

    if (x1 >= x + width  ||
	y1 >= y + height ||
	x2 <= x		 ||
	y2 <= y)
    {
	output = outputDeviceForPoint (s, x + width  / 2, y + height / 2);
    }

    return output;
}

Bool
updateDefaultIcon (CompScreen *screen)
{
    CompIcon *icon;
    char     *file = screen->opt[COMP_SCREEN_OPTION_DEFAULT_ICON].value.s;
    void     *data;
    int      width, height;

    if (screen->defaultIcon)
    {
	finiTexture (screen, &screen->defaultIcon->texture);
	free (screen->defaultIcon);
	screen->defaultIcon = NULL;
    }

    if (!readImageFromFile (screen->display, file, &width, &height, &data))
	return FALSE;

    icon = malloc (sizeof (CompIcon) + width * height * sizeof (CARD32));
    if (!icon)
    {
	free (data);
	return FALSE;
    }

    initTexture (screen, &icon->texture);

    icon->width  = width;
    icon->height = height;

    memcpy (icon + 1, data, + width * height * sizeof (CARD32));

    screen->defaultIcon = icon;

    free (data);

    return TRUE;
}

CompCursor *
findCursorAtScreen (CompScreen *screen)
{
    return screen->cursors;
}

CompCursorImage *
findCursorImageAtScreen (CompScreen    *screen,
			 unsigned long serial)
{
    CompCursorImage *image;

    for (image = screen->cursorImages; image; image = image->next)
	if (image->serial == serial)
	    return image;

    return NULL;
}

void
setCurrentActiveWindowHistory (CompScreen *s,
			       int	  x,
			       int	  y)
{
    int	i, min = 0;

    for (i = 0; i < ACTIVE_WINDOW_HISTORY_NUM; i++)
    {
	if (s->history[i].x == x && s->history[i].y == y)
	{
	    s->currentHistory = i;
	    return;
	}
    }

    for (i = 1; i < ACTIVE_WINDOW_HISTORY_NUM; i++)
	if (s->history[i].activeNum < s->history[min].activeNum)
	    min = i;

    s->currentHistory = min;

    s->history[min].activeNum = s->activeNum;
    s->history[min].x	      = x;
    s->history[min].y	      = y;

    memset (s->history[min].id, 0, sizeof (s->history[min].id));
}

void
addToCurrentActiveWindowHistory (CompScreen *s,
				 Window	    id)
{
    CompActiveWindowHistory *history = &s->history[s->currentHistory];
    Window		    tmp, next = id;
    int			    i;

    /* walk and move history */
    for (i = 0; i < ACTIVE_WINDOW_HISTORY_SIZE; i++)
    {
	tmp = history->id[i];
	history->id[i] = next;
	next = tmp;

	/* we're done when we find an old instance or an empty slot */
	if (tmp == id || tmp == None)
	    break;
    }

    history->activeNum = s->activeNum;
}
