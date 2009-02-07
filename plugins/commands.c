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

static CompMetadata commandsMetadata;

static int displayPrivateIndex;

#define COMMANDS_DISPLAY_OPTION_COMMAND0              0
#define COMMANDS_DISPLAY_OPTION_COMMAND1              1
#define COMMANDS_DISPLAY_OPTION_COMMAND2              2
#define COMMANDS_DISPLAY_OPTION_COMMAND3              3
#define COMMANDS_DISPLAY_OPTION_COMMAND4              4
#define COMMANDS_DISPLAY_OPTION_COMMAND5              5
#define COMMANDS_DISPLAY_OPTION_COMMAND6              6
#define COMMANDS_DISPLAY_OPTION_COMMAND7              7
#define COMMANDS_DISPLAY_OPTION_COMMAND8              8
#define COMMANDS_DISPLAY_OPTION_COMMAND9              9
#define COMMANDS_DISPLAY_OPTION_COMMAND10            10
#define COMMANDS_DISPLAY_OPTION_COMMAND11            11
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_KEY     12
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND1_KEY     13
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND2_KEY     14
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND3_KEY     15
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND4_KEY     16
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND5_KEY     17
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND6_KEY     18
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND7_KEY     19
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND8_KEY     20
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND9_KEY     21
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND10_KEY    22
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND11_KEY    23
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_BUTTON  24
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND1_BUTTON  25
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND2_BUTTON  26
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND3_BUTTON  27
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND4_BUTTON  28
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND5_BUTTON  29
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND6_BUTTON  30
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND7_BUTTON  31
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND8_BUTTON  32
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND9_BUTTON  33
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND10_BUTTON 34
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND11_BUTTON 35
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_EDGE    36
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND1_EDGE    37
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND2_EDGE    38
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND3_EDGE    39
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND4_EDGE    40
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND5_EDGE    41
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND6_EDGE    42
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND7_EDGE    43
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND8_EDGE    44
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND9_EDGE    45
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND10_EDGE   46
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND11_EDGE   47
#define COMMANDS_DISPLAY_OPTION_NUM                  48

typedef struct _CommandsDisplay {
    CompOption opt[COMMANDS_DISPLAY_OPTION_NUM];
} CommandsDisplay;

#define GET_COMMANDS_DISPLAY(d)                                       \
    ((CommandsDisplay *) (d)->base.privates[displayPrivateIndex].ptr)
#define COMMANDS_DISPLAY(d)                                           \
    CommandsDisplay *cd = GET_COMMANDS_DISPLAY (d)

#define NUM_OPTIONS(d) (sizeof ((d)->opt) / sizeof (CompOption))

static Bool
runCommandDispatch (CompDisplay     *d,
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
	int index = COMMANDS_DISPLAY_OPTION_COMMAND0 + action->priv.val;

	COMMANDS_DISPLAY (d);

	runCommand (s, cd->opt[index].value.s);
    }

    return TRUE;
}

static const CompMetadataOptionInfo commandsDisplayOptionInfo[] = {
    { "command0", "string", 0, 0, 0 },
    { "command1", "string", 0, 0, 0 },
    { "command2", "string", 0, 0, 0 },
    { "command3", "string", 0, 0, 0 },
    { "command4", "string", 0, 0, 0 },
    { "command5", "string", 0, 0, 0 },
    { "command6", "string", 0, 0, 0 },
    { "command7", "string", 0, 0, 0 },
    { "command8", "string", 0, 0, 0 },
    { "command9", "string", 0, 0, 0 },
    { "command10", "string", 0, 0, 0 },
    { "command11", "string", 0, 0, 0 },
    { "run_command0_key", "key", 0, runCommandDispatch, 0 },
    { "run_command1_key", "key", 0, runCommandDispatch, 0 },
    { "run_command2_key", "key", 0, runCommandDispatch, 0 },
    { "run_command3_key", "key", 0, runCommandDispatch, 0 },
    { "run_command4_key", "key", 0, runCommandDispatch, 0 },
    { "run_command5_key", "key", 0, runCommandDispatch, 0 },
    { "run_command6_key", "key", 0, runCommandDispatch, 0 },
    { "run_command7_key", "key", 0, runCommandDispatch, 0 },
    { "run_command8_key", "key", 0, runCommandDispatch, 0 },
    { "run_command9_key", "key", 0, runCommandDispatch, 0 },
    { "run_command10_key", "key", 0, runCommandDispatch, 0 },
    { "run_command11_key", "key", 0, runCommandDispatch, 0 },
    { "run_command0_button", "button", 0, runCommandDispatch, 0 },
    { "run_command1_button", "button", 0, runCommandDispatch, 0 },
    { "run_command2_button", "button", 0, runCommandDispatch, 0 },
    { "run_command3_button", "button", 0, runCommandDispatch, 0 },
    { "run_command4_button", "button", 0, runCommandDispatch, 0 },
    { "run_command5_button", "button", 0, runCommandDispatch, 0 },
    { "run_command6_button", "button", 0, runCommandDispatch, 0 },
    { "run_command7_button", "button", 0, runCommandDispatch, 0 },
    { "run_command8_button", "button", 0, runCommandDispatch, 0 },
    { "run_command9_button", "button", 0, runCommandDispatch, 0 },
    { "run_command10_button", "button", 0, runCommandDispatch, 0 },
    { "run_command11_button", "button", 0, runCommandDispatch, 0 },
    { "run_command0_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command1_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command2_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command3_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command4_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command5_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command6_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command7_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command8_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command9_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command10_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command11_edge", "edge", 0, runCommandDispatch, 0 }
};

