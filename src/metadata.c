/*
 * Copyright © 2007 Dennis Kasprzyk
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Dennis Kasprzyk not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Dennis Kasprzyk makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DENNIS KASPRZYK DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DENNIS KASPRZYK BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Dennis Kasprzyk <onestone@deltatauchi.de>
 *          David Reveman <davidr@novell.com>
 */

#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <locale.h>

#include <compiz-core.h>

#define HOME_METADATADIR ".compiz/metadata"
#define EXTENSION ".xml"

Bool
compInitMetadata (CompMetadata *metadata)
{
    metadata->path = strdup ("core");
    if (!metadata->path)
	return FALSE;

    metadata->doc  = NULL;
    metadata->nDoc = 0;

    return TRUE;
}

Bool
compInitPluginMetadata (CompMetadata *metadata,
			const char   *plugin)
{
    char str[1024];

    snprintf (str, 1024, "plugin[@name=\"%s\"]", plugin);

    metadata->path = strdup (str);
    if (!metadata->path)
	return FALSE;

    metadata->doc  = NULL;
    metadata->nDoc = 0;

    return TRUE;
}

void
compFiniMetadata (CompMetadata *metadata)
{
    int i;

    for (i = 0; i < metadata->nDoc; i++)
	xmlFreeDoc (metadata->doc[i]);

    if (metadata->doc)
	free (metadata->doc);

    free (metadata->path);
}

static xmlDoc *
readXmlFile (const char	*path,
	     const char	*name)
{
    char   *file;
    int    length = strlen (name) + strlen (EXTENSION) + 1;
    xmlDoc *doc = NULL;
    FILE   *fp;

    if (path)
	length += strlen (path) + 1;

    file = malloc (length);
    if (!file)
	return NULL;

    if (path)
	sprintf (file, "%s/%s%s", path, name, EXTENSION);
    else
	sprintf (file, "%s%s", name, EXTENSION);

    fp = fopen (file, "r");
    if (!fp)
    {
	free (file);
	return NULL;
    }

    fclose (fp);

    doc = xmlReadFile (file, NULL, 0);

    free (file);

    return doc;
}

static Bool
addMetadataFromFilename (CompMetadata *metadata,
			 const char   *path,
			 const char   *file)
{
    xmlDoc **d, *doc;

    doc = readXmlFile (path, file);
    if (!doc)
	return FALSE;

    d = realloc (metadata->doc, (metadata->nDoc + 1) * sizeof (xmlDoc *));
    if (!d)
    {
	xmlFreeDoc (doc);
	return FALSE;
    }

    d[metadata->nDoc++] = doc;
    metadata->doc = d;

    return TRUE;
}

Bool
compAddMetadataFromFile (CompMetadata *metadata,
			 const char   *file)
{
    char *home;
    Bool status = FALSE;

    home = getenv ("HOME");
    if (home)
    {
	char *path;

	path = malloc (strlen (home) + strlen (HOME_METADATADIR) + 2);
	if (path)
	{
	    sprintf (path, "%s/%s", home, HOME_METADATADIR);
	    status |= addMetadataFromFilename (metadata, path, file);
	    free (path);
	}
    }

    status |= addMetadataFromFilename (metadata, METADATADIR, file);
    if (!status)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"Unable to parse XML metadata from file \"%s%s\"",
			file, EXTENSION);

	return FALSE;
    }

    return TRUE;
}

Bool
compAddMetadataFromString (CompMetadata *metadata,
			   const char   *string)
{
    xmlDoc **d, *doc;

    doc = xmlReadMemory (string, strlen (string), NULL, NULL, 0);
    if (!doc)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"Unable to parse XML metadata");

	return FALSE;
    }

    d = realloc (metadata->doc, (metadata->nDoc + 1) * sizeof (xmlDoc *));
    if (!d)
    {
	xmlFreeDoc (doc);
	return FALSE;
    }

    d[metadata->nDoc++] = doc;
    metadata->doc = d;

    return TRUE;
}

Bool
compAddMetadataFromIO (CompMetadata	     *metadata,
		       xmlInputReadCallback  ioread,
		       xmlInputCloseCallback ioclose,
		       void		     *ioctx)
{
    xmlDoc **d, *doc;

    doc = xmlReadIO (ioread, ioclose, ioctx, NULL, NULL, 0);
    if (!doc)
    {
	compLogMessage ("core", CompLogLevelWarn,
			"Unable to parse XML metadata");

	return FALSE;
    }

    d = realloc (metadata->doc, (metadata->nDoc + 1) * sizeof (xmlDoc *));
    if (!d)
    {
	xmlFreeDoc (doc);
	return FALSE;
    }

    d[metadata->nDoc++] = doc;
    metadata->doc = d;

    return TRUE;
}

