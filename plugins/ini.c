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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <compiz.h>

#define DEFAULT_PLUGINS     "ini,inotify,png,decoration,move,resize,switcher"
#define NUM_DEFAULT_PLUGINS 7
#define MAX_OPTION_LENGTH   1024
#define HOME_OPTION_DIR     ".compiz/options"
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

#define N_ACTION_PARTS 5
typedef struct _IniActionProxy {
    int               nSet;

    CompBindingType   type;
    CompKeyBinding    key;
    CompButtonBinding button;

    Bool bell;

    unsigned int edgeMask;
    int		 edgeButton;
} IniActionProxy;

typedef struct _IniDisplay {
    int		                  screenPrivateIndex;

    CompFileWatchHandle		  directoryWatch;

    InitPluginForDisplayProc      initPluginForDisplay;
    SetDisplayOptionProc	  setDisplayOption;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;

    IniFileData			*fileData;
} IniDisplay;

typedef struct _IniScreen {
    InitPluginForScreenProc        initPluginForScreen;
    SetScreenOptionProc		   setScreenOption;
    SetScreenOptionForPluginProc   setScreenOptionForPlugin;
} IniScreen;

static void
initActionProxy (IniActionProxy *a)
{
    a->type = 0;

    a->nSet = 0;

    a->key.keycode = 0;
    a->key.modifiers = 0;

    a->button.button = 0;
    a->button.modifiers = 0;

    a->bell = FALSE;

    a->edgeMask = 0;
    a->edgeButton = 0;
}

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

    /* fd now contains 'prev' or NULL */
    if (fd)
	fd->next = newFd;
    else
	id->fileData = newFd;

    newFd->prev = fd;
    newFd->next = NULL;

    newFd->filename = strdup (filename);

    pluginStr = malloc (sizeof (char) * pluginSep + 1);
    screenStr = malloc (sizeof (char) * (screenSep - pluginSep) + 1);

    if (!pluginStr || !screenStr)
	return NULL;

    strncpy (pluginStr, filename, pluginSep + 1);
    pluginStr[pluginSep + 1] = '\0';

    strncpy (screenStr, &filename[pluginSep+2], (screenSep - pluginSep) + 1);
    screenStr[(screenSep - pluginSep) -1] = '\0';

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
iniOptionValueToString (CompOptionValue *value, CompOptionType type)
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
    case CompOptionTypeMatch:
	snprintf (tmp, MAX_OPTION_LENGTH, "%s", matchToString (&value->match));
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
	tmp = malloc (strlen (home) + strlen (HOME_OPTION_DIR) + 2);
	if (tmp)
	{
	    sprintf (tmp, "%s/%s", home, HOME_OPTION_DIR);
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
		char *plugin,
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
	    fprintf (stderr, "Invalid screen number passed " \
			     "to iniGetFilename %d\n", screen);
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
		 plugin?plugin:CORE_NAME, screenStr, FILE_SUFFIX);

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
    int  pos = 0;
    int  splitPos = 0;
    int  endPos = 0;
    char tmpName[MAX_OPTION_LENGTH];
    char tmpValue[MAX_OPTION_LENGTH];

    if (line[0] == '\0' || line[0] == '\n')
	return FALSE;

    while (pos < strlen(line))
    {
	if (!splitPos && line[pos] == '=')
	    splitPos = pos;
	if (line[pos] == '\n')
	{
	    endPos = pos;
	    break;
	}
	pos++;
    }

    if (splitPos && endPos)
    {
	tmpName[0] = '\0';
	tmpValue[0] = '\0';

	int i;
	for (i=0; i < splitPos; i++)
	    tmpName[i] = line[i];
	tmpName[splitPos] = '\0';

	for (i=splitPos+1; i<endPos; i++)
	    tmpValue[i - (splitPos+1)] = line[i];
	tmpValue[endPos - (splitPos+1)] = '\0';

	*optionName = strdup (tmpName);
	*optionValue = strdup (tmpValue);
    }
    else
    {
	return FALSE;
    }

    return TRUE;
}

