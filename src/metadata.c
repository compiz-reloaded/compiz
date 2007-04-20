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

#include <compiz.h>

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
	fprintf (stderr,
		 "%s: Unable to parse XML metadata from file \"%s%s\"\n",
		 programName, file, EXTENSION);

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
	fprintf (stderr, "%s: Unable to parse XML metadata\n", programName);

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
	fprintf (stderr, "%s: Unable to parse XML metadata\n", programName);

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

    v->f = (r->f.min + r->f.max) / 2;

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	float f = strtod ((char *) value, NULL);

	if (f >= r->f.min && f <= r->f.max)
	    v->f = f;

	xmlFree (value);
    }
}

static void
initStringValue (CompOptionValue       *v,
		 CompOptionRestriction *r,
		 xmlDocPtr	       doc,
		 xmlNodePtr	       node)
{
    xmlChar *value;

    if (r->s.nString)
	v->s = strdup (r->s.string[0]);
    else
	v->s = strdup ("");

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	if (r->s.nString)
	{
	    int i;

	    for (i = 0; i < r->s.nString; i++)
	    {
		if (strcmp (r->s.string[i], (char *) value) == 0)
		{
		    free (v->s);
		    v->s = strdup (r->s.string[i]);
		    break;
		}
	    }
	}
	else
	{
	    free (v->s);
	    v->s = strdup ((char *) value);
	}

	xmlFree (value);
    }
}

static void
initColorValue (CompOptionValue *v,
		xmlDocPtr       doc,
		xmlNodePtr      node)
{
    xmlNodePtr child;

    memset (v->c, 0, sizeof (v->c));

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
    xmlNodePtr child;
    xmlChar    *value;

    memset (&v->action, 0, sizeof (v->action));

    v->action.state = state;

    if (!doc)
	return;

    for (child = node->xmlChildrenNode; child; child = child->next)
    {
	if (!xmlStrcmp (child->name, BAD_CAST "key"))
	{
	    value = xmlNodeListGetString (child->doc,
					  child->xmlChildrenNode, 1);
	    if (value)
	    {
		char *binding = (char *) value;

		if (strcasecmp (binding, "disabled") && *binding)
		{
		    v->action.type |= CompBindingTypeKey;
		    if (!stringToKeyBinding (d, binding, &v->action.key))
			v->action.type &= ~CompBindingTypeKey;
		}

		xmlFree (value);
	    }
	}
	else if (!xmlStrcmp (child->name, BAD_CAST "button"))
	{
	    value = xmlNodeListGetString (child->doc,
					  child->xmlChildrenNode, 1);
	    if (value)
	    {
		char *binding = (char *) value;

		if (strcasecmp (binding, "disabled") && *binding)
		{
		    v->action.type |= CompBindingTypeButton;
		    if (!stringToButtonBinding (d, binding, &v->action.button))
			v->action.type &= ~CompBindingTypeButton;
		}

		xmlFree (value);
	    }
	}
	else if (!xmlStrcmp (child->name, BAD_CAST "edges"))
	{
	    static char *edge[] = {
		"left",
		"right",
		"top",
		"bottom",
		"top_left",
		"top_right",
		"bottom_left",
		"bottom_right"
	    };
	    int i;

	    for (i = 0; i < sizeof (edge) / sizeof (edge[0]); i++)
	    {
		value = xmlGetProp (child, BAD_CAST edge);
		if (value)
		{
		    if (strcasecmp ((char *) value, "true") == 0)
			v->action.edgeMask |= (1 << i);

		    xmlFree (value);
		}
	    }

	    value = xmlGetProp (child, BAD_CAST "button");
	    if (value)
	    {
		v->action.edgeButton = strtol ((char *) value, NULL, 0);
		xmlFree (value);
	    }
	}
	else if (!xmlStrcmp (child->name, (const xmlChar *) "bell"))
	{
	    value = xmlNodeListGetString (child->doc,
					  child->xmlChildrenNode, 1);
	    if (value)
	    {
		v->action.bell = !strcasecmp ((char *) value, "true");
		xmlFree (value);
	    }
	}
    }
}

