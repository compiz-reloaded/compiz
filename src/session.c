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
#include <X11/SM/SMlib.h>
#include <compiz.h>

static SmcConn smcConnection;
static Bool    connected = FALSE;

void
initSession (char *smPrevClientId)
{
    static SmcCallbacks callbacks;
    static char		*smClientId;

    if (getenv ("SESSION_MANAGER"))
    {
	char errorBuffer[1024];

	smcConnection = SmcOpenConnection (NULL,
					   NULL,
					   SmProtoMajor,
					   SmProtoMinor,
					   0,
					   &callbacks,
					   smPrevClientId,
					   &smClientId,
					   sizeof (errorBuffer),
					   errorBuffer);
	if (!smcConnection)
	    fprintf (stderr, "SmcOpenConnection failed: %s\n", errorBuffer);
	else
	    connected = TRUE;
    }
}

void
closeSession (void)
{
    if (connected)
    {
	if (SmcCloseConnection (smcConnection, 0, NULL) != SmcConnectionInUse)
	    connected = FALSE;
    }
}
