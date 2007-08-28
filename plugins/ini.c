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
#include <compiz.h>

#define DEFAULT_PLUGINS     "ini,inotify,png,decoration,move,resize,switcher"
#define NUM_DEFAULT_PLUGINS 7
#define MAX_OPTION_LENGTH   1024
#define HOME_OPTIONDIR     ".compiz/options"
#define CORE_NAME           "general"
#define FILE_SUFFIX         ".conf"

#define GET_INI_DISPLAY(d) \
	((IniDisplay *) (d)->privates[displayPrivateIndex].ptr)
#define INI_DISPLAY(d) \
	IniDisplay *id = GET_INI_DISPLAY (d)
#define GET_INI_SCREEN(s, id) \
	((IniScreen *) (s)->privates[(id)->screenPrivateIndex].ptr)
#define INI_SCREEN(s) \
	IniScreen *is = GET_INI_SCREEN (s, GET_INI_DISPLAY (s->display))

#define NUM_OPTIONS(s) (sizeof ((s)->opt) / sizeof (CompOption))

static int displayPrivateIndex;

static CompMetadata iniMetadata;

static Bool iniSaveOptions (CompDisplay *d,
			    int         screen,
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
 * IniDisplay
 */
typedef struct _IniDisplay {
    int		                  screenPrivateIndex;

    CompFileWatchHandle		  directoryWatch;

    InitPluginForDisplayProc      initPluginForDisplay;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;

    IniFileData			*fileData;
} IniDisplay;

/*
 * IniScreeen
 */
typedef struct _IniScreen {
    InitPluginForScreenProc        initPluginForScreen;
    SetScreenOptionForPluginProc   setScreenOptionForPlugin;
} IniScreen;

static IniFileData *
iniGetFileDataFromFilename (CompDisplay *d,
			    const char *filename)
{
    int len, i;
    int pluginSep = 0, screenSep = 0;
    char *pluginStr, *screenStr;
    IniFileData *fd;

    INI_DISPLAY (d);

    if (!filename)
	return NULL;

    len = strlen (filename);

    if (len < (strlen(FILE_SUFFIX) + 2))
	return NULL;

    if ((filename[0]=='.') || (filename[len-1]=='~'))
	return NULL;

    for (fd = id->fileData; fd; fd = fd->next)
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
	id->fileData = newFd;

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
	newFd->screen = atoi(&screenStr[6]);

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
iniGetFilename (CompDisplay *d,
		int screen,
		const char *plugin,
		char **filename)
{
    CompScreen *s;
    int	       len;
    char       *fn = NULL, *screenStr;

    screenStr = malloc (sizeof(char) * 12);
    if (!screenStr)
	return FALSE;

    if (screen > -1)
    {
	for (s = d->screens; s ; s = s->next)
	    if (s && (s->screenNum == screen))
		break;

	if (!s)
	{
	    compLogMessage (d, "ini", CompLogLevelWarn,
			    "Invalid screen number passed " \
			     "to iniGetFilename %d", screen);
	    free(screenStr);
	    return FALSE;
	}
	snprintf (screenStr, 12, "screen%d", screen);
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
    *optionName = strndup (line, length);
    splitPos++;
    optionLength = strlen (splitPos);
    if (splitPos[optionLength-1] == '\n')
	optionLength--;
    *optionValue = strndup (splitPos, optionLength);

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
    if (list->value)
    {
	for (i = 0; i < count; i++)
	{
	    splitEnd = strchr (splitStart, ',');

	    if (splitEnd)
	    {
		itemLength = strlen (splitStart) - strlen (splitEnd);
		item = strndup (splitStart, itemLength);
	    }
	    else // last value
	    {
		item = strdup (splitStart);
	    }

	    switch (type)
	    {
		case CompOptionTypeString:
		    if (item[0] != '\0')
			list->value[i].s = strdup (item);
		    break;
		case CompOptionTypeBool:
		    if (item[0] != '\0')
			list->value[i].b = (Bool) atoi (item);
		    break;
		case CompOptionTypeInt:
		    if (item[0] != '\0')
			list->value[i].i = atoi (item);
		    break;
		case CompOptionTypeFloat:
		    if (item[0] != '\0')
			list->value[i].f = atof (item);
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
	list->nValue = count;
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
	compLogMessage (NULL, "ini", CompLogLevelWarn,
			"Could not get HOME environmental variable");
	return FALSE;
    }
}

static Bool
iniLoadOptionsFromFile (CompDisplay *d,
			FILE        *optionFile,
			const char  *plugin,
			int         screen,
			Bool        *reSave)
{
    CompOption      *option = NULL, *o;
    CompScreen      *s = NULL;
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
	    compLogMessage (d, "ini", CompLogLevelWarn,
			    "Could not find running plugin " \
			    "%s (iniLoadOptionsFromFile)", plugin);
	    return FALSE;
	}
    }
    else
    {
	return FALSE;
    }

    if (screen > -1)
    {
	for (s = d->screens; s; s = s->next)
	    if (s && s->screenNum == screen)
		break;

	if (!s)
	{
	    compLogMessage (d, "ini", CompLogLevelWarn,
			    "Invalid screen number passed to " \
			    "iniLoadOptionsFromFile %d", screen);
	    return FALSE;
	}
    }

    if (s && p->vTable->getScreenOptions)
    {
	option = (*p->vTable->getScreenOptions) (p, s, &nOption);
    }
    else if (p->vTable->getDisplayOptions)
    {
	option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
    }

    while (fgets (tmp, MAX_OPTION_LENGTH, optionFile) != NULL)
    {
	status = FALSE;

	if (!iniParseLine (tmp, &optionName, &optionValue))
	{
	    compLogMessage (d, "ini", CompLogLevelWarn,
			    "Ignoring line '%s' in %s %i", tmp, plugin, screen);
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
		    stringToKeyAction (d, optionValue, &value.action);
		    break;
		case CompOptionTypeButton:
		    hasValue = TRUE;
		    stringToButtonAction (d, optionValue, &value.action);
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
		    hasValue = csvToList (d, optionValue, &value.list, value.list.type);
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
		    if (s)
			status = (*s->setScreenOptionForPlugin) (s,
								 plugin,
								 optionName,
								 &value);
		    else
			status = (*d->setDisplayOptionForPlugin) (d, plugin,
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
iniSaveOptions (CompDisplay *d,
	        int         screen,
		const char  *plugin)
{
    CompScreen *s = NULL;
    CompOption *option = NULL;
    int	       nOption = 0;
    char       *filename, *directory, *fullPath, *strVal = NULL;

    if (screen > -1)
    {
	for (s = d->screens; s; s = s->next)
	    if (s && s->screenNum == screen)
		break;

	if (!s)
	{
	    compLogMessage (d, "ini", CompLogLevelWarn,
			    "Invalid screen number passed to " \
			    "iniSaveOptions %d", screen);
	    return FALSE;
	}
    }

    if (plugin)
    {
	CompPlugin *p;
	p = findActivePlugin (plugin);
	if (!p)
	    return FALSE;

	if (s)
	    option = (*p->vTable->getScreenOptions) (p, s, &nOption);
	else
	    option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
    }
    else
    {
	return FALSE;
    }

    if (!option)
	return FALSE;

    if (!iniGetFilename (d, screen, plugin, &filename))
	return FALSE;

    IniFileData *fileData;

    fileData = iniGetFileDataFromFilename (d, filename);
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
	compLogMessage (d, "ini", CompLogLevelError,
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
		strVal = iniOptionValueToString (d, &option->value, option->type);
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
		    itemVal = iniOptionValueToString (d,
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
		compLogMessage (d, "ini", CompLogLevelWarn,
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
iniLoadOptions (CompDisplay *d,
	        int         screen,
	        const char  *plugin)
{
    char         *filename, *directory, *fullPath;
    FILE         *optionFile;
    Bool         loadRes, reSave = FALSE;
    IniFileData *fileData;

    filename = directory = fullPath = NULL;
    optionFile = NULL;
    fileData = NULL;

    if (!iniGetFilename (d, screen, plugin, &filename))
	return FALSE;

    fileData = iniGetFileDataFromFilename (d, filename);
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
	if (!plugin && (screen == -1))
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

	    if (!csvToList (d, DEFAULT_PLUGINS,
		            &value.list,
		            CompOptionTypeString))
	    {
		free (filename);
		free (directory);
		free (fullPath);
		return FALSE;
	    }

	    value.list.type = CompOptionTypeString;

	    compLogMessage (d, "ini", CompLogLevelWarn,
			    "Could not open main display config file %s", fullPath);
	    compLogMessage (d, "ini", CompLogLevelWarn,
			    "Loading default plugins (%s)", DEFAULT_PLUGINS);

	    (*d->setDisplayOptionForPlugin) (d, "core", "active_plugins",
					     &value);

	    free (value.list.value);

	    fileData->blockWrites = FALSE;

	    iniSaveOptions (d, screen, plugin);

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
	    compLogMessage (d, "ini", CompLogLevelWarn,
			    "Could not open config file %s - using " \
			    "defaults for %s", fullPath, (plugin)?plugin:"core");

	    fileData->blockWrites = FALSE;

	    iniSaveOptions (d, screen, plugin);

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

    loadRes = iniLoadOptionsFromFile (d, optionFile, plugin, screen, &reSave);

    fileData->blockWrites = FALSE;

    fclose (optionFile);

    if (loadRes && reSave)
    {
	fileData->blockReads = TRUE;
	iniSaveOptions (d, screen, plugin);
	fileData->blockReads = FALSE;
    }

    free (filename);
    free (directory);
    free (fullPath);

    return TRUE;
}

static void
iniFileModified (const char *name,
		 void       *closure)
{
    CompDisplay *d;
    IniFileData *fd;

    d = (CompDisplay *) closure;

    fd = iniGetFileDataFromFilename (d, name);
    if (fd)
    {
	iniLoadOptions (d, fd->screen, fd->plugin);
    }
}

static void
iniFreeFileData (CompDisplay *d)
{
    IniFileData *fd, *tmp;

    INI_DISPLAY (d);

    fd = id->fileData;

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
    Bool status;

    INI_DISPLAY (d);

    UNWRAP (id, d, initPluginForDisplay);
    status = (*d->initPluginForDisplay) (p, d);
    WRAP (id, d, initPluginForDisplay, iniInitPluginForDisplay);

    if (status && p->vTable->getDisplayOptions)
    {
	iniLoadOptions (d, -1, p->vTable->name);
    }
    else if (!status)
    {
	compLogMessage (d, "ini", CompLogLevelWarn,
			"Plugin '%s' failed to initialize " \
			"display settings", p->vTable->name);
    }

    return status;
}

static Bool
iniInitPluginForScreen (CompPlugin *p,
			CompScreen *s)
{
    Bool status;

    INI_SCREEN (s);

    UNWRAP (is, s, initPluginForScreen);
    status = (*s->initPluginForScreen) (p, s);
    WRAP (is, s, initPluginForScreen, iniInitPluginForScreen);

    if (status && p->vTable->getScreenOptions)
    {
	iniLoadOptions (s->display, s->screenNum, p->vTable->name);
    }
    else if (!status)
    {
	compLogMessage (s->display, "ini", CompLogLevelWarn,
			"Plugin '%s' failed to initialize " \
			"screen %d settings", p->vTable->name, s->screenNum);
    }

    return status;
}

static Bool
iniSetDisplayOptionForPlugin (CompDisplay     *d,
			      const char      *plugin,
			      const char      *name,
			      CompOptionValue *value)
{
    Bool status;

    INI_DISPLAY (d);

    UNWRAP (id, d, setDisplayOptionForPlugin);
    status = (*d->setDisplayOptionForPlugin) (d, plugin, name, value);
    WRAP (id, d, setDisplayOptionForPlugin, iniSetDisplayOptionForPlugin);

    if (status)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getDisplayOptions)
	    iniSaveOptions (d, -1, plugin);
    }

    return status;
}

static Bool
iniSetScreenOptionForPlugin (CompScreen      *s,
			     const char	     *plugin,
			     const char	     *name,
			     CompOptionValue *value)
{
    Bool status;

    INI_SCREEN (s);

    UNWRAP (is, s, setScreenOptionForPlugin);
    status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
    WRAP (is, s, setScreenOptionForPlugin, iniSetScreenOptionForPlugin);

    if (status)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getScreenOptions)
	    iniSaveOptions (s->display, s->screenNum, plugin);
    }

    return status;
}

static Bool
iniInitDisplay (CompPlugin *p, CompDisplay *d)
{
    IniDisplay *id;
    char *homeDir;

    if (!checkPluginABI ("core", ABIVERSION))
	return FALSE;

    id = malloc (sizeof (IniDisplay));
    if (!id)
        return FALSE;

    id->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (id->screenPrivateIndex < 0)
    {
        free (id);
        return FALSE;
    }

    id->fileData = NULL;
    id->directoryWatch = 0;

    WRAP (id, d, initPluginForDisplay, iniInitPluginForDisplay);
    WRAP (id, d, setDisplayOptionForPlugin, iniSetDisplayOptionForPlugin);

    d->privates[displayPrivateIndex].ptr = id;

    iniLoadOptions (d, -1, NULL);

    if (iniGetHomeDir (&homeDir))
    {
	id->directoryWatch = addFileWatch (d, homeDir,
					   NOTIFY_DELETE_MASK |
					   NOTIFY_CREATE_MASK |
					   NOTIFY_MODIFY_MASK,
					   iniFileModified, (void *) d);
	free (homeDir);
    }

    return TRUE;
}

static void
iniFiniDisplay (CompPlugin *p, CompDisplay *d)
{
    INI_DISPLAY (d);

    if (id->directoryWatch)
	removeFileWatch (d, id->directoryWatch);

    iniFreeFileData (d);

    freeScreenPrivateIndex (d, id->screenPrivateIndex);

    UNWRAP (id, d, initPluginForDisplay);
    UNWRAP (id, d, setDisplayOptionForPlugin);

    free (id);
}

static Bool
iniInitScreen (CompPlugin *p, CompScreen *s)
{
    IniScreen *is;

    INI_DISPLAY (s->display);

    is = malloc (sizeof (IniScreen));
    if (!is)
        return FALSE;

    s->privates[id->screenPrivateIndex].ptr = is;

    WRAP (is, s, initPluginForScreen, iniInitPluginForScreen);
    WRAP (is, s, setScreenOptionForPlugin, iniSetScreenOptionForPlugin);

    iniLoadOptions (s->display, s->screenNum, NULL);

    return TRUE;
}

static void
iniFiniScreen (CompPlugin *p, CompScreen *s)
{
    INI_SCREEN (s);

    UNWRAP (is, s, initPluginForScreen);
    UNWRAP (is, s, setScreenOptionForPlugin);

    free (is);
}

static Bool
iniInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&iniMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
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
    if (displayPrivateIndex >= 0)
        freeDisplayPrivateIndex (displayPrivateIndex);
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
    iniInitDisplay,
    iniFiniDisplay,
    iniInitScreen,
    iniFiniScreen,
    0,
    0,
    0,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &iniVTable;
}
