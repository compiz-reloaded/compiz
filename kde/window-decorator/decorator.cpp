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

#include <qglobal.h>

#include <dcopclient.h>
#include <kcmdlineargs.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kwinmodule.h>
#include <klocale.h>
#include <kcommondecoration.h>
#include <kwin.h>
#include <qwidgetlist.h>
#include <qpoint.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

#include "decorator.h"
#include "options.h"
#include "utils.h"

#include <stdio.h>

#define SHADOW_RADIUS      8.0
#define SHADOW_OPACITY     0.5
#define SHADOW_OFFSET_X    1
#define SHADOW_OFFSET_Y    1
#define SHADOW_COLOR_RED   0x0000
#define SHADOW_COLOR_GREEN 0x0000
#define SHADOW_COLOR_BLUE  0x0000

#define DBUS_DEST       "org.freedesktop.compiz"
#define DBUS_PATH       "/org/freedesktop/compiz/decoration/display"
#define DBUS_INTERFACE  "org.freedesktop.compiz"
#define DBUS_METHOD_GET "get"

double decorationOpacity = 0.75;
bool   decorationOpacityShade = false;
double activeDecorationOpacity = 1.0;
bool   activeDecorationOpacityShade = false;
int    blurType = BLUR_TYPE_NONE;

decor_context_t KWD::Decorator::mDefaultContext;
decor_extents_t KWD::Decorator::mDefaultBorder;
decor_shadow_t *KWD::Decorator::mNoBorderShadow = 0;
decor_shadow_t *KWD::Decorator::mDefaultShadow  = 0;
KWD::PluginManager *KWD::Decorator::mPlugins = 0;
KWD::Options *KWD::Decorator::mOptions = 0;
NETRootInfo *KWD::Decorator::mRootInfo;
WId KWD::Decorator::mActiveId;
decor_shadow_options_t KWD::Decorator::mShadowOptions;

extern Time qt_x_time;

struct _cursor cursors[3][3] = {
    { C (top_left_corner), C (top_side), C (top_right_corner) },
    { C (left_side), C (left_ptr), C (right_side) },
    { C (bottom_left_corner), C (bottom_side), C (bottom_right_corner) }
};

KWD::PluginManager::PluginManager (KConfig *config): KDecorationPlugins (config)
{
    defaultPlugin = "kwin3_plastik";
}

static DBusHandlerResult
dbusHandleMessage (DBusConnection *connection,
		   DBusMessage    *message,
		   void           *userData)
{
    KWD::Decorator    *d = (KWD::Decorator *) userData;
    char	      **path;
    const char        *interface, *member;
    DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    interface = dbus_message_get_interface (message);
    member    = dbus_message_get_member (message);

    (void) connection;

    if (!interface || !member)
	return result;

    if (!dbus_message_is_signal (message, interface, member))
	return result;

    if (strcmp (member, "changed"))
	return result;

    if (!dbus_message_get_path_decomposed (message, &path))
	return result;

    if (!path[0] || !path[1] || !path[2] || !path[3] || !path[4] || !path[5])
    {
	dbus_free_string_array (path);
	return result;
    }

    if (!strcmp (path[0], "org")	 &&
	!strcmp (path[1], "freedesktop") &&
	!strcmp (path[2], "compiz")      &&
	!strcmp (path[3], "decoration")  &&
	!strcmp (path[4], "display"))
    {
	decor_shadow_options_t opt = *d->shadowOptions ();

	result = DBUS_HANDLER_RESULT_HANDLED;

	if (strcmp (path[5], "shadow_radius") == 0)
	{
	    dbus_message_get_args (message, NULL,
				   DBUS_TYPE_DOUBLE, &opt.shadow_radius,
				   DBUS_TYPE_INVALID);
	}
	else if (strcmp (path[5], "shadow_opacity") == 0)
	{
	    dbus_message_get_args (message, NULL,
				   DBUS_TYPE_DOUBLE, &opt.shadow_opacity,
				   DBUS_TYPE_INVALID);
	}
	else if (strcmp (path[5], "shadow_color") == 0)
	{
	    DBusError error;
	    char      *str;

	    dbus_error_init (&error);

	    dbus_message_get_args (message, &error,
				   DBUS_TYPE_STRING, &str,
				   DBUS_TYPE_INVALID);

	    if (!dbus_error_is_set (&error))
	    {
		int c[4];

		if (sscanf (str, "#%2x%2x%2x%2x",
			    &c[0], &c[1], &c[2], &c[3]) == 4)
		{
		    opt.shadow_color[0] = c[0] << 8 | c[0];
		    opt.shadow_color[1] = c[1] << 8 | c[1];
		    opt.shadow_color[2] = c[2] << 8 | c[2];
		}
	    }

	    dbus_error_free (&error);
	}
	else if (strcmp (path[5], "shadow_x_offset") == 0)
	{
	    dbus_message_get_args (message, NULL,
				   DBUS_TYPE_INT32, &opt.shadow_offset_x,
				   DBUS_TYPE_INVALID);
	}
	else if (strcmp (path[5], "shadow_y_offset") == 0)
	{
	    dbus_message_get_args (message, NULL,
				   DBUS_TYPE_INT32, &opt.shadow_offset_y,
				   DBUS_TYPE_INVALID);
	}

	d->changeShadowOptions (&opt);
    }

    dbus_free_string_array (path);

    return result;
}

