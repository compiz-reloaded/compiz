/*
 * Copyright © 2006 Novell, Inc.
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

#include <string.h>
#include <stdlib.h>
#include <poll.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <libxml/xmlwriter.h>

#include <compiz-core.h>

static CompMetadata dbusMetadata;

#define COMPIZ_DBUS_SERVICE_NAME	            "org.freedesktop.compiz"
#define COMPIZ_DBUS_INTERFACE			    "org.freedesktop.compiz"
#define COMPIZ_DBUS_ROOT_PATH			    "/org/freedesktop/compiz"

#define COMPIZ_DBUS_ACTIVATE_MEMBER_NAME            "activate"
#define COMPIZ_DBUS_DEACTIVATE_MEMBER_NAME          "deactivate"
#define COMPIZ_DBUS_SET_MEMBER_NAME                 "set"
#define COMPIZ_DBUS_GET_MEMBER_NAME                 "get"
#define COMPIZ_DBUS_GET_METADATA_MEMBER_NAME	    "getMetadata"
#define COMPIZ_DBUS_LIST_MEMBER_NAME		    "list"
#define COMPIZ_DBUS_GET_PLUGINS_MEMBER_NAME	    "getPlugins"
#define COMPIZ_DBUS_GET_PLUGIN_METADATA_MEMBER_NAME "getPluginMetadata"

#define COMPIZ_DBUS_CHANGED_SIGNAL_NAME		    "changed"
#define COMPIZ_DBUS_PLUGINS_CHANGED_SIGNAL_NAME	    "pluginsChanged"

#define DBUS_FILE_WATCH_CURRENT 0
#define DBUS_FILE_WATCH_PLUGIN  1
#define DBUS_FILE_WATCH_HOME    2
#define DBUS_FILE_WATCH_NUM     3

static int corePrivateIndex;
static int displayPrivateIndex;

typedef struct _DbusCore {
    DBusConnection    *connection;
    CompWatchFdHandle watchFdHandle;

    CompFileWatchHandle fileWatch[DBUS_FILE_WATCH_NUM];

    InitPluginForObjectProc initPluginForObject;
    SetOptionForPluginProc  setOptionForPlugin;
} DbusCore;

typedef struct _DbusDisplay {
    char         **pluginList;
    unsigned int nPlugins;
} DbusDisplay;

static DBusHandlerResult dbusHandleMessage (DBusConnection *,
					    DBusMessage *,
					    void *);

static DBusObjectPathVTable dbusMessagesVTable = {
    NULL, dbusHandleMessage, /* handler function */
    NULL, NULL, NULL, NULL
};

#define GET_DBUS_CORE(c)				    \
    ((DbusCore *) (c)->base.privates[corePrivateIndex].ptr)

#define DBUS_CORE(c)		     \
    DbusCore *dc = GET_DBUS_CORE (c)

#define GET_DBUS_DISPLAY(d)                                       \
    ((DbusDisplay *) (d)->base.privates[displayPrivateIndex].ptr)

#define DBUS_DISPLAY(d)                    \
    DbusDisplay *dd = GET_DBUS_DISPLAY (d)

static void
dbusUpdatePluginList (CompDisplay *d)
{
    CompListValue *pl;
    unsigned int  i;

    DBUS_DISPLAY (d);

    pl = &d->opt[COMP_DISPLAY_OPTION_ACTIVE_PLUGINS].value.list;

    for (i = 0; i < dd->nPlugins; i++)
	free (dd->pluginList[i]);

    dd->pluginList = realloc (dd->pluginList, pl->nValue * sizeof (char *));
    if (!dd->pluginList)
    {
	dd->nPlugins = 0;
	return;
    }

    for (i = 0; i < pl->nValue; i++)
	dd->pluginList[i] = strdup (pl->value[i].s);

    dd->nPlugins = pl->nValue;
}

static CompOption *
dbusGetOptionsFromPath (char	     **path,
			CompObject   **returnObject,
			CompMetadata **returnMetadata,
			int	     *nOption)
{
    CompPlugin *p;
    CompObject *object;

    object = compObjectFind (&core.base, COMP_OBJECT_TYPE_DISPLAY, NULL);
    if (!object)
	return NULL;

    if (strncmp (path[1], "screen", 6) == 0)
    {
	object = compObjectFind (object, COMP_OBJECT_TYPE_SCREEN,
				 path[1] + 6);
	if (!object)
	    return NULL;
    }
    else if (strcmp (path[1], "allscreens") != 0)
    {
	return NULL;
    }

    if (returnObject)
	*returnObject = object;

    for (p = getPlugins (); p; p = p->next)
	if (strcmp (p->vTable->name, path[0]) == 0)
	    break;

    if (returnMetadata)
    {
	if (p && p->vTable->getMetadata)
	    *returnMetadata = (*p->vTable->getMetadata) (p);
	else
	    *returnMetadata = NULL;
    }

    if (!p)
	return NULL;

    if (!p->vTable->getObjectOptions)
	return NULL;

    return (*p->vTable->getObjectOptions) (p, object, nOption);
}

/* functions to create introspection XML */
static void
dbusIntrospectStartInterface (xmlTextWriterPtr writer)
{
    xmlTextWriterStartElement (writer, BAD_CAST "interface");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name",
				 BAD_CAST COMPIZ_DBUS_SERVICE_NAME);
}

static void
dbusIntrospectEndInterface (xmlTextWriterPtr writer)
{
    xmlTextWriterEndElement (writer);
}

static void
dbusIntrospectAddArgument (xmlTextWriterPtr writer,
			   char             *type,
			   char             *direction)
{
    xmlTextWriterStartElement (writer, BAD_CAST "arg");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "type", BAD_CAST type);
    xmlTextWriterWriteAttribute (writer, BAD_CAST "direction",
				 BAD_CAST direction);
    xmlTextWriterEndElement (writer);
}

static void
dbusIntrospectAddMethod (xmlTextWriterPtr writer,
			 char             *name,
			 int              nArgs,
			 ...)
{
    va_list var_args;
    char *type, *direction;

    xmlTextWriterStartElement (writer, BAD_CAST "method");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name", BAD_CAST name);

    va_start (var_args, nArgs);
    while (nArgs)
    {
	type = va_arg (var_args, char *);
	direction = va_arg (var_args, char *);
	dbusIntrospectAddArgument (writer, type, direction);
	nArgs--;
    }
    va_end (var_args);

    xmlTextWriterEndElement (writer);
}

static void
dbusIntrospectAddSignal (xmlTextWriterPtr writer,
			 char             *name,
			 int              nArgs,
			 ...)
{
    va_list var_args;
    char *type;

    xmlTextWriterStartElement (writer, BAD_CAST "signal");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name", BAD_CAST name);

    va_start (var_args, nArgs);
    while (nArgs)
    {
	type = va_arg (var_args, char *);
	dbusIntrospectAddArgument (writer, type, "out");
	nArgs--;
    }
    va_end (var_args);

    xmlTextWriterEndElement (writer);
}

static void
dbusIntrospectAddNode (xmlTextWriterPtr writer,
		       char             *name)
{
    xmlTextWriterStartElement (writer, BAD_CAST "node");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name", BAD_CAST name);
    xmlTextWriterEndElement (writer);
}

static void
dbusIntrospectStartRoot (xmlTextWriterPtr writer)
{
    xmlTextWriterStartElement (writer, BAD_CAST "node");

    xmlTextWriterStartElement (writer, BAD_CAST "interface");
    xmlTextWriterWriteAttribute (writer, BAD_CAST "name",
				 BAD_CAST "org.freedesktop.DBus.Introspectable");

    dbusIntrospectAddMethod (writer, "Introspect", 1, "s", "out");

    xmlTextWriterEndElement (writer);
}

static void
dbusIntrospectEndRoot (xmlTextWriterPtr writer)
{
    xmlTextWriterEndDocument (writer);
}

