/*
 * Copyright © 2009 Danny Baumann
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Danny Baumann not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Danny Baumann makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DANNY BAUMANN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DENNIS KASPRZYK BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Danny Baumann <dannybaumann@web.de>
 */

#include <compiz-core.h>

static CompMetadata gnomeMetadata;

static int displayPrivateIndex;

#define GNOME_DISPLAY_OPTION_MAIN_MENU_KEY              0
#define GNOME_DISPLAY_OPTION_RUN_DIALOG_KEY             1
#define GNOME_DISPLAY_OPTION_SCREENSHOT_CMD             2
#define GNOME_DISPLAY_OPTION_RUN_SCREENSHOT_KEY         3
#define GNOME_DISPLAY_OPTION_WINDOW_SCREENSHOT_CMD      4
#define GNOME_DISPLAY_OPTION_RUN_WINDOW_SCREENSHOT_KEY  5
#define GNOME_DISPLAY_OPTION_TERMINAL_CMD               6
#define GNOME_DISPLAY_OPTION_RUN_TERMINAL_KEY           7
#define GNOME_DISPLAY_OPTION_NUM                        8

typedef struct _GnomeDisplay {
    CompOption opt[GNOME_DISPLAY_OPTION_NUM];

    Atom panelActionAtom;
    Atom panelMainMenuAtom;
    Atom panelRunDialogAtom;
} GnomeDisplay;

#define GET_GNOME_DISPLAY(d)                                       \
    ((GnomeDisplay *) (d)->base.privates[displayPrivateIndex].ptr)
#define GNOME_DISPLAY(d)                                           \
    GnomeDisplay *gd = GET_GNOME_DISPLAY (d)

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static Bool
runDispatch (CompDisplay     *d,
	     CompAction      *action,
	     CompActionState state,
	     CompOption      *option,
	     int             nOption)
{
    CompScreen *s;
    Window     xid;

    xid = getIntOptionNamed (option, nOption, "root", 0);
    s   = findScreenAtDisplay (d, xid);

    if (s)
    {
	GNOME_DISPLAY (d);

	runCommand (s, gd->opt[action->priv.val].value.s);
    }

    return TRUE;
}

static void
panelAction (CompDisplay *d,
	     CompOption  *option,
	     int         nOption,
	     Atom        actionAtom)
{
    Window     xid;
    CompScreen *s;
    XEvent     event;
    Time       time;

    GNOME_DISPLAY (d);

    xid = getIntOptionNamed (option, nOption, "root", 0);
    s   = findScreenAtDisplay (d, xid);

    if (!s)
	return;

    time = getIntOptionNamed (option, nOption, "time", CurrentTime);

    /* we need to ungrab the keyboard here, otherwise the panel main
       menu won't popup as it wants to grab the keyboard itself */
    XUngrabKeyboard (d->display, time);

    event.type                 = ClientMessage;
    event.xclient.window       = s->root;
    event.xclient.message_type = gd->panelActionAtom;
    event.xclient.format       = 32;
    event.xclient.data.l[0]    = actionAtom;
    event.xclient.data.l[1]    = time;
    event.xclient.data.l[2]    = 0;
    event.xclient.data.l[3]    = 0;
    event.xclient.data.l[4]    = 0;

    XSendEvent (d->display, s->root, FALSE, StructureNotifyMask, &event);
}

static Bool
showMainMenu (CompDisplay     *d,
	      CompAction      *action,
	      CompActionState state,
	      CompOption      *option,
	      int             nOption)
{
    GNOME_DISPLAY (d);

    panelAction (d, option, nOption, gd->panelMainMenuAtom);

    return TRUE;
}

static Bool
showRunDialog (CompDisplay     *d,
	       CompAction      *action,
	       CompActionState state,
	       CompOption      *option,
	       int             nOption)
{
    GNOME_DISPLAY (d);

    panelAction (d, option, nOption, gd->panelRunDialogAtom);

    return TRUE;
}
static const CompMetadataOptionInfo gnomeDisplayOptionInfo[] = {
    { "main_menu_key", "key", 0, showMainMenu, 0 },
    { "run_key", "key", 0, showRunDialog, 0 },
    { "command_screenshot", "string", 0, 0, 0 },
    { "run_command_screenshot_key", "key", 0, runDispatch, 0 },
    { "command_window_screenshot", "string", 0, 0, 0 },
    { "run_command_window_screenshot_key", "key", 0, runDispatch, 0 },
    { "command_terminal", "string", 0, 0, 0 },
    { "run_command_terminal_key", "key", 0, runDispatch, 0 }
};

