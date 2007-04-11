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

		    return TRUE;
		}

		xmlXPathFreeObject (obj);
	    }

	    xmlXPathFreeContext (ctx);
	}
    }

    return FALSE;
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
initBoolValue (CompOptionValue *v, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    v->b = FALSE;
    
    if (!xmlStrcmp (cur->name, BAD_CAST "element") ||
	!xmlStrcmp (cur->name, BAD_CAST "bool") ||
	!xmlStrcmp (cur->name, BAD_CAST "default"))
    {
	xmlChar *value = xmlNodeListGetString (node->doc,
					       node->xmlChildrenNode, 1);
	if (!value)
	    return;
	v->b = (!strcasecmp ((char *) value, "true")) ? TRUE : FALSE;
	xmlFree (value);
	return;
    }
    
    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "default") ||
            !xmlStrcmp (cur->name, BAD_CAST "bool"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    if (!value)
		continue;
	    v->b = (!strcasecmp ((char *) value, "true")) ? TRUE : FALSE;
	    xmlFree (value);
	    return;
	}
	cur = cur->next;
    }
}

static void
initIntValue (CompOptionValue *v, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    v->i = 0;
    
    if (!xmlStrcmp (cur->name, BAD_CAST "element") ||
	!xmlStrcmp (cur->name, BAD_CAST "int") ||
	!xmlStrcmp (cur->name, BAD_CAST "default"))
    {
	xmlChar *value = xmlNodeListGetString (node->doc,
					       node->xmlChildrenNode, 1);
	if (!value)
	    return;
	v->i = strtol ((char *) value, NULL, 0);
	xmlFree (value);
	return;
    }
    
    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "default") ||
            !xmlStrcmp (cur->name, BAD_CAST "int"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    if (!value)
		continue;
	    v->i = strtol ((char *) value, NULL, 0);
	    xmlFree (value);
	    return;
	}
	cur = cur->next;
    }
}

static void
initFloatValue (CompOptionValue *v, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    v->f = 0;
    
    if (!xmlStrcmp (cur->name, BAD_CAST "element") ||
	!xmlStrcmp (cur->name, BAD_CAST "float") ||
	!xmlStrcmp (cur->name, BAD_CAST "default"))
    {
	xmlChar *value = xmlNodeListGetString (node->doc,
					       node->xmlChildrenNode, 1);
	if (!value)
	    return;
	v->f = strtod ((char *) value, NULL);
	xmlFree (value);
	return;
    }
    
    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "default") ||
            !xmlStrcmp (cur->name, BAD_CAST "float"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    if (!value)
		continue;
	    v->f = strtod ((char *) value, NULL);
	    xmlFree (value);
	    return;
	}
	cur = cur->next;
    }
}

static void
initStringValue (CompOptionValue *v, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    v->s = strdup("");
    
    if (!xmlStrcmp (cur->name, BAD_CAST "element") ||
	!xmlStrcmp (cur->name, BAD_CAST "string") ||
	!xmlStrcmp (cur->name, BAD_CAST "default"))
    {
	xmlChar *value = xmlNodeListGetString (node->doc,
					       node->xmlChildrenNode, 1);
	if (!value)
	    return;
	free (v->s);
	v->s = strdup ((char *) value);
	xmlFree (value);
	return;
    }
    
    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "default") ||
            !xmlStrcmp (cur->name, BAD_CAST "string"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    if (!value)
	    {
		cur = cur->next;
		continue;
	    }
	    free (v->s);
	    v->s = strdup ((char *) value);
	    xmlFree (value);
	    return;
	}
	cur = cur->next;
    }
}

