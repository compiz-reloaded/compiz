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

#include <compiz.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gconf/gconf-client.h>

static CompMetadata gconfMetadata;

#define APP_NAME "/apps/compiz"

/* From gconf-internal.h. Bleah. */
int gconf_value_compare (const GConfValue *value_a,
			 const GConfValue *value_b);

static int displayPrivateIndex;

typedef struct _GConfDisplay {
    int screenPrivateIndex;

    GConfClient *client;

    InitPluginForDisplayProc      initPluginForDisplay;
    SetDisplayOptionProc	  setDisplayOption;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;
} GConfDisplay;

typedef struct _GConfScreen {
    InitPluginForScreenProc      initPluginForScreen;
    SetScreenOptionProc		 setScreenOption;
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

static int
strcmpskipifequal (char **ptr,
		   char *s)
{
    int ret, len;

    len = strlen (s);
    ret = strncmp (*ptr, s, len);
    if (ret == 0)
	*ptr = (*ptr) + len;

    return ret;
}

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
		const gchar *screen,
		const gchar *plugin)
{
    GConfValue *gvalue, *existingValue = NULL;
    gchar      *key;

    GCONF_DISPLAY (d);

    if (plugin)
    {
	key = g_strjoin ("/", APP_NAME "/plugins", plugin, screen, "options",
			 o->name, NULL);
    }
    else
    {
	key = g_strjoin ("/", APP_NAME "/general", screen, "options", o->name,
			 NULL);
    }

    switch (o->type) {
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
	existingValue = gconf_client_get (gd->client, key, NULL);
	gvalue = gconf_value_new (gconfTypeFromCompType (o->type));
	gconfSetValue (d, &o->value, o->type, gvalue);
	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gd->client, key, gvalue, NULL);
	gconf_value_free (gvalue);
	break;
    case CompOptionTypeList: {
	GConfValueType type;
	GSList         *node, *list = NULL;
	GConfValue     *gv;
	int	       i;

	existingValue = gconf_client_get (gd->client, key, NULL);

	gvalue = gconf_value_new (GCONF_VALUE_LIST);

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
	gconf_value_free (gvalue);
    } break;
    default:
	break;
    }

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
gconfGetOptionValue (CompDisplay *d,
		     gchar	 *key)
{
    GConfValue      *gvalue;
    GConfEntry      *entry;
    CompOptionValue value;
    CompPlugin	    *p = 0;
    CompScreen	    *s = 0;
    CompOption	    *o, *option;
    gchar	    *ptr = key;
    gchar	    *pluginPtr = 0;
    gint	    pluginLen = 0;
    gint	    nOption;
    Bool	    status = FALSE;

    GCONF_DISPLAY (d);

    if (strncmp (ptr, APP_NAME, strlen (APP_NAME)))
	return FALSE;

    ptr += strlen (APP_NAME);

    if (strcmpskipifequal (&ptr, "/plugins/") == 0)
    {
	pluginPtr = ptr;
	ptr = strchr (ptr, '/');
	if (!ptr)
	    return FALSE;

	pluginLen = ptr - pluginPtr;
	if (pluginLen < 1)
	    return FALSE;
    }
    else if (strcmpskipifequal (&ptr, "/general"))
	return FALSE;

    if (strcmpskipifequal (&ptr, "/screen") == 0)
    {
	int screenNum;

	screenNum = strtol (ptr, &ptr, 0);

	for (s = d->screens; s; s = s->next)
	    if (s->screenNum == screenNum)
		break;

	if (!s || !ptr)
	    return FALSE;
    }
    else if (strcmpskipifequal (&ptr, "/allscreens"))
	return FALSE;

    if (strcmpskipifequal (&ptr, "/options/"))
	return FALSE;

    if (pluginPtr)
    {
	pluginPtr = g_strndup (pluginPtr, pluginLen);

	option  = 0;
	nOption = 0;

	p = findActivePlugin (pluginPtr);
	if (p)
	{
	    if (s)
	    {
		if (p->vTable->getScreenOptions)
		    option = (*p->vTable->getScreenOptions) (p, s, &nOption);
	    }
	    else
	    {
		if (p->vTable->getDisplayOptions)
		    option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
	    }
	}
    }
    else
    {
	if (s)
	    option = compGetScreenOptions (s, &nOption);
	else
	    option = compGetDisplayOptions (d, &nOption);
    }

    o = compFindOption (option, nOption, ptr, 0);
    if (!o)
    {
	if (pluginPtr)
	    g_free (pluginPtr);

	return FALSE;
    }

    entry = gconf_client_get_entry (gd->client, key, NULL, TRUE, NULL);
    if (entry)
    {
	gvalue = gconf_entry_get_value (entry);
	if (gvalue)
	{
	    compInitOptionValue (&value);

	    if (o->type      == CompOptionTypeList &&
		gvalue->type == GCONF_VALUE_LIST)
	    {
		GConfValueType type;

		type = gconf_value_get_list_type (gvalue);
		if (type == gconfTypeFromCompType (o->value.list.type))
		{
		    GSList *list;
		    int    i, length;

		    status = TRUE;

		    list = gconf_value_get_list (gvalue);

		    length = g_slist_length (list);

		    if (length)
		    {
			value.list.value =
			    malloc (sizeof (CompOptionValue) * length);
			if (value.list.value)
			{
			    for (i = 0; i < length; i++)
			    {
				if (!gconfGetValue (d,
						    &value.list.value[i],
						    o->value.list.type,
						    (GConfValue *)
						    list->data))
				{
				    status = FALSE;
				    break;
				}

				value.list.nValue++;

				list = g_slist_next (list);
			    }
			}
			else
			    status = FALSE;
		    }
		}
	    }
	    else
	    {
		status = gconfGetValue (d, &value, o->type, gvalue);
	    }

	    if (status)
	    {
		if (s)
		{
		    if (pluginPtr)
			status = (*s->setScreenOptionForPlugin) (s,
								 pluginPtr,
								 ptr,
								 &value);
		    else
			status = (*s->setScreenOption) (s, ptr, &value);
		}
		else
		{
		    if (pluginPtr)
			status = (*d->setDisplayOptionForPlugin) (d,
								  pluginPtr,
								  ptr,
								  &value);
		    else
			status = (*d->setDisplayOption) (d, ptr, &value);
		}
	    }

	    compFiniOptionValue (&value, o->type);
	}

	gconf_entry_free (entry);
    }

    if (pluginPtr)
	g_free (pluginPtr);

    return status;
}

