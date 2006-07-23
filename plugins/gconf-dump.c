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
 */

/* This is a hack to generate GConf schema files for compiz and its
 * plugins. To regenerate the base compiz.schemas.in file, just do
 *
 *   rm compiz.schemas.in; make compiz.schemas.in
 *
 * To generate a schema file for third-party plugins, do something
 * like:
 *
 *   COMPIZ_SCHEMA_PLUGINS="plugin1 plugin2" \
 *   COMPIZ_SCHEMA_FILE="output_filename" \
 *   compiz --replace dep1 plugin1 dep2 dep3 plugin2 gconf-dump
 *
 * COMPIZ_SCHEMA_PLUGINS indicates the plugins to generate schema
 * info for, which might be a subset of the plugins listed on the
 * compiz command line. (Eg, if your plugin depends on "cube", you
 * need to list that on the command line, but don't put in the
 * environment variable, because you don't want to dump the cube schema
 * into your plugin's schema file.)
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <compiz.h>
#undef _
#undef N_

#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <gconf-compiz-utils.h>

static FILE *schemaFile;

static int displayPrivateIndex;

typedef struct _GConfDumpDisplay {
    HandleEventProc handleEvent;
} GConfDumpDisplay;

#define GET_GCONF_DUMP_DISPLAY(d)				  \
    ((GConfDumpDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define GCONF_DUMP_DISPLAY(d)			      \
    GConfDumpDisplay *gd = GET_GCONF_DUMP_DISPLAY (d)

/* Decimal point character */
#define DECIMAL_POINT _(".")

static char **locales;

static gchar *actionSufix[] = {
    "key",
    "button",
    "bell",
    "edge",
    NULL
};

static void
gconfPrintf (int level, char *format, ...)
{
    va_list args;

    if (level > 0) {
	int i;

	for (i = 0; i < level * 2; i++)
	    fprintf (schemaFile, "  ");
    }

    va_start (args, format);
    vfprintf (schemaFile, format, args);
    va_end (args);
}

static char *
gconfTypeToString (CompOptionType type)
{
    switch (type) {
    case CompOptionTypeBool:
	return "bool";
    case CompOptionTypeColor:
	return "string";
    case CompOptionTypeInt:
	return "int";
    case CompOptionTypeFloat:
	return "float";
    case CompOptionTypeString:
	return "string";
    case CompOptionTypeList:
	return "list";
    default:
	break;
    }

    return "unknown";
}

static char *
gconfValueToString (CompDisplay	    *d,
		    CompOptionType  type,
		    CompOptionValue *value)
{
    char *tmp, *escaped;

    switch (type) {
    case CompOptionTypeBool:
	return g_strdup (value->b ? "true" : "false");
    case CompOptionTypeColor:
	return g_strdup_printf ("#%.2x%.2x%.2x%.2x",
				value->c[0] / 256,
				value->c[1] / 256,
				value->c[2] / 256,
				value->c[3] / 256);
    case CompOptionTypeInt:
	return g_strdup_printf ("%d", value->i);
    case CompOptionTypeFloat:
	return g_strdup_printf ("%f", value->f);
    case CompOptionTypeString:
	escaped = g_markup_escape_text (value->s, -1);
	return escaped;
    case CompOptionTypeList: {
	char *tmp2, *tmp3;
	int  i;

	tmp = g_strdup_printf ("[");

	for (i = 0; i < value->list.nValue; i++)
	{
	    tmp2 = gconfValueToString (d, value->list.type,
				       &value->list.value[i]);
	    tmp3 = g_strdup_printf ("%s%s%s", tmp, tmp2,
				    ((i + 1) < value->list.nValue) ? "," : "");
	    g_free (tmp);
	    g_free (tmp2);

	    tmp = tmp3;
	}

	tmp2 = g_strdup_printf ("%s]", tmp);
	g_free (tmp);
	escaped = g_markup_escape_text (tmp2, -1);
	g_free (tmp2);
	return escaped;
	}
    default:
	break;
    }

    return g_strdup ("unknown");
}

static char *
gconfActionValueToString (CompDisplay	  *d,
			  CompBindingType type,
			  CompOptionValue *value)
{
    char *tmp, *escaped;

    if (type == CompBindingTypeKey)
    {
	if (value->action.type & CompBindingTypeKey)
	{
	    tmp = gconfKeyBindingToString (d, &value->action.key);
	    escaped = g_markup_escape_text (tmp, -1);
	    g_free (tmp);
	}
	else
	    escaped = g_strdup ("Disabled");
    }
    else
    {
	if (value->action.type & CompBindingTypeButton)
	{
	    tmp = gconfButtonBindingToString (d, &value->action.button);
	    escaped = g_markup_escape_text (tmp, -1);
	    g_free (tmp);
	}
	else
	    escaped = g_strdup ("Disabled");
    }

    return escaped;
}

