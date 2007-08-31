/*
 * Copyright © 2007 Novell, Inc.
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

#include <compiz-core.h>

static CompCore core;

static char *corePrivateIndices = 0;
static int  corePrivateLen = 0;

static int
reallocCorePrivate (int  size,
		    void *closure)
{
    void *privates;

    privates = realloc (core.object.privates, size * sizeof (CompPrivate));
    if (!privates)
	return FALSE;

    core.object.privates = (CompPrivate *) privates;

    return TRUE;
}

int
allocCoreObjectPrivateIndex (CompObject *parent)
{
    return allocatePrivateIndex (&corePrivateLen,
				 &corePrivateIndices,
				 reallocCorePrivate,
				 0);
}

void
freeCoreObjectPrivateIndex (CompObject *parent,
			    int	       index)
{
    freePrivateIndex (corePrivateLen, corePrivateIndices, index);
}

void
forEachCoreObject (CompObject         *parent,
		   ObjectCallbackProc proc,
		   void		      *closure)
{
}

CompBool
initCore (void)
{
    compObjectInit (&core.object, 0, COMP_OBJECT_TYPE_CORE);

    return TRUE;
}

void
finiCore (void)
{
}
