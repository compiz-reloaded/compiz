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

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gconf/gconf-client.h>

#include <compiz-core.h>

static CompMetadata gconfMetadata;

#define APP_NAME "compiz"

/* From gconf-internal.h. Bleah. */
int gconf_value_compare (const GConfValue *value_a,
			 const GConfValue *value_b);

static int displayPrivateIndex;

typedef struct _GConfDisplay {
    int screenPrivateIndex;

    InitPluginForDisplayProc      initPluginForDisplay;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;

    GConfClient *client;

    CompTimeoutHandle reloadHandle;
} GConfDisplay;

typedef struct _GConfScreen {
    InitPluginForScreenProc      initPluginForScreen;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;
} GConfScreen;

#define GET_GCONF_DISPLAY(d)				      \
    ((GConfDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define GCONF_DISPLAY(d)		     \
    GConfDisplay *gd = GET_GCONF_DISPLAY (d)

#define GET_GCONF_SCREEN(s, gd)				         \
    ((GConfScreen *) (s)->privates[(gd)->screenPrivateIndex].ptr)

#define GCONF_SCREEN(s)						           \
    GConfScreen *gs = GET_GCONF_SCREEN (s, GET_GCONF_DISPLAY (s->display))

static GConfValueType
gconfTypeFromCompType (CompOptionType type)
{
    switch (type) {
    case CompOptionTypeBool:
    case CompOptionTypeBell:
	return GCONF_VALUE_BOOL;
    case CompOptionTypeInt:
	return GCONF_VALUE_INT;
    case CompOptionTypeFloat:
	return GCONF_VALUE_FLOAT;
    case CompOptionTypeString:
    case CompOptionTypeColor:
    case CompOptionTypeKey:
    case CompOptionTypeButton:
    case CompOptionTypeEdge:
    case CompOptionTypeMatch:
	return GCONF_VALUE_STRING;
    case CompOptionTypeList:
	return GCONF_VALUE_LIST;
    default:
	break;
    }

    return GCONF_VALUE_INVALID;
}

static void
gconfSetValue (CompDisplay     *d,
	       CompOptionValue *value,
	       CompOptionType  type,
	       GConfValue      *gvalue)
{
    switch (type) {
    case CompOptionTypeBool:
	gconf_value_set_bool (gvalue, value->b);
	break;
    case CompOptionTypeInt:
	gconf_value_set_int (gvalue, value->i);
	break;
    case CompOptionTypeFloat:
	gconf_value_set_float (gvalue, value->f);
	break;
    case CompOptionTypeString:
	gconf_value_set_string (gvalue, value->s);
	break;
    case CompOptionTypeColor: {
	gchar *color;

	color = colorToString (value->c);
	gconf_value_set_string (gvalue, color);

	free (color);
    } break;
    case CompOptionTypeKey: {
	gchar *action;

	action = keyActionToString (d, &value->action);
	gconf_value_set_string (gvalue, action);

	free (action);
    } break;
    case CompOptionTypeButton: {
	gchar *action;

	action = buttonActionToString (d, &value->action);
	gconf_value_set_string (gvalue, action);

	free (action);
    } break;
    case CompOptionTypeEdge: {
	gchar *edge;

	edge = edgeMaskToString (value->action.edgeMask);
	gconf_value_set_string (gvalue, edge);

	free (edge);
    } break;
    case CompOptionTypeBell:
	gconf_value_set_bool (gvalue, value->action.bell);
	break;
    case CompOptionTypeMatch: {
	gchar *match;

	match = matchToString (&value->match);
	gconf_value_set_string (gvalue, match);

	free (match);
    } break;
    default:
	break;
    }
}

static void
gconfSetOption (CompDisplay *d,
		CompOption  *o,
		const gchar *plugin,
		const gchar *object)
{
    GConfValueType type = gconfTypeFromCompType (o->type);
    GConfValue     *gvalue, *existingValue = NULL;
    gchar          *key;

    GCONF_DISPLAY (d);

    if (type == GCONF_VALUE_INVALID)
	return;

    if (strcmp (plugin, "core") == 0)
	key = g_strjoin ("/", "/apps", APP_NAME, "general", object, "options",
			 o->name, NULL);
    else
	key = g_strjoin ("/", "/apps", APP_NAME, "plugins", plugin, object,
			 "options", o->name, NULL);

    existingValue = gconf_client_get (gd->client, key, NULL);
    gvalue = gconf_value_new (type);

    if (o->type == CompOptionTypeList)
    {
	GSList     *node, *list = NULL;
	GConfValue *gv;
	int	   i;

	type = gconfTypeFromCompType (o->value.list.type);

	for (i = 0; i < o->value.list.nValue; i++)
	{
	    gv = gconf_value_new (type);
	    gconfSetValue (d, &o->value.list.value[i], o->value.list.type, gv);
	    list = g_slist_append (list, gv);
	}

	gconf_value_set_list_type (gvalue, type);
	gconf_value_set_list (gvalue, list);

	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gd->client, key, gvalue, NULL);

	for (node = list; node; node = node->next)
	    gconf_value_free ((GConfValue *) node->data);

	g_slist_free (list);
    }
    else
    {
	gconfSetValue (d, &o->value, o->type, gvalue);

	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gd->client, key, gvalue, NULL);
    }

    gconf_value_free (gvalue);

    if (existingValue)
	gconf_value_free (existingValue);

    g_free (key);
}