#define COMP_OPTION_IS_NUMERIC(o) \
	(isdigit ((unsigned char)o->name[strlen (o->name) - 1]))

static char *
translate_numeric (const char *str)
{
    char *p, *q, *denum, *out;

    p = strpbrk (str, "0123456789");
    if (!p)
	return g_strdup (str);
    q = p + strspn (p, "0123456789");

    denum = g_strdup_printf ("%.*s%%d%s", (int)(p - str), str, q);
    if (strcmp (denum, _(denum)) != 0)
	out = g_strdup_printf (_(denum), strtol (p, NULL, 10));
    else
	out = g_strdup (str);
    g_free (denum);

    return out;
}

static char *
getShortDesc (CompOption *o)
{
    if (COMP_OPTION_IS_NUMERIC (o))
	return translate_numeric (o->shortDesc);
    else
	return g_strdup (_(o->shortDesc));
}

static char *
getLongDesc (CompOption *o, const char *option_name)
{
    char *base, *out;

    if (COMP_OPTION_IS_NUMERIC (o))
	base = translate_numeric (o->longDesc);
    else
	base = g_strdup (_(o->longDesc));

    if (o->type == CompOptionTypeAction &&
	g_str_has_suffix (option_name, "_edge"))
    {
	GString *str = g_string_new (base);
	const char *edge;
	int i;

	g_string_append (str, " (");
	for (i = 0; i < SCREEN_EDGE_NUM; i++)
	{
	    edge = gconfEdgeToString (i);

	    if (i > 0)
		g_string_append (str, ", ");
	    if (!strcmp (edge, _(edge)))
	    {
		g_string_append (str, edge);
	    }
	    else
	    {
		g_string_append_printf (str, "%s [%s]",
					edge, _(edge));
	    }
	}
	g_string_append (str, ")");

	out = g_string_free (str, FALSE);
    }
    else if (o->type == CompOptionTypeInt)
    {
	out = g_strdup_printf ("%s (%d-%d)", base,
			       o->rest.i.min, o->rest.i.max);
    }
    else if (o->type == CompOptionTypeFloat)
    {
	int digits = -(logf (o->rest.f.precision) / logf (10));
	int invprec = 1.0 / o->rest.f.precision;

	/* Because we're sort of cheating on the locales, %.*f won't
	 * work here. We need to localize the format ourselves.
	 */
	out = g_strdup_printf ("%s (%d%s%0*d-%d%s%0*d)", base,
			       (int)o->rest.f.min, DECIMAL_POINT,
			       digits, (int)(o->rest.f.min * invprec) % invprec,
			       (int)o->rest.f.max, DECIMAL_POINT,
			       digits, (int)(o->rest.f.max * invprec) % invprec);
    }
    else if (o->type == CompOptionTypeString ||
	     (o->type == CompOptionTypeList &&
	      o->value.list.type == CompOptionTypeString))
    {
	GString *str = g_string_new (base);
	int i;

	if (o->rest.s.nString)
	{
	    g_string_append (str, " (");
	    for (i = 0; i < o->rest.s.nString; i++)
	    {
		if (i > 0)
		    g_string_append (str, ", ");
		if (!strcmp (o->rest.s.string[i], _(o->rest.s.string[i])))
		{
		    g_string_append (str, o->rest.s.string[i]);
		}
		else
		{
		    g_string_append_printf (str, "%s [%s]",
					    o->rest.s.string[i],
					    _(o->rest.s.string[i]));
		}
	    }
	    g_string_append (str, ")");
	}

	out = g_string_free (str, FALSE);
    }
    else
	return base;

    g_free (base);
    return out;
}

