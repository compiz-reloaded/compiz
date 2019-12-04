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

#define COMMANDS_NUM                                 25

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
#define COMMANDS_DISPLAY_OPTION_COMMAND12            12
#define COMMANDS_DISPLAY_OPTION_COMMAND13            13
#define COMMANDS_DISPLAY_OPTION_COMMAND14            14
#define COMMANDS_DISPLAY_OPTION_COMMAND15            15
#define COMMANDS_DISPLAY_OPTION_COMMAND16            16
#define COMMANDS_DISPLAY_OPTION_COMMAND17            17
#define COMMANDS_DISPLAY_OPTION_COMMAND18            18
#define COMMANDS_DISPLAY_OPTION_COMMAND19            19
#define COMMANDS_DISPLAY_OPTION_COMMAND20            20
#define COMMANDS_DISPLAY_OPTION_COMMAND21            21
#define COMMANDS_DISPLAY_OPTION_COMMAND22            22
#define COMMANDS_DISPLAY_OPTION_COMMAND23            23
#define COMMANDS_DISPLAY_OPTION_COMMAND24            24
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_KEY     25
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND1_KEY     26
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND2_KEY     27
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND3_KEY     28
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND4_KEY     29
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND5_KEY     30
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND6_KEY     31
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND7_KEY     32
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND8_KEY     33
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND9_KEY     34
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND10_KEY    35
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND11_KEY    36
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND12_KEY    37
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND13_KEY    38
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND14_KEY    39
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND15_KEY    40
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND16_KEY    41
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND17_KEY    42
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND18_KEY    43
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND19_KEY    44
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND20_KEY    45
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND21_KEY    46
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND22_KEY    47
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND23_KEY    48
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND24_KEY    49
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_BUTTON  50
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND1_BUTTON  51
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND2_BUTTON  52
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND3_BUTTON  53
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND4_BUTTON  54
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND5_BUTTON  55
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND6_BUTTON  56
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND7_BUTTON  57
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND8_BUTTON  58
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND9_BUTTON  59
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND10_BUTTON 60
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND11_BUTTON 61
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND12_BUTTON 62
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND13_BUTTON 63
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND14_BUTTON 64
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND15_BUTTON 65
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND16_BUTTON 66
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND17_BUTTON 67
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND18_BUTTON 68
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND19_BUTTON 69
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND20_BUTTON 70
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND21_BUTTON 71
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND22_BUTTON 72
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND23_BUTTON 73
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND24_BUTTON 74
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_EDGE    75
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND1_EDGE    76
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND2_EDGE    77
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND3_EDGE    78
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND4_EDGE    79
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND5_EDGE    80
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND6_EDGE    81
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND7_EDGE    82
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND8_EDGE    83
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND9_EDGE    84
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND10_EDGE   85
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND11_EDGE   86
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND12_EDGE   87
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND13_EDGE   88
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND14_EDGE   89
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND15_EDGE   90
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND16_EDGE   91
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND17_EDGE   92
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND18_EDGE   93
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND19_EDGE   94
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND20_EDGE   95
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND21_EDGE   96
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND22_EDGE   97
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND23_EDGE   98
#define COMMANDS_DISPLAY_OPTION_RUN_COMMAND24_EDGE   99
#define COMMANDS_DISPLAY_OPTION_IGNORE_GRABS        100
#define COMMANDS_DISPLAY_OPTION_NUM                 101

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
    { "command12", "string", 0, 0, 0 },
    { "command13", "string", 0, 0, 0 },
    { "command14", "string", 0, 0, 0 },
    { "command15", "string", 0, 0, 0 },
    { "command16", "string", 0, 0, 0 },
    { "command17", "string", 0, 0, 0 },
    { "command18", "string", 0, 0, 0 },
    { "command19", "string", 0, 0, 0 },
    { "command20", "string", 0, 0, 0 },
    { "command21", "string", 0, 0, 0 },
    { "command22", "string", 0, 0, 0 },
    { "command23", "string", 0, 0, 0 },
    { "command24", "string", 0, 0, 0 },
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
    { "run_command12_key", "key", 0, runCommandDispatch, 0 },
    { "run_command13_key", "key", 0, runCommandDispatch, 0 },
    { "run_command14_key", "key", 0, runCommandDispatch, 0 },
    { "run_command15_key", "key", 0, runCommandDispatch, 0 },
    { "run_command16_key", "key", 0, runCommandDispatch, 0 },
    { "run_command17_key", "key", 0, runCommandDispatch, 0 },
    { "run_command18_key", "key", 0, runCommandDispatch, 0 },
    { "run_command19_key", "key", 0, runCommandDispatch, 0 },
    { "run_command20_key", "key", 0, runCommandDispatch, 0 },
    { "run_command21_key", "key", 0, runCommandDispatch, 0 },
    { "run_command22_key", "key", 0, runCommandDispatch, 0 },
    { "run_command23_key", "key", 0, runCommandDispatch, 0 },
    { "run_command24_key", "key", 0, runCommandDispatch, 0 },
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
    { "run_command12_button", "button", 0, runCommandDispatch, 0 },
    { "run_command13_button", "button", 0, runCommandDispatch, 0 },
    { "run_command14_button", "button", 0, runCommandDispatch, 0 },
    { "run_command15_button", "button", 0, runCommandDispatch, 0 },
    { "run_command16_button", "button", 0, runCommandDispatch, 0 },
    { "run_command17_button", "button", 0, runCommandDispatch, 0 },
    { "run_command18_button", "button", 0, runCommandDispatch, 0 },
    { "run_command19_button", "button", 0, runCommandDispatch, 0 },
    { "run_command20_button", "button", 0, runCommandDispatch, 0 },
    { "run_command21_button", "button", 0, runCommandDispatch, 0 },
    { "run_command22_button", "button", 0, runCommandDispatch, 0 },
    { "run_command23_button", "button", 0, runCommandDispatch, 0 },
    { "run_command24_button", "button", 0, runCommandDispatch, 0 },
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
    { "run_command11_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command12_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command13_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command14_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command15_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command16_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command17_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command18_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command19_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command20_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command21_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command22_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command23_edge", "edge", 0, runCommandDispatch, 0 },
    { "run_command24_edge", "edge", 0, runCommandDispatch, 0 },
    { "ignore_grabs", "bool", 0, 0, 0 }
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

    for (i = 0; i < COMMANDS_NUM; i++)
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
    int	       index, i;

    COMMANDS_DISPLAY (d);

    o = compFindOption (cd->opt, NUM_OPTIONS (cd), name, &index);
    if (!o)
	return FALSE;

    if (index == COMMANDS_DISPLAY_OPTION_IGNORE_GRABS)
    {
	for (i = 0; i < COMMANDS_NUM; i++)
	{
	    int opt;
	    
	    opt = COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_KEY + i;
	    cd->opt[opt].value.action.ignoreGrabs = value->b;
	    compSetDisplayOption (d, &cd->opt[opt], &cd->opt[opt].value);
	    opt = COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_BUTTON + i;
	    cd->opt[opt].value.action.ignoreGrabs = value->b;
	    compSetDisplayOption (d, &cd->opt[opt], &cd->opt[opt].value);
	    opt = COMMANDS_DISPLAY_OPTION_RUN_COMMAND0_EDGE + i;
	    cd->opt[opt].value.action.ignoreGrabs = value->b;
	    compSetDisplayOption (d, &cd->opt[opt], &cd->opt[opt].value);
	}
    }

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