typedef struct _CompIOCtx {
    int				 offset;
    const char			 *name;
    const CompMetadataOptionInfo *displayOInfo;
    int				 nDisplayOInfo;
    const CompMetadataOptionInfo *screenOInfo;
    int				 nScreenOInfo;
} CompIOCtx;

static int
readPluginXmlCallback (void *context,
		       char *buffer,
		       int  length)
{
    CompIOCtx *ctx = (CompIOCtx *) context;
    int	      offset = ctx->offset;
    int	      i, j;

    i = compReadXmlChunk ("<compiz><plugin name=\"", &offset, buffer, length);
    i += compReadXmlChunk (ctx->name, &offset, buffer + i, length - i);
    i += compReadXmlChunk ("\">", &offset, buffer + i, length - i);

    if (ctx->nDisplayOInfo)
    {
	i += compReadXmlChunk ("<display>", &offset, buffer + i, length - i);

	for (j = 0; j < ctx->nDisplayOInfo; j++)
	    i += compReadXmlChunkFromMetadataOptionInfo (&ctx->displayOInfo[j],
							 &offset,
							 buffer + i,
							 length - i);

	i += compReadXmlChunk ("</display>", &offset, buffer + i, length - i);
    }

    if (ctx->nScreenOInfo)
    {
	i += compReadXmlChunk ("<screen>", &offset, buffer + i, length - i);

	for (j = 0; j < ctx->nScreenOInfo; j++)
	    i += compReadXmlChunkFromMetadataOptionInfo (&ctx->screenOInfo[j],
							 &offset,
							 buffer + i,
							 length - i);

	i += compReadXmlChunk ("</screen>", &offset, buffer + i, length - i);
    }

    i += compReadXmlChunk ("</plugin></compiz>", &offset, buffer + i,
			   length - i);

    if (!offset && length > i)
	buffer[i++] = '\0';

    ctx->offset += i;

    return i;
}

Bool
compInitPluginMetadataFromInfo (CompMetadata		     *metadata,
				const char		     *plugin,
				const CompMetadataOptionInfo *displayOptionInfo,
				int			     nDisplayOptionInfo,
				const CompMetadataOptionInfo *screenOptionInfo,
				int			     nScreenOptionInfo)
{
    if (!compInitPluginMetadata (metadata, plugin))
	return FALSE;

    if (nDisplayOptionInfo || nScreenOptionInfo)
    {
	CompIOCtx ctx;

	ctx.offset	  = 0;
	ctx.name	  = plugin;
	ctx.displayOInfo  = displayOptionInfo;
	ctx.nDisplayOInfo = nDisplayOptionInfo;
	ctx.screenOInfo   = screenOptionInfo;
	ctx.nScreenOInfo  = nScreenOptionInfo;

	if (!compAddMetadataFromIO (metadata,
				    readPluginXmlCallback, NULL,
				    (void *) &ctx))
	{
	    compFiniMetadata (metadata);
	    return FALSE;
	}
    }

    return TRUE;
}

typedef struct _CompXPath {
    xmlXPathObjectPtr  obj;
    xmlXPathContextPtr ctx;
    xmlDocPtr	       doc;
} CompXPath;

static Bool
initXPathFromMetadataPath (CompXPath	 *xPath,
			   CompMetadata  *metadata,
			   const xmlChar *path)
{
    xmlXPathObjectPtr  obj;
    xmlXPathContextPtr ctx;
    int		       i;

    for (i = 0; i < metadata->nDoc; i++)
    {
	ctx = xmlXPathNewContext (metadata->doc[i]);
	if (ctx)
	{
	    obj = xmlXPathEvalExpression (path, ctx);
	    if (obj)
	    {
		if (obj->nodesetval && obj->nodesetval->nodeNr)
		{
		    xPath->ctx = ctx;
		    xPath->obj = obj;
		    xPath->doc = metadata->doc[i];

		    return TRUE;
		}

		xmlXPathFreeObject (obj);
	    }

	    xmlXPathFreeContext (ctx);
	}
    }

    return FALSE;
}

static Bool
initXPathFromMetadataPathElement (CompXPath	*xPath,
				  CompMetadata  *metadata,
				  const xmlChar *path,
				  const xmlChar *element)
{
    char str[1024];

    snprintf (str, 1024, "%s/%s", path, element);

    return initXPathFromMetadataPath (xPath, metadata, BAD_CAST str);
}

