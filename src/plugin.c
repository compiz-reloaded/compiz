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
#include <dlfcn.h>

#include <compiz.h>

#define HOME_PLUGINDIR ".compiz/plugins"

CompPlugin *plugins = 0;

static Bool
initPlugin (CompPlugin *p)
{
    CompDisplay *d = compDisplays;
    int         failed = 0;

    if (!(*p->vTable->init) (p))
    {
	fprintf (stderr, "%s: InitPlugin '%s' failed\n", programName,
		 p->vTable->name);
	return FALSE;
    }

    if (d)
    {
	if ((*d->initPluginForDisplay) (p, d))
	{
	    CompScreen *s, *failedScreen = d->screens;

	    for (s = d->screens; s; s = s->next)
	    {
		if (!p->vTable->initScreen || (*s->initPluginForScreen) (p, s))
		{
		    CompWindow *w, *failedWindow = s->windows;

		    for (w = s->windows; w; w = w->next)
		    {
			if (p->vTable->initWindow &&
			    !(*p->vTable->initWindow) (p, w))
			{
			    fprintf (stderr, "%s: Plugin '%s':initWindow "
				     "failed\n", programName, p->vTable->name);
			    failedWindow = w;
			    failed = 1;
			    break;
			}
		    }

		    for (w = s->windows; w != failedWindow; w = w->next)
		    {
			if (p->vTable->finiWindow)
			    (*p->vTable->finiWindow) (p, w);
		    }
		}
		else
		{
		    fprintf (stderr, "%s: Plugin '%s':initScreen failed\n",
			     programName, p->vTable->name);
		    failedScreen = s;
		    failed = 1;
		    break;
		}
	    }

	    for (s = d->screens; s != failedScreen; s = s->next)
		(*s->finiPluginForScreen) (p, s);
	}
	else
	{
	    fprintf (stderr, "%s: Plugin '%s':initDisplay failed\n",
		     programName, p->vTable->name);

	    failed = 1;
	    (*d->finiPluginForDisplay) (p, d);
	}
    }

    if (failed)
    {
	(*p->vTable->fini) (p);

	return FALSE;
    }

    return TRUE;
}

static void
finiPlugin (CompPlugin *p)
{
    CompDisplay *d = compDisplays;
    CompScreen  *s;

    if (d)
    {
	for (s = d->screens; s; s = s->next)
	{
	    CompWindow *w = s->windows;

	    if (p->vTable->finiWindow)
	    {
		for (w = s->windows; w; w = w->next)
		    (*p->vTable->finiWindow) (p, w);
	    }

	    (*s->finiPluginForScreen) (p, s);
	}

	(*d->finiPluginForDisplay) (p, d);
    }

    (*p->vTable->fini) (p);
}

void
screenInitPlugins (CompScreen *s)
{
    CompPlugin *p;
    int	       i, j = 0;

    for (p = plugins; p; p = p->next)
	j++;

    while (j--)
    {
	i = 0;
	for (p = plugins; i < j; p = p->next)
	    i++;

	if (p->vTable->initScreen)
	    (*s->initPluginForScreen) (p, s);
    }
}

void
screenFiniPlugins (CompScreen *s)
{
    CompPlugin *p;

    for (p = plugins; p; p = p->next)
    {
	if (p->vTable->finiScreen)
	    (*s->finiPluginForScreen) (p, s);
    }
}

void
windowInitPlugins (CompWindow *w)
{
    CompPlugin *p;

    for (p = plugins; p; p = p->next)
    {
	if (p->vTable->initWindow)
	    (*p->vTable->initWindow) (p, w);
    }
}

void
windowFiniPlugins (CompWindow *w)
{
    CompPlugin *p;

    for (p = plugins; p; p = p->next)
    {
	if (p->vTable->finiWindow)
	    (*p->vTable->finiWindow) (p, w);
    }
}

CompPlugin *
findActivePlugin (char *name)
{
    CompPlugin *p;

    for (p = plugins; p; p = p->next)
    {
	if (strcmp (p->vTable->name, name) == 0)
	    return p;
    }

    return 0;
}

