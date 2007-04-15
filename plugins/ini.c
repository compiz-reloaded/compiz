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
    SetDisplayOptionProc	  setDisplayOption;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;

    IniFileData			*fileData;
} IniDisplay;

/*
 * IniScreeen
 */
typedef struct _IniScreen {
    InitPluginForScreenProc        initPluginForScreen;
    SetScreenOptionProc		   setScreenOption;
    SetScreenOptionForPluginProc   setScreenOptionForPlugin;
} IniScreen;

/*
 * IniAction
 */
static char * validActionTypes[] = {
	"key",
	"button",
	"bell",
	"edge",
	"edgebutton"};

#define ACTION_VALUE_KEY	    (1 << 0)
#define ACTION_VALUE_BUTTON	    (1 << 1)
#define ACTION_VALUE_BELL	    (1 << 2)
#define ACTION_VALUE_EDGE	    (1 << 3)
#define ACTION_VALUE_EDGEBUTTON	    (1 << 4)
#define ACTION_VALUES_ALL \
	( ACTION_VALUE_KEY \
	| ACTION_VALUE_BUTTON \
	| ACTION_VALUE_BELL \
	| ACTION_VALUE_EDGE \
	| ACTION_VALUE_EDGEBUTTON )

static int actionValueMasks[] = {
    ACTION_VALUE_KEY,
    ACTION_VALUE_BUTTON,
    ACTION_VALUE_BELL,
    ACTION_VALUE_EDGE,
    ACTION_VALUE_EDGEBUTTON
};

enum {
    ACTION_TYPE_KEY = 0,
    ACTION_TYPE_BUTTON,
    ACTION_TYPE_BELL,
    ACTION_TYPE_EDGE,
    ACTION_TYPE_EDGEBUTTON,
    ACTION_TYPES_NUM
};

typedef struct _IniAction {
    char *realOptionName;
    unsigned int valueMasks;
    CompAction a;
} IniAction;


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
    char *split_pos;
    int length;

    if (line[0] == '\0' || line[0] == '\n')
	return FALSE;

    split_pos = strchr(line, '=');
    if (!split_pos)
	return FALSE;

    length = strlen(line) - strlen(split_pos);
    *optionName = strndup(line, length);
    split_pos++;
    *optionValue = strndup(split_pos, strlen(split_pos)-1);

    return TRUE;
}

static Bool
csvToList (char *csv, CompListValue *list, CompOptionType type)
{
    char *splitStart = NULL;
    char *splitEnd = NULL;
    char *item = NULL;
    int itemLength;
    int count;
    int i;

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
	fprintf(stderr, "Could not get HOME environmental variable\n");
	return FALSE;
    }
}

static Bool
findActionType(char *optionName, int *type)
{ 
    char * optionType = strrchr (optionName, '_');
    if (!optionType)
	return FALSE;

    optionType++; /* skip the '_' */

    int i;
    for (i = 0; i < ACTION_TYPES_NUM; i++)
    {
	if (strcmp (optionType, validActionTypes[i]) == 0)
	{
	    if (type)
		*type = i;
	    return TRUE;
	}
    }

    return FALSE;
}

static Bool
parseAction(CompDisplay *d, char *optionName, char *optionValue, IniAction *action)
{ 
    int type;

    if (!findActionType (optionName, &type))
	return FALSE; /* no action, exit the loop */

    /* we have a new action */
    if (!action->realOptionName)
    {
	char *optionType = strrchr (optionName, '_');
	/* chars until the last "_" */
	int len = strlen (optionName) - strlen (optionType);
	
	action->realOptionName = malloc (sizeof (char) * (len+1));
	strncpy (action->realOptionName, optionName, len);
	action->realOptionName[len] = '\0';

	/* make sure all defaults are set */
	action->a.type = 0;
	action->a.key.keycode = 0;
	action->a.key.modifiers = 0;
	action->a.button.button = 0;
	action->a.button.modifiers = 0;
	action->a.bell = FALSE;
	action->a.edgeMask = 0;
	action->a.edgeButton = 0;
	action->valueMasks = 0;
    }
    /* detect a new option (might happen when the options are incomplete) */
    else if (action->valueMasks != ACTION_VALUES_ALL)
    {
	char *optionType = strrchr (optionName, '_');
	/* chars until the last "_" */
	int len = strlen (optionName) - strlen (optionType);
	
	char *realOptionName = malloc (sizeof (char) * (len+1));
	strncpy (realOptionName, optionName, len);
	realOptionName[len] = '\0';

	if (strcmp (action->realOptionName, realOptionName) != 0)
	{
	    free (realOptionName);
	    return FALSE;
	}
	
	free (realOptionName);
    } 

    int i, j;
    CompListValue edges;
    switch (type)
    {
	case ACTION_TYPE_KEY: 
	    if (optionValue[0] != '\0' &&
		strcasecmp (optionValue, "disabled") != 0 &&
		stringToKeyBinding (d, optionValue, &action->a.key))
		action->a.type |= CompBindingTypeKey;
	    break;

	case ACTION_TYPE_BUTTON:
	    if (optionValue[0] != '\0' &&
		strcasecmp (optionValue, "disabled") != 0 &&
		stringToButtonBinding (d, optionValue, &action->a.button))
		action->a.type |= CompBindingTypeButton;
	    break;

	case ACTION_TYPE_BELL:
	    action->a.bell  = (Bool) atoi (optionValue);
	    break;

	case ACTION_TYPE_EDGE:
	    if (optionValue[0] != '\0' &&
		csvToList (optionValue, &edges, CompOptionTypeString))
	    {
		for (i = 0; i < edges.nValue; i++)
		{
		    for (j = 0; j < SCREEN_EDGE_NUM; j++)
		    {
			if (strcasecmp (edges.value[i].s, edgeToString(j)) == 0)
			{
			    action->a.edgeMask |= (1 << j);

			    /* found corresponding mask, next value */
			    break; 
			}
		    }
		}
	    }
	    break;

	case ACTION_TYPE_EDGEBUTTON:
	    action->a.edgeButton = atoi (optionValue);
	    if (action->a.edgeButton != 0)
		action->a.type |= CompBindingTypeEdgeButton;
	    break;

	default:
	    break;
    }

    action->valueMasks |= actionValueMasks[type];

    /* no need to read any further since all value are set */
    if (action->valueMasks == ACTION_VALUES_ALL)
	return FALSE;

    return TRUE; /* continue loop, not finished parsing yet */
}