/* introspection handlers */
static Bool
dbusHandleRootIntrospectMessage (DBusConnection *connection,
				 DBusMessage    *message)
{
    char **plugins, **pluginName;
    int nPlugins;

    xmlTextWriterPtr writer;
    xmlBufferPtr buf;

    buf = xmlBufferCreate ();
    writer = xmlNewTextWriterMemory (buf, 0);

    dbusIntrospectStartRoot (writer);
    dbusIntrospectStartInterface (writer);

    dbusIntrospectAddMethod (writer, COMPIZ_DBUS_GET_PLUGINS_MEMBER_NAME, 1,
			     "as", "out");
    dbusIntrospectAddMethod (writer,
			     COMPIZ_DBUS_GET_PLUGIN_METADATA_MEMBER_NAME, 7,
			     "s", "in", "s", "out", "s", "out", "s", "out",
			     "b", "out", "as", "out", "as", "out");
    dbusIntrospectAddSignal (writer,
			     COMPIZ_DBUS_PLUGINS_CHANGED_SIGNAL_NAME, 0);

    dbusIntrospectEndInterface (writer);

    plugins = availablePlugins (&nPlugins);
    if (plugins)
    {
	pluginName = plugins;

	while (nPlugins--)
	{
	    dbusIntrospectAddNode (writer, *pluginName);
	    free (*pluginName);
	    pluginName++;
	}

	free (plugins);
    }
    else
    {
	xmlFreeTextWriter (writer);
	xmlBufferFree (buf);
	return FALSE;
    }

    dbusIntrospectEndRoot (writer);

    xmlFreeTextWriter (writer);

    DBusMessage *reply = dbus_message_new_method_return (message);
    if (!reply)
    {
	xmlBufferFree (buf);
	return FALSE;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append (reply, &args);

    if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING,
					 &buf->content))
    {
	xmlBufferFree (buf);
	return FALSE;
    }

    xmlBufferFree (buf);

    if (!dbus_connection_send (connection, reply, NULL))
    {
	return FALSE;
    }

    dbus_connection_flush (connection);
    dbus_message_unref (reply);

    return TRUE;
}

/* MULTIDPYERROR: only works with one or less displays present */
static Bool
dbusHandlePluginIntrospectMessage (DBusConnection *connection,
				   DBusMessage    *message,
				   char           **path)
{
    CompDisplay *d;
    CompScreen *s;
    char screenName[256];

    xmlTextWriterPtr writer;
    xmlBufferPtr buf;

    buf = xmlBufferCreate ();
    writer = xmlNewTextWriterMemory (buf, 0);

    dbusIntrospectStartRoot (writer);

    for (d = core.displays; d; d = d->next)
    {
	dbusIntrospectAddNode (writer, "allscreens");

	for (s = d->screens; s; s = s->next)
	{
	    sprintf (screenName, "screen%d", s->screenNum);
	    dbusIntrospectAddNode (writer, screenName);
	}
    }

    dbusIntrospectEndRoot (writer);

    xmlFreeTextWriter (writer);

    DBusMessage *reply = dbus_message_new_method_return (message);
    if (!reply)
    {
	xmlBufferFree (buf);
	return FALSE;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append (reply, &args);

    if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING,
					 &buf->content))
    {
	xmlBufferFree (buf);
	return FALSE;
    }

    xmlBufferFree (buf);

    if (!dbus_connection_send (connection, reply, NULL))
    {
	return FALSE;
    }

    dbus_connection_flush (connection);
    dbus_message_unref (reply);

    return TRUE;
}

static Bool
dbusHandleScreenIntrospectMessage (DBusConnection *connection,
				   DBusMessage    *message,
				   char           **path)
{
    CompOption *option = NULL;
    int nOptions;

    xmlTextWriterPtr writer;
    xmlBufferPtr buf;

    buf = xmlBufferCreate ();
    writer = xmlNewTextWriterMemory (buf, 0);

    dbusIntrospectStartRoot (writer);
    dbusIntrospectStartInterface (writer);

    dbusIntrospectAddMethod (writer, COMPIZ_DBUS_LIST_MEMBER_NAME, 1,
			     "as", "out");

    dbusIntrospectEndInterface (writer);

    option = dbusGetOptionsFromPath (path, NULL, NULL, &nOptions);
    if (option)
    {
	while (nOptions--)
	{
	    dbusIntrospectAddNode (writer, option->name);
	    option++;
	}
    }

    dbusIntrospectEndRoot (writer);

    xmlFreeTextWriter (writer);

    DBusMessage *reply = dbus_message_new_method_return (message);
    if (!reply)
    {
	xmlBufferFree (buf);
	return FALSE;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append (reply, &args);

    if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING,
					 &buf->content))
    {
	xmlBufferFree (buf);
	return FALSE;
    }

    xmlBufferFree (buf);

    if (!dbus_connection_send (connection, reply, NULL))
    {
	return FALSE;
    }

    dbus_connection_flush (connection);
    dbus_message_unref (reply);

    return TRUE;
}

static Bool
dbusHandleOptionIntrospectMessage (DBusConnection *connection,
				   DBusMessage    *message,
				   char           **path)
{
    CompOption       *option;
    int              nOptions;
    CompOptionType   restrictionType;
    Bool             metadataHandled;
    char             type[3];
    xmlTextWriterPtr writer;
    xmlBufferPtr     buf;
    Bool             isList = FALSE;

    buf = xmlBufferCreate ();
    writer = xmlNewTextWriterMemory (buf, 0);

    dbusIntrospectStartRoot (writer);
    dbusIntrospectStartInterface (writer);

    option = dbusGetOptionsFromPath (path, NULL, NULL, &nOptions);
    if (!option)
    {
	xmlFreeTextWriter (writer);
	xmlBufferFree (buf);
	return FALSE;
    }

    while (nOptions--)
    {
	if (strcmp (option->name, path[2]) == 0)
	{
	    restrictionType = option->type;
	    if (restrictionType == CompOptionTypeList)
	    {
		restrictionType = option->value.list.type;
		isList = TRUE;
	    }

	    metadataHandled = FALSE;
	    switch (restrictionType)
	    {
	    case CompOptionTypeInt:
		if (isList)
		    strcpy (type, "ai");
		else
		    strcpy (type, "i");

		dbusIntrospectAddMethod (writer,
					 COMPIZ_DBUS_GET_METADATA_MEMBER_NAME,
					 6, "s", "out", "s", "out",
					 "b", "out", "s", "out",
					 "i", "out", "i", "out");
		metadataHandled = TRUE;
		break;
	    case CompOptionTypeFloat:
		if (isList)
		    strcpy (type, "ad");
		else
		    strcpy (type, "d");

		dbusIntrospectAddMethod (writer,
					 COMPIZ_DBUS_GET_METADATA_MEMBER_NAME,
					 7, "s", "out", "s", "out",
					 "b", "out", "s", "out",
					 "d", "out", "d", "out",
					 "d", "out");
		metadataHandled = TRUE;
		break;
	    case CompOptionTypeString:
		if (isList)
		    strcpy (type, "as");
		else
		    strcpy (type, "s");

		dbusIntrospectAddMethod (writer,
					 COMPIZ_DBUS_GET_METADATA_MEMBER_NAME,
					 5, "s", "out", "s", "out",
					 "b", "out", "s", "out",
					 "as", "out");
		metadataHandled = TRUE;
		break;
	    case CompOptionTypeBool:
	    case CompOptionTypeBell:
		if (isList)
		    strcpy (type, "ab");
		else
		    strcpy (type, "b");

		break;
	    case CompOptionTypeColor:
	    case CompOptionTypeKey:
	    case CompOptionTypeButton:
	    case CompOptionTypeEdge:
	    case CompOptionTypeMatch:
		if (isList)
		    strcpy (type, "as");
		else
		    strcpy (type, "s");
		break;
	    default:
		continue;
	    }

	    dbusIntrospectAddMethod (writer,
				     COMPIZ_DBUS_GET_MEMBER_NAME, 1,
				     type, "out");
	    dbusIntrospectAddMethod (writer,
				     COMPIZ_DBUS_SET_MEMBER_NAME, 1,
				     type, "in");
	    dbusIntrospectAddSignal (writer,
				     COMPIZ_DBUS_CHANGED_SIGNAL_NAME, 1,
				     type, "out");

	    if (!metadataHandled)
		dbusIntrospectAddMethod (writer,
					 COMPIZ_DBUS_GET_METADATA_MEMBER_NAME,
					 4, "s", "out", "s", "out",
					 "b", "out", "s", "out");
	    break;
	}

	option++;
    }

    dbusIntrospectEndInterface (writer);
    dbusIntrospectEndRoot (writer);

    xmlFreeTextWriter (writer);

    DBusMessage *reply = dbus_message_new_method_return (message);
    if (!reply)
    {
	xmlBufferFree (buf);
	return FALSE;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append (reply, &args);

    if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING,
					 &buf->content))
    {
	xmlBufferFree (buf);
	return FALSE;
    }

    xmlBufferFree (buf);

    if (!dbus_connection_send (connection, reply, NULL))
    {
	return FALSE;
    }

    dbus_connection_flush (connection);
    dbus_message_unref (reply);

    return TRUE;
}


