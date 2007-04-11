/*
 * Copyright © 2007 Dennis Kasprzyk
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Mike Dransfield not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Mike Dransfield makes no representations about the suitability of this
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

CompMetadata *
compGetMetadataFromFile (const char *file, const char *plugin)
{
    xmlDoc *doc = NULL;
    CompMetadata *m;

    if (!file)
	return NULL;
    
    doc = xmlReadFile (file, NULL, 0);
    if (!doc)
    {
	fprintf (stderr, "%s: Unable to parse XML metadata form file \"%s\" "
		 "for \"%s\"\n",
		 programName, file, plugin);
	return NULL;
    }
    m = malloc (sizeof(CompMetadata));
    m->plugin = (plugin) ? strdup(plugin) : strdup("");
    m->doc = doc;
    return m;
}

CompMetadata *
compGetMetadataFromString (char *string, const char *plugin)
{
    xmlDoc *doc = NULL;
    CompMetadata *m;

    if (!string)
	return NULL;

    doc = xmlReadMemory (string, strlen(string), NULL, NULL, 0);
    if (!doc)
    {
	fprintf (stderr, "%s: Unable to parse XML metadata for \"%s\"\n",
		 programName, plugin);
	return NULL;
    }
    m = malloc (sizeof(CompMetadata));
    m->plugin = (plugin) ? strdup(plugin) : strdup("");
    m->doc = doc;
    return m;
}

void
compFreeMetadata (CompMetadata *data)
{
    if (!data)
	return;
    
    xmlFreeDoc (data->doc);
    free (data->plugin);
}

static Bool
getOptionType (char *type, CompOptionType *oType)
{
    if (!strcasecmp (type, "bool"))
    {
	*oType = CompOptionTypeBool;
	return TRUE;
    }
    else if (!strcasecmp (type, "int"))
    {
	*oType = CompOptionTypeInt;
	return TRUE;
    }
    else if (!strcasecmp (type, "float"))
    {
	*oType = CompOptionTypeFloat;
	return TRUE;
    }
    else if (!strcasecmp (type, "string"))
    {
	*oType = CompOptionTypeString;
	return TRUE;
    }
    else if (!strcasecmp (type, "color"))
    {
	*oType = CompOptionTypeColor;
	return TRUE;
    }
    else if (!strcasecmp (type, "action"))
    {
	*oType = CompOptionTypeAction;
	return TRUE;
    }
    else if (!strcasecmp (type, "match"))
    {
	*oType = CompOptionTypeMatch;
	return TRUE;
    }
    else if (!strcasecmp (type, "list"))
    {
	*oType = CompOptionTypeList;
	return TRUE;
    }
    return FALSE;
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
		continue;
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
	    return;
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "green"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    v->c[1] = MAX (0, MIN (0xffff, strtol ((char *) value, NULL , 0)));
	    xmlFree (value);
	    return;
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "blue"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    v->c[2] = MAX (0, MIN (0xffff, strtol ((char *) value, NULL , 0)));
	    xmlFree (value);
	    return;
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "alpha"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode, 1);
	    v->c[3] = MAX (0, MIN (0xffff, strtol ((char *) value, NULL , 0)));
	    xmlFree (value);
	    return;
	}
	cur = cur->next;
    }
}

static void
convertKeyBinding (CompKeyBinding *kb, char *bind)
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
	    kb->keycode = XKeysymToKeycode (dpy, XStringToKeysym (tok));
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
initActionValue (CompOptionValue *v, xmlNodePtr node)
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
	    initActionValue (v, cur->xmlChildrenNode);
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
		convertKeyBinding (&v->action.key, (char *) key);
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
		continue;
            matchAddFromString (&v->match, (char *) value);
	    xmlFree (value);
	    return;
	}
	cur = cur->next;
    }
}

static void
initListValue (CompOptionValue *v, xmlNodePtr node)
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
	    xmlChar *type = xmlGetProp (node, BAD_CAST "type");
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
	    if (!getOptionType ((char *)type, &oType))
	    {
        	fprintf (stderr, "%s: Not supported list type \"%s\"\n",
		         programName, (char *) type);
		xmlFree (type);
		return;
	    }
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
			initActionValue (&v->list.value[i], cur);
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
	    return;
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "max"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode,
						  1);
	    r->i.max = strtol ((char *) value, NULL, 0);
	    xmlFree (value);
	    return;
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
	    return;
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "max"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode,
						  1);
	    r->f.max = strtod ((char *) value, NULL);
	    xmlFree (value);
	    return;
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST "precision"))
	{
	    xmlChar *value = xmlNodeListGetString (cur->doc,
						   cur->xmlChildrenNode,
						  1);
	    r->f.precision = strtod ((char *) value, NULL);
	    xmlFree (value);
	    return;
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
initOptionFromNode (CompOption *o, xmlNodePtr node)
{
    xmlChar *name;
    xmlChar *type;
    CompOptionType oType;

    memset (o, 0, sizeof(CompOption));
    
    name = xmlGetProp (node, BAD_CAST "name");
    type = xmlGetProp (node, BAD_CAST "type");

    o->name = strdup ((char *)name);
    xmlFree (name);
    
    if (!type || !xmlStrlen(type))
    {
        fprintf (stderr, "%s: No Option Type defined for option \"%s\"\n",
		 programName, o->name);
        if (type)
	   xmlFree (type);
	return FALSE;
    }
    if (!getOptionType ((char *)type, &oType))
    {
        fprintf (stderr, "%s: Not supported option type \"%s\"\n",
		 programName, (char *) type);
        xmlFree (type);
	return FALSE;
    }
    xmlFree (type);

    o->type = oType;
    
    switch (oType)
    {
    case CompOptionTypeBool:
	initBoolValue (&o->value, node);
	break;
    case CompOptionTypeInt:
	initIntValue (&o->value, node);
	initIntRestriction (&o->rest, node);
	break;
    case CompOptionTypeFloat:
	initFloatValue (&o->value, node);
	initFloatRestriction (&o->rest, node);
	break;
    case CompOptionTypeString:
	initStringValue (&o->value, node);
	initStringRestriction (&o->rest, node);
	break;
    case CompOptionTypeColor:
	initColorValue (&o->value, node);
	break;
    case CompOptionTypeAction:
	initActionValue (&o->value, node);
	break;
    case CompOptionTypeMatch:
	initMatchValue (&o->value, node);
	break;
    case CompOptionTypeList:
	initListValue (&o->value, node);
	switch (o->value.list.type)
	{
	case CompOptionTypeInt:
	    initIntRestriction (&o->rest, node);
	    break;
	case CompOptionTypeFloat:
	    initFloatRestriction (&o->rest, node);
	    break;
	case CompOptionTypeString:
	    initStringRestriction (&o->rest, node);
	    break;
	default:
	    break;
	}
	break;
    default:
	return FALSE;
    }
   
    
    return TRUE;
}

static Bool
initOptionFromMetadataPath (CompMetadata *m,
			    CompOption *o,
			    const xmlChar *path)
{
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;
    int i;
    int size;
    Bool rv = FALSE;

    xpathCtx = xmlXPathNewContext (m->doc);
    xpathObj = xmlXPathEvalExpression (path, xpathCtx);
    if (!xpathObj)
        return FALSE;

    size = (xpathObj->nodesetval)? xpathObj->nodesetval->nodeNr : 0;
    for (i = 0; i < size && !rv; i++)
	rv |= initOptionFromNode (o, xpathObj->nodesetval->nodeTab[i]);
    
    xmlXPathFreeObject (xpathObj);
    xmlXPathFreeContext (xpathCtx);
    return rv;
}

Bool
compInitScreenOptionFromMetadata (CompMetadata *m,
				  CompOption *o,
				  const char *name)
{
    char str[1024];
    
    if (!m || !o || !name)
	return FALSE;

    if (strlen (m->plugin))
    {
	sprintf (str, "/compiz/plugin[@name=\"%s\"]/screen//"
		"option[@name=\"%s\"]", m->plugin, name);
    }
    else
    {
	sprintf (str, "/compiz/general/screen//"
		"option[@name=\"%s\"]", name);
    }
    
    return initOptionFromMetadataPath (m, o, BAD_CAST str);
}

Bool
compInitDisplayOptionFromMetadata (CompMetadata *m,
				   CompOption *o,
				   const char *name)
{
    char str[1024];
    
    if (!m || !o || !name)
	return FALSE;

    if (strlen (m->plugin))
    {
	sprintf (str, "/compiz/plugin[@name=\"%s\"]/display//"
		"option[@name=\"%s\"]", m->plugin, name);
    }
    else
    {
	sprintf (str, "/compiz/general/display//"
		"option[@name=\"%s\"]", name);
    }
    
    return initOptionFromMetadataPath (m, o, BAD_CAST str);
}

char *
compGetStringFromMetadataPath (CompMetadata *m,
			       char         *path)
{
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;
    char *rv = NULL;

    xpathCtx = xmlXPathNewContext (m->doc);
    xpathObj = xmlXPathEvalExpression (BAD_CAST path, xpathCtx);
    if (!xpathObj)
        return NULL;
    xpathObj = xmlXPathConvertString (xpathObj);

    if (xpathObj->type == XPATH_STRING && xpathObj->stringval)
    {
	rv = strdup ((char *) xpathObj->stringval);
    }

    xmlXPathFreeObject (xpathObj);
    xmlXPathFreeContext (xpathCtx);
    return rv;
}

char *
compGetShortPluginDescription (CompMetadata *m)
{
    char str[1024];
    
    if (!m)
	return NULL;

    if (strlen (m->plugin))
    {
	sprintf (str, "/compiz/plugin[@name=\"%s\"]/short/child::text()",
		 m->plugin);
    }
    else
    {
	sprintf (str, "/compiz/general/short/child::text()");
    }
    
    return compGetStringFromMetadataPath (m, str);
}

char *
compGetLongPluginDescription (CompMetadata *m)
{
    char str[1024];
    
    if (!m)
	return NULL;

    if (strlen (m->plugin))
    {
	sprintf (str, "/compiz/plugin[@name=\"%s\"]/long/child::text()",
		 m->plugin);
    }
    else
    {
	sprintf (str, "/compiz/general/long/child::text()");
    }
    
    return compGetStringFromMetadataPath (m, str);
}

char *
compGetShortScreenOptionDescription (CompMetadata *m,
				     CompOption   *o)
{
    char str[1024];
    
    if (!m)
	return NULL;

    if (strlen (m->plugin))
    {
	sprintf (str, "/compiz/plugin[@name=\"%s\"]/screen//"
		"option[@name=\"%s\"]/short/child::text()",
		m->plugin, o->name);
    }
    else
    {
	sprintf (str, "/compiz/general/screen//"
		"option[@name=\"%s\"]/short/child::text()", o->name);
    }
    
    return compGetStringFromMetadataPath (m, str);
}

char *
compGetLongScreenOptionDescription (CompMetadata *m,
				    CompOption   *o)
{
    char str[1024];
    
    if (!m)
	return NULL;

    if (strlen (m->plugin))
    {
	sprintf (str, "/compiz/plugin[@name=\"%s\"]/screen//"
		"option[@name=\"%s\"]/long/child::text()",
		m->plugin, o->name);
    }
    else
    {
	sprintf (str, "/compiz/general/screen//"
		"option[@name=\"%s\"]/long/child::text()", o->name);
    }
    
    return compGetStringFromMetadataPath (m, str);
}


char *
compGetShortDisplayOptionDescription (CompMetadata *m,
				      CompOption   *o)
{
    char str[1024];
    
    if (!m)
	return NULL;

    if (strlen (m->plugin))
    {
	sprintf (str, "/compiz/plugin[@name=\"%s\"]/display//"
		"option[@name=\"%s\"]/short/child::text()",
		m->plugin, o->name);
    }
    else
    {
	sprintf (str, "/compiz/general/display//"
		"option[@name=\"%s\"]/short/child::text()", o->name);
    }
    
    return compGetStringFromMetadataPath (m, str);
}


char *
compGetLongDisplayOptionDescription (CompMetadata *m,
				     CompOption   *o)
{
    char str[1024];
    
    if (!m)
	return NULL;

    if (strlen (m->plugin))
    {
	sprintf (str, "/compiz/plugin[@name=\"%s\"]/display//"
		"option[@name=\"%s\"]/long/child::text()",
		m->plugin, o->name);
    }
    else
    {
	sprintf (str, "/compiz/general/display//"
		"option[@name=\"%s\"]/long/child::text()", o->name);
    }
    
    return compGetStringFromMetadataPath (m, str);
}