static Bool
csvToList (char *csv, CompListValue *list, CompOptionType type)
{
    char *csvtmp, *split, *item = NULL;
    int  count = 1, i, itemLength;

    if (csv[0] == '\0')
    {
	list->nValue = 0;
	return FALSE;
    }

    csvtmp = strdup (csv);
    csvtmp = strchr (csv, ',');

    while (csvtmp)
    {
	csvtmp++;  /* avoid the comma */
	count++;
	csvtmp = strchr (csvtmp, ',');
    }

    list->value = malloc (sizeof (CompOptionValue) * count);
    if (list->value)
    {
	for (i=0; i<count; i++)
	{
	    split = strchr (csv, ',');
	    if (split)
	    {
		/* > 1 value */
		itemLength = strlen(csv) - strlen(split);
		item = realloc (item, sizeof (char) * (itemLength+1));
		strncpy (item, csv, itemLength);
		item[itemLength] = '\0';
		csv += itemLength + 1;
	    }
	    else
	    {
		/* 1 value only */
		itemLength = strlen(csv);
		item = realloc (item, sizeof (char) * (itemLength+1));
		strncpy (item, csv, itemLength);
		item[itemLength] = '\0';
	    }

	    switch (type)
	    {
		case CompOptionTypeString:
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
		case CompOptionTypeMatch:
		    matchInit (&list->value[i].match);
		    matchAddFromString (&list->value[i].match, item);
		    break;
		default:
		    break;
	    }
	}
	list->nValue = count;
    }

    if (item)
	free (item);

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
	fprintf(stderr, "Could not get HOME environmental variable\n");
	return FALSE;
    }
}