static void
gconfInitOption (CompDisplay *d,
		 CompOption  *o,
		 const gchar *screen,
		 const gchar *plugin)
{
    gchar *key;

    if (plugin)
    {
	key = g_strjoin ("/", APP_NAME "/plugins", plugin, screen,
			 "options", o->name, NULL);
    }
    else
    {
	key = g_strjoin ("/", APP_NAME "/general", screen, "options",
			 o->name, NULL);
    }

    gconfGetOptionValue (d, key);

    g_free (key);
}

static Bool
gconfSetDisplayOption (CompDisplay     *d,
		       const char      *name,
		       CompOptionValue *value)
{
    Bool status;

    GCONF_DISPLAY (d);

    UNWRAP (gd, d, setDisplayOption);
    status = (*d->setDisplayOption) (d, name, value);
    WRAP (gd, d, setDisplayOption, gconfSetDisplayOption);

    if (status)
    {
	CompOption *option;
	int	   nOption;

	option = compGetDisplayOptions (d, &nOption);
	gconfSetOption (d, compFindOption (option, nOption, name, 0),
			"allscreens", 0);
    }

    return status;
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

    if (status)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getDisplayOptions)
	{
	    CompOption *option;
	    int	       nOption;

	    option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
	    gconfSetOption (d, compFindOption (option, nOption, name, 0),
			    "allscreens", plugin);
	}
    }

    return status;
}

