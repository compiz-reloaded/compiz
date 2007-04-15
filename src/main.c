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
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include <compiz.h>

char *programName;
char **programArgv;
int  programArgc;

char *backgroundImage = NULL;

REGION   emptyRegion;
REGION   infiniteRegion;
GLushort defaultColor[4] = { 0xffff, 0xffff, 0xffff, 0xffff };
Window   currentRoot = 0;

int  defaultRefreshRate = 50;
char *defaultTextureFilter = "Good";

char *windowTypeString[] = {
    N_("Desktop"),
    N_("Dock"),
    N_("Toolbar"),
    N_("Menu"),
    N_("Utility"),
    N_("Splash"),
    N_("Dialog"),
    N_("Normal"),
    N_("DropdownMenu"),
    N_("PopupMenu"),
    N_("Tooltip"),
    N_("Notification"),
    N_("Combo"),
    N_("Dnd"),
    N_("ModalDialog"),
    N_("Fullscreen"),
    N_("Unknown")
};
int  nWindowTypeString =
    sizeof (windowTypeString) / sizeof (windowTypeString[0]);

Bool shutDown = FALSE;
Bool restartSignal = FALSE;

CompWindow *lastFoundWindow = 0;
CompWindow *lastDamagedWindow = 0;

Bool replaceCurrentWm = FALSE;
Bool indirectRendering = FALSE;
Bool strictBinding = TRUE;
Bool noDetection = FALSE;
Bool useDesktopHints = TRUE;
Bool onlyCurrentScreen = FALSE;

#ifdef USE_COW
Bool useCow = TRUE;
#endif

CompMetadata coreMetadata;

static void
usage (void)
{
    printf ("Usage: %s "
	    "[--display DISPLAY] "
	    "[--bg-image PNG] "
	    "[--refresh-rate RATE]\n       "
	    "[--fast-filter] "
	    "[--indirect-rendering] "
	    "[--loose-binding] "
	    "[--replace]\n       "
	    "[--sm-disable] "
	    "[--sm-client-id ID] "
	    "[--no-detection]\n       "
	    "[--ignore-desktop-hints] "
	    "[--only-current-screen]"

#ifdef USE_COW
	    " [--use-root-window]\n       "
#else
	    "\n       "
#endif

	    "[--version] "
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
	break;
    case SIGINT:
    case SIGTERM:
	shutDown = TRUE;
    default:
	break;
    }
}

static char *
strAdd (char	   *dst,
	const char *src)
{
    int  newSize, oldSize = 0;
    char *s;

    if (dst)
	oldSize = strlen (dst);

    newSize = oldSize + strlen (src) + 1;

    s = realloc (dst, sizeof (char) * newSize);
    if (!s)
    {
	fprintf (stderr, "%s: memory allocation failure\n", programName);
	exit (1);
    }

    strcpy (s + oldSize, src);

    return s;
}

static char *
strAddOption (char			   *dst,
	      const CompMetadataOptionInfo *info)
{
    char *xml;

    xml = compMetadataOptionInfoToXml (info);
    if (!xml)
    {
	fprintf (stderr, "%s: memory allocation failure 2\n", programName);
	exit (1);
    }

    return strAdd (dst, xml);
}

int
main (int argc, char **argv)
{
    char *displayName = 0;
    char *plugin[256];
    int  i, j, nPlugin = 0;
    Bool disableSm = FALSE;
    char *clientId = NULL;
    char *str;
    char *textureFilterArg = NULL;
    char *refreshRateArg = NULL;

    programName = argv[0];
    programArgc = argc;
    programArgv = argv;

    signal (SIGHUP, signalHandler);
    signal (SIGCHLD, signalHandler);
    signal (SIGINT, signalHandler);
    signal (SIGTERM, signalHandler);

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
	else if (!strcmp (argv[i], "--version"))
	{
	    printf (PACKAGE_STRING "\n");
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
		refreshRateArg = programArgv[++i];
		defaultRefreshRate = atoi (refreshRateArg);
		defaultRefreshRate = RESTRICT_VALUE (defaultRefreshRate,
						     1, 1000);
	    }
	}
	else if (!strcmp (argv[i], "--fast-filter"))
	{
	    textureFilterArg = "Fast";
	    defaultTextureFilter = textureFilterArg;
	}
	else if (!strcmp (argv[i], "--indirect-rendering"))
	{
	    indirectRendering = TRUE;
	}
	else if (!strcmp (argv[i], "--loose-binding"))
	{
	    strictBinding = FALSE;
	}
	else if (!strcmp (argv[i], "--ignore-desktop-hints"))
	{
	    useDesktopHints = FALSE;
	}
	else if (!strcmp (argv[i], "--only-current-screen"))
	{
	    onlyCurrentScreen = TRUE;
	}

