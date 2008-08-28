/*
 * Copyright © 2007 Mike Dransfield
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Mike Dransfield not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Mike Dransfield makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * MIKE DRANSFIELD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL MIKE DRANSFIELD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Mike Dransfield <mike@blueroot.co.uk>
 *
 * Some code taken from gconf.c by :
 *                       David Reveman <davidr@novell.com>
 */

#define _GNU_SOURCE /* for asprintf */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <compiz-core.h>

#define DEFAULT_PLUGINS     "ini,inotify,png,decoration,move,resize,switcher"
#define NUM_DEFAULT_PLUGINS 7
#define MAX_OPTION_LENGTH   1024
#define HOME_OPTIONDIR     ".compiz/options"
#define CORE_NAME           "general"
#define FILE_SUFFIX         ".conf"

#define GET_INI_CORE(c) \
	((IniCore *) (c)->base.privates[corePrivateIndex].ptr)
#define INI_CORE(c) \
	IniCore *ic = GET_INI_CORE (c)

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static int corePrivateIndex;

static CompMetadata iniMetadata;

static Bool iniSaveOptions (CompObject  *object,
			    const char  *plugin);

/*
 * IniFileData
 */
typedef struct _IniFileData IniFileData;
struct _IniFileData {
    char		 *filename;
    char		 *plugin;
    int			 screen;

    Bool		 blockWrites;
    Bool		 blockReads;

    IniFileData		 *next;
    IniFileData		 *prev;
};

/*
 * IniCore
 */
typedef struct _IniCore {
    CompFileWatchHandle	directoryWatch;

    IniFileData	*fileData;

    InitPluginForObjectProc initPluginForObject;
    SetOptionForPluginProc  setOptionForPlugin;
} IniCore;

static IniFileData *
iniGetFileDataFromFilename (const char *filename)
{
    int len, i;
    int pluginSep = 0, screenSep = 0;
    char *pluginStr, *screenStr;
    IniFileData *fd;

    INI_CORE (&core);

    if (!filename)
	return NULL;

    len = strlen (filename);

    if (len < (strlen(FILE_SUFFIX) + 2))
	return NULL;

    if ((filename[0]=='.') || (filename[len-1]=='~'))
	return NULL;

    for (fd = ic->fileData; fd; fd = fd->next)
	if (strcmp (fd->filename, filename) == 0)
	    return fd;

    for (i=0; i<len; i++)
    {
	if (filename[i] == '-')
	{
	    if (!pluginSep)
		pluginSep = i-1;
	    else
		return NULL; /*found a second dash */
	}
	else if (filename[i] == '.')
	{
	    if (!screenSep)
		screenSep = i-1;
	    else
		return NULL; /*found a second dot */
	}
    }

    if (!pluginSep || !screenSep)
	return NULL;

    /* If we get here then there is no fd in the display variable */
    IniFileData *newFd = malloc (sizeof (IniFileData));
    if (!newFd)
	return NULL;

    /* fd is NULL here, see condition "fd" in first for-loop */
    /* if (fd)
	fd->next = newFd;
    else
    */
	ic->fileData = newFd;

    newFd->prev = fd;
    newFd->next = NULL;

    newFd->filename = strdup (filename);

    pluginStr = calloc (1, sizeof (char) * pluginSep + 2);
    if (!pluginStr)
	return NULL;

    screenStr = calloc (1, sizeof (char) * (screenSep - pluginSep));
    if (!screenStr) {
	free(pluginStr);
	return NULL;
    }

    strncpy (pluginStr, filename, pluginSep + 1);
    strncpy (screenStr, &filename[pluginSep+2], (screenSep - pluginSep) - 1);

    if (strcmp (pluginStr, CORE_NAME) == 0)
	newFd->plugin = NULL;
    else
	newFd->plugin = strdup (pluginStr);

    if (strcmp (screenStr, "allscreens") == 0)
	newFd->screen = -1;
    else
	newFd->screen = atoi (&screenStr[6]);

    newFd->blockReads  = FALSE;
    newFd->blockWrites = FALSE;

    free (pluginStr);
    free (screenStr);

    return newFd;
}

