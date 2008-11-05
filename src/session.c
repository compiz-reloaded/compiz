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
 * Author: Radek Doulik <rodo@novell.com>
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <X11/SM/SMlib.h>
#include <X11/ICE/ICElib.h>

#include <compiz-core.h>

#define SM_DEBUG(x)

static SmcConn		 smcConnection;
static CompWatchFdHandle iceWatchFdHandle;
static Bool		 connected = 0;
static Bool		 iceConnected = 0;
static char		 *smClientId, *smPrevClientId;

static void iceInit (void);

static void
setStringListProperty (SmcConn	  connection,
		       const char *name,
		       const char **values,
		       int	  nValues)
{
    SmProp prop, *pProp;
    int	   i;

    prop.name = (char *) name;
    prop.type = SmLISTofARRAY8;

    prop.vals = malloc (nValues * sizeof (SmPropValue));
    if (!prop.vals)
	return;

    for (i = 0; i < nValues; i++)
    {
	prop.vals[i].value = (char *) values[i];
	prop.vals[i].length = strlen (values[i]);
    }

    prop.num_vals = nValues;

    pProp = &prop;

    SmcSetProperties (connection, 1, &pProp);

    free (prop.vals);
}

static void
setCloneRestartCommands (SmcConn connection)
{
    const char **args;
    int        i, count = 0;

    /* at maximum, we pass our old arguments + our new client id
       to the SM, so allocate for that case */
    args = malloc ((programArgc + 2) * sizeof (char *));
    if (!args)
	return;

    for (i = 0; i < programArgc; i++)
    {
	if (strcmp (programArgv[i], "--sm-client-id") == 0)
	    i++; /* skip old client id, we'll add the new one later */
	else if (strcmp (programArgv[i], "--replace") == 0)
	    continue; /* there's nothing to replace when starting session */
	else
	    args[count++] = programArgv[i];
    }

    setStringListProperty (connection, SmCloneCommand, args, count);

    /* insert new client id at position 1 and 2;
       position 0 is the executable name */
    for (i = count - 1; i >= 1; i--)
	args[i + 2] = args[i];
    args[1] = "--sm-client-id";
    args[2] = smClientId;
    count += 2;

    setStringListProperty (connection, SmRestartCommand, args, count);

    free (args);
}

static void
setRestartStyle (SmcConn connection,
		 char    hint)
{
    SmProp	prop, *pProp;
    SmPropValue propVal;

    prop.name = SmRestartStyleHint;
    prop.type = SmCARD8;
    prop.num_vals = 1;
    prop.vals = &propVal;
    propVal.value = &hint;
    propVal.length = 1;

    pProp = &prop;

    SmcSetProperties (connection, 1, &pProp);
}

static void
setProgramInfo (SmcConn    connection,
		pid_t      pid,
		uid_t      uid)
{
    SmProp	  progProp, pidProp, userProp;
    SmPropValue   progVal, pidVal, userVal;
    SmProp	  *props[3];
    char	  pidBuffer[32];
    unsigned int  count = 0;
    struct passwd *pw;

    progProp.name     = SmProgram;
    progProp.type     = SmARRAY8;
    progProp.num_vals = 1;
    progProp.vals     = &progVal;
    progVal.value     = (SmPointer) "compiz";
    progVal.length    = strlen (progVal.value);

    props[count++] = &progProp;

    snprintf (pidBuffer, sizeof (pidBuffer), "%d", pid);

    pidProp.name     = SmProcessID;
    pidProp.type     = SmARRAY8;
    pidProp.num_vals = 1;
    pidProp.vals     = &pidVal;
    pidVal.value     = (SmPointer) pidBuffer;
    pidVal.length    = strlen (pidBuffer);

    props[count++] = &pidProp;

    pw = getpwuid (uid);
    if (pw)
    {
	userProp.name     = SmUserID;
	userProp.type     = SmARRAY8;
	userProp.num_vals = 1;
	userProp.vals     = &userVal;
	userVal.value     = (SmPointer) pw->pw_name;
	userVal.length    = strlen (pw->pw_name);

	props[count++] = &userProp;
    }

    SmcSetProperties (connection, count, props);
}

static void
saveYourselfCallback (SmcConn	connection,
		      SmPointer client_data,
		      int	saveType,
		      Bool	shutdown,
		      int	interact_Style,
		      Bool	fast)
{
    CompOption args[4];

    args[0].type    = CompOptionTypeInt;
    args[0].name    = "save_type";
    args[0].value.i = saveType;

    args[1].type    = CompOptionTypeBool;
    args[1].name    = "shutdown";
    args[1].value.b = shutdown;

    args[2].type    = CompOptionTypeInt;
    args[2].name    = "interact_style";
    args[2].value.i = interact_Style;

    args[3].type    = CompOptionTypeBool;
    args[3].name    = "fast";
    args[3].value.b = fast;

    (*core.sessionEvent) (&core, CompSessionEventSaveYourself, args, 4);

    setCloneRestartCommands (connection);
    setRestartStyle (connection, SmRestartImmediately);
    setProgramInfo (connection, getpid (), getuid ());
    SmcSaveYourselfDone (connection, 1);
}

static void
dieCallback (SmcConn   connection,
	     SmPointer clientData)
{
    (*core.sessionEvent) (&core, CompSessionEventDie, NULL, 0);

    closeSession ();
    exit (0);
}

