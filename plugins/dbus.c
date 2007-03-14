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

#include <compiz.h>

#define COMPIZ_DBUS_SERVICE_NAME	            "org.freedesktop.compiz"

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

typedef enum {
    DbusActionIndexKeyBinding    = 0,
    DbusActionIndexButtonBinding = 1,
    DbusActionIndexBell          = 2,
    DbusActionIndexEdge          = 3,
    DbusActionIndexEdgeButton    = 4
} DbusActionIndex;

static int displayPrivateIndex;

typedef struct _DbusDisplay {
    int screenPrivateIndex;

    DBusConnection    *connection;
    CompWatchFdHandle watchFdHandle;

    CompFileWatchHandle fileWatch[DBUS_FILE_WATCH_NUM];

    SetDisplayOptionProc	  setDisplayOption;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;
} DbusDisplay;

typedef struct _DbusScreen {
    SetScreenOptionProc		 setScreenOption;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;
} DbusScreen;

#define GET_DBUS_DISPLAY(d)				     \
    ((DbusDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define DBUS_DISPLAY(d)			   \
    DbusDisplay *dd = GET_DBUS_DISPLAY (d)

#define GET_DBUS_SCREEN(s, dd)				         \
    ((DbusScreen *) (s)->privates[(dd)->screenPrivateIndex].ptr)

#define DBUS_SCREEN(s)						        \
    DbusScreen *ds = GET_DBUS_SCREEN (s, GET_DBUS_DISPLAY (s->display))


static CompOption *
dbusGetOptionsFromPath (CompDisplay *d,
			char	    **path,
			CompScreen  **return_screen,
			int	    *nOption)
{
    CompScreen *s = NULL;

    if (strcmp (path[1], "allscreens"))
    {
	int screenNum;

	if (sscanf (path[1], "screen%d", &screenNum) != 1)
	    return FALSE;

	for (s = d->screens; s; s = s->next)
	    if (s->screenNum == screenNum)
		break;

	if (!s)
	    return NULL;
    }

    if (return_screen)
	*return_screen = s;

    if (strcmp (path[0], "core") == 0)
    {
	if (s)
	    return compGetScreenOptions (s, nOption);
	else
	    return compGetDisplayOptions (d, nOption);
    }
    else
    {
	CompPlugin *p;

	for (p = getPlugins (); p; p = p->next)
	    if (strcmp (p->vTable->name, path[0]) == 0)
		break;

	if (!p)
	    return NULL;

	if (s)
	{
	    if (p->vTable->getScreenOptions)
		return (*p->vTable->getScreenOptions) (s, nOption);
	}
	else
	{
	    if (p->vTable->getDisplayOptions)
		return (*p->vTable->getDisplayOptions) (d, nOption);
	}
    }

    return NULL;
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
			 CompDisplay	*d,
			 char	        **path,
			 Bool           activate)
{
    CompOption *option;
    int	       nOption;

    option = dbusGetOptionsFromPath (d, path, NULL, &nOption);
    if (!option)
	return FALSE;

    while (nOption--)
    {
	if (strcmp (option->name, path[2]) == 0)
	{
	    CompOption	    *argument = NULL;
	    int		    i, nArgument = 0;
	    DBusMessageIter iter;

	    if (option->type != CompOptionTypeAction)
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
		(*option->value.action.initiate) (d,
						  &option->value.action,
						  0,
						  argument, nArgument);
	    }
	    else
	    {
		(*option->value.action.terminate) (d,
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
dbusGetOptionValue (DBusMessageIter *iter,
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
			    CompDisplay	   *d,
			    char	   **path)
{
    CompScreen *s;
    CompOption *option;
    int	       nOption;

    option = dbusGetOptionsFromPath (d, path, &s, &nOption);
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
			if (dbusGetOptionValue (&aiter,
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
		DbusActionIndex	actionIndex = DbusActionIndexKeyBinding;

		do
		{
		    if (option->type == CompOptionTypeAction)
		    {
			CompAction *a = &value.action;
			char	   *str;

			status = TRUE;

			switch (actionIndex) {
			case DbusActionIndexKeyBinding:
			    if (dbusTryGetValueWithType (&iter,
							 DBUS_TYPE_STRING,
							 &str))
			    {
				if (stringToKeyBinding (d, str, &a->key))
				    a->type |= CompBindingTypeKey;
			    }
			    break;
			case DbusActionIndexButtonBinding:
			    if (dbusTryGetValueWithType (&iter,
							 DBUS_TYPE_STRING,
							 &str))
			    {
				if (stringToButtonBinding (d, str, &a->button))
				    a->type |= CompBindingTypeButton;
			    }
			    break;
			case DbusActionIndexBell:
			    dbusTryGetValueWithType (&iter,
						     DBUS_TYPE_BOOLEAN,
						     &a->bell);
			    break;
			case DbusActionIndexEdge:
			    if (dbusTryGetValueWithType (&iter,
							 DBUS_TYPE_STRING,
							 &str))
			    {
				status |= TRUE;

				while (strlen (str))
				{
				    char *edge;
				    int  len, i = SCREEN_EDGE_NUM;

				    for (;;)
				    {
					edge = edgeToString (--i);
					len  = strlen (edge);

					if (strncasecmp (str, edge, len) == 0)
					{
					    a->edgeMask |= 1 << i;

					    str += len;
					    break;
					}

					if (!i)
					{
					    str++;
					    break;
					}
				    }
				}
			    }
			    break;
			case DbusActionIndexEdgeButton:
			    if (dbusTryGetValueWithType (&iter,
							 DBUS_TYPE_INT32,
							 &a->edgeButton))
			    {
				if (a->edgeButton)
				    a->type |= CompBindingTypeEdgeButton;
			    }
			default:
			    break;
			}

			actionIndex++;
		    }
		    else if (dbusGetOptionValue (&iter, option->type, &value))
		    {
			status |= TRUE;
		    }
		} while (dbus_message_iter_next (&iter));
	    }

	    if (status)
	    {
		if (s)
		{
		    if (strcmp (path[0], "core"))
			(*s->setScreenOptionForPlugin) (s,
							path[0],
							option->name,
							&value);
		    else
			(*s->setScreenOption) (s, option->name, &value);
		}
		else
		{
		    if (strcmp (path[0], "core"))
			(*d->setDisplayOptionForPlugin) (d,
							 path[0],
							 option->name,
							 &value);
		    else
			(*d->setDisplayOption) (d, option->name, &value);
		}

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
dbusAppendSimpleOptionValue (DBusMessage     *message,
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
dbusAppendOptionValue (CompDisplay     *d,
		       DBusMessage     *message,
		       CompOptionType  type,
		       CompOptionValue *value)
{
    int	i;

    if (type == CompOptionTypeList)
    {
	for (i = 0; i < value->list.nValue; i++)
	    dbusAppendSimpleOptionValue (message, value->list.type,
					 &value->list.value[i]);
    }
    else if (type == CompOptionTypeAction)
    {
	CompAction *a = &value->action;
	char	   *key = "Disabled";
	char	   *button = "Disabled";
	char	   *edge = "";
	char	   *keyValue = NULL;
	char	   *buttonValue = NULL;
	char	   *edgeValue = NULL;
	int	   edgeButton = 0;

	if (a->type & CompBindingTypeKey)
	    key = keyValue = keyBindingToString (d, &a->key);

	if (a->type & CompBindingTypeButton)
	    button = buttonValue = buttonBindingToString (d, &a->button);

	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	{
	    if (a->edgeMask & (1 << i))
	    {
		if (strlen (edge))
		{
		    char *e;

		    e = malloc (strlen (edge) + strlen (edgeToString (i)) + 2);
		    if (e)
		    {
			sprintf (e, "%s,%s", edge, edgeToString (i));
			if (edgeValue)
			    free (edgeValue);

			edge = edgeValue = e;
		    }
		}
		else
		{
		    edge = edgeToString (i);
		}
	    }
	}

	if (a->type & CompBindingTypeEdgeButton)
	    edgeButton = a->edgeButton;

	dbus_message_append_args (message,
				  DBUS_TYPE_STRING, &key,
				  DBUS_TYPE_STRING, &button,
				  DBUS_TYPE_BOOLEAN, &a->bell,
				  DBUS_TYPE_STRING, &edge,
				  DBUS_TYPE_INT32, &edgeButton,
				  DBUS_TYPE_INVALID);

	if (keyValue)
	    free (keyValue);

	if (buttonValue)
	    free (buttonValue);

	if (edgeValue)
	    free (edgeValue);
    }
    else
    {
	dbusAppendSimpleOptionValue (message, type, value);
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
			    CompDisplay	   *d,
			    char	   **path)
{
    CompScreen  *s;
    CompOption  *option;
    int	        nOption = 0;
    DBusMessage *reply = NULL;

    option = dbusGetOptionsFromPath (d, path, &s, &nOption);

    while (nOption--)
    {
	if (strcmp (option->name, path[2]) == 0)
	{
	    reply = dbus_message_new_method_return (message);
	    dbusAppendOptionValue (d, reply, option->type, &option->value);
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
		       CompDisplay    *d,
		       char	      **path)
{
    CompScreen  *s;
    CompOption  *option;
    int	        nOption = 0;
    DBusMessage *reply;

    option = dbusGetOptionsFromPath (d, path, &s, &nOption);

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
			      CompDisplay    *d,
			      char	     **path)
{
    CompScreen  *s;
    CompOption  *option;
    int	        nOption = 0;
    DBusMessage *reply = NULL;

    option = dbusGetOptionsFromPath (d, path, &s, &nOption);

    while (nOption--)
    {
	if (strcmp (option->name, path[2]) == 0)
	{
	    CompOptionType restrictionType = option->type;
	    char	   *type;

	    reply = dbus_message_new_method_return (message);

	    type = optionTypeToString (option->type);

	    dbus_message_append_args (reply,
				      DBUS_TYPE_STRING, &option->shortDesc,
				      DBUS_TYPE_STRING, &option->longDesc,
				      DBUS_TYPE_STRING, &type,
				      DBUS_TYPE_INVALID);

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
	    case CompOptionTypeString: {
		DBusMessageIter iter;
		DBusMessageIter listIter;
		char		sig[2];

		sig[0] = DBUS_TYPE_STRING;
		sig[1] = '\0';

		dbus_message_iter_init_append (reply, &iter);
		dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY,
						  sig, &listIter);

		if (option->rest.s.nString)
		{
		    char *possible;
		    int  i;

		    for (i = 0; i < option->rest.s.nString; i++)
		    {
			possible = option->rest.s.string[i];

			dbus_message_iter_append_basic (&listIter,
							DBUS_TYPE_STRING,
							&possible);
		    }
		}

		dbus_message_iter_close_container (&iter, &listIter);
	    }
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
			     DBusMessage    *message,
			     CompDisplay    *d)
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
				    DBusMessage    *message,
				    CompDisplay    *d)
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
	CompPluginDep	  *deps;
	int		  nDeps;
	CompPluginFeature *features;
	int		  nFeatures;
	dbus_bool_t	  supportedABI;
	int		  version;
	DBusMessageIter   iter;
	DBusMessageIter   listIter;
	char		  sig[2];

	reply = dbus_message_new_method_return (message);

	dbus_message_append_args (reply,
				  DBUS_TYPE_STRING, &p->vTable->name,
				  DBUS_TYPE_STRING, &p->vTable->shortDesc,
				  DBUS_TYPE_STRING, &p->vTable->longDesc,
				  DBUS_TYPE_INVALID);

	version = (*p->vTable->getVersion) (p, ABIVERSION);
	supportedABI = (version == ABIVERSION) ? TRUE : FALSE;

	dbus_message_append_args (reply,
				  DBUS_TYPE_BOOLEAN, &supportedABI,
				  DBUS_TYPE_INVALID);

	sig[0] = DBUS_TYPE_STRING;
	sig[1] = '\0';

	dbus_message_iter_init_append (reply, &iter);
	dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY,
					  sig, &listIter);

	deps  = p->vTable->deps;
	nDeps = p->vTable->nDeps;

	while (nDeps--)
	{
	    char *str;

	    str = malloc ((strlen (deps->name) + 10) * sizeof (char));
	    if (str)
	    {
		switch (deps->rule) {
		case CompPluginRuleBefore:
		    sprintf (str, "before:%s", deps->name);
		    break;
		case CompPluginRuleAfter:
		    sprintf (str, "after:%s", deps->name);
		    break;
		case CompPluginRuleRequire:
		default:
		    sprintf (str, "required:%s", deps->name);
		    break;
		}

		dbus_message_iter_append_basic (&listIter,
						DBUS_TYPE_STRING,
						&str);

		free (str);
	    }

	    deps++;
	}

	dbus_message_iter_close_container (&iter, &listIter);

	dbus_message_iter_init_append (reply, &iter);
	dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY,
					  sig, &listIter);

	features  = p->vTable->features;
	nFeatures = p->vTable->nFeatures;

	while (nFeatures--)
	{
	    dbus_message_iter_append_basic (&listIter,
					    DBUS_TYPE_STRING,
					    &features->name);

	    features++;
	}

	dbus_message_iter_close_container (&iter, &listIter);
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
    CompDisplay *d = (CompDisplay *) userData;
    Bool	status = FALSE;
    char	**path;
    const char  *service, *interface, *member;

    service   = dbus_message_get_destination (message);
    interface = dbus_message_get_interface (message);
    member    = dbus_message_get_member (message);

    if (!service || !interface || !member)
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_is_method_call (message, interface, member))
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_has_destination (message, COMPIZ_DBUS_SERVICE_NAME))
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_get_path_decomposed (message, &path))
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!path[0] || !path[1] || !path[2])
    {
	dbus_free_string_array (path);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (strcmp (path[0], "org")	        ||
	strcmp (path[1], "freedesktop") ||
	strcmp (path[2], "compiz"))
    {
	dbus_free_string_array (path);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_message_has_member (message,
				 COMPIZ_DBUS_GET_PLUGIN_METADATA_MEMBER_NAME))
    {
	if (dbusHandleGetPluginMetadataMessage (connection, message, d))
	{
	    dbus_free_string_array (path);
	    return DBUS_HANDLER_RESULT_HANDLED;
	}
    }
    else if (dbus_message_has_member (message,
				      COMPIZ_DBUS_GET_PLUGINS_MEMBER_NAME))
    {
	if (dbusHandleGetPluginsMessage (connection, message, d))
	{
	    dbus_free_string_array (path);
	    return DBUS_HANDLER_RESULT_HANDLED;
	}
    }

    if (!path[3] || !path[4])
    {
	dbus_free_string_array (path);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_message_has_member (message, COMPIZ_DBUS_LIST_MEMBER_NAME))
    {
	if (dbusHandleListMessage (connection, message, d, &path[3]))
	{
	    dbus_free_string_array (path);
	    return DBUS_HANDLER_RESULT_HANDLED;
	}
    }

    if (!path[5])
    {
	dbus_free_string_array (path);
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (dbus_message_has_member (message, COMPIZ_DBUS_ACTIVATE_MEMBER_NAME))
    {
	status = dbusHandleActionMessage (connection, message, d, &path[3],
					  TRUE);
    }
    else if (dbus_message_has_member (message,
				      COMPIZ_DBUS_DEACTIVATE_MEMBER_NAME))
    {
	status = dbusHandleActionMessage (connection, message, d, &path[3],
					  FALSE);
    }
    else if (dbus_message_has_member (message, COMPIZ_DBUS_SET_MEMBER_NAME))
    {
	status = dbusHandleSetOptionMessage (connection, message, d, &path[3]);
    }
    else if (dbus_message_has_member (message, COMPIZ_DBUS_GET_MEMBER_NAME))
    {
	status = dbusHandleGetOptionMessage (connection, message, d, &path[3]);
    }
    else if (dbus_message_has_member (message,
				      COMPIZ_DBUS_GET_METADATA_MEMBER_NAME))
    {
	status = dbusHandleGetMetadataMessage (connection, message, d,
					       &path[3]);
    }

    dbus_free_string_array (path);

    if (status)
	return DBUS_HANDLER_RESULT_HANDLED;

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static Bool
dbusProcessMessages (void *data)
{
    CompDisplay	       *d = (CompDisplay *) data;
    DBusDispatchStatus status;

    DBUS_DISPLAY (d);

    do
    {
	dbus_connection_read_write_dispatch (dd->connection, 0);
	status = dbus_connection_get_dispatch_status (dd->connection);
    }
    while (status == DBUS_DISPATCH_DATA_REMAINS);

    return TRUE;
}

static void
dbusSendChangeSignalForOption (CompDisplay     *d,
			       CompOptionType  type,
			       CompOptionValue *value,
			       char	       *path)
{
    DBusMessage *signal;

    DBUS_DISPLAY (d);

    signal = dbus_message_new_signal (path,
				      COMPIZ_DBUS_SERVICE_NAME,
				      COMPIZ_DBUS_CHANGED_SIGNAL_NAME);

    dbusAppendOptionValue (d, signal, type,  value);

    dbus_connection_send (dd->connection, signal, NULL);
    dbus_connection_flush (dd->connection);

    dbus_message_unref (signal);
}

static void
dbusSendChangeSignalForDisplayOption (CompDisplay *d,
				      CompOption  *o,
				      char	  *plugin)
{
    char path[256];

    if (o)
    {
	sprintf (path, "/org/freedesktop/compiz/%s/allscreens/%s",
		 plugin, o->name);
	dbusSendChangeSignalForOption (d, o->type, &o->value, path);
    }
}

static void
dbusSendChangeSignalForScreenOption (CompScreen *s,
				     CompOption *o,
				     char	*plugin)
{
    char path[256];

    if (o)
    {
	sprintf (path, "/org/freedesktop/compiz/%s/screens%d/%s",
		 plugin, s->screenNum, o->name);
	dbusSendChangeSignalForOption (s->display, o->type, &o->value, path);
    }
}

static Bool
dbusSetDisplayOption (CompDisplay     *d,
		      char	      *name,
		      CompOptionValue *value)
{
    Bool status;

    DBUS_DISPLAY (d);

    UNWRAP (dd, d, setDisplayOption);
    status = (*d->setDisplayOption) (d, name, value);
    WRAP (dd, d, setDisplayOption, dbusSetDisplayOption);

    if (status)
    {
	CompOption *option;
	int	   nOption;

	option = compGetDisplayOptions (d, &nOption);
	dbusSendChangeSignalForDisplayOption (d,
					      compFindOption (option, nOption,
							      name, 0),
					      "core");
    }

    return status;
}

static Bool
dbusSetDisplayOptionForPlugin (CompDisplay     *d,
			       char	       *plugin,
			       char	       *name,
			       CompOptionValue *value)
{
    Bool status;

    DBUS_DISPLAY (d);

    UNWRAP (dd, d, setDisplayOptionForPlugin);
    status = (*d->setDisplayOptionForPlugin) (d, plugin, name, value);
    WRAP (dd, d, setDisplayOptionForPlugin, dbusSetDisplayOptionForPlugin);

    if (status)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getDisplayOptions)
	{
	    CompOption *option;
	    int	       nOption;

	    option = (*p->vTable->getDisplayOptions) (d, &nOption);
	    dbusSendChangeSignalForDisplayOption (d,
						  compFindOption (option,
								  nOption,
								  name, 0),
						  p->vTable->name);
	}
    }

    return status;
}

static Bool
dbusSetScreenOption (CompScreen      *s,
		     char	     *name,
		     CompOptionValue *value)
{
    Bool status;

    DBUS_SCREEN (s);

    UNWRAP (ds, s, setScreenOption);
    status = (*s->setScreenOption) (s, name, value);
    WRAP (ds, s, setScreenOption, dbusSetScreenOption);

    if (status)
    {
	CompOption *option;
	int	   nOption;

	option = compGetScreenOptions (s, &nOption);
	dbusSendChangeSignalForScreenOption (s,
					     compFindOption (option,
							     nOption,
							     name, 0),
					     "core");
    }

    return status;
}

static Bool
dbusSetScreenOptionForPlugin (CompScreen      *s,
			      char	      *plugin,
			      char	      *name,
			      CompOptionValue *value)
{
    Bool status;

    DBUS_SCREEN (s);

    UNWRAP (ds, s, setScreenOptionForPlugin);
    status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
    WRAP (ds, s, setScreenOptionForPlugin, dbusSetScreenOptionForPlugin);

    if (status)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getScreenOptions)
	{
	    CompOption *option;
	    int	       nOption;

	    option = (*p->vTable->getScreenOptions) (s, &nOption);
	    dbusSendChangeSignalForScreenOption (s,
						 compFindOption (option,
								 nOption,
								 name, 0),
						 p->vTable->name);
	}
    }

    return status;
}

static void
dbusSendPluginsChangedSignal (const char *name,
			      void	 *closure)
{
    CompDisplay *d = (CompDisplay *) closure;
    DBusMessage *signal;

    DBUS_DISPLAY (d);

    signal = dbus_message_new_signal ("/org/freedesktop/compiz",
				      COMPIZ_DBUS_SERVICE_NAME,
				      COMPIZ_DBUS_PLUGINS_CHANGED_SIGNAL_NAME);

    dbus_connection_send (dd->connection, signal, NULL);
    dbus_connection_flush (dd->connection);

    dbus_message_unref (signal);
}

static Bool
dbusInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    DbusDisplay *dd;
    DBusError	error;
    dbus_bool_t status;
    int		fd, ret, mask;
    char        *home, *plugindir;

    dd = malloc (sizeof (DbusDisplay));
    if (!dd)
	return FALSE;

    dd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (dd->screenPrivateIndex < 0)
    {
	free (dd);
	return FALSE;
    }

    dbus_error_init (&error);

    dd->connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set (&error))
    {
	fprintf (stderr, "%s: dbus_bus_get error: %s\n",
		 programName, error.message);

	dbus_error_free (&error);
	free (dd);

	return FALSE;
    }

    ret = dbus_bus_request_name (dd->connection,
				 COMPIZ_DBUS_SERVICE_NAME,
				 DBUS_NAME_FLAG_REPLACE_EXISTING |
				 DBUS_NAME_FLAG_ALLOW_REPLACEMENT,
				 &error);

    if (dbus_error_is_set (&error))
    {
	fprintf (stderr, "%s: dbus_bus_request_name error: %s\n",
		 programName, error.message);

	/* dbus_connection_unref (dd->connection); */
	dbus_error_free (&error);
	free (dd);

	return FALSE;
    }

    dbus_error_free (&error);

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
	fprintf (stderr, "%s: dbus_bus_request_name reply is not "
		 "primary owner\n", programName);

	/* dbus_connection_unref (dd->connection); */
	free (dd);

	return FALSE;
    }

    status = dbus_connection_add_filter (dd->connection,
					 dbusHandleMessage,
					 d, NULL);
    if (!status)
    {
	fprintf (stderr, "%s: dbus_connection_add_filter failed\n",
		 programName);

	/* dbus_connection_unref (dd->connection); */
	free (dd);

	return FALSE;
    }

    status = dbus_connection_get_unix_fd (dd->connection, &fd);
    if (!status)
    {
	fprintf (stderr, "%s: dbus_connection_get_unix_fd failed\n",
		 programName);

	/* dbus_connection_unref (dd->connection); */
	free (dd);

	return FALSE;
    }

    dd->watchFdHandle = compAddWatchFd (fd,
					POLLIN | POLLPRI | POLLHUP | POLLERR,
					dbusProcessMessages,
					d);

    mask = NOTIFY_CREATE_MASK | NOTIFY_DELETE_MASK | NOTIFY_MOVE_MASK;

    dd->fileWatch[DBUS_FILE_WATCH_CURRENT] =
	addFileWatch (d,
		      ".",
		      mask,
		      dbusSendPluginsChangedSignal,
		      (void *) d);
    dd->fileWatch[DBUS_FILE_WATCH_PLUGIN]  =
	addFileWatch (d,
		      PLUGINDIR,
		      mask,
		      dbusSendPluginsChangedSignal,
		      (void *) d);
    dd->fileWatch[DBUS_FILE_WATCH_HOME] = 0;

    home = getenv ("HOME");
    if (home)
    {
	plugindir = malloc (strlen (home) + strlen (HOME_PLUGINDIR) + 3);
	if (plugindir)
	{
	    sprintf (plugindir, "%s/%s", home, HOME_PLUGINDIR);

	    dd->fileWatch[DBUS_FILE_WATCH_HOME]  =
		addFileWatch (d,
			      plugindir,
			      mask,
			      dbusSendPluginsChangedSignal,
			      (void *) d);

	    free (plugindir);
	}
    }

    WRAP (dd, d, setDisplayOption, dbusSetDisplayOption);
    WRAP (dd, d, setDisplayOptionForPlugin, dbusSetDisplayOptionForPlugin);

    d->privates[displayPrivateIndex].ptr = dd;

    return TRUE;
}

static void
dbusFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    int i;

    DBUS_DISPLAY (d);

    for (i = 0; i < DBUS_FILE_WATCH_NUM; i++)
	removeFileWatch (d, dd->fileWatch[i]);

    compRemoveWatchFd (dd->watchFdHandle);

    dbus_bus_release_name (dd->connection, COMPIZ_DBUS_SERVICE_NAME, NULL);

    /*
      can't unref the connection returned by dbus_bus_get as it's
      shared and we can't know if it's closed or not.

      dbus_connection_unref (dd->connection);
    */

    UNWRAP (dd, d, setDisplayOption);
    UNWRAP (dd, d, setDisplayOptionForPlugin);

    free (dd);
}

static Bool
dbusInitScreen (CompPlugin *p,
		CompScreen *s)
{
    DbusScreen *ds;

    DBUS_DISPLAY (s->display);

    ds = malloc (sizeof (DbusScreen));
    if (!ds)
	return FALSE;

    WRAP (ds, s, setScreenOption, dbusSetScreenOption);
    WRAP (ds, s, setScreenOptionForPlugin, dbusSetScreenOptionForPlugin);

    s->privates[dd->screenPrivateIndex].ptr = ds;

    return TRUE;
}

static void
dbusFiniScreen (CompPlugin *p,
		CompScreen *s)
{
    DBUS_SCREEN (s);

    UNWRAP (ds, s, setScreenOption);
    UNWRAP (ds, s, setScreenOptionForPlugin);

    free (ds);
}

static Bool
dbusInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    return TRUE;
}

static void
dbusFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

static int
dbusGetVersion (CompPlugin *plugin,
		int	   version)
{
    return ABIVERSION;
}

CompPluginVTable dbusVTable = {
    "dbus",
    "Dbus",
    "Dbus Control Backend",
    dbusGetVersion,
    dbusInit,
    dbusFini,
    dbusInitDisplay,
    dbusFiniDisplay,
    dbusInitScreen,
    dbusFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    0, /* Deps */
    0, /* nDeps */
    0, /* Features */
    0  /* nFeatures */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &dbusVTable;
}