static char *
iniOptionValueToString (CompDisplay *d, CompOptionValue *value, CompOptionType type)
{
    char tmp[MAX_OPTION_LENGTH];
    tmp[0] = '\0';

    switch (type)
    {
    case CompOptionTypeBool:
    case CompOptionTypeInt:
	snprintf(tmp, 256, "%i", (int)value->i);
	break;
    case CompOptionTypeFloat:
	snprintf(tmp, 256, "%f", value->f);
	break;
    case CompOptionTypeString:
	snprintf (tmp, MAX_OPTION_LENGTH, "%s", strdup (value->s));
	break;
    case CompOptionTypeColor:
	snprintf (tmp, 10, "%s", colorToString (value->c));
	break;
    case CompOptionTypeKey:
	return keyActionToString (d, &value->action);
	break;
    case CompOptionTypeButton:
	return buttonActionToString (d, &value->action);
	break;
    case CompOptionTypeEdge:
	return edgeMaskToString (value->action.edgeMask);
	break;
    case CompOptionTypeBell:
	snprintf (tmp, 256, "%i", (int) value->action.bell);
	break;
    case CompOptionTypeMatch:
        {
	    char *s = matchToString (&value->match);
	    snprintf (tmp, MAX_OPTION_LENGTH, "%s", s);
	    free(s);
	}
	break;
    default:
	break;
    }

    return strdup (tmp);
}

static Bool
iniGetHomeDir (char **homeDir)
{
    char *home = NULL, *tmp;

    home = getenv ("HOME");
    if (home)
    {
	tmp = malloc (strlen (home) + strlen (HOME_OPTIONDIR) + 2);
	if (tmp)
	{
	    sprintf (tmp, "%s/%s", home, HOME_OPTIONDIR);
	    (*homeDir) = strdup (tmp);
	    free (tmp);

	    return TRUE;
	}
    }

    return FALSE;
}

static Bool
iniGetFilename (CompObject *object,
		const char *plugin,
		char **filename)
{
    int	 len;
    char *fn = NULL, *screenStr;

    screenStr = malloc (sizeof(char) * 12);
    if (!screenStr)
	return FALSE;

    if (object->type == COMP_OBJECT_TYPE_SCREEN)
    {
	CORE_SCREEN (object);

	snprintf (screenStr, 12, "screen%d", s->screenNum);
    }
    else
    {
	strncpy (screenStr, "allscreens", 12);
    }

    len = strlen (screenStr) + strlen (FILE_SUFFIX) + 2;

    if (plugin)
	len += strlen (plugin);
    else
	len += strlen (CORE_NAME);

    fn = malloc (sizeof (char) * len);
    if (fn)
    {
	sprintf (fn, "%s-%s%s",
		 plugin ? plugin : CORE_NAME, screenStr, FILE_SUFFIX);

	*filename = strdup (fn);

	free (screenStr);
	free (fn);

	return TRUE;
    }

    free (screenStr);

    return FALSE;
}

static Bool
iniParseLine (char *line, char **optionName, char **optionValue)
{
    char *splitPos;
    int  length, optionLength;

    if (line[0] == '\0' || line[0] == '\n')
	return FALSE;

    splitPos = strchr (line, '=');
    if (!splitPos)
	return FALSE;

    length = strlen (line) - strlen (splitPos);
    *optionName = malloc (sizeof (char) * (length + 1));
    if (*optionName)
    {
       strncpy (*optionName, line, length);
       (*optionName)[length] = 0;
    }
    splitPos++;
    optionLength = strlen (splitPos);
    if (splitPos[optionLength-1] == '\n')
	optionLength--;
    *optionValue = malloc (sizeof (char) * (optionLength + 1));
    if (*optionValue)
    {
      strncpy (*optionValue, splitPos, optionLength);
      (*optionValue)[optionLength] = 0;
    }
    return TRUE;
}

