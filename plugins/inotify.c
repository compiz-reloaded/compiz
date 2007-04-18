/*
 * Copyright © 2006 Novell, Inc.
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

#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/inotify.h>

#include <compiz.h>

static int displayPrivateIndex;

typedef struct _CompInotifyWatch {
    struct _CompInotifyWatch *next;
    CompFileWatchHandle	     handle;
    int			     wd;
} CompInotifyWatch;

typedef struct _InotifyDisplay {
    int		      fd;
    CompInotifyWatch  *watch;
    CompWatchFdHandle watchFdHandle;

    FileWatchAddedProc   fileWatchAdded;
    FileWatchRemovedProc fileWatchRemoved;
} InotifyDisplay;

#define GET_INOTIFY_DISPLAY(d)				        \
    ((InotifyDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define INOTIFY_DISPLAY(d)		         \
    InotifyDisplay *id = GET_INOTIFY_DISPLAY (d)


static Bool
inotifyProcessEvents (void *data)
{
    char	buf[256 * (sizeof (struct inotify_event) + 16)];
    CompDisplay	*d = (CompDisplay *) data;
    int		len;

    INOTIFY_DISPLAY (d);

    len = read (id->fd, buf, sizeof (buf));
    if (len < 0)
    {
	perror ("read");
    }
    else
    {
	struct inotify_event *event;
	CompInotifyWatch     *iw;
	CompFileWatch	     *fw;
	int		     i = 0;

	while (i < len)
	{
	    event = (struct inotify_event *) &buf[i];

	    for (iw = id->watch; iw; iw = iw->next)
		if (iw->handle == event->wd)
		    break;

	    if (iw)
	    {
		for (fw = d->fileWatch; fw; fw = fw->next)
		    if (fw->handle == iw->handle)
			break;

		if (fw)
		    (*fw->callBack) (event->name, fw->closure);
	    }

	    i += sizeof (*event) + event->len;
	}
    }

    return TRUE;
}

static int
inotifyMask (CompFileWatch *fileWatch)
{
    int mask = 0;

    if (fileWatch->mask & NOTIFY_CREATE_MASK)
	mask |= IN_CREATE;

    if (fileWatch->mask & NOTIFY_DELETE_MASK)
	mask |= IN_DELETE;

    if (fileWatch->mask & NOTIFY_MOVE_MASK)
	mask |= IN_MOVE;

    if (fileWatch->mask & NOTIFY_MODIFY_MASK)
	mask |= IN_MODIFY;

    return mask;
}

static void
inotifyFileWatchAdded (CompDisplay   *d,
		       CompFileWatch *fileWatch)
{
    CompInotifyWatch *iw;

    INOTIFY_DISPLAY (d);

    iw = malloc (sizeof (CompInotifyWatch));
    if (!iw)
	return;

    iw->handle = fileWatch->handle;
    iw->wd     = inotify_add_watch (id->fd,
				    fileWatch->path,
				    inotifyMask (fileWatch));
    if (iw->wd < 0)
    {
	perror ("inotify_add_watch");
	free (iw);
	return;
    }

    iw->next  = id->watch;
    id->watch = iw;
}

static void
inotifyFileWatchRemoved (CompDisplay   *d,
			 CompFileWatch *fileWatch)
{
    CompInotifyWatch *p = 0, *iw;

    INOTIFY_DISPLAY (d);

    for (iw = id->watch; iw; iw = iw->next)
    {
	if (iw->handle == fileWatch->handle)
	    break;

	p = iw;
    }

    if (iw)
    {
	if (p)
	    p->next = iw->next;
	else
	    id->watch = iw->next;

	if (inotify_rm_watch (id->fd, iw->wd))
	    perror ("inotify_rm_watch");

	free (iw);
    }
}

static Bool
inotifyInitDisplay (CompPlugin  *p,
		    CompDisplay *d)
{
    InotifyDisplay *id;
    CompFileWatch  *fw;

    id = malloc (sizeof (InotifyDisplay));
    if (!id)
	return FALSE;

    id->fd = inotify_init ();
    if (id->fd < 0)
    {
	perror ("inotify_init");
	free (id);
	return FALSE;
    }

    id->watch = NULL;

    id->watchFdHandle = compAddWatchFd (id->fd,
					POLLIN | POLLPRI | POLLHUP | POLLERR,
					inotifyProcessEvents,
					d);

    WRAP (id, d, fileWatchAdded, inotifyFileWatchAdded);
    WRAP (id, d, fileWatchRemoved, inotifyFileWatchRemoved);

    d->privates[displayPrivateIndex].ptr = id;

    for (fw = d->fileWatch; fw; fw = fw->next)
	inotifyFileWatchAdded (d, fw);

    return TRUE;
}

static void
inotifyFiniDisplay (CompPlugin  *p,
		    CompDisplay *d)
{
    INOTIFY_DISPLAY (d);

    compRemoveWatchFd (id->watchFdHandle);

    close (id->fd);

    UNWRAP (id, d, fileWatchAdded);
    UNWRAP (id, d, fileWatchRemoved);

    free (id);
}

static Bool
inotifyInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
inotifyFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
inotifyGetVersion (CompPlugin *plugin,
		   int	      version)
{
    return ABIVERSION;
}

CompPluginVTable inotifyVTable = {
    "inotify",
    "Inotify",
    "File change notification plugin",
    inotifyGetVersion,
    0, /* GetMetadata */
    inotifyInit,
    inotifyFini,
    inotifyInitDisplay,
    inotifyFiniDisplay,
    0, /* InitScreen */
    0, /* FiniScreen */
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &inotifyVTable;
}