static Bool
iniLoadOptionsFromFile (CompDisplay *d,
			FILE *optionFile,
			char *plugin,
			int screen)
{
    char *optionName = NULL;
    char *optionValue = NULL;
    char tmp[MAX_OPTION_LENGTH];
    CompOption *option = NULL, *o;
    int nOption;
    CompScreen *s = NULL;
    CompPlugin *p = NULL;
    Bool status = FALSE;
    Bool hasValue = FALSE;
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

    IniAction action;
    action.realOptionName = NULL;
    Bool continueReading;
    while (fgets (tmp, MAX_OPTION_LENGTH, optionFile) != NULL)
    {
	status = FALSE;
	continueReading = FALSE;

	if (!iniParseLine (tmp, &optionName, &optionValue))
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
		case CompOptionTypeList:
		    hasValue = csvToList (optionValue, &value.list, value.list.type);
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
		/* an action has several values, so we need
		   to read more then one line into our buffer */
		continueReading = parseAction(d, optionName, optionValue, &action);
	    }

	    /* parsing action finished, write it */
	    if (action.realOptionName &&
		!continueReading)
	    {
		CompOption *realOption = compFindOption (option, nOption, action.realOptionName, 0);
		if (realOption)
		{
		    value = realOption->value;

		    value.action.type = action.a.type;
		    value.action.key = action.a.key;
		    value.action.button = action.a.button;
		    value.action.bell = action.a.bell;
		    value.action.edgeMask = action.a.edgeMask;
		    value.action.edgeButton = action.a.edgeButton;

		    if (plugin)
		    {
			if (s)
			    status = (*s->setScreenOptionForPlugin) (s, plugin, action.realOptionName, &value);
			else
			    status = (*d->setDisplayOptionForPlugin) (d, plugin, action.realOptionName, &value);
		    }
		    else
		    {
			if (s)
			    status = (*s->setScreenOption) (s, action.realOptionName, &value);
			else
			    status = (*d->setDisplayOption) (d, action.realOptionName, &value);
		    }

		    /* clear the buffer */
		    free(action.realOptionName);
		    action.realOptionName = NULL;

		    /* we missed the current line because we exited it in the first call.
		       we also need to check wether we have a incomplete options here,
		       because otherwise parsing the last line again, would cause real
		       trouble. ;-) */
		    if (!o && action.valueMasks != ACTION_VALUES_ALL)
		        parseAction(d, optionName, optionValue, &action);
		}
	    }
	}

	/* clear up */
	if (optionName)
	    free (optionName);
	if (optionValue)
	    free (optionValue);
    } 

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
	fprintf (stderr, "Failed to write to %s, check you " \
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
		if (strVal)
		{
		    fprintf (optionFile, "%s=%s\n", option->name, strVal);
		    free (strVal);
		}
		else
		    fprintf (optionFile, "%s=\n", option->name);
		break;
	case CompOptionTypeAction:
	    firstInList = TRUE;
	    if (option->value.action.type & CompBindingTypeKey)
		strVal = keyBindingToString (d, &option->value.action.key);
	    else
		strVal = strdup ("");
	    fprintf (optionFile, "%s_%s=%s\n", option->name, "key", strVal);
	    free (strVal);

	    if (option->value.action.type & CompBindingTypeButton)
		strVal = buttonBindingToString (d, &option->value.action.button);
	    else
		strVal = strdup ("");
	    fprintf (optionFile, "%s_%s=%s\n", option->name, "button", strVal);
	    free (strVal);

	    asprintf(&strVal, "%i", (int)option->value.action.bell);
	    fprintf (optionFile, "%s_%s=%s\n", option->name, "bell", strVal);
	    free (strVal);

	    strVal = malloc (sizeof(char) * MAX_OPTION_LENGTH);
	    strcpy (strVal, "");
	    firstInList = TRUE;
	    for (i = 0; i < SCREEN_EDGE_NUM; i++)
	    {
		if (option->value.action.edgeMask & (1 << i))
		{
		    if (!firstInList)
		    	strncat (strVal, ",", MAX_OPTION_LENGTH);
		    firstInList = FALSE;

		    strncat (strVal, edgeToString (i), MAX_OPTION_LENGTH);
		}
	    }
	    fprintf (optionFile, "%s_%s=%s\n", option->name, "edge", strVal);
	    free (strVal);

	    if (option->value.action.type & CompBindingTypeEdgeButton)
		asprintf (&strVal, "%i", option->value.action.edgeButton);
	    else
		asprintf (&strVal, "%i", 0);

	    fprintf (optionFile, "%s_%s=%s\n", option->name, "edgebutton", strVal);
	    free (strVal);
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
		if (!strVal)
		    return FALSE;
		strcpy (strVal, "");
		firstInList = TRUE;

		for (i = 0; i < option->value.list.nValue; i++)
		{
		    itemVal = iniOptionValueToString (
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
		fprintf (stderr, "Unknown list option type %d, %s\n",
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
