/*
 * Copyright © 2007 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <kglobal.h>
#include <kstandarddirs.h>
#include <kapplication.h>
#include <ksimpleconfig.h>
#include <qfile.h>

#include <compiz-core.h>

#define COMPIZ_KCONFIG_RC "compizrc"

static KInstance *kInstance;

static CompMetadata kconfigMetadata;

static int displayPrivateIndex;

typedef struct _KconfigDisplay {
    int screenPrivateIndex;

    InitPluginForDisplayProc      initPluginForDisplay;
    SetDisplayOptionForPluginProc setDisplayOptionForPlugin;

    KConfig *config;

    CompTimeoutHandle   syncHandle;
    CompTimeoutHandle   reloadHandle;
    CompFileWatchHandle fileWatch;
} KconfigDisplay;

typedef struct _KconfigScreen {
    InitPluginForScreenProc      initPluginForScreen;
    SetScreenOptionForPluginProc setScreenOptionForPlugin;
} KconfigScreen;

#define GET_KCONFIG_DISPLAY(d)				        \
    ((KconfigDisplay *) (d)->privates[displayPrivateIndex].ptr)

#define KCONFIG_DISPLAY(d)		         \
    KconfigDisplay *kd = GET_KCONFIG_DISPLAY (d)

#define GET_KCONFIG_SCREEN(s, kd)				    \
    ((KconfigScreen *) (s)->privates[(kd)->screenPrivateIndex].ptr)

#define KCONFIG_SCREEN(s)					              \
    KconfigScreen *ks = GET_KCONFIG_SCREEN (s,				      \
					    GET_KCONFIG_DISPLAY (s->display))

static void
kconfigRcChanged (const char *name,
		  void	     *closure);

static Bool
kconfigRcSync (void *closure)
{
    CompDisplay *d = (CompDisplay *) closure;

    KCONFIG_DISPLAY (d);

    kd->config->sync ();

    kd->syncHandle = 0;

    return FALSE;
}

static bool
kconfigValueToBool (CompOptionType  type,
		    CompOptionValue *value)
{
    switch (type) {
    case CompOptionTypeBool:
	return (value->b) ? true : false;
    case CompOptionTypeBell:
	return (value->action.bell) ? true : false;
    default:
	break;
    }

    return false;
}

static QString
kconfigValueToString (CompDisplay     *d,
		      CompOptionType  type,
		      CompOptionValue *value)
{
    QString str;

    switch (type) {
    case CompOptionTypeBool:
	str = QString::number (value->b ? TRUE : FALSE);
	break;
    case CompOptionTypeFloat:
	str = QString::number (value->f);
	break;
    case CompOptionTypeString:
	str = QString (value->s);
	break;
    case CompOptionTypeColor: {
	char *color;

	color = colorToString (value->c);
	if (color)
	{
	    str = QString (color);
	    free (color);
	}
    } break;
    case CompOptionTypeKey: {
	char *action;

	action = keyActionToString (d, &value->action);
	if (action)
	{
	    str = QString (action);
	    free (action);
	}
    } break;
    case CompOptionTypeButton: {
	char *action;

	action = buttonActionToString (d, &value->action);
	if (action)
	{
	    str = QString (action);
	    free (action);
	}
    } break;
    case CompOptionTypeEdge: {
	char *edge;

	edge = edgeMaskToString (value->action.edgeMask);
	if (edge)
	{
	    str = QString (edge);
	    free (edge);
	}
    } break;
    case CompOptionTypeBell:
	str = QString::number (value->action.bell ? TRUE : FALSE);
	break;
    case CompOptionTypeMatch: {
	char *match;

	match = matchToString (&value->match);
	if (match)
	{
	    str = QString (match);
	    free (match);
	}
    }
    default:
	break;
    }

    return str;
}

static void
kconfigSetOption (CompDisplay *d,
		  CompOption  *o,
		  const char  *plugin,
		  const char  *object)
{
    QString group (QString (plugin) + "_" + QString (object));

    KCONFIG_DISPLAY (d);

    kd->config->setGroup (group);

    switch (o->type) {
    case CompOptionTypeBool:
    case CompOptionTypeBell:
	kd->config->writeEntry (o->name,
				kconfigValueToBool (o->type, &o->value));
	break;
    case CompOptionTypeInt:
	kd->config->writeEntry (o->name, o->value.i);
	break;
    case CompOptionTypeFloat:
	kd->config->writeEntry (o->name, (double) o->value.f);
	break;
    case CompOptionTypeString:
    case CompOptionTypeColor:
    case CompOptionTypeKey:
    case CompOptionTypeButton:
    case CompOptionTypeEdge:
    case CompOptionTypeMatch:
	kd->config->writeEntry (o->name,
				kconfigValueToString (d, o->type, &o->value));
	break;
    case CompOptionTypeList: {
	int i;

	switch (o->value.list.type) {
	case CompOptionTypeInt: {
	    QValueList< int > list;

	    for (i = 0; i < o->value.list.nValue; i++)
		list += o->value.list.value[i].i;

	    kd->config->writeEntry (o->name, list);
	} break;
	case CompOptionTypeBool:
	case CompOptionTypeFloat:
	case CompOptionTypeString:
	case CompOptionTypeColor:
	case CompOptionTypeKey:
	case CompOptionTypeButton:
	case CompOptionTypeEdge:
	case CompOptionTypeBell:
	case CompOptionTypeMatch: {
	    QStringList list;

	    for (i = 0; i < o->value.list.nValue; i++)
		list += kconfigValueToString (d,
					      o->value.list.type,
					      &o->value.list.value[i]);

	    kd->config->writeEntry (o->name, list);
	} break;
	case CompOptionTypeAction:
	case CompOptionTypeList:
	    break;
	}
    } break;
    case CompOptionTypeAction:
	return;
    }

    if (!kd->syncHandle)
	kd->syncHandle = compAddTimeout (0, kconfigRcSync, (void *) d);
}

static Bool
kconfigStringToValue (CompDisplay     *d,
		      QString	      str,
		      CompOptionType  type,
		      CompOptionValue *value)
{
    switch (type) {
    case CompOptionTypeBool:
	value->b = str.toInt () ? TRUE : FALSE;
	break;
    case CompOptionTypeFloat:
	value->f = str.toFloat ();
	break;
    case CompOptionTypeString:
	value->s = strdup (str.ascii ());
	if (!value->s)
	    return FALSE;
	break;
    case CompOptionTypeColor:
	if (!stringToColor (str.ascii (), value->c))
	    return FALSE;
	break;
    case CompOptionTypeKey:
	stringToKeyAction (d, str.ascii (), &value->action);
	break;
    case CompOptionTypeButton:
	stringToButtonAction (d, str.ascii (), &value->action);
	break;
    case CompOptionTypeEdge:
	value->action.edgeMask = stringToEdgeMask (str.ascii ());
	break;
    case CompOptionTypeBell:
	value->action.bell = str.toInt () ? TRUE : FALSE;
	break;
    case CompOptionTypeMatch:
	matchInit (&value->match);
	matchAddFromString (&value->match, str.ascii ());
	break;
    default:
	return FALSE;
    }

    return TRUE;
}

static void
kconfigBoolToValue (bool	    b,
		    CompOptionType  type,
		    CompOptionValue *value)
{
    switch (type) {
    case CompOptionTypeBool:
	value->b = (b) ? TRUE : FALSE;
	break;
    case CompOptionTypeBell:
	value->action.bell = (b) ? TRUE : FALSE;
    default:
	break;
    }
}

static Bool
kconfigReadOptionValue (CompDisplay	*d,
			KConfig		*config,
			CompOption	*o,
			CompOptionValue *value)
{
    compInitOptionValue (value);

    switch (o->type) {
    case CompOptionTypeBool:
    case CompOptionTypeBell:
	kconfigBoolToValue (config->readBoolEntry (o->name), o->type, value);
	break;
    case CompOptionTypeInt:
	value->i = config->readNumEntry (o->name);
	break;
    case CompOptionTypeFloat:
	value->f = config->readDoubleNumEntry (o->name);
	break;
    case CompOptionTypeString:
    case CompOptionTypeColor:
    case CompOptionTypeKey:
    case CompOptionTypeButton:
    case CompOptionTypeEdge:
    case CompOptionTypeMatch:
	if (!kconfigStringToValue (d, config->readEntry (o->name), o->type,
				   value))
	    return FALSE;
	break;
    case CompOptionTypeList: {
	int n, i;

	value->list.value  = NULL;
	value->list.nValue = 0;
	value->list.type   = o->value.list.type;

	switch (o->value.list.type) {
	case CompOptionTypeInt: {
	    QValueList< int > list;

	    list = config->readIntListEntry (o->name);

	    n = list.size ();
	    if (n)
	    {
		value->list.value = (CompOptionValue *)
		    malloc (sizeof (CompOptionValue) * n);
		if (value->list.value)
		{
		    for (i = 0; i < n; i++)
			value->list.value[i].i = list[i];

		    value->list.nValue = n;
		}
	    }
	} break;
	case CompOptionTypeBool:
	case CompOptionTypeFloat:
	case CompOptionTypeString:
	case CompOptionTypeColor:
	case CompOptionTypeKey:
	case CompOptionTypeButton:
	case CompOptionTypeEdge:
	case CompOptionTypeBell:
	case CompOptionTypeMatch: {
	    QStringList list;

	    list = config->readListEntry (o->name);

	    n = list.size ();
	    if (n)
	    {
		value->list.value = (CompOptionValue *)
		    malloc (sizeof (CompOptionValue) * n);
		if (value->list.value)
		{
		    for (i = 0; i < n; i++)
		    {
			if (!kconfigStringToValue (d,
						   list[i],
						   value->list.type,
						   &value->list.value[i]))
			    break;

			value->list.nValue++;
		    }

		    if (value->list.nValue != n)
		    {
			compFiniOptionValue (value, o->type);
			return FALSE;
		    }
		}
	    }
	} break;
	case CompOptionTypeList:
	case CompOptionTypeAction:
	    return FALSE;
	}
    } break;
    case CompOptionTypeAction:
	return FALSE;
	break;
    }

    return TRUE;
}

static void
kconfigGetDisplayOption (CompDisplay *d,
			 CompOption  *o,
			 const char  *plugin)
{
    QString       group (QString (plugin) + "_display");
    const QString name (o->name);

    KCONFIG_DISPLAY (d);

    kd->config->setGroup (group);

    if (kd->config->hasKey (name))
    {
	CompOptionValue value;

	if (kconfigReadOptionValue (d, kd->config, o, &value))
	{
	    (*d->setDisplayOptionForPlugin) (d, plugin, o->name, &value);
	    compFiniOptionValue (&value, o->type);
	}
    }
    else
    {
	kconfigSetOption (d, o, plugin, "display");
    }
}

static void
kconfigGetScreenOption (CompScreen *s,
			CompOption *o,
			const char *plugin,
			const char *screen)
{
    QString       group (QString (plugin) + "_" + QString (screen));
    const QString name (o->name);

    KCONFIG_DISPLAY (s->display);

    kd->config->setGroup (group);

    if (kd->config->hasKey (name))
    {
	CompOptionValue value;

	if (kconfigReadOptionValue (s->display, kd->config, o, &value))
	{
	    (*s->setScreenOptionForPlugin) (s, plugin, o->name, &value);

	    compFiniOptionValue (&value, o->type);
	}
    }
    else
    {
	kconfigSetOption (s->display, o, plugin, screen);
    }
}

static Bool
kconfigRcReload (void *closure)
{
    CompDisplay *d = (CompDisplay *) closure;
    CompScreen  *s;
    CompPlugin  *p;
    CompOption  *option;
    int		nOption;

    KCONFIG_DISPLAY (d);

    kd->config->reparseConfiguration ();

    for (p = getPlugins (); p; p = p->next)
    {
	if (!p->vTable->getDisplayOptions)
	    continue;

	option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
	while (nOption--)
	    kconfigGetDisplayOption (d, option++, p->vTable->name);
    }

    for (s = d->screens; s; s = s->next)
    {
	QString screen ("screen" + QString::number (s->screenNum));

	for (p = getPlugins (); p; p = p->next)
	{
	    if (!p->vTable->getScreenOptions)
		continue;

	    option = (*p->vTable->getScreenOptions) (p, s, &nOption);
	    while (nOption--)
		kconfigGetScreenOption (s, option++, p->vTable->name,
					screen.ascii ());
	}
    }

    kd->reloadHandle = 0;

    return FALSE;
}

static void
kconfigRcChanged (const char *name,
		  void	     *closure)
{
    CompDisplay *d = (CompDisplay *) closure;

    KCONFIG_DISPLAY (d);

    if (strcmp (name, COMPIZ_KCONFIG_RC) == 0)
    {
	if (!kd->reloadHandle)
	    kd->reloadHandle = compAddTimeout (0, kconfigRcReload, closure);
    }
}

static Bool
kconfigSetDisplayOptionForPlugin (CompDisplay     *d,
				  const char	  *plugin,
				  const char	  *name,
				  CompOptionValue *value)
{
    Bool status;

    KCONFIG_DISPLAY (d);

    UNWRAP (kd, d, setDisplayOptionForPlugin);
    status = (*d->setDisplayOptionForPlugin) (d, plugin, name, value);
    WRAP (kd, d, setDisplayOptionForPlugin, kconfigSetDisplayOptionForPlugin);

    if (status && !kd->reloadHandle)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getDisplayOptions)
	{
	    CompOption *option;
	    int	       nOption;

	    option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
	    option = compFindOption (option, nOption, name, 0);
	    if (option)
		kconfigSetOption (d, option, p->vTable->name, "display");
	}
    }

    return status;
}

static Bool
kconfigSetScreenOptionForPlugin (CompScreen      *s,
				 const char	 *plugin,
				 const char	 *name,
				 CompOptionValue *value)
{
    Bool status;

    KCONFIG_SCREEN (s);

    UNWRAP (ks, s, setScreenOptionForPlugin);
    status = (*s->setScreenOptionForPlugin) (s, plugin, name, value);
    WRAP (ks, s, setScreenOptionForPlugin, kconfigSetScreenOptionForPlugin);

    if (status)
    {
	KCONFIG_DISPLAY (s->display);

	if (!kd->reloadHandle)
	{
	    CompPlugin *p;

	    p = findActivePlugin (plugin);
	    if (p && p->vTable->getScreenOptions)
	    {
		CompOption *option;
		int	   nOption;
		QString    screen ("screen");

		screen += QString::number (s->screenNum);

		option = (*p->vTable->getScreenOptions) (p, s, &nOption);
		option = compFindOption (option, nOption, name, 0);
		if (option)
		    kconfigSetOption (s->display, option, plugin,
				      screen.ascii ());
	    }
	}
    }

    return status;
}

static Bool
kconfigInitPluginForDisplay (CompPlugin  *p,
			     CompDisplay *d)
{
    Bool status;

    KCONFIG_DISPLAY (d);

    UNWRAP (kd, d, initPluginForDisplay);
    status = (*d->initPluginForDisplay) (p, d);
    WRAP (kd, d, initPluginForDisplay, kconfigInitPluginForDisplay);

    if (status && p->vTable->getDisplayOptions)
    {
	CompOption *option;
	int	   nOption;

	option = (*p->vTable->getDisplayOptions) (p, d, &nOption);
	while (nOption--)
	    kconfigGetDisplayOption (d, option++, p->vTable->name);
    }

    return status;
}

static Bool
kconfigInitPluginForScreen (CompPlugin *p,
			    CompScreen *s)
{
    Bool status;

    KCONFIG_SCREEN (s);

    UNWRAP (ks, s, initPluginForScreen);
    status = (*s->initPluginForScreen) (p, s);
    WRAP (ks, s, initPluginForScreen, kconfigInitPluginForScreen);

    if (status && p->vTable->getScreenOptions)
    {
	CompOption *option;
	int	   nOption;
	QString    screen ("screen");

	screen += QString::number (s->screenNum);

	option = (*p->vTable->getScreenOptions) (p, s, &nOption);
	while (nOption--)
	    kconfigGetScreenOption (s, option++, p->vTable->name,
				    screen.ascii ());
    }

    return status;
}

static Bool
kconfigInitDisplay (CompPlugin  *p,
		    CompDisplay *d)
{
    KconfigDisplay *kd;
    QString	   dir;

    if (!checkPluginABI ("core", ABIVERSION))
	return FALSE;

    kd = new KconfigDisplay;
    if (!kd)
	return FALSE;

    kd->config = new KConfig (COMPIZ_KCONFIG_RC);
    if (!kd->config)
    {
	delete kd;
	return FALSE;
    }

    kd->screenPrivateIndex = allocateScreenPrivateIndex (d);
    if (kd->screenPrivateIndex < 0)
    {
	delete kd->config;
	delete kd;
	return FALSE;
    }

    kd->reloadHandle = compAddTimeout (0, kconfigRcReload, (void *) d);
    kd->syncHandle   = 0;
    kd->fileWatch    = 0;

    dir = KGlobal::dirs ()->saveLocation ("config", QString::null, false);

    if (QFile::exists (dir))
    {
	kd->fileWatch = addFileWatch (d, dir.ascii (), ~0, kconfigRcChanged,
				      (void *) d);
    }
    else
    {
	compLogMessage (d, "kconfig", CompLogLevelWarn, "Bad access \"%s\"",
			dir.ascii ());
    }

    WRAP (kd, d, initPluginForDisplay, kconfigInitPluginForDisplay);
    WRAP (kd, d, setDisplayOptionForPlugin, kconfigSetDisplayOptionForPlugin);

    d->privates[displayPrivateIndex].ptr = kd;

    return TRUE;
}

static void
kconfigFiniDisplay (CompPlugin  *p,
		    CompDisplay *d)
{
    KCONFIG_DISPLAY (d);

    UNWRAP (kd, d, initPluginForDisplay);
    UNWRAP (kd, d, setDisplayOptionForPlugin);

    if (kd->reloadHandle)
	compRemoveTimeout (kd->reloadHandle);

    if (kd->syncHandle)
    {
	compRemoveTimeout (kd->syncHandle);
	kconfigRcSync (d);
    }

    if (kd->fileWatch)
	removeFileWatch (d, kd->fileWatch);

    freeScreenPrivateIndex (d, kd->screenPrivateIndex);

    delete kd->config;
    delete kd;
}

static Bool
kconfigInitScreen (CompPlugin *p,
		   CompScreen *s)
{
    KconfigScreen *ks;

    KCONFIG_DISPLAY (s->display);

    ks = new KconfigScreen;
    if (!ks)
	return FALSE;

    WRAP (ks, s, initPluginForScreen, kconfigInitPluginForScreen);
    WRAP (ks, s, setScreenOptionForPlugin, kconfigSetScreenOptionForPlugin);

    s->privates[kd->screenPrivateIndex].ptr = ks;

    return TRUE;
}

static void
kconfigFiniScreen (CompPlugin *p,
		   CompScreen *s)
{
    KCONFIG_SCREEN (s);

    UNWRAP (ks, s, initPluginForScreen);
    UNWRAP (ks, s, setScreenOptionForPlugin);

    delete ks;
}

static Bool
kconfigInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&kconfigMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    displayPrivateIndex = allocateDisplayPrivateIndex ();
    if (displayPrivateIndex < 0)
    {
	compFiniMetadata (&kconfigMetadata);
	return FALSE;
    }

    kInstance = new KInstance ("compiz-kconfig");
    if (!kInstance)
    {
	freeDisplayPrivateIndex (displayPrivateIndex);
	compFiniMetadata (&kconfigMetadata);
	return FALSE;
    }

    compAddMetadataFromFile (&kconfigMetadata, p->vTable->name);

    return TRUE;
}

static void
kconfigFini (CompPlugin *p)
{
    delete kInstance;

    freeDisplayPrivateIndex (displayPrivateIndex);
    compFiniMetadata (&kconfigMetadata);
}

static CompMetadata *
kconfigGetMetadata (CompPlugin *plugin)
{
    return &kconfigMetadata;
}

CompPluginVTable kconfigVTable = {
    "kconfig",
    kconfigGetMetadata,
    kconfigInit,
    kconfigFini,
    kconfigInitDisplay,
    kconfigFiniDisplay,
    kconfigInitScreen,
    kconfigFiniScreen,
    0, /* InitWindow */
    0, /* FiniWindow */
    0, /* GetDisplayOptions */
    0, /* SetDisplayOption */
    0, /* GetScreenOptions */
    0  /* SetScreenOption */
};

CompPluginVTable *
getCompPluginInfo (void)
{
    return &kconfigVTable;
}