static Bool
csvToList (CompDisplay *d, char *csv, CompListValue *list, CompOptionType type)
{
    char *splitStart = NULL;
    char *splitEnd = NULL;
    char *item = NULL;
    int  itemLength, count, i;

    if (csv[0] == '\0')
    {
	list->nValue = 0;
	return FALSE;
    }

    int length = strlen (csv);
    count = 1;
    for (i = 0; csv[i] != '\0'; i++)
	if (csv[i] == ',' && i != length-1)
	    count++;

    splitStart = csv;
    list->value = malloc (sizeof (CompOptionValue) * count);
    list->nValue = count;

    if (list->value)
    {
	for (i = 0; i < count; i++)
	{
	    splitEnd = strchr (splitStart, ',');

	    if (splitEnd)
	    {
		itemLength = strlen (splitStart) - strlen (splitEnd);
		item = malloc (sizeof (char) * (itemLength + 1));
		if (item)
		{
		   strncpy (item, splitStart, itemLength);
		   item[itemLength] = 0;
		}
	    }
	    else // last value
	    {
		item = strdup (splitStart);
	    }

	    if (!item) {
	        compLogMessage ("ini", CompLogLevelError, "Not enough memory");
	        list->nValue = 0;
	        return FALSE;
	    }

	    switch (type)
	    {
		case CompOptionTypeString:
		    list->value[i].s = strdup (item);
		    break;
		case CompOptionTypeBool:
		    list->value[i].b = item[0] ? (Bool) atoi (item) : FALSE;
		    break;
		case CompOptionTypeInt:
		    list->value[i].i = item[0] ? atoi (item) : 0;
		    break;
		case CompOptionTypeFloat:
		    list->value[i].f = item[0] ? atof (item) : 0.0f;
		    break;
		case CompOptionTypeKey:
		    stringToKeyAction (d, item, &list->value[i].action);
		    break;
		case CompOptionTypeButton:
		    stringToButtonAction (d, item, &list->value[i].action);
		    break;
		case CompOptionTypeEdge:
		    list->value[i].action.edgeMask = stringToEdgeMask (item);
		    break;
		case CompOptionTypeBell:
		    list->value[i].action.bell = (Bool) atoi (item);
		    break;
		case CompOptionTypeMatch:
		    matchInit (&list->value[i].match);
		    matchAddFromString (&list->value[i].match, item);
		    break;
		default:
		    break;
	    }

	    splitStart = ++splitEnd;
	    if (item)
	    {
		free (item);
		item = NULL;
	    }
	}
    }

    return TRUE;
}

static Bool
iniMakeDirectories (void)
{
    char *homeDir;

    if (iniGetHomeDir (&homeDir))
    {
	mkdir (homeDir, 0700);
	free (homeDir);

	return TRUE;
    }
    else
    {
	compLogMessage ("ini", CompLogLevelWarn,
			"Could not get HOME environmental variable");
	return FALSE;
    }
}

