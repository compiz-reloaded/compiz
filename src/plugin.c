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
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <compiz-core.h>

CompPlugin *plugins = 0;

static Bool
coreInit (CompPlugin *p)
{
    return TRUE;
}

static void
coreFini (CompPlugin *p)
{
}

static CompMetadata *
coreGetMetadata (CompPlugin *plugin)
{
    return &coreMetadata;
}

static CompOption *
coreGetObjectOptions (CompPlugin *plugin,
		      CompObject *object,
		      int	 *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) getDisplayOptions,
	(GetPluginObjectOptionsProc) getScreenOptions
    };

    *count = 0;
    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab),
		     NULL, (plugin, object, count));
}

static Bool
coreSetObjectOption (CompPlugin      *plugin,
		     CompObject      *object,
		     const char      *name,
		     CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) setDisplayOption,
	(SetPluginObjectOptionProc) setScreenOption
    };

    RETURN_DISPATCH (object, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (plugin, object, name, value));
}

static CompPluginVTable coreVTable = {
    "core",
    coreGetMetadata,
    coreInit,
    coreFini,
    0, /* InitObject */
    0, /* FiniObject */
    coreGetObjectOptions,
    coreSetObjectOption
};

static Bool
cloaderLoadPlugin (CompPlugin *p,
		   const char *path,
		   const char *name)
{
    if (path)
	return FALSE;

    if (strcmp (name, coreVTable.name))
	return FALSE;

    p->vTable	      = &coreVTable;
    p->devPrivate.ptr = NULL;
    p->devType	      = "cloader";

    return TRUE;
}

static void
cloaderUnloadPlugin (CompPlugin *p)
{
}

static char **
cloaderListPlugins (const char *path,
		    int	       *n)
{
    char **list;

    if (path)
	return 0;

    list = malloc (sizeof (char *));
    if (!list)
	return 0;

    *list = strdup (coreVTable.name);
    if (!*list)
    {
	free (list);
	return 0;
    }

    *n = 1;

    return list;
}

static Bool
dlloaderLoadPlugin (CompPlugin *p,
		    const char *path,
		    const char *name)
{
    char        *file;
    void        *dlhand;
    struct stat fileInfo;
    Bool        loaded = FALSE;

    if (cloaderLoadPlugin (p, path, name))
	return TRUE;

    file = malloc ((path ? strlen (path) : 0) + strlen (name) + 8);
    if (!file)
	return FALSE;

    if (path)
	sprintf (file, "%s/lib%s.so", path, name);
    else
	sprintf (file, "lib%s.so", name);

    if (stat (file, &fileInfo) != 0)
    {
	/* file likely not present */
	compLogMessage ("core", CompLogLevelDebug,
			"Could not stat() file %s : %s",
			file, strerror (errno));
	free (file);
	return FALSE;
    }

    dlhand = dlopen (file, RTLD_LAZY);
    if (dlhand)
    {
	PluginGetInfoProc getInfo;
	char		  *error;

	dlerror ();

	getInfo = (PluginGetInfoProc) dlsym (dlhand,
					     "getCompPluginInfo20070830");

	error = dlerror ();
	if (error)
	{
	    compLogMessage ("core", CompLogLevelError, "dlsym: %s", error);

	    getInfo = 0;
	}

	if (getInfo)
	{
	    p->vTable = (*getInfo) ();
	    if (!p->vTable)
	    {
		compLogMessage ("core", CompLogLevelError,
				"Couldn't get vtable from '%s' plugin",
				file);
	    }
	    else
	    {
		p->devPrivate.ptr = dlhand;
		p->devType	  = "dlloader";
		loaded		  = TRUE;
	    }
	}
    }
    else
    {
	compLogMessage ("core", CompLogLevelError,
			"Couldn't load plugin '%s' : %s", file, dlerror ());
    }

    free (file);

    if (!loaded && dlhand)
	dlclose (dlhand);

    return loaded;
}

static void
dlloaderUnloadPlugin (CompPlugin *p)
{
    if (strcmp (p->devType, "dlloader") == 0)
	dlclose (p->devPrivate.ptr);
    else
	cloaderUnloadPlugin (p);
}

