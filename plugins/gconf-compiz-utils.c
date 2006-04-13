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
#include <glib.h>
#include <gconf/gconf-client.h>

#include <compiz.h>

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
    { "<Release>",    CompReleaseMask    }
};

#define N_MODIFIERS (sizeof (modifiers) / sizeof (struct _GConfModifier))

char *
gconfBindingToString (CompDisplay     *d,
		      CompOptionValue *value)
{
    guint modMask;

    if (value->bind.type == CompBindingTypeNone)
	return g_strdup ("Disabled");

    if (value->bind.type == CompBindingTypeButton)
	modMask = value->bind.u.button.modifiers;
    else
	modMask = value->bind.u.key.modifiers;

    if (modMask & (CompPressMask | CompReleaseMask))
    {
	gchar *m, *mods = g_strdup ("");
	gchar *binding;
	gint  i;

	for (i = 0; i < N_MODIFIERS; i++)
	{
	    if (modMask & modifiers[i].modifier)
	    {
		m = g_strconcat (mods, modifiers[i].name, NULL);
		if (m)
		{
		    g_free (mods);
		    mods = m;
		}
	    }
	}

	if (value->bind.type == CompBindingTypeButton)
	{
	    binding = g_strdup_printf ("%sButton%d", mods,
				       value->bind.u.button.button);
	}
	else
	{
	    KeySym keysym;
	    gchar  *keyname;

	    keysym = XKeycodeToKeysym (d->display,
				       value->bind.u.key.keycode,
				       0);
	    keyname = XKeysymToString (keysym);

	    if (keyname)
		binding = g_strdup_printf ("%s%s", mods, keyname);
	    else
		binding = g_strdup_printf ("%s0x%x", mods,
					   value->bind.u.key.keycode);

	}

	g_free (mods);

	return binding;
    }
    else
    {
	return g_strdup ("Disabled");
    }
}

int
gconfStringToBinding (CompDisplay     *d,
		      const char      *binding,
		      CompOptionValue *value)
{
    gchar *ptr;
    gint  i;
    guint mods = 0;

    if (strcasecmp (binding, "disabled") == 0)
    {
	value->bind.type = CompBindingTypeNone;
	return TRUE;
    }

    for (i = 0; i < N_MODIFIERS; i++)
    {
	if (strcasestr (binding, modifiers[i].name))
	    mods |= modifiers[i].modifier;
    }

    /* if not explicetly set to be triggered at release
       assume it to be triggered at press */
    if (!(mods & CompReleaseMask))
	mods |= CompPressMask;

    ptr = strrchr (binding, '>');
    if (ptr)
	binding = ptr + 1;

    while (*binding && !isalnum (*binding))
	binding++;

    ptr = (gchar *) binding;
    if (strcmpskipifequal (&ptr, "Button") == 0)
    {
	gint button;

	if (sscanf (ptr, "%d", &button) == 1)
	{
	    value->bind.type = CompBindingTypeButton;
	    value->bind.u.button.button = button;
	    value->bind.u.button.modifiers = mods;

	    return TRUE;
	}
    }
    else
    {
	KeySym keysym;

	keysym = XStringToKeysym (binding);
	if (keysym != NoSymbol)
	{
	    KeyCode keycode;

	    keycode = XKeysymToKeycode (d->display, keysym);
	    if (keycode)
	    {
		value->bind.type = CompBindingTypeKey;
		value->bind.u.key.keycode = keycode;
		value->bind.u.key.modifiers = mods;

		return TRUE;
	    }
	}

	if (strncmp (binding, "0x", 2) == 0)
	{
	    value->bind.type = CompBindingTypeKey;
	    value->bind.u.key.keycode = strtol (binding, NULL, 0);
	    value->bind.u.key.modifiers = mods;

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