static Bool
iniLoadOptionsFromFile (FILE       *optionFile,
			CompObject *object,
			const char *plugin,
			Bool       *reSave)
{
    CompOption      *option = NULL, *o;
    CompPlugin      *p = NULL;
    CompOptionValue value;
    char            *optionName = NULL, *optionValue = NULL;
    char            tmp[MAX_OPTION_LENGTH];
    int             nOption, nOptionRead = 0;
    Bool            status = FALSE, hasValue = FALSE;

    if (plugin)
    {
	p = findActivePlugin (plugin);
	if (!p)
	{
	    compLogMessage ("ini", CompLogLevelWarn,
			    "Could not find running plugin " \
			    "%s (iniLoadOptionsFromFile)", plugin);
	    return FALSE;
	}
    }
    else
    {
	return FALSE;
    }

    if (p->vTable->getObjectOptions)
	option = (*p->vTable->getObjectOptions) (p, object, &nOption);

    while (fgets (tmp, MAX_OPTION_LENGTH, optionFile) != NULL)
    {
	status = FALSE;

	if (!iniParseLine (tmp, &optionName, &optionValue))
	{
	    compLogMessage ("ini", CompLogLevelWarn,
			    "Ignoring line '%s' in %s", tmp, plugin);
	    continue;
	}

	if (option)
	{
	    o = compFindOption (option, nOption, optionName, 0);
	    if (o)
	    {
		value = o->value;

		switch (o->type)
		{
		case CompOptionTypeBool:
		    hasValue = TRUE;
		    value.b = (Bool) atoi (optionValue);
			break;
		case CompOptionTypeInt:
		    hasValue = TRUE;
		    value.i = atoi (optionValue);
			break;
		case CompOptionTypeFloat:
		    hasValue = TRUE;
		    value.f = atof (optionValue);
			break;
		case CompOptionTypeString:
		    hasValue = TRUE;
		    value.s = strdup (optionValue);
			break;
		case CompOptionTypeColor:
		    hasValue = stringToColor (optionValue, value.c);
			break;
		case CompOptionTypeKey:
		    hasValue = TRUE;
		    stringToKeyAction (GET_CORE_DISPLAY (object),
				       optionValue, &value.action);
		    break;
		case CompOptionTypeButton:
		    hasValue = TRUE;
		    stringToButtonAction (GET_CORE_DISPLAY (object),
					  optionValue, &value.action);
		    break;
		case CompOptionTypeEdge:
		    hasValue = TRUE;
		    value.action.edgeMask = stringToEdgeMask (optionValue);
		    break;
		case CompOptionTypeBell:
		    hasValue = TRUE;
		    value.action.bell = (Bool) atoi (optionValue);
		    break;
		case CompOptionTypeList:
		    hasValue = csvToList (GET_CORE_DISPLAY (object),
					  optionValue,
					  &value.list, value.list.type);
			break;
		case CompOptionTypeMatch:
		    hasValue = TRUE;
		    matchInit (&value.match);
		    matchAddFromString (&value.match, optionValue);
			break;
		default:
			break;
		}

		if (hasValue)
		{
		    status = (*core.setOptionForPlugin) (object,
							 plugin,
							 optionName,
							 &value);

		    if (o->type == CompOptionTypeMatch)
		    {
			matchFini (&value.match);
		    }
		}

		nOptionRead++;
	    }
	}

	/* clear up */
	if (optionName)
	    free (optionName);
	if (optionValue)
	    free (optionValue);
    }

    if (nOption != nOptionRead)
    {
	*reSave = TRUE;
    }

    return TRUE;
}