static Bool
gconfGetValue (CompDisplay     *d,
	       CompOptionValue *value,
	       CompOptionType  type,
	       GConfValue      *gvalue)

{
    if (type         == CompOptionTypeBool &&
	gvalue->type == GCONF_VALUE_BOOL)
    {
	value->b = gconf_value_get_bool (gvalue);
	return TRUE;
    }
    else if (type         == CompOptionTypeInt &&
	     gvalue->type == GCONF_VALUE_INT)
    {
	value->i = gconf_value_get_int (gvalue);
	return TRUE;
    }
    else if (type         == CompOptionTypeFloat &&
	     gvalue->type == GCONF_VALUE_FLOAT)
    {
	value->f = gconf_value_get_float (gvalue);
	return TRUE;
    }
    else if (type         == CompOptionTypeString &&
	     gvalue->type == GCONF_VALUE_STRING)
    {
	const char *str;

	str = gconf_value_get_string (gvalue);
	if (str)
	{
	    value->s = strdup (str);
	    if (value->s)
		return TRUE;
	}
    }
    else if (type         == CompOptionTypeColor &&
	     gvalue->type == GCONF_VALUE_STRING)
    {
	const gchar *color;

	color = gconf_value_get_string (gvalue);

	if (stringToColor (color, value->c))
	    return TRUE;
    }
    else if (type         == CompOptionTypeKey &&
	     gvalue->type == GCONF_VALUE_STRING)
    {
	const gchar *action;

	action = gconf_value_get_string (gvalue);

	stringToKeyAction (d, action, &value->action);
	return TRUE;
    }
    else if (type         == CompOptionTypeButton &&
	     gvalue->type == GCONF_VALUE_STRING)
    {
	const gchar *action;

	action = gconf_value_get_string (gvalue);

	stringToButtonAction (d, action, &value->action);
	return TRUE;
    }
    else if (type         == CompOptionTypeEdge &&
	     gvalue->type == GCONF_VALUE_STRING)
    {
	const gchar *edge;

	edge = gconf_value_get_string (gvalue);

	value->action.edgeMask = stringToEdgeMask (edge);
	return TRUE;
    }
    else if (type         == CompOptionTypeBell &&
	     gvalue->type == GCONF_VALUE_BOOL)
    {
	value->action.bell = gconf_value_get_bool (gvalue);
	return TRUE;
    }
    else if (type         == CompOptionTypeMatch &&
	     gvalue->type == GCONF_VALUE_STRING)
    {
	const gchar *match;

	match = gconf_value_get_string (gvalue);

	matchInit (&value->match);
	matchAddFromString (&value->match, match);
	return TRUE;
    }

    return FALSE;
}

static Bool
gconfReadOptionValue (CompDisplay     *d,
		      GConfEntry      *entry,
		      CompOption      *o,
		      CompOptionValue *value)
{
    GConfValue *gvalue;

    gvalue = gconf_entry_get_value (entry);
    if (!gvalue)
	return FALSE;

    compInitOptionValue (value);

    if (o->type      == CompOptionTypeList &&
	gvalue->type == GCONF_VALUE_LIST)
    {
	GConfValueType type;
	GSList	       *list;
	int	       i, n;

	type = gconf_value_get_list_type (gvalue);
	if (gconfTypeFromCompType (o->value.list.type) != type)
	    return FALSE;

	list = gconf_value_get_list (gvalue);
	n    = g_slist_length (list);

	if (n)
	{
	    value->list.value = malloc (sizeof (CompOptionValue) * n);
	    if (value->list.value)
	    {
		for (i = 0; i < n; i++)
		{
		    if (!gconfGetValue (d,
					&value->list.value[i],
					o->value.list.type,
					(GConfValue *) list->data))
			break;

		    value->list.nValue++;

		    list = g_slist_next (list);
		}

		if (value->list.nValue != n)
		{
		    compFiniOptionValue (value, o->type);
		    return FALSE;
		}
	    }
	}
    }
    else
    {
	if (!gconfGetValue (d, value, o->type, gvalue))
	    return FALSE;
    }

    gconf_entry_free (entry);

    return TRUE;
}