KWD::Decorator::Decorator (void) : DCOPObject ("KWinInterface"),
    KApplication (),
    mConfig (0),
    mKWinModule (new KWinModule (this, KWinModule::INFO_ALL)),
    mDBusQtConnection (this),
    mCompositeWindow (0)
{
    XSetWindowAttributes attr;
    DCOPClient		 *client;
    int			 i, j;

    mRootInfo = new NETRootInfo (qt_xdisplay (), 0);

    mActiveId = 0;

    Atoms::init ();

    mConfig = new KConfig ("kwinrc");
    mConfig->setGroup ("Style");

    mOptions = new KWD::Options (mConfig);
    mPlugins = new PluginManager (mConfig);

    for (i = 0; i < 3; i++)
    {
	for (j = 0; j < 3; j++)
	{
	    if (cursors[i][j].shape != XC_left_ptr)
		cursors[i][j].cursor =
		    XCreateFontCursor (qt_xdisplay (), cursors[i][j].shape);
	}
    }

    client = dcopClient ();
    client->registerAs ("kwin", false);
    client->setDefaultObject ("KWinInterface");

    mShadowOptions.shadow_radius   = SHADOW_RADIUS;
    mShadowOptions.shadow_opacity  = SHADOW_OPACITY;
    mShadowOptions.shadow_offset_x = SHADOW_OFFSET_X;
    mShadowOptions.shadow_offset_y = SHADOW_OFFSET_Y;
    mShadowOptions.shadow_color[0] = SHADOW_COLOR_RED;
    mShadowOptions.shadow_color[1] = SHADOW_COLOR_GREEN;
    mShadowOptions.shadow_color[2] = SHADOW_COLOR_BLUE;

    attr.override_redirect = True;

    mCompositeWindow = XCreateWindow (qt_xdisplay (), qt_xrootwin (),
				      -ROOT_OFF_X, -ROOT_OFF_Y, 1, 1, 0,
				      CopyFromParent,
				      CopyFromParent,
				      CopyFromParent,
				      CWOverrideRedirect, &attr);

    XCompositeRedirectSubwindows (qt_xdisplay (), mCompositeWindow,
				  CompositeRedirectManual);

    XMapWindow (qt_xdisplay (), mCompositeWindow);
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

    XDestroyWindow (qt_xdisplay (), mCompositeWindow);

    delete mOptions;
    delete mPlugins;
    delete mConfig;
    delete mKWinModule;
    delete mRootInfo;
}