static Bool
iniSaveOptions (CompObject *object,
		const char *plugin)
{
    CompOption *option = NULL;
    int	       nOption = 0;
    char       *filename, *directory, *fullPath, *strVal = NULL;

    if (plugin)
    {
	CompPlugin *p;
	p = findActivePlugin (plugin);
	if (!p)
	    return FALSE;

	option = (*p->vTable->getObjectOptions) (p, object, &nOption);
    }
    else
    {
	return FALSE;
    }

    if (!option)
	return FALSE;

    if (!iniGetFilename (object, plugin, &filename))
	return FALSE;

    IniFileData *fileData;

    fileData = iniGetFileDataFromFilename (filename);
    if (!fileData || (fileData && fileData->blockWrites))
    {
	free (filename);
	return FALSE;
    }

    if (!iniGetHomeDir (&directory))
	return FALSE;

    fullPath = malloc (sizeof (char) * (strlen (filename) + strlen (directory) + 2));
    if (!fullPath)
    {
	free (filename);
	free (directory);
	return FALSE;
    }

    sprintf (fullPath, "%s/%s", directory, filename);

    FILE *optionFile = fopen (fullPath, "w");

    if (!optionFile && iniMakeDirectories ())
	optionFile = fopen (fullPath, "w");

    if (!optionFile)
    {
	compLogMessage ("ini", CompLogLevelError,
			"Failed to write to %s, check you " \
			"have the correct permissions", fullPath);
	free (filename);
	free (directory);
	free (fullPath);
	return FALSE;
    }

    fileData->blockReads = TRUE;

    Bool status, firstInList;
    while (nOption--)
    {
	status = FALSE;
	int i;

	switch (option->type)
	{
	case CompOptionTypeBool:
	case CompOptionTypeInt:
	case CompOptionTypeFloat:
	case CompOptionTypeString:
	case CompOptionTypeColor:
	case CompOptionTypeKey:
	case CompOptionTypeButton:
	case CompOptionTypeEdge:
	case CompOptionTypeBell:
	case CompOptionTypeMatch:
	    strVal = iniOptionValueToString (GET_CORE_DISPLAY (object),
					     &option->value, option->type);
		if (strVal)
		{
		    fprintf (optionFile, "%s=%s\n", option->name, strVal);
		    free (strVal);
		}
		else
		    fprintf (optionFile, "%s=\n", option->name);
		break;
	case CompOptionTypeList:
	    firstInList = TRUE;
	    switch (option->value.list.type)
	    {
	    case CompOptionTypeBool:
	    case CompOptionTypeInt:
	    case CompOptionTypeFloat:
	    case CompOptionTypeString:
	    case CompOptionTypeColor:
	    case CompOptionTypeMatch:
	    {
		int stringLen = MAX_OPTION_LENGTH * option->value.list.nValue;
		char *itemVal;

		strVal = malloc (sizeof(char) * stringLen);
		if (!strVal) {
		    fclose(optionFile);
		    free(fullPath);
		    return FALSE;
		}
		strcpy (strVal, "");
		firstInList = TRUE;

		for (i = 0; i < option->value.list.nValue; i++)
		{
		    itemVal =
			iniOptionValueToString (GET_CORE_DISPLAY (object),
						&option->value.list.value[i],
						option->value.list.type);
		    if (!firstInList)
		        strncat (strVal, ",", stringLen);
		    firstInList = FALSE;
			
		    if (itemVal)
		    {
			strncat (strVal, itemVal, stringLen);
			free (itemVal);
		    }
		}

		fprintf (optionFile, "%s=%s\n", option->name, strVal);
		free (strVal);
		break;
	    }
	    default:
		compLogMessage ("ini", CompLogLevelWarn,
				"Unknown list option type %d, %s\n",
				option->value.list.type,
				optionTypeToString (option->value.list.type));
		break;
	    }
		break;
	default:
		break;
	}

	option++;
    }

    fileData->blockReads = FALSE;

    fclose (optionFile);

    free (filename);
    free (directory);
    free (fullPath);

    return TRUE;
}