static void
gconfGetDisplayOption (CompDisplay *d,
		       CompOption  *o,
		       const char  *plugin)
{
    GConfEntry *entry;
    gchar      *key;

    GCONF_DISPLAY (d);

    if (strcmp (plugin, "core") == 0)
	key = g_strjoin ("/", "/apps", APP_NAME, "general", "allscreens",
			 "options", o->name, NULL);
    else
	key = g_strjoin ("/", "/apps", APP_NAME, "plugins", plugin,
			 "allscreens", "options", o->name, NULL);

    entry = gconf_client_get_entry (gd->client, key, NULL, TRUE, NULL);
    if (entry)
    {
	CompOptionValue value;

	if (gconfReadOptionValue (d, entry, o, &value))
	{
	    (*d->setDisplayOptionForPlugin) (d, plugin, o->name, &value);

	    compFiniOptionValue (&value, o->type);
	}
	else
	{
	    gconfSetOption (d, o, plugin, "allscreens");
	}
    }

    g_free (key);
}

static void
gconfGetScreenOption (CompScreen *s,
		      CompOption *o,
		      const char *plugin,
		      const char *screen)
{
    GConfEntry *entry;
    gchar      *key;

    GCONF_DISPLAY (s->display);

    if (strcmp (plugin, "core") == 0)
	key = g_strjoin ("/", "/apps", APP_NAME, "general", screen, "options",
			 o->name, NULL);
    else
	key = g_strjoin ("/", "/apps", APP_NAME, "plugins", plugin, screen,
			 "options", o->name, NULL);

    entry = gconf_client_get_entry (gd->client, key, NULL, TRUE, NULL);
    if (entry)
    {
	CompOptionValue value;

	if (gconfReadOptionValue (s->display, entry, o, &value))
	{
	    (*s->setScreenOptionForPlugin) (s, plugin, o->name, &value);
	    compFiniOptionValue (&value, o->type);
	}
	else
	{
	    gconfSetOption (s->display, o, plugin, screen);
	}
    }

    g_free (key);
}

static Bool
gconfReload (void *closure)
{
    CompDisplay *d = (CompDisplay *) closure;
    CompScreen  *s;
    CompPlugin  *p;
    CompOption  *option;
    int		nOption;

    GCONF_DISPLAY (d);

    for (p = getPlugins (); p; p = p->next)
    {
	if (!p->vTable->getDisplayOptions)
	    continue;

	option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
	while (nOption--)
	    gconfGetDisplayOption (d, option++, p->vTable->name);
    }

    for (s = d->screens; s; s = s->next)
    {
	gchar *screen = g_strdup_printf ("screen%d", s->screenNum);

	for (p = getPlugins (); p; p = p->next)
	{
	    if (!p->vTable->getScreenOptions)
		continue;

	    option = (*p->vTable->getScreenOptions) (p, s, &nOption);
	    while (nOption--)
		gconfGetScreenOption (s, option++, p->vTable->name, screen);
	}

	g_free (screen);
    }

    gd->reloadHandle = 0;

    return FALSE;
}

static Bool
gconfSetDisplayOptionForPlugin (CompDisplay     *d,
				const char	*plugin,
				const char	*name,
				CompOptionValue *value)
{
    Bool status;

    GCONF_DISPLAY (d);

    UNWRAP (gd, d, setDisplayOptionForPlugin);
    status = (*d->setDisplayOptionForPlugin) (d, plugin, name, value);
    WRAP (gd, d, setDisplayOptionForPlugin, gconfSetDisplayOptionForPlugin);

    if (status && !gd->reloadHandle)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getDisplayOptions)
	{
	    CompOption *option;
	    int	       nOption;

	    option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
	    option = compFindOption (option, nOption, name, 0);
	    if (option)
		gconfSetOption (d, option, p->vTable->name, "allscreens");
	}
    }

    return status;
}

