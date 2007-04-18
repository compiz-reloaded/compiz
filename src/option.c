/*
 * Copyright © 2005 Novell, Inc.
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
 *
 * Author: David Reveman <davidr@novell.com>
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

#include <compiz.h>

struct _Modifier {
    char *name;
    int  modifier;
} modifiers[] = {
    { "<Shift>",      ShiftMask		 },
    { "<Control>",    ControlMask	 },
    { "<Mod1>",	      Mod1Mask		 },
    { "<Mod2>",	      Mod2Mask		 },
    { "<Mod3>",	      Mod3Mask		 },
    { "<Mod4>",	      Mod4Mask		 },
    { "<Mod5>",	      Mod5Mask		 },
    { "<Alt>",	      CompAltMask        },
    { "<Meta>",	      CompMetaMask       },
    { "<Super>",      CompSuperMask      },
    { "<Hyper>",      CompHyperMask	 },
    { "<ModeSwitch>", CompModeSwitchMask },
};

#define N_MODIFIERS (sizeof (modifiers) / sizeof (struct _Modifier))

static char *edgeName[] = {
    "Left",
    "Right",
    "Top",
    "Bottom",
    "TopLeft",
    "TopRight",
    "BottomLeft",
    "BottomRight"
};

static void
finiOptionValue (CompOptionValue *v,
		 CompOptionType  type)
{
    int i;

    switch (type) {
    case CompOptionTypeString:
	if (v->s)
	    free (v->s);
	break;
    case CompOptionTypeMatch:
	matchFini (&v->match);
	break;
    case CompOptionTypeList:
	for (i = 0; i < v->list.nValue; i++)
	    finiOptionValue (&v->list.value[i], v->list.type);
    default:
	break;
    }
}

static void
finiOptionRestriction (CompOptionRestriction *r,
		       CompOptionType	     type)
{
    int i;

    switch (type) {
    case CompOptionTypeString:
	if (r->s.nString)
	{
	    for (i = 0; i < r->s.nString; i++)
		free (r->s.string[i]);

	    free (r->s.string);
	}
    default:
	break;
    }
}

void
compInitOption (CompOption *o)
{
    memset (o, 0, sizeof (CompOption));
}

void
compFiniOption (CompOption *o)
{
    switch (o->type) {
    case CompOptionTypeList:
	finiOptionRestriction (&o->rest, o->value.list.type);
    default:
	break;
    }

    finiOptionValue (&o->value, o->type);
}

CompOption *
compFindOption (CompOption *option,
		int	    nOption,
		char	    *name,
		int	    *index)
{
    int i;

    for (i = 0; i < nOption; i++)
    {
	if (strcmp (option[i].name, name) == 0)
	{
	    if (index)
		*index = i;

	    return &option[i];
	}
    }

    return 0;
}

Bool
compSetBoolOption (CompOption	   *option,
		   CompOptionValue *value)
{
    int b;

    b = (value->b) ? TRUE : FALSE;

    if (option->value.b == b)
	return FALSE;

    option->value.b = b;

    return TRUE;
}

Bool
compSetIntOption (CompOption	  *option,
		  CompOptionValue *value)
{
    if (value->i < option->rest.i.min ||
	value->i > option->rest.i.max ||
	value->i == option->value.i)
	return FALSE;

    option->value.i = value->i;

    return TRUE;
}

Bool
compSetFloatOption (CompOption	    *option,
		    CompOptionValue *value)
{
    float v, p;
    int sign = (value->f < 0 ? -1 : 1);

    p = 1.0f / option->rest.f.precision;
    v = ((int) (value->f * p + sign * 0.5f)) / p;

    if (v < option->rest.f.min ||
	v > option->rest.f.max ||
	v == option->value.f)
	return FALSE;

    option->value.f = v;

    return TRUE;
}

Bool
compSetStringOption (CompOption	     *option,
		     CompOptionValue *value)
{
    char *s;

    s = value->s;
    if (!s)
	s = "";

    if (option->rest.s.nString)
    {
	int i;

	for (i = 0; i < option->rest.s.nString; i++)
	{
	    if (strcmp (option->rest.s.string[i], s) == 0)
		break;
	}

	if (i == option->rest.s.nString)
	    s = option->rest.s.string[0];
    }

    if (option->value.s == s)
	return FALSE;

    if (option->value.s && s)
    {
	if (strcmp (option->value.s, s) == 0)
	    return FALSE;
    }

    if (option->value.s)
	free (option->value.s);

    option->value.s = strdup (s);

    return TRUE;
}

Bool
compSetColorOption (CompOption	    *option,
		    CompOptionValue *value)
{
    if (memcmp (value->c, option->value.c, sizeof (value->c)) == 0)
	return FALSE;

    memcpy (option->value.c, value->c, sizeof (value->c));

    return TRUE;
}

Bool
compSetActionOption (CompOption      *option,
		     CompOptionValue *value)
{
    CompAction *action = &option->value.action;

    if (value->action.type     == action->type &&
	value->action.bell     == action->bell &&
	value->action.edgeMask == action->edgeMask)
    {
	Bool equal = TRUE;

	if (value->action.type & CompBindingTypeButton)
	{
	    if (action->button.button    != value->action.button.button ||
		action->button.modifiers != value->action.button.modifiers)
		equal = FALSE;
	}

	if (value->action.type & CompBindingTypeKey)
	{
	    if (action->key.keycode   != value->action.key.keycode ||
		action->key.modifiers != value->action.key.modifiers)
		equal = FALSE;
	}

	if (value->action.type & CompBindingTypeEdgeButton)
	{
	    if (action->edgeButton != value->action.edgeButton)
		equal = FALSE;
	}

	if (equal)
	    return FALSE;
    }

    *action = value->action;

    return TRUE;
}

Bool
compSetMatchOption (CompOption      *option,
		    CompOptionValue *value)
{
    CompDisplay *display = option->value.match.display;
    CompMatch	match;

    if (matchEqual (&option->value.match, &value->match))
	return FALSE;

    if (!matchCopy (&match, &value->match))
	return FALSE;

    matchFini (&option->value.match);

    option->value.match.op  = match.op;
    option->value.match.nOp = match.nOp;

    if (display)
	matchUpdate (display, &option->value.match);

    return TRUE;
}

Bool
compSetOptionList (CompOption      *option,
		   CompOptionValue *value)
{
    CompOption o;
    Bool       status = FALSE;
    int        i, min;

    if (value->list.nValue != option->value.list.nValue)
    {
	CompOptionValue *v;

	v = malloc (sizeof (CompOptionValue) * value->list.nValue);
	if (!v)
	    return FALSE;

	min = MIN (value->list.nValue, option->value.list.nValue);

	for (i = min; i < option->value.list.nValue; i++)
	{
	    switch (option->value.list.type) {
	    case CompOptionTypeString:
		if (option->value.list.value[i].s)
		    free (option->value.list.value[i].s);
		break;
	    case CompOptionTypeMatch:
		matchFini (&option->value.list.value[i].match);
	    default:
		break;
	    }
	}

	memset (v, 0, sizeof (CompOptionValue) * value->list.nValue);

	if (min)
	    memcpy (v, option->value.list.value,
		    sizeof (CompOptionValue) * min);

	if (option->value.list.value)
	    free (option->value.list.value);

	option->value.list.value = v;
	option->value.list.nValue = value->list.nValue;

	status = TRUE;
    }

    o = *option;
    o.type = option->value.list.type;

    for (i = 0; i < value->list.nValue; i++)
    {
	o.value = option->value.list.value[i];

	switch (o.type) {
	case CompOptionTypeBool:
	    status |= compSetBoolOption (&o, &value->list.value[i]);
	    break;
	case CompOptionTypeInt:
	    status |= compSetIntOption (&o, &value->list.value[i]);
	    break;
	case CompOptionTypeFloat:
	    status |= compSetFloatOption (&o, &value->list.value[i]);
	    break;
	case CompOptionTypeString:
	    status |= compSetStringOption (&o, &value->list.value[i]);
	    break;
	case CompOptionTypeColor:
	    status |= compSetColorOption (&o, &value->list.value[i]);
	    break;
	case CompOptionTypeMatch:
	    status |= compSetMatchOption (&o, &value->list.value[i]);
	default:
	    break;
	}

	option->value.list.value[i] = o.value;
    }

    return status;
}

Bool
compSetOption (CompOption      *option,
	       CompOptionValue *value)
{
    switch (option->type) {
    case CompOptionTypeBool:
	return compSetBoolOption (option, value);
    case CompOptionTypeInt:
	return compSetIntOption (option, value);
    case CompOptionTypeFloat:
	return compSetFloatOption (option, value);
    case CompOptionTypeString:
	return compSetStringOption (option, value);
    case CompOptionTypeColor:
	return compSetColorOption (option, value);
    case CompOptionTypeMatch:
	return compSetMatchOption (option, value);
    case CompOptionTypeAction:
	return compSetActionOption (option, value);
    case CompOptionTypeList:
	return compSetOptionList (option, value);
    }

    return FALSE;
}

unsigned int
compWindowTypeMaskFromStringList (CompOptionValue *value)
{
    int		 i;
    unsigned int mask = 0;

    for (i = 0; i < value->list.nValue; i++)
	mask |= windowTypeFromString (value->list.value[i].s);

    return mask;
}

Bool
getBoolOptionNamed (CompOption *option,
		    int	       nOption,
		    char       *name,
		    Bool       defaultValue)
{
    while (nOption--)
    {
	if (option->type == CompOptionTypeBool)
	    if (strcmp (option->name, name) == 0)
		return option->value.b;

	option++;
    }

    return defaultValue;
}

int
getIntOptionNamed (CompOption *option,
		   int	      nOption,
		   char	      *name,
		   int	      defaultValue)
{
    while (nOption--)
    {
	if (option->type == CompOptionTypeInt)
	    if (strcmp (option->name, name) == 0)
		return option->value.i;

	option++;
    }

    return defaultValue;
}

float
getFloatOptionNamed (CompOption *option,
		     int	nOption,
		     char	*name,
		     float	defaultValue)
{
    while (nOption--)
    {
	if (option->type == CompOptionTypeFloat)
	    if (strcmp (option->name, name) == 0)
		return option->value.f;

	option++;
    }

    return defaultValue;
}

char *
getStringOptionNamed (CompOption *option,
		      int	 nOption,
		      char	 *name,
		      char	 *defaultValue)
{
    while (nOption--)
    {
	if (option->type == CompOptionTypeString)
	    if (strcmp (option->name, name) == 0)
		return option->value.s;

	option++;
    }

    return defaultValue;
}

unsigned short *
getColorOptionNamed (CompOption	    *option,
		     int	    nOption,
		     char	    *name,
		     unsigned short *defaultValue)
{
    while (nOption--)
    {
	if (option->type == CompOptionTypeColor)
	    if (strcmp (option->name, name) == 0)
		return option->value.c;

	option++;
    }

    return defaultValue;
}

CompMatch *
getMatchOptionNamed (CompOption	*option,
		     int	nOption,
		     char	*name,
		     CompMatch  *defaultValue)
{
    while (nOption--)
    {
	if (option->type == CompOptionTypeMatch)
	    if (strcmp (option->name, name) == 0)
		return &option->value.match;

	option++;
    }

    return defaultValue;
}

static char *
stringAppend (char *s,
	      char *a)
{
    char *r;
    int  len;

    len = strlen (a);

    if (s)
	len += strlen (s);

    r = malloc (len + 1);
    if (r)
    {
	if (s)
	{
	    sprintf (r, "%s%s", s, a);
	    free (s);
	}
	else
	{
	    sprintf (r, "%s", a);
	}

	s = r;
    }

    return s;
}

static char *
modifiersToString (CompDisplay  *d,
		   unsigned int modMask)
{
    char *binding = NULL;
    int  i;

    for (i = 0; i < N_MODIFIERS; i++)
    {
	if (modMask & modifiers[i].modifier)
	    binding = stringAppend (binding, modifiers[i].name);
    }

    return binding;
}

char *
keyBindingToString (CompDisplay    *d,
		    CompKeyBinding *key)
{
    char *binding;

    binding = modifiersToString (d, key->modifiers);

    if (key->keycode != 0)
    {
	KeySym keysym;
	char   *keyname;

	keysym  = XKeycodeToKeysym (d->display, key->keycode, 0);
	keyname = XKeysymToString (keysym);

	if (keyname)
	{
	    binding = stringAppend (binding, keyname);
	}
	else
	{
	    char keyCodeStr[256];

	    snprintf (keyCodeStr, 256, "0x%x", key->keycode);
	    binding = stringAppend (binding, keyCodeStr);
	}
    }

    return binding;
}

char *
buttonBindingToString (CompDisplay       *d,
		       CompButtonBinding *button)
{
    char *binding;
    char buttonStr[256];

    binding = modifiersToString (d, button->modifiers);

    snprintf (buttonStr, 256, "Button%d", button->button);
    binding = stringAppend (binding, buttonStr);

    return binding;
}

static unsigned int
stringToModifiers (CompDisplay *d,
		   const char  *binding)
{
    unsigned int mods = 0;
    int		 i;

    for (i = 0; i < N_MODIFIERS; i++)
    {
	if (strcasestr (binding, modifiers[i].name))
	    mods |= modifiers[i].modifier;
    }

    return mods;
}

Bool
stringToKeyBinding (CompDisplay    *d,
		    const char     *binding,
		    CompKeyBinding *key)
{
    char	  *ptr;
    unsigned int  mods;
    KeySym	  keysym;

    mods = stringToModifiers (d, binding);

    ptr = strrchr (binding, '>');
    if (ptr)
	binding = ptr + 1;

    while (*binding && !isalnum (*binding))
	binding++;

    keysym = XStringToKeysym (binding);
    if (keysym != NoSymbol)
    {
	KeyCode keycode;

	keycode = XKeysymToKeycode (d->display, keysym);
	if (keycode)
	{
	    key->keycode   = keycode;
	    key->modifiers = mods;

	    return TRUE;
	}
    }

    if (strncmp (binding, "0x", 2) == 0)
    {
	key->keycode   = strtol (binding, NULL, 0);
	key->modifiers = mods;

	return TRUE;
    }

    return FALSE;
}

Bool
stringToButtonBinding (CompDisplay	 *d,
		       const char	 *binding,
		       CompButtonBinding *button)
{
    char	 *ptr;
    unsigned int mods;

    mods = stringToModifiers (d, binding);

    ptr = strrchr (binding, '>');
    if (ptr)
	binding = ptr + 1;

    while (*binding && !isalnum (*binding))
	binding++;

    if (strncmp (binding, "Button", strlen ("Button")) == 0)
    {
	int buttonNum;

	if (sscanf (binding + strlen ("Button"), "%d", &buttonNum) == 1)
	{
	    button->button    = buttonNum;
	    button->modifiers = mods;

	    return TRUE;
	}
    }

    return FALSE;
}

char *
edgeToString (unsigned int edge)
{
    return edgeName[edge];
}

Bool
stringToColor (const char     *color,
	       unsigned short *rgba)
{
    int c[4];

    if (sscanf (color, "#%2x%2x%2x%2x", &c[0], &c[1], &c[2], &c[3]) == 4)
    {
	rgba[0] = c[0] << 8 | c[0];
	rgba[1] = c[1] << 8 | c[1];
	rgba[2] = c[2] << 8 | c[2];
	rgba[3] = c[3] << 8 | c[3];

	return TRUE;
    }

    return FALSE;
}

char *
colorToString (unsigned short *rgba)
{
    char tmp[256];

    snprintf (tmp, 256, "#%.2x%.2x%.2x%.2x",
	      rgba[0] / 256, rgba[1] / 256, rgba[2] / 256, rgba[3] / 256);

    return strdup (tmp);
}

char *
optionTypeToString (CompOptionType type)
{
    switch (type) {
    case CompOptionTypeAction:
	return "action";
    case CompOptionTypeMatch:
	return "match";
    case CompOptionTypeBool:
	return "bool";
    case CompOptionTypeInt:
	return "int";
    case CompOptionTypeFloat:
	return "float";
    case CompOptionTypeString:
	return "string";
    case CompOptionTypeColor:
	return "color";
    case CompOptionTypeList:
	return "list";
    }

    return "unknown";
}