static Bool
iniLoadOptionsFromFile (CompDisplay *d,
			FILE *optionFile,
			char *plugin,
			int screen)
{
    char *optionName = NULL;
    char *optionValue = NULL;
    char *actionTest = NULL;
    char *actionTmp = NULL;
    char *realOption = NULL;
    char tmp[MAX_OPTION_LENGTH];
    CompOption *option = NULL, *o;
    int nOption;
    CompScreen *s = NULL;
    CompPlugin *p = NULL;
    Bool status = FALSE;
    Bool hv = FALSE;
    CompOptionValue value;

    if (plugin)
    {
	p = findActivePlugin (plugin);
	if (!p)
	{
	    fprintf(stderr, "Could not find running plugin " \
			    "%s (iniLoadOptionsFromFile)\n", plugin);
	    return FALSE;
	}
    }

    if (screen > -1)
    {
	for (s = d->screens; s; s = s->next)
	    if (s && s->screenNum == screen)
		break;

	if (!s)
	{
	    fprintf (stderr, "Invalid screen number passed to " \
			     "iniLoadOptionsFromFile %d\n", screen);
	    return FALSE;
	}
    }

    if (plugin && p)
    {
	if (s && p->vTable->getScreenOptions)
	{
	    option = (*p->vTable->getScreenOptions) (p, s, &nOption);
	}
	else if (p->vTable->getDisplayOptions)
	{
	    option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
	}
    }
    else
    {
	if (s)
	    option = compGetScreenOptions (s, &nOption);
	else
	    option = compGetDisplayOptions (d, &nOption);
    }

    IniActionProxy actionProxy;

    initActionProxy (&actionProxy);

    while (fgets (&tmp[0], MAX_OPTION_LENGTH, optionFile) != NULL)
    {
	status = FALSE;

	if (!iniParseLine (&tmp[0], &optionName, &optionValue))
	{
	    fprintf(stderr,
		    "Ignoring line '%s' in %s %i\n", tmp, plugin, screen);
	    continue;
	}

	if (option)
	{
	    o = compFindOption (option, nOption, optionName, 0);
	    if (o)
	    {
		if (actionProxy.nSet != 0)
		{
		    /* there is an action where a line is
		       missing / out of order.  realOption
		       should still be set from the last loop */

		    value.action.type       = actionProxy.type;
		    value.action.key        = actionProxy.key;
		    value.action.button     = actionProxy.button;
		    value.action.bell       = actionProxy.bell;
		    value.action.edgeMask   = actionProxy.edgeMask;
		    value.action.edgeButton = actionProxy.edgeButton;

		    if (plugin && p)
		    {
			if (s)
			    status = (*s->setScreenOptionForPlugin) (s,
								     plugin,
								     realOption,
								     &value);
			else
			    status = (*d->setDisplayOptionForPlugin) (d, plugin,
								      realOption,
								      &value);
		    }
		    else
		    {
			if (s)
			    status = (*s->setScreenOption) (s, realOption, &value);
			else
			    status = (*d->setDisplayOption) (d, realOption, &value);
		    }

		    initActionProxy (&actionProxy);
		}

		value = o->value;

		switch (o->type)
		{
		case CompOptionTypeBool:
		    hv = TRUE;
		    value.b = (Bool) atoi (optionValue);
			break;
		case CompOptionTypeInt:
		    hv = TRUE;
		    value.i = atoi (optionValue);
			break;
		case CompOptionTypeFloat:
		    hv = TRUE;
		    value.f = atof (optionValue);
			break;
		case CompOptionTypeString:
		    hv = TRUE;
		    value.s = strdup (optionValue);
			break;
		case CompOptionTypeColor:
		    hv = stringToColor (optionValue, value.c);
			break;
		case CompOptionTypeList:
		    hv = csvToList (optionValue, &value.list, value.list.type);
			break;
		case CompOptionTypeMatch:
		    hv = TRUE;
		    matchInit (&value.match);
		    matchAddFromString (&value.match, optionValue);
			break;
		default:
			break;
		}

		if (hv)
		{
		    if (plugin && p)
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
			}
			else
			{
			if (s)
			    status = (*s->setScreenOption)
						(s, optionName, &value);
			else
			    status = (*d->setDisplayOption)
						(d, optionName, &value);
		    }
		    if (o->type == CompOptionTypeMatch)
		    {
			matchFini (&value.match);
		    }
		}
	    }
	    else
	    {
		/* option not found, it might be an action option */
		actionTmp = strchr (optionName, '_');
		if (actionTmp)
		{
		    actionTmp++;  /* skip the _ */
		    actionTest = strchr (actionTmp, '_');
		    while (actionTest)
		    {
			actionTmp++;
			actionTest = strchr (actionTmp, '_');
			if (actionTest)
			    actionTmp = strchr (actionTmp, '_');
		    }

		    if (actionTmp)
		    {
			/* find the real option */
			int len = strlen (optionName) - strlen (actionTmp) - 1;
			realOption = realloc (realOption, sizeof (char) * len);
			strncpy (realOption, optionName, len);
			realOption[len] = '\0';
			o = compFindOption (option, nOption, realOption, 0);

			if (o)
			{
			    value = o->value;

			    if (strcmp (actionTmp, "key") == 0)
			    {
				if (!*optionValue ||
				    strcasecmp (optionValue, "disabled") == 0)
				{
				    actionProxy.type &= ~CompBindingTypeKey;
				}
				else
				{
				    if (stringToKeyBinding (d,
							     optionValue,
							     &actionProxy.key))
					actionProxy.type |= CompBindingTypeKey;
				}
				actionProxy.nSet++;
			    }
			    else if (strcmp (actionTmp, "button") == 0)
			    {
				if (!*optionValue ||
				    strcasecmp (optionValue, "disabled") == 0)
				{
				    actionProxy.type &= ~CompBindingTypeButton;
				}
				else
				{
				    if (stringToButtonBinding (d, optionValue,
								&actionProxy.button))
					actionProxy.type |= CompBindingTypeButton;
				}
				actionProxy.nSet++;
			    }
			    else if (strcmp (actionTmp, "edge") == 0)
			    {
				actionProxy.edgeMask = 0;
				if (optionValue[0] != '\0')
				{
				    int i, e;
				    CompListValue edges;

				    if (csvToList (optionValue, &edges, CompOptionTypeString))
				    {
					for (i = 0; i < SCREEN_EDGE_NUM; i++)
					{
					    for (e=0; e<edges.nValue; e++)
					    {
						if (strcasecmp (edges.value[e].s, edgeToString (i)) == 0)
						    actionProxy.edgeMask |= 1 << i;
					    }
					}
				    }
				}
				actionProxy.nSet++;
			    }
			    else if (strcmp (actionTmp, "edgebutton") == 0)
			    {
				actionProxy.edgeButton = atoi (optionValue);

				if (actionProxy.edgeButton)
				    actionProxy.type |= CompBindingTypeEdgeButton;
				else
				    actionProxy.type &= ~CompBindingTypeEdgeButton;

				actionProxy.nSet++;
			    }
			    else if (strcmp (actionTmp, "bell") == 0)
			    {
				actionProxy.bell = (Bool) atoi (optionValue);
				actionProxy.nSet++;
			    }

			    if (actionProxy.nSet >= N_ACTION_PARTS)
			    {
				value.action.type       = actionProxy.type;
				value.action.key        = actionProxy.key;
				value.action.button     = actionProxy.button;
				value.action.bell       = actionProxy.bell;
				value.action.edgeMask   = actionProxy.edgeMask;
				value.action.edgeButton = actionProxy.edgeButton;

				if (plugin)
				{
				    if (s)
					status = (*s->setScreenOptionForPlugin) (s,
										plugin,
										realOption,
										&value);
				    else
					status = (*d->setDisplayOptionForPlugin) (d, plugin,
										realOption,
										&value);
				}
				else
				{
				    if (s)
					status = (*s->setScreenOption) (s, realOption, &value);
				    else
					status = (*d->setDisplayOption) (d, realOption, &value);
				}

				initActionProxy (&actionProxy);
			    }
			}
		    }
		}
	    }
	}
    }

    if (optionName)
	free (optionName);
    if (optionValue)
	free (optionValue);
    if (realOption)
	free (realOption);

    return TRUE;
}

