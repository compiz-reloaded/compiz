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

#include <compiz.h>

#define COMPIZ_DBUS_SERVICE_NAME	   "org.freedesktop.compiz"
#define COMPIZ_DBUS_ACTIVATE_MEMBER_NAME   "activate"
#define COMPIZ_DBUS_DEACTIVATE_MEMBER_NAME "deactivate"
#define COMPIZ_DBUS_SET_MEMBER_NAME        "set"
#define COMPIZ_DBUS_GET_MEMBER_NAME        "get"

typedef enum {
    DbusActionIndexKeyBinding    = 0,
    DbusActionIndexButtonBinding = 1,
    DbusActionIndexEdge          = 2,
    DbusActionIndexBell          = 3
} DbusActionIndex;

static int displayPrivateIndex;

typedef struct _DbusDisplay {
    DBusConnection    *connection;
    CompWatchFdHandle watchFdHandle;
} DbusDisplay;

#define GET_DBUS_DISPLAY(d)				     \
    ((DbusDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define DBUS_DISPLAY(d)			   \
    DbusDisplay *dd = GET_DBUS_DISPLAY (d)


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
 * int32:`xwininfo -root | grep id: | awk '{ print $4 }'`     \
 * string:'face' int32:1
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/cube/allscreens/unfold	      \
 * org.freedesktop.compiz.deactivate			      \
 * string:'root'					      \
 * int32:`xwininfo -root | grep id: | awk '{ print $4 }'`     \
 * string:'face' int32:1
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
	    int		    nArgument = 0;
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
			    type     = CompOptionTypeString;

			    dbus_message_iter_get_basic (&iter, &value.s);
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

	    if (argument)
		free (argument);

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
 * string:'dbus' string:'decoration' string:'place'
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
 * string:''						      \
 * boolean:'false'
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
	    DBusMessageIter iter;

	    if (dbus_message_iter_init (message, &iter))
	    {
		CompOptionValue value, tmpValue;
		DbusActionIndex	actionIndex = DbusActionIndexKeyBinding;
		Bool		status = FALSE;

		memset (&value, 0, sizeof (value));

		do
		{
		    if (option->type == CompOptionTypeList)
		    {
			if (dbusGetOptionValue (&iter, option->type, &tmpValue))
			{
			    CompOptionValue *v;

			    v = realloc (value.list.value,
					 sizeof (CompOptionValue) *
					 (value.list.nValue + 1));
			    if (v)
			    {
				v[value.list.nValue++] = tmpValue;
				value.list.value = v;
				status |= TRUE;
			    }
			}
		    }
		    else if (option->type == CompOptionTypeAction)
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
			case DbusActionIndexBell:
			    dbusTryGetValueWithType (&iter,
						     DBUS_TYPE_BOOLEAN,
						     &a->bell);
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

		if (status)
		{
		    if (s)
		    {
			if (strcmp (path[0], "core"))
			    status =
				(*s->setScreenOptionForPlugin) (s,
								path[0],
								option->name,
								&value);
			else
			    status = (*s->setScreenOption) (s, option->name,
							    &value);
		    }
		    else
		    {
			if (strcmp (path[0], "core"))
			    status =
				(*d->setDisplayOptionForPlugin) (d,
								 path[0],
								 option->name,
								 &value);
			else
			    status = (*d->setDisplayOption) (d, option->name,
							     &value);
		    }

		    return status;
		}
		else
		{
		    return FALSE;
		}
	    }
	}

	option++;
    }

    return FALSE;
}

static void
dbusAppendOptionValue (DBusMessage     *message,
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
    default:
	break;
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
	    DBusMessage *reply;
	    int		i;

	    reply = dbus_message_new_method_return (message);

	    if (option->type == CompOptionTypeList)
	    {
		for (i = 0; i < option->value.list.nValue; i++)
		    dbusAppendOptionValue (reply, option->value.list.type,
					   &option->value.list.value[i]);
	    }
	    else if (option->type == CompOptionTypeAction)
	    {
		CompAction *a = &option->value.action;
		char	   *key = "Disabled";
		char	   *button = "Disabled";
		char	   edge[256];

		if (a->type & CompBindingTypeKey)
		    key = keyBindingToString (d, &a->key);

		if (a->type & CompBindingTypeButton)
		    button = buttonBindingToString (d, &a->button);

		*edge = '\0';

		for (i = 0; i < SCREEN_EDGE_NUM; i++)
		    if (a->edgeMask & (1 << i))
			strcpy (edge + strlen (edge), edgeToString (i));

		dbus_message_append_args (reply,
					  DBUS_TYPE_STRING, &key,
					  DBUS_TYPE_STRING, &button,
					  DBUS_TYPE_STRING, &edge,
					  DBUS_TYPE_BOOLEAN, &a->bell,
					  DBUS_TYPE_INVALID);
	    }
	    else
	    {
		dbusAppendOptionValue (reply, option->type, &option->value);
	    }

	    dbus_connection_send (connection, reply, NULL);
	    dbus_connection_flush (connection);

	    dbus_message_unref (reply);

	    return TRUE;
	}

	option++;
    }

    return FALSE;
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

    if (!path[0] || !path[1] || !path[2] || !path[3] || !path[4] || !path[5])
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (strcmp (path[0], "org")	       ||
	strcmp (path[1], "freedesktop")||
	strcmp (path[2], "compiz"))
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

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

static Bool
dbusInitDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    DbusDisplay *dd;
    DBusError	error;
    dbus_bool_t status;
    int		fd, ret;

    dd = malloc (sizeof (DbusDisplay));
    if (!dd)
	return FALSE;

    dbus_error_init (&error);

    dd->connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set (&error))
    {
	fprintf (stderr, "%s: dbus_bus_get error: %s\n",
		 programName, error.message);

	dbus_error_free (&error);

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

	return FALSE;
    }

    dbus_error_free (&error);

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
	fprintf (stderr, "%s: dbus_bus_request_name reply is not "
		 "primary owner\n", programName);

	/* dbus_connection_unref (dd->connection); */

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

	return FALSE;
    }

    status = dbus_connection_get_unix_fd (dd->connection, &fd);
    if (!status)
    {
	fprintf (stderr, "%s: dbus_connection_get_unix_fd failed\n",
		 programName);

	/* dbus_connection_unref (dd->connection); */

	return FALSE;
    }

    dd->watchFdHandle = compAddWatchFd (fd,
					POLLIN | POLLPRI | POLLHUP | POLLERR,
					dbusProcessMessages,
					d);

    d->privates[displayPrivateIndex].ptr = dd;

    return TRUE;
}

static void
dbusFiniDisplay (CompPlugin  *p,
		 CompDisplay *d)
{
    DBUS_DISPLAY (d);

    compRemoveWatchFd (dd->watchFdHandle);

    dbus_bus_release_name (dd->connection, COMPIZ_DBUS_SERVICE_NAME, NULL);

    /*
      can't unref the connection returned by dbus_bus_get as it's
      shared and we can't know if it's closed or not.

      dbus_connection_unref (dd->connection);
    */

    free (dd);
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
    0, /* InitScreen */
    0, /* FiniScreen */
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