static void
gconfDumpToSchema (CompDisplay *d,
		   CompOption  *o,
		   char	       *name,
		   char	       *plugin,
		   char	       *screen)
{
    char *value;
    char *cshort, *clong, *lshort, *llong;
    int l;

    gconfPrintf (2, "<schema>\n");
    if (plugin && screen)
    {
	gconfPrintf (3, "<key>/schemas%s/plugins/%s/%s/options/%s</key>\n",
		     APP_NAME, plugin, screen, name);
	gconfPrintf (3, "<applyto>%s/plugins/%s/%s/options/%s</applyto>\n",
		     APP_NAME, plugin, screen, name);
    }
    else if (plugin)
    {
	gconfPrintf (3, "<key>/schemas%s/plugins/%s/%s</key>\n",
		     APP_NAME, plugin, name);
	gconfPrintf (3, "<applyto>%s/plugins/%s/%s</applyto>\n",
		     APP_NAME, plugin, name);
    }
    else
    {
	gconfPrintf (3, "<key>/schemas%s/general/%s/options/%s</key>\n",
		     APP_NAME, screen, name);
	gconfPrintf (3, "<applyto>%s/general/%s/options/%s</applyto>\n",
		     APP_NAME, screen, name);
    }
    gconfPrintf (3, "<owner>compiz</owner>\n");
    if (o->type == CompOptionTypeList)
    {
	gconfPrintf (3, "<type>%s</type>\n", gconfTypeToString (o->type));
	gconfPrintf (3, "<list_type>%s</list_type>\n",
		     gconfTypeToString (o->value.list.type));
	value = gconfValueToString (d, o->type, &o->value);
    }
    else if (o->type == CompOptionTypeAction)
    {
	gint len;

	len = strlen (name);
	if (strcmp (name + len - 3, "key") == 0)
	{
	    gconfPrintf (3, "<type>string</type>\n");
	    value = gconfActionValueToString (d, CompBindingTypeKey, &o->value);
	}
	else if (strcmp (name + len - 6, "button") == 0)
	{
	    gconfPrintf (3, "<type>string</type>\n");
	    value = gconfActionValueToString (d, CompBindingTypeButton,
					      &o->value);
	}
	else if (strcmp (name + len - 4, "bell") == 0)
	{
	    gconfPrintf (3, "<type>bool</type>\n");
	    value = g_strdup (o->value.action.bell ? "true" : "false");
	}
	else
	{
	    char *tmp1, *tmp2 = 0;
	    int  i;

	    gconfPrintf (3, "<type>list</type>\n");
	    gconfPrintf (3, "<list_type>string</list_type>\n");

	    tmp1 = g_strdup_printf ("[");

	    for (i = 0; i < SCREEN_EDGE_NUM; i++)
	    {
		if (o->value.action.edgeMask & (1 << i))
		{
		    tmp2 = g_strdup_printf ("%s%s%s", tmp1,
					    (tmp2) ? "," : "",
					    gconfEdgeToString (i));
		    g_free (tmp1);

		    tmp1 = tmp2;
		}
	    }

	    tmp2 = g_strdup_printf ("%s]", tmp1);
	    g_free (tmp1);
	    value = g_markup_escape_text (tmp2, -1);
	    g_free (tmp2);
	}
    }
    else
    {
	gconfPrintf (3, "<type>%s</type>\n", gconfTypeToString (o->type));
	value = gconfValueToString (d, o->type, &o->value);
    }

    gconfPrintf (3, "<default>%s</default>\n", value);
    g_free (value);

    setlocale (LC_ALL, "C");
    cshort = getShortDesc (o);
    clong = getLongDesc (o, name);
    gconfPrintf (3, "<locale name=\"C\">\n");
    gconfPrintf (4, "<short>%s</short>\n", cshort);
    gconfPrintf (4, "<long>%s</long>\n", clong);
    gconfPrintf (3, "</locale>\n");

    for (l = 0; locales[l]; l++)
    {
	/* setlocale (LC_ALL, "fr") won't work (it wants "fr_FR"),
	 * but this will.
	 */
	setenv ("LANGUAGE", locales[l], TRUE);
	setlocale (LC_ALL, "");

	lshort = getShortDesc (o);
	llong = getLongDesc (o, name);

	if (!strcmp (cshort, lshort) && !strcmp (clong, llong))
	    continue;

	gconfPrintf (3, "<locale name=\"%s\">\n", locales[l]);
	gconfPrintf (4, "<short>%s</short>\n", lshort);
	gconfPrintf (4, "<long>%s</long>\n", llong);
	gconfPrintf (3, "</locale>\n");

	g_free (lshort);
	g_free (llong);
    }

    g_free (cshort);
    g_free (clong);
    setlocale (LC_ALL, "C");

    gconfPrintf (2, "</schema>\n\n");
}