/*
 * Activate can be used to trigger any existing action. Arguments
 * should be a pair of { string, bool|int32|double|string }.
 *
 * Example (rotate to face 1):
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/rotate/allscreens/rotate_to	      \
 * org.freedesktop.compiz.activate			      \
 * string:'root'					      \
 * int32:`xwininfo -root | grep id: | awk '{ print $4 }'`     \
 * string:'face' int32:1
 *
 *
 * You can also call the terminate function
 *
 * Example unfold and refold cube:
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/cube/allscreens/unfold	      \
 * org.freedesktop.compiz.activate			      \
 * string:'root'					      \
 * int32:`xwininfo -root | grep id: | awk '{ print $4 }'`
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/cube/allscreens/unfold	      \
 * org.freedesktop.compiz.deactivate			      \
 * string:'root'					      \
 * int32:`xwininfo -root | grep id: | awk '{ print $4 }'`
 *
 */
static Bool
dbusHandleActionMessage (DBusConnection *connection,
			 DBusMessage    *message,
			 char	        **path,
			 Bool           activate)
{
    CompObject *object;
    CompOption *option;
    int	       nOption;

    option = dbusGetOptionsFromPath (path, &object, NULL, &nOption);
    if (!option)
	return FALSE;

    while (nOption--)
    {
	if (strcmp (option->name, path[2]) == 0)
	{
	    CompOption	    *argument = NULL;
	    int		    i, nArgument = 0;
	    DBusMessageIter iter;

	    if (!isActionOption (option))
		return FALSE;

	    while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
		object = object->parent;

	    if (!object)
		return FALSE;

	    if (activate)
	    {
		if (!option->value.action.initiate)
		    return FALSE;
	    }
	    else
	    {
		if (!option->value.action.terminate)
		    return FALSE;
	    }

	    if (dbus_message_iter_init (message, &iter))
	    {
		CompOptionValue value;
		CompOptionType  type = 0;
		char		*name;
		Bool		hasValue;

		do
		{
		    name     = NULL;
		    hasValue = FALSE;

		    while (!name)
		    {
			switch (dbus_message_iter_get_arg_type (&iter)) {
			case DBUS_TYPE_STRING:
			    dbus_message_iter_get_basic (&iter, &name);
			default:
			    break;
			}

			if (!dbus_message_iter_next (&iter))
			    break;
		    }

		    while (!hasValue)
		    {
			double tmp;

			switch (dbus_message_iter_get_arg_type (&iter)) {
			case DBUS_TYPE_BOOLEAN:
			    hasValue = TRUE;
			    type     = CompOptionTypeBool;

			    dbus_message_iter_get_basic (&iter, &value.b);
			    break;
			case DBUS_TYPE_INT32:
			    hasValue = TRUE;
			    type     = CompOptionTypeInt;

			    dbus_message_iter_get_basic (&iter, &value.i);
			    break;
			case DBUS_TYPE_DOUBLE:
			    hasValue = TRUE;
			    type     = CompOptionTypeFloat;

			    dbus_message_iter_get_basic (&iter, &tmp);

			    value.f = tmp;
			    break;
			case DBUS_TYPE_STRING:
			    hasValue = TRUE;

			    /* XXX: use match option type if name is "match" */
			    if (name && strcmp (name, "match") == 0)
			    {
				char *s;

				type = CompOptionTypeMatch;

				dbus_message_iter_get_basic (&iter, &s);

				matchInit (&value.match);
				matchAddFromString (&value.match, s);
			    }
			    else
			    {
				type = CompOptionTypeString;

				dbus_message_iter_get_basic (&iter, &value.s);
			    }
			default:
			    break;
			}

			if (!dbus_message_iter_next (&iter))
			    break;
		    }

		    if (name && hasValue)
		    {
			CompOption *a;

			a = realloc (argument,
				     sizeof (CompOption) * (nArgument + 1));
			if (a)
			{
			    argument = a;

			    argument[nArgument].name  = name;
			    argument[nArgument].type  = type;
			    argument[nArgument].value = value;

			    nArgument++;
			}
		    }
		} while (dbus_message_iter_has_next (&iter));
	    }

	    if (activate)
	    {
		(*option->value.action.initiate) (GET_CORE_DISPLAY (object),
						  &option->value.action,
						  0,
						  argument, nArgument);
	    }
	    else
	    {
		(*option->value.action.terminate) (GET_CORE_DISPLAY (object),
						   &option->value.action,
						   0,
						   argument, nArgument);
	    }

	    for (i = 0; i < nArgument; i++)
		if (argument[i].type == CompOptionTypeMatch)
		    matchFini (&argument[i].value.match);

	    if (argument)
		free (argument);

	    if (!dbus_message_get_no_reply (message))
	    {
		DBusMessage *reply;

		reply = dbus_message_new_method_return (message);

		dbus_connection_send (connection, reply, NULL);
		dbus_connection_flush (connection);

		dbus_message_unref (reply);
	    }

	    return TRUE;
	}

	option++;
    }

    return FALSE;
}

static Bool
dbusTryGetValueWithType (DBusMessageIter *iter,
			 int		 type,
			 void		 *value)
{
    if (dbus_message_iter_get_arg_type (iter) == type)
    {
	dbus_message_iter_get_basic (iter, value);

	return TRUE;
    }

    return FALSE;
}

static Bool
dbusGetOptionValue (CompObject	    *object,
		    DBusMessageIter *iter,
		    CompOptionType  type,
		    CompOptionValue *value)
{
    double d;
    char   *s;

    switch (type) {
    case CompOptionTypeBool:
	return dbusTryGetValueWithType (iter,
					DBUS_TYPE_BOOLEAN,
					&value->b);
	break;
    case CompOptionTypeInt:
	return dbusTryGetValueWithType (iter,
					DBUS_TYPE_INT32,
					&value->i);
	break;
    case CompOptionTypeFloat:
	if (dbusTryGetValueWithType (iter,
				     DBUS_TYPE_DOUBLE,
				     &d))
	{
	    value->f = d;
	    return TRUE;
	}
	break;
    case CompOptionTypeString:
	return dbusTryGetValueWithType (iter,
					DBUS_TYPE_STRING,
					&value->s);
	break;
    case CompOptionTypeColor:
	if (dbusTryGetValueWithType (iter,
				     DBUS_TYPE_STRING,
				     &s))
	{
	    if (stringToColor (s, value->c))
		return TRUE;
	}
	break;
    case CompOptionTypeKey:
	if (dbusTryGetValueWithType (iter,
				     DBUS_TYPE_STRING,
				     &s))
	{
	    while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
		object = object->parent;

	    if (!object)
		return FALSE;

	    stringToKeyAction (GET_CORE_DISPLAY (object), s, &value->action);
	    return TRUE;
	}
	break;
    case CompOptionTypeButton:
	if (dbusTryGetValueWithType (iter,
				     DBUS_TYPE_STRING,
				     &s))
	{
	    while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
		object = object->parent;

	    if (!object)
		return FALSE;

	    stringToButtonAction (GET_CORE_DISPLAY (object),
				  s, &value->action);
	    return TRUE;
	}
	break;
    case CompOptionTypeEdge:
	if (dbusTryGetValueWithType (iter,
				     DBUS_TYPE_STRING,
				     &s))
	{
	    value->action.edgeMask = stringToEdgeMask (s);
	    return TRUE;
	}
	break;
    case CompOptionTypeBell:
	return dbusTryGetValueWithType (iter,
					DBUS_TYPE_BOOLEAN,
					&value->action.bell);
	break;
    case CompOptionTypeMatch:
	if (dbusTryGetValueWithType (iter,
				     DBUS_TYPE_STRING,
				     &s))
	{
	    matchAddFromString (&value->match, s);
	    return TRUE;
	}

    default:
	break;
    }

    return FALSE;
}