static void
initColorValue (CompOptionValue *v, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    v->c[0] = 0;
    v->c[1] = 0;
    v->c[2] = 0;
    v->c[3] = 0;
    
    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "default") ||
	    !xmlStrcmp (cur->name, BAD_CAST "element") ||
            !xmlStrcmp (cur->name, BAD_CAST "color"))
	{
	    initColorValue (v, cur->xmlChildrenNode);
	    return;
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "red"))
	{
	    xmlChar *value = xmlNodeListGetString(cur->doc,
						  cur->xmlChildrenNode, 1);
	    v->c[0] = MAX (0, MIN (0xffff, strtol ((char *) value, NULL , 0)));
	    xmlFree (value);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "green"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    v->c[1] = MAX (0, MIN (0xffff, strtol ((char *) value, NULL , 0)));
	    xmlFree (value);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "blue"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    v->c[2] = MAX (0, MIN (0xffff, strtol ((char *) value, NULL , 0)));
	    xmlFree (value);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "alpha"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    v->c[3] = MAX (0, MIN (0xffff, strtol ((char *) value, NULL , 0)));
	    xmlFree (value);
	}
	cur = cur->next;
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
initActionValue (CompDisplay *d, CompOptionValue *v, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    v->action.initiate         = NULL;
    v->action.terminate        = NULL;
    v->action.state            = 0;
    v->action.type             = 0;
    v->action.key.keycode      = 0;
    v->action.key.modifiers    = 0;
    v->action.button.button    = 0;
    v->action.button.modifiers = 0;
    v->action.bell             = FALSE;
    v->action.edgeMask         = 0;
    v->action.edgeButton       = 0;

    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "default") ||
	    !xmlStrcmp (cur->name, BAD_CAST "element") ||
            !xmlStrcmp (cur->name, BAD_CAST "action"))
	{
	    initActionValue (d, v, cur->xmlChildrenNode);
	    return;
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "key"))
	{
	    xmlChar *state;
	    xmlChar *key = xmlNodeListGetString (cur->doc,
		                                 cur->xmlChildrenNode, 1);

	    if (key && xmlStrlen (key))
	    {
		v->action.type |= CompBindingTypeKey;
		convertKeyBinding (d, &v->action.key, (char *) key);
	    }
	    xmlFree(key);
	    
	    state = xmlGetProp(cur, BAD_CAST "state");

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
	else if (!xmlStrcmp (cur->name, BAD_CAST "button"))
	{
	    xmlChar *state;
	    xmlChar *button = xmlNodeListGetString (cur->doc,
		                                    cur->xmlChildrenNode, 1);
	    if (button && xmlStrlen (button))
	    {
                v->action.type |= CompBindingTypeButton;
		convertButtonBinding (&v->action.button, (char *) button);
	    }
	    xmlFree(button);
     
	    state = xmlGetProp(cur, (xmlChar *)"state");
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
	else if (!xmlStrcmp (cur->name, BAD_CAST "edge"))
	{
	    xmlChar *state;
	    xmlChar *button;
	    xmlChar *edge = xmlNodeListGetString (cur->doc,
		                                  cur->xmlChildrenNode, 1);
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
	   
	    state = xmlGetProp (cur, BAD_CAST "state");
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

	    button = xmlGetProp (cur, BAD_CAST "button");
            if (button)
	    {
                v->action.edgeButton = strtol ((char *) button, NULL, 0);
	    }
	    xmlFree(button);
	}
	else if (!xmlStrcmp(cur->name, (const xmlChar *) "bell"))
	{
	    xmlChar *bell = xmlNodeListGetString (cur->doc,
		                                  cur->xmlChildrenNode, 1);
	    if (bell)
		v->action.bell = (xmlStrcasecmp (bell, BAD_CAST "true") == 0);
	    xmlFree(bell);
	    
	    v->action.state |= CompActionStateInitBell;
	}
	cur = cur->next;
    }
}

static void
initMatchValue (CompOptionValue *v, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    matchInit (&v->match);
    
    if (!xmlStrcmp (cur->name, BAD_CAST "element") ||
	!xmlStrcmp (cur->name, BAD_CAST "match") ||
	!xmlStrcmp (cur->name, BAD_CAST "default"))
    {
	xmlChar *value = xmlNodeListGetString (node->doc,
					       node->xmlChildrenNode, 1);
	if (!value)
	    return;
	matchAddFromString (&v->match, (char *) value);
	xmlFree (value);
	return;
    }
    
    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "default") ||
            !xmlStrcmp (cur->name, BAD_CAST "match"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    if (!value)
	    {
		cur = cur->next;
		continue;
	    }
            matchAddFromString (&v->match, (char *) value);
	    xmlFree (value);
	    return;
	}
	cur = cur->next;
    }
}