static int
dlloaderFilter (const struct dirent *name)
{
    int length = strlen (name->d_name);

    if (length < 7)
	return 0;

    if (strncmp (name->d_name, "lib", 3) ||
	strncmp (name->d_name + length - 3, ".so", 3))
	return 0;

    return 1;
}

static char **
dlloaderListPlugins (const char *path,
		     int	*n)
{
    struct dirent **nameList;
    char	  **list, **cList;
    char	  *name;
    int		  length, nFile, i, j = 0;

    cList = cloaderListPlugins (path, n);
    if (cList)
	j = *n;

    if (!path)
	path = ".";

    nFile = scandir (path, &nameList, dlloaderFilter, alphasort);
    if (!nFile)
	return cList;

    list = realloc (cList, (j + nFile) * sizeof (char *));
    if (!list)
	return cList;

    for (i = 0; i < nFile; i++)
    {
	length = strlen (nameList[i]->d_name);

	name = malloc ((length - 5) * sizeof (char));
	if (name)
	{
	    strncpy (name, nameList[i]->d_name + 3, length - 6);
	    name[length - 6] = '\0';

	    list[j++] = name;
	}
    }

    if (j)
    {
	*n = j;

	return list;
    }

    free (list);

    return NULL;
}

LoadPluginProc   loaderLoadPlugin   = dlloaderLoadPlugin;
UnloadPluginProc loaderUnloadPlugin = dlloaderUnloadPlugin;
ListPluginsProc  loaderListPlugins  = dlloaderListPlugins;

typedef struct _InitObjectContext {
    CompPlugin *plugin;
    CompObject *object;
} InitObjectContext;

typedef struct _InitObjectTypeContext {
    CompPlugin     *plugin;
    CompObjectType type;
} InitObjectTypeContext;

static CompBool
initObjectTree (CompObject *object,
		void       *closure);

static CompBool
finiObjectTree (CompObject *object,
		void       *closure);

static CompBool
initObjectsWithType (CompObjectType type,
		     CompObject	    *parent,
		     void	    *closure)
{
    InitObjectTypeContext *pCtx = (InitObjectTypeContext *) closure;
    InitObjectContext	  ctx;

    pCtx->type = type;

    ctx.plugin = pCtx->plugin;
    ctx.object = NULL;

    if (!compObjectForEach (parent, type, initObjectTree, (void *) &ctx))
    {
	compObjectForEach (parent, type, finiObjectTree, (void *) &ctx);

	return FALSE;
    }

    return TRUE;
}

static CompBool
finiObjectsWithType (CompObjectType type,
		     CompObject	    *parent,
		     void	    *closure)
{
    InitObjectTypeContext *pCtx = (InitObjectTypeContext *) closure;
    InitObjectContext	  ctx;

    /* pCtx->type is set to the object type that failed to be initialized */
    if (pCtx->type == type)
	return FALSE;

    ctx.plugin = pCtx->plugin;
    ctx.object = NULL;

    compObjectForEach (parent, type, finiObjectTree, (void *) &ctx);

    return TRUE;
}

static CompBool
initObjectTree (CompObject *object,
		void       *closure)
{
    InitObjectContext     *pCtx = (InitObjectContext *) closure;
    CompPlugin		  *p = pCtx->plugin;
    InitObjectTypeContext ctx;

    pCtx->object = object;

    if (p->vTable->initObject)
    {
	if (!(*p->vTable->initObject) (p, object))
	{
	    compLogMessage (p->vTable->name, CompLogLevelError,
			    "InitObject failed");
	    return FALSE;
	}
    }

    ctx.plugin = p;
    ctx.type   = 0;

    /* initialize children */
    if (!compObjectForEachType (object, initObjectsWithType, (void *) &ctx))
    {
	compObjectForEachType (object, finiObjectsWithType, (void *) &ctx);

	if (p->vTable->initObject && p->vTable->finiObject)
	    (*p->vTable->finiObject) (p, object);

	return FALSE;
    }

    if (!(*core.initPluginForObject) (p, object))
    {
	compObjectForEachType (object, finiObjectsWithType, (void *) &ctx);

	if (p->vTable->initObject && p->vTable->finiObject)
	    (*p->vTable->finiObject) (p, object);

	return FALSE;
    }

    return TRUE;
}

