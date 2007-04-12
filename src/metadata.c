/*
 * Copyright © 2007 Dennis Kasprzyk
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
 * Author: Dennis Kasprzyk <onestone@deltatauchi.de>
 *
 */

#include <string.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <compiz.h>

#define HOME_METADATADIR ".compiz/metadata"
#define EXTENSION ".metadata"

struct _XmlMod {
    char *name;
    int  modifier;
} xmlMods[] = {
    { "Shift",      ShiftMask	       },
    { "Control",    ControlMask	       },
    { "Mod1",       Mod1Mask	       },
    { "Mod2",       Mod2Mask	       },
    { "Mod3",       Mod3Mask	       },
    { "Mod4",       Mod4Mask	       },
    { "Mod5",       Mod5Mask	       },
    { "Alt",	    CompAltMask        },
    { "Meta",	    CompMetaMask       },
    { "Super",      CompSuperMask      },
    { "Hyper",      CompHyperMask      },
    { "ModeSwitch", CompModeSwitchMask },
};

#define N_XMLMODS (sizeof (xmlMods) / sizeof (struct _XmlMod))

Bool
compInitMetadata (CompMetadata *metadata)
{
    metadata->path = strdup ("general");
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

Bool
compAddMetadataFromFile (CompMetadata *metadata,
			 const char   *file)
{
    xmlDoc **d, *doc = NULL;
    char   *home;

    home = getenv ("HOME");
    if (home)
    {
	char *path;

	path = malloc (strlen (home) + strlen (HOME_METADATADIR) + 2);
	if (path)
	{
	    sprintf (path, "%s/%s", home, HOME_METADATADIR);
	    doc = readXmlFile (path, file);
	    free (path);
	}
    }

    if (!doc)
    {
	doc = readXmlFile (METADATADIR, file);
	if (!doc)
	{
	    fprintf (stderr,
		     "%s: Unable to parse XML metadata from file \"%s%s\"\n",
		     programName, file, EXTENSION);

	    return FALSE;
	}
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
initIntValue (CompOptionValue *v,
	      xmlDocPtr       doc,
	      xmlNodePtr      node)
{
    xmlChar *value;

    v->i = 0;

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	v->i = strtol ((char *) value, NULL, 0);
	xmlFree (value);
    }
}

static void
initFloatValue (CompOptionValue *v,
		xmlDocPtr       doc,
		xmlNodePtr      node)
{
    xmlChar *value;

    v->f = 0.0f;

    if (!doc)
	return;

    value = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (value)
    {
	v->f = strtod ((char *) value, NULL);
	xmlFree (value);
    }
}

static void
initStringValue (CompOptionValue *v,
		 xmlDocPtr       doc,
		 xmlNodePtr      node)
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
convertKeyBinding (CompDisplay *d, CompKeyBinding *kb, char *bind)
{
    char *tok = strtok (bind, "+");
    int i;
    Bool changed;

    while (tok)
    {
	changed = FALSE;
	for (i = 0; i < N_XMLMODS && !changed; i++)
	    if (!strcmp (tok, xmlMods[i].name))
	    {
		kb->modifiers |= xmlMods[i].modifier;
		changed = TRUE;
	    }
	if (!changed)
	{
	    kb->keycode = XKeysymToKeycode (d->display, XStringToKeysym (tok));
	    break;
	}
	tok = strtok(NULL,"+");
    }
}

static void
convertButtonBinding (CompButtonBinding *bb, char *bind)
{
    char *tok = strtok (bind, "+");
    int i;
    Bool changed;

    while (tok)
    {
	changed = FALSE;
	for (i = 0; i < N_XMLMODS && !changed; i++)
	    if (!strcmp (tok, xmlMods[i].name))
	    {
		bb->modifiers |= xmlMods[i].modifier;
		changed = TRUE;
	    }
	    
	if (!changed && strncmp(tok, "Button", strlen("Button")) == 0)
	{
	    int but;
	    if (sscanf (tok, "Button%d", &but) == 1)
	    {
		bb->button = but;
		break;
	    }
	}
	tok = strtok(NULL,"+");
    }
}

static void
initActionValue (CompDisplay	 *d,
		 CompOptionValue *v,
		 xmlDocPtr       doc,
		 xmlNodePtr      node)
{
    xmlNodePtr child;

    memset (&v->action, 0, sizeof (v->action));

    if (!doc)
	return;

    for (child = node->xmlChildrenNode; child; child = child->next)
    {
	if (!xmlStrcmp (child->name, BAD_CAST "key"))
	{
	    xmlChar *state;
	    xmlChar *key = xmlNodeListGetString (child->doc,
		                                 child->xmlChildrenNode, 1);

	    if (key && xmlStrlen (key))
	    {
		v->action.type |= CompBindingTypeKey;
		convertKeyBinding (d, &v->action.key, (char *) key);
	    }
	    xmlFree(key);
	    
	    state = xmlGetProp (child, BAD_CAST "state");

	    if (!state)
	    {
		v->action.state |= CompActionStateInitKey;
	    }
	    else
	    {
		char *tok = strtok ((char *) state, ",");
		while (tok)
		{
		    if (!strcasecmp (tok, "init"))
			v->action.state |= CompActionStateInitKey;
		    else if (!strcasecmp (tok, "term"))
			v->action.state |= CompActionStateTermKey;
		    tok = strtok (NULL, ",");
		}
	    }
	    xmlFree(state);
	}
	else if (!xmlStrcmp (child->name, BAD_CAST "button"))
	{
	    xmlChar *state;
	    xmlChar *button = xmlNodeListGetString (child->doc,
		                                    child->xmlChildrenNode, 1);
	    if (button && xmlStrlen (button))
	    {
                v->action.type |= CompBindingTypeButton;
		convertButtonBinding (&v->action.button, (char *) button);
	    }
	    xmlFree(button);
     
	    state = xmlGetProp (child, BAD_CAST "state");
	    if (!state)
	    {
		v->action.state |= CompActionStateInitButton;
	    }
	    else
	    {
		char *tok = strtok((char *)state,",");
		while (tok)
		{
		    if (!strcasecmp(tok,"init"))
                        v->action.state |= CompActionStateInitButton;
		    else if (!strcasecmp(tok,"term"))
                        v->action.state |= CompActionStateTermButton;
		    tok = strtok(NULL,",");
		}
	    }
	    xmlFree(state);
	}
	else if (!xmlStrcmp (child->name, BAD_CAST "edge"))
	{
	    xmlChar *state;
	    xmlChar *button;
	    xmlChar *edge = xmlNodeListGetString (child->doc,
		                                  child->xmlChildrenNode, 1);
	    char *tok = strtok ((char *) edge, ",");
	    
	    while (tok && edge)
	    {
		if (!strcasecmp(tok,"left"))
			v->action.edgeMask |= (1 << SCREEN_EDGE_LEFT);
		else if (!strcasecmp(tok,"right"))
			v->action.edgeMask |= (1 << SCREEN_EDGE_RIGHT);
		else if (!strcasecmp(tok,"top"))
			v->action.edgeMask |= (1 << SCREEN_EDGE_TOP);
		else if (!strcasecmp(tok,"bottom"))
			v->action.edgeMask |= (1 << SCREEN_EDGE_BOTTOM);
		else if (!strcasecmp(tok,"topleft"))
			v->action.edgeMask |= (1 << SCREEN_EDGE_TOPLEFT);
		else if (!strcasecmp(tok,"topright"))
			v->action.edgeMask |= (1 << SCREEN_EDGE_TOPRIGHT);
		else if (!strcasecmp(tok,"bottomleft"))
			v->action.edgeMask |= (1 << SCREEN_EDGE_BOTTOMLEFT);
		else if (!strcasecmp(tok,"bottomright"))
			v->action.edgeMask |= (1 << SCREEN_EDGE_BOTTOMRIGHT);
		tok = strtok(NULL,",");
	    }
	    xmlFree(edge);
	   
	    state = xmlGetProp (child, BAD_CAST "state");
	    if (!state)
	    {
		v->action.state |= CompActionStateInitEdge;
	    }
	    else
	    {
		char *tok = strtok ((char *) state, ",");
		while (tok)
		{
		    if (!strcasecmp(tok,"init"))
			v->action.state |= CompActionStateInitEdge;
		    else if (!strcasecmp(tok,"term"))
			v->action.state |= CompActionStateTermEdge;
		    else if (!strcasecmp(tok,"initdnd"))
			v->action.state |= CompActionStateInitEdgeDnd;
		    else if (!strcasecmp(tok,"termdnd"))
			v->action.state |= CompActionStateTermEdgeDnd;
		    tok = strtok(NULL,",");
		}
	    }
	    xmlFree(state);

	    button = xmlGetProp (child, BAD_CAST "button");
            if (button)
	    {
                v->action.edgeButton = strtol ((char *) button, NULL, 0);
	    }
	    xmlFree(button);
	}
	else if (!xmlStrcmp(child->name, (const xmlChar *) "bell"))
	{
	    xmlChar *bell = xmlNodeListGetString (child->doc,
		                                  child->xmlChildrenNode, 1);
	    if (bell)
		v->action.bell = (xmlStrcasecmp (bell, BAD_CAST "true") == 0);
	    xmlFree(bell);
	    
	    v->action.state |= CompActionStateInitBell;
	}
    }
}

static void
initMatchValue (CompOptionValue *v,
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
}

static void
initListValue (CompDisplay     *d,
	       CompOptionValue *v,
	       xmlDocPtr       doc,
	       xmlNodePtr      node)
{
    xmlNodePtr child;

    v->list.value  = NULL;
    v->list.nValue = 0;

    if (!doc)
	return;

    for (child = node->xmlChildrenNode; child; child = child->next)
    {
	CompOptionValue *value;

	if (xmlStrcmp (child->name, BAD_CAST "element"))
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
		initIntValue (&value[v->list.nValue], doc, child);
		break;
	    case CompOptionTypeFloat:
		initFloatValue (&value[v->list.nValue], doc, child);
		break;
	    case CompOptionTypeString:
		initStringValue (&value[v->list.nValue], doc, child);
		break;
	    case CompOptionTypeColor:
		initColorValue (&value[v->list.nValue], doc, child);
		break;
	    case CompOptionTypeAction:
		initActionValue (d, &value[v->list.nValue], doc, child);
		break;
	    case CompOptionTypeMatch:
		initMatchValue (&value[v->list.nValue], doc, child);
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
initStringRestriction (CompOptionRestriction *r, xmlNodePtr node)
{
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;
    int num;
    int i;
    
    xpathCtx = xmlXPathNewContext (node->doc);
    xpathCtx->node = node;
    xpathObj = xmlXPathEvalExpression (BAD_CAST "allowed/string", xpathCtx);

    r->s.string = NULL;
    r->s.nString = 0;
    
    if (!xpathObj)
	return;

    num = (xpathObj->nodesetval)? xpathObj->nodesetval->nodeNr : 0;
    if (num)
    {
	r->s.string = malloc (sizeof (char*) * num);
	r->s.nString = num;
	for (i = 0; i < num; i++)
	{
	    xmlChar *value;
	    xmlNodePtr cur = xpathObj->nodesetval->nodeTab[i];

	    value = xmlNodeListGetString (cur->doc,
					  cur->xmlChildrenNode, 1);
	    r->s.string[i] = (value)? strdup ((char *) value) : strdup ("");
	}
    }
}

static Bool
initOptionFromMetadataPath (CompDisplay   *d,
			    CompMetadata  *metadata,
			    CompOption	  *option,
			    const xmlChar *path)
{
    CompXPath  xPath, xDefaultPath;
    xmlNodePtr node, defaultNode;
    xmlDocPtr  defaultDoc;
    xmlChar    *name, *type;
    char       *value;

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
	initIntValue (&option->value, defaultDoc, defaultNode);
	initIntRestriction (metadata, &option->rest, (char *) path);
	break;
    case CompOptionTypeFloat:
	initFloatValue (&option->value, defaultDoc, defaultNode);
	initFloatRestriction (metadata, &option->rest, (char *) path);
	break;
    case CompOptionTypeString:
	initStringValue (&option->value, defaultDoc, defaultNode);
	initStringRestriction (&option->rest, node);
	break;
    case CompOptionTypeColor:
	initColorValue (&option->value, defaultDoc, defaultNode);
	break;
    case CompOptionTypeAction:
	initActionValue (d, &option->value, defaultDoc, defaultNode);
	break;
    case CompOptionTypeMatch:
	initMatchValue (&option->value, defaultDoc, defaultNode);
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

	initListValue (d, &option->value, defaultDoc, defaultNode);

	switch (option->value.list.type) {
	case CompOptionTypeInt:
	    initIntRestriction (metadata, &option->rest, (char *) path);
	    break;
	case CompOptionTypeFloat:
	    initFloatRestriction (metadata, &option->rest, (char *) path);
	    break;
	case CompOptionTypeString:
	    initStringRestriction (&option->rest, node);
	default:
	    break;
	}
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
				  CompOption *o,
				  const char *name)
{
    char str[1024];

    sprintf (str, "/compiz/%s/screen//option[@name=\"%s\"]", m->path, name);

    return initOptionFromMetadataPath (s->display, m, o, BAD_CAST str);
}

Bool
compInitDisplayOptionFromMetadata (CompDisplay  *d,
				   CompMetadata *m,
				   CompOption *o,
				   const char *name)
{
    char str[1024];

    sprintf (str, "/compiz/%s/display//option[@name=\"%s\"]", m->path, name);

    return initOptionFromMetadataPath (d, m, o, BAD_CAST str);
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
