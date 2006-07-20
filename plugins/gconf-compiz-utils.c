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

#define _GNU_SOURCE
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <compiz.h>

#include <glib.h>
#include <gconf/gconf-client.h>
#include <gconf-compiz-utils.h>

struct _GConfModifier {
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

#define N_MODIFIERS (sizeof (modifiers) / sizeof (struct _GConfModifier))

static gchar *edgeName[] = {
    N_("Left"),
    N_("Right"),
    N_("Top"),
    N_("Bottom"),
    N_("TopLeft"),
    N_("TopRight"),
    N_("BottomLeft"),
    N_("BottomRight")
};

static GString *
gconfModifiersToString (CompDisplay *d,
			guint	    modMask)
{
    GString *binding;
    gint    i;

    binding = g_string_new (NULL);
    for (i = 0; i < N_MODIFIERS; i++)
    {
	if (modMask & modifiers[i].modifier)
	    g_string_append (binding, modifiers[i].name);
    }

    return binding;
}

char *
gconfKeyBindingToString (CompDisplay    *d,
			 CompKeyBinding *key)
{
    GString *binding;

    binding = gconfModifiersToString (d, key->modifiers);

    if (key->keycode != 0)
    {
	KeySym keysym;
	gchar  *keyname;

	keysym  = XKeycodeToKeysym (d->display, key->keycode, 0);
	keyname = XKeysymToString (keysym);

	if (keyname)
	    g_string_append (binding, keyname);
	else
	    g_string_append_printf (binding, "0x%x", key->keycode);
    }

    return g_string_free (binding, FALSE);
}

char *
gconfButtonBindingToString (CompDisplay       *d,
			    CompButtonBinding *button)
{
    GString *binding;

    binding = gconfModifiersToString (d, button->modifiers);

    g_string_append_printf (binding, "Button%d", button->button);

    return g_string_free (binding, FALSE);
}

static guint
gconfStringToModifiers (CompDisplay *d,
			const char  *binding)
{
    guint mods = 0;
    gint  i;

    for (i = 0; i < N_MODIFIERS; i++)
    {
	if (strcasestr (binding, modifiers[i].name))
	    mods |= modifiers[i].modifier;
    }

    return mods;
}

int
gconfStringToKeyBinding (CompDisplay    *d,
			 const char     *binding,
			 CompKeyBinding *key)
{
    gchar  *ptr;
    guint  mods;
    KeySym keysym;

    mods = gconfStringToModifiers (d, binding);

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

int
gconfStringToButtonBinding (CompDisplay	      *d,
			    const char	      *binding,
			    CompButtonBinding *button)
{
    gchar *ptr;
    guint mods;

    mods = gconfStringToModifiers (d, binding);

    ptr = strrchr (binding, '>');
    if (ptr)
	binding = ptr + 1;

    while (*binding && !isalnum (*binding))
	binding++;

    ptr = (gchar *) binding;
    if (strcmpskipifequal (&ptr, "Button") == 0)
    {
	gint buttonNum;

	if (sscanf (ptr, "%d", &buttonNum) == 1)
	{
	    button->button    = buttonNum;
	    button->modifiers = mods;

	    return TRUE;
	}
    }

    return FALSE;
}

int
strcmpskipifequal (char **ptr,
		   char *s)
{
    int ret, len;

    len = strlen (s);
    ret = strncmp (*ptr, s, len);
    if (ret == 0)
	*ptr = (*ptr) + len;

    return ret;
}

gchar *
gconfEdgeToString (guint edge)
{
    return edgeName[edge];
}