static Bool
iniLoadOptions (CompObject *object,
		const char *plugin)
{
    char         *filename, *directory, *fullPath;
    FILE         *optionFile;
    Bool         loadRes, reSave = FALSE;
    IniFileData *fileData;

    filename = directory = fullPath = NULL;
    optionFile = NULL;
    fileData = NULL;

    if (!iniGetFilename (object, plugin, &filename))
	return FALSE;

    fileData = iniGetFileDataFromFilename (filename);
    if (!fileData || (fileData && fileData->blockReads))
    {
	free(filename);
	return FALSE;
    }

    if (!iniGetHomeDir (&directory))
    {
	free (filename);
	return FALSE;
    }

    fullPath = malloc (sizeof (char) * (strlen (filename) + strlen (directory) + 2));
    if (!fullPath)
    {
	free (filename);
	free (directory);
	return FALSE;
    }

    sprintf(fullPath, "%s/%s", directory, filename);

    optionFile = fopen (fullPath, "r");

    if (!optionFile && iniMakeDirectories ())
	optionFile = fopen (fullPath, "r");

    if (!optionFile)
    {
	if (!plugin && object->type == COMP_OBJECT_TYPE_DISPLAY)
	{
	    CompOptionValue value;
	    value.list.value = malloc (NUM_DEFAULT_PLUGINS * sizeof (CompListValue));
	    if (!value.list.value)
	    {
		free (filename);
		free (directory);
		free (fullPath);
		return FALSE;
	    }

	    if (!csvToList (GET_CORE_DISPLAY (object), DEFAULT_PLUGINS,
		            &value.list,
		            CompOptionTypeString))
	    {
		free (filename);
		free (directory);
		free (fullPath);
		return FALSE;
	    }

	    value.list.type = CompOptionTypeString;

	    compLogMessage ("ini", CompLogLevelWarn,
			    "Could not open main display config file %s",
			    fullPath);
	    compLogMessage ("ini", CompLogLevelWarn,
			    "Loading default plugins (%s)", DEFAULT_PLUGINS);

	    (*core.setOptionForPlugin) (object,
					"core", "active_plugins",
					&value);

	    free (value.list.value);

	    fileData->blockWrites = FALSE;

	    iniSaveOptions (object, plugin);

	    fileData->blockWrites = TRUE;

	    optionFile = fopen (fullPath, "r");

	    if (!optionFile)
	    {
		free (filename);
		free (directory);
		free (fullPath);
		return FALSE;
	    }
	}
	else
	{
	    compLogMessage ("ini", CompLogLevelWarn,
			    "Could not open config file %s - "
			    "using defaults for %s",
			    fullPath, plugin ? plugin : "core");

	    fileData->blockWrites = FALSE;

	    iniSaveOptions (object, plugin);

	    fileData->blockWrites = TRUE;

	    optionFile = fopen (fullPath, "r");
	    if (!optionFile)
	    {
		free (filename);
		free (directory);
		free (fullPath);
		return FALSE;
	    }
	}
    }

    fileData->blockWrites = TRUE;

    loadRes = iniLoadOptionsFromFile (optionFile, object, plugin, &reSave);

    fileData->blockWrites = FALSE;

    fclose (optionFile);

    if (loadRes && reSave)
    {
	fileData->blockReads = TRUE;
	iniSaveOptions (object, plugin);
	fileData->blockReads = FALSE;
    }

    free (filename);
    free (directory);
    free (fullPath);

    return TRUE;
}

/* MULTIDPYERROR: only works with one or less displays present */
/* OBJECTOPTION: only display and screen options are supported */
static void
iniFileModified (const char *name,
		 void       *closure)
{
    IniFileData *fd;

    fd = iniGetFileDataFromFilename (name);
    if (fd && core.displays)
    {
	if (fd->screen < 0)
	{
	    iniLoadOptions (&core.displays->base, fd->plugin);
	}
	else
	{
	    CompScreen *s;

	    for (s = core.displays->screens; s; s = s->next)
		if (s->screenNum == fd->screen)
		    break;

	    if (s)
		iniLoadOptions (&s->base, fd->plugin);
	}
    }
}

static void
iniFreeFileData (void)
{
    IniFileData *fd, *tmp;

    INI_CORE (&core);

    fd = ic->fileData;

    while (fd)
    {
        tmp = fd;
        fd = fd->next;
        free (tmp);
    }
}

/*
CORE FUNCTIONS
*/

static Bool
iniInitPluginForDisplay (CompPlugin  *p,
			 CompDisplay *d)
{
    iniLoadOptions (&d->base, p->vTable->name);

    return TRUE;
}

