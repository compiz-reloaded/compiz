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

#include <KDE/KCmdLineArgs>
#include <KDE/KConfig>
#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <kwindowsystem.h>
#include <KDE/KLocale>
#include <KDE/Plasma/Theme>
#include <kcommondecoration.h>
#include <kwindowsystem.h>

#include <QPoint>
#include <QList>
#include <QX11Info>

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#include "decorator.h"
#include "options.h"
#include "utils.h"

#include "kwinadaptor.h"

#include <stdio.h>

#define SHADOW_RADIUS      8.0
#define SHADOW_OPACITY     0.5
#define SHADOW_OFFSET_X    1
#define SHADOW_OFFSET_Y    1
#define SHADOW_COLOR_RED   0x0000
#define SHADOW_COLOR_GREEN 0x0000
#define SHADOW_COLOR_BLUE  0x0000

#define DBUS_DEST           "org.freedesktop.compiz"
#define DBUS_SIGNAL_PATH    "/org/freedesktop/compiz/decoration/display"
#define DBUS_QUERY_PATH     "/org/freedesktop/compiz/decoration/allscreens"
#define DBUS_INTERFACE      "org.freedesktop.compiz"
#define DBUS_METHOD_GET     "get"
#define DBUS_SIGNAL_CHANGED "changed"

int    blurType = BLUR_TYPE_NONE;

decor_shadow_t *KWD::Decorator::mNoBorderShadow = 0;
KWD::PluginManager *KWD::Decorator::mPlugins = 0;
KWD::Options *KWD::Decorator::mOptions = 0;
NETRootInfo *KWD::Decorator::mRootInfo;
WId KWD::Decorator::mActiveId;
decor_shadow_options_t KWD::Decorator::mShadowOptions;

struct _cursor cursors[3][3] = {
    { C (top_left_corner), C (top_side), C (top_right_corner) },
    { C (left_side), C (left_ptr), C (right_side) },
    { C (bottom_left_corner), C (bottom_side), C (bottom_right_corner) }
};

KWD::PluginManager::PluginManager (KSharedConfigPtr config):
    KWD::KDecorationPlugins (config)
{
    defaultPlugin = (QPixmap::defaultDepth() > 8) ?
            "kwin3_oxygen" : "kwin3_plastik";
}


KWD::Decorator::Decorator () :
    KApplication (),
    mConfig (0),
    mCompositeWindow (0),
    mSwitcher (0)
{
    XSetWindowAttributes attr;
    int			 i, j;
    QDBusConnection      dbus = QDBusConnection::sessionBus();

    mRootInfo = new NETRootInfo (QX11Info::display(), 0);

    mActiveId = 0;

    KConfigGroup cfg (KSharedConfig::openConfig("plasmarc"), QString("Theme"));
    Plasma::Theme::defaultTheme ()->setThemeName (cfg.readEntry ("name"));

    Atoms::init ();

    (void *) new KWinAdaptor (this);
    dbus.registerObject ("/KWin", this);
    dbus.connect (QString (), "/KWin", "org.kde.KWin", "reloadConfig", this,
		  SLOT (reconfigure ()));

    dbus.connect (QString (), DBUS_SIGNAL_PATH "/shadow_radius",
		  DBUS_INTERFACE, DBUS_SIGNAL_CHANGED, this,
		  SLOT (shadowRadiusChanged (double)));

    dbus.connect (QString (), DBUS_SIGNAL_PATH "/shadow_opacity",
		  DBUS_INTERFACE, DBUS_SIGNAL_CHANGED, this,
		  SLOT (shadowOpacityChanged (double)));

    dbus.connect (QString (), DBUS_SIGNAL_PATH "/shadow_x_offset",
		  DBUS_INTERFACE, DBUS_SIGNAL_CHANGED, this,
		  SLOT (shadowXOffsetChanged (int)));

    dbus.connect (QString (), DBUS_SIGNAL_PATH "/shadow_y_offset",
		  DBUS_INTERFACE, DBUS_SIGNAL_CHANGED, this,
		  SLOT (shadowYOffsetChanged (int)));

    dbus.connect (QString (), DBUS_SIGNAL_PATH "/shadow_color",
		  DBUS_INTERFACE, DBUS_SIGNAL_CHANGED, this,
		  SLOT (shadowColorChanged (QString)));

    mConfig = new KConfig ("kwinrc");

    mOptions = new KWD::Options (mConfig);
    mPlugins = new PluginManager (KSharedConfig::openConfig("kwinrc"));

    mShadowOptions.shadow_radius   = SHADOW_RADIUS;
    mShadowOptions.shadow_opacity  = SHADOW_OPACITY;
    mShadowOptions.shadow_offset_x = SHADOW_OFFSET_X;
    mShadowOptions.shadow_offset_y = SHADOW_OFFSET_Y;
    mShadowOptions.shadow_color[0] = SHADOW_COLOR_RED;
    mShadowOptions.shadow_color[1] = SHADOW_COLOR_GREEN;
    mShadowOptions.shadow_color[2] = SHADOW_COLOR_BLUE;

    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
	{
	    if (cursors[i][j].shape != XC_left_ptr)
		cursors[i][j].cursor =
		    XCreateFontCursor (QX11Info::display(), cursors[i][j].shape);
	}
    }

    attr.override_redirect = True;

    mCompositeWindow = XCreateWindow (QX11Info::display(), QX11Info::appRootWindow(),
				      -ROOT_OFF_X, -ROOT_OFF_Y, 1, 1, 0,
				      CopyFromParent,
				      CopyFromParent,
				      CopyFromParent,
				      CWOverrideRedirect, &attr);
				      
    long data = 1;
    XChangeProperty (QX11Info::display(), mCompositeWindow, Atoms::enlightmentDesktop,
		      XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &data, 1);

    XCompositeRedirectSubwindows (QX11Info::display(), mCompositeWindow,
				  CompositeRedirectManual);

    XMapWindow (QX11Info::display(), mCompositeWindow);
}

