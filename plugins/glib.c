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

#include <glib.h>
#include <compiz.h>

static CompMetadata glibMetadata;

static int displayPrivateIndex;

typedef struct _GLibWatch {
    CompWatchFdHandle handle;
    int		      index;
    CompDisplay	      *display;
} GLibWatch;

typedef struct _GConfDisplay {
    HandleEventProc   handleEvent;
    CompTimeoutHandle timeoutHandle;
    gint	      maxPriority;
    GPollFD	      *fds;
    gint	      fdsSize;
    gint	      nFds;
    GLibWatch	      *watch;
    Atom	      notifyAtom;
} GLibDisplay;

#define GET_GLIB_DISPLAY(d)				     \
    ((GLibDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define GLIB_DISPLAY(d)			   \
    GLibDisplay *gd = GET_GLIB_DISPLAY (d)

static void
glibDispatch (CompDisplay  *display,
	      GMainContext *context)
{
    int i;

    GLIB_DISPLAY (display);

    g_main_context_check (context, gd->maxPriority, gd->fds, gd->nFds);
    g_main_context_dispatch (context);

    for (i = 0; i < gd->nFds; i++)
	compRemoveWatchFd (gd->watch[i].handle);
}

static void
glibPrepare (CompDisplay  *display,
	     GMainContext *context);

static Bool
glibDispatchAndPrepare (void *closure)
{
    CompDisplay  *display = (CompDisplay *) closure;
    GMainContext *context = g_main_context_default ();

    glibDispatch (display, context);
    glibPrepare (display, context);

    return FALSE;
}

static void
glibWakeup (CompDisplay *display)
{
    GLIB_DISPLAY (display);

    if (gd->timeoutHandle)
    {
	compRemoveTimeout (gd->timeoutHandle);
	compAddTimeout (0, glibDispatchAndPrepare, (void *) display);

	gd->timeoutHandle = 0;
    }
}

static Bool
glibCollectEvents (void *closure)
{
    GLibWatch   *watch = (GLibWatch *) closure;
    CompDisplay *display = watch->display;

    GLIB_DISPLAY (display);

    gd->fds[watch->index].revents |= compWatchFdEvents (watch->handle);

    glibWakeup (display);

    return TRUE;
}

static void
glibPrepare (CompDisplay  *display,
	     GMainContext *context)
{
    int nFds = 0;
    int timeout = -1;
    int i;

    GLIB_DISPLAY (display);

    g_main_context_prepare (context, &gd->maxPriority);

    do
    {
	if (nFds > gd->fdsSize)
	{
	    if (gd->fds)
		free (gd->fds);

	    gd->fds = malloc ((sizeof (GPollFD) + sizeof (GLibWatch)) * nFds);
	    if (!gd->fds)
	    {
		nFds = 0;
		break;
	    }

	    gd->watch   = (GLibWatch *) (gd->fds + nFds);
	    gd->fdsSize = nFds;
	}

	nFds = g_main_context_query (context,
				     gd->maxPriority,
				     &timeout,
				     gd->fds,
				     gd->fdsSize);
    } while (nFds > gd->fdsSize);

    if (timeout < 0)
	timeout = INT_MAX;

    for (i = 0; i < nFds; i++)
    {
	gd->watch[i].display = display;
	gd->watch[i].index   = i;
	gd->watch[i].handle  = compAddWatchFd (gd->fds[i].fd,
					       gd->fds[i].events,
					       glibCollectEvents,
					       &gd->watch[i]);
    }

    gd->nFds	      = nFds;
    gd->timeoutHandle =
	compAddTimeout (timeout, glibDispatchAndPrepare, display);
}

static void
glibHandleEvent (CompDisplay *d,
		 XEvent      *event)
{
    GLIB_DISPLAY (d);

    if (event->type == ClientMessage)
    {
	if (event->xclient.message_type == gd->notifyAtom)
	    glibWakeup (d);
    }

    UNWRAP (gd, d, handleEvent);
    (*d->handleEvent) (d, event);
    WRAP (gd, d, handleEvent, glibHandleEvent);
}

static Bool
glibInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    GLibDisplay *gd;

    gd = malloc (sizeof (GLibDisplay));
    if (!gd)
	return FALSE;

    gd->fds	      = NULL;
    gd->fdsSize	      = 0;
    gd->timeoutHandle = 0;
    gd->notifyAtom    = XInternAtom (d->display, "_COMPIZ_GLIB_NOTIFY", 0);

    WRAP (gd, d, handleEvent, glibHandleEvent);

    d->privates[displayPrivateIndex].ptr = gd;

    glibPrepare (d, g_main_context_default ());

    return TRUE;
}

static void
glibFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    GLIB_DISPLAY (d);

    if (gd->timeoutHandle)
	compRemoveTimeout (gd->timeoutHandle);

    glibDispatch (d, g_main_context_default ());

    UNWRAP (gd, d, handleEvent);

    if (gd->fds)
	free (gd->fds);

    free (gd);
}

static Bool
glibInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&glibMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&glibMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&glibMetadata, p->vTable->name);

    return TRUE;
}

static void
glibFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&glibMetadata);
}

static int
glibGetVersion (CompPlugin *plugin,
		int	   version)
{
    return ABIVERSION;
}

static CompMetadata *
glibGetMetadata (CompPlugin *plugin)
{
    return &glibMetadata;
}

CompPluginVTable glibVTable = {
    "glib",
    glibGetVersion,
    glibGetMetadata,
    glibInit,
    glibFini,
    glibInitDisplay,
    glibFiniDisplay,
    0, /* InitScreen */
    0, /* FiniScreen */
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0  /* SetScreenOption */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &glibVTable;
}