static void
initMatchValue (CompDisplay     *d,
		CompOptionValue *v,
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
    matchUpdate (d, &v->match);
}

static void
initListValue (CompDisplay	     *d,
	       CompOptionValue	     *v,
	       CompOptionRestriction *r,
	       CompActionState	     state,
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
	    case CompOptionTypeMatch:
		initMatchValue (d, &value[v->list.nValue], doc, child);
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

    r->f.min	   = MINSHORT;
    r->f.max	   = MAXSHORT;
    r->f.precision = 0.1f;

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
}

static void
initStringRestriction (CompMetadata	     *metadata,
		       CompOptionRestriction *r,
		       const char	     *path)
{
    CompXPath  xPath;
    xmlNodePtr node, child;

    r->s.string  = NULL;
    r->s.nString = 0;

    if (!initXPathFromMetadataPathElement (&xPath, metadata, BAD_CAST path,
					   BAD_CAST "allowed"))
	return;

    node = *xPath.obj->nodesetval->nodeTab;

    for (child = node->xmlChildrenNode; child; child = child->next)
    {
	xmlChar *value;

	if (xmlStrcmp (child->name, BAD_CAST "value"))
	    continue;

	value = xmlNodeListGetString (child->doc, child->xmlChildrenNode, 1);
	if (value)
	{
	    char **string;

	    string = realloc (r->s.string, sizeof (char *) *
			      (r->s.nString + 1));
	    if (string)
	    {
		string[r->s.nString] = strdup ((char *) value);

		r->s.string = string;
		r->s.nString++;
	    }
	}
    }

    finiXPath (&xPath);
}

static void
initActionState (CompMetadata    *metadata,
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

    *state = 0;

    if (!initXPathFromMetadataPathElement (&xPath, metadata, BAD_CAST path,
					   BAD_CAST "allowed"))
	return;

    for (i = 0; i < sizeof (map) / sizeof (map[0]); i++)
    {
	xmlChar *value;

	value = xmlGetProp (*xPath.obj->nodesetval->nodeTab,
			    BAD_CAST map[i].name);
	if (value && xmlStrcmp (value, BAD_CAST "true") == 0)
	    *state |= map[i].state;
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

    value = stringFromMetadataPathElement (metadata, (char *) path, "short");
    if (value)
	option->shortDesc = value;
    else
	option->shortDesc = strdup ("");

    value = stringFromMetadataPathElement (metadata, (char *) path, "long");
    if (value)
	option->longDesc = value;
    else
	option->longDesc = strdup ("");

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
	initStringRestriction (metadata, &option->rest, (char *) path);
	initStringValue (&option->value, &option->rest,
			 defaultDoc, defaultNode);
	break;
    case CompOptionTypeColor:
	initColorValue (&option->value, defaultDoc, defaultNode);
	break;
    case CompOptionTypeAction:
	initActionState (metadata, &state, (char *) path);
	initActionValue (d, &option->value, state, defaultDoc, defaultNode);
	break;
    case CompOptionTypeMatch:
	initMatchValue (d, &option->value, defaultDoc, defaultNode);
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
	case CompOptionTypeString:
	    initStringRestriction (metadata, &option->rest, (char *) path);
	    break;
	case CompOptionTypeAction:
	    initActionState (metadata, &state, (char *) path);
	default:
	    break;
	}

	initListValue (d, &option->value, &option->rest, state,
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

void
compFiniScreenOption (CompScreen *s,
		      CompOption *o)
{
    compFiniOption (o);
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
compInitDisplayOptionFromMetadata (CompDisplay  *d,
				   CompMetadata *m,
				   CompOption	*o,
				   const char	*name)
{
    char str[1024];

    sprintf (str, "/compiz/%s/display//option[@name=\"%s\"]", m->path, name);

    return initOptionFromMetadataPath (d, m, o, BAD_CAST str);
}

void
compFiniDisplayOption (CompDisplay *d,
		       CompOption  *o)
{
    compFiniOption (o);
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