static void
finiXPath (CompXPath *xPath)
{
    xmlXPathFreeObject (xPath->obj);
    xmlXPathFreeContext (xPath->ctx);
}

static CompOptionType
getOptionType (char *name)
{
    static struct _TypeMap {
	char	       *name;
	CompOptionType type;
    } map[] = {
	{ "int",    CompOptionTypeInt    },
	{ "float",  CompOptionTypeFloat  },
	{ "string", CompOptionTypeString },
	{ "color",  CompOptionTypeColor  },
	{ "action", CompOptionTypeAction },
	{ "key",    CompOptionTypeKey    },
	{ "button", CompOptionTypeButton },
	{ "edge",   CompOptionTypeEdge   },
	{ "bell",   CompOptionTypeBell   },
	{ "match",  CompOptionTypeMatch  },
	{ "list",   CompOptionTypeList   }
    };
    int i;

    for (i = 0; i < sizeof (map) / sizeof (map[0]); i++)
	if (strcasecmp (name, map[i].name) == 0)
	    return map[i].type;

    return CompOptionTypeBool;
}

static void
initBoolValue (CompOptionValue *v,
	       xmlDocPtr       doc,
	       xmlNodePtr      node)
{
    xmlChar *value;

    v->b = FALSE;

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	if (strcasecmp ((char *) value, "true") == 0)
	    v->b = TRUE;

	xmlFree (value);
    }
}

static void
initIntValue (CompOptionValue	    *v,
	      CompOptionRestriction *r,
	      xmlDocPtr		    doc,
	      xmlNodePtr	    node)
{
    xmlChar *value;

    v->i = (r->i.min + r->i.max) / 2;

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	int i = strtol ((char *) value, NULL, 0);

	if (i >= r->i.min && i <= r->i.max)
	    v->i = i;

	xmlFree (value);
    }
}

static void
initFloatValue (CompOptionValue	      *v,
		CompOptionRestriction *r,
		xmlDocPtr	      doc,
		xmlNodePtr	      node)
{
    xmlChar *value;
    char *loc;

    v->f = (r->f.min + r->f.max) / 2;

    if (!doc)
	return;

    loc = setlocale (LC_NUMERIC, NULL);
    setlocale (LC_NUMERIC, "C");
    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	float f = strtod ((char *) value, NULL);

	if (f >= r->f.min && f <= r->f.max)
	    v->f = f;

	xmlFree (value);
    }
    setlocale (LC_NUMERIC, loc);
}

static void
initStringValue (CompOptionValue       *v,
		 CompOptionRestriction *r,
		 xmlDocPtr	       doc,
		 xmlNodePtr	       node)
{
    xmlChar *value;

    v->s = strdup ("");

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	free (v->s);
	v->s = strdup ((char *) value);

	xmlFree (value);
    }
}

static void
initColorValue (CompOptionValue *v,
		xmlDocPtr       doc,
		xmlNodePtr      node)
{
    xmlNodePtr child;

    v->c[0] = 0x0000;
    v->c[1] = 0x0000;
    v->c[2] = 0x0000;
    v->c[3] = 0xffff;

    if (!doc)
	return;

    for (child = node->xmlChildrenNode; child; child = child->next)
    {
	xmlChar *value;
	int	index;

	if (!xmlStrcmp (child->name, BAD_CAST "red"))
	    index = 0;
	else if (!xmlStrcmp (child->name, BAD_CAST "green"))
	    index = 1;
	else if (!xmlStrcmp (child->name, BAD_CAST "blue"))
	    index = 2;
	else if (!xmlStrcmp (child->name, BAD_CAST "alpha"))
	    index = 3;
	else
	    continue;

	value = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
	if (value)
	{
	    int color = strtol ((char *) value, NULL , 0);

	    v->c[index] = MAX (0, MIN (0xffff, color));

	    xmlFree (value);
	}
    }
}

static void
initActionValue (CompDisplay	 *d,
		 CompOptionValue *v,
		 CompActionState state,
		 xmlDocPtr       doc,
		 xmlNodePtr      node)
{
    memset (&v->action, 0, sizeof (v->action));

    v->action.state = state;
}