/*
 * 'Set' can be used to change any existing option. Argument
 * should be the new value for the option.
 *
 * Example (will set command0 option to firefox):
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/core/allscreens/command0	      \
 * org.freedesktop.compiz.set				      \
 * string:'firefox'
 *
 * List and action options can be changed using more than one
 * argument.
 *
 * Example (will set active_plugins option to
 * [dbus,decoration,place]):
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/core/allscreens/active_plugins     \
 * org.freedesktop.compiz.set				      \
 * array:string:'dbus','decoration','place'
 *
 * Example (will set run_command0 option to trigger on key
 * binding <Control><Alt>Return and not trigger on any button
 * bindings, screen edges or bell notifications):
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/core/allscreens/run_command0	      \
 * org.freedesktop.compiz.set				      \
 * string:'<Control><Alt>Return'			      \
 * string:'Disabled'					      \
 * boolean:'false'					      \
 * string:''						      \
 * int32:'0'
 */
static Bool
dbusHandleSetOptionMessage (DBusConnection *connection,
			    DBusMessage    *message,
			    char	   **path)
{
    CompObject *object;
    CompOption *option;
    int	       nOption;

    option = dbusGetOptionsFromPath (path, &object, NULL, &nOption);
    if (!option)
	return FALSE;

    while (nOption--)
    {
	if (strcmp (option->name, path[2]) == 0)
	{
	    DBusMessageIter iter, aiter;
	    CompOptionValue value, tmpValue;
	    Bool	    status = FALSE;

	    memset (&value, 0, sizeof (value));

	    if (option->type == CompOptionTypeList)
	    {
		if (dbus_message_iter_init (message, &iter) &&
		    dbus_message_iter_get_arg_type (&iter) == DBUS_TYPE_ARRAY)
		{
		    dbus_message_iter_recurse (&iter, &aiter);

		    do
		    {
			memset (&tmpValue, 0, sizeof (tmpValue));

			if (dbusGetOptionValue (object,
						&aiter,
						option->value.list.type,
						&tmpValue))
			{
			    CompOptionValue *v;

			    v = realloc (value.list.value,
					 sizeof (CompOptionValue) *
					 (value.list.nValue + 1));
			    if (v)
			    {
				v[value.list.nValue++] = tmpValue;
				value.list.value = v;
			    }
			}
		    } while (dbus_message_iter_next (&aiter));

		    status = TRUE;
		}
	    }
	    else if (dbus_message_iter_init (message, &iter))
	    {
		status = dbusGetOptionValue (object, &iter, option->type,
					     &value);
	    }

	    if (status)
	    {
		(*core.setOptionForPlugin) (object,
					    path[0],
					    option->name,
					    &value);

		if (!dbus_message_get_no_reply (message))
		{
		    DBusMessage *reply;

		    reply = dbus_message_new_method_return (message);

		    dbus_connection_send (connection, reply, NULL);
		    dbus_connection_flush (connection);

		    dbus_message_unref (reply);
		}

		return TRUE;
	    }
	    else
	    {
		return FALSE;
	    }
	}

	option++;
    }

    return FALSE;
}

static void
dbusAppendSimpleOptionValue (CompObject      *object,
			     DBusMessage     *message,
			     CompOptionType  type,
			     CompOptionValue *value)
{
    double d;
    char   *s;

    switch (type) {
    case CompOptionTypeBool:
	dbus_message_append_args (message,
				  DBUS_TYPE_BOOLEAN, &value->b,
				  DBUS_TYPE_INVALID);
	break;
    case CompOptionTypeInt:
	dbus_message_append_args (message,
				  DBUS_TYPE_INT32, &value->i,
				  DBUS_TYPE_INVALID);
	break;
    case CompOptionTypeFloat:
	d = value->f;

	dbus_message_append_args (message,
				  DBUS_TYPE_DOUBLE, &d,
				  DBUS_TYPE_INVALID);
	break;
    case CompOptionTypeString:
	dbus_message_append_args (message,
				  DBUS_TYPE_STRING, &value->s,
				  DBUS_TYPE_INVALID);
	break;
    case CompOptionTypeColor:
	s = colorToString (value->c);
	if (s)
	{
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	    free (s);
	}
	break;
    case CompOptionTypeKey:
	s = keyActionToString ((CompDisplay *) object, &value->action);
	if (s)
	{
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	    free (s);
	}
	break;
    case CompOptionTypeButton:
	s = buttonActionToString ((CompDisplay *) object, &value->action);
	if (s)
	{
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	    free (s);
	}
	break;
    case CompOptionTypeEdge:
	s = edgeMaskToString (value->action.edgeMask);
	if (s)
	{
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	    free (s);
	}
	break;
    case CompOptionTypeBell:
	dbus_message_append_args (message,
				  DBUS_TYPE_BOOLEAN, &value->action.bell,
				  DBUS_TYPE_INVALID);
	break;
    case CompOptionTypeMatch:
	s = matchToString (&value->match);
	if (s)
	{
	    dbus_message_append_args (message,
				      DBUS_TYPE_STRING, &s,
				      DBUS_TYPE_INVALID);
	    free (s);
	}
    default:
	break;
    }
}

static void
dbusAppendListOptionValue (CompObject      *object,
			   DBusMessage     *message,
			   CompOptionType  type,
			   CompOptionValue *value)
{
    DBusMessageIter iter;
    DBusMessageIter listIter;
    char	    sig[2];
    char	    *s;
    int		    i;

    switch (value->list.type) {
    case CompOptionTypeInt:
	sig[0] = DBUS_TYPE_INT32;
	break;
    case CompOptionTypeFloat:
	sig[0] = DBUS_TYPE_DOUBLE;
	break;
    case CompOptionTypeBool:
    case CompOptionTypeBell:
	sig[0] = DBUS_TYPE_BOOLEAN;
	break;
    default:
	sig[0] = DBUS_TYPE_STRING;
	break;
    }
    sig[1] = '\0';

    dbus_message_iter_init_append (message, &iter);

    if (!dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY,
					   sig, &listIter))
	return;

    for (i = 0; i < value->list.nValue; i++)
    {
	switch (value->list.type) {
	case CompOptionTypeInt:
	    dbus_message_iter_append_basic (&listIter,
					    sig[0],
					    &value->list.value[i].i);
	    break;
	case CompOptionTypeFloat:
	    dbus_message_iter_append_basic (&listIter,
					    sig[0],
					    &value->list.value[i].f);
	    break;
	case CompOptionTypeBool:
	    dbus_message_iter_append_basic (&listIter,
					    sig[0],
					    &value->list.value[i].b);
	    break;
	case CompOptionTypeString:
	    dbus_message_iter_append_basic (&listIter,
					    sig[0],
					    &value->list.value[i].s);
	    break;
	case CompOptionTypeKey:
	    s = keyActionToString ((CompDisplay *) object,
				   &value->list.value[i].action);
	    if (s)
	    {
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
		free (s);
	    }
	    break;
	case CompOptionTypeButton:
	    s = buttonActionToString ((CompDisplay *) object,
				      &value->list.value[i].action);
	    if (s)
	    {
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
		free (s);
	    }
	    break;
	case CompOptionTypeEdge:
	    s = edgeMaskToString (value->list.value[i].action.edgeMask);
	    if (s)
	    {
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
		free (s);
	    }
	    break;
	case CompOptionTypeBell:
	    dbus_message_iter_append_basic (&listIter,
					    sig[0],
					    &value->list.value[i].action.bell);
	    break;
	case CompOptionTypeMatch:
	    s = matchToString (&value->list.value[i].match);
	    if (s)
	    {
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
		free (s);
	    }
	    break;
	case CompOptionTypeColor:
	    s = colorToString (value->list.value[i].c);
	    if (s)
	    {
		dbus_message_iter_append_basic (&listIter, sig[0], &s);
		free (s);
	    }
	    break;
	default:
	    break;
	}
    }

    dbus_message_iter_close_container (&iter, &listIter);
}