static void
initListValue (CompDisplay *d, CompOptionValue *v, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    v->list.type = CompOptionTypeBool;
    v->list.value = NULL;
    v->list.nValue = 0;
    
    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "default") ||
            !xmlStrcmp (cur->name, BAD_CAST "list"))
	{
	    CompOptionType oType;
	    xmlChar *type = xmlGetProp (cur, BAD_CAST "type");
	    xmlXPathObjectPtr xpathObj;
            xmlXPathContextPtr xpathCtx;
	    int i;
	    int num;

	    if (!type || !xmlStrlen(type))
	    {
       		fprintf (stderr, "%s: No list type defined in metadata\n",
		         programName);
		if (type)
		   xmlFree (type);
		return;
	    }

	    oType = getOptionType ((char *) type);
	    xmlFree (type);

	    if (oType == CompOptionTypeList)
	    {
        	fprintf (stderr, "%s: Not supported list type \"list\"\n",
		         programName);
		return;
	    }

	    v->list.type = oType;

	    xpathCtx = xmlXPathNewContext (cur->doc);
            xpathCtx->node = cur;
            xpathObj = xmlXPathEvalExpression (BAD_CAST "*", xpathCtx);

	    if (!xpathObj)
		return;

	    num = (xpathObj->nodesetval)? xpathObj->nodesetval->nodeNr : 0;
	    if (num)
	    {
		v->list.value = malloc (sizeof (CompOptionValue) * num);
		v->list.nValue = num;
		for (i = 0; i < num; i++)
		{
		    xmlNodePtr cur = xpathObj->nodesetval->nodeTab[i];

		    switch (oType)
		    {
		    case CompOptionTypeBool:
			initBoolValue (&v->list.value[i], cur);
			break;
		    case CompOptionTypeInt:
			initIntValue (&v->list.value[i], cur);
			break;
		    case CompOptionTypeFloat:
			initFloatValue (&v->list.value[i], cur);
			break;
		    case CompOptionTypeString:
			initStringValue (&v->list.value[i], cur);
			break;
		    case CompOptionTypeColor:
			initColorValue (&v->list.value[i], cur);
			break;
		    case CompOptionTypeAction:
			initActionValue (d, &v->list.value[i], cur);
			break;
		    case CompOptionTypeMatch:
			initMatchValue (&v->list.value[i], cur);
			break;
		    default:
			break;
		    }
		}
	    }
	    return;
	}
	cur = cur->next;
    }
}

static void
initIntRestriction (CompOptionRestriction *r, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    r->i.min = 0;
    r->i.max = 100;
    
    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "min"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode,
						  1);
	    r->i.min = strtol ((char *) value, NULL, 0);
	    xmlFree (value);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "max"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode,
						  1);
	    r->i.max = strtol ((char *) value, NULL, 0);
	    xmlFree (value);
	}
	cur = cur->next;
    }
}

static void
initFloatRestriction (CompOptionRestriction *r, xmlNodePtr node)
{
    xmlNode *cur = node->xmlChildrenNode;

    r->f.min = 0.0;
    r->f.max = 100.0;
    r->f.min = 0.1;
    
    while (cur != NULL)
    {
	if (!xmlStrcmp (cur->name, BAD_CAST "min"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode,
						  1);
	    r->f.min = strtod ((char *) value, NULL);
	    xmlFree (value);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "max"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode,
						  1);
	    r->f.max = strtod ((char *) value, NULL);
	    xmlFree (value);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "precision"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode,
						  1);
	    r->f.precision = strtod ((char *) value, NULL);
	    xmlFree (value);
	}
	cur = cur->next;
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
    CompXPath xPath;
    xmlNode   *node;
    xmlChar   *name, *type;

    if (!initXPathFromMetadataPath (&xPath, metadata, BAD_CAST path))
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

    switch (option->type) {
    case CompOptionTypeBool:
	initBoolValue (&option->value, node);
	break;
    case CompOptionTypeInt:
	initIntValue (&option->value, node);
	initIntRestriction (&option->rest, node);
	break;
    case CompOptionTypeFloat:
	initFloatValue (&option->value, node);
	initFloatRestriction (&option->rest, node);
	break;
    case CompOptionTypeString:
	initStringValue (&option->value, node);
	initStringRestriction (&option->rest, node);
	break;
    case CompOptionTypeColor:
	initColorValue (&option->value, node);
	break;
    case CompOptionTypeAction:
	initActionValue (d, &option->value, node);
	break;
    case CompOptionTypeMatch:
	initMatchValue (&option->value, node);
	break;
    case CompOptionTypeList:
	initListValue (d, &option->value, node);
	switch (option->value.list.type) {
	case CompOptionTypeInt:
	    initIntRestriction (&option->rest, node);
	    break;
	case CompOptionTypeFloat:
	    initFloatRestriction (&option->rest, node);
	    break;
	case CompOptionTypeString:
	    initStringRestriction (&option->rest, node);
	default:
	    break;
	}
	break;
    }

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
compGetStringFromMetadataPath (CompMetadata *m,
			       char         *path)
{
    CompXPath xPath;
    char      *v = NULL;

    if (!initXPathFromMetadataPath (&xPath, m, BAD_CAST path))
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