static void
initKeyValue (CompDisplay     *d,
	      CompOptionValue *v,
	      CompActionState state,
	      xmlDocPtr       doc,
	      xmlNodePtr      node)
{
    xmlChar *value;

    memset (&v->action, 0, sizeof (v->action));

    v->action.state = state | CompActionStateInitKey;

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	char *binding = (char *) value;

	if (strcasecmp (binding, "disabled") && *binding)
	    stringToKeyAction (d, binding, &v->action);

	xmlFree (value);
    }

    if (state & CompActionStateAutoGrab)
    {
	CompScreen *s;

	for (s = d->screens; s; s = s->next)
	    addScreenAction (s, &v->action);
    }
}

static void
initButtonValue (CompDisplay     *d,
		 CompOptionValue *v,
		 CompActionState state,
		 xmlDocPtr       doc,
		 xmlNodePtr      node)
{
    xmlChar *value;

    memset (&v->action, 0, sizeof (v->action));

    v->action.state = state | CompActionStateInitButton |
	CompActionStateInitEdge;

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	char *binding = (char *) value;

	if (strcasecmp (binding, "disabled") && *binding)
	    stringToButtonAction (d, binding, &v->action);

	xmlFree (value);
    }

    if (state & CompActionStateAutoGrab)
    {
	CompScreen *s;

	for (s = d->screens; s; s = s->next)
	    addScreenAction (s, &v->action);
    }
}

static void
initEdgeValue (CompDisplay     *d,
	       CompOptionValue *v,
	       CompActionState state,
	       xmlDocPtr       doc,
	       xmlNodePtr      node)
{
    xmlNodePtr child;
    xmlChar    *value;

    memset (&v->action, 0, sizeof (v->action));

    v->action.state = state | CompActionStateInitEdge;

    if (!doc)
	return;

    for (child = node->xmlChildrenNode; child; child = child->next)
    {
	value = xmlGetProp (child, BAD_CAST "name");
	if (value)
	{
	    int i;

	    for (i = 0; i < SCREEN_EDGE_NUM; i++)
		if (strcasecmp ((char *) value, edgeToString (i)) == 0)
		    v->action.edgeMask |= (1 << i);

	    xmlFree (value);
	}
    }

    if (state & CompActionStateAutoGrab)
    {
	CompScreen *s;

	for (s = d->screens; s; s = s->next)
	    addScreenAction (s, &v->action);
    }
}

static void
initBellValue (CompDisplay     *d,
	       CompOptionValue *v,
	       CompActionState state,
	       xmlDocPtr       doc,
	       xmlNodePtr      node)
{
    xmlChar *value;

    memset (&v->action, 0, sizeof (v->action));

    v->action.state = state | CompActionStateInitBell;

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	if (strcasecmp ((char *) value, "true") == 0)
	    v->action.bell = TRUE;

	xmlFree (value);
    }
}

static void
initMatchValue (CompDisplay     *d,
		CompOptionValue *v,
		Bool		helper,
		xmlDocPtr       doc,
		xmlNodePtr      node)
{
    xmlChar *value;

    matchInit (&v->match);

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	matchAddFromString (&v->match, (char *) value);
	xmlFree (value);
    }

    if (!helper)
	matchUpdate (d, &v->match);
}

static void
initListValue (CompDisplay	     *d,
	       CompOptionValue	     *v,
	       CompOptionRestriction *r,
	       CompActionState	     state,
	       Bool		     helper,
	       xmlDocPtr	     doc,
	       xmlNodePtr	     node)
{
    xmlNodePtr child;

    v->list.value  = NULL;
    v->list.nValue = 0;

    if (!doc)
	return;

    for (child = node->xmlChildrenNode; child; child = child->next)
    {
	CompOptionValue *value;

	if (xmlStrcmp (child->name, BAD_CAST "value"))
	    continue;

	value = realloc (v->list.value,
			 sizeof (CompOptionValue) * (v->list.nValue + 1));
	if (value)
	{
	    switch (v->list.type) {
	    case CompOptionTypeBool:
		initBoolValue (&value[v->list.nValue], doc, child);
		break;
	    case CompOptionTypeInt:
		initIntValue (&value[v->list.nValue], r, doc, child);
		break;
	    case CompOptionTypeFloat:
		initFloatValue (&value[v->list.nValue], r, doc, child);
		break;
	    case CompOptionTypeString:
		initStringValue (&value[v->list.nValue], r, doc, child);
		break;
	    case CompOptionTypeColor:
		initColorValue (&value[v->list.nValue], doc, child);
		break;
	    case CompOptionTypeAction:
		initActionValue (d, &value[v->list.nValue], state, doc, child);
		break;
	    case CompOptionTypeKey:
		initKeyValue (d, &value[v->list.nValue], state, doc, child);
		break;
	    case CompOptionTypeButton:
		initButtonValue (d, &value[v->list.nValue], state, doc, child);
		break;
	    case CompOptionTypeEdge:
		initEdgeValue (d, &value[v->list.nValue], state, doc, child);
		break;
	    case CompOptionTypeBell:
		initBellValue (d, &value[v->list.nValue], state, doc, child);
		break;
	    case CompOptionTypeMatch:
		initMatchValue (d, &value[v->list.nValue], helper, doc, child);
	    default:
		break;
	    }

	    v->list.value = value;
	    v->list.nValue++;
	}
    }
}