static void
dbusAppendOptionValue (CompObject      *object,
		       DBusMessage     *message,
		       CompOptionType  type,
		       CompOptionValue *value)
{
    if (type == CompOptionTypeList)
    {
	dbusAppendListOptionValue (object, message, type, value);
    }
    else
    {
	dbusAppendSimpleOptionValue (object, message, type, value);
    }
}

/*
 * 'Get' can be used to retrieve the value of any existing option.
 *
 * Example (will retrieve the current value of command0 option):
 *
 * dbus-send --print-reply --type=method_call	    \
 * --dest=org.freedesktop.compiz		    \
 * /org/freedesktop/compiz/core/allscreens/command0 \
 * org.freedesktop.compiz.get
 */
static Bool
dbusHandleGetOptionMessage (DBusConnection *connection,
			    DBusMessage    *message,
			    char	   **path)
{
    CompObject  *object;
    CompOption  *option;
    int	        nOption = 0;
    DBusMessage *reply = NULL;

    option = dbusGetOptionsFromPath (path, &object, NULL, &nOption);

    while (nOption--)
    {
	if (strcmp (option->name, path[2]) == 0)
	{
	    reply = dbus_message_new_method_return (message);
	    dbusAppendOptionValue (object, reply, option->type,
				   &option->value);
	    break;
	}

	option++;
    }

    if (!reply)
	reply = dbus_message_new_error (message,
					DBUS_ERROR_FAILED,
					"No such option");

    dbus_connection_send (connection, reply, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (reply);

    return TRUE;
}

/*
 * 'List' can be used to retrieve a list of available options.
 *
 * Example:
 *
 * dbus-send --print-reply --type=method_call \
 * --dest=org.freedesktop.compiz	      \
 * /org/freedesktop/compiz/core/allscreens    \
 * org.freedesktop.compiz.list
 */
static Bool
dbusHandleListMessage (DBusConnection *connection,
		       DBusMessage    *message,
		       char	      **path)
{
    CompObject  *object;
    CompOption  *option;
    int	        nOption = 0;
    DBusMessage *reply;

    option = dbusGetOptionsFromPath (path, &object, NULL, &nOption);

    reply = dbus_message_new_method_return (message);

    while (nOption--)
    {
	dbus_message_append_args (reply,
				  DBUS_TYPE_STRING, &option->name,
				  DBUS_TYPE_INVALID);
	option++;
    }

    dbus_connection_send (connection, reply, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (reply);

    return TRUE;
}

/*
 * 'GetMetadata' can be used to retrieve metadata for an option.
 *
 * Example:
 *
 * dbus-send --print-reply --type=method_call		\
 * --dest=org.freedesktop.compiz			\
 * /org/freedesktop/compiz/core/allscreens/run_command0 \
 * org.freedesktop.compiz.getMetadata
 */
static Bool
dbusHandleGetMetadataMessage (DBusConnection *connection,
			      DBusMessage    *message,
			      char	     **path)
{
    CompObject   *object;
    CompOption   *option;
    int	         nOption = 0;
    DBusMessage  *reply = NULL;
    CompMetadata *m;

    option = dbusGetOptionsFromPath (path, &object, &m, &nOption);

    while (nOption--)
    {
	if (strcmp (option->name, path[2]) == 0)
	{
	    CompOptionType restrictionType = option->type;
	    const char	   *type;
	    char	   *shortDesc = NULL;
	    char	   *longDesc = NULL;
	    const char     *blankStr = "";

	    reply = dbus_message_new_method_return (message);

	    type = optionTypeToString (option->type);

	    if (m)
	    {
		if (object->type == COMP_OBJECT_TYPE_SCREEN)
		{
		    shortDesc = compGetShortScreenOptionDescription (m, option);
		    longDesc  = compGetLongScreenOptionDescription (m, option);
		}
		else
		{
		    shortDesc =
			compGetShortDisplayOptionDescription (m, option);
		    longDesc  = compGetLongDisplayOptionDescription (m, option);
		}
	    }

	    if (shortDesc)
		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &shortDesc,
					  DBUS_TYPE_INVALID);
	    else
		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &blankStr,
					  DBUS_TYPE_INVALID);

	    if (longDesc)
		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &longDesc,
					  DBUS_TYPE_INVALID);
	    else
		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &blankStr,
					  DBUS_TYPE_INVALID);

	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &type,
				      DBUS_TYPE_INVALID);

	    if (shortDesc)
		free (shortDesc);
	    if (longDesc)
		free (longDesc);

	    if (restrictionType == CompOptionTypeList)
	    {
		type = optionTypeToString (option->value.list.type);
		restrictionType = option->value.list.type;

		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &type,
					  DBUS_TYPE_INVALID);
	    }

	    switch (restrictionType) {
	    case CompOptionTypeInt:
		dbus_message_append_args (reply,
					  DBUS_TYPE_INT32, &option->rest.i.min,
					  DBUS_TYPE_INT32, &option->rest.i.max,
					  DBUS_TYPE_INVALID);
		break;
	    case CompOptionTypeFloat: {
		double min, max, precision;

		min	  = option->rest.f.min;
		max	  = option->rest.f.max;
		precision = option->rest.f.precision;

		dbus_message_append_args (reply,
					  DBUS_TYPE_DOUBLE, &min,
					  DBUS_TYPE_DOUBLE, &max,
					  DBUS_TYPE_DOUBLE, &precision,
					  DBUS_TYPE_INVALID);
	    } break;
	    default:
		break;
	    }
	    break;
	}

	option++;
    }

    if (!reply)
	reply = dbus_message_new_error (message,
					DBUS_ERROR_FAILED,
					"No such option");

    dbus_connection_send (connection, reply, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (reply);

    return TRUE;
}

/*
 * 'GetPlugins' can be used to retrieve a list of available plugins. There's
 * no guarantee that a plugin in this list can actually be loaded.
 *
 * Example:
 *
 * dbus-send --print-reply --type=method_call \
 * --dest=org.freedesktop.compiz	      \
 * /org/freedesktop/compiz		      \
 * org.freedesktop.compiz.getPlugins
 */
static Bool
dbusHandleGetPluginsMessage (DBusConnection *connection,
			     DBusMessage    *message)
{
    DBusMessage *reply;
    char	**plugins, **p;
    int		n;

    reply = dbus_message_new_method_return (message);

    plugins = availablePlugins (&n);
    if (plugins)
    {
	p = plugins;

	while (n--)
	{
	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, p,
				      DBUS_TYPE_INVALID);
	    free (*p);

	    p++;
	}

	free (plugins);
    }

    dbus_connection_send (connection, reply, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (reply);

    return TRUE;
}

/*
 * 'GetPluginMetadata' can be used to retrieve metadata for a plugin.
 *
 * Example:
 *
 * dbus-send --print-reply --type=method_call \
 * --dest=org.freedesktop.compiz	      \
 * /org/freedesktop/compiz		      \
 * org.freedesktop.compiz.getPluginMetadata   \
 * string:'png'
 */