KWD::Decorator::~Decorator (void)
{
    QMap <WId, KWD::Window *>::ConstIterator it;

    for (it = mClients.begin (); it != mClients.end (); it++)
	delete (*it);

    if (mDecorNormal)
	delete mDecorNormal;

    if (mDecorActive)
	delete mDecorActive;

    if (mSwitcher)
	delete mSwitcher;

    XDestroyWindow (QX11Info::display(), mCompositeWindow);

    delete mOptions;
    delete mPlugins;
    delete mConfig;
    delete mRootInfo;
}

bool
KWD::Decorator::enableDecorations (Time timestamp)
{
    QList <WId>::ConstIterator it;

    mDmSnTimestamp = timestamp;

    if (!pluginManager ()->loadPlugin (""))
	return false;

    updateAllShadowOptions ();

    KWD::trapXError ();
    (void) QApplication::desktop (); // trigger creation of desktop widget
    KWD::popXError ();

    updateShadow ();

    mDecorNormal = new KWD::Window (mCompositeWindow, QX11Info::appRootWindow(),
				    0, Window::Default);
    mDecorActive = new KWD::Window (mCompositeWindow, QX11Info::appRootWindow(),
				    0, Window::DefaultActive);

    mActiveId = KWindowSystem::activeWindow ();

    connect (KWindowSystem::self (), SIGNAL (windowAdded (WId)),
	     SLOT (handleWindowAdded (WId)));
    connect (KWindowSystem::self (), SIGNAL (windowRemoved (WId)),
	     SLOT (handleWindowRemoved (WId)));
    connect (KWindowSystem::self (), SIGNAL (activeWindowChanged (WId)),
	     SLOT (handleActiveWindowChanged (WId)));
    connect (KWindowSystem::self (),
	     SIGNAL (windowChanged (WId, const unsigned long *)),
	     SLOT (handleWindowChanged (WId, const unsigned long *)));

    foreach (WId id, KWindowSystem::windows ())
	handleWindowAdded (id);

    connect (Plasma::Theme::defaultTheme (), SIGNAL (themeChanged ()),
	     SLOT (plasmaThemeChanged ()));

    // select for client messages
    XSelectInput (QX11Info::display(), QX11Info::appRootWindow(),
		  StructureNotifyMask | PropertyChangeMask);

    return true;
}

