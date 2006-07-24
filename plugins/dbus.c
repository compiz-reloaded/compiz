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

#define COMPIZ_DBUS_SERVICE_NAME	 "org.freedesktop.compiz"
#define COMPIZ_DBUS_ACTIVATE_MEMBER_NAME "activate"

static int displayPrivateIndex;

typedef struct _DbusDisplay {
    DBusConnection    *connection;
    CompWatchFdHandle watchFdHandle;
} DbusDisplay;

#define GET_DBUS_DISPLAY(d)				     \
    ((DbusDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define DBUS_DISPLAY(d)			   \
    DbusDisplay *dd = GET_DBUS_DISPLAY (d)

/*
 * Activate can be used to trigger any existing action. Arguments
 * should be a pair of { string, bool|int32|double|string }.
 *
 * Example (rotate to face 1):
 *
 * dbus-send --type=method_call --dest=org.freedesktop.compiz \
 * /org/freedesktop/compiz/rotate/allscreens/rotate_to	      \
 * org.freedesktop.compiz.activate			      \
 * string:'root' int32:0x52 string:'face' int32:1
 */

static Bool
dbusHandleActivateMessage (DBusConnection *connection,
			   DBusMessage    *message,
			   CompDisplay	  *d,
			   char	          **path)
{
    CompScreen *s = NULL;
    CompOption *option = NULL;
    int	       nOption = 0;

    if (strcmp (path[1], "allscreens"))
    {
	int screenNum;

	if (sscanf (path[1], "screen%d", &screenNum) != 1)
	    return FALSE;

	for (s = d->screens; s; s = s->next)
	    if (s->screenNum == screenNum)
		break;

	if (!s)
	    return FALSE;
    }

    if (strcmp (path[0], "core") == 0)
    {
	if (s)
	    option = compGetScreenOptions (s, &nOption);
	else
	    option = compGetDisplayOptions (d, &nOption);
    }
    else
    {
	CompPlugin *p;

	for (p = getPlugins (); p; p = p->next)
	    if (strcmp (p->vTable->name, path[0]) == 0)
		break;

	if (!p)
	    return FALSE;

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

    while (nOption--)
    {
	if (strcmp (option->name, path[2]) == 0)
	{
	    CompOption	    *argument = NULL;
	    int		    nArgument = 0;
	    DBusMessageIter iter;

	    if (option->type != CompOptionTypeAction)
		return FALSE;

	    if (!option->value.action.initiate)
		return FALSE;

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

	    (*option->value.action.initiate) (d,
					      &option->value.action,
					      0,
					      argument, nArgument);

	    if (argument)
		free (argument);

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
    Bool	status;
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

    if (!dbus_message_has_member (message, COMPIZ_DBUS_ACTIVATE_MEMBER_NAME))
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!dbus_message_get_path_decomposed (message, &path))
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!path[0] || !path[1] || !path[2] || !path[3] || !path[4] || !path[5])
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (strcmp (path[0], "org")	       ||
	strcmp (path[1], "freedesktop")||
	strcmp (path[2], "compiz"))
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    status = dbusHandleActivateMessage (connection, message, d, &path[3]);

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

	dbus_connection_close (dd->connection);
	dbus_error_free (&error);

	return FALSE;
    }

    dbus_error_free (&error);

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
	fprintf (stderr, "%s: dbus_bus_request_name reply is not "
		 "primary owner\n", programName);

	dbus_connection_close (dd->connection);

	return FALSE;
    }

    status = dbus_connection_add_filter (dd->connection,
					 dbusHandleMessage,
					 d, NULL);
    if (!status)
    {
	fprintf (stderr, "%s: dbus_connection_add_filter failed\n",
		 programName);

	dbus_connection_close (dd->connection);

	return FALSE;
    }

    status = dbus_connection_get_unix_fd (dd->connection, &fd);
    if (!status)
    {
	fprintf (stderr, "%s: dbus_connection_get_unix_fd failed\n",
		 programName);

	dbus_connection_close (dd->connection);

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
    dbus_connection_close (dd->connection);

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

CompPluginVTable dbusVTable = {
    "dbus",
    "Dbus",
    "Dbus Control Backend",
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
    NULL,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &dbusVTable;
}
