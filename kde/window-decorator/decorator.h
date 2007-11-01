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

#ifndef _DECORATOR_H
#define _DECORATOR_H

#include <kapplication.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <qtimer.h>

#include <fixx11h.h>
#include <kconfig.h>
#include <kdecoration_plugins_p.h>
#include <kdecoration_p.h>
#include <netwm.h>

#include <decoration.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/connection.h>

#include "window.h"
#include "KWinInterface.h"

#define ROOT_OFF_X 8192
#define ROOT_OFF_Y 8192

#define C(name) { 0, XC_ ## name }

struct _cursor {
    Cursor       cursor;
    unsigned int shape;
};

extern struct _cursor cursors[3][3];

extern double decorationOpacity;
extern bool   decorationOpacityShade;
extern double activeDecorationOpacity;
extern bool   activeDecorationOpacityShade;

#define BLUR_TYPE_NONE     0
#define BLUR_TYPE_TITLEBAR 1
#define BLUR_TYPE_ALL      2

extern int blurType;

class KConfig;
class KWinModule;

namespace KWD
{
    class Options;

class PluginManager:public KDecorationPlugins {
    public:
	PluginManager (KConfig *config);
	virtual bool provides (Requirement)
	{
	    return false;
	}
    };


class Decorator:public KApplication, public KWinInterface {
    Q_OBJECT public:
	Decorator (void);
	~Decorator (void);

	static NETRootInfo *rootInfo (void)
	{
	    return mRootInfo;
	}
	static PluginManager *pluginManager (void)
	{
	    return mPlugins;
	}
	static KWD::Options *options (void)
	{
	    return mOptions;
	}
	static WId activeId (void)
	{
	    return mActiveId;
	}
	static decor_shadow_options_t *shadowOptions (void)
	{
	    return &mShadowOptions;
	}
	static decor_shadow_t *defaultWindowShadow (decor_context_t *context,
						    decor_extents_t *border)
	{
	    if (!mDefaultShadow)
		return NULL;

	    if (memcmp (border, &mDefaultBorder, sizeof (decor_extents_t)) != 0)
		return NULL;

	    *context = mDefaultContext;
	    return mDefaultShadow;
	}
	static void sendClientMessage (WId  eventWid,
				       WId  wid,
				       Atom atom,
				       Atom value,
				       long data1 = 0,
				       long data2 = 0,
				       long data3 = 0);
	static void updateDefaultShadow (KWD::Window *w);

	bool enableDecorations (Time timestamp, int  damageEvent);
	bool x11EventFilter (XEvent *xevent);
	void changeShadowOptions (decor_shadow_options_t *opt);

    public slots:
	void reconfigure (void);

    private:
	DBusMessage *sendAndBlockForShadowOptionReply (const char *path);
	WId fetchFrame (WId window);
	void updateShadow (void);
	void updateAllShadowOptions (void);

    private slots:
	void handleWindowAdded (WId id);
	void handleWindowRemoved (WId id);
	void handleActiveWindowChanged (WId id);
	void handleWindowChanged (WId		      id,
				  const unsigned long *properties);
	void processDamage (void);

    private:
	static PluginManager *mPlugins;
	static KWD::Options *mOptions;
	static decor_extents_t mDefaultBorder;
	static decor_context_t mDefaultContext;
	static decor_shadow_t *mDefaultShadow;
	static decor_shadow_t *mNoBorderShadow;
	static decor_shadow_options_t mShadowOptions;
	static NETRootInfo *mRootInfo;
	static WId mActiveId;

	KWD::Window *mDecorNormal;
	KWD::Window *mDecorActive;
	QMap <WId, KWD::Window *>mClients;
	QMap <WId, KWD::Window *>mFrames;
	QMap <WId, KWD::Window *>mWindows;
	KConfig *mConfig;
	Time mDmSnTimestamp;
	int mDamageEvent;
	QTimer mIdleTimer;
	KWinModule *mKWinModule;
	DBusConnection *mDBusConnection;
	DBusQt::Connection mDBusQtConnection;
	WId mCompositeWindow;
    };
}

#endif