static Bool
gconfSetScreenOptionForPlugin (CompScreen      *s,
			       const char      *plugin,
			       const char      *name,
			       CompOptionValue *value)
{
    Bool status;

    GCONF_SCREEN (s);

    UNWRAP (gs, s, setScreenOptionForPlugin);
    status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
    WRAP (gs, s, setScreenOptionForPlugin, gconfSetScreenOptionForPlugin);

    if (status)
    {
	GCONF_DISPLAY (s->display);

	if (!gd->reloadHandle)
	{
	    CompPlugin *p;

	    p = findActivePlugin (plugin);
	    if (p && p->vTable->getScreenOptions)
	    {
		CompOption *option;
		int	   nOption;
		gchar      *screen;

		screen = g_strdup_printf ("screen%d", s->screenNum);

		option = (*p->vTable->getScreenOptions) (p, s, &nOption);
		option = compFindOption (option, nOption, name, 0);
		if (option)
		    gconfSetOption (s->display, option, plugin, screen);

		g_free (screen);
	    }
	}
    }

    return status;
}

static Bool
gconfInitPluginForDisplay (CompPlugin  *p,
			   CompDisplay *d)
{
    Bool status;

    GCONF_DISPLAY (d);

    UNWRAP (gd, d, initPluginForDisplay);
    status = (*d->initPluginForDisplay) (p, d);
    WRAP (gd, d, initPluginForDisplay, gconfInitPluginForDisplay);

    if (status && p->vTable->getDisplayOptions)
    {
	CompOption *option;
	int	   nOption;

	option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
	while (nOption--)
	    gconfGetDisplayOption (d, option++, p->vTable->name);
    }

    return status;
}

static Bool
gconfInitPluginForScreen (CompPlugin *p,
			  CompScreen *s)
{
    Bool status;

    GCONF_SCREEN (s);

    UNWRAP (gs, s, initPluginForScreen);
    status = (*s->initPluginForScreen) (p, s);
    WRAP (gs, s, initPluginForScreen, gconfInitPluginForScreen);

    if (status && p->vTable->getScreenOptions)
    {
	CompOption *option;
	int	   nOption;
	gchar      *screen;

	screen = g_strdup_printf ("screen%d", s->screenNum);

	option = (*p->vTable->getScreenOptions) (p, s, &nOption);
	while (nOption--)
	    gconfGetScreenOption (s, option++, p->vTable->name, screen);

	g_free (screen);
    }

    return status;
}

static void
gconfKeyChanged (GConfClient *client,
		 guint	     cnxn_id,
		 GConfEntry  *entry,
		 gpointer    user_data)
{
    CompDisplay *display = (CompDisplay *) user_data;
    CompScreen  *screen = NULL;
    CompPlugin  *plugin = NULL;
    CompOption  *option = NULL;
    int		nOption = 0;
    gchar	**token;
    int		object = 4;

    token = g_strsplit (entry->key, "/", 8);

    if (g_strv_length (token) < 7)
    {
	g_strfreev (token);
	return;
    }

    if (strcmp (token[0], "")	    != 0 ||
	strcmp (token[1], "apps")   != 0 ||
	strcmp (token[2], APP_NAME) != 0)
    {
	g_strfreev (token);
	return;
    }

    if (strcmp (token[3], "general") == 0)
    {
	plugin = findActivePlugin ("core");
    }
    else
    {
	if (strcmp (token[3], "plugins") != 0 || g_strv_length (token) < 8)
	{
	    g_strfreev (token);
	    return;
	}

	object = 5;
	plugin = findActivePlugin (token[4]);
    }

    if (!plugin)
    {
	g_strfreev (token);
	return;
    }

    if (strcmp (token[object], "allscreens") != 0)
    {
	int screenNum;

	if (sscanf (token[object], "screen%d", &screenNum) != 1)
	{
	    g_strfreev (token);
	    return;
	}

	for (screen = display->screens; screen; screen = screen->next)
	    if (screen->screenNum == screenNum)
		break;

	if (!screen)
	{
	    g_strfreev (token);
	    return;
	}
    }

    if (strcmp (token[object + 1], "options") != 0)
    {
	g_strfreev (token);
	return;
    }

    if (screen)
    {
	if (plugin->vTable->getScreenOptions)
	    option = (*plugin->vTable->getScreenOptions) (plugin, screen,
							  &nOption);

	option = compFindOption (option, nOption, token[object + 2], 0);
	if (option)
	{
	    CompOptionValue value;

	    if (gconfReadOptionValue (display, entry, option, &value))
	    {
		(*screen->setScreenOptionForPlugin) (screen,
						     plugin->vTable->name,
						     option->name,
						     &value);

		compFiniOptionValue (&value, option->type);
	    }
	}
    }
    else
    {
	if (plugin->vTable->getDisplayOptions)
	    option = (*plugin->vTable->getDisplayOptions) (plugin, display,
							   &nOption);

	option = compFindOption (option, nOption, token[object + 2], 0);
	if (option)
	{
	    CompOptionValue value;

	    if (gconfReadOptionValue (display, entry, option, &value))
	    {
		(*display->setDisplayOptionForPlugin) (display,
						       plugin->vTable->name,
						       option->name,
						       &value);

		compFiniOptionValue (&value, option->type);
	    }
	}
    }

    g_strfreev (token);
}