bool
KWD::Decorator::enableDecorations (Time timestamp,
				   int  damageEvent)
{
    QValueList <WId>::ConstIterator it;
    DBusError			    error;

    mDmSnTimestamp = timestamp;
    mDamageEvent   = damageEvent;

    if (!pluginManager ()->loadPlugin (""))
	return false;

    dbus_error_init (&error);

    mDBusConnection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (!dbus_error_is_set (&error))
    {
	dbus_bus_add_match (mDBusConnection, "type='signal'", &error);

	dbus_connection_add_filter (mDBusConnection,
				    dbusHandleMessage,
				    this, NULL);

	mDBusQtConnection.dbus_connection_setup_with_qt_main (mDBusConnection);

	updateAllShadowOptions ();
    }

    dbus_error_free (&error);

    updateShadow ();

    mDecorNormal = new KWD::Window (mCompositeWindow, qt_xrootwin (),
				    0, Window::Default);
    mDecorActive = new KWD::Window (mCompositeWindow, qt_xrootwin (),
				    0, Window::DefaultActive);

    connect (mKWinModule, SIGNAL (windowAdded (WId)),
	     SLOT (handleWindowAdded (WId)));
    connect (mKWinModule, SIGNAL (windowRemoved (WId)),
	     SLOT (handleWindowRemoved (WId)));
    connect (mKWinModule, SIGNAL (activeWindowChanged (WId)),
	     SLOT (handleActiveWindowChanged (WId)));
    connect (mKWinModule,
	     SIGNAL (windowChanged (WId, const unsigned long *)),
	     SLOT (handleWindowChanged (WId, const unsigned long *)));

    connect (&mIdleTimer, SIGNAL (timeout ()), SLOT (processDamage ()));

    mActiveId = mKWinModule->activeWindow ();

    it = mKWinModule->windows ().begin ();
    for (; it != mKWinModule->windows ().end (); it++)
	handleWindowAdded ((*it));

    connect (this, SIGNAL (appearanceChanged ()), SLOT (reconfigure ()));

    (void) QApplication::desktop (); // trigger creation of desktop widget

    // select for client messages
    XSelectInput (qt_xdisplay(), qt_xrootwin (),
		  StructureNotifyMask | PropertyChangeMask);

    return true;
}

void
KWD::Decorator::updateDefaultShadow (KWD::Window *w)
{
    bool uniqueHorzShape, uniqueVertShape;

    if (mDefaultShadow)
    {
	decor_shadow_destroy (qt_xdisplay (), mDefaultShadow);
	mDefaultShadow = NULL;
    }

    w->getShapeInfo (&uniqueHorzShape, &uniqueVertShape);

    /* only return shadow if decoration doesn't use a unique shape */
    if (uniqueHorzShape || uniqueVertShape)
	return;

    mDefaultContext = *w->context ();
    mDefaultBorder  = *w->border ();
    mDefaultShadow  = w->shadow ();

    if (mDefaultShadow)
	decor_shadow_reference (mDefaultShadow);
}

DBusMessage *
KWD::Decorator::sendAndBlockForShadowOptionReply (const char *path)
{
    DBusMessage *message;

    message = dbus_message_new_method_call (NULL,
					    path,
					    DBUS_INTERFACE,
					    DBUS_METHOD_GET);
    if (message)
    {
	DBusMessage *reply;
	DBusError   error;

	dbus_message_set_destination (message, DBUS_DEST);

	dbus_error_init (&error);
	reply = dbus_connection_send_with_reply_and_block (mDBusConnection,
							   message, -1,
							   &error);
	dbus_message_unref (message);

	if (!dbus_error_is_set (&error))
	    return reply;
    }

    return NULL;
}