static CompBool
finiObjectTree (CompObject *object,
		void       *closure)
{
    InitObjectContext     *pCtx = (InitObjectContext *) closure;
    CompPlugin		  *p = pCtx->plugin;
    InitObjectTypeContext ctx;

    /* pCtx->object is set to the object that failed to be initialized */
    if (pCtx->object == object)
	return FALSE;

    ctx.plugin = p;
    ctx.type   = ~0;

    compObjectForEachType (object, finiObjectsWithType, (void *) &ctx);

    if (p->vTable->initObject && p->vTable->finiObject)
	(*p->vTable->finiObject) (p, object);

    (*core.finiPluginForObject) (p, object);

    return TRUE;
}

static Bool
initPlugin (CompPlugin *p)
{
    InitObjectContext ctx;

    if (!(*p->vTable->init) (p))
    {
	compLogMessage ("core", CompLogLevelError,
			"InitPlugin '%s' failed", p->vTable->name);
	return FALSE;
    }

    ctx.plugin = p;
    ctx.object = NULL;

    if (!initObjectTree (&core.base, (void *) &ctx))
    {
	(*p->vTable->fini) (p);
	return FALSE;
    }

    return TRUE;
}

static void
finiPlugin (CompPlugin *p)
{
    InitObjectContext ctx;

    ctx.plugin = p;
    ctx.object = NULL;

    finiObjectTree (&core.base, (void *) &ctx);

    (*p->vTable->fini) (p);
}

CompBool
objectInitPlugins (CompObject *o)
{
    InitObjectContext ctx;
    CompPlugin	      *p;
    int		      i, j = 0;

    ctx.object = NULL;

    for (p = plugins; p; p = p->next)
	j++;

    while (j--)
    {
	i = 0;
	for (p = plugins; i < j; p = p->next)
	    i++;

	ctx.plugin = p;

	if (!initObjectTree (o, (void *) &ctx))
	{
	    for (p = p->next; p; p = p->next)
	    {
		ctx.plugin = p;

		finiObjectTree (o, (void *) &ctx);
	    }

	    return FALSE;
	}
    }

    return TRUE;
}

void
objectFiniPlugins (CompObject *o)
{
    InitObjectContext ctx;
    CompPlugin	      *p;

    ctx.object = NULL;

    for (p = plugins; p; p = p->next)
    {
	ctx.plugin = p;

	finiObjectTree (o, (void *) &ctx);
    }
}

CompPlugin *
findActivePlugin (const char *name)
{
    CompPlugin *p;

    for (p = plugins; p; p = p->next)
    {
	if (strcmp (p->vTable->name, name) == 0)
	    return p;
    }

    return 0;
}

void
unloadPlugin (CompPlugin *p)
{
    (*loaderUnloadPlugin) (p);
    free (p);
}

CompPlugin *
loadPlugin (const char *name)
{
    CompPlugin *p;
    char       *home, *plugindir;
    Bool       status;

    p = malloc (sizeof (CompPlugin));
    if (!p)
	return 0;

    p->next	       = 0;
    p->devPrivate.uval = 0;
    p->devType	       = NULL;
    p->vTable	       = 0;

    home = getenv ("HOME");
    if (home)
    {
	plugindir = malloc (strlen (home) + strlen (HOME_PLUGINDIR) + 3);
	if (plugindir)
	{
	    sprintf (plugindir, "%s/%s", home, HOME_PLUGINDIR);
	    status = (*loaderLoadPlugin) (p, plugindir, name);
	    free (plugindir);

	    if (status)
		return p;
	}
    }

    status = (*loaderLoadPlugin) (p, PLUGINDIR, name);
    if (status)
	return p;

    status = (*loaderLoadPlugin) (p, NULL, name);
    if (status)
	return p;

    compLogMessage ("core", CompLogLevelError,
		    "Couldn't load plugin '%s'", name);

    free (p);

    return 0;
}