static Bool
dbusHandleGetPluginMetadataMessage (DBusConnection *connection,
				    DBusMessage    *message)
{
    DBusMessage     *reply;
    DBusMessageIter iter;
    char	    *name;
    CompPlugin	    *p, *loadedPlugin = NULL;

    if (!dbus_message_iter_init (message, &iter))
	return FALSE;

    if (!dbusTryGetValueWithType (&iter,
				  DBUS_TYPE_STRING,
				  &name))
	return FALSE;

    p = findActivePlugin (name);
    if (!p)
	p = loadedPlugin = loadPlugin (name);

    if (p)
    {
	Bool	   initializedPlugin = TRUE;
	char	   *shortDesc = NULL;
	char	   *longDesc = NULL;
	const char *blankStr = "";

	reply = dbus_message_new_method_return (message);

	if (loadedPlugin)
	{
	    if (!(*p->vTable->init) (p))
		initializedPlugin = FALSE;
	}

	if (initializedPlugin && p->vTable->getMetadata)
	{
	    CompMetadata *m;

	    m = (*p->vTable->getMetadata) (p);
	    if (m)
	    {
		shortDesc = compGetShortPluginDescription (m);
		longDesc  = compGetLongPluginDescription (m);
	    }
	}

	dbus_message_append_args (reply,
				  DBUS_TYPE_STRING, &p->vTable->name,
				  DBUS_TYPE_INVALID);

	if (shortDesc)
	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &shortDesc,
				      DBUS_TYPE_INVALID);
	else
	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &blankStr,
				      DBUS_TYPE_INVALID);

	if (longDesc)
	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &longDesc,
				      DBUS_TYPE_INVALID);
	else
	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &blankStr,
				      DBUS_TYPE_INVALID);

	dbus_message_append_args (reply,
				  DBUS_TYPE_BOOLEAN, &initializedPlugin,
				  DBUS_TYPE_INVALID);

	if (shortDesc)
	    free (shortDesc);
	if (longDesc)
	    free (longDesc);

	if (loadedPlugin && initializedPlugin)
	    (*p->vTable->fini) (p);
    }
    else
    {
	char *str;

	str = malloc (strlen (name) + 256);
	if (!str)
	    return FALSE;

	sprintf (str, "Plugin '%s' could not be loaded", name);

	reply = dbus_message_new_error (message,
					DBUS_ERROR_FAILED,
					str);

	free (str);
    }

    if (loadedPlugin)
	unloadPlugin (loadedPlugin);

    dbus_connection_send (connection, reply, NULL);
    dbus_connection_flush (connection);

    dbus_message_unref (reply);

    return TRUE;
}

static DBusHandlerResult
dbusHandleMessage (DBusConnection *connection,
		   DBusMessage    *message,
		   void           *userData)
{
    Bool status = FALSE;
    char **path;

    if (!dbus_message_get_path_decomposed (message, &path))
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!path[0] || !path[1] || !path[2])
    {
	dbus_free_string_array (path);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    /* root messages */
    if (!path[3])
    {
	if (dbus_message_is_method_call (message,
					 DBUS_INTERFACE_INTROSPECTABLE,
					 "Introspect"))
	{
	    if (dbusHandleRootIntrospectMessage (connection, message))
	    {
		dbus_free_string_array (path);
		return DBUS_HANDLER_RESULT_HANDLED;
	    }
	}
	else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
				 COMPIZ_DBUS_GET_PLUGIN_METADATA_MEMBER_NAME))
	{
	    if (dbusHandleGetPluginMetadataMessage (connection, message))
	    {
		dbus_free_string_array (path);
		return DBUS_HANDLER_RESULT_HANDLED;
	    }
	}
	else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_GET_PLUGINS_MEMBER_NAME))
	{
	    if (dbusHandleGetPluginsMessage (connection, message))
	    {
		dbus_free_string_array (path);
		return DBUS_HANDLER_RESULT_HANDLED;
	    }
	}

	dbus_free_string_array (path);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    /* plugin message */
    else if (!path[4])
    {
	if (dbus_message_is_method_call (message,
					 DBUS_INTERFACE_INTROSPECTABLE,
					 "Introspect"))
	{
	    if (dbusHandlePluginIntrospectMessage (connection, message,
						   &path[3]))
	    {
		dbus_free_string_array (path);
		return DBUS_HANDLER_RESULT_HANDLED;
	    }
	}

	dbus_free_string_array (path);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    /* screen message */
    else if (!path[5])
    {
	if (dbus_message_is_method_call (message,
					 DBUS_INTERFACE_INTROSPECTABLE,
					 "Introspect"))
	{
	    if (dbusHandleScreenIntrospectMessage (connection, message,
						   &path[3]))
	    {
		dbus_free_string_array (path);
		return DBUS_HANDLER_RESULT_HANDLED;
	    }
	}
	else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					      COMPIZ_DBUS_LIST_MEMBER_NAME))
	{
	    if (dbusHandleListMessage (connection, message, &path[3]))
	    {
		dbus_free_string_array (path);
		return DBUS_HANDLER_RESULT_HANDLED;
	    }
	}

	dbus_free_string_array (path);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    /* option message */
    if (dbus_message_is_method_call (message, DBUS_INTERFACE_INTROSPECTABLE,
				     "Introspect"))
    {
	status = dbusHandleOptionIntrospectMessage (connection, message,
						    &path[3]);
    }
    else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_ACTIVATE_MEMBER_NAME))
    {
	status = dbusHandleActionMessage (connection, message, &path[3], TRUE);
    }
    else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_DEACTIVATE_MEMBER_NAME))
    {
	status = dbusHandleActionMessage (connection, message, &path[3],
					  FALSE);
    }
    else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_SET_MEMBER_NAME))
    {
	status = dbusHandleSetOptionMessage (connection, message, &path[3]);
    }
    else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_GET_MEMBER_NAME))
    {
	status = dbusHandleGetOptionMessage (connection, message, &path[3]);
    }
    else if (dbus_message_is_method_call (message, COMPIZ_DBUS_INTERFACE,
					  COMPIZ_DBUS_GET_METADATA_MEMBER_NAME))
    {
	status = dbusHandleGetMetadataMessage (connection, message, &path[3]);
    }

    dbus_free_string_array (path);

    if (status)
	return DBUS_HANDLER_RESULT_HANDLED;

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static Bool
dbusProcessMessages (void *data)
{
    DBusDispatchStatus status;

    DBUS_CORE (&core);

    do
    {
	dbus_connection_read_write_dispatch (dc->connection, 0);
	status = dbus_connection_get_dispatch_status (dc->connection);
    }
    while (status == DBUS_DISPATCH_DATA_REMAINS);

    return TRUE;
}

static void
dbusSendChangeSignalForOption (CompObject *object,
			       CompOption *o,
			       const char *plugin)
{
    DBusMessage *signal;
    char	*name, path[256];

    DBUS_CORE (&core);

    if (!o)
	return;

    name = compObjectName (object);
    if (name)
    {
	sprintf (path, "%s/%s/%s%s/%s", COMPIZ_DBUS_ROOT_PATH,
		 plugin, compObjectTypeName (object->type), name, o->name);

	free (name);
    }
    else
	sprintf (path, "%s/%s/%s/%s", COMPIZ_DBUS_ROOT_PATH,
		 plugin, compObjectTypeName (object->type), o->name);

    signal = dbus_message_new_signal (path,
				      COMPIZ_DBUS_SERVICE_NAME,
				      COMPIZ_DBUS_CHANGED_SIGNAL_NAME);

    dbusAppendOptionValue (object, signal, o->type, &o->value);

    dbus_connection_send (dc->connection, signal, NULL);
    dbus_connection_flush (dc->connection);

    dbus_message_unref (signal);
}

static Bool
dbusGetPathDecomposed (char *data,
		       char ***path,
		       int  *count)
{
    char **retval;
    char *temp;
    char *token;
    int nComponents;
    int i;

    nComponents = 0;
    if (strlen (data) > 1)
    {
	i = 0;
	while (i < strlen (data))
	{
            if (data[i] == '/')
		nComponents += 1;
	    ++i;
	}
    }

    retval = malloc (sizeof (char*) * (nComponents + 1));

    if (nComponents == 0)
    {
	retval[0] = malloc (sizeof (char));
	retval[0][0] = '\0';
	*path  = retval;
	*count = 1;

	return TRUE;
    }

    temp = strdup (data);

    i = 0;
    token = strtok (temp, "/");
    while (token != NULL)
    {
	retval[i] = strdup (token);
	token = strtok (NULL, "/");
	i++;
    }
    retval[i] = malloc (sizeof (char));
    retval[i][0] = '\0';

    free (temp);

    *path  = retval;
    *count = i + 1;

    return TRUE;
}