static CompBool
commandsInitDisplay (CompPlugin  *p,
		     CompDisplay *d)
{
    CommandsDisplay *cd;
    int             i;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    cd = malloc (sizeof (CommandsDisplay));
    if (!cd)
	return FALSE;

    if (!compInitDisplayOptionsFromMetadata (d,
					     &commandsMetadata,
					     commandsDisplayOptionInfo,
					     cd->opt,
					     COMMANDS_DISPLAY_OPTION_NUM))
    {
	free (cd);
	return FALSE;
    }

    for (i = 0; i < 12; i++)
    {
	int opt;
	
	opt = COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_KEY + i;
	cd->opt[opt].value.action.priv.val = i;
	opt = COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_BUTTON + i;
	cd->opt[opt].value.action.priv.val = i;
	opt = COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_EDGE + i;
	cd->opt[opt].value.action.priv.val = i;
    }

    d->base.privates[displayPrivateIndex].ptr = cd;

    return TRUE;
}

static void
commandsFiniDisplay (CompPlugin  *p,
		     CompDisplay *d)
{
    COMMANDS_DISPLAY (d);

    compFiniDisplayOptions (d, cd->opt, COMMANDS_DISPLAY_OPTION_NUM);

    free (cd);
}

static CompOption *
commandsGetDisplayOptions (CompPlugin  *p,
			   CompDisplay *d,
			   int         *count)
{
    COMMANDS_DISPLAY (d);

    *count = NUM_OPTIONS (cd);
    return cd->opt;
}

static CompBool
commandsSetDisplayOption (CompPlugin      *p,
			  CompDisplay     *d,
			  const char      *name,
			  CompOptionValue *value)
{
    CompOption *o;

    COMMANDS_DISPLAY (d);

    o = compFindOption (cd->opt, NUM_OPTIONS (cd), name, NULL);
    if (!o)
	return FALSE;

    return compSetDisplayOption (d, o, value);
}

static CompBool
commandsInitObject (CompPlugin *p,
		    CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) 0, /* InitCore */
	(InitPluginObjectProc) commandsInitDisplay
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
commandsFiniObject (CompPlugin *p,
		    CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) 0, /* FiniCore */
	(FiniPluginObjectProc) commandsFiniDisplay
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static CompOption *
commandsGetObjectOptions (CompPlugin *p,
			  CompObject *o,
			  int        *count)
{
    static GetPluginObjectOptionsProc dispTab[] = {
	(GetPluginObjectOptionsProc) 0, /* GetCoreOptions */
	(GetPluginObjectOptionsProc) commandsGetDisplayOptions
    };

    *count = 0;
    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab),
		     (void *) count, (p, o, count));
}

static CompBool
commandsSetObjectOption (CompPlugin      *p,
			 CompObject      *o,
			 const char      *name,
			 CompOptionValue *value)
{
    static SetPluginObjectOptionProc dispTab[] = {
	(SetPluginObjectOptionProc) 0, /* SetCoreOption */
	(SetPluginObjectOptionProc) commandsSetDisplayOption,
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), FALSE,
		     (p, o, name, value));
}

static Bool
commandsInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&commandsMetadata,
					 p->vTable->name,
					 commandsDisplayOptionInfo,
					 COMMANDS_DISPLAY_OPTION_NUM, 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&commandsMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&commandsMetadata, p->vTable->name);

    return TRUE;
}

static void
commandsFini (CompPlugin *p)
{
    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&commandsMetadata);
}

static CompMetadata *
commandsGetMetadata (CompPlugin *p)
{
    return &commandsMetadata;
}

static CompPluginVTable commandsVTable = {
    "commands",
    commandsGetMetadata,
    commandsInit,
    commandsFini,
    commandsInitObject,
    commandsFiniObject,
    commandsGetObjectOptions,
    commandsSetObjectOption
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &commandsVTable;
}