CompPlugin *
loadPlugin (char *name)
{
    CompPlugin *p;
    char       *file, *home, *plugindir;

    p = malloc (sizeof (CompPlugin));
    if (!p)
	return 0;

    file = malloc (strlen (name) + 7);
    if (!file)
    {
	free (p);
	return 0;
    }

    sprintf (file, "lib%s.so", name);

    p->next    = 0;
    p->dlhand  = 0;
    p->vTable  = 0;

    home = getenv ("HOME");
    if (home)
    {
	plugindir = malloc (strlen (home) +
			    strlen (HOME_PLUGINDIR) +
			    strlen (file) + 3);
	if (plugindir)
	{
	    sprintf (plugindir, "%s/%s/%s", home, HOME_PLUGINDIR, file);
	    p->dlhand = dlopen (plugindir, RTLD_LAZY);
	    free (plugindir);
	}
    }

    if (!p->dlhand)
    {
	plugindir = malloc (strlen (PLUGINDIR) + strlen (file) + 2);
	if (plugindir)
	{
	    sprintf (plugindir, "%s/%s", PLUGINDIR, file);
	    p->dlhand = dlopen (plugindir, RTLD_LAZY);
	    free (plugindir);
	}

	if (!p->dlhand)
	    p->dlhand = dlopen (file, RTLD_LAZY);
    }

    if (p->dlhand)
    {
	PluginGetInfoProc getInfo;
	char		  *error;

	dlerror ();

	getInfo = (PluginGetInfoProc) dlsym (p->dlhand, "getCompPluginInfo");

	error = dlerror ();
	if (error)
	{
	    fprintf (stderr, "%s: dlsym: %s\n", programName, error);

	    getInfo = 0;
	}

	if (getInfo)
	{
	    p->vTable = (*getInfo) ();
	    if (!p->vTable)
	    {
		fprintf (stderr, "%s: Couldn't get vtable from '%s' plugin\n",
			 programName, file);

		dlclose (p->dlhand);
		free (p);
		p = 0;
	    }
	}
	else
	{
	    fprintf (stderr, "%s: Failed to lookup getCompPluginInfo in '%s' "
		     "plugin\n", programName, file);

	    dlclose (p->dlhand);
	    free (p);
	    p = 0;
	}
    }
    else
    {
	fprintf (stderr, "%s: Couldn't load plugin '%s'\n", programName,
		 file);
	free (p);
	p = 0;
    }

    free (file);

    return p;
}

void
unloadPlugin (CompPlugin *p)
{
    dlclose (p->dlhand);
    free (p);
}

static Bool
checkPluginDeps (CompPlugin *p)
{
    CompPluginDep *deps = p->vTable->deps;
    int	          nDeps = p->vTable->nDeps;

    while (nDeps--)
    {
	switch (deps->rule) {
	case CompPluginRuleBefore:
	    if (findActivePlugin (deps->plugin))
	    {
		fprintf (stderr, "%s: '%s' plugin must be loaded before '%s' "
			 "plugin\n", programName, p->vTable->name, deps->plugin);

		return FALSE;
	    }
	    break;
	case CompPluginRuleAfter:
	    if (!findActivePlugin (deps->plugin))
	    {
		fprintf (stderr, "%s: '%s' plugin must be loaded after '%s' "
			 "plugin\n", programName, p->vTable->name, deps->plugin);

		return FALSE;
	    }
	    break;
	}

	deps++;
    }

    return TRUE;
}

Bool
pushPlugin (CompPlugin *p)
{
    if (findActivePlugin (p->vTable->name))
    {
	fprintf (stderr, "%s: Plugin '%s' already active\n", programName,
		 p->vTable->name);

	return FALSE;
    }

    if (!checkPluginDeps (p))
    {
	fprintf (stderr, "%s: Can't activate '%s' plugin due to dependency "
		 "problems\n", programName, p->vTable->name);

	return FALSE;
    }

    p->next = plugins;
    plugins = p;

    if (!initPlugin (p))
    {
	fprintf (stderr, "%s: Couldn't activate plugin '%s'\n", programName,
		 p->vTable->name);
	plugins = p->next;

	return FALSE;
    }

    return TRUE;
}

CompPlugin *
popPlugin (void)
{
    CompPlugin *p = plugins;

    if (!p)
	return 0;

    finiPlugin (p);

    plugins = p->next;

    return p;
}