void
KWD::Decorator::updateAllShadowOptions (void)
{
    QDBusInterface       *compiz;
    QDBusReply<QString>  stringReply;
    QDBusReply<double>   doubleReply;
    QDBusReply<int>      intReply;
    int                  c[4];

    compiz = new QDBusInterface (DBUS_DEST, DBUS_QUERY_PATH "/shadow_radius",
				 DBUS_INTERFACE);
    doubleReply = compiz->call (DBUS_METHOD_GET);
    delete compiz;

    if (doubleReply.isValid ())
	mShadowOptions.shadow_radius = doubleReply.value ();

    compiz = new QDBusInterface (DBUS_DEST, DBUS_QUERY_PATH "/shadow_opacity",
				 DBUS_INTERFACE);
    doubleReply = compiz->call (DBUS_METHOD_GET);
    delete compiz;

    if (doubleReply.isValid ())
	mShadowOptions.shadow_opacity = doubleReply.value ();

    compiz = new QDBusInterface (DBUS_DEST, DBUS_QUERY_PATH "/shadow_x_offset",
				 DBUS_INTERFACE);
    intReply = compiz->call (DBUS_METHOD_GET);
    delete compiz;

    if (intReply.isValid ())
	mShadowOptions.shadow_offset_x = intReply.value ();

    compiz = new QDBusInterface (DBUS_DEST, DBUS_QUERY_PATH "/shadow_y_offset",
				 DBUS_INTERFACE);
    intReply = compiz->call (DBUS_METHOD_GET);
    delete compiz;

    if (intReply.isValid ())
	mShadowOptions.shadow_offset_y = intReply.value ();
    else
	mShadowOptions.shadow_offset_y = SHADOW_OFFSET_Y;

    compiz = new QDBusInterface (DBUS_DEST, DBUS_QUERY_PATH "/shadow_color",
				 DBUS_INTERFACE);
    stringReply = compiz->call (DBUS_METHOD_GET);
    delete compiz;

    if (stringReply.isValid () &&
	sscanf (stringReply.value ().toAscii ().data (), "#%2x%2x%2x%2x",
		&c[0], &c[1], &c[2], &c[3]) == 4)
    {
	mShadowOptions.shadow_color[0] = c[0] << 8 | c[0];
	mShadowOptions.shadow_color[1] = c[1] << 8 | c[1];
	mShadowOptions.shadow_color[2] = c[2] << 8 | c[2];
    }
}

void
KWD::Decorator::changeShadowOptions (decor_shadow_options_t *opt)
{
    QMap <WId, KWD::Window *>::ConstIterator it;

    if (!memcmp (opt, &mShadowOptions, sizeof (decor_shadow_options_t)))
	return;

    mShadowOptions = *opt;

    updateShadow ();
}

