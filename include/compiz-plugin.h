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

#ifndef _COMPIZ_PLUGIN_H
#define _COMPIZ_PLUGIN_H

#include <compiz.h>

COMPIZ_BEGIN_DECLS

typedef CompBool (*InitPluginProc) (CompPlugin *plugin);
typedef void (*FiniPluginProc) (CompPlugin *plugin);

typedef CompMetadata *(*GetMetadataProc) (CompPlugin *plugin);

typedef CompBool (*InitPluginForDisplayProc) (CompPlugin  *plugin,
					      CompDisplay *display);
typedef void (*FiniPluginForDisplayProc) (CompPlugin  *plugin,
					  CompDisplay *display);

typedef CompBool (*InitPluginForScreenProc) (CompPlugin *plugin,
					     CompScreen *screen);
typedef void (*FiniPluginForScreenProc) (CompPlugin *plugin,
					 CompScreen *screen);

typedef CompBool (*InitPluginForWindowProc) (CompPlugin *plugin,
					     CompWindow *window);
typedef void (*FiniPluginForWindowProc) (CompPlugin *plugin,
					 CompWindow *window);

typedef CompOption *(*GetPluginDisplayOptionsProc) (CompPlugin  *plugin,
						    CompDisplay *display,
						    int	        *count);
typedef CompBool (*SetPluginDisplayOptionProc) (CompPlugin      *plugin,
						CompDisplay     *display,
						const char	*name,
						CompOptionValue *value);

typedef CompOption *(*GetPluginScreenOptionsProc) (CompPlugin *plugin,
						   CompScreen *screen,
						   int	      *count);
typedef CompBool (*SetPluginScreenOptionProc) (CompPlugin      *plugin,
					       CompScreen      *screen,
					       const char      *name,
					       CompOptionValue *value);

typedef struct _CompPluginVTable {
    const char *name;

    GetMetadataProc getMetadata;

    InitPluginProc init;
    FiniPluginProc fini;

    InitPluginForDisplayProc initDisplay;
    FiniPluginForDisplayProc finiDisplay;

    InitPluginForScreenProc initScreen;
    FiniPluginForScreenProc finiScreen;

    InitPluginForWindowProc initWindow;
    FiniPluginForWindowProc finiWindow;

    GetPluginDisplayOptionsProc getDisplayOptions;
    SetPluginDisplayOptionProc  setDisplayOption;
    GetPluginScreenOptionsProc  getScreenOptions;
    SetPluginScreenOptionProc   setScreenOption;
} CompPluginVTable;

CompPluginVTable *
getCompPluginInfo (void);

COMPIZ_END_DECLS

#endif
