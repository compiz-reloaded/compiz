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

static int corePrivateIndex;

typedef struct _GConfCore {
    GConfClient *client;
    guint	cnxn;

    CompTimeoutHandle reloadHandle;

    InitPluginForObjectProc initPluginForObject;
    SetOptionForPluginProc  setOptionForPlugin;
} GConfCore;

#define GET_GCONF_CORE(c)				     \
    ((GConfCore *) (c)->base.privates[corePrivateIndex].ptr)

#define GCONF_CORE(c)		       \
    GConfCore *gc = GET_GCONF_CORE (c)


static gchar *
gconfGetKey (CompObject  *object,
	     const gchar *plugin,
	     const gchar *option)
{
    const gchar *type;
    gchar	*key, *name, *objectName;

    type = compObjectTypeName (object->type);
    if (strcmp (type, "display") == 0)
	type = "allscreens";

    name = compObjectName (object);
    if (name)
    {
	objectName = g_strdup_printf ("%s%s", type, name);
	free (name);
    }
    else
	objectName = g_strdup (type);

    if (strcmp (plugin, "core") == 0)
	key = g_strjoin ("/", "/apps", APP_NAME, "general", objectName,
			 "options", option, NULL);
    else
	key = g_strjoin ("/", "/apps", APP_NAME, "plugins", plugin, objectName,
			 "options", option, NULL);

    g_free (objectName);

    return key;
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
gconfSetValue (CompObject      *object,
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

	while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
	    object = object->parent;

	if (!object)
	    return;

	action = keyActionToString (GET_CORE_DISPLAY (object), &value->action);
	gconf_value_set_string (gvalue, action);

	free (action);
    } break;
    case CompOptionTypeButton: {
	gchar *action;

	while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
	    object = object->parent;

	if (!object)
	    return;

	action = buttonActionToString (GET_CORE_DISPLAY (object),
				       &value->action);
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
gconfSetOption (CompObject  *object,
		CompOption  *o,
		const gchar *plugin)
{
    GConfValueType type = gconfTypeFromCompType (o->type);
    GConfValue     *gvalue, *existingValue = NULL;
    gchar          *key;

    GCONF_CORE (&core);

    if (type == GCONF_VALUE_INVALID)
	return;

    key = gconfGetKey (object, plugin, o->name);

    existingValue = gconf_client_get (gc->client, key, NULL);
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
	    gconfSetValue (object, &o->value.list.value[i],
			   o->value.list.type, gv);
	    list = g_slist_append (list, gv);
	}

	gconf_value_set_list_type (gvalue, type);
	gconf_value_set_list (gvalue, list);

	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gc->client, key, gvalue, NULL);

	for (node = list; node; node = node->next)
	    gconf_value_free ((GConfValue *) node->data);

	g_slist_free (list);
    }
    else
    {
	gconfSetValue (object, &o->value, o->type, gvalue);

	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gc->client, key, gvalue, NULL);
    }

    gconf_value_free (gvalue);

    if (existingValue)
	gconf_value_free (existingValue);

    g_free (key);
}