void
KWD::Decorator::updateShadow (void)
{
    Display	    *xdisplay = QX11Info::display();
    Screen	    *xscreen;
    decor_context_t context;

    xscreen = ScreenOfDisplay (xdisplay, QX11Info::appScreen ());

    if (mNoBorderShadow)
	decor_shadow_destroy (xdisplay, mNoBorderShadow);

    mNoBorderShadow = decor_shadow_create (xdisplay,
					   xscreen,
					   1, 1,
					   0,
					   0,
					   0,
					   0,
					   0, 0, 0, 0,
					   &mShadowOptions,
					   &context,
					   decor_draw_simple,
					   0);

    if (mNoBorderShadow)
    {
	decor_extents_t extents = { 0, 0, 0, 0 };
	long	        data[256];
	decor_quad_t    quads[N_QUADS_MAX];
	int	        nQuad;
	decor_layout_t  layout;

	decor_get_default_layout (&context, 1, 1, &layout);

	nQuad = decor_set_lSrStSbS_window_quads (quads, &context, &layout);

	decor_quads_to_property (data, mNoBorderShadow->pixmap,
				 &extents, &extents,
				 0, 0, quads, nQuad);

	KWD::trapXError ();
	XChangeProperty (QX11Info::display(), QX11Info::appRootWindow(),
			 Atoms::netWindowDecorBare,
			 XA_INTEGER,
			 32, PropModeReplace, (unsigned char *) data,
			 BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
	KWD::popXError ();
    }
}

bool
KWD::Decorator::x11EventFilter (XEvent *xevent)
{
    KWD::Window *client;
    int		status;
    
    switch (xevent->type) {
    case ConfigureNotify: {
	XConfigureEvent *xce = reinterpret_cast <XConfigureEvent *> (xevent);

	if (mFrames.contains (xce->window))
	    mFrames[xce->window]->updateFrame (xce->window);

    } break;
    case SelectionRequest:
	decor_handle_selection_request (QX11Info::display(), xevent, mDmSnTimestamp);
	break;
    case SelectionClear:
	status = decor_handle_selection_clear (QX11Info::display(),
					       xevent, 0);
	if (status == DECOR_SELECTION_GIVE_UP)
	    KApplication::exit (0);

	break;
    case PropertyNotify:
	if (xevent->xproperty.atom == Atoms::netFrameWindow)
	{
	    handleWindowAdded (xevent->xproperty.window);
	}
	else if (xevent->xproperty.atom == Atoms::switchSelectWindow)
	{
	    WId id = xevent->xproperty.window;

	    if (!mSwitcher || mSwitcher->xid () != id)
		handleWindowAdded (id);
	    mSwitcher->update ();
	}
	else if (xevent->xproperty.atom == Atoms::netWmWindowOpacity)
	{
	    if (mClients.contains (xevent->xproperty.window))
		mClients[xevent->xproperty.window]->updateOpacity ();
	}
	break;
    case EnterNotify:
    {
	XCrossingEvent *xce = reinterpret_cast <XCrossingEvent *> (xevent);
	QWidget	       *child;

	if (!mFrames.contains (xce->window))
	    break;

	client = mFrames[xce->window];

	if (!client->decorWidget ())
	    break;

	child = client->childAt (xce->x, xce->y);
	if (child)
	{
	    QEvent qe (QEvent::Enter);

	    QApplication::sendEvent (child, &qe);

	    client->setActiveChild (child);
	    client->updateCursor (QPoint (xce->x, xce->y));
	}
    } break;
    case LeaveNotify:
    {
	XCrossingEvent *xce = reinterpret_cast <XCrossingEvent *> (xevent);

	if (mFrames.contains (xce->window))
	{
	    QEvent qe (QEvent::Leave);

	    client = mFrames[xce->window];

	    if (client->activeChild ())
		QApplication::sendEvent (client->activeChild (), &qe);

	    XUndefineCursor (QX11Info::display(), client->frameId ());
	}
    } break;
    case MotionNotify:
    {
	XMotionEvent *xme = reinterpret_cast < XMotionEvent * >(xevent);
	QWidget	     *child;

	if (!mFrames.contains (xme->window))
	    break;

	client = mFrames[xme->window];

	if (!client->decorWidget ())
	    break;

	child = client->childAt (xme->x, xme->y);

	if (child)
	{
	    QPoint qp (xme->x, xme->y);

	    if (child != client->activeChild ())
	    {
		QEvent qee (QEvent::Enter);
		QEvent qle (QEvent::Leave);

		if (client->activeChild ())
		    QApplication::sendEvent (client->activeChild (), &qle);

		QApplication::sendEvent (child, &qee);

		client->setActiveChild (child);
	    }

	    if (client->decorWidget () != child)
		qp = child->mapFrom (client->decorWidget (), qp);

	    QMouseEvent qme (QEvent::MouseMove, qp, Qt::NoButton,
			     Qt::NoButton, Qt::NoModifier);

	    QApplication::sendEvent (child, &qme);

	    client->updateCursor (QPoint (xme->x, xme->y));
	}
    } break;
    case ButtonPress:
    case ButtonRelease:
    {
	XButtonEvent *xbe = reinterpret_cast <XButtonEvent *>(xevent);
	QWidget	     *child;

	if (!mFrames.contains (xbe->window))
	    break;

	client = mFrames[xbe->window];

	if (!client->decorWidget ())
	    break;

	child = client->childAt (xbe->x, xbe->y);

	if (child)
	{
	    XButtonEvent xbe2 = *xbe;
	    xbe2.window = child->winId ();

	    QPoint p;
		
	    p = client->mapToChildAt (QPoint (xbe->x, xbe->y));
	    xbe2.x = p.x ();
	    xbe2.y = p.y ();
	    
	    p = child->mapToGlobal(p);
	    xbe2.x_root = p.x ();
	    xbe2.y_root = p.y ();

	    client->setFakeRelease (false);
	    QApplication::x11ProcessEvent ((XEvent *) &xbe2);

	    /* We won't get a button release event, because of the screengrabs
	       in compiz */
	    if (client->getFakeRelease () && xevent->type == ButtonPress)
	    {
		xbe2.type = ButtonRelease;
		QApplication::x11ProcessEvent ((XEvent *) &xbe2);
	    }

	    return true;
	}
    } break;
    case ClientMessage:
	if (xevent->xclient.message_type == Atoms::toolkitActionAtom)
	{
	    unsigned long action;

	    action = xevent->xclient.data.l[0];
	    if (action == Atoms::toolkitActionWindowMenuAtom)
	    {
		if (mClients.contains (xevent->xclient.window))
		{
		    QPoint pos;

		    client = mClients[xevent->xclient.window];

		    if (xevent->xclient.data.l[2])
		    {
			pos = QPoint (xevent->xclient.data.l[3],
				      xevent->xclient.data.l[4]);
		    }
		    else
		    {
			pos = client->clientGeometry ().topLeft ();
		    }

		    client->showWindowMenu (pos);
		}
	    }
	    else if (action == Atoms::toolkitActionForceQuitDialogAtom)
	    {
		if (mClients.contains (xevent->xclient.window))
		{
		    Time timestamp = xevent->xclient.data.l[1];

		    client = mClients[xevent->xclient.window];

		    if (xevent->xclient.data.l[2])
			client->showKillProcessDialog (timestamp);
		    else
			client->hideKillProcessDialog ();
		}
	    }
	}
	break;
    default:
	break;
    }

    return KApplication::x11EventFilter (xevent);
}

void
KWD::Decorator::reconfigure (void)
{
    unsigned long changed;

    mConfig->reparseConfiguration ();

    changed = mOptions->updateSettings ();
    if (mPlugins->reset (changed))
    {
	QMap < WId, KWD::Window * >::ConstIterator it;

	updateShadow ();

	mDecorNormal->reloadDecoration ();
	mDecorActive->reloadDecoration ();

	for (it = mClients.constBegin (); it != mClients.constEnd (); it++)
	    it.value ()->reloadDecoration ();

	mPlugins->destroyPreviousPlugin ();
    }
}

void
KWD::Decorator::handleWindowAdded (WId id)
{
    QMap <WId, KWD::Window *>::ConstIterator it;
    KWD::Window				     *client = 0;
    WId					     select, frame = 0;
    KWD::Window::Type			     type = KWD::Window::Normal;
    unsigned int			     width, height, border, depth;
    int					     x, y;
    XID					     root;
    QWidgetList				     widgets;

    /* avoid adding any of our own top level windows */
    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
        if (widget->winId() == id)
	    return;
    }

    KWD::trapXError ();
    XGetGeometry (QX11Info::display(), id, &root, &x, &y, &width, &height,
		  &border, &depth);
    if (KWD::popXError ())
	return;

    KWD::readWindowProperty (id, Atoms::netFrameWindow, (long *) &frame);

    if (KWD::readWindowProperty (id, Atoms::switchSelectWindow,
				 (long *) &select))
    {
	if (!mSwitcher)
            mSwitcher = new Switcher (mCompositeWindow, id);
        if (mSwitcher->xid () != id)
        {
            delete mSwitcher;
            mSwitcher = new Switcher (mCompositeWindow, id);
        }
	frame = None;
    }
    else
    {
	KWindowInfo wInfo = KWindowSystem::windowInfo (id, NET::WMWindowType, 0);

	switch (wInfo.windowType (~0)) {
	case NET::Normal:
	case NET::Dialog:
	case NET::Toolbar:
	case NET::Menu:
	case NET::Utility:
	case NET::Splash:
	case NET::Unknown:
	    /* decorate these window types */
	    break;
	default:
	    return;
	}

	type = KWD::Window::Normal;
    }

    KWD::trapXError ();
    XSelectInput (QX11Info::display(), id, StructureNotifyMask | PropertyChangeMask);
    KWD::popXError ();

    if (frame)
    {
	if (!mClients.contains (id))
	{
	    client = new KWD::Window (mCompositeWindow, id, frame, type,
				      x, y,
				      width + border * 2,
				      height + border * 2);

	    mClients.insert (id, client);
	    mFrames.insert (frame, client);
	}
	else
	{
	    client = mClients[id];
	    mFrames.remove (client->frameId ());
	    mFrames.insert (frame, client);

	    client->updateFrame (frame);
	}
    }
    else
    {
	if (mClients.contains (id))
	    client = mClients[id];

	if (client)
	{
	    mClients.remove (client->windowId ());
	    mFrames.remove (client->frameId ());

	    delete client;
	}
    }
}