static void
saveCompleteCallback (SmcConn	connection,
		      SmPointer clientData)
{
    (*core.sessionEvent) (&core, CompSessionEventSaveComplete, NULL, 0);
}

static void
shutdownCancelledCallback (SmcConn   connection,
			   SmPointer clientData)
{
    (*core.sessionEvent) (&core, CompSessionEventShutdownCancelled, NULL, 0);
}

void
initSession (char *prevClientId)
{
    static SmcCallbacks callbacks;

    if (getenv ("SESSION_MANAGER"))
    {
	char errorBuffer[1024];

	iceInit ();

	callbacks.save_yourself.callback    = saveYourselfCallback;
	callbacks.save_yourself.client_data = NULL;

	callbacks.die.callback	  = dieCallback;
	callbacks.die.client_data = NULL;

	callbacks.save_complete.callback    = saveCompleteCallback;
	callbacks.save_complete.client_data = NULL;

	callbacks.shutdown_cancelled.callback	 = shutdownCancelledCallback;
	callbacks.shutdown_cancelled.client_data = NULL;

	smcConnection = SmcOpenConnection (NULL,
					   NULL,
					   SmProtoMajor,
					   SmProtoMinor,
					   SmcSaveYourselfProcMask |
					   SmcDieProcMask	   |
					   SmcSaveCompleteProcMask |
					   SmcShutdownCancelledProcMask,
					   &callbacks,
					   prevClientId,
					   &smClientId,
					   sizeof (errorBuffer),
					   errorBuffer);
	if (!smcConnection)
	    compLogMessage ("core", CompLogLevelWarn,
			    "SmcOpenConnection failed: %s",
			    errorBuffer);
	else
	{
	    connected = TRUE;
	    if (prevClientId)
		smPrevClientId = strdup (prevClientId);
	}
    }
}

void
closeSession (void)
{
    if (connected)
    {
	setRestartStyle (smcConnection, SmRestartIfRunning);

	if (SmcCloseConnection (smcConnection, 0, NULL) != SmcConnectionInUse)
	    connected = FALSE;
	if (smClientId)
	{
	    free (smClientId);
	    smClientId = NULL;
	}
	if (smPrevClientId)
	{
	    free (smPrevClientId);
	    smPrevClientId = NULL;
	}
    }
}

void
sessionEvent (CompCore         *c,
	      CompSessionEvent event,
	      CompOption       *arguments,
	      unsigned int     nArguments)
{
}

char *
getSessionClientId (CompSessionClientIdType type)
{
    if (!connected)
	return NULL;

    switch (type) {
    case CompSessionClientId:
	if (smClientId)
	    return strdup (smClientId);
	break;

    case CompSessionPrevClientId:
	if (smPrevClientId)
	    return strdup (smPrevClientId);
	break;
    }

    return NULL;
}
/* ice connection handling taken and updated from gnome-ice.c
 * original gnome-ice.c code written by Tom Tromey <tromey@cygnus.com>
 */

/* This is called when data is available on an ICE connection. */
static Bool
iceProcessMessages (void *data)
{
    IceConn		     connection = (IceConn) data;
    IceProcessMessagesStatus status;

    SM_DEBUG (printf ("ICE connection process messages\n"));

    status = IceProcessMessages (connection, NULL, NULL);

    if (status == IceProcessMessagesIOError)
    {
	SM_DEBUG (printf ("ICE connection process messages"
			  " - error => shutting down the connection\n"));

	IceSetShutdownNegotiation (connection, False);
	IceCloseConnection (connection);
    }

    return 1;
}

/* This is called when a new ICE connection is made.  It arranges for
   the ICE connection to be handled via the event loop.  */
static void
iceNewConnection (IceConn    connection,
		  IcePointer clientData,
		  Bool	     opening,
		  IcePointer *watchData)
{
    if (opening)
    {
	SM_DEBUG (printf ("ICE connection opening\n"));

	/* Make sure we don't pass on these file descriptors to any
	   exec'ed children */
	fcntl (IceConnectionNumber (connection), F_SETFD,
	       fcntl (IceConnectionNumber (connection),
		      F_GETFD,0) | FD_CLOEXEC);

	iceWatchFdHandle = compAddWatchFd (IceConnectionNumber (connection),
					   POLLIN | POLLPRI | POLLHUP | POLLERR,
					   iceProcessMessages, connection);

	iceConnected = 1;
    }
    else
    {
	SM_DEBUG (printf ("ICE connection closing\n"));

	if (iceConnected)
	{
	    compRemoveWatchFd (iceWatchFdHandle);

	    iceWatchFdHandle = 0;
	    iceConnected = 0;
	}
    }
}

static IceIOErrorHandler oldIceHandler;

static void
iceErrorHandler (IceConn connection)
{
    if (oldIceHandler)
	(*oldIceHandler) (connection);
}

/* We call any handler installed before (or after) iceInit but
   avoid calling the default libICE handler which does an exit() */
static void
iceInit (void)
{
    static Bool iceInitialized = 0;

    if (!iceInitialized)
    {
	IceIOErrorHandler defaultIceHandler;

	oldIceHandler	  = IceSetIOErrorHandler (NULL);
	defaultIceHandler = IceSetIOErrorHandler (iceErrorHandler);

	if (oldIceHandler == defaultIceHandler)
	    oldIceHandler = NULL;

	IceAddConnectionWatch (iceNewConnection, NULL);

	iceInitialized = 1;
    }
}
