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
#include <X11/cursorfont.h>

#include <compiz.h>

#define DETECT_REFRESH_RATE_DEFAULT TRUE

#define SCREEN_SIZE_DEFAULT 4
#define SCREEN_SIZE_MIN	    4
#define SCREEN_SIZE_MAX	    32

#define LIGHTING_DEFAULT TRUE

#define OPACITY_STEP_DEFAULT 10
#define OPACITY_STEP_MIN     1
#define OPACITY_STEP_MAX     50

#define UNREDIRECT_FS_DEFAULT FALSE

#define DEFAULT_ICON_DEFAULT "icon.png"

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

static void
setVirtualScreenSize (CompScreen *screen,
		      int	 size)
{
    unsigned long data[2];

    data[0] = screen->width * size;
    data[1] = screen->height;

    XChangeProperty (screen->display->display, screen->root,
		     screen->display->desktopGeometryAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 2);

    screen->size = size;
}

static Bool
updateDefaultIcon (CompScreen *screen)
{
    CompIcon     *icon;
    char         *data;
    unsigned int width, height;

    if (!readPng (screen->opt[COMP_SCREEN_OPTION_DEFAULT_ICON].value.s,
		  &data, &width, &height))
	return FALSE;

    icon = malloc (sizeof (CompIcon) + width * height * sizeof (CARD32));
    if (!icon)
    {
	free (data);
	return FALSE;
    }

    if (screen->defaultIcon)
    {
	finiTexture (screen, &screen->defaultIcon->texture);
	free (screen->defaultIcon);
    }

    initTexture (screen, &icon->texture);

    icon->width  = width;
    icon->height = height;

    memcpy (icon + 1, data, + width * height * sizeof (CARD32));

    screen->defaultIcon = icon;

    free (data);

    return TRUE;
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
		 char	         *name,
		 CompOptionValue *value)
{
    CompOption *o;
    int	       index;

    o = compFindOption (screen->opt, NUM_OPTIONS (screen), name, &index);
    if (!o)
	return FALSE;

    switch (index) {
    case COMP_SCREEN_OPTION_DETECT_REFRESH_RATE:
    case COMP_SCREEN_OPTION_LIGHTING:
    case COMP_SCREEN_OPTION_UNREDIRECT_FS:
	if (compSetBoolOption (o, value))
	    return TRUE;
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
    case COMP_SCREEN_OPTION_SIZE:
	if (compSetIntOption (o, value))
	{
	    if (o->value.i * screen->width > MAXSHORT)
		return FALSE;

	    setVirtualScreenSize (screen, o->value.i);
	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_OPACITY_STEP:
	if (compSetIntOption (o, value))
	{
	    screen->opacityStep = o->value.i;
	    return TRUE;
	}
	break;
    case COMP_SCREEN_OPTION_DEFAULT_ICON:
	if (compSetStringOption (o, value))
	    return updateDefaultIcon (screen);
    default:
	break;
    }

    return FALSE;
}

static Bool
setScreenOptionForPlugin (CompScreen      *screen,
			  char	          *plugin,
			  char	          *name,
			  CompOptionValue *value)
{
    CompPlugin *p;

    p = findActivePlugin (plugin);
    if (p && p->vTable->setScreenOption)
	return (*p->vTable->setScreenOption) (screen, name, value);

    return FALSE;
}

static void
compScreenInitOptions (CompScreen *screen)
{
    CompOption *o;

    o = &screen->opt[COMP_SCREEN_OPTION_DETECT_REFRESH_RATE];
    o->name       = "detect_refresh_rate";
    o->shortDesc  = "Detect Refresh Rate";
    o->longDesc   = "Automatic detection of refresh rate";
    o->type       = CompOptionTypeBool;
    o->value.b    = DETECT_REFRESH_RATE_DEFAULT;

    o = &screen->opt[COMP_SCREEN_OPTION_LIGHTING];
    o->name       = "lighting";
    o->shortDesc  = "Lighting";
    o->longDesc   = "Use diffuse light when screen is transformed";
    o->type       = CompOptionTypeBool;
    o->value.b    = LIGHTING_DEFAULT;

    o = &screen->opt[COMP_SCREEN_OPTION_REFRESH_RATE];
    o->name       = "refresh_rate";
    o->shortDesc  = "Refresh Rate";
    o->longDesc   = "The rate at which the screen is redrawn (times/second)";
    o->type       = CompOptionTypeInt;
    o->value.i    = defaultRefreshRate;
    o->rest.i.min = 1;
    o->rest.i.max = 200;

    o = &screen->opt[COMP_SCREEN_OPTION_SIZE];
    o->name	  = "size";
    o->shortDesc  = "Virtual Size";
    o->longDesc	  = "Screen size multiplier for virtual size";
    o->type	  = CompOptionTypeInt;
    o->value.i    = SCREEN_SIZE_DEFAULT;
    o->rest.i.min = SCREEN_SIZE_MIN;
    o->rest.i.max = SCREEN_SIZE_MAX;

    o = &screen->opt[COMP_SCREEN_OPTION_OPACITY_STEP];
    o->name		= "opacity_step";
    o->shortDesc	= "Opacity Step";
    o->longDesc		= "Opacity change step";
    o->type		= CompOptionTypeInt;
    o->value.i		= OPACITY_STEP_DEFAULT;
    o->rest.i.min	= OPACITY_STEP_MIN;
    o->rest.i.max	= OPACITY_STEP_MAX;

    o = &screen->opt[COMP_SCREEN_OPTION_UNREDIRECT_FS];
    o->name       = "unredirect_fullscreen_windows";
    o->shortDesc  = "Unredirect Fullscreen Windows";
    o->longDesc   = "Allow drawing of fullscreen windows to not be redirected "
	"to offscreen pixmaps";
    o->type       = CompOptionTypeBool;
    o->value.b    = UNREDIRECT_FS_DEFAULT;

    o = &screen->opt[COMP_SCREEN_OPTION_DEFAULT_ICON];
    o->name	      = "default_icon";
    o->shortDesc      = "Default Icon";
    o->longDesc	      = "Default window icon image";
    o->type	      = CompOptionTypeString;
    o->value.s	      = strdup (DEFAULT_ICON_DEFAULT);
    o->rest.s.string  = 0;
    o->rest.s.nString = 0;
}

static Bool
initPluginForScreen (CompPlugin *p,
		     CompScreen *s)
{
    if (p->vTable->initScreen)
	return (*p->vTable->initScreen) (p, s);

    return FALSE;
}

static void
finiPluginForScreen (CompPlugin *p,
		     CompScreen *s)
{
    if (p->vTable->finiScreen)
	(*p->vTable->finiScreen) (p, s);
}

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
	{ 0,  0,   0,  1,   0,  1,   1, -2 }, /* left */
	{ 1, -1,   0,  1,   0,  1,   1, -2 }, /* right */
	{ 0,  1,   0,  0,   1, -2,   0,  1 }, /* top */
	{ 0,  1,   1, -1,   1, -2,   0,  1 }, /* bottom */
	{ 0,  0,   0,  0,   0,  1,   0,  1 }, /* top-left */
	{ 1, -1,   0,  0,   0,  1,   0,  1 }, /* top-right */
	{ 0,  0,   1, -1,   0,  1,   0,  1 }, /* bottom-left */
	{ 1, -1,   1, -1,   0,  1,   0,  1 }  /* bottom-right */
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
frustum (GLfloat left,
	 GLfloat right,
	 GLfloat bottom,
	 GLfloat top,
	 GLfloat nearval,
	 GLfloat farval)
{
   GLfloat x, y, a, b, c, d;
   GLfloat m[16];

   x = (2.0 * nearval) / (right - left);
   y = (2.0 * nearval) / (top - bottom);
   a = (right + left) / (right - left);
   b = (top + bottom) / (top - bottom);
   c = -(farval + nearval) / ( farval - nearval);
   d = -(2.0 * farval * nearval) / (farval - nearval);

#define M(row,col)  m[col*4+row]
   M(0,0) = x;     M(0,1) = 0.0F;  M(0,2) = a;      M(0,3) = 0.0F;
   M(1,0) = 0.0F;  M(1,1) = y;     M(1,2) = b;      M(1,3) = 0.0F;
   M(2,0) = 0.0F;  M(2,1) = 0.0F;  M(2,2) = c;      M(2,3) = d;
   M(3,0) = 0.0F;  M(3,1) = 0.0F;  M(3,2) = -1.0F;  M(3,3) = 0.0F;
#undef M

   glMultMatrixf (m);
}

static void
perspective (GLfloat fovy,
	     GLfloat aspect,
	     GLfloat zNear,
	     GLfloat zFar)
{
   GLfloat xmin, xmax, ymin, ymax;

   ymax = zNear * tan (fovy * M_PI / 360.0);
   ymin = -ymax;
   xmin = ymin * aspect;
   xmax = ymax * aspect;

   frustum (xmin, xmax, ymin, ymax, zNear, zFar);
}

static void
reshape (CompScreen *s,
	 int	    w,
	 int	    h)
{
    s->width  = w;
    s->height = h;

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();
    glDepthRange (0, 1);
    glViewport (-1, -1, 2, 2);
    glRasterPos2f (0, 0);

    s->rasterX = s->rasterY = 0;

    glViewport (0, 0, w, h);
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    perspective (60.0f, 1.0f, 0.1f, 100.0f);
    glMatrixMode (GL_MODELVIEW);

    s->region.rects = &s->region.extents;
    s->region.numRects = 1;
    s->region.extents.x1 = 0;
    s->region.extents.y1 = 0;
    s->region.extents.x2 = w;
    s->region.extents.y2 = h;
    s->region.size = 1;

    updateScreenEdges (s);
    updateWorkareaForScreen (s);
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
	    fprintf (stderr, "%s: Couldn't bind background pixmap 0x%x to "
		     "texture\n", programName, (int) pixmap);
	}
    }
    else
    {
	finiTexture (screen, texture);
	initTexture (screen, texture);
    }

    if (!texture->name)
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
    if (s->opt[COMP_SCREEN_OPTION_DETECT_REFRESH_RATE].value.b)
    {
	XRRScreenConfiguration *config;
	char		       *name;
	CompOptionValue	       value;

	config  = XRRGetScreenInfo (s->display->display, s->root);
	value.i = (int) XRRConfigCurrentRate (config);

	XRRFreeScreenConfigInfo (config);

	name = s->opt[COMP_SCREEN_OPTION_REFRESH_RATE].name;

	s->opt[COMP_SCREEN_OPTION_DETECT_REFRESH_RATE].value.b = FALSE;
	(*s->setScreenOption) (s, name, &value);
	s->opt[COMP_SCREEN_OPTION_DETECT_REFRESH_RATE].value.b = TRUE;
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
setDesktopHints (CompScreen *s)
{
    CompDisplay   *d = s->display;
    unsigned long data[2];
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *propData;

    result = XGetWindowProperty (s->display->display, s->root,
				 d->desktopViewportAtom, 0L, 2L, FALSE,
				 XA_CARDINAL, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && n && propData)
    {
	if (n == 2)
	{
	    memcpy (data, propData, sizeof (unsigned long));

	    if (data[0] / s->width < s->size - 1)
		s->x = data[0] / s->width;
	}

	XFree (propData);
    }

    data[0] = s->x * s->width;
    data[1] = 0;

    XChangeProperty (d->display, s->root, d->desktopViewportAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 2);

    data[0] = s->width * s->size;
    data[1] = s->height;

    XChangeProperty (d->display, s->root, d->desktopGeometryAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 2);

    data[0] = 1;

    XChangeProperty (d->display, s->root, d->numberOfDesktopsAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 1);

    data[0] = 0;

    XChangeProperty (d->display, s->root, d->currentDesktopAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 1);

    data[0] = 0;

    result = XGetWindowProperty (s->display->display, s->root,
				 d->showingDesktopAtom, 0L, 1L, FALSE,
				 XA_CARDINAL, &actual, &format,
				 &n, &left, &propData);

    if (result == Success && n && propData)
    {
	memcpy (data, propData, sizeof (unsigned long));
	XFree (propData);

	if (data[0])
	    enterShowDesktopMode (s);
    }

    XChangeProperty (d->display, s->root, d->showingDesktopAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) data, 1);
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
    GLint	         stencilBits;
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

    compScreenInitOptions (s);

    s->damage = XCreateRegion ();
    if (!s->damage)
	return FALSE;

    s->x    = 0;
    s->size = 4;

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

    s->escapeKeyCode = XKeysymToKeycode (display->display,
					 XStringToKeysym ("Escape"));

    s->damageMask  = COMP_SCREEN_DAMAGE_ALL_MASK;
    s->next	   = 0;
    s->exposeRects = 0;
    s->sizeExpose  = 0;
    s->nExpose     = 0;

    s->rasterX = 0;
    s->rasterY = 0;

    s->windows = 0;
    s->reverseWindows = 0;

    s->stencilRef = 0x1;

    s->opacityStep = OPACITY_STEP_DEFAULT;

    s->nextRedraw  = 0;
    s->frameStatus = 0;

    s->showingDesktopMask = 0;

    s->overlayWindowCount = 0;

    gettimeofday (&s->lastRedraw, 0);

    s->setScreenOption	        = setScreenOption;
    s->setScreenOptionForPlugin = setScreenOptionForPlugin;

    s->initPluginForScreen = initPluginForScreen;
    s->finiPluginForScreen = finiPluginForScreen;

    s->preparePaintScreen     = preparePaintScreen;
    s->donePaintScreen        = donePaintScreen;
    s->paintScreen	      = paintScreen;
    s->paintTransformedScreen = paintTransformedScreen;
    s->paintBackground        = paintBackground;
    s->paintWindow            = paintWindow;
    s->addWindowGeometry      = addWindowGeometry;
    s->drawWindowTexture      = drawWindowTexture;
    s->drawWindowGeometry     = drawWindowGeometry;
    s->damageWindowRect       = damageWindowRect;
    s->focusWindow	      = focusWindow;
    s->setWindowScale	      = setWindowScale;

    s->windowResizeNotify = windowResizeNotify;
    s->windowMoveNotify	  = windowMoveNotify;
    s->windowGrabNotify   = windowGrabNotify;
    s->windowUngrabNotify = windowUngrabNotify;

    s->getProcAddress = 0;

    if (!XGetWindowAttributes (dpy, s->root, &s->attrib))
	return FALSE;

    s->workArea.x      = 0;
    s->workArea.y      = 0;
    s->workArea.width  = s->attrib.width;
    s->workArea.height = s->attrib.height;

    s->grabWindow = None;

    templ.visualid = XVisualIDFromVisual (s->attrib.visual);

    visinfo = XGetVisualInfo (dpy, VisualIDMask, &templ, &nvisinfo);
    if (!nvisinfo)
    {
	fprintf (stderr, "%s: Couldn't get visual info for default visual\n",
		 programName);
	return FALSE;
    }

    defaultDepth = visinfo->depth;

    black.red = black.green = black.blue = 0;

    if (!XAllocColor (dpy, s->colormap, &black))
    {
	fprintf (stderr, "%s: Couldn't allocate color\n", programName);
	return FALSE;
    }

    bitmap = XCreateBitmapFromData (dpy, s->root, &data, 1, 1);
    if (!bitmap)
    {
	fprintf (stderr, "%s: Couldn't create bitmap\n", programName);
	return FALSE;
    }

    s->invisibleCursor = XCreatePixmapCursor (dpy, bitmap, bitmap,
					      &black, &black, 0, 0);
    if (!s->invisibleCursor)
    {
	fprintf (stderr, "%s: Couldn't create invisible cursor\n",
		 programName);
	return FALSE;
    }

    XFreePixmap (dpy, bitmap);
    XFreeColors (dpy, s->colormap, &black.pixel, 1, 0);

    glXGetConfig (dpy, visinfo, GLX_USE_GL, &value);
    if (!value)
    {
	fprintf (stderr, "%s: Root visual is not a GL visual\n",
		 programName);
	return FALSE;
    }

    glXGetConfig (dpy, visinfo, GLX_DOUBLEBUFFER, &value);
    if (!value)
    {
	fprintf (stderr,
		 "%s: Root visual is not a double buffered GL visual\n",
		 programName);
	return FALSE;
    }

    s->ctx = glXCreateContext (dpy, visinfo, NULL, !indirectRendering);
    if (!s->ctx)
    {
	fprintf (stderr, "%s: glXCreateContext failed\n", programName);
	return FALSE;
    }

    XFree (visinfo);

    glxExtensions = glXQueryExtensionsString (s->display->display, screenNum);
    if (!strstr (glxExtensions, "GLX_EXT_texture_from_pixmap"))
    {
	fprintf (stderr, "%s: GLX_EXT_texture_from_pixmap is missing\n",
		 programName);
	return FALSE;
    }

    if (!strstr (glxExtensions, "GLX_SGIX_fbconfig"))
    {
	fprintf (stderr, "%s: GLX_SGIX_fbconfig is missing\n",
		 programName);
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
	fprintf (stderr, "%s: glXBindTexImageEXT is missing\n", programName);
	return FALSE;
    }

    if (!s->releaseTexImage)
    {
	fprintf (stderr, "%s: glXReleaseTexImageEXT is missing\n",
		 programName);
	return FALSE;
    }

    if (!s->queryDrawable     ||
	!s->getFBConfigs      ||
	!s->getFBConfigAttrib ||
	!s->createPixmap)
    {
	fprintf (stderr, "%s: fbconfig functions missing\n", programName);
	return FALSE;
    }

    s->copySubBuffer = NULL;
    if (strstr (glxExtensions, "GLX_MESA_copy_sub_buffer"))
	s->copySubBuffer = (GLXCopySubBufferProc)
	    getProcAddress (s, "glXCopySubBufferMESA");

    glXMakeCurrent (dpy, s->root, s->ctx);
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
	    if (maxTextureSize < s->maxTextureSize)
		s->maxTextureSize = maxTextureSize;
	}
    }

    if (!(s->textureRectangle || s->textureNonPowerOfTwo))
    {
	fprintf (stderr, "%s: Support for non power of two textures missing\n",
		 programName);
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
	s->programLocalParameter4f = (GLProgramLocalParameter4fProc)
	    getProcAddress (s, "glProgramLocalParameter4fARB");

	if (s->genPrograms    &&
	    s->deletePrograms &&
	    s->bindProgram    &&
	    s->programString  &&
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

	s->glxPixmapFBConfigs[i].fbConfig      = NULL;
	s->glxPixmapFBConfigs[i].mipmap        = 0;
	s->glxPixmapFBConfigs[i].yInverted     = 0;
	s->glxPixmapFBConfigs[i].textureFormat = 0;

	db      = MAXSHORT;
	stencil = MAXSHORT;
	depth   = MAXSHORT;
	mipmap  = 0;
	rgba    = 0;

	for (j = 0; j < nElements; j++)
	{
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
	    s->glxPixmapFBConfigs[i].fbConfig  = fbConfigs[j];
	    s->glxPixmapFBConfigs[i].mipmap    = mipmap;
	}
    }

    if (nElements)
	XFree (fbConfigs);

    if (!s->glxPixmapFBConfigs[defaultDepth].fbConfig)
    {
	fprintf (stderr, "%s: No GLXFBConfig for default depth, "
		 "this isn't going to work.\n",
		 programName);
	return FALSE;
    }

    initTexture (s, &s->backgroundTexture);

    s->defaultIcon = NULL;
    if (!updateDefaultIcon (s))
    {
	fprintf (stderr, "%s: Couldn't load default window icon.\n",
		 programName);
	return FALSE;
    }

    s->desktopWindowCount = 0;

    glGetIntegerv (GL_STENCIL_BITS, &stencilBits);
    if (!stencilBits)
    {
	fprintf (stderr, "%s: No stencil buffer. Clipping of transformed "
		 "windows is not going to be correct when screen is "
		 "transformed.\n", programName);
    }

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

    screenInitPlugins (s);

    detectRefreshRateOfScreen (s);

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
	    w->placed    = TRUE;
	    w->invisible = WINDOW_INVISIBLE (w);
	}
	else if (w->state & CompWindowStateHiddenMask)
	{
	    w->placed = TRUE;
	}
    }

    XFree (children);

    attrib.override_redirect = 1;
    s->grabWindow = XCreateWindow (dpy, s->root, -100, -100, 1, 1, 0,
				   CopyFromParent, InputOnly,
				   CopyFromParent, CWOverrideRedirect,
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
			    Window      id)
{
    CompWindow *found = NULL;

    if (lastFoundWindow && lastFoundWindow->id == id)
    {
	found = lastFoundWindow;
    }
    else
    {
	CompWindow *w;

	for (w = s->windows; w; w = w->next)
	{
	    if (w->id == id)
	    {
		found = (lastFoundWindow = w);
		break;
	    }
	}
    }

    if (!found)
	return NULL;

    if (found->attrib.override_redirect)
    {
	/* likely a frame window */
	if (found->attrib.class == InputOnly)
	{
	    CompWindow *w;

	    for (w = s->windows; w; w = w->next)
		if (w->frame == id)
		    return w;
	}

	return NULL;
    }

    return found;
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
		warpPointer (s->display,
			     restorePointer->x - pointerX,
			     restorePointer->y - pointerY);

	    XUngrabPointer (s->display->display, CurrentTime);
	    XUngrabKeyboard (s->display->display, CurrentTime);
	}

	s->maxGrab = maxGrab;
    }
}

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
    } else {
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
addScreenBinding (CompScreen  *s,
		  CompBinding *binding)
{
    if (binding->type == CompBindingTypeNone)
	return TRUE;
    else if (binding->type == CompBindingTypeKey)
	return addPassiveKeyGrab (s, &binding->u.key);
    else if (binding->type == CompBindingTypeButton)
	return addPassiveButtonGrab (s, &binding->u.button);

    return FALSE;
}

void
removeScreenBinding (CompScreen  *s,
		     CompBinding *binding)
{
    if (binding->type == CompBindingTypeKey)
	removePassiveKeyGrab (s, &binding->u.key);
    else if (binding->type == CompBindingTypeButton)
	removePassiveButtonGrab (s, &binding->u.button);
}

void
updatePassiveGrabs (CompScreen *s)
{
    updatePassiveKeyGrabs (s);
}

void
updateWorkareaForScreen (CompScreen *s)
{
    CompWindow *w;
    int	       leftStrut, rightStrut, topStrut, bottomStrut;
    XRectangle workArea;

    leftStrut   = 0;
    rightStrut  = 0;
    topStrut    = 0;
    bottomStrut = 0;

    for (w = s->windows; w; w = w->next)
    {
	if (w->attrib.map_state == IsUnmapped)
	    continue;

	if (w->struts)
	{
	    if (w->struts->left.width > leftStrut)
		leftStrut = w->struts->left.width;

	    if (w->struts->right.width > rightStrut)
		rightStrut = w->struts->right.width;

	    if (w->struts->top.height > topStrut)
		topStrut = w->struts->top.height;

	    if (w->struts->bottom.height > bottomStrut)
		bottomStrut = w->struts->bottom.height;
	}
    }

#define MIN_SANE_AREA 100

    if ((leftStrut + rightStrut) > (s->width - MIN_SANE_AREA))
    {
	leftStrut  = (s->width - MIN_SANE_AREA) / 2;
	rightStrut = leftStrut;
    }

    if ((topStrut + bottomStrut) > (s->height - MIN_SANE_AREA))
    {
	topStrut    = (s->height - MIN_SANE_AREA) / 2;
	bottomStrut = topStrut;
    }

    workArea.x      = leftStrut;
    workArea.y      = topStrut;
    workArea.width  = s->width  - leftStrut - rightStrut;
    workArea.height = s->height - topStrut  - bottomStrut;

    if (memcmp (&workArea, &s->workArea, sizeof (XRectangle)))
    {
	unsigned long data[4];

	data[0] = workArea.x;
	data[1] = workArea.y;
	data[2] = workArea.width;
	data[3] = workArea.height;

	XChangeProperty (s->display->display, s->root,
			 s->display->workareaAtom, XA_CARDINAL, 32,
			 PropModeReplace, (unsigned char *) data, 4);

	s->workArea = workArea;
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
		    Bool       sync)
{
    tx = s->x - tx;
    tx = MOD (tx, s->size);
    tx -= s->x;

    if (tx)
    {
	CompWindow *w;
	int	   m, wx, vWidth = s->width * s->size;

	s->x += tx;

	tx *= -s->width;

	for (w = s->windows; w; w = w->next)
	{
	    if (w->attrib.override_redirect)
		continue;

	    if (w->type & (CompWindowTypeDesktopMask | CompWindowTypeDockMask))
		continue;

	    if (w->state & CompWindowStateStickyMask)
		continue;

	    m = w->attrib.x + tx;
	    if (m - w->output.left < s->width - vWidth)
		wx = tx + vWidth;
	    else if (m + w->width + w->output.right > vWidth)
		wx = tx - vWidth;
	    else
		wx = tx;

	    if (w->saveMask & CWX)
		w->saveWc.x += wx;

	    moveWindow (w, wx, 0, sync, TRUE);

	    if (sync)
		syncWindowPosition (w);
	}

	if (sync)
	{
	    unsigned long data[2];

	    data[0] = s->x * s->width;
	    data[1] = 0;

	    XChangeProperty (s->display->display, s->root,
			     s->display->desktopViewportAtom,
			     XA_CARDINAL, 32, PropModeReplace,
			     (unsigned char *) data, 2);
	}
    }
}

void
moveWindowToViewportPosition (CompWindow *w,
			      int	 x,
			      Bool       sync)
{
    int	tx, vWidth = w->screen->width * w->screen->size;

    x += w->screen->x * w->screen->width;
    x = MOD (x, vWidth);
    x -= w->screen->x * w->screen->width;

    tx = x - w->attrib.x;
    if (tx)
    {
	int m, wx;

	if (w->attrib.map_state != IsViewable)
	    return;

	if (w->attrib.override_redirect)
	    return;

	if (w->type & (CompWindowTypeDesktopMask | CompWindowTypeDockMask))
	    return;

	if (w->state & CompWindowStateStickyMask)
	    return;

	m = w->attrib.x + tx;
	if (m - w->output.left < w->screen->width - vWidth)
	    wx = tx + vWidth;
	else if (m + w->width + w->output.right > vWidth)
	    wx = tx - vWidth;
	else
	    wx = tx;

	if (w->saveMask & CWX)
	    w->saveWc.x += wx;

	moveWindow (w, wx, 0, sync, TRUE);

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
    CompStartupSequence *s = NULL;
    const char	        *startupId = window->startupId;

    printf ("Applying startup props to %d id \"%s\"\n",
	    (int) window->id, window->startupId ? window->startupId : "(none)");

    if (!startupId)
    {
	const char *wmClass;

	for (s = screen->startupSequences; s; s = s->next)
	{
	    wmClass = sn_startup_sequence_get_wmclass (s->sequence);
	    if (!wmClass)
		continue;

	    if ((window->resClass && strcmp (wmClass, window->resClass) == 0) ||
		(window->resName  && strcmp (wmClass, window->resName)  == 0))
	    {
		startupId = sn_startup_sequence_get_id (s->sequence);

		window->startupId = strdup (startupId);

		printf ("Ending legacy sequence %s due to window %d\n",
			startupId, (int) window->id);

		sn_startup_sequence_complete (s->sequence);
		break;
	    }
	}

	if (!startupId)
	    return;
    }

    if (!s)
    {
	const char *id;

	for (s = screen->startupSequences; s; s = s->next)
	{
	    id = sn_startup_sequence_get_id (s->sequence);

	    if (strcmp (id, startupId) == 0)
		break;
	}
    }

    if (s)
    {
	printf ("Found startup sequence for window %d ID \"%s\"\n",
		(int) window->id, startupId);
    }
    else
    {
	printf ("Did not find startup sequence for window %d ID \"%s\"\n",
		(int) window->id, startupId);
    }
}

void
enterShowDesktopMode (CompScreen *s)
{
    CompWindow    *w;
    unsigned long data = 1;
    int		  count = 0;

    s->showingDesktopMask = ~(CompWindowTypeDesktopMask |
			      CompWindowTypeDockMask);

    for (w = s->windows; w; w = w->next)
    {
	if (s->showingDesktopMask & w->type)
	{
	    if ((*s->focusWindow) (w))
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

void
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
	    w->inShowDesktopMode = FALSE;

	    showWindow (w);
	}
    }

    XChangeProperty (s->display->display, s->root,
		     s->display->showingDesktopAtom,
		     XA_CARDINAL, 32, PropModeReplace,
		     (unsigned char *) &data, 1);
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