void
KWD::Decorator::handleWindowRemoved (WId id)
{
    KWD::Window *window = 0;

    if (mClients.contains (id))
	window = mClients[id];
    else if (mFrames.contains (id))
	window = mFrames[id];

    if (window)
    {
	mClients.remove (window->windowId ());
	mFrames.remove (window->frameId ());
	delete window;
    }

    if (mSwitcher && mSwitcher->xid () == id)
    {
	delete mSwitcher;
	mSwitcher = NULL;
    }
}

void
KWD::Decorator::handleActiveWindowChanged (WId id)
{
    if (id != mActiveId)
    {
	KWD::Window *newActiveWindow = 0;
	KWD::Window *oldActiveWindow = 0;

	if (mClients.contains (id))
	    newActiveWindow = mClients[id];

	if (mClients.contains (mActiveId))
	    oldActiveWindow = mClients[mActiveId];

	mActiveId = id;

	if (oldActiveWindow)
	    oldActiveWindow->handleActiveChange ();

	if (newActiveWindow)
	    newActiveWindow->handleActiveChange ();
    }
}

void
KWD::Decorator::handleWindowChanged (WId		 id,
				     const unsigned long *properties)
{
    KWD::Window *client;

    if (mSwitcher && mSwitcher->xid () == id)
    {
	mSwitcher->updateGeometry ();
	return;
    }

    if (!mClients.contains (id))
	return;

    client = mClients[id];

    if (properties[0] & NET::WMName)
	client->updateName ();
    if (properties[0] & NET::WMVisibleName)
	client->updateName ();
    if (properties[0] & NET::WMState)
	client->updateState ();
    if (properties[0] & NET::WMIcon)
	client->updateIcons ();
    if (properties[0] & NET::WMGeometry)
	client->updateWindowGeometry ();
	
}