static CompBool
gnomeInitDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    GnomeDisplay *gd;
    int          opt, index;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    gd = malloc (sizeof (GnomeDisplay));
    if (!gd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &gnomeMetadata,
					     gnomeDisplayOptionInfo,
					     gd->opt,
					     GNOME_DISPLAY_OPTION_NUM))
    {
	free (gd);
	return FALSE;
    }

    opt = GNOME_DISPLAY_OPTION_RUN_SCREENSHOT_KEY;
    gd->opt[opt].value.action.priv.val = GNOME_DISPLAY_OPTION_SCREENSHOT_CMD;

    opt   = GNOME_DISPLAY_OPTION_RUN_WINDOW_SCREENSHOT_KEY;
    index = GNOME_DISPLAY_OPTION_WINDOW_SCREENSHOT_CMD;
    gd->opt[opt].value.action.priv.val = index;

    opt = GNOME_DISPLAY_OPTION_RUN_TERMINAL_KEY;
    gd->opt[opt].value.action.priv.val = GNOME_DISPLAY_OPTION_TERMINAL_CMD;

    gd->panelActionAtom =
	XInternAtom (d->display, "_GNOME_PANEL_ACTION", FALSE);
    gd->panelMainMenuAtom =
	XInternAtom (d->display, "_GNOME_PANEL_ACTION_MAIN_MENU", FALSE);
    gd->panelRunDialogAtom =
	XInternAtom (d->display, "_GNOME_PANEL_ACTION_RUN_DIALOG", FALSE);

    d->base.privates[displayPrivateIndex].ptr = gd;

    return TRUE;
}

static void
gnomeFiniDisplay (CompPlugin  *p,
		  CompDisplay *d)
{
    GNOME_DISPLAY (d);

    compFiniDisplayOptions (d, gd->opt, GNOME_DISPLAY_OPTION_NUM);

    free (gd);
}

static CompOption *
gnomeGetDisplayOptions (CompPlugin  *p,
			CompDisplay *d,
			int         *count)
{
    GNOME_DISPLAY (d);

    *count = NUM_OPTIONS (gd);
    return gd->opt;
}

static CompBool
gnomeSetDisplayOption (CompPlugin      *p,
		       CompDisplay     *d,
		       const char      *name,
		       CompOptionValue *value)
{
    CompOption *o;

    GNOME_DISPLAY (d);

    o = compFindOption (gd->opt, NUM_OPTIONS (gd), name, NULL);
    if (!o)
	return FALSE;

    return compSetDisplayOption (d, o, value);
}

static CompBool
gnomeInitObject (CompPlugin *p,
		 CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) gnomeInitDisplay
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
gnomeFiniObject (CompPlugin *p,
		 CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) gnomeFiniDisplay
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
gnomeGetObjectOptions (CompPlugin *p,
		       CompObject *o,
		       int        *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) gnomeGetDisplayOptions
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab),
		     (void *) (*count = 0), (p, o, count));
}

static CompBool
gnomeSetObjectOption (CompPlugin      *p,
		      CompObject      *o,
		      const char      *name,
		      CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) gnomeSetDisplayOption,
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (p, o, name, value));
}

static Bool
gnomeInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&gnomeMetadata,
					 p->vTable->name,
					 gnomeDisplayOptionInfo,
					 GNOME_DISPLAY_OPTION_NUM, 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&gnomeMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&gnomeMetadata, p->vTable->name);

    return TRUE;
}

static void
gnomeFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&gnomeMetadata);
}

static CompMetadata *
gnomeGetMetadata (CompPlugin *p)
{
    return &gnomeMetadata;
}

static CompPluginVTable gnomeVTable = {
    "gnomecompat",
    gnomeGetMetadata,
    gnomeInit,
    gnomeFini,
    gnomeInitObject,
    gnomeFiniObject,
    gnomeGetObjectOptions,
    gnomeSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &gnomeVTable;
}
