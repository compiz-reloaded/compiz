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

#include <config.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <gconf/gconf-client.h>

#include "compiz-window-manager.h"

#define COMPIZ_CLICK_TO_FOCUS_KEY			     \
    "/apps/compiz/general/allscreens/options/click_to_focus"

#define COMPIZ_AUTORAISE_KEY				\
    "/apps/compiz/general/allscreens/options/autoraise"

#define COMPIZ_AUTORAISE_DELAY_KEY			      \
    "/apps/compiz/general/allscreens/options/autoraise_delay"

#define COMPIZ_MOUSE_MOVE_KEY				 \
    "/apps/compiz/plugins/move/screen0/options/initiate"

#define COMPIZ_DOUBLE_CLICK_TITLEBAR_KEY				   \
    "/apps/compiz/general/allscreens/options/action_double_click_titlebar"

enum {
    DOUBLE_CLICK_MAXIMIZE
};

static GnomeWindowManagerClass *parent_class;

struct _CompizWindowManagerPrivate {
    GConfClient *gconf;
    char	*mouse_modifier;
};

static void
value_changed (GConfClient *client,
	       const gchar *key,
	       GConfValue  *value,
	       void        *data)
{
    CompizWindowManager *wm;

    wm = COMPIZ_WINDOW_MANAGER (data);

    gnome_window_manager_settings_changed (GNOME_WINDOW_MANAGER (wm));
}

/* this function is called when the shared lib is loaded */
GObject *
window_manager_new (int expected_interface_version)
{
    GObject *wm;

    if (expected_interface_version != GNOME_WINDOW_MANAGER_INTERFACE_VERSION)
    {
	g_warning ("Compiz window manager module wasn't compiled with the "
		   "current version of gnome-control-center");
	return NULL;
    }

    wm = g_object_new (compiz_window_manager_get_type (), NULL);

    return wm;
}

static void
compiz_change_settings (GnomeWindowManager    *wm,
			const GnomeWMSettings *settings)
{
    CompizWindowManager *cwm;

    cwm = COMPIZ_WINDOW_MANAGER (wm);

    if (settings->flags & GNOME_WM_SETTING_MOUSE_FOCUS)
	gconf_client_set_bool (cwm->p->gconf,
			       COMPIZ_CLICK_TO_FOCUS_KEY,
			       settings->focus_follows_mouse == FALSE,
			       NULL);

     if (settings->flags & GNOME_WM_SETTING_AUTORAISE)
	gconf_client_set_bool (cwm->p->gconf,
			       COMPIZ_AUTORAISE_KEY,
			       settings->autoraise, NULL);

     if (settings->flags & GNOME_WM_SETTING_AUTORAISE_DELAY)
	gconf_client_set_int (cwm->p->gconf,
			      COMPIZ_AUTORAISE_DELAY_KEY,
			      settings->autoraise_delay, NULL);

    if (settings->flags & GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER)
    {
	char *value;

	value = g_strdup_printf ("<%s>Button1", settings->mouse_move_modifier);
	gconf_client_set_string (cwm->p->gconf,
				 COMPIZ_MOUSE_MOVE_KEY,
				 value, NULL);
	g_free (value);
    }

/*
    if (settings->flags & GNOME_WM_SETTING_DOUBLE_CLICK_ACTION)
    {
	const char *action = NULL;

	switch (settings->double_click_action) {
	case DOUBLE_CLICK_SHADE:
	    action = "toggle_shade";
	    break;
	case DOUBLE_CLICK_MAXIMIZE:
	    action = "toggle_maximize";
	    break;
	}

	if (action)
	    gconf_client_set_string (meta_wm->p->gconf,
				     COMPIZ_DOUBLE_CLICK_TITLEBAR_KEY,
				     action, NULL);
    }
*/

}

static void
compiz_get_settings (GnomeWindowManager *wm,
		     GnomeWMSettings    *settings)
{
    CompizWindowManager *cwm;
    int			to_get;

    cwm = COMPIZ_WINDOW_MANAGER (wm);

    to_get = settings->flags;
    settings->flags = 0;

    if (to_get & GNOME_WM_SETTING_MOUSE_FOCUS)
    {
	settings->focus_follows_mouse =
	    gconf_client_get_bool (cwm->p->gconf,
				   COMPIZ_CLICK_TO_FOCUS_KEY, NULL) == FALSE;

	settings->flags |= GNOME_WM_SETTING_MOUSE_FOCUS;
    }

    if (to_get & GNOME_WM_SETTING_AUTORAISE)
    {
	settings->autoraise = FALSE;

	settings->autoraise = gconf_client_get_bool (cwm->p->gconf,
						     COMPIZ_AUTORAISE_KEY,
						     NULL);

	settings->flags |= GNOME_WM_SETTING_AUTORAISE;
    }

    if (to_get & GNOME_WM_SETTING_AUTORAISE_DELAY)
    {
	settings->autoraise_delay =
	    gconf_client_get_int (cwm->p->gconf,
				  COMPIZ_AUTORAISE_DELAY_KEY,
				  NULL);

	settings->flags |= GNOME_WM_SETTING_AUTORAISE_DELAY;
    }

    if (to_get & GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER)
    {
	const char *new;
	char	   *str;

	str = gconf_client_get_string (cwm->p->gconf,
				       COMPIZ_MOUSE_MOVE_KEY,
				       NULL);

	if (str == NULL)
	    str = g_strdup ("<Super>");

	if (strncmp (str, "<Super>", 7) == 0)
	    new = "Super";
	else if (strncmp (str, "<Alt>", 5) == 0)
	    new = "Alt";
	else if (strncmp (str, "<Meta>", 6) == 0)
	    new = "Meta";
	else if (strncmp (str, "<Hyper>", 7) == 0)
	    new = "Hyper";
	else if (strncmp (str, "<Control>", 9) == 0)
	    new = "Control";
	else
	    new = NULL;

	if (new && cwm->p->mouse_modifier &&
	    strcmp (new, cwm->p->mouse_modifier) == 0)
	{
	    /* unchanged */;
	}
	else
	{
	    g_free (cwm->p->mouse_modifier);
	    cwm->p->mouse_modifier = g_strdup (new ? new : "");
	}

	g_free (str);

	settings->mouse_move_modifier = cwm->p->mouse_modifier;

	settings->flags |= GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER;
    }

    if (to_get & GNOME_WM_SETTING_DOUBLE_CLICK_ACTION)
    {
/*
	char *str;

	str = gconf_client_get_string (cwm->p->gconf,
				       COMPIZ_DOUBLE_CLICK_TITLEBAR_KEY,
				       NULL);

	if (str == NULL)
	    str = g_strdup ("toggle_shade");

	if (strcmp (str, "toggle_shade") == 0)
	    settings->double_click_action = DOUBLE_CLICK_SHADE;
	else if (strcmp (str, "toggle_maximize") == 0)
	    settings->double_click_action = DOUBLE_CLICK_MAXIMIZE;
	else
	    settings->double_click_action = DOUBLE_CLICK_SHADE;

	g_free (str);

	settings->flags |= GNOME_WM_SETTING_DOUBLE_CLICK_ACTION;
*/
    }
}

