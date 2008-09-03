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

static int corePrivateIndex;

typedef struct _KconfigCore {
    KConfig *config;

    CompTimeoutHandle   syncHandle;
    CompTimeoutHandle   reloadHandle;
    CompFileWatchHandle fileWatch;

    InitPluginForObjectProc initPluginForObject;
    SetOptionForPluginProc  setOptionForPlugin;
} KconfigCore;

#define GET_KCONFIG_CORE(c)				       \
    ((KconfigCore *) (c)->base.privates[corePrivateIndex].ptr)

#define KCONFIG_CORE(c)			   \
    KconfigCore *kc = GET_KCONFIG_CORE (c)


static void
kconfigRcChanged (const char *name,
		  void	     *closure);

static Bool
kconfigRcSync (void *closure)
{
    KCONFIG_CORE (&core);

    kc->config->sync ();

    kc->syncHandle = 0;

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
kconfigValueToString (CompObject      *object,
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
	char *action = NULL;

	while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
	    object = object->parent;

	if (object)
	    action = keyActionToString (GET_CORE_DISPLAY (object),
					&value->action);
	if (action)
	{
	    str = QString (action);
	    free (action);
	}
    } break;
    case CompOptionTypeButton: {
	char *action = NULL;

	while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
	    object = object->parent;

	if (object)
	    action = buttonActionToString (GET_CORE_DISPLAY (object),
					   &value->action);
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

static QString
kconfigObjectString (CompObject *object)
{
    QString objectName (QString (compObjectTypeName (object->type)));
    char    *name;

    name = compObjectName (object);
    if (name)
    {
	objectName += name;
	free (name);
    }

    return objectName;
}

static void
kconfigSetOption (CompObject *object,
		  CompOption *o,
		  const char *plugin)
{
    QString group (QString (plugin) + "_" + kconfigObjectString (object));

    KCONFIG_CORE (&core);

    kc->config->setGroup (group);

    switch (o->type) {
    case CompOptionTypeBool:
    case CompOptionTypeBell:
	kc->config->writeEntry (o->name,
				kconfigValueToBool (o->type, &o->value));
	break;
    case CompOptionTypeInt:
	kc->config->writeEntry (o->name, o->value.i);
	break;
    case CompOptionTypeFloat:
	kc->config->writeEntry (o->name, (double) o->value.f);
	break;
    case CompOptionTypeString:
    case CompOptionTypeColor:
    case CompOptionTypeKey:
    case CompOptionTypeButton:
    case CompOptionTypeEdge:
    case CompOptionTypeMatch:
	kc->config->writeEntry (o->name,
				kconfigValueToString (object, o->type,
						      &o->value));
	break;
    case CompOptionTypeList: {
	int i;

	switch (o->value.list.type) {
	case CompOptionTypeInt: {
	    QValueList< int > list;

	    for (i = 0; i < o->value.list.nValue; i++)
		list += o->value.list.value[i].i;

	    kc->config->writeEntry (o->name, list);
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
		list += kconfigValueToString (object,
					      o->value.list.type,
					      &o->value.list.value[i]);

	    kc->config->writeEntry (o->name, list);
	} break;
	case CompOptionTypeAction:
	case CompOptionTypeList:
	    break;
	}
    } break;
    case CompOptionTypeAction:
	return;
    }

    if (!kc->syncHandle)
	kc->syncHandle = compAddTimeout (0, 0, kconfigRcSync, 0);
}

static Bool
kconfigStringToValue (CompObject      *object,
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
	while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
	    object = object->parent;

	if (!object)
	    return FALSE;

	stringToKeyAction (GET_CORE_DISPLAY (object), str.ascii (),
			   &value->action);
	break;
    case CompOptionTypeButton:
	while (object && object->type != COMP_OBJECT_TYPE_DISPLAY)
	    object = object->parent;

	if (!object)
	    return FALSE;

	stringToButtonAction (GET_CORE_DISPLAY (object), str.ascii (),
			      &value->action);
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
kconfigReadOptionValue (CompObject	*object,
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
	if (!kconfigStringToValue (object,
				   config->readEntry (o->name), o->type,
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
			if (!kconfigStringToValue (object,
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
kconfigGetOption (CompObject *object,
		  CompOption *o,
		  const char *plugin)
{
    QString	  group (QString (plugin) + "_" +
			 kconfigObjectString (object));
    const QString name (o->name);

    KCONFIG_CORE (&core);

    kc->config->setGroup (group);

    if (kc->config->hasKey (name))
    {
	CompOptionValue value;

	if (kconfigReadOptionValue (object, kc->config, o, &value))
	{
	    (*core.setOptionForPlugin) (object, plugin, o->name, &value);
	    compFiniOptionValue (&value, o->type);
	}
    }
    else
    {
	kconfigSetOption (object, o, plugin);
    }
}

static CompBool
kconfigReloadObjectTree (CompObject *object,
			 void       *closure);

static CompBool
kconfigReloadObjectsWithType (CompObjectType type,
			      CompObject     *parent,
			      void	     *closure)
{
    compObjectForEach (parent, type, kconfigReloadObjectTree, closure);

    return TRUE;
}

static CompBool
kconfigReloadObjectTree (CompObject *object,
			 void       *closure)
{
    CompPlugin *p = (CompPlugin *) closure;
    CompOption  *option;
    int		nOption;

    option = (*p->vTable->getObjectOptions) (p, object, &nOption);
    while (nOption--)
	kconfigGetOption (object, option++, p->vTable->name);

    compObjectForEachType (object, kconfigReloadObjectsWithType, closure);

    return TRUE;
}

static Bool
kconfigRcReload (void *closure)
{
    CompPlugin  *p;

    KCONFIG_CORE (&core);

    kc->config->reparseConfiguration ();

    for (p = getPlugins (); p; p = p->next)
    {
	if (!p->vTable->getObjectOptions)
	    continue;

	kconfigReloadObjectTree (&core.base, (void *) p);
    }

    kc->reloadHandle = 0;

    return FALSE;
}

static void
kconfigRcChanged (const char *name,
		  void	     *closure)
{
    if (strcmp (name, COMPIZ_KCONFIG_RC) == 0)
    {
	KCONFIG_CORE (&core);

	if (!kc->reloadHandle)
	    kc->reloadHandle = compAddTimeout (0, 0, kconfigRcReload, closure);
    }
}

static CompBool
kconfigSetOptionForPlugin (CompObject      *object,
			   const char	   *plugin,
			   const char	   *name,
			   CompOptionValue *value)
{
    CompBool status;

    KCONFIG_CORE (&core);

    UNWRAP (kc, &core, setOptionForPlugin);
    status = (*core.setOptionForPlugin) (object, plugin, name, value);
    WRAP (kc, &core, setOptionForPlugin, kconfigSetOptionForPlugin);

    if (status && !kc->reloadHandle)
    {
	CompPlugin *p;

	p = findActivePlugin (plugin);
	if (p && p->vTable->getObjectOptions)
	{
	    CompOption *option;
	    int	       nOption;

	    option = (*p->vTable->getObjectOptions) (p, object, &nOption);
	    option = compFindOption (option, nOption, name, 0);
	    if (option)
		kconfigSetOption (object, option, p->vTable->name);
	}
    }

    return status;
}

static CompBool
kconfigInitPluginForObject (CompPlugin *p,
			    CompObject *o)
{
    CompBool status;

    KCONFIG_CORE (&core);

    UNWRAP (kc, &core, initPluginForObject);
    status = (*core.initPluginForObject) (p, o);
    WRAP (kc, &core, initPluginForObject, kconfigInitPluginForObject);

    if (status && p->vTable->getObjectOptions)
    {
	CompOption *option;
	int	   nOption;

	option = (*p->vTable->getObjectOptions) (p, o, &nOption);
	while (nOption--)
	    kconfigGetOption (o, option++, p->vTable->name);
    }

    return status;
}

static Bool
kconfigInitCore (CompPlugin *p,
		 CompCore   *c)
{
    KconfigCore *kc;
    QString	dir;

    if (!checkPluginABI ("core", CORE_ABIVERSION))
	return FALSE;

    kc = new KconfigCore;
    if (!kc)
	return FALSE;

    kc->config = new KConfig (COMPIZ_KCONFIG_RC);
    if (!kc->config)
    {
	delete kc;
	return FALSE;
    }

    kc->reloadHandle = compAddTimeout (0, 0, kconfigRcReload, 0);
    kc->syncHandle   = 0;
    kc->fileWatch    = 0;

    dir = KGlobal::dirs ()->saveLocation ("config", QString::null, false);

    if (QFile::exists (dir))
    {
	kc->fileWatch = addFileWatch (dir.ascii (), ~0, kconfigRcChanged, 0);
    }
    else
    {
	compLogMessage ("kconfig", CompLogLevelWarn, "Bad access \"%s\"",
			dir.ascii ());
    }

    WRAP (kc, c, initPluginForObject, kconfigInitPluginForObject);
    WRAP (kc, c, setOptionForPlugin, kconfigSetOptionForPlugin);

    c->base.privates[corePrivateIndex].ptr = kc;

    return TRUE;
}

static void
kconfigFiniCore (CompPlugin *p,
		 CompCore   *c)
{
    KCONFIG_CORE (c);

    UNWRAP (kc, c, initPluginForObject);
    UNWRAP (kc, c, setOptionForPlugin);

    if (kc->reloadHandle)
	compRemoveTimeout (kc->reloadHandle);

    if (kc->syncHandle)
    {
	compRemoveTimeout (kc->syncHandle);
	kconfigRcSync (0);
    }

    if (kc->fileWatch)
	removeFileWatch (kc->fileWatch);

    delete kc->config;
    delete kc;
}

static CompBool
kconfigInitObject (CompPlugin *p,
		   CompObject *o)
{
    static InitPluginObjectProc dispTab[] = {
	(InitPluginObjectProc) kconfigInitCore
    };

    RETURN_DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), TRUE, (p, o));
}

static void
kconfigFiniObject (CompPlugin *p,
		   CompObject *o)
{
    static FiniPluginObjectProc dispTab[] = {
	(FiniPluginObjectProc) kconfigFiniCore
    };

    DISPATCH (o, dispTab, ARRAY_SIZE (dispTab), (p, o));
}

static Bool
kconfigInit (CompPlugin *p)
{
    if (!compInitPluginMetadataFromInfo (&kconfigMetadata, p->vTable->name,
					 0, 0, 0, 0))
	return FALSE;

    corePrivateIndex = allocateCorePrivateIndex ();
    if (corePrivateIndex < 0)
    {
	compFiniMetadata (&kconfigMetadata);
	return FALSE;
    }

    kInstance = new KInstance ("compiz-kconfig");
    if (!kInstance)
    {
	freeCorePrivateIndex (corePrivateIndex);
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

    freeCorePrivateIndex (corePrivateIndex);
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
    kconfigInitObject,
    kconfigFiniObject,
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &kconfigVTable;
}