static void
dumpGeneralOptions (CompDisplay *d)
{
    CompOption   *option;
    int	         nOption;

    gconfPrintf (2, "<!-- general compiz options -->\n\n");

    option = compGetDisplayOptions (d, &nOption);
    while (nOption--)
    {
	if (!strcmp (option->name, "active_plugins"))
	{
	    /* "Fix" it for the schema file */
	    memmove (option->value.list.value + 1,
		     option->value.list.value,
		     (option->value.list.nValue - 1) *
		     sizeof (CompOptionValue));
	    option->value.list.value[0].s = strdup ("gconf");
	}

	if (option->type == CompOptionTypeAction)
	{
	    gchar *name;
	    int	  i = 0;

	    while (actionSufix[i])
	    {
		name = g_strdup_printf ("%s_%s", option->name, actionSufix[i]);
		gconfDumpToSchema (d, option, name, NULL, "allscreens");
		g_free (name);

		i++;
	    }
	}
	else
	{
	    gconfDumpToSchema (d, option, option->name, NULL, "allscreens");
	}

	option++;
    }

    option = compGetScreenOptions (&d->screens[0], &nOption);
    while (nOption--)
    {
	if (option->type == CompOptionTypeAction)
	{
	    gchar *name;
	    int	  i = 0;

	    while (actionSufix[i])
	    {
		name = g_strdup_printf ("%s_%s", option->name, actionSufix[i]);
		gconfDumpToSchema (d, option, name, NULL, "screen0");
		g_free (name);

		i++;
	    }
	}
	else
	{
	    gconfDumpToSchema (d, option, option->name, NULL, "screen0");
	}

	option++;
    }
}

static void
dumpPluginOptions (CompDisplay *d, CompPlugin *p)
{
    CompOption   *option, info, bopt, ropt;
    int	         nOption;
    GArray       *reqs, *before;

    if (!strcmp (p->vTable->name, "gconf-dump"))
	return;

    gconfPrintf (2, "<!-- %s options -->\n\n", p->vTable->name);

    memset (&info, 0, sizeof (info));
    info.name      = "info";
    info.shortDesc = p->vTable->shortDesc;
    info.longDesc  = p->vTable->longDesc;
    info.type      = CompOptionTypeString;
    info.value.s   = "";
    gconfDumpToSchema (d, &info, info.name, p->vTable->name, NULL);

    reqs   = g_array_new (FALSE, FALSE, sizeof (CompOptionValue));
    before = g_array_new (FALSE, FALSE, sizeof (CompOptionValue));

    if (p->vTable->deps)
    {
	int             i;
	CompOptionValue value;

	for (i = 0; i < p->vTable->nDeps; i++)
	{
	    value.s = p->vTable->deps[i].plugin;
	    if (p->vTable->deps[i].rule == CompPluginRuleBefore)
		g_array_append_val (before, value);
	    else
		g_array_append_val (reqs, value);
	}
    }

    memset (&bopt, 0, sizeof (bopt));
    bopt.name		   = "load_before";
    bopt.shortDesc	   = _("Plugins that this must load before");
    bopt.longDesc	   = _("Do not modify");
    bopt.type		   = CompOptionTypeList;
    bopt.value.list.type   = CompOptionTypeString;
    bopt.value.list.nValue = before->len;
    bopt.value.list.value  = (CompOptionValue *)before->data;
    gconfDumpToSchema (d, &bopt, bopt.name, p->vTable->name, NULL);

    memset (&ropt, 0, sizeof (ropt));
    ropt.name		   = "requires";
    ropt.shortDesc	   = _("Plugins that this requires");
    ropt.longDesc	   = _("Do not modify");
    ropt.type		   = CompOptionTypeList;
    ropt.value.list.type   = CompOptionTypeString;
    ropt.value.list.nValue = reqs->len;
    ropt.value.list.value  = (CompOptionValue *)reqs->data;
    gconfDumpToSchema (d, &ropt, ropt.name, p->vTable->name, NULL);

    g_array_free (before, TRUE);
    g_array_free (reqs, TRUE);

    if (p->vTable->getDisplayOptions)
    {
	option = p->vTable->getDisplayOptions (d, &nOption);
	while (nOption--)
	{
	    if (option->type == CompOptionTypeAction)
	    {
		gchar *name;
		int   i = 0;

		while (actionSufix[i])
		{
		    name = g_strdup_printf ("%s_%s",
					    option->name,
					    actionSufix[i]);
		    gconfDumpToSchema (d, option, name, p->vTable->name,
				       "allscreens");
		    g_free (name);

		    i++;
		}
	    }
	    else
	    {
		gconfDumpToSchema (d, option, option->name, p->vTable->name,
				   "allscreens");
	    }

	    option++;
	}
    }

    if (p->vTable->getScreenOptions)
    {
	option = p->vTable->getScreenOptions (&d->screens[0], &nOption);
	while (nOption--)
	{
	    if (option->type == CompOptionTypeAction)
	    {
		gchar *name;
		int   i = 0;

		while (actionSufix[i])
		{
		    name = g_strdup_printf ("%s_%s",
					    option->name,
					    actionSufix[i]);
		    gconfDumpToSchema (d, option, name, p->vTable->name,
				       "screen0");
		    g_free (name);

		    i++;
		}
	    }
	    else
	    {
		gconfDumpToSchema (d, option, option->name, p->vTable->name,
				   "screen0");
	    }

	    option++;
	}
    }
}

