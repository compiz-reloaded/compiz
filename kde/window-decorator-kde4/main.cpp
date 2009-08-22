/*
 * Copyright © 2008 Dennis Kasprzyk <onestone@opencompositing.org>
 * Copyright © 2006 Novell, Inc.
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
#include <KDE/KApplication>
#include <KDE/KCmdLineArgs>
#include <KDE/KAboutData>
#include <KDE/KDebug>
#include <KDE/KLocale>

#include "decorator.h"
#include "utils.h"

#include <QX11Info>
#include <QtDBus/QtDBus>



int
main (int argc, char **argv)
{
    KWD::Decorator  *app;
    KCmdLineArgs    *args;
    KCmdLineOptions options;
    int		    status;
    Time	    timestamp;
    QString         appname;

    options.add ("replace", ki18n ("Replace existing window decorator"));
    options.add ("sm-disable", ki18n ("Disable connection to session manager"));
    options.add ("blur <type>", ki18n ("Blur type (none/titlebar/all)"), "none");

    KAboutData about("kde-window-decorator", "kwin", ki18n ("KDE Window Decorator"),
                     "0.0.1", KLocalizedString(), KAboutData::License_GPL,
                     KLocalizedString(), KLocalizedString(), "http://www.compiz.org", 
		     "dev@lists.compiz-fusion.org");
    KCmdLineArgs::init (argc, argv,
			"kde-window-decorator",
			"kwin",
			ki18n ("KDE Window Decorator"),
			"0.0.1");
    KCmdLineArgs::addCmdLineOptions (options);
    args = KCmdLineArgs::parsedArgs ();

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

    status = decor_acquire_dm_session (QX11Info::display(),
				       QX11Info::appScreen (),
				       "kwd", args->isSet ("replace"),
				       &timestamp);
    if (status != DECOR_ACQUIRE_STATUS_SUCCESS)
    {
	if (status == DECOR_ACQUIRE_STATUS_FAILED)
	{
	    fprintf (stderr,
		     "%s: Could not acquire decoration manager "
		     "selection on screen %d display \"%s\"\n",
		     argv[0], QX11Info::appScreen (),
		     DisplayString (QX11Info::display()));
	}
	else if (status == DECOR_ACQUIRE_STATUS_OTHER_DM_RUNNING)
	{
	    fprintf (stderr,
		     "%s: Screen %d on display \"%s\" already "
		     "has a decoration manager; try using the "
		     "--replace option to replace the current "
		     "decoration manager.\n",
		     argv[0], QX11Info::appScreen (),
		     DisplayString (QX11Info::display()));
	}

	return 1;
    }

    decor_set_dm_check_hint (QX11Info::display(), QX11Info::appScreen ());

    if (!app->enableDecorations (timestamp))
    {
	fprintf (stderr,
		 "%s: Could not enable decorations on display \"%s\"\n",
		 argv[0], DisplayString (QX11Info::display()));

	return 1;
    }

    if (QX11Info::appScreen () == 0)
        appname = "org.kde.kwin";
    else
        appname.sprintf("org.kde.kwin-screen-%d", QX11Info::appScreen ());

    QDBusConnection::sessionBus ().interface ()->registerService
	(appname, QDBusConnectionInterface::DontQueueService);

    status = app->exec ();

    delete app;

    return status;
}