static Bool
gconfSetScreenOption (CompScreen      *s,
		      const char      *name,
		      CompOptionValue *value)
{
    Bool status;

    GCONF_SCREEN (s);

    UNWRAP (gs, s, setScreenOption);
    status = (*s->setScreenOption) (s, name, value);
    WRAP (gs, s, setScreenOption, gconfSetScreenOption);

    if (status)
    {
	CompOption *option;
	int	   nOption;
	gchar      *screen;

	screen = g_strdup_printf ("screen%d", s->screenNum);

	option = compGetScreenOptions (s, &nOption);
	gconfSetOption (s->display, compFindOption (option, nOption, name, 0),
			screen, 0);

	g_free (screen);
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
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getScreenOptions)
	{
	    CompOption *option;
	    int	       nOption;
	    gchar      *screen;

	    screen = g_strdup_printf ("screen%d", s->screenNum);

	    option = (*p->vTable->getScreenOptions) (p, s, &nOption);
	    gconfSetOption (s->display,
			    compFindOption (option, nOption, name, 0),
			    screen, plugin);

	    g_free (screen);
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
	    gconfInitOption (d, option++, "allscreens", p->vTable->name);
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
	    gconfInitOption (s->display, option++, screen, p->vTable->name);

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

    gconfGetOptionValue (display, entry->key);
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
    CompOption   *option;
    int	         nOption;
    GConfDisplay *gd;

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

    gconf_client_add_dir (gd->client, APP_NAME,
			  GCONF_CLIENT_PRELOAD_NONE, NULL);

    WRAP (gd, d, initPluginForDisplay, gconfInitPluginForDisplay);
    WRAP (gd, d, setDisplayOption, gconfSetDisplayOption);
    WRAP (gd, d, setDisplayOptionForPlugin, gconfSetDisplayOptionForPlugin);

    d->privates[displayPrivateIndex].ptr = gd;

    option = compGetDisplayOptions (d, &nOption);
    while (nOption--)
	gconfInitOption (d, option++, "allscreens", 0);

    gconf_client_notify_add (gd->client, APP_NAME, gconfKeyChanged, d,
			     NULL, NULL);

    gconfSendGLibNotify (d);

    return TRUE;
}

static void
gconfFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    GCONF_DISPLAY (d);

    g_object_unref (gd->client);

    UNWRAP (gd, d, initPluginForDisplay);
    UNWRAP (gd, d, setDisplayOption);
    UNWRAP (gd, d, setDisplayOptionForPlugin);

    freeScreenPrivateIndex (d, gd->screenPrivateIndex);

    free (gd);
}

static Bool
gconfInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    CompOption  *option;
    int	        nOption;
    GConfScreen *gs;
    gchar       *screen;

    GCONF_DISPLAY (s->display);

    gs = malloc (sizeof (GConfScreen));
    if (!gs)
	return FALSE;

    WRAP (gs, s, initPluginForScreen, gconfInitPluginForScreen);
    WRAP (gs, s, setScreenOption, gconfSetScreenOption);
    WRAP (gs, s, setScreenOptionForPlugin, gconfSetScreenOptionForPlugin);

    s->privates[gd->screenPrivateIndex].ptr = gs;

    screen = g_strdup_printf ("screen%d", s->screenNum);

    option = compGetScreenOptions (s, &nOption);
    while (nOption--)
	gconfInitOption (s->display, option++, screen, 0);

    g_free (screen);

    return TRUE;
}

static void
gconfFiniScreen (CompPlugin *p,
		 CompScreen *s)
{
    GCONF_SCREEN (s);

    UNWRAP (gs, s, initPluginForScreen);
    UNWRAP (gs, s, setScreenOption);
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

static int
gconfGetVersion (CompPlugin *plugin,
		 int	    version)
{
    return ABIVERSION;
}

static CompMetadata *
gconfGetMetadata (CompPlugin *plugin)
{
    return &gconfMetadata;
}

CompPluginVTable gconfVTable = {
    "gconf",
    gconfGetVersion,
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