Bool
pushPlugin (CompPlugin *p)
{
    if (findActivePlugin (p->vTable->name))
    {
	compLogMessage ("core", CompLogLevelWarn,
			"Plugin '%s' already active",
			p->vTable->name);

	return FALSE;
    }

    p->next = plugins;
    plugins = p;

    if (!initPlugin (p))
    {
	compLogMessage ("core", CompLogLevelError,
			"Couldn't activate plugin '%s'", p->vTable->name);
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

CompPlugin *
getPlugins (void)
{
    return plugins;
}

static Bool
stringExist (char **list,
	     int  nList,
	     char *s)
{
    int i;

    for (i = 0; i < nList; i++)
	if (strcmp (list[i], s) == 0)
	    return TRUE;

    return FALSE;
}

char **
availablePlugins (int *n)
{
    char *home, *plugindir;
    char **list, **currentList, **pluginList, **homeList = NULL;
    int  nCurrentList, nPluginList, nHomeList;
    int  count, i, j;

    home = getenv ("HOME");
    if (home)
    {
	plugindir = malloc (strlen (home) + strlen (HOME_PLUGINDIR) + 3);
	if (plugindir)
	{
	    sprintf (plugindir, "%s/%s", home, HOME_PLUGINDIR);
	    homeList = (*loaderListPlugins) (plugindir, &nHomeList);
	    free (plugindir);
	}
    }

    pluginList  = (*loaderListPlugins) (PLUGINDIR, &nPluginList);
    currentList = (*loaderListPlugins) (NULL, &nCurrentList);

    count = 0;
    if (homeList)
	count += nHomeList;
    if (pluginList)
	count += nPluginList;
    if (currentList)
	count += nCurrentList;

    if (!count)
	return NULL;

    list = malloc (count * sizeof (char *));
    if (!list)
	return NULL;

    j = 0;
    if (homeList)
    {
	for (i = 0; i < nHomeList; i++)
	    if (!stringExist (list, j, homeList[i]))
		list[j++] = homeList[i];

	free (homeList);
    }

    if (pluginList)
    {
	for (i = 0; i < nPluginList; i++)
	    if (!stringExist (list, j, pluginList[i]))
		list[j++] = pluginList[i];

	free (pluginList);
    }

    if (currentList)
    {
	for (i = 0; i < nCurrentList; i++)
	    if (!stringExist (list, j, currentList[i]))
		list[j++] = currentList[i];

	free (currentList);
    }

    *n = j;

    return list;
}

int
getPluginABI (const char *name)
{
    CompPlugin *p = findActivePlugin (name);
    CompOption	*option;
    int		nOption;

    if (!p || !p->vTable->getObjectOptions)
	return 0;

    /* MULTIDPYERROR: ABI options should be moved into core */
    option = (*p->vTable->getObjectOptions) (p, &core.displays->base,
					     &nOption);

    return getIntOptionNamed (option, nOption, "abi", 0);
}

Bool
checkPluginABI (const char *name,
		int	   abi)
{
    int pluginABI;

    pluginABI = getPluginABI (name);
    if (!pluginABI)
    {
	compLogMessage ("core", CompLogLevelError,
			"Plugin '%s' not loaded.\n", name);
	return FALSE;
    }
    else if (pluginABI != abi)
    {
	compLogMessage ("core", CompLogLevelError,
			"Plugin '%s' has ABI version '%d', expected "
			"ABI version '%d'.\n",
			name, pluginABI, abi);
	return FALSE;
    }

    return TRUE;
}

Bool
getPluginDisplayIndex (CompDisplay *d,
		       const char  *name,
		       int	   *index)
{
    CompPlugin *p = findActivePlugin (name);
    CompOption	*option;
    int		nOption, value;

    if (!p || !p->vTable->getObjectOptions)
	return FALSE;

    option = (*p->vTable->getObjectOptions) (p, &d->base, &nOption);

    value = getIntOptionNamed (option, nOption, "index", -1);
    if (value < 0)
	return FALSE;

    *index = value;

    return TRUE;
}