static char *
stringFromMetadataPathElement (CompMetadata *metadata,
			       const char   *path,
			       const char   *element)
{
    char str[1024];

    snprintf (str, 1024, "%s/%s", path, element);

    return compGetStringFromMetadataPath (metadata, str);
}

static Bool
boolFromMetadataPathElement (CompMetadata *metadata,
			     const char   *path,
			     const char   *element,
			     Bool	  defaultValue)
{
    Bool value = FALSE;
    char *str;

    str = stringFromMetadataPathElement (metadata, path, element);
    if (!str)
	return defaultValue;

    if (strcasecmp (str, "true") == 0)
	value = TRUE;

    free (str);

    return value;
}

static void
initIntRestriction (CompMetadata	  *metadata,
		    CompOptionRestriction *r,
		    const char		  *path)
{
    char *value;

    r->i.min = MINSHORT;
    r->i.max = MAXSHORT;

    value = stringFromMetadataPathElement (metadata, path, "min");
    if (value)
    {
	r->i.min = strtol ((char *) value, NULL, 0);
	free (value);
    }

    value = stringFromMetadataPathElement (metadata, path, "max");
    if (value)
    {
	r->i.max = strtol ((char *) value, NULL, 0);
	free (value);
    }
}

static void
initFloatRestriction (CompMetadata	    *metadata,
		      CompOptionRestriction *r,
		      const char	    *path)
{
    char *value;
    char *loc;

    r->f.min	   = MINSHORT;
    r->f.max	   = MAXSHORT;
    r->f.precision = 0.1f;

    loc = setlocale (LC_NUMERIC, NULL);
    setlocale (LC_NUMERIC, "C");
    value = stringFromMetadataPathElement (metadata, path, "min");
    if (value)
    {
	r->f.min = strtod ((char *) value, NULL);
	free (value);
    }

    value = stringFromMetadataPathElement (metadata, path, "max");
    if (value)
    {
	r->f.max = strtod ((char *) value, NULL);
	free (value);
    }

    value = stringFromMetadataPathElement (metadata, path, "precision");
    if (value)
    {
	r->f.precision = strtod ((char *) value, NULL);
	free (value);
    }

    setlocale (LC_NUMERIC, loc);
}

static void
initActionState (CompMetadata    *metadata,
		 CompOptionType  type,
		 CompActionState *state,
		 const char      *path)
{
    static struct _StateMap {
	char	       *name;
	CompActionState state;
    } map[] = {
	{ "key",     CompActionStateInitKey     },
	{ "button",  CompActionStateInitButton  },
	{ "bell",    CompActionStateInitBell    },
	{ "edge",    CompActionStateInitEdge    },
	{ "edgednd", CompActionStateInitEdgeDnd }
    };
    int	      i;
    CompXPath xPath;
    char      *grab;

    *state = CompActionStateAutoGrab;

    grab = stringFromMetadataPathElement (metadata, path, "passive_grab");
    if (grab)
    {
	if (strcmp (grab, "false") == 0)
	    *state = 0;

	free (grab);
    }

    if (type == CompOptionTypeEdge)
    {
	char *noEdgeDelay;

	noEdgeDelay = stringFromMetadataPathElement (metadata, path, "nodelay");
	if (noEdgeDelay)
	{
	    if (strcmp (noEdgeDelay, "true") == 0)
		*state |= CompActionStateNoEdgeDelay;

	    free (noEdgeDelay);
	}
    }

    if (!initXPathFromMetadataPathElement (&xPath, metadata, BAD_CAST path,
					   BAD_CAST "allowed"))
	return;

    for (i = 0; i < sizeof (map) / sizeof (map[0]); i++)
    {
	xmlChar *value;

	value = xmlGetProp (*xPath.obj->nodesetval->nodeTab,
			    BAD_CAST map[i].name);
	if (value)
	{
	    if (xmlStrcmp (value, BAD_CAST "true") == 0)
		*state |= map[i].state;
	    xmlFree (value);
	}
    }

    finiXPath (&xPath);
}