void
KWD::Decorator::updateAllShadowOptions (void)
{
    DBusMessage *reply;

    reply = sendAndBlockForShadowOptionReply (DBUS_PATH "/shadow_radius");
    if (reply)
    {
	dbus_message_get_args (reply, NULL,
			       DBUS_TYPE_DOUBLE, &mShadowOptions.shadow_radius,
			       DBUS_TYPE_INVALID);

	dbus_message_unref (reply);
    }

    reply = sendAndBlockForShadowOptionReply (DBUS_PATH "/shadow_opacity");
    if (reply)
    {
	dbus_message_get_args (reply, NULL,
			       DBUS_TYPE_DOUBLE, &mShadowOptions.shadow_opacity,
			       DBUS_TYPE_INVALID);
	dbus_message_unref (reply);
    }

    reply = sendAndBlockForShadowOptionReply (DBUS_PATH "/shadow_color");
    if (reply)
    {
	DBusError error;
	char      *str;

	dbus_error_init (&error);

	dbus_message_get_args (reply, &error,
			       DBUS_TYPE_STRING, &str,
			       DBUS_TYPE_INVALID);

	if (!dbus_error_is_set (&error))
	{
	    int c[4];

	    if (sscanf (str, "#%2x%2x%2x%2x", &c[0], &c[1], &c[2], &c[3]) == 4)
	    {
		mShadowOptions.shadow_color[0] = c[0] << 8 | c[0];
		mShadowOptions.shadow_color[1] = c[1] << 8 | c[1];
		mShadowOptions.shadow_color[2] = c[2] << 8 | c[2];
	    }
	}

	dbus_error_free (&error);

	dbus_message_unref (reply);
    }

    reply = sendAndBlockForShadowOptionReply (DBUS_PATH "/shadow_x_offset");
    if (reply)
    {
	dbus_message_get_args (reply, NULL,
			       DBUS_TYPE_INT32, &mShadowOptions.shadow_offset_x,
			       DBUS_TYPE_INVALID);
	dbus_message_unref (reply);
    }

    reply = sendAndBlockForShadowOptionReply (DBUS_PATH "/shadow_y_offset");
    if (reply)
    {
	dbus_message_get_args (reply, NULL,
			       DBUS_TYPE_INT32, &mShadowOptions.shadow_offset_y,
			       DBUS_TYPE_INVALID);
	dbus_message_unref (reply);
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

    mDecorNormal->reloadDecoration ();
    mDecorActive->reloadDecoration ();

    for (it = mClients.constBegin (); it != mClients.constEnd (); it++)
	it.data ()->reloadDecoration ();
}

void
KWD::Decorator::updateShadow (void)
{
    Display	    *xdisplay = qt_xdisplay ();
    Screen	    *xscreen = ScreenOfDisplay (xdisplay, qt_xscreen ());
    decor_context_t context;

    if (mDefaultShadow)
    {
	decor_shadow_destroy (xdisplay, mDefaultShadow);
	mDefaultShadow = NULL;
    }

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
	XChangeProperty (qt_xdisplay (), qt_xrootwin (),
			 Atoms::netWindowDecorBare,
			 XA_INTEGER,
			 32, PropModeReplace, (unsigned char *) data,
			 BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
	KWD::popXError ();
    }
}

void
KWD::Decorator::processDamage (void)
{
    QMap <WId, KWD::Window *>::ConstIterator it;

    mDecorNormal->processDamage ();
    mDecorActive->processDamage ();

    for (it = mClients.constBegin (); it != mClients.constEnd (); it++)
	it.data ()->processDamage ();
}

bool
KWD::Decorator::x11EventFilter (XEvent *xevent)
{
    KWD::Window *client;
    int		status;

    switch (xevent->type) {
    case MapNotify: {
	XMapEvent *xme = reinterpret_cast <XMapEvent *> (xevent);

	if (mWindows.contains (xme->window))
	    client = mWindows[xme->window];
	else if (mDecorNormal->winId () == xme->window)
	    client = mDecorNormal;
	else if (mDecorActive->winId () == xme->window)
	    client = mDecorActive;
	else
	    break;

	if (client->handleMap ())
	{
	    if (!mIdleTimer.isActive ())
		mIdleTimer.start (0, TRUE);
	}
    } break;
    case ConfigureNotify: {
	XConfigureEvent *xce = reinterpret_cast <XConfigureEvent *> (xevent);

	if (mFrames.contains (xce->window))
	    mFrames[xce->window]->updateFrame (xce->window);

	if (mWindows.contains (xce->window))
	    client = mWindows[xce->window];
	else if (mDecorNormal->winId () == xce->window)
	    client = mDecorNormal;
	else if (mDecorActive->winId () == xce->window)
	    client = mDecorActive;
	else
	    break;

	if (client->handleConfigure (QSize (xce->width, xce->height)))
	{
	    if (!mIdleTimer.isActive ())
		mIdleTimer.start (0, TRUE);
	}
    } break;
    case SelectionRequest:
	decor_handle_selection_request (qt_xdisplay (), xevent, mDmSnTimestamp);
	break;
    case SelectionClear:
	status = decor_handle_selection_clear (qt_xdisplay (),
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
	    if (!mClients.contains (xevent->xproperty.window))
	    {
		handleWindowAdded (xevent->xproperty.window);
	    }
	    else
	    {
		WId id;

		if (KWD::readWindowProperty (xevent->xproperty.window,
					     Atoms::switchSelectWindow,
					     (long *) &id))
		    mClients[xevent->xproperty.window]->updateSelected (id);
	    }
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
	QWidget	       *widget, *child;

	if (!mFrames.contains (xce->window))
	    break;

	client = mFrames[xce->window];

	widget = client->decoration ()->widget ();
	child = widget->childAt (xce->x, xce->y, true);
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

	    QApplication::sendEvent (client->activeChild (), &qe);

	    XUndefineCursor (qt_xdisplay (), client->frameId ());
	}
    } break;
    case MotionNotify:
    {
	XMotionEvent *xme = reinterpret_cast < XMotionEvent * >(xevent);
	QWidget	     *widget, *child;

	if (!mFrames.contains (xme->window))
	    break;

	client = mFrames[xme->window];

	widget = client->decoration ()->widget ();
	child = widget->childAt (xme->x, xme->y, true);
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

	    if (widget != child)
		qp -= QPoint (child->pos ().x (), child->pos ().y ());

	    QMouseEvent qme (QEvent::MouseMove, qp, Qt::NoButton, Qt::NoButton);

	    QApplication::sendEvent (child, &qme);

	    client->updateCursor (QPoint (xme->x, xme->y));
	}
    } break;
    case ButtonPress:
    case ButtonRelease:
    {
	XButtonEvent *xbe = reinterpret_cast <XButtonEvent *>(xevent);
	QWidget	     *widget, *child;

	if (!mFrames.contains (xbe->window))
	    break;

	client = mFrames[xbe->window];

	widget = client->decoration ()->widget ();
	child = widget->childAt (xbe->x, xbe->y, true);

	if (child)
	{
	    XButtonEvent xbe2 = *xbe;

	    xbe2.window = child->winId ();
	    if (widget != child)
	    {
		xbe2.x = xbe->x - child->pos ().x ();
		xbe2.y = xbe->y - child->pos ().y ();
	    }

	    QApplication::x11ProcessEvent ((XEvent *) &xbe2);

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
	if (xevent->type == mDamageEvent + XDamageNotify)
	{
	    XDamageNotifyEvent *xde =
		reinterpret_cast <XDamageNotifyEvent *>(xevent);

	    if (mWindows.contains (xde->drawable))
		client = mWindows[xde->drawable];
	    else if (mDecorNormal->winId () == xde->drawable)
		client = mDecorNormal;
	    else if (mDecorActive->winId () == xde->drawable)
		client = mDecorActive;
	    else
		break;

	    client->addDamageRect (xde->area.x,
				   xde->area.y,
				   xde->area.width,
				   xde->area.height);

	    if (client->pixmapId ())
	    {
		if (!mIdleTimer.isActive ())
		    mIdleTimer.start (0, TRUE);
	    }

	    return true;
	}
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
	    it.data ()->reloadDecoration ();

	mPlugins->destroyPreviousPlugin ();
    }
}

void
KWD::Decorator::handleWindowAdded (WId id)
{
    QMap <WId, KWD::Window *>::ConstIterator it;
    KWD::Window				     *client = 0;
    WId					     select, frame = 0;
    KWD::Window::Type			     type;
    unsigned int			     width, height, border, depth;
    int					     x, y;
    XID					     root;
    QWidgetList				     *widgets;

    /* avoid adding any of our own top level windows */
    widgets = QApplication::topLevelWidgets ();
    if (widgets)
    {
	for (QWidgetListIt it (*widgets); it.current (); ++it)
	{
	    if (it.current ()->winId () == id)
	    {
		delete widgets;
		return;
	    }
	}

	delete widgets;
    }

    KWD::trapXError ();
    XGetGeometry (qt_xdisplay (), id, &root, &x, &y, &width, &height,
		  &border, &depth);
    if (KWD::popXError ())
	return;

    KWD::readWindowProperty (id, Atoms::netFrameWindow, (long *) &frame);
    if (KWD::readWindowProperty (id, Atoms::switchSelectWindow,
				 (long *) &select))
    {
	type = KWD::Window::Switcher;
    }
    else
    {
	KWin::WindowInfo wInfo = KWin::windowInfo (id, NET::WMWindowType, 0);

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
    XSelectInput (qt_xdisplay (), id, StructureNotifyMask | PropertyChangeMask);
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
	    mWindows.insert (client->winId (), client);
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
    else if (type == KWD::Window::Switcher)
    {
	if (!mClients.contains (id))
	{
	    client = new KWD::Window (mCompositeWindow, id, 0, type,
				      x, y,
				      width + border * 2,
				      height + border * 2);
	    mClients.insert (id, client);
	    mWindows.insert (client->winId (), client);
	}
    }
    else
    {
	if (mClients.contains (id))
	    client = mClients[id];

	if (client)
	{
	    mClients.remove (client->windowId ());
	    mWindows.remove (client->winId ());
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
	mWindows.remove (window->winId ());
	mFrames.remove (window->frameId ());

	delete window;
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
    ev.xclient.data.l[1] = qt_x_time;
    ev.xclient.data.l[2] = data1;
    ev.xclient.data.l[3] = data2;
    ev.xclient.data.l[4] = data3;

    if (eventWid == qt_xrootwin ())
	mask = SubstructureRedirectMask | SubstructureNotifyMask;

    KWD::trapXError ();
    XSendEvent (qt_xdisplay (), eventWid, false, mask, &ev);
    KWD::popXError ();
}
