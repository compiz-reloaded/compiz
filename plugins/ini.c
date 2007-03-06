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
    int			 pendingWrites;
};

typedef struct _IniDisplay {
    int		                  screenPrivateIndex;

    CompFileWatchHandle		  directoryWatch;

    InitPluginForDisplayProc      initPluginForDisplay;
    SetDisplayOptionProc	  setDisplayOption;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;
} IniDisplay;

typedef struct _IniScreen {
    InitPluginForScreenProc        initPluginForScreen;
    SetScreenOptionProc		   setScreenOption;
    SetScreenOptionForPluginProc   setScreenOptionForPlugin;
} IniScreen;

static Bool
iniGetFileDataFromFilename (CompDisplay *d,
			    const char *filename,
			    IniFileData **fileData)
{
    int len, i;
    int pluginSep = 0, screenSep = 0;
    char *pluginStr, *screenStr;

    if (!filename)
	return FALSE;

    len = strlen (filename);

    if (len < 6)
	return FALSE;

    if ((filename[0]=='.') || (filename[len-1]=='~'))
	return FALSE;

    (*fileData)->filename = strdup (filename);

    for (i=0; i<len; i++)
    {
	if (filename[i] == '-')
	{
	    if (!pluginSep)
		pluginSep = i-1;
	    else
		return FALSE; /*found a second dash */
	}
	else if (filename[i] == '.')
	{
	    if (!screenSep)
		screenSep = i-1;
	    else
		return FALSE; /*found a second dot */
	}
    }

    if (!pluginSep || !screenSep)
	return FALSE;

    pluginStr = malloc (sizeof (char) * pluginSep + 1);
    screenStr = malloc (sizeof (char) * (screenSep - pluginSep) + 1);

    if (!pluginStr || !screenStr)
	return FALSE;

    strncpy (pluginStr, filename, pluginSep + 1);
    pluginStr[pluginSep + 1] = '\0';

    strncpy (screenStr, &filename[pluginSep+2], (screenSep - pluginSep) + 1);
    screenStr[(screenSep - pluginSep) -1] = '\0';

    if (strcmp (pluginStr, CORE_NAME) == 0)
	(*fileData)->plugin = NULL;
    else
	(*fileData)->plugin = strdup (pluginStr);

    if (strcmp (screenStr, "allscreens") == 0)
	(*fileData)->screen = -1;
    else
	(*fileData)->screen = atoi(&screenStr[6]);

    free (pluginStr);
    free (screenStr);

    return TRUE;
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
iniGetFilename (CompDisplay *d,
		int screen,
		char *plugin,
		char **filename)
{
    CompScreen *s;
    int	       len;
    char       *fn = NULL;
    char       *screenStr;
    char       *homeDir, *baseDir;

    homeDir = getenv ("HOME");
    if (homeDir)
    {
	baseDir = malloc (strlen (homeDir) + strlen (HOME_OPTION_DIR) + 3);
	if (baseDir)
	    sprintf (baseDir, "%s/%s", homeDir, HOME_OPTION_DIR);
    }
    else
    {
	fprintf(stderr, "Could not get HOME environmental variable\n");
	return FALSE;
    }

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

    /* add extra for allscreens/screen0/.conf etc */
    len = strlen (baseDir) + 27;

    if (plugin)
	len += strlen (plugin);
    else
	len += strlen (CORE_NAME);

    fn = realloc (fn, sizeof (char) * len);
    if (!fn)
	return FALSE;

    if (plugin)
    {
	sprintf (fn, "%s/%s-%s%s", baseDir,
		 plugin, screenStr, FILE_SUFFIX);
    }
    else
    {
	sprintf (fn, "%s/%s-%s%s", baseDir,
		 CORE_NAME, screenStr, FILE_SUFFIX);
    }

    if (baseDir)
	free (baseDir);
    free (screenStr);

    if (fn)
    {
	*filename = strdup (fn);
	free (fn);
	return TRUE;
    }

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
    char *csvtmp;
    csvtmp = strdup (csv);
    int count = 1;

    if (csv[0] == '\0')
    {
	list->nValue = 0;
	return FALSE;
    }

    csvtmp = strchr (csv, ',');

    while (csvtmp)
    {
	csvtmp++;  /* avoid the comma */
	count++;
	csvtmp = strchr (csvtmp, ',');
    }

    csvtmp = strdup (csv);

    int i, itemLength;
    char *split;
    char *item = NULL;

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
iniMakeDirectories (int screen)
{
    char *homeDir, *baseDir;
    baseDir = NULL;
    homeDir = getenv ("HOME");
    if (homeDir)
    {
	baseDir = malloc (sizeof (char) * (strlen (homeDir) + strlen (HOME_OPTION_DIR) + 3));
	if (baseDir)
	    sprintf (baseDir, "%s/%s", homeDir, HOME_OPTION_DIR);
	else
	    return FALSE;
    }
    else
    {
	fprintf(stderr, "Could not get HOME environmental variable\n");
	return FALSE;
    }

    mkdir (baseDir, 0700);

    free (baseDir);

    return TRUE;
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
    CompOption *option, *o;
    int nOption;
    CompScreen *s = NULL;
    CompPlugin *p;
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


    while (fgets (&tmp[0], MAX_OPTION_LENGTH, optionFile) != NULL)
    {
	status = FALSE;

	if (!iniParseLine (&tmp[0], &optionName, &optionValue))
	{
	    fprintf(stderr,
		    "Ignoring line '%s' in %s %i\n", tmp, plugin, screen);
	    continue;
	}

	if (plugin && p)
	{
	    if (s)
	    {
		if (p->vTable->getScreenOptions)
		    option = (*p->vTable->getScreenOptions) (s, &nOption);
	    }
	    else
	    {
		if (p->vTable->getDisplayOptions)
		    option = (*p->vTable->getDisplayOptions) (d, &nOption);
	    }
	}
	else
	{
	    if (s)
		option = compGetScreenOptions (s, &nOption);
	    else
		option = compGetDisplayOptions (d, &nOption);
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
		actionTmp = strdup (optionName);
		actionTmp = strchr (actionTmp, '_');
		if (actionTmp)
		{
		    actionTmp++;  /* skip the _ */
		    actionTest = strchr (actionTmp, '_');
		    while (actionTest && actionTmp)
		    {
			actionTmp++;
			actionTest = strchr (actionTmp, '_');
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
					value.action.type &= ~CompBindingTypeKey;
					hv = TRUE;
				}
				else
				{
					value.action.type |= CompBindingTypeKey;
					hv = stringToKeyBinding (d,
								 optionValue,
								 &value.action.key);
				}
			    }
			    else if (strcmp (actionTmp, "button") == 0)
			    {
				if (!*optionValue ||
				    strcasecmp (optionValue, "disabled") == 0)
				{
					value.action.type &= ~CompBindingTypeButton;
					hv = TRUE;
				}
				else
				{
					value.action.type |= CompBindingTypeButton;
					hv = stringToButtonBinding (d, optionValue,
								    &value.action.button);
				}
			    }
			    else if (strcmp (actionTmp, "edge") == 0)
			    {
				value.action.edgeMask = 0;
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
						    value.action.edgeMask |= 1 << i;
					    }
					}
				    }
				}
				hv = TRUE;
			    }
			    else if (strcmp (actionTmp, "edgebutton") == 0)
			    {
				value.action.edgeButton = atoi (optionValue);

				if (value.action.edgeButton)
				    value.action.type |= CompBindingTypeEdgeButton;
				else
				    value.action.type &= ~CompBindingTypeEdgeButton;

				hv = TRUE;
			    }
			    else if (strcmp (actionTmp, "bell") == 0)
			    {
				value.action.bell = (Bool) atoi (optionValue);
				hv = TRUE;
			    }

			    if (hv)
			    {
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
    char       *filename, *strVal = NULL;

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
	    option = (*p->vTable->getScreenOptions) (s, &nOption);
	else
	    option = (*p->vTable->getDisplayOptions) (d, &nOption);
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
    fileData = malloc (sizeof (IniFileData));
    if (!fileData)
	return FALSE;

    Bool fileMatch = iniGetFileDataFromFilename (d, filename, &fileData);
    if (fileMatch)
	fileData->pendingWrites++;

    FILE *optionFile = fopen (filename, "w");
    if (!optionFile)
    {
	fprintf(stderr, "Failed to write to %s, check you " \
			"have the correct permissions\n", filename);
	free (filename);
	return FALSE;
    }

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
		for (i = 0; i < option->value.list.nValue; i++)
		{
		    char listVal[MAX_OPTION_LENGTH];

		    strncpy (listVal, iniOptionValueToString (
						&option->value.list.value[i],
						option->value.list.type),
						MAX_OPTION_LENGTH);

		    if (!(strVal = realloc (strVal, 2048)))
			return FALSE;

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

    fclose (optionFile);

    if (strVal)
	free (strVal);
    if (filename)
	free (filename);
    if (fileData)
	free (fileData);

    return TRUE;
}

static Bool
iniLoadOptions (CompDisplay *d,
	        int         screen,
	        char        *plugin)
{
    char         *filename;
    FILE         *optionFile;
    Bool         loadRes;

    if (!iniGetFilename (d, screen, plugin, &filename))
	return FALSE;

    optionFile = fopen (filename, "r");

    if (!optionFile && iniMakeDirectories (screen))
	optionFile = fopen (filename, "r");

    if (!optionFile)
    {
	if (!plugin && (screen == -1))
	{
	    CompOptionValue value;
	    value.list.value = malloc (NUM_DEFAULT_PLUGINS * sizeof (CompListValue));
	    if (!value.list.value)
		return FALSE;

	    if (!csvToList (DEFAULT_PLUGINS,
		            &value.list,
		            CompOptionTypeString))
	    {
		return FALSE;
	    }

	    value.list.type = CompOptionTypeString;

	    fprintf(stderr, "Could not open main display config file %s\n", filename);
	    fprintf(stderr, "Loading default plugins (%s)\n", DEFAULT_PLUGINS);

	    (*d->setDisplayOption) (d, "active_plugins", &value);

	    free (value.list.value);
	    return FALSE;
	}
	else
	{
	    fprintf(stderr, "Could not open config file %s - using " \
			    "defaults for %s\n", filename, (plugin)?plugin:"core");

	    iniSaveOptions (d, screen, plugin);

	    optionFile = fopen (filename, "r");
	    if (!optionFile)
	    {
		if (filename)
		    free (filename);
		return FALSE;
	    }
	}
    }

    loadRes = iniLoadOptionsFromFile (d, optionFile, plugin, screen);

    if (filename)
	free (filename);

    fclose (optionFile);

    return TRUE;
}

static void
iniFileModified (const char *name,
		 void       *closure)
{
    CompDisplay *d;
    IniFileData *fd;

    fd = malloc (sizeof (IniFileData));
    if (!fd)
	return;

    d = (CompDisplay *) closure;

    if (iniGetFileDataFromFilename (d, name, &fd))
    {
	iniLoadOptions (d, fd->screen, fd->plugin);
    }

    free (fd);
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
	{
	    CompOption *option;
	    int	       nOption;

	    option = (*p->vTable->getScreenOptions) (s, &nOption);

	    iniSaveOptions (s->display, s->screenNum, plugin);
	}
    }

    return status;
}

static Bool
iniInitDisplay (CompPlugin *p, CompDisplay *d)
{
    IniDisplay *id;
    char *homeDir, *baseDir;

    id = malloc (sizeof (IniDisplay));
    if (!id)
        return FALSE;

    id->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (id->screenPrivateIndex < 0)
    {
        free (id);
        return FALSE;
    }

    WRAP (id, d, initPluginForDisplay, iniInitPluginForDisplay);
    WRAP (id, d, setDisplayOption, iniSetDisplayOption);
    WRAP (id, d, setDisplayOptionForPlugin, iniSetDisplayOptionForPlugin);

    d->privates[displayPrivateIndex].ptr = id;

    iniLoadOptions (d, -1, NULL);

    homeDir = getenv ("HOME");
    if (homeDir)
    {
	baseDir = malloc (strlen (homeDir) + strlen (HOME_OPTION_DIR) + 3);
	if (baseDir)
	{
	    sprintf (baseDir, "%s/%s", homeDir, HOME_OPTION_DIR);
	    id->directoryWatch = addFileWatch (d, baseDir,
					       NOTIFY_DELETE_MASK |
					       NOTIFY_CREATE_MASK |
					       NOTIFY_MODIFY_MASK,
					       iniFileModified, d);
	}
    }

    if (baseDir)
	free (baseDir);

    return TRUE;
}

static void
iniFiniDisplay (CompPlugin *p, CompDisplay *d)
{
    INI_DISPLAY (d);

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