static Bool
initOptionFromMetadataPath (CompDisplay   *d,
			    CompMetadata  *metadata,
			    CompOption	  *option,
			    const xmlChar *path)
{
    CompXPath	    xPath, xDefaultPath;
    xmlNodePtr	    node, defaultNode;
    xmlDocPtr	    defaultDoc;
    xmlChar	    *name, *type;
    char	    *value;
    CompActionState state = 0;
    Bool	    helper = FALSE;

    if (!initXPathFromMetadataPath (&xPath, metadata, path))
	return FALSE;

    node = *xPath.obj->nodesetval->nodeTab;

    type = xmlGetProp (node, BAD_CAST "type");
    if (type)
    {
	option->type = getOptionType ((char *) type);
	xmlFree (type);
    }
    else
    {
	option->type = CompOptionTypeBool;
    }

    name = xmlGetProp (node, BAD_CAST "name");
    option->name = strdup ((char *) name);
    xmlFree (name);

    if (initXPathFromMetadataPathElement (&xDefaultPath, metadata, path,
					  BAD_CAST "default"))
    {
	defaultDoc  = xDefaultPath.doc;
	defaultNode = *xDefaultPath.obj->nodesetval->nodeTab;
    }
    else
    {
	defaultDoc  = NULL;
	defaultNode = NULL;
    }

    switch (option->type) {
    case CompOptionTypeBool:
	initBoolValue (&option->value, defaultDoc, defaultNode);
	break;
    case CompOptionTypeInt:
	initIntRestriction (metadata, &option->rest, (char *) path);
	initIntValue (&option->value, &option->rest, defaultDoc, defaultNode);
	break;
    case CompOptionTypeFloat:
	initFloatRestriction (metadata, &option->rest, (char *) path);
	initFloatValue (&option->value, &option->rest, defaultDoc, defaultNode);
	break;
    case CompOptionTypeString:
	initStringValue (&option->value, &option->rest,
			 defaultDoc, defaultNode);
	break;
    case CompOptionTypeColor:
	initColorValue (&option->value, defaultDoc, defaultNode);
	break;
    case CompOptionTypeAction:
	initActionState (metadata, option->type, &state, (char *) path);
	initActionValue (d, &option->value, state, defaultDoc, defaultNode);
	break;
    case CompOptionTypeKey:
	initActionState (metadata, option->type, &state, (char *) path);
	initKeyValue (d, &option->value, state, defaultDoc, defaultNode);
	break;
    case CompOptionTypeButton:
	initActionState (metadata, option->type, &state, (char *) path);
	initButtonValue (d, &option->value, state, defaultDoc, defaultNode);
	break;
    case CompOptionTypeEdge:
	initActionState (metadata, option->type, &state, (char *) path);
	initEdgeValue (d, &option->value, state, defaultDoc, defaultNode);
	break;
    case CompOptionTypeBell:
	initActionState (metadata, option->type, &state, (char *) path);
	initBellValue (d, &option->value, state, defaultDoc, defaultNode);
	break;
    case CompOptionTypeMatch:
	helper = boolFromMetadataPathElement (metadata, (char *) path, "helper",
					      FALSE);
	initMatchValue (d, &option->value, helper, defaultDoc, defaultNode);
	break;
    case CompOptionTypeList:
	value = stringFromMetadataPathElement (metadata, (char *) path, "type");
	if (value)
	{
	    option->value.list.type = getOptionType ((char *) value);
	    free (value);
	}
	else
	{
	    option->value.list.type = CompOptionTypeBool;
	}

	switch (option->value.list.type) {
	case CompOptionTypeInt:
	    initIntRestriction (metadata, &option->rest, (char *) path);
	    break;
	case CompOptionTypeFloat:
	    initFloatRestriction (metadata, &option->rest, (char *) path);
	    break;
	case CompOptionTypeAction:
	case CompOptionTypeKey:
	case CompOptionTypeButton:
	case CompOptionTypeEdge:
	case CompOptionTypeBell:
	    initActionState (metadata, option->value.list.type,
			     &state, (char *) path);
	    break;
	case CompOptionTypeMatch:
	    helper = boolFromMetadataPathElement (metadata, (char *) path,
						  "helper", FALSE);
	default:
	    break;
	}

	initListValue (d, &option->value, &option->rest, state, helper,
		       defaultDoc, defaultNode);
	break;
    }

    if (defaultDoc)
	finiXPath (&xDefaultPath);

    finiXPath (&xPath);

    return TRUE;
}