static void
gconfSendGLibNotify (CompDisplay *d)
{
    Display *dpy = d->display;
    XEvent  xev;

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = dpy;
    xev.xclient.format  = 32;

    xev.xclient.message_type = XInternAtom (dpy, "_COMPIZ_GLIB_NOTIFY", 0);
    xev.xclient.window	     = d->screens->root;

    memset (xev.xclient.data.l, 0, sizeof (xev.xclient.data.l));

    XSendEvent (dpy,
		d->screens->root,
		FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xev);
}

static Bool
gconfInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    GConfDisplay *gd;

    if (!checkPluginABI ("core", ABIVERSION))
	return FALSE;

    gd = malloc (sizeof (GConfDisplay));
    if (!gd)
	return FALSE;

    gd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (gd->screenPrivateIndex < 0)
    {
	free (gd);
	return FALSE;
    }

    g_type_init ();

    gd->client = gconf_client_get_default ();

    gconf_client_add_dir (gd->client, "/apps/" APP_NAME,
			  GCONF_CLIENT_PRELOAD_NONE, NULL);

    gd->reloadHandle = compAddTimeout (0, gconfReload, (void *) d);

    WRAP (gd, d, initPluginForDisplay, gconfInitPluginForDisplay);
    WRAP (gd, d, setDisplayOptionForPlugin, gconfSetDisplayOptionForPlugin);

    d->privates[displayPrivateIndex].ptr = gd;

    gconf_client_notify_add (gd->client, "/apps/" APP_NAME, gconfKeyChanged,
			     d, NULL, NULL);

    gconfSendGLibNotify (d);

    return TRUE;
}

static void
gconfFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    GCONF_DISPLAY (d);

    if (gd->reloadHandle)
	compRemoveTimeout (gd->reloadHandle);

    g_object_unref (gd->client);

    UNWRAP (gd, d, initPluginForDisplay);
    UNWRAP (gd, d, setDisplayOptionForPlugin);

    freeScreenPrivateIndex (d, gd->screenPrivateIndex);

    free (gd);
}

static Bool
gconfInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    GConfScreen *gs;

    GCONF_DISPLAY (s->display);

    gs = malloc (sizeof (GConfScreen));
    if (!gs)
	return FALSE;

    WRAP (gs, s, initPluginForScreen, gconfInitPluginForScreen);
    WRAP (gs, s, setScreenOptionForPlugin, gconfSetScreenOptionForPlugin);

    s->privates[gd->screenPrivateIndex].ptr = gs;

    return TRUE;
}

static void
gconfFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    GCONF_SCREEN (s);

    UNWRAP (gs, s, initPluginForScreen);
    UNWRAP (gs, s, setScreenOptionForPlugin);

    free (gs);
}

static Bool
gconfInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&gconfMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&gconfMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&gconfMetadata, p->vTable->name);

    return TRUE;
}

static void
gconfFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&gconfMetadata);
}

static CompMetadata *
gconfGetMetadata (CompPlugin *plugin)
{
    return &gconfMetadata;
}

CompPluginVTable gconfVTable = {
    "gconf",
    gconfGetMetadata,
    gconfInit,
    gconfFini,
    gconfInitDisplay,
    gconfFiniDisplay,
    gconfInitScreen,
    gconfFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0  /* SetScreenOption */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &gconfVTable;
}
