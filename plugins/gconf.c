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

#define KEY_CHANGE_TIMEOUT 250

static int displayPrivateIndex;

typedef struct _GConfDisplay {
    int screenPrivateIndex;

    GConfClient       *client;
    CompTimeoutHandle timeoutHandle;

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
	return GCONF_VALUE_BOOL;
    case CompOptionTypeInt:
	return GCONF_VALUE_INT;
    case CompOptionTypeFloat:
	return GCONF_VALUE_FLOAT;
    case CompOptionTypeString:
	return GCONF_VALUE_STRING;
    case CompOptionTypeColor:
	return GCONF_VALUE_STRING;
    case CompOptionTypeAction:
	return GCONF_VALUE_STRING;
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
gconfSetActionValue (CompDisplay     *d,
		     CompOptionValue *value,
		     CompOptionType  type,
		     GConfValue      *gvalue,
		     CompBindingType bindingType)
{
    char *binding = NULL;

    if (bindingType == CompBindingTypeKey)
    {
	if (value->action.type & CompBindingTypeKey)
	    binding = keyBindingToString (d, &value->action.key);
    }
    else
    {
	if (value->action.type & CompBindingTypeButton)
	    binding = buttonBindingToString (d, &value->action.button);
    }

    if (!binding)
	binding = strdup ("Disabled");

    gconf_value_set_string (gvalue, binding);

    free (binding);
}

static void
gconfSetOption (CompDisplay *d,
		CompOption  *o,
		gchar	    *screen,
		gchar	    *plugin)
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
    case CompOptionTypeMatch:
	existingValue = gconf_client_get (gd->client, key, NULL);
	gvalue = gconf_value_new (gconfTypeFromCompType (o->type));
	gconfSetValue (d, &o->value, o->type, gvalue);
	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gd->client, key, gvalue, NULL);
	gconf_value_free (gvalue);
	break;
    case CompOptionTypeAction: {
	gchar	   *key1, *key2, *key3, *key4, *key5;
	GSList     *node, *list = NULL;
	GConfValue *gv;
	int	   i;

	key1 = g_strdup_printf ("%s_%s", key, "key");
	key2 = g_strdup_printf ("%s_%s", key, "button");
	key3 = g_strdup_printf ("%s_%s", key, "bell");
	key4 = g_strdup_printf ("%s_%s", key, "edge");
	key5 = g_strdup_printf ("%s_%s", key, "edgebutton");

	gvalue = gconf_value_new (GCONF_VALUE_STRING);

	gconfSetActionValue (d, &o->value, o->type, gvalue,
			     CompBindingTypeKey);

	existingValue = gconf_client_get (gd->client, key1, NULL);
	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gd->client, key1, gvalue, NULL);

	if (existingValue)
	    gconf_value_free (existingValue);

	gconfSetActionValue (d, &o->value, o->type, gvalue,
			     CompBindingTypeButton);

	existingValue = gconf_client_get (gd->client, key2, NULL);
	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gd->client, key2, gvalue, NULL);

	if (existingValue)
	    gconf_value_free (existingValue);

	gconf_value_free (gvalue);

	gvalue = gconf_value_new (GCONF_VALUE_BOOL);
	gconf_value_set_bool (gvalue, o->value.action.bell);
	existingValue = gconf_client_get (gd->client, key3, NULL);
	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gd->client, key3, gvalue, NULL);

	if (existingValue)
	    gconf_value_free (existingValue);

	gconf_value_free (gvalue);

	existingValue = gconf_client_get (gd->client, key4, NULL);

	gvalue = gconf_value_new (GCONF_VALUE_LIST);

	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	{
	    if (o->value.action.edgeMask & (1 << i))
	    {
		gv = gconf_value_new (GCONF_VALUE_STRING);
		gconf_value_set_string (gv, edgeToString (i));
		list = g_slist_append (list, gv);
	    }
	}

	gconf_value_set_list_type (gvalue, GCONF_VALUE_STRING);
	gconf_value_set_list (gvalue, list);
	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gd->client, key4, gvalue, NULL);

	for (node = list; node; node = node->next)
	    gconf_value_free ((GConfValue *) node->data);

	if (existingValue)
	    gconf_value_free (existingValue);

	g_slist_free (list);
	gconf_value_free (gvalue);

	gvalue = gconf_value_new (GCONF_VALUE_INT);
	if (o->value.action.type & CompBindingTypeEdgeButton)
	    gconf_value_set_int (gvalue, o->value.action.edgeButton);
	else
	    gconf_value_set_int (gvalue, 0);

	existingValue = gconf_client_get (gd->client, key5, NULL);
	if (!existingValue || gconf_value_compare (existingValue, gvalue))
	    gconf_client_set (gd->client, key5, gvalue, NULL);

	gconf_value_free (gvalue);

	g_free (key1);
	g_free (key2);
	g_free (key3);
	g_free (key4);
	g_free (key5);
    } break;
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
	value->s = (char *) gconf_value_get_string (gvalue);
	return TRUE;
    }
    else if (type         == CompOptionTypeColor &&
	     gvalue->type == GCONF_VALUE_STRING)
    {
	const gchar *color;

	color = gconf_value_get_string (gvalue);

	if (stringToColor (color, value->c))
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

    if (o->type == CompOptionTypeAction)
    {
	gchar *key1, *key2, *key3, *key4, *key5;

	key1 = g_strdup_printf ("%s_%s", key, "key");
	key2 = g_strdup_printf ("%s_%s", key, "button");
	key3 = g_strdup_printf ("%s_%s", key, "bell");
	key4 = g_strdup_printf ("%s_%s", key, "edge");
	key5 = g_strdup_printf ("%s_%s", key, "edgebutton");

	value = o->value;

	entry = gconf_client_get_entry (gd->client, key1, NULL, TRUE, NULL);
	if (entry)
	{
	    gvalue = gconf_entry_get_value (entry);
	    if (gvalue && gvalue->type == GCONF_VALUE_STRING)
	    {
		const char *binding;

		binding = gconf_value_get_string (gvalue);
		if (!binding)
		    binding = "";

		if (strcasecmp (binding, "disabled") == 0 || !*binding)
		{
		    value.action.type &= ~CompBindingTypeKey;
		    status = TRUE;
		}
		else
		{
		    value.action.type |= CompBindingTypeKey;
		    status = stringToKeyBinding (d, binding,
						 &value.action.key);
		}
	    }

	    gconf_entry_free (entry);
	}

	entry = gconf_client_get_entry (gd->client, key2, NULL, TRUE, NULL);
	if (entry)
	{
	    gvalue = gconf_entry_get_value (entry);
	    if (gvalue && gvalue->type == GCONF_VALUE_STRING)
	    {
		const char *binding;

		binding = gconf_value_get_string (gvalue);
		if (!binding)
		    binding = "";

		if (strcasecmp (binding, "disabled") == 0 || !*binding)
		{
		    value.action.type &= ~CompBindingTypeButton;
		    status = TRUE;
		}
		else
		{
		    value.action.type |= CompBindingTypeButton;
		    status = stringToButtonBinding (d, binding,
						    &value.action.button);
		}
	    }

	    gconf_entry_free (entry);
	}

	entry = gconf_client_get_entry (gd->client, key3, NULL, TRUE, NULL);
	if (entry)
	{
	    gvalue = gconf_entry_get_value (entry);
	    if (gvalue && gvalue->type == GCONF_VALUE_BOOL)
	    {
		value.action.bell = gconf_value_get_bool (gvalue);
		status = TRUE;
	    }

	    gconf_entry_free (entry);
	}

	entry = gconf_client_get_entry (gd->client, key4, NULL, TRUE, NULL);
	if (entry)
	{
	    gvalue = gconf_entry_get_value (entry);
	    if (gvalue && gvalue->type == GCONF_VALUE_LIST)
	    {
		if (gconf_value_get_list_type (gvalue) == GCONF_VALUE_STRING)
		{
		    GSList *list;
		    gchar  *edge;
		    int    i;

		    value.action.edgeMask = 0;

		    status = TRUE;

		    list = gconf_value_get_list (gvalue);
		    while (list)
		    {
			GConfValue *gv;

			gv = (GConfValue *) list->data;
			edge = (gchar *) gconf_value_get_string (gv);

			for (i = 0; i < SCREEN_EDGE_NUM; i++)
			{
			    if (strcasecmp (edge, edgeToString (i)) == 0)
				value.action.edgeMask |= 1 << i;
			}

			list = g_slist_next (list);
		    }
		}
	    }

	    gconf_entry_free (entry);
	}

	entry = gconf_client_get_entry (gd->client, key5, NULL, TRUE, NULL);
	if (entry)
	{
	    gvalue = gconf_entry_get_value (entry);
	    if (gvalue && gvalue->type == GCONF_VALUE_INT)
	    {
		value.action.edgeButton = gconf_value_get_int (gvalue);

		if (value.action.edgeButton)
		    value.action.type |= CompBindingTypeEdgeButton;
		else
		    value.action.type &= ~CompBindingTypeEdgeButton;

		status = TRUE;
	    }

	    gconf_entry_free (entry);
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

	g_free (key1);
	g_free (key2);
	g_free (key3);
	g_free (key4);
	g_free (key5);
    }
    else
    {
	entry = gconf_client_get_entry (gd->client, key, NULL, TRUE, NULL);
	if (entry)
	{
	    gvalue = gconf_entry_get_value (entry);
	    if (gvalue)
	    {
		if (o->type      == CompOptionTypeList &&
		    gvalue->type == GCONF_VALUE_LIST)
		{
		    GConfValueType type;

		    value.list.value  = 0;
		    value.list.nValue = 0;

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
				    if (!gconfGetValue (d, &value.list.value[i],
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

			    if (!status)
			    {
				free (value.list.value);
			    }
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

		    if (o->type == CompOptionTypeList)
		    {
			if (o->value.list.type == CompOptionTypeMatch)
			{
			    int i;

			    for (i = 0; i < value.list.nValue; i++)
				matchFini (&value.list.value[i].match);
			}

			if (value.list.value)
			    free (value.list.value);
		    }
		    else if (o->type == CompOptionTypeMatch)
		    {
			matchFini (&value.match);
		    }
		}
	    }

	    gconf_entry_free (entry);
	}
    }

    if (pluginPtr)
	g_free (pluginPtr);

    return status;
}

static void
gconfInitOption (CompDisplay *d,
		 CompOption  *o,
		 gchar       *screen,
		 gchar       *plugin)
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
		       char	       *name,
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
				char	        *plugin,
				char	        *name,
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
		      char	      *name,
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
			       char	       *plugin,
			       char	       *name,
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
    static int  tail[] = { 0, 4, 5, 7, 11 };
    CompDisplay *display = (CompDisplay *) user_data;
    gchar	*key = g_strdup (entry->key);
    int		i;

    for (i = 0; i < sizeof (tail) / sizeof (tail[0]); i++)
    {
	if (strlen (entry->key) > tail[i])
	{
	    key[strlen (entry->key) - tail[i]] = '\0';
	    if (gconfGetOptionValue (display, key))
		break;
	}
    }

    if (key)
	g_free (key);
}

static Bool
gconfTimeout (void *closure)
{
    while (g_main_pending ()) g_main_iteration (FALSE);

    return TRUE;
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

    gd->timeoutHandle = compAddTimeout (KEY_CHANGE_TIMEOUT, gconfTimeout, 0);

    return TRUE;
}

static void
gconfFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    GCONF_DISPLAY (d);

    compRemoveTimeout (gd->timeoutHandle);

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
    if (displayPrivateIndex >= 0)
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

CompPluginDep gconfDeps[] = {
    { CompPluginRuleBefore, "decoration" },
    { CompPluginRuleBefore, "wobbly" },
    { CompPluginRuleBefore, "fade" },
    { CompPluginRuleBefore, "cube" },
    { CompPluginRuleBefore, "scale" }
};

CompPluginVTable gconfVTable = {
    "gconf",
    "GConf",
    "GConf Control Backend",
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
    0, /* SetScreenOption */
    gconfDeps,
    sizeof (gconfDeps) / sizeof (gconfDeps[0]),
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &gconfVTable;
}