Bool
compInitScreenOptionFromMetadata (CompScreen   *s,
				  CompMetadata *m,
				  CompOption   *o,
				  const char   *name)
{
    char str[1024];

    sprintf (str, "/compiz/%s/screen//option[@name=\"%s\"]", m->path, name);

    return initOptionFromMetadataPath (s->display, m, o, BAD_CAST str);
}

static void
finiScreenOptionValue (CompScreen      *s,
		       CompOptionValue *v,
		       CompOptionType  type)
{
    int	i;

    switch (type) {
    case CompOptionTypeAction:
    case CompOptionTypeKey:
    case CompOptionTypeButton:
    case CompOptionTypeEdge:
    case CompOptionTypeBell:
	if (v->action.state & CompActionStateAutoGrab)
	    removeScreenAction (s, &v->action);
	break;
    case CompOptionTypeList:
	for (i = 0; i < v->list.nValue; i++)
	    finiScreenOptionValue (s, &v->list.value[i], v->list.type);
    default:
	break;
    }
}

void
compFiniScreenOption (CompScreen *s,
		      CompOption *o)
{
    finiScreenOptionValue (s, &o->value, o->type);
    compFiniOption (o);
    free (o->name);
}

Bool
compInitScreenOptionsFromMetadata (CompScreen			*s,
				   CompMetadata			*m,
				   const CompMetadataOptionInfo *info,
				   CompOption			*opt,
				   int				n)
{
    int i;

    for (i = 0; i < n; i++)
    {
	if (!compInitScreenOptionFromMetadata (s, m, &opt[i], info[i].name))
	{
	    compFiniScreenOptions (s, opt, i);
	    return FALSE;
	}

	if (info[i].initiate)
	    opt[i].value.action.initiate = info[i].initiate;

	if (info[i].terminate)
	    opt[i].value.action.terminate = info[i].terminate;
    }

    return TRUE;
}

void
compFiniScreenOptions (CompScreen *s,
		       CompOption *opt,
		       int	  n)
{
    int i;

    for (i = 0; i < n; i++)
	compFiniScreenOption (s, &opt[i]);
}

Bool
compSetScreenOption (CompScreen      *s,
		     CompOption      *o,
		     CompOptionValue *value)
{
    if (compSetOption (o, value))
	return TRUE;

    return FALSE;
}

Bool
compInitDisplayOptionFromMetadata (CompDisplay  *d,
				   CompMetadata *m,
				   CompOption	*o,
				   const char	*name)
{
    char str[1024];

    sprintf (str, "/compiz/%s/display//option[@name=\"%s\"]", m->path, name);

    return initOptionFromMetadataPath (d, m, o, BAD_CAST str);
}

static void
finiDisplayOptionValue (CompDisplay	*d,
			CompOptionValue *v,
			CompOptionType  type)
{
    CompScreen *s;
    int	       i;

    switch (type) {
    case CompOptionTypeAction:
    case CompOptionTypeKey:
    case CompOptionTypeButton:
    case CompOptionTypeEdge:
    case CompOptionTypeBell:
	if (v->action.state & CompActionStateAutoGrab)
	    for (s = d->screens; s; s = s->next)
		removeScreenAction (s, &v->action);
	break;
    case CompOptionTypeList:
	for (i = 0; i < v->list.nValue; i++)
	    finiDisplayOptionValue (d, &v->list.value[i], v->list.type);
    default:
	break;
    }
}

void
compFiniDisplayOption (CompDisplay *d,
		       CompOption  *o)
{
    finiDisplayOptionValue (d, &o->value, o->type);
    compFiniOption (o);
    free (o->name);
}

Bool
compInitDisplayOptionsFromMetadata (CompDisplay			 *d,
				    CompMetadata		 *m,
				    const CompMetadataOptionInfo *info,
				    CompOption			 *opt,
				    int				 n)
{
    int i;

    for (i = 0; i < n; i++)
    {
	if (!compInitDisplayOptionFromMetadata (d, m, &opt[i], info[i].name))
	{
	    compFiniDisplayOptions (d, opt, i);
	    return FALSE;
	}

	if (info[i].initiate)
	    opt[i].value.action.initiate = info[i].initiate;

	if (info[i].terminate)
	    opt[i].value.action.terminate = info[i].terminate;
    }

    return TRUE;
}