static int
compiz_get_settings_mask (GnomeWindowManager *wm)
{
    return
/*      GNOME_WM_SETTING_FONT                | */
	GNOME_WM_SETTING_MOUSE_FOCUS         |
	GNOME_WM_SETTING_AUTORAISE           |
	GNOME_WM_SETTING_AUTORAISE_DELAY     |
	GNOME_WM_SETTING_MOUSE_MOVE_MODIFIER |
/*      GNOME_WM_SETTING_THEME               | */
/*      GNOME_WM_SETTING_DOUBLE_CLICK_ACTION */ 0;
}

static GList *
compiz_get_theme_list (GnomeWindowManager *wm)
{
    return NULL;
}

static char *
compiz_get_user_theme_folder (GnomeWindowManager *wm)
{
    return NULL;
}

static void
compiz_get_double_click_actions (GnomeWindowManager             *wm,
				 const GnomeWMDoubleClickAction **actions_p,
				 int                            *n_actions_p)
{
    static GnomeWMDoubleClickAction actions[] = {
	{ DOUBLE_CLICK_MAXIMIZE, "Maximize" }
    };
    static gboolean initialized = FALSE;

    if (!initialized)
    {
	int i = 0;

	initialized = TRUE;
	while (i < (int) G_N_ELEMENTS (actions))
	{
	    g_assert (actions[i].number == i);
	    actions[i].human_readable_name = actions[i].human_readable_name;

	    ++i;
	}
    }

    *actions_p   = actions;
    *n_actions_p = (int) G_N_ELEMENTS (actions);
}

static void
compiz_window_manager_init (CompizWindowManager	     *cwm,
			    CompizWindowManagerClass *class)
{
    cwm->p		   = g_new0 (CompizWindowManagerPrivate, 1);
    cwm->p->gconf	   = gconf_client_get_default ();
    cwm->p->mouse_modifier = NULL;

    gconf_client_add_dir (cwm->p->gconf,
			  "/apps/compiz",
			  GCONF_CLIENT_PRELOAD_ONELEVEL,
			  NULL);

    g_signal_connect (G_OBJECT (cwm->p->gconf),
		      "value_changed",
		      G_CALLBACK (value_changed),
		      cwm);
}

static void
compiz_window_manager_finalize (GObject *object)
{
    CompizWindowManager *cwm;

    g_return_if_fail (object != NULL);
    g_return_if_fail (IS_COMPIZ_WINDOW_MANAGER (object));

    cwm = COMPIZ_WINDOW_MANAGER (object);

    g_signal_handlers_disconnect_by_func (G_OBJECT (cwm->p->gconf),
					  G_CALLBACK (value_changed),
					  cwm);

    g_object_unref (G_OBJECT (cwm->p->gconf));
    g_free (cwm->p);

    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
compiz_window_manager_class_init (CompizWindowManagerClass *class)
{
    GObjectClass	    *object_class;
    GnomeWindowManagerClass *wm_class;

    object_class = G_OBJECT_CLASS (class);
    wm_class	 = GNOME_WINDOW_MANAGER_CLASS (class);

    object_class->finalize = compiz_window_manager_finalize;

    wm_class->change_settings	       = compiz_change_settings;
    wm_class->get_settings	       = compiz_get_settings;
    wm_class->get_settings_mask	       = compiz_get_settings_mask;
    wm_class->get_theme_list	       = compiz_get_theme_list;
    wm_class->get_user_theme_folder    = compiz_get_user_theme_folder;
    wm_class->get_double_click_actions = compiz_get_double_click_actions;

    parent_class = g_type_class_peek_parent (class);
}

GType
compiz_window_manager_get_type (void)
{
    static GType compiz_window_manager_type = 0;

    if (!compiz_window_manager_type)
    {
	static GTypeInfo compiz_window_manager_info = {
	    sizeof (CompizWindowManagerClass),
	    NULL,
	    NULL,
	    (GClassInitFunc) compiz_window_manager_class_init,
	    NULL,
	    NULL,
	    sizeof (CompizWindowManager),
	    0,
	    (GInstanceInitFunc) compiz_window_manager_init,
	    NULL
	};

	compiz_window_manager_type =
	    g_type_register_static (gnome_window_manager_get_type (),
				    "CompizWindowManager",
				    &compiz_window_manager_info, 0);
    }

    return compiz_window_manager_type;
}