#ifdef USE_COW
	else if (!strcmp (argv[i], "--use-root-window"))
	{
	    useCow = FALSE;
	}
#endif

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
	else if (!strcmp (argv[i], "--no-detection"))
	{
	    noDetection = TRUE;
	}
	else if (!strcmp (argv[i], "--bg-image"))
	{
	    if (i + 1 < argc)
		backgroundImage = argv[++i];
	}
	else if (*argv[i] == '-')
	{
	    fprintf (stderr, "%s: Unknown option '%s'\n", programName, argv[i]);
	}
	else
	{
	    if (nPlugin < 256)
		plugin[nPlugin++] = argv[i];
	}
    }

    xmlInitParser ();

    LIBXML_TEST_VERSION;

    if (!compInitMetadata (&coreMetadata))
    {
	fprintf (stderr, "%s: Couldn't initialize core metadata\n",
		 programName);
	return 1;
    }

    str = strAdd (NULL, "<compiz><core><display>");

    for (i = 0; i < COMP_DISPLAY_OPTION_NUM; i++)
    {
	CompMetadataOptionInfo info = coreDisplayOptionInfo[i];
	char		       *tmp = NULL;

	switch (i) {
	case COMP_DISPLAY_OPTION_ACTIVE_PLUGINS:
	    if (nPlugin)
	    {
		tmp = strAdd (tmp, "<type>string</type><default>");

		for (j = 0; j < nPlugin; j++)
		{
		    tmp = strAdd (tmp, "<value>");
		    tmp = strAdd (tmp, plugin[j]);
		    tmp = strAdd (tmp, "</value>");
		}

		tmp = strAdd (tmp, "</default>");

		info.data = tmp;
	    }
	    break;
	case COMP_DISPLAY_OPTION_TEXTURE_FILTER:
	    if (textureFilterArg)
	    {
		tmp = strAdd (tmp, "<type>string</type><default>");
		tmp = strAdd (tmp, textureFilterArg);
		tmp = strAdd (tmp, "</default>");

		info.data = tmp;
	    }
	default:
	    break;
	}

	str = strAddOption (str, &info);

	if (tmp)
	    free (tmp);
    }

    str = strAdd (str, "</display><screen>");

    for (i = 0; i < COMP_SCREEN_OPTION_NUM; i++)
    {
	CompMetadataOptionInfo info = coreScreenOptionInfo[i];
	char		       tmp[256];

	switch (i) {
	case COMP_SCREEN_OPTION_REFRESH_RATE:
	    if (refreshRateArg)
	    {
		snprintf (tmp, 256, "<min>1</min><default>%s</default>",
			  refreshRateArg);
		info.data = tmp;
	    }
	default:
	    break;
	}

	str = strAddOption (str, &info);
    }

    str = strAdd (str, "</screen></core></compiz>");

    if (!compAddMetadataFromString (&coreMetadata, str))
	return 1;

    free (str);

    compAddMetadataFromFile (&coreMetadata, "compiz");

    if (!disableSm)
	initSession (clientId);

    if (!addDisplay (displayName))
	return 1;

    eventLoop ();

    if (!disableSm)
	closeSession ();

    xmlCleanupParser ();

    if (restartSignal)
    {
	execvp (programName, programArgv);
	return 1;
    }

    return 0;
}