static int
strcmpref (const void *p1, const void *p2)
{
    return strcmp (*(const char **)p1, *(const char **)p2);
}

static int
dumpSchema (CompDisplay *d)
{
    const char *schemaFilename, *pluginList;
    char **plugins;
    int i;

    g_type_init ();

    if (findActivePlugin ("gconf"))
    {
	fprintf (stderr, "Can't use gconf-dump plugin with gconf plugin\n");
	return 1;
    }

    pluginList = getenv ("COMPIZ_SCHEMA_PLUGINS");
    if (!pluginList)
    {
	fprintf (stderr, "COMPIZ_SCHEMA_PLUGINS must be set\n");
	return 1;
    }

    plugins = g_strsplit (pluginList, " ", 0);
    qsort (plugins, g_strv_length (plugins), sizeof (char *), strcmpref);

    schemaFilename = getenv ("COMPIZ_SCHEMA_FILE");
    if (!schemaFilename)
	schemaFilename = "compiz.schemas.dump";

    schemaFile = fopen (schemaFilename, "w");
    if (!schemaFile)
    {
	fprintf (stderr, "Could not open %s: %s\n", schemaFilename,
		 strerror (errno));
	return 1;
    }

    gconfPrintf (0,
		 "<!--\n"
		 "      this file is autogenerated "
		 "by gconf-dump compiz plugin\n"
		 "  -->\n\n"
		 "<gconfschemafile>\n");
    gconfPrintf (1, "<schemalist>\n\n");

    if (getenv ("COMPIZ_SCHEMA_GENERAL"))
	dumpGeneralOptions (d);

    for (i = 0; plugins[i]; i++)
    {
	CompPlugin *plugin = findActivePlugin (plugins[i]);
	if (!plugin)
	{
	    fprintf (stderr, "No such plugin %s\n", plugins[i]);
	    return 1;
	}
	dumpPluginOptions (d, plugin);
    }

    gconfPrintf (1, "</schemalist>\n");
    gconfPrintf (0, "</gconfschemafile>\n");
    fclose (schemaFile);

    return 0;
}

static char *restartArgv[] = { "--replace", "gconf", NULL };

static void
gconfDumpHandleEvent (CompDisplay *d,
		      XEvent	  *event)
{
    int status;

    GCONF_DUMP_DISPLAY (d);

    UNWRAP (gd, d, handleEvent);

    status = dumpSchema (d);

    if (fork () != 0)
	exit (status);

    programArgv = restartArgv;
    kill (getpid (), SIGHUP);
}

static Bool
gconfDumpInitDisplay (CompPlugin  *p,
		      CompDisplay *d)
{
    GConfDumpDisplay *gd;

    gd = malloc (sizeof (GConfDumpDisplay));
    if (!gd)
	return FALSE;

    WRAP (gd, d, handleEvent, gconfDumpHandleEvent);

    d->privates[displayPrivateIndex].ptr = gd;

    return TRUE;
}

static void
gconfDumpFiniDisplay (CompPlugin  *p,
		      CompDisplay *d)
{
    GCONF_DUMP_DISPLAY (d);

    UNWRAP (gd, d, handleEvent);
    free (gd);
}

static Bool
gconfDumpInit (CompPlugin *p)
{
    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
	return FALSE;

    locales = g_strsplit (ALL_LINGUAS, " ", 0);
    setenv ("LANG", "en_US.UTF-8", TRUE);
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    return TRUE;
}

static void
gconfDumpFini (CompPlugin *p)
{
    if (displayPrivateIndex >= 0)
	freeDisplayPrivateIndex (displayPrivateIndex);
}

CompPluginVTable gconfDumpVTable = {
    "gconf-dump",
    "GConf dump",
    "GConf dump - dumps gconf schemas",
    gconfDumpInit,
    gconfDumpFini,
    gconfDumpInitDisplay,
    gconfDumpFiniDisplay,
    0, /* InitScreen */
    0, /* FiniScreen */
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0, /* SetScreenOption */
    0,
    0
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &gconfDumpVTable;
}