void
KWD::Decorator::sendClientMessage (WId  eventWid,
				   WId  wid,
				   Atom atom,
				   Atom value,
				   long data1,
				   long data2,
				   long data3)
{
    XEvent ev;
    long   mask = 0;

    memset (&ev, 0, sizeof (ev));

    ev.xclient.type	    = ClientMessage;
    ev.xclient.window	    = wid;
    ev.xclient.message_type = atom;
    ev.xclient.format       = 32;

    ev.xclient.data.l[0] = value;
    ev.xclient.data.l[1] = QX11Info::appTime();
    ev.xclient.data.l[2] = data1;
    ev.xclient.data.l[3] = data2;
    ev.xclient.data.l[4] = data3;

    if (eventWid == QX11Info::appRootWindow())
	mask = SubstructureRedirectMask | SubstructureNotifyMask;

    KWD::trapXError ();
    XSendEvent (QX11Info::display(), eventWid, false, mask, &ev);
    KWD::popXError ();
}

void
KWD::Decorator::shadowRadiusChanged (double value)
{
    decor_shadow_options_t opt = *shadowOptions ();

    opt.shadow_radius = value;

    changeShadowOptions (&opt);
}

void
KWD::Decorator::shadowOpacityChanged (double value)
{
    decor_shadow_options_t opt = *shadowOptions ();

    opt.shadow_opacity = value;

    changeShadowOptions (&opt);
}

void
KWD::Decorator::shadowXOffsetChanged (int value)
{
    decor_shadow_options_t opt = *shadowOptions ();

    opt.shadow_offset_x = value;

    changeShadowOptions (&opt);
}

void
KWD::Decorator::shadowYOffsetChanged (int value)
{
    decor_shadow_options_t opt = *shadowOptions ();

    opt.shadow_offset_y = value;

    changeShadowOptions (&opt);
}

void
KWD::Decorator::shadowColorChanged (QString value)
{
    decor_shadow_options_t opt = *shadowOptions ();

    int c[4];

    if (sscanf (value.toAscii().data(), "#%2x%2x%2x%2x",
	        &c[0], &c[1], &c[2], &c[3]) == 4)
    {
	opt.shadow_color[0] = c[0] << 8 | c[0];
	opt.shadow_color[1] = c[1] << 8 | c[1];
	opt.shadow_color[2] = c[2] << 8 | c[2];
    }

    changeShadowOptions (&opt);
}

void
KWD::Decorator::plasmaThemeChanged ()
{
    if (mSwitcher)
    {
	WId win = mSwitcher->xid();
	delete mSwitcher;
	mSwitcher = new Switcher (mCompositeWindow, win);
    }
}
