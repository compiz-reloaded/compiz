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
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include <compiz.h>

char *programName;
char **programArgv;
int  programArgc;

char *backgroundImage = "background.png";
char *windowImage = "window.png";

REGION   emptyRegion;
REGION   infiniteRegion;
GLushort defaultColor[4] = { 0xffff, 0xffff, 0xffff, 0xffff };
Window   currentRoot = 0;

int  defaultRefreshRate = 50;
char *defaultTextureFilter = "Good";

char *windowTypeString[] = {
    "Desktop",
    "Dock",
    "Toolbar",
    "Menu",
    "Utility",
    "Splash",
    "Dialog",
    "ModalDialog",
    "Normal",
    "Fullscreen",
    "Unknown"
};
int  nWindowTypeString =
    sizeof (windowTypeString) / sizeof (windowTypeString[0]);

Bool testMode = FALSE;
Bool restartSignal = FALSE;

CompWindow *lastFoundWindow = 0;
CompWindow *lastDamagedWindow = 0;

Bool replaceCurrentWm = FALSE;

static void
usage (void)
{
    printf ("Usage: %s "
	    "[--display DISPLAY] "
	    "[--bg-image PNG] "
	    "[--window-image PNG]\n       "
	    "[--refresh-rate RATE] "
	    "[--fast-filter] "
	    "[--test-mode] "
	    "[--replace]\n       "
	    "[--sm-disable] "
	    "[--sm-save-file] "
	    "[--sm-client-id] "
	    "[--help] "
	    "[PLUGIN]...\n",
	    programName);
}

static void
signalHandler (int sig)
{
    int status;

    switch (sig) {
    case SIGCHLD:
	waitpid (-1, &status, WNOHANG | WUNTRACED);
	break;
    case SIGHUP:
	restartSignal = TRUE;
    default:
	break;
    }
}

int
main (int argc, char **argv)
{
    char *displayName = 0;
    char *plugin[256];
    int  i, nPlugin = 0;
    Bool disableSm = TRUE;
    char *clientId = NULL;
    char *saveFile = NULL;

    programName = argv[0];
    programArgc = argc;
    programArgv = argv;

    signal (SIGHUP, signalHandler);
    signal (SIGCHLD, signalHandler);

    emptyRegion.rects = &emptyRegion.extents;
    emptyRegion.numRects = 0;
    emptyRegion.extents.x1 = 0;
    emptyRegion.extents.y1 = 0;
    emptyRegion.extents.x2 = 0;
    emptyRegion.extents.y2 = 0;
    emptyRegion.size = 0;

    infiniteRegion.rects = &infiniteRegion.extents;
    infiniteRegion.numRects = 1;
    infiniteRegion.extents.x1 = MINSHORT;
    infiniteRegion.extents.y1 = MINSHORT;
    infiniteRegion.extents.x2 = MAXSHORT;
    infiniteRegion.extents.y2 = MAXSHORT;

    for (i = 1; i < argc; i++)
    {
	if (!strcmp (argv[i], "--help"))
	{
	    usage ();
	    return 0;
	}
	else if (!strcmp (argv[i], "--display"))
	{
	    if (i + 1 < argc)
		displayName = argv[++i];
	}
	else if (!strcmp (argv[i], "--refresh-rate"))
	{
	    if (i + 1 < argc)
	    {
		defaultRefreshRate = atoi (programArgv[++i]);
		defaultRefreshRate = RESTRICT_VALUE (defaultRefreshRate,
						     1, 1000);
	    }
	}
	else if (!strcmp (argv[i], "--fast-filter"))
	{
	    defaultTextureFilter = "Fast";
	}
	else if (!strcmp (argv[i], "--test-mode"))
	{
	    testMode = TRUE;
	}
	else if (!strcmp (argv[i], "--replace"))
	{
	    replaceCurrentWm = TRUE;
	}
	else if (!strcmp (argv[i], "--sm-disable"))
	{
	    disableSm = TRUE;
	}
	else if (!strcmp (argv[i], "--sm-client-id"))
	{
	    if (i + 1 < argc)
		clientId = argv[++i];
	}
	else if (!strcmp (argv[i], "--sm-save-file"))
	{
	    if (i + 1 < argc)
		saveFile = argv[++i];
	}
	else if (!strcmp (argv[i], "--bg-image"))
	{
	    if (i + 1 < argc)
		backgroundImage = argv[++i];
	}
	else if (!strcmp (argv[i], "--window-image"))
	{
	    if (i + 1 < argc)
		windowImage = argv[++i];
	}
	else
	{
	    if (nPlugin < 256)
		plugin[nPlugin++] = argv[i];
	}
    }

    if (!addDisplay (displayName, plugin, nPlugin))
	return 1;

    eventLoop ();

    return 0;
}