static Bool
iniSaveOptions (CompDisplay *d,
	        int         screen,
	        char        *plugin)
{
    CompScreen *s = NULL;
    CompOption *option;
    int	       nOption = 0;
    char       *filename, *directory, *fullPath, *strVal = NULL;

    if (screen > -1)
    {
	for (s = d->screens; s; s = s->next)
	    if (s && s->screenNum == screen)
		break;

	if (!s)
	{
	    fprintf (stderr, "Invalid screen number passed to " \
			     "iniSaveOptions %d\n", screen);
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
	/* core (general) setting */
	if (s)
	    option = compGetScreenOptions (s, &nOption);
	else
	    option = compGetDisplayOptions (d, &nOption);
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

    fullPath = malloc (sizeof(char) * (strlen(filename) + strlen(directory) + 2));
    if (!fullPath)
    {
	free (filename);
	free (directory);
	return FALSE;
    }

    sprintf(fullPath, "%s/%s", directory, filename);

    FILE *optionFile = fopen (fullPath, "w");

    if (!optionFile && iniMakeDirectories ())
	optionFile = fopen (fullPath, "w");

    if (!optionFile)
    {
	fprintf(stderr, "Failed to write to %s, check you " \
			"have the correct permissions\n", fullPath);
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
	strVal = strdup ("");
	int i;

	switch (option->type)
	{
	case CompOptionTypeBool:
	case CompOptionTypeInt:
	case CompOptionTypeFloat:
	case CompOptionTypeString:
	case CompOptionTypeColor:
	case CompOptionTypeMatch:
		strVal = iniOptionValueToString (&option->value, option->type);
		if (strVal[0] != '\0')
		    fprintf (optionFile, "%s=%s\n", option->name, strVal);
		else
		    fprintf (optionFile, "%s=\n", option->name);
		break;
	case CompOptionTypeAction:
	    firstInList = TRUE;
	    if (option->value.action.type & CompBindingTypeKey)
		strVal = keyBindingToString (d, &option->value.action.key);
	    fprintf (optionFile, "%s_%s=%s\n", option->name, "key", strVal);

	    strVal = strdup ("");
	    if (option->value.action.type & CompBindingTypeButton)
		strVal = buttonBindingToString (d, &option->value.action.button);
	    fprintf (optionFile, "%s_%s=%s\n", option->name, "button", strVal);

	    if (!(strVal = realloc (strVal, sizeof (char) * 32)))
		return FALSE;
	    sprintf(strVal, "%i", (int)option->value.action.bell);
	    fprintf (optionFile, "%s_%s=%s\n", option->name, "bell", strVal);

	    strVal = strdup ("");
	    for (i = 0; i < SCREEN_EDGE_NUM; i++)
	    {
		if (option->value.action.edgeMask & (1 << i))
		{
		    char listVal[MAX_OPTION_LENGTH];
		    strcpy (listVal, edgeToString (i));
		    if (!(strVal = realloc (strVal, MAX_OPTION_LENGTH)))
			return FALSE;

		    if (!firstInList)
			strVal = strcat (strVal, ",");
		    firstInList = FALSE;

		    if (listVal)
			strVal = strcat (strVal, listVal);
		}
	    }
	    fprintf (optionFile, "%s_%s=%s\n", option->name, "edge", strVal);

	    strVal = strdup ("");
	    if (option->value.action.type & CompBindingTypeEdgeButton)
		sprintf(strVal, "%i", option->value.action.edgeButton);
	    else
		sprintf(strVal, "%i", 0);

	    fprintf (optionFile, "%s_%s=%s\n", option->name, "edgebutton", strVal);

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
		if (option->value.list.nValue && 
		    !(strVal = realloc (strVal, sizeof (char) * MAX_OPTION_LENGTH * option->value.list.nValue)))
		    return FALSE;

		for (i = 0; i < option->value.list.nValue; i++)
		{
		    char listVal[MAX_OPTION_LENGTH];

		    strncpy (listVal, iniOptionValueToString (
						&option->value.list.value[i],
						option->value.list.type),
						MAX_OPTION_LENGTH);

		    if (!firstInList)
			strVal = strcat (strVal, ",");
		    firstInList = FALSE;

		    if (listVal)
			strVal = strcat (strVal, listVal);
		}
		if (strVal[0] != '\0')
		    fprintf (optionFile, "%s=%s\n", option->name, strVal);
		else
		    fprintf (optionFile, "%s=\n", option->name);
		break;
	    }
	    default:
		fprintf(stderr, "Unknown list option type %d, %s\n",
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

    if (strVal)
	free (strVal);
    free (filename);
    free (directory);
    free (fullPath);

    return TRUE;
}

static Bool
iniLoadOptions (CompDisplay *d,
	        int         screen,
	        char        *plugin)
{
    char         *filename, *directory, *fullPath;
    FILE         *optionFile;
    Bool         loadRes;
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

    fullPath = malloc (sizeof(char) * (strlen(filename) + strlen(directory) + 2));
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

	    if (!csvToList (DEFAULT_PLUGINS,
		            &value.list,
		            CompOptionTypeString))
	    {
		free (filename);
		free (directory);
		free (fullPath);
		return FALSE;
	    }

	    value.list.type = CompOptionTypeString;

	    fprintf(stderr, "Could not open main display config file %s\n", fullPath);
	    fprintf(stderr, "Loading default plugins (%s)\n", DEFAULT_PLUGINS);

	    (*d->setDisplayOption) (d, "active_plugins", &value);

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
	    fprintf(stderr, "Could not open config file %s - using " \
			    "defaults for %s\n", fullPath, (plugin)?plugin:"core");

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

    loadRes = iniLoadOptionsFromFile (d, optionFile, plugin, screen);

    fileData->blockWrites = FALSE;

    fclose (optionFile);

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
    tmp = fd;

    while (tmp && fd)
    {
	free (tmp);
	tmp = fd->next;
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
	fprintf(stderr, "Plugin %s failed to initialize " \
			"display settings\n", p->vTable->name);
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
	fprintf(stderr, "Plugin %s failed to initialize " \
			"screen %d settings\n", p->vTable->name, s->screenNum);
    }

    return status;
}

static Bool
iniSetScreenOption (CompScreen *s, char *name, CompOptionValue *value)
{
    Bool status;

    INI_SCREEN (s);

    UNWRAP (is, s, setScreenOption);
    status = (*s->setScreenOption) (s, name, value);
    WRAP (is, s, setScreenOption, iniSetScreenOption);

    if (status)
    {
	iniSaveOptions (s->display, s->screenNum, NULL);
    }

    return status;
}

static Bool
iniSetDisplayOption (CompDisplay *d, char *name, CompOptionValue *value)
{
    Bool status;

    INI_DISPLAY (d);

    UNWRAP (id, d, setDisplayOption);
    status = (*d->setDisplayOption) (d, name, value);
    WRAP (id, d, setDisplayOption, iniSetDisplayOption);

    if (status)
    {
	iniSaveOptions (d, -1, NULL);
    }

    return status;
}

static Bool
iniSetDisplayOptionForPlugin (CompDisplay     *d,
			      char	      *plugin,
			      char	      *name,
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
			     char	     *plugin,
			     char	     *name,
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
    WRAP (id, d, setDisplayOption, iniSetDisplayOption);
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
    UNWRAP (id, d, setDisplayOption);
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
    WRAP (is, s, setScreenOption, iniSetScreenOption);
    WRAP (is, s, setScreenOptionForPlugin, iniSetScreenOptionForPlugin);

    iniLoadOptions (s->display, s->screenNum, NULL);

    return TRUE;
}

static void
iniFiniScreen (CompPlugin *p, CompScreen *s)
{
    INI_SCREEN (s);

    UNWRAP (is, s, initPluginForScreen);
    UNWRAP (is, s, setScreenOption);
    UNWRAP (is, s, setScreenOptionForPlugin);

    free (is);
}

static Bool
iniInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
        return FALSE;

    return TRUE;
}

static void
iniFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
        freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
iniGetVersion (CompPlugin *plugin, int	version)
{
    return ABIVERSION;
}

CompPluginVTable iniVTable = {
    "ini",
    "Ini file backend for compiz",
    "Ini (flat file) option storage",
    iniGetVersion,
    iniInit,
    iniFini,
    iniInitDisplay,
    iniFiniDisplay,
    iniInitScreen,
    iniFiniScreen,
    0,
    0,
    0,
    0,
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
