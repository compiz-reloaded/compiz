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

#ifndef _COMPIZ_H
#define _COMPIZ_H

#include <libxml/parser.h>

#include <compiz-common.h>

COMPIZ_BEGIN_DECLS

typedef int CompBool;
typedef int CompTimeoutHandle;
typedef int CompWatchFdHandle;

typedef union _CompOptionValue CompOptionValue;

typedef struct _CompObject   CompObject;
typedef struct _CompCore     CompCore;
typedef struct _CompDisplay  CompDisplay;
typedef struct _CompMetadata CompMetadata;
typedef struct _CompOption   CompOption;
typedef struct _CompPlugin   CompPlugin;
typedef struct _CompScreen   CompScreen;
typedef struct _CompWindow   CompWindow;

typedef CompBool (*CallBackProc) (void *closure);

typedef enum {
    CompOptionTypeBool,
    CompOptionTypeInt,
    CompOptionTypeFloat,
    CompOptionTypeString,
    CompOptionTypeColor,
    CompOptionTypeAction,
    CompOptionTypeKey,
    CompOptionTypeButton,
    CompOptionTypeEdge,
    CompOptionTypeBell,
    CompOptionTypeMatch,
    CompOptionTypeList
} CompOptionType;

void
compInitOptionValue (CompOptionValue *v);

void
compFiniOptionValue (CompOptionValue *v,
		     CompOptionType  type);

void
compInitOption (CompOption *option);

void
compFiniOption (CompOption *option);

CompOption *
compFindOption (CompOption *option,
		int	    nOption,
		const char  *name,
		int	    *index);

CompBool
compSetBoolOption (CompOption      *option,
		   CompOptionValue *value);

CompBool
compSetIntOption (CompOption	  *option,
		  CompOptionValue *value);

CompBool
compSetFloatOption (CompOption	    *option,
		    CompOptionValue *value);

CompBool
compSetStringOption (CompOption	     *option,
		     CompOptionValue *value);

CompBool
compSetColorOption (CompOption	    *option,
		    CompOptionValue *value);

CompBool
compSetActionOption (CompOption      *option,
		     CompOptionValue *value);

CompBool
compSetMatchOption (CompOption      *option,
		    CompOptionValue *value);

CompBool
compSetOptionList (CompOption      *option,
		   CompOptionValue *value);

CompBool
compSetOption (CompOption      *option,
	       CompOptionValue *value);

CompTimeoutHandle
compAddTimeout (int	     minTime,
		int	     maxTime,
		CallBackProc callBack,
		void	     *closure);

void *
compRemoveTimeout (CompTimeoutHandle handle);

CompWatchFdHandle
compAddWatchFd (int	     fd,
		short int    events,
		CallBackProc callBack,
		void	     *closure);

void
compRemoveWatchFd (CompWatchFdHandle handle);

short int
compWatchFdEvents (CompWatchFdHandle handle);

CompBool
compInitMetadata (CompMetadata *metadata);

CompBool
compInitPluginMetadata (CompMetadata *metadata,
			const char   *plugin);

void
compFiniMetadata (CompMetadata *metadata);

CompBool
compAddMetadataFromFile (CompMetadata *metadata,
			 const char   *file);

CompBool
compAddMetadataFromString (CompMetadata *metadata,
			   const char	*string);

CompBool
compAddMetadataFromIO (CompMetadata	     *metadata,
		       xmlInputReadCallback  ioread,
		       xmlInputCloseCallback ioclose,
		       void		     *ioctx);

char *
compGetStringFromMetadataPath (CompMetadata *metadata,
			       const char   *path);

int
compReadXmlChunk (const char *src,
		  int	     *offset,
		  char	     *buffer,
		  int	     length);


COMPIZ_END_DECLS

#endif