static Bool
gconfGetValue (CompObject      *object,
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

	while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
	    object = object->parent;

	if (!object)
	    return FALSE;

	stringToKeyAction (GET_CORE_DISPLAY (object), action, &value->action);
	return TRUE;
    }
    else if (type         == CompOptionTypeButton &&
	     gvalue->type == GCONF_VALUE_STRING)
    {
	const gchar *action;

	action = gconf_value_get_string (gvalue);

	while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
	    object = object->parent;

	if (!object)
	    return FALSE;

	stringToButtonAction (GET_CORE_DISPLAY (object), action,
			      &value->action);
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
gconfReadOptionValue (CompObject      *object,
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

	value->list.value  = NULL;
	value->list.nValue = 0;
	value->list.type   = o->value.list.type;

	if (n)
	{
	    value->list.value = malloc (sizeof (CompOptionValue) * n);
	    if (value->list.value)
	    {
		for (i = 0; i < n; i++)
		{
		    if (!gconfGetValue (object,
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
	if (!gconfGetValue (object, value, o->type, gvalue))
	    return FALSE;
    }

    return TRUE;
}

static void
gconfGetOption (CompObject *object,
		CompOption *o,
		const char *plugin)
{
    GConfEntry *entry;
    gchar      *key;

    GCONF_CORE (&core);

    key = gconfGetKey (object, plugin, o->name);

    entry = gconf_client_get_entry (gc->client, key, NULL, TRUE, NULL);
    if (entry)
    {
	CompOptionValue value;

	if (gconfReadOptionValue (object, entry, o, &value))
	{
	    (*core.setOptionForPlugin) (object, plugin, o->name, &value);
	    compFiniOptionValue (&value, o->type);
	}
	else
	{
	    gconfSetOption (object, o, plugin);
	}

	gconf_entry_free (entry);
    }

    g_free (key);
}

static CompBool
gconfReloadObjectTree (CompObject *object,
			 void       *closure);

static CompBool
gconfReloadObjectsWithType (CompObjectType type,
			      CompObject     *parent,
			      void	     *closure)
{
    compObjectForEach (parent, type, gconfReloadObjectTree, closure);

    return TRUE;
}

static CompBool
gconfReloadObjectTree (CompObject *object,
		       void       *closure)
{
    CompPlugin *p = (CompPlugin *) closure;
    CompOption  *option;
    int		nOption;

    option = (*p->vTable->getObjectOptions) (p, object, &nOption);
    while (nOption--)
	gconfGetOption (object, option++, p->vTable->name);

    compObjectForEachType (object, gconfReloadObjectsWithType, closure);

    return TRUE;
}

static Bool
gconfReload (void *closure)
{
    CompPlugin  *p;

    GCONF_CORE (&core);

    for (p = getPlugins (); p; p = p->next)
    {
	if (!p->vTable->getObjectOptions)
	    continue;

	gconfReloadObjectTree (&core.base, (void *) p);
    }

    gc->reloadHandle = 0;

    return FALSE;
}

static Bool
gconfSetOptionForPlugin (CompObject      *object,
			 const char	 *plugin,
			 const char	 *name,
			 CompOptionValue *value)
{
    CompBool status;

    GCONF_CORE (&core);

    UNWRAP (gc, &core, setOptionForPlugin);
    status = (*core.setOptionForPlugin) (object, plugin, name, value);
    WRAP (gc, &core, setOptionForPlugin, gconfSetOptionForPlugin);

    if (status && !gc->reloadHandle)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getObjectOptions)
	{
	    CompOption *option;
	    int	       nOption;

	    option = (*p->vTable->getObjectOptions) (p, object, &nOption);
	    option = compFindOption (option, nOption, name, 0);
	    if (option)
		gconfSetOption (object, option, p->vTable->name);
	}
    }

    return status;
}

static CompBool
gconfInitPluginForObject (CompPlugin *p,
			  CompObject *o)
{
    CompBool status;

    GCONF_CORE (&core);

    UNWRAP (gc, &core, initPluginForObject);
    status = (*core.initPluginForObject) (p, o);
    WRAP (gc, &core, initPluginForObject, gconfInitPluginForObject);

    if (status && p->vTable->getObjectOptions)
    {
	CompOption *option;
	int	   nOption;

	option = (*p->vTable->getObjectOptions) (p, o, &nOption);
	while (nOption--)
	    gconfGetOption (o, option++, p->vTable->name);
    }

    return status;
}

/* MULTIDPYERROR: only works with one or less displays present */
static void
gconfKeyChanged (GConfClient *client,
		 guint	     cnxn_id,
		 GConfEntry  *entry,
		 gpointer    user_data)
{
    CompPlugin *plugin;
    CompObject *object;
    CompOption *option = NULL;
    int	       nOption = 0;
    gchar      **token;
    int	       objectIndex = 4;

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

	objectIndex = 5;
	plugin = findActivePlugin (token[4]);
    }

    if (!plugin)
    {
	g_strfreev (token);
	return;
    }

    object = compObjectFind (&core.base, COMP_OBJECT_TYPE_DISPLAY, NULL);
    if (!object)
    {
	g_strfreev (token);
	return;
    }

    if (strncmp (token[objectIndex], "screen", 6) == 0)
    {
	object = compObjectFind (object, COMP_OBJECT_TYPE_SCREEN,
				 token[objectIndex] + 6);
	if (!object)
	{
	    g_strfreev (token);
	    return;
	}
    }
    else if (strcmp (token[objectIndex], "allscreens") != 0)
    {
	g_strfreev (token);
	return;
    }

    if (strcmp (token[objectIndex + 1], "options") != 0)
    {
	g_strfreev (token);
	return;
    }

    if (plugin->vTable->getObjectOptions)
	option = (*plugin->vTable->getObjectOptions) (plugin, object,
						      &nOption);

    option = compFindOption (option, nOption, token[objectIndex + 2], 0);
    if (option)
    {
	CompOptionValue value;

	if (gconfReadOptionValue (object, entry, option, &value))
	{
	    (*core.setOptionForPlugin) (object,
					plugin->vTable->name,
					option->name,
					&value);

	    compFiniOptionValue (&value, option->type);
	}
    }

    g_strfreev (token);
}

static void
gconfSendGLibNotify (CompScreen *s)
{
    Display *dpy = s->display->display;
    XEvent  xev;

    xev.xclient.type    = ClientMessage;
    xev.xclient.display = dpy;
    xev.xclient.format  = 32;

    xev.xclient.message_type = XInternAtom (dpy, "_COMPIZ_GLIB_NOTIFY", 0);
    xev.xclient.window	     = s->root;

    memset (xev.xclient.data.l, 0, sizeof (xev.xclient.data.l));

    XSendEvent (dpy,
		s->root,
		FALSE,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&xev);
}

static Bool
gconfInitCore (CompPlugin *p,
	       CompCore   *c)
{
    GConfCore *gc;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    gc = malloc (sizeof (GConfCore));
    if (!gc)
	return FALSE;

    g_type_init ();

    gc->client = gconf_client_get_default ();

    gconf_client_add_dir (gc->client, "/apps/" APP_NAME,
			  GCONF_CLIENT_PRELOAD_NONE, NULL);

    gc->reloadHandle = compAddTimeout (0, 0, gconfReload, 0);

    gc->cnxn = gconf_client_notify_add (gc->client, "/apps/" APP_NAME,
					gconfKeyChanged, c, NULL, NULL);

    WRAP (gc, c, initPluginForObject, gconfInitPluginForObject);
    WRAP (gc, c, setOptionForPlugin, gconfSetOptionForPlugin);

    c->base.privates[corePrivateIndex].ptr = gc;

    return TRUE;
}

static void
gconfFiniCore (CompPlugin *p,
	       CompCore   *c)
{
    GCONF_CORE (c);

    UNWRAP (gc, c, initPluginForObject);
    UNWRAP (gc, c, setOptionForPlugin);

    if (gc->reloadHandle)
	compRemoveTimeout (gc->reloadHandle);

    if (gc->cnxn)
	gconf_client_notify_remove (gc->client, gc->cnxn);

    gconf_client_remove_dir (gc->client, "/apps/" APP_NAME, NULL);
    gconf_client_clear_cache (gc->client);

    free (gc);
}

static Bool
gconfInitScreen (CompPlugin *p,
		 CompScreen *s)
{
    gconfSendGLibNotify (s);

    return TRUE;
}

static CompBool
gconfInitObject (CompPlugin *p,
		 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) gconfInitCore,
	(InitPluginObjectProc) 0, /* InitDisplay */
	(InitPluginObjectProc) gconfInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
gconfFiniObject (CompPlugin *p,
		 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) gconfFiniCore
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
gconfInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&gconfMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    corePrivateIndex = allocateCorePrivateIndex ();
    if (corePrivateIndex < 0)
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
    freeCorePrivateIndex (corePrivateIndex);
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
    gconfInitObject,
    gconfFiniObject,
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &gconfVTable;
}
