/*
 * Copyright © 2006 Novell, Inc.
 * Copyright © 2006 Dennis Kasprzyk <onestone@beryl-project.org>
 * Copyright © 2006 Volker Krause <vkrause@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>

#include <fixx11h.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <klocale.h>

#include "decorator.h"

static const KCmdLineOptions options[] = {
    { "replace", "Replace existing window decorator", 0 },
    { "sm-disable", "Disable connection to session manager", 0 },
    { "opacity <value>", "Decoration opacity", "0.75" },
    { "no-opacity-shade", "No decoration opacity shading", 0 },
    { "active-opacity <value>", "Active decoration opacity", "1.0" },
    { "no-active-opacity-shade", "No active decoration opacity shading", 0 },
    { "blur <type>", "Blur type", "none" },
    KCmdLineLastOption
};

#include <kdebug.h>
int
main (int argc, char **argv)
{
    KWD::Decorator *app;
    KCmdLineArgs   *args;
    int		   status;
    int		   event, error;
    Time	   timestamp;

    KLocale::setMainCatalogue("kwin");

    KCmdLineArgs::init (argc, argv,
			"kde-window-decorator",
			"KWD",
			"KDE Window Decorator",
			"0.0.1");
    KCmdLineArgs::addCmdLineOptions (options);
    args = KCmdLineArgs::parsedArgs ();

    if (args->isSet ("opacity"))
	decorationOpacity = args->getOption ("opacity").toDouble ();

    if (args->isSet ("-opacity-shade"))
	decorationOpacityShade = true;

    if (args->isSet ("active-opacity"))
	activeDecorationOpacity =
	    args->getOption ("active-opacity").toDouble ();

    if (args->isSet ("-active-opacity-shade"))
	activeDecorationOpacityShade = true;

    if (args->isSet ("blur"))
    {
	QString blur = args->getOption ("blur");

	if (blur == QString ("titlebar"))
	    blurType = BLUR_TYPE_TITLEBAR;
	else if (blur == QString ("all"))
	    blurType = BLUR_TYPE_ALL;
    }

    app = new KWD::Decorator ();

    if (args->isSet ("sm-disable"))
	app->disableSessionManagement ();

    if (!XDamageQueryExtension (qt_xdisplay (), &event, &error))
    {
	fprintf (stderr,
		 "%s: Damage extension is missing on display \"%s\"\n",
		 argv[0], DisplayString (qt_xdisplay ()));

	return 1;
    }

    status = decor_acquire_dm_session (qt_xdisplay (), qt_xscreen (), "kwd",
				       args->isSet ("replace"),
				       &timestamp);
    if (status != DECOR_ACQUIRE_STATUS_SUCCESS)
    {
	if (status == DECOR_ACQUIRE_STATUS_FAILED)
	{
	    fprintf (stderr,
		     "%s: Could not acquire decoration manager "
		     "selection on screen %d display \"%s\"\n",
		     argv[0], qt_xscreen (), DisplayString (qt_xdisplay ()));
	}
	else if (status == DECOR_ACQUIRE_STATUS_OTHER_DM_RUNNING)
	{
	    fprintf (stderr,
		     "%s: Screen %d on display \"%s\" already "
		     "has a decoration manager; try using the "
		     "--replace option to replace the current "
		     "decoration manager.\n",
		     argv[0], qt_xscreen (), DisplayString (qt_xdisplay ()));
	}

	return 1;
    }

    decor_set_dm_check_hint (qt_xdisplay (), qt_xscreen ());

    if (!app->enableDecorations (timestamp, event))
    {
	fprintf (stderr,
		 "%s: Could not enable decorations on display \"%s\"\n",
		 argv[0], DisplayString (qt_xdisplay ()));

	return 1;
    }

    status = app->exec ();

    delete app;

    return status;
}