void
compFiniDisplayOptions (CompDisplay *d,
			CompOption  *opt,
			int	    n)
{
    int i;

    for (i = 0; i < n; i++)
	compFiniDisplayOption (d, &opt[i]);
}

Bool
compSetDisplayOption (CompDisplay     *d,
		      CompOption      *o,
		      CompOptionValue *value)
{
    if (isActionOption (o))
    {
	if (o->value.action.state & CompActionStateAutoGrab)
	{
	    if (setDisplayAction (d, o, value))
		return TRUE;
	}
	else
	{
	    if (compSetActionOption (o, value))
		return TRUE;
	}
    }
    else
    {
	if (compSetOption (o, value))
	    return TRUE;
    }

    return FALSE;
}

char *
compGetStringFromMetadataPath (CompMetadata *metadata,
			       const char   *path)
{
    CompXPath xPath;
    char      *v = NULL;

    if (!initXPathFromMetadataPath (&xPath, metadata, BAD_CAST path))
	return NULL;

    xPath.obj = xmlXPathConvertString (xPath.obj);

    if (xPath.obj->type == XPATH_STRING && xPath.obj->stringval)
	v = strdup ((char *) xPath.obj->stringval);

    finiXPath (&xPath);

    return v;
}

char *
compGetShortPluginDescription (CompMetadata *m)
{
    char str[1024];

    sprintf (str, "/compiz/%s/short/child::text()", m->path);

    return compGetStringFromMetadataPath (m, str);
}

char *
compGetLongPluginDescription (CompMetadata *m)
{
    char str[1024];

    sprintf (str, "/compiz/%s/long/child::text()", m->path);

    return compGetStringFromMetadataPath (m, str);
}

char *
compGetShortScreenOptionDescription (CompMetadata *m,
				     CompOption   *o)
{
    char str[1024];

    sprintf (str, "/compiz/%s/screen//option[@name=\"%s\"]/short/child::text()",
	     m->path, o->name);

    return compGetStringFromMetadataPath (m, str);
}

char *
compGetLongScreenOptionDescription (CompMetadata *m,
				    CompOption   *o)
{
    char str[1024];

    sprintf (str, "/compiz/%s/screen//option[@name=\"%s\"]/long/child::text()",
	     m->path, o->name);

    return compGetStringFromMetadataPath (m, str);
}


char *
compGetShortDisplayOptionDescription (CompMetadata *m,
				      CompOption   *o)
{
    char str[1024];

    sprintf (str,
	     "/compiz/%s/display//option[@name=\"%s\"]/short/child::text()",
	     m->path, o->name);

    return compGetStringFromMetadataPath (m, str);
}


char *
compGetLongDisplayOptionDescription (CompMetadata *m,
				     CompOption   *o)
{
    char str[1024];

    sprintf (str, "/compiz/%s/display//option[@name=\"%s\"]/long/child::text()",
	     m->path, o->name);

    return compGetStringFromMetadataPath (m, str);
}

int
compReadXmlChunk (const char *src,
		  int	     *offset,
		  char	     *buffer,
		  int	     length)
{
    int srcLength = strlen (src);
    int srcOffset = *offset;

    if (srcOffset > srcLength)
	srcOffset = srcLength;

    *offset -= srcOffset;

    src += srcOffset;
    srcLength -= srcOffset;

    if (srcLength > 0 && length > 0)
    {
	if (srcLength < length)
	    length = srcLength;

	memcpy (buffer, src, length);

	return length;
    }

    return 0;
}

int
compReadXmlChunkFromMetadataOptionInfo (const CompMetadataOptionInfo *info,
					int			     *offset,
					char			     *buffer,
					int			     length)
{
    int i;

    i = compReadXmlChunk ("<option name=\"", offset, buffer, length);
    i += compReadXmlChunk (info->name, offset, buffer + i, length - i);

    if (info->type)
    {
	i += compReadXmlChunk ("\" type=\"", offset, buffer + i, length - i);
	i += compReadXmlChunk (info->type, offset, buffer + i, length - i);
    }

    if (info->data)
    {
	i += compReadXmlChunk ("\">", offset, buffer + i, length - i);
	i += compReadXmlChunk (info->data, offset, buffer + i, length - i);
	i += compReadXmlChunk ("</option>", offset, buffer + i, length - i);
    }
    else
    {
	i += compReadXmlChunk ("\"/>", offset, buffer + i, length - i);
    }

    return i;
}