static Bool
iniInitPluginForScreen (CompPlugin *p,
			CompScreen *s)
{
    iniLoadOptions (&s->base, p->vTable->name);

    return TRUE;
}

static CompBool
iniInitPluginForObject (CompPlugin *p,
			CompObject *o)
{
    CompBool status;

    INI_CORE (&core);

    UNWRAP (ic, &core, initPluginForObject);
    status = (*core.initPluginForObject) (p, o);
    WRAP (ic, &core, initPluginForObject, iniInitPluginForObject);

    if (status && p->vTable->getObjectOptions)
    {
	static InitPluginForObjectProc dispTab[] = {
	    (InitPluginForObjectProc) 0, /* InitPluginForCore */
	    (InitPluginForObjectProc) iniInitPluginForDisplay,
	    (InitPluginForObjectProc) iniInitPluginForScreen
	};

	RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
    }

    return status;
}

static CompBool
iniSetOptionForPlugin (CompObject      *object,
		       const char      *plugin,
		       const char      *name,
		       CompOptionValue *value)
{
    CompBool status;

    INI_CORE (&core);

    UNWRAP (ic, &core, setOptionForPlugin);
    status = (*core.setOptionForPlugin) (object, plugin, name, value);
    WRAP (ic, &core, setOptionForPlugin, iniSetOptionForPlugin);

    if (status)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getObjectOptions)
	    iniSaveOptions (object, plugin);
    }

    return status;
}

static Bool
iniInitCore (CompPlugin *p,
	     CompCore   *c)
{
    IniCore *ic;
    char    *homeDir;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    ic = malloc (sizeof (IniCore));
    if (!ic)
	return FALSE;

    ic->fileData = NULL;
    ic->directoryWatch = 0;

    if (iniGetHomeDir (&homeDir))
    {
	ic->directoryWatch = addFileWatch (homeDir,
					   NOTIFY_DELETE_MASK |
					   NOTIFY_CREATE_MASK |
					   NOTIFY_MODIFY_MASK,
					   iniFileModified, 0);
	free (homeDir);
    }

    WRAP (ic, c, initPluginForObject, iniInitPluginForObject);
    WRAP (ic, c, setOptionForPlugin, iniSetOptionForPlugin);

    c->base.privates[corePrivateIndex].ptr = ic;

    return TRUE;
}

static void
iniFiniCore (CompPlugin *p,
	     CompCore   *c)
{
    INI_CORE (c);

    UNWRAP (ic, c, initPluginForObject);
    UNWRAP (ic, c, setOptionForPlugin);

    if (ic->directoryWatch)
	removeFileWatch (ic->directoryWatch);

    iniFreeFileData ();

    free (ic);
}

static Bool
iniInitDisplay (CompPlugin *p, CompDisplay *d)
{
    iniLoadOptions (&d->base, NULL);

    return TRUE;
}

static Bool
iniInitScreen (CompPlugin *p, CompScreen *s)
{
    iniLoadOptions (&s->base, NULL);

    return TRUE;
}

static CompBool
iniInitObject (CompPlugin *p,
	       CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) iniInitCore,
	(InitPluginObjectProc) iniInitDisplay,
	(InitPluginObjectProc) iniInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
iniFiniObject (CompPlugin *p,
	       CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) iniFiniCore
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
iniInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&iniMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    corePrivateIndex = allocateCorePrivateIndex ();
    if (corePrivateIndex < 0)
    {
	compFiniMetadata (&iniMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&iniMetadata, p->vTable->name);

    return TRUE;
}

static void
iniFini (CompPlugin *p)
{
    freeCorePrivateIndex (corePrivateIndex);
}

static CompMetadata *
iniGetMetadata (CompPlugin *plugin)
{
    return &iniMetadata;
}

CompPluginVTable iniVTable = {
    "ini",
    iniGetMetadata,
    iniInit,
    iniFini,
    iniInitObject,
    iniFiniObject,
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &iniVTable;
}