static void
dbusFreePathDecomposed (char **path,
			int  count)
{
    int i;

    for (i = 0; i < count; i++)
	free (path[i]);

    free (path);
}

/* dbus registration */

static Bool
dbusRegisterOptions (DBusConnection *connection,
		     char           *screenPath)
{
    CompOption *option = NULL;
    int        nOptions;
    char       objectPath[256];
    char       **path;
    int        count;

    dbusGetPathDecomposed (screenPath, &path, &count);

    option = dbusGetOptionsFromPath (&path[3], NULL, NULL, &nOptions);

    if (!option) {
	dbusFreePathDecomposed (path, count);
	return FALSE;
    }

    while (nOptions--)
    {
	snprintf (objectPath, 256, "%s/%s", screenPath, option->name);

	dbus_connection_register_object_path (connection, objectPath,
					      &dbusMessagesVTable, 0);
	option++;
    }

    dbusFreePathDecomposed (path, count);

    return TRUE;
}

static Bool
dbusUnregisterOptions (DBusConnection *connection,
		       char           *screenPath)
{
    CompOption *option = NULL;
    int        nOptions;
    char       objectPath[256];
    char       **path;
    int        count;

    dbusGetPathDecomposed (screenPath, &path, &count);

    option = dbusGetOptionsFromPath (&path[3], NULL, NULL, &nOptions);

    dbusFreePathDecomposed (path, count);

    if (!option)
	return FALSE;

    while (nOptions--)
    {
	snprintf (objectPath, 256, "%s/%s", screenPath, option->name);

	dbus_connection_unregister_object_path (connection, objectPath);
	option++;
    }

    return TRUE;
}

static void
dbusRegisterPluginForDisplay (DBusConnection *connection,
			      CompDisplay    *d,
			      char           *pluginName)
{
    char       objectPath[256];

    /* register plugin root path */
    snprintf (objectPath, 256, "%s/%s", COMPIZ_DBUS_ROOT_PATH, pluginName);
    dbus_connection_register_object_path (connection, objectPath,
					  &dbusMessagesVTable, d);

    /* register plugin/screen path */
    snprintf (objectPath, 256, "%s/%s/%s", COMPIZ_DBUS_ROOT_PATH,
	      pluginName, "allscreens");
    dbus_connection_register_object_path (connection, objectPath,
					  &dbusMessagesVTable, d);
}

static void
dbusRegisterPluginForScreen (DBusConnection *connection,
			     CompScreen     *s,
			     char           *pluginName)
{
    char       objectPath[256];

    /* register plugin/screen path */
    snprintf (objectPath, 256, "%s/%s/screen%d", COMPIZ_DBUS_ROOT_PATH,
	      pluginName, s->screenNum);
    dbus_connection_register_object_path (connection, objectPath,
					  &dbusMessagesVTable, s->display);
}

static void
dbusRegisterPluginsForDisplay (DBusConnection *connection,
			       CompDisplay    *d)
{
    unsigned int i;
    char         path[256];

    DBUS_DISPLAY (d);

    for (i = 0; i < dd->nPlugins; i++)
    {
	snprintf (path, 256, "%s/%s/allscreens",
		  COMPIZ_DBUS_ROOT_PATH, dd->pluginList[i]);

	dbusRegisterPluginForDisplay (connection, d, dd->pluginList[i]);
	dbusRegisterOptions (connection, path);
    }
}

static void
dbusRegisterPluginsForScreen (DBusConnection *connection,
			      CompScreen    *s)
{
    unsigned int i;
    char         path[256];

    DBUS_DISPLAY (s->display);

    for (i = 0; i < dd->nPlugins; i++)
    {
	snprintf (path, 256, "%s/%s/screen%d",
		  COMPIZ_DBUS_ROOT_PATH, dd->pluginList[i], s->screenNum);
	dbusRegisterPluginForScreen (connection, s, dd->pluginList[i]);
	dbusRegisterOptions (connection, path);
    }
}

static void
dbusUnregisterPluginForDisplay (DBusConnection *connection,
			        CompDisplay    *d,
			        char           *pluginName)
{
    char objectPath[256];

    snprintf (objectPath, 256, "%s/%s/%s", COMPIZ_DBUS_ROOT_PATH,
	      pluginName, "allscreens");

    dbusUnregisterOptions (connection, objectPath);
    dbus_connection_unregister_object_path (connection, objectPath);

    snprintf (objectPath, 256, "%s/%s", COMPIZ_DBUS_ROOT_PATH, pluginName);
    dbus_connection_unregister_object_path (connection, objectPath);
}

static void
dbusUnregisterPluginsForDisplay (DBusConnection *connection,
			         CompDisplay    *d)
{
    unsigned int i;

    DBUS_DISPLAY (d);

    for (i = 0; i < dd->nPlugins; i++)
	dbusUnregisterPluginForDisplay (connection, d, dd->pluginList[i]);
}

static void
dbusUnregisterPluginForScreen (DBusConnection *connection,
			       CompScreen     *s,
			       char           *pluginName)
{
    char objectPath[256];

    snprintf (objectPath, 256, "%s/%s/screen%d", COMPIZ_DBUS_ROOT_PATH,
	      pluginName, s->screenNum);

    dbusUnregisterOptions (connection, objectPath);
    dbus_connection_unregister_object_path (connection, objectPath);
}

static void
dbusUnregisterPluginsForScreen (DBusConnection *connection,
			        CompScreen     *s)
{
    unsigned int i;

    DBUS_DISPLAY (s->display);

    for (i = 0; i < dd->nPlugins; i++)
	dbusUnregisterPluginForScreen (connection, s, dd->pluginList[i]);
}

static CompBool
dbusInitPluginForDisplay (CompPlugin  *p,
			  CompDisplay *d)
{
    char objectPath[256];

    DBUS_CORE (&core);

    snprintf (objectPath, 256, "%s/%s/%s", COMPIZ_DBUS_ROOT_PATH,
	      p->vTable->name, "allscreens");
    dbusRegisterOptions (dc->connection, objectPath);

    return TRUE;
}

static Bool
dbusInitPluginForScreen (CompPlugin *p,
			 CompScreen *s)
{
    char objectPath[256];

    DBUS_CORE (&core);

    snprintf (objectPath, 256, "%s/%s/screen%d", COMPIZ_DBUS_ROOT_PATH,
	      p->vTable->name, s->screenNum);
    dbusRegisterOptions (dc->connection, objectPath);

    return TRUE;
}

static CompBool
dbusInitPluginForObject (CompPlugin *p,
			 CompObject *o)
{
    CompBool status;

    DBUS_CORE (&core);

    UNWRAP (dc, &core, initPluginForObject);
    status = (*core.initPluginForObject) (p, o);
    WRAP (dc, &core, initPluginForObject, dbusInitPluginForObject);

    if (status && p->vTable->getObjectOptions)
    {
	static InitPluginForObjectProc dispTab[] = {
	    (InitPluginForObjectProc) 0, /* InitPluginForCore */
	    (InitPluginForObjectProc) dbusInitPluginForDisplay,
	    (InitPluginForObjectProc) dbusInitPluginForScreen
	};

	RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
    }

    return status;
}

static CompBool
dbusSetOptionForPlugin (CompObject      *object,
			const char      *plugin,
			const char      *name,
			CompOptionValue *value)
{
    Bool status;

    DBUS_CORE (&core);

    UNWRAP (dc, &core, setOptionForPlugin);
    status = (*core.setOptionForPlugin) (object, plugin, name, value);
    WRAP (dc, &core, setOptionForPlugin, dbusSetOptionForPlugin);

    if (status)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getObjectOptions)
	{
	    CompOption *option;
	    int	       nOption;

	    option = (*p->vTable->getObjectOptions) (p, object, &nOption);
	    dbusSendChangeSignalForOption (object,
					   compFindOption (option,
							   nOption,
							   name, 0),
					   p->vTable->name);

	    if (object->type == COMP_OBJECT_TYPE_DISPLAY &&
		strcmp (p->vTable->name, "core") == 0 &&
		strcmp (name, "active_plugins") == 0)
	    {
		CompScreen *s;

		CORE_DISPLAY (object);

		dbusUnregisterPluginsForDisplay (dc->connection, d);
		for (s = d->screens; s; s = s->next)
		    dbusUnregisterPluginsForScreen (dc->connection, s);

		dbusUpdatePluginList (d);

		dbusRegisterPluginsForDisplay (dc->connection, d);
		for (s = d->screens; s; s = s->next)
		    dbusRegisterPluginsForScreen (dc->connection, s);
	    }
	}
    }

    return status;
}

static void
dbusSendPluginsChangedSignal (const char *name,
			      void	 *closure)
{
    DBusMessage *signal;

    DBUS_CORE (&core);

    signal = dbus_message_new_signal (COMPIZ_DBUS_ROOT_PATH,
				      COMPIZ_DBUS_SERVICE_NAME,
				      COMPIZ_DBUS_PLUGINS_CHANGED_SIGNAL_NAME);

    dbus_connection_send (dc->connection, signal, NULL);
    dbus_connection_flush (dc->connection);

    dbus_message_unref (signal);
}

static Bool
dbusInitCore (CompPlugin *p,
	      CompCore   *c)
{
    DbusCore    *dc;
    DBusError   error;
    dbus_bool_t status;
    int		fd, ret, mask;
    char        *home, *plugindir;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    dc = malloc (sizeof (DbusCore));
    if (!dc)
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	free (dc);
	return FALSE;
    }

    dbus_error_init (&error);

    dc->connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set (&error))
    {
	compLogMessage ("dbus", CompLogLevelError,
			"dbus_bus_get error: %s", error.message);

	dbus_error_free (&error);
	free (dc);

	return FALSE;
    }

    ret = dbus_bus_request_name (dc->connection,
				 COMPIZ_DBUS_SERVICE_NAME,
				 DBUS_NAME_FLAG_REPLACE_EXISTING |
				 DBUS_NAME_FLAG_ALLOW_REPLACEMENT,
				 &error);

    if (dbus_error_is_set (&error))
    {
	compLogMessage ("dbus", CompLogLevelError,
			"dbus_bus_request_name error: %s", error.message);

	/* dbus_connection_unref (dc->connection); */
	dbus_error_free (&error);
	free (dc);

	return FALSE;
    }

    dbus_error_free (&error);

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
	compLogMessage ("dbus", CompLogLevelError,
			"dbus_bus_request_name reply is not primary owner");

	/* dbus_connection_unref (dc->connection); */
	free (dc);

	return FALSE;
    }

    status = dbus_connection_get_unix_fd (dc->connection, &fd);
    if (!status)
    {
	compLogMessage ("dbus", CompLogLevelError,
			"dbus_connection_get_unix_fd failed");

	/* dbus_connection_unref (dc->connection); */
	free (dc);

	return FALSE;
    }

    dc->watchFdHandle = compAddWatchFd (fd,
					POLLIN | POLLPRI | POLLHUP | POLLERR,
					dbusProcessMessages,
					0);

    mask = NOTIFY_CREATE_MASK | NOTIFY_DELETE_MASK | NOTIFY_MOVE_MASK;

    dc->fileWatch[DBUS_FILE_WATCH_CURRENT] =
	addFileWatch (".",
		      mask,
		      dbusSendPluginsChangedSignal,
		      0);
    dc->fileWatch[DBUS_FILE_WATCH_PLUGIN]  =
	addFileWatch (PLUGINDIR,
		      mask,
		      dbusSendPluginsChangedSignal,
		      0);
    dc->fileWatch[DBUS_FILE_WATCH_HOME] = 0;

    home = getenv ("HOME");
    if (home)
    {
	plugindir = malloc (strlen (home) + strlen (HOME_PLUGINDIR) + 3);
	if (plugindir)
	{
	    sprintf (plugindir, "%s/%s", home, HOME_PLUGINDIR);

	    dc->fileWatch[DBUS_FILE_WATCH_HOME]  =
		addFileWatch (plugindir,
			      mask,
			      dbusSendPluginsChangedSignal,
			      0);

	    free (plugindir);
	}
    }

    WRAP (dc, c, initPluginForObject, dbusInitPluginForObject);
    WRAP (dc, c, setOptionForPlugin, dbusSetOptionForPlugin);

    c->base.privates[corePrivateIndex].ptr = dc;

    /* register the objects */
    dbus_connection_register_object_path (dc->connection,
					  COMPIZ_DBUS_ROOT_PATH,
					  &dbusMessagesVTable, 0);

    return TRUE;
}

static void
dbusFiniCore (CompPlugin *p,
	      CompCore   *c)
{
    int i;

    DBUS_CORE (c);

    for (i = 0; i < DBUS_FILE_WATCH_NUM; i++)
	removeFileWatch (dc->fileWatch[i]);

    freeDisplayPrivateIndex (displayPrivateIndex);

    compRemoveWatchFd (dc->watchFdHandle);

    dbus_bus_release_name (dc->connection, COMPIZ_DBUS_SERVICE_NAME, NULL);

    /*
      can't unref the connection returned by dbus_bus_get as it's
      shared and we can't know if it's closed or not.

      dbus_connection_unref (dc->connection);
    */

    UNWRAP (dc, c, initPluginForObject);
    UNWRAP (dc, c, setOptionForPlugin);

    free (dc);
}

static Bool
dbusInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    DbusDisplay *dd;

    DBUS_CORE (&core);

    dd = malloc (sizeof (DbusDisplay));
    if (!dd)
	return FALSE;

    dd->pluginList = NULL;
    dd->nPlugins   = 0;

    d->base.privates[displayPrivateIndex].ptr = dd;

    dbusUpdatePluginList (d);
    dbusRegisterPluginsForDisplay (dc->connection, d);

    return TRUE;
}

static void
dbusFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    DBUS_CORE (&core);
    DBUS_DISPLAY (d);

    dbusUnregisterPluginsForDisplay (dc->connection, d);

    if (dd->pluginList)
    {
	unsigned int i;

	for (i = 0; i < dd->nPlugins; i++)
	    free (dd->pluginList[i]);
	free (dd->pluginList);
    }

    free (dd);
}

static Bool
dbusInitScreen (CompPlugin *p,
		CompScreen *s)
{
    DBUS_CORE (&core);

    dbusRegisterPluginsForScreen (dc->connection, s);

    return TRUE;
}

static void
dbusFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    DBUS_CORE (&core);

    dbusUnregisterPluginsForScreen (dc->connection, s);
}

static CompBool
dbusInitObject (CompPlugin *p,
		CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) dbusInitCore,
	(InitPluginObjectProc) dbusInitDisplay,
	(InitPluginObjectProc) dbusInitScreen
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
dbusFiniObject (CompPlugin *p,
		CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) dbusFiniCore,
	(FiniPluginObjectProc) dbusFiniDisplay,
	(FiniPluginObjectProc) dbusFiniScreen
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
dbusInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&dbusMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    corePrivateIndex = allocateCorePrivateIndex ();
    if (corePrivateIndex < 0)
    {
	compFiniMetadata (&dbusMetadata);
	return FALSE;
    }

    return TRUE;
}

static void
dbusFini (CompPlugin *p)
{
    freeCorePrivateIndex (corePrivateIndex);
    compFiniMetadata (&dbusMetadata);
}

static CompMetadata *
dbusGetMetadata (CompPlugin *plugin)
{
    return &dbusMetadata;
}

CompPluginVTable dbusVTable = {
    "dbus",
    dbusGetMetadata,
    dbusInit,
    dbusFini,
    dbusInitObject,
    dbusFiniObject,
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &dbusVTable;
}
