/*
 * Copyright © 2009 Dennis Kasprzyk <onestone@compiz-fusion.org>
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

#include "window.h"
#include "decorator.h"
#include "options.h"
#include "utils.h"

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/Xregion.h>

#include <fixx11h.h>

#include <KDE/KGlobal>
#include <KDE/KGlobalSettings>
#include <KDE/KIconLoader>
#include <kdecoration.h>
#include <kwindowsystem.h>
#include <KDE/KLocale>
#include <KDE/KStandardDirs>
#include <KDE/KAction>
#include <KDE/KActionCollection>
#include <KDE/KIcon>

#include <QApplication>
#include <QEvent>
#include <QWidget>
#include <QString>
#include <QTimer>
#include <QMenu>
#include <QX11Info>
#include <QObjectList>
#include <QVector>
#include <QProcess>
#include <QStyle>
#include <QPainter>

#include "paintredirector.h"

KWD::Window::Window (WId  parentId,
		     WId  clientId,
		     WId  frame,
		     Type type,
		     int  x,
		     int  y,
		     int  w,
		     int  h) :
    mType (type),
    mParentId (parentId),
    mFrame (0),
    mClientId (clientId),
    mSelectedId (0),
    mDecor (0),
    mPixmap (0),
    mUpdateProperty (false),
    mShapeSet (false),
    mPopup (0),
    mAdvancedMenu (0),
    mOpacityMenu (0),
    mDesktopMenu (0),
    mProcessKiller (this),
    mKeys (this),
    mResizeOpAction (0),
    mMoveOpAction (0),
    mMaximizeOpAction (0),
    mShadeOpAction (0),
    mKeepAboveOpAction (0),
    mKeepBelowOpAction (0),
    mFullScreenOpAction (0),
    mMinimizeOpAction (0),
    mCloseOpAction (0),
    mDesktopOpAction (0),
    mPaintRedirector (0)
{
    memset (&mBorder, 0, sizeof (mBorder));

    if (mType == Normal)
    {
	KWindowInfo wInfo = KWindowSystem::windowInfo (mClientId, NET::WMState |
						       NET::WMVisibleName, 0);

	mState = wInfo.state ();

	if (mType == Normal)
	{
	    mName = wInfo.visibleName ();

	    mIcon = KWindowSystem::icon (mClientId, 32, 32, true,
					 KWindowSystem::NETWM |
					 KWindowSystem::WMHints );

	    mMiniIcon = KWindowSystem::icon (mClientId, 16, 16, true,
					     KWindowSystem::NETWM |
					     KWindowSystem::WMHints );

	    if (mIcon.isNull ())
	    {
		mIcon = KWindowSystem::icon (mClientId, 32, 32, true,
					     KWindowSystem::ClassHint |
					     KWindowSystem::XApp );
		mMiniIcon = KWindowSystem::icon (mClientId, 16, 16, true,
						 KWindowSystem::ClassHint |
						 KWindowSystem::XApp );
	    }

	    mOpacity = readPropertyShort (mClientId, Atoms::netWmWindowOpacity,
					  0xffff);
	}
	else
	{
	    mIcon     = QPixmap ();
	    mMiniIcon = QPixmap ();
	    mName     = QString ("");
	}

	updateFrame (frame);

	mGeometry = QRect (x, y, w, h);

	getWindowProtocols ();
    }
    else
    {
	mIcon     = QPixmap ();
	mMiniIcon = QPixmap ();
	mName = QString ("");
	mGeometry = QRect (50, 50, 30, 1);
    }

    createDecoration ();

    mActiveChild = NULL;
}

KWD::Window::~Window (void)
{
    if (mPixmap)
	XFreePixmap (QX11Info::display(), mPixmap);

    if (mDecor)
	delete mDecor;

    if (mPopup)
	delete mPopup;

    if (mPaintRedirector)
	delete mPaintRedirector;

    if (mProcessKiller.state () == QProcess::Running)
    {
	mProcessKiller.terminate ();
	mProcessKiller.waitForFinished (10000);
	if (mProcessKiller.state () == QProcess::Running)
	{
	    mProcessKiller.kill ();
	    mProcessKiller.waitForFinished (5000);
	}
    }
}

bool
KWD::Window::isActive (void) const
{
    if (mType == DefaultActive)
	return true;

    return Decorator::activeId () == mClientId;
}

bool
KWD::Window::isCloseable (void) const
{
    KWindowInfo wInfo;

    if (mType != Normal)
	return false;

    wInfo = KWindowSystem::windowInfo (mClientId, NET::WMPid,
				       NET::WM2AllowedActions);
    return wInfo.actionSupported (NET::ActionClose);
}

bool
KWD::Window::isMaximizable (void) const
{
    KWindowInfo wInfo;

    if (mType != Normal)
	return false;

    wInfo = KWindowSystem::windowInfo (mClientId, NET::WMPid,
				       NET::WM2AllowedActions);
    return wInfo.actionSupported (NET::ActionMax);
}

KDecoration::MaximizeMode
KWD::Window::maximizeMode (void) const
{
    MaximizeMode mode = MaximizeRestore;

    if (mType != Normal)
	return mode;

    mode =
	((mState & NET::MaxVert) ? MaximizeVertical : MaximizeRestore) |
	((mState & NET::MaxHoriz) ? MaximizeHorizontal : MaximizeRestore);

    return mode;
}

bool
KWD::Window::isMinimizable (void) const
{
    KWindowInfo wInfo;

    if (mType != Normal)
	return false;

    wInfo = KWindowSystem::windowInfo (mClientId, NET::WMPid,
				       NET::WM2AllowedActions);
    return wInfo.actionSupported (NET::ActionMinimize);
}

bool
KWD::Window::providesContextHelp (void) const
{
    if (mType != Normal)
	return false;

    return mSupportContextHelp;
}

int
KWD::Window::desktop (void) const
{
    KWindowInfo wInfo = KWindowSystem::windowInfo (mClientId,
						   NET::WMDesktop, 0);

    return wInfo.desktop ();
}

bool
KWD::Window::isModal (void) const
{
    return mState & NET::Modal;
}

bool
KWD::Window::isShadeable (void) const
{
    KWindowInfo wInfo = KWindowSystem::windowInfo (mClientId, NET::WMPid,
						   NET::WM2AllowedActions);

    return wInfo.actionSupported (NET::ActionShade);
}

bool
KWD::Window::isShade (void) const
{
    if (mType != Normal)
	return false;

    return (mState & NET::Shaded);
}

bool
KWD::Window::isSetShade (void) const
{
    return isShade ();
}

bool
KWD::Window::keepAbove (void) const
{
    if (mType != Normal)
	return false;

    return (mState & NET::KeepAbove);
}

bool
KWD::Window::keepBelow (void) const
{
    if (mType != Normal)
	return false;

    return (mState & NET::KeepBelow);
}

bool
KWD::Window::isMovable (void) const
{
    KWindowInfo wInfo = KWindowSystem::windowInfo (mClientId, NET::WMPid,
						   NET::WM2AllowedActions);

    return wInfo.actionSupported (NET::ActionMove);
}

NET::WindowType
KWD::Window::windowType (unsigned long mask) const
{
    KWindowInfo wInfo = KWindowSystem::windowInfo (mClientId,
						   NET::WMWindowType, 0);

    return wInfo.windowType (mask);
}

bool
KWD::Window::isResizable (void) const
{
    KWindowInfo wInfo = KWindowSystem::windowInfo (mClientId, NET::WMPid,
						   NET::WM2AllowedActions);

    return wInfo.actionSupported (NET::ActionResize);
}

QIcon
KWD::Window::icon (void) const
{
    QIcon icon (mIcon);
    icon.addPixmap (mMiniIcon);
    return icon;
}

QString
KWD::Window::caption (void) const
{
    return mName;
}

/* TODO: We should use libtaskmanager, which is part of kdebase to create
   the window menu instead but the headers for that library are currently
   not installed. If kdebase could install those headers, we wouldn't have
   to have our own window menu implementaion here. */
void
KWD::Window::showWindowMenu (const QPoint &pos)
{
    if (!mPopup)
    {
	QAction *action;
	const int levels[] = { 100, 90, 75, 50, 25, 10 };
	
	mPopup = new QMenu ();
	mPopup->setFont (KGlobalSettings::menuFont ());

	connect (mPopup, SIGNAL (aboutToShow ()),
		 SLOT (handlePopupAboutToShow ()));
	connect (mPopup, SIGNAL (triggered (QAction*)),
		 SLOT (handlePopupActivated (QAction*)));

	mAdvancedMenu = new QMenu (mPopup);
	mAdvancedMenu->setFont (KGlobalSettings::menuFont ());

	mKeepAboveOpAction = mAdvancedMenu->addAction (i18n ("Keep &Above Others"));
        mKeepAboveOpAction->setIcon (KIcon ("go-up"));
        KAction *kaction = qobject_cast<KAction*>
			   (mKeys.action ("Window Above Other Windows"));
        if (kaction != 0)
            mKeepAboveOpAction->setShortcut (kaction->globalShortcut ().primary ());
        mKeepAboveOpAction->setCheckable (true);
        mKeepAboveOpAction->setData (KDecorationDefines::KeepAboveOp);

        mKeepBelowOpAction = mAdvancedMenu->addAction (i18n ("Keep &Below Others"));
        mKeepBelowOpAction->setIcon (KIcon ("go-down"));
        kaction = qobject_cast<KAction*>
		  (mKeys.action ("Window Below Other Windows"));
        if (kaction != 0)
            mKeepBelowOpAction->setShortcut (kaction->globalShortcut ().primary ());
        mKeepBelowOpAction->setCheckable (true);
        mKeepBelowOpAction->setData (KDecorationDefines::KeepBelowOp);

        mFullScreenOpAction = mAdvancedMenu->addAction (i18n ("&Fullscreen"));
        mFullScreenOpAction->setIcon (KIcon ("view-fullscreen"));
        kaction = qobject_cast<KAction*> (mKeys.action ("Window Fullscreen"));
        if (kaction != 0)
            mFullScreenOpAction->setShortcut (kaction->globalShortcut ().primary ());
        mFullScreenOpAction->setCheckable (true);
        mFullScreenOpAction->setData (KDecorationDefines::FullScreenOp);

	action = mPopup->addMenu (mAdvancedMenu);
	action->setText (i18n ("Ad&vanced"));

	mOpacityMenu = new QMenu (mPopup);
	mOpacityMenu->setFont (KGlobalSettings::menuFont ());

	connect (mOpacityMenu, SIGNAL (triggered (QAction*)),
		 SLOT (handleOpacityPopupActivated (QAction*)));
	

        for( unsigned int i = 0; i < sizeof (levels) / sizeof (levels[0]); ++i)
	{
	    action = mOpacityMenu->addAction
			(QString::number (levels[i]) + "%");
	    action->setCheckable (true);
	    action->setData (levels[i]);
	}
	action = mPopup->addMenu (mOpacityMenu);
	action->setText (i18n ("&Opacity"));


	mDesktopMenu = new QMenu (mPopup);
	mDesktopMenu->setFont (KGlobalSettings::menuFont ());

	connect (mDesktopMenu, SIGNAL (triggered (QAction*)),
		 SLOT (handleDesktopPopupActivated (QAction*)));

	mDesktopOpAction = mPopup->addMenu (mDesktopMenu);
	mDesktopOpAction->setText (i18n ("To &Desktop"));

	mMoveOpAction = mPopup->addAction (i18n ("&Move"));
        mMoveOpAction->setIcon (KIcon ("move"));
        kaction = qobject_cast<KAction*> (mKeys.action ("Window Move"));
        if (kaction != 0)
            mMoveOpAction->setShortcut (kaction->globalShortcut ().primary ());
        mMoveOpAction->setData (KDecorationDefines::MoveOp);

        mResizeOpAction = mPopup->addAction (i18n ("Re&size"));
        kaction = qobject_cast<KAction*> (mKeys.action("Window Resize"));
        if (kaction != 0)
            mResizeOpAction->setShortcut (kaction->globalShortcut ().primary ());
        mResizeOpAction->setData (KDecorationDefines::ResizeOp);

        mMinimizeOpAction = mPopup->addAction (i18n ("Mi&nimize"));
        kaction = qobject_cast<KAction*> (mKeys.action ("Window Minimize"));
        if (kaction != 0)
            mMinimizeOpAction->setShortcut (kaction->globalShortcut ().primary ());
        mMinimizeOpAction->setData (KDecorationDefines::MinimizeOp);

        mMaximizeOpAction = mPopup->addAction (i18n ("Ma&ximize"));
        kaction = qobject_cast<KAction*> (mKeys.action ("Window Maximize"));
        if (kaction != 0)
            mMaximizeOpAction->setShortcut (kaction->globalShortcut ().primary ());
        mMaximizeOpAction->setCheckable (true);
        mMaximizeOpAction->setData (KDecorationDefines::MaximizeOp);

        mShadeOpAction = mPopup->addAction (i18n ("Sh&ade"));
        kaction = qobject_cast<KAction*> (mKeys.action ("Window Shade"));
        if (kaction != 0)
            mShadeOpAction->setShortcut (kaction->globalShortcut ().primary ());
        mShadeOpAction->setCheckable (true);
        mShadeOpAction->setData (KDecorationDefines::ShadeOp);

	mPopup->addSeparator ();

	mCloseOpAction = mPopup->addAction (i18n("&Close"));
        mCloseOpAction->setIcon (KIcon ("window-close" ));
        kaction = qobject_cast<KAction*> (mKeys.action("Window Close"));
        if (kaction != 0)
            mCloseOpAction->setShortcut (kaction->globalShortcut ().primary ());
        mCloseOpAction->setData (KDecorationDefines::CloseOp);
    }

    QPoint pnt = mDecor->widget ()->mapFromGlobal (pos);

    pnt += QPoint (mGeometry.x () - mBorder.left - mPadding.left,
		   mGeometry.y () - mBorder.top - mPadding.top);

    mPopup->exec (pnt);
}

void
KWD::Window::showWindowMenu (const QRect &pos)
{
    showWindowMenu (pos.bottomLeft ());
}

KWD::Options::MouseCommand
KWD::Window::buttonToCommand (Qt::MouseButtons button)
{
    Options::MouseCommand com = Options::MouseNothing;
    bool                  active = isActive ();

    if (!mSupportTakeFocus)
	active = true;

    switch (button) {
    case Qt::LeftButton:
	com = active ? Decorator::options ()->commandActiveTitlebar1 () :
	               Decorator::options()->commandInactiveTitlebar1 ();
	break;
    case Qt::MidButton:
	com = active ? Decorator::options ()->commandActiveTitlebar2 () :
	               Decorator::options()->commandInactiveTitlebar2 ();
	break;
    case Qt::RightButton:
	com = active ? Decorator::options ()->commandActiveTitlebar3 () :
	               Decorator::options()->commandInactiveTitlebar3 ();
    default:
	break;
    }

    return com;
}

void
KWD::Window::processMousePressEvent (QMouseEvent *qme)
{
    Options::MouseCommand com = buttonToCommand (qme->button ());

    if (qme->button () == Qt::LeftButton)
    {
	// actions where it's not possible to get the matching release event
	if (com != Options::MouseOperationsMenu &&
	    com != Options::MouseMinimize)
	{
	    moveWindow (qme);
	    return;
	}
    }

    performMouseCommand (com, qme);
}

void
KWD::Window::performWindowOperation (WindowOperation wo)
{
    switch (wo) {
    case KDecoration::MaximizeOp:
	maximize (maximizeMode () == KDecoration::MaximizeFull ?
		  KDecoration::MaximizeRestore : KDecoration::MaximizeFull);
	break;
    case KDecoration::HMaximizeOp:
	maximize (maximizeMode () ^ KDecoration::MaximizeHorizontal);
	break;
    case KDecoration::VMaximizeOp:
	maximize (maximizeMode () ^ KDecoration::MaximizeVertical);
	break;
    case KDecoration::MinimizeOp:
	minimize ();
	break;
    case KDecoration::ShadeOp:
	setShade (!isShade ());
	break;
    case KDecoration::CloseOp:
	closeWindow ();
	break;
    case KDecoration::KeepAboveOp:
	setKeepAbove (!keepAbove ());
	break;
    case KDecoration::KeepBelowOp:
	setKeepBelow (!keepBelow ());
	break;
    case KDecoration::FullScreenOp:
	if (mState & NET::FullScreen)
	    KWindowSystem::clearState (mClientId, NET::FullScreen);
	else
	    KWindowSystem::setState (mClientId, NET::FullScreen);
	break;
    case KDecoration::MoveOp:
	Decorator::rootInfo ()->moveResizeRequest (mClientId,
						   mGeometry.x () +
						   mGeometry.width () / 2,
						   mGeometry.y () +
						   mGeometry.height () / 2,
						   NET::KeyboardMove);
	break;
    case KDecoration::ResizeOp:
	Decorator::rootInfo ()->moveResizeRequest (mClientId,
						   mGeometry.x () +
						   mGeometry.width () / 2,
						   mGeometry.y () +
						   mGeometry.height () / 2,
						   NET::KeyboardSize);
    default:
	break;
    }
}

bool
KWD::Window::isPreview (void) const
{
    return false;
}

QRect
KWD::Window::geometry (void) const
{
    QRect rect = mGeometry;

    return QRect (rect.x () - ROOT_OFF_X,
		  rect.y () - ROOT_OFF_Y,
		  rect.width (),
		  rect.height ());
}

QRect
KWD::Window::iconGeometry (void) const
{
    return QRect ();
}

QRect
KWD::Window::clientGeometry (void)
{
    return mGeometry;

    QRect frame = geometry ();

    return QRect (frame.x () + mBorder.left,
		  frame.y () + mBorder.top,
		  frame.width () - mBorder.left - mBorder.right,
		  frame.height () - mBorder.top - mBorder.bottom);
}

QRegion
KWD::Window::unobscuredRegion (const QRegion & r) const
{
    return r;
}

WId
KWD::Window::windowId (void) const
{
    return mClientId;
}

void
KWD::Window::closeWindow (void)
{
    Decorator::rootInfo ()->closeWindowRequest (mClientId);
}

void
KWD::Window::maximize (MaximizeMode mode)
{
    KWindowSystem::setState (mClientId,
		    ((mode & MaximizeVertical) ? NET::MaxVert : 0) |
		    ((mode & MaximizeHorizontal) ? NET::MaxHoriz : 0));
    KWindowSystem::clearState (mClientId,
		      ((mode & MaximizeVertical) ? 0 : NET::MaxVert) |
		      ((mode & MaximizeHorizontal) ? 0 : NET::MaxHoriz));
}

void
KWD::Window::minimize (void)
{
    KWindowSystem::minimizeWindow (mClientId, false);
}

void
KWD::Window::showContextHelp (void)
{
    if (mSupportContextHelp)
	KWD::Decorator::sendClientMessage (mClientId, mClientId,
					   Atoms::wmProtocols,
					   Atoms::netWmContextHelp);
}

void
KWD::Window::titlebarDblClickOperation (void)
{
    WindowOperation op;

    op = KWD::Decorator::options ()->operationTitlebarDblClick ();
    performWindowOperation (op);
}

void
KWD::Window::setDesktop (int desktop)
{
    KWindowSystem::setOnDesktop (mClientId, desktop);
}

void
KWD::Window::setKeepBelow (bool set)
{
    if (set)
    {
	KWindowSystem::clearState (mClientId, NET::KeepAbove);
	KWindowSystem::setState (mClientId, NET::KeepBelow);
    }
    else
    {
	KWindowSystem::clearState (mClientId, NET::KeepBelow);
    }
}

void
KWD::Window::setKeepAbove (bool set)
{
    if (set)
    {
	KWindowSystem::clearState (mClientId, NET::KeepBelow);
	KWindowSystem::setState (mClientId, NET::KeepAbove);
    }
    else
    {
	KWindowSystem::clearState (mClientId, NET::KeepAbove);
    }
}

void
KWD::Window::setShade (bool set)
{
    if (set)
	KWindowSystem::setState (mClientId, NET::Shaded);
    else
	KWindowSystem::clearState (mClientId, NET::Shaded);

    mDecor->shadeChange ();
}

void
KWD::Window::titlebarMouseWheelOperation (int delta)
{
    Options::MouseCommand com;

    com = Decorator::options()->operationTitlebarMouseWheel (delta);
    performMouseCommand (com, 0);
}

int
KWD::Window::currentDesktop (void) const
{
    return KWindowSystem::currentDesktop ();
}

QWidget *
KWD::Window::initialParentWidget (void) const
{
    return 0;
}

Qt::WFlags
KWD::Window::initialWFlags (void) const
{
    return 0;
}

void
KWD::Window::grabXServer (bool)
{
}

bool
KWD::Window::compositingActive (void) const
{
    return true;
}

#if KDE_IS_VERSION(4,3,90)

QRect
KWD::Window::transparentRect () const
{
    return QRect ();
}

bool
KWD::Window::isClientGroupActive ()
{
    return false;
}

QList<ClientGroupItem>
KWD::Window::clientGroupItems () const
{
    QList<ClientGroupItem> items;

    QIcon icon (mIcon);
    icon.addPixmap (mMiniIcon);

    items.append (ClientGroupItem (mName, icon));

    return items;
}

long
KWD::Window::itemId (int index)
{
    return (long) mClientId;
}

int
KWD::Window::visibleClientGroupItem ()
{
    return 0;
}

void
KWD::Window::setVisibleClientGroupItem (int index)
{
}

void
KWD::Window::moveItemInClientGroup (int index, int before)
{
}

void
KWD::Window::moveItemToClientGroup (long itemId, int before)
{
}

void
KWD::Window::removeFromClientGroup (int index, const QRect& newGeom)
{
}

void
KWD::Window::closeClientGroupItem (int index)
{
    closeWindow ();
}

void
KWD::Window::closeAllInClientGroup ()
{
    closeWindow ();
}

void
KWD::Window::displayClientMenu (int index, const QPoint& pos)
{
    showWindowMenu (pos);
}

KDecorationDefines::WindowOperation
KWD::Window::buttonToWindowOperation(Qt::MouseButtons button)
{
    Options::MouseCommand com = buttonToCommand (button);

    if (com == Options::MouseOperationsMenu)
	return KDecorationDefines::OperationsOp;

    return KDecorationDefines::NoOp;
}

#endif

void
KWD::Window::createDecoration (void)
{
    KDecoration *decor;

    if (mDecor)
	return;

    decor = Decorator::pluginManager ()->createDecoration (this);
    decor->init ();

    mDecor = decor;
    
    mDecor->widget ()->installEventFilter (this);

    mPaintRedirector = new KWin::PaintRedirector (mDecor->widget ());
    connect (mPaintRedirector, SIGNAL (paintPending()),
	     this, SLOT (decorRepaintPending ()));

    mPadding.top = mPadding.bottom = mPadding.left = mPadding.right = 0;

    if (KDecorationUnstable *deco2 = dynamic_cast<KDecorationUnstable*>(decor))
        deco2->padding (mPadding.left, mPadding.right, mPadding.top, mPadding.bottom);

    XReparentWindow (QX11Info::display(), mDecor->widget ()->winId (), mParentId, 0, 0);
    
    //decor->widget()->move(-mPadding.left, -mPadding.top);

    if (mType == Normal && mFrame)
    {
	KWD::trapXError ();
	XSelectInput (QX11Info::display(), mFrame,
		      StructureNotifyMask | PropertyChangeMask |
		      ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
		      EnterWindowMask | LeaveWindowMask);
	if (KWD::popXError ())
	    return;
    }

    resizeDecoration (true);
}

void
KWD::Window::setMask (const QRegion &region, int)
{
    if (region.isEmpty ())
    {
      mShapeSet = false;
      return;
    }

    if (mShapeSet && region == mShape)
	return;

    mShape    = region;
    mShapeSet = true;

    if (mFrame)
    {
	QRegion r = region.translated (-mPadding.left, -mPadding.top);

	r -= QRegion (mBorder.left, mBorder.top,
		      mGeometry.width (), mGeometry.height ());

	KWD::trapXError ();
	XShapeCombineRegion (QX11Info::display(),
			     mFrame,
			     ShapeInput,
			     0,
			     0,
			     r.handle (),
			     ShapeSet);
	KWD::popXError ();
    }
}

void
KWD::Window::resizeDecoration (bool force)
{
    int w, h;

    mDecor->borders (mBorder.left, mBorder.right, mBorder.top, mBorder.bottom);
    
    mExtents.left   = mBorder.left + mPadding.left;
    mExtents.right  = mBorder.right + mPadding.right;
    mExtents.top    = mBorder.top + mPadding.top;
    mExtents.bottom = mBorder.bottom + mPadding.bottom;

    if (mType != Normal)
    {
	mGeometry = QRect (50, 50, 100, 100);
    }

    w = mGeometry.width () + mExtents.left + mExtents.right;
    h = mGeometry.height () + mExtents.top + mExtents.bottom;
    
    if (!force)
    {
      if (w == decorWidget ()->width () && h == decorWidget ()->height ())
        return;
    }
    
    /* reset shape */
    mShapeSet        = false;

    if (mPixmap)
    {
	XFreePixmap (QX11Info::display(), mPixmap);
	mPixmap = None;
    }

    mDecor->resize (QSize (w, h));
    mDecor->widget ()->show ();
    mDecor->widget ()->update ();

    mPixmap = XCreatePixmap (QX11Info::display(),
			     QX11Info::appRootWindow (),
			     qMax (w, mGeometry.height ()),
			     mExtents.top + mExtents.bottom + 
			     mExtents.left + mExtents.right, 32);

    mPixmapQt = QPixmap::fromX11Pixmap (mPixmap, QPixmap::ExplicitlyShared);

    mPixmapQt.fill (Qt::transparent);

    mUpdateProperty = true;
}

void
KWD::Window::updateBlurProperty (int topOffset,
				 int bottomOffset,
				 int leftOffset,
				 int rightOffset)
{
    Atom    atom = Atoms::compizWindowBlurDecor;
    QRegion topQRegion, bottomQRegion, leftQRegion, rightQRegion;
    Region  topRegion = NULL;
    Region  bottomRegion = NULL;
    Region  leftRegion = NULL;
    Region  rightRegion = NULL;
    int     size = 0;
    int     w, h;

    w = mGeometry.width () + mBorder.left + mBorder.right;
    h = mGeometry.height () + mBorder.top + mBorder.bottom;

    if (blurType != BLUR_TYPE_NONE)
    {
	QRegion r, shape = QRegion (0, 0, w, h);

	if (mShapeSet)
	    shape = mShape.translated (-mPadding.left, -mPadding.top);

	r = QRegion (0, 0, w, mBorder.top);
	topQRegion = r.intersect (shape);
	if (!topQRegion.isEmpty ())
	{
	    topQRegion.translate (-mBorder.left,
				  -mBorder.top);
	    topRegion = topQRegion.handle ();
	}

	if (blurType == BLUR_TYPE_ALL)
	{
	    r = QRegion (0, h - mBorder.bottom,
			 w, mBorder.bottom);
	    bottomQRegion = r.intersect (shape);
	    if (!bottomQRegion.isEmpty ())
	    {
		bottomQRegion.translate (-mBorder.left,
					 -(h - mBorder.bottom));
		bottomRegion = bottomQRegion.handle ();
	    }

	    r = QRegion (0, mBorder.top,
			 mBorder.left, mGeometry.height ());
	    leftQRegion = r.intersect (shape);
	    if (!leftQRegion.isEmpty ())
	    {
		leftQRegion.translate (-mBorder.left,
				       -mBorder.top);
		leftRegion = leftQRegion.handle ();
	    }

	    r = QRegion (w - mBorder.right, mBorder.top,
			 mBorder.right, mGeometry.height ());
	    rightQRegion = r.intersect (shape);
	    if (!rightQRegion.isEmpty ())
	    {
		rightQRegion.translate (-(w - mBorder.right),
					-mBorder.top);
		rightRegion = rightQRegion.handle ();
	    }
	}
    }

    if (topRegion)
	size += topRegion->numRects;
    if (bottomRegion)
	size += bottomRegion->numRects;
    if (leftRegion)
	size += leftRegion->numRects;
    if (rightRegion)
	size += rightRegion->numRects;

    if (size)
    {
	long data[size * 6 + 2];

	decor_region_to_blur_property (data, 4, 0,
				       mGeometry.width (),
				       mGeometry.height (),
				       topRegion, topOffset,
				       bottomRegion, bottomOffset,
				       leftRegion, leftOffset,
				       rightRegion, rightOffset);

	KWD::trapXError ();
	XChangeProperty (QX11Info::display(), mClientId, atom,
			 XA_INTEGER,
			 32, PropModeReplace, (unsigned char *) data,
			 2 + size * 6);
	KWD::popXError ();
    }
    else
    {
	KWD::trapXError ();
	XDeleteProperty (QX11Info::display(), mClientId, atom);
	KWD::popXError ();
    }
}

void
KWD::Window::updateProperty (void)
{
    Atom	    atom = Atoms::netWindowDecor;
    decor_extents_t maxExtents;
    long	    data[256];
    decor_quad_t    quads[N_QUADS_MAX];
    int		    nQuad = 0;
    int             left, right, top, bottom, width, height;
    unsigned int    saveState;

    if (mType == Default)
	atom = Atoms::netWindowDecorNormal;
    else if (mType == DefaultActive)
	atom = Atoms::netWindowDecorActive;

    saveState = mState;
    mState = NET::MaxVert | NET::MaxHoriz;
    mDecor->borders (maxExtents.left, maxExtents.right,
		     maxExtents.top, maxExtents.bottom);
    mState = saveState;
    mDecor->borders (mBorder.left, mBorder.right, mBorder.top, mBorder.bottom);

    left = mExtents.left;
    right = mExtents.right;
    top = mExtents.top;
    bottom = mExtents.bottom;
    width = mGeometry.width ();
    height = mGeometry.height ();

    if (mType == Normal)
    {
        decor_quad_t *q = quads;
	int n = 0;

	int	topXOffset = width;
	QWidget *widget = mDecor->widget ();
	int	x;

	if (widget)
	{
	    const QList<QObject*> children = widget->children ();

	    foreach (QObject *obj, children)
	    {
		QWidget *child;

		if (!obj->isWidgetType ())
		    continue;

		child = static_cast <QWidget *> (obj);

		x = child->x () - mExtents.left - 2;
		if (x > width / 2 && x < topXOffset)
		    topXOffset = x;
	    }
	}

	// top quads
	n = decor_set_horz_quad_line (q, left, topXOffset, right, 
				      width - topXOffset - 1, -top, 0, GRAVITY_NORTH,
				      left + right + width, -(width - topXOffset - 1),
				      GRAVITY_EAST, 0, 0);

	q += n; nQuad += n;
	
	// bottom quads
	n = decor_set_horz_quad_line (q, left, width / 2, right, (width / 2) - 1, 0,
				      bottom, GRAVITY_SOUTH, left + right + width,
				      -((width / 2) - 1), GRAVITY_EAST, 0, top);

	q += n; nQuad += n;

	// left quads
	n = decor_set_vert_quad_row (q, 0, height / 2, 0, (height / 2) - 1, -left, 0,
				     GRAVITY_WEST, height, -((height / 2) - 1),
				     GRAVITY_SOUTH, 0, top + bottom, 1);

	q += n; nQuad += n;

	// right quads
	n = decor_set_vert_quad_row (q, 0, height / 2, 0, (height / 2) - 1, 0, right,
				     GRAVITY_EAST, height, -((height / 2) - 1),
				     GRAVITY_SOUTH, 0, top + bottom + left, 1);

	q += n; nQuad += n;

	updateBlurProperty (topXOffset, width / 2, height / 2, height / 2);
    }
    else
    {
	decor_quad_t *q = quads;
	int n = 0;
	
	// top
	n = decor_set_horz_quad_line (q, left, 0, right, 0, -top, 0,
				      GRAVITY_NORTH, left + right + width,
				      width / 2, 0, 0, 0);

        q += n; nQuad += n;
	
	// bottom
	n = decor_set_horz_quad_line (q, left, 0, right, 0, 0, bottom,
				      GRAVITY_SOUTH, left + right + width,
				      width / 2, 0, 0, top);

        q += n; nQuad += n;
	
	// left
	n = decor_set_vert_quad_row (q, 0, 0, 0, 0, -left, 0, GRAVITY_WEST,
				     height, height / 2, 0, 0, top + bottom, 1);

	q += n; nQuad += n;

	// right
	n = decor_set_vert_quad_row (q, 0, 0, 0, 0, 0, right, GRAVITY_EAST,
				     height, height / 2, 0, 0, top + bottom + left, 1);
	
	q += n; nQuad += n;
    }

    decor_quads_to_property (data, mPixmap,
			     &mBorder, &maxExtents,
			     1, 0,
			     quads, nQuad);

    KWD::trapXError ();
    XChangeProperty (QX11Info::display(), mClientId, atom,
		     XA_INTEGER,
		     32, PropModeReplace, (unsigned char *) data,
		     BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
    KWD::popXError ();

    mUpdateProperty = false;
}

void
KWD::Window::handleActiveChange (void)
{
    mDecor->activeChange ();
    resizeDecoration ();
}

void
KWD::Window::updateFrame (WId frame)
{
    mFrame = frame;

    KWD::trapXError ();
    XSelectInput (QX11Info::display(), mFrame,
		  StructureNotifyMask | PropertyChangeMask |
		  ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
		  EnterWindowMask | LeaveWindowMask);
    KWD::popXError ();
}

void
KWD::Window::updateSelected (WId selectedId)
{
    mSelectedId = selectedId;

    updateName ();
}

void
KWD::Window::updateWindowGeometry (void)
{
    KWD::trapXError ();
    KWindowInfo wInfo = KWindowSystem::windowInfo (mClientId, NET::WMGeometry);
    KWD::popXError ();

    if (!wInfo.valid ())
	return;

    QRect	geometry = wInfo.geometry ();
    int         w, h;

    w = mGeometry.width () + mBorder.left + mBorder.right;
    h = mGeometry.height () + mBorder.top + mBorder.bottom;

    if (mGeometry.width ()  != geometry.width () ||
	mGeometry.height () != geometry.height ())
    {
	mGeometry = geometry;
	resizeDecoration ();
    }
    else if (mGeometry.x ()  != geometry.x () ||
	mGeometry.y () != geometry.y ())
    {
	mGeometry = geometry;
    }
}

void
KWD::Window::reloadDecoration (void)
{
    delete mDecor;
    mDecor = 0;

    delete mPaintRedirector;
    mPaintRedirector = 0;

    mShapeSet = false;

    createDecoration ();
}

Cursor
KWD::Window::positionToCursor (QPoint pos)
{
    switch (mDecor->mousePosition (pos + QPoint (mPadding.left, mPadding.top))) {
    case PositionCenter:
	return cursors[1][1].cursor;
    case PositionLeft:
	return cursors[1][0].cursor;
    case PositionRight:
	return cursors[1][2].cursor;
    case PositionTop:
	return cursors[0][1].cursor;
    case PositionBottom:
	return cursors[2][1].cursor;
    case PositionTopLeft:
	return cursors[0][0].cursor;
    case PositionTopRight:
	return cursors[0][2].cursor;
    case PositionBottomLeft:
	return cursors[2][0].cursor;
    case PositionBottomRight:
	return cursors[2][2].cursor;
    default:
	break;
    }

    return cursors[1][1].cursor;
}

void
KWD::Window::updateCursor (QPoint pos)
{
    KWD::trapXError ();
    XDefineCursor (QX11Info::display(), mFrame, positionToCursor (pos));
    KWD::popXError ();
}

void
KWD::Window::getWindowProtocols (void)
{
    Atom *p;
    int  n;
    int  status;

    mSupportTakeFocus   = false;
    mSupportContextHelp = false;

    KWD::trapXError ();
    status = XGetWMProtocols (QX11Info::display(), mClientId, &p, &n);
    if (KWD::popXError ())
	return;

    if (status)
    {
	int i;

	for (i = 0; i < n; i++)
	{
	    if (p[i] == Atoms::wmTakeFocus)
		mSupportTakeFocus = true;
	    else if (p[i] == Atoms::netWmContextHelp)
		mSupportContextHelp = true;
	}

	if (n > 0)
	    XFree (p);
    }
}

void
KWD::Window::handlePopupActivated (QAction * action)
{
    WindowOperation op = static_cast <WindowOperation> (action->data().toInt());

    performWindowOperation (op);
}

void
KWD::Window::handleOpacityPopupActivated (QAction *action)
{
    int op = action->data().toInt();

    op = op * 0xffff / 100;

    if (op != mOpacity)
	Decorator::sendClientMessage (QX11Info::appRootWindow(), mClientId,
				      Atoms::netWmWindowOpacity,
				      (op << 16) | op);
}


void
KWD::Window::handleDesktopPopupActivated (QAction *action)
{

    if (action->data().toInt())
	setDesktop (action->data().toInt());
    else
	KWindowSystem::setOnAllDesktops (mClientId, true);
}

void
KWD::Window::handlePopupAboutToShow (void)
{
    int numberOfDesktops;

    numberOfDesktops = KWindowSystem::numberOfDesktops ();
    if (numberOfDesktops > 1)
    {
	NETRootInfo *rootInfo = Decorator::rootInfo ();
	QString	    name;
	int	    i;
	int	    winDesktop = desktop ();
	QAction     *action;
	const int   BASE = 10;

	mDesktopMenu->clear ();

	action = mDesktopMenu->addAction (i18n ("&All Desktops"));
	action->setData (0);
	action->setCheckable (true);


	action->setChecked (winDesktop == NET::OnAllDesktops);
	mDesktopMenu->addSeparator ();

	for (i = 1; i <= numberOfDesktops; i++)
	{
	    QString basic_name ("%1 %2");
	    if (i < BASE)
		basic_name.prepend ('&');

	    basic_name = basic_name.arg (i).arg (
			 QString (rootInfo->desktopName (i)).replace
			 ('&', "&&"));

	    action = mDesktopMenu->addAction (basic_name);
	    action->setData (i);
	    action->setCheckable (true);
	    action->setChecked (winDesktop == i);
	}

	mDesktopOpAction->setVisible (true);
    }
    else
    {
	mDesktopOpAction->setVisible (false);
    }

    mResizeOpAction->setEnabled (isResizable ());
    mMoveOpAction->setEnabled (isMovable ());

    mMaximizeOpAction->setEnabled (isMaximizable ());
    mMaximizeOpAction->setChecked (maximizeMode () == MaximizeFull);

    mShadeOpAction->setChecked (isShade ());
    mShadeOpAction->setEnabled (isShadeable ());

    mKeepAboveOpAction->setChecked (keepAbove ());
    mKeepBelowOpAction->setChecked (keepBelow ());
    mFullScreenOpAction->setChecked (mState & NET::FullScreen);

    mMinimizeOpAction->setEnabled (isMinimizable ());
    mCloseOpAction->setEnabled (isCloseable ());

    foreach (QAction* action, mOpacityMenu->actions ())
    {
	if(action->data ().toInt () ==
	   qRound ((float)mOpacity * 100.0 / 0xffff))
	    action->setChecked( true );
	else
	    action->setChecked( false );
    }

}

void
KWD::Window::updateState (void)
{
    KWindowInfo wInfo = KWindowSystem::windowInfo (mClientId, NET::WMState, 0);

    unsigned long newState = wInfo.state ();
    unsigned long stateChange = mState ^ newState;

    mState = newState;

    if (stateChange & NET::Max)
    {
	mDecor->maximizeChange ();
	resizeDecoration (false);
    }

    if (stateChange & NET::KeepAbove && !(mState & NET::KeepAbove))
	mDecor->emitKeepAboveChanged (mState & NET::KeepAbove);
    if (stateChange & NET::KeepBelow && !(mState & NET::KeepBelow))
	mDecor->emitKeepBelowChanged (mState & NET::KeepBelow);
    if (stateChange & NET::KeepAbove && mState & NET::KeepAbove)
	mDecor->emitKeepAboveChanged (mState & NET::KeepAbove);
    if (stateChange & NET::KeepBelow && mState & NET::KeepBelow)
	mDecor->emitKeepBelowChanged (mState & NET::KeepBelow);
    if (stateChange & NET::Shaded)
	mDecor->shadeChange ();
    if (stateChange & NET::Sticky)
	mDecor->desktopChange ();
}

void
KWD::Window::updateName (void)
{
    KWindowInfo wInfo;

    wInfo = KWindowSystem::windowInfo (mClientId, NET::WMVisibleName, 0);

    mName = wInfo.visibleName ();

    mDecor->captionChange ();
}

void
KWD::Window::updateIcons (void)
{
    mIcon = KWindowSystem::icon (mClientId, 32, 32, true,
				 KWindowSystem::NETWM |
				 KWindowSystem::WMHints);

    mMiniIcon = KWindowSystem::icon (mClientId, 16, 16, true,
				     KWindowSystem::NETWM |
				     KWindowSystem::WMHints);

    if (mIcon.isNull ())
    {
	mIcon = KWindowSystem::icon (mClientId, 32, 32, true,
				     KWindowSystem::ClassHint |
				     KWindowSystem::XApp );
	mMiniIcon = KWindowSystem::icon (mClientId, 16, 16, true,
					 KWindowSystem::ClassHint |
					 KWindowSystem::XApp );
    }

    mDecor->iconChange ();
}

NET::Direction
KWD::Window::positionToDirection (int pos)
{
    switch (pos) {
    case PositionLeft:
	return NET::Left;
    case PositionRight:
	return NET::Right;
    case PositionTop:
	return NET::Top;
    case PositionBottom:
	return NET::Bottom;
    case PositionTopLeft:
	return NET::TopLeft;
    case PositionTopRight:
	return NET::TopRight;
    case PositionBottomLeft:
	return NET::BottomLeft;
    case PositionBottomRight:
	return NET::BottomRight;
    default:
	break;
    }

    return NET::Move;
}

void
KWD::Window::moveWindow (QMouseEvent *qme)
{
    NET::Direction direction;

    direction = positionToDirection (mDecor->mousePosition (qme->pos ()));

    QPoint p (mGeometry.x () - mExtents.left, mGeometry.y () - mExtents.top);
    p += qme->pos ();

    XUngrabPointer (QX11Info::display(), CurrentTime);
    XUngrabKeyboard (QX11Info::display(), CurrentTime);

    Decorator::rootInfo ()->restackRequest (mClientId, NET::FromApplication,
			 		    None, Above,
					    QX11Info::appTime());

    Decorator::rootInfo ()->moveResizeRequest (mClientId,
					       p.x (),
					       p.y (),
					       direction);
    mFakeRelease = true;

}

#define OPACITY_STEP (0xffff / 10)

void
KWD::Window::performMouseCommand (Options::MouseCommand command,
				  QMouseEvent		*qme)
{
    switch (command) {
    case Options::MouseRaise:
	KWindowSystem::raiseWindow (mClientId);
	break;
    case Options::MouseLower:
	KWindowSystem::lowerWindow (mClientId);
	break;
    case Options::MouseShade :
	setShade (!isShade ());
	break;
    case Options::MouseSetShade:
	setShade (true);
	break;
    case Options::MouseUnsetShade:
	setShade (false);
	break;
    case Options::MouseOperationsMenu:
	showWindowMenu (mDecor->widget ()->mapToGlobal (qme->pos ()));
	break;
    case Options::MouseMaximize:
	maximize (KDecoration::MaximizeFull);
	break;
    case Options::MouseRestore:
	maximize (KDecoration::MaximizeRestore);
	break;
    case Options::MouseMinimize:
	minimize ();
	break;
    case Options::MouseAbove:
	if (keepBelow ())
	    setKeepBelow (false);
	else
	    setKeepAbove (true);
	break;
    case Options::MouseBelow:
	if (keepAbove ())
	    setKeepAbove (false);
	else
	    setKeepBelow (true);
	break;
    case Options::MousePreviousDesktop:
	break;
    case Options::MouseNextDesktop:
	break;
    case Options::MouseOpacityMore:
    {
	int opacity = mOpacity;

	if (opacity < 0xffff)
	{
	    opacity += OPACITY_STEP;
	    if (opacity > 0xffff)
		opacity = 0xffff;

	    Decorator::sendClientMessage (QX11Info::appRootWindow(),
					  mClientId,
					  Atoms::netWmWindowOpacity,
					  (opacity << 16) | opacity);
	}
    } break;
    case Options::MouseOpacityLess:
    {
	int opacity = mOpacity;

	if (opacity > OPACITY_STEP)
	{
	    opacity -= OPACITY_STEP;
	    if (opacity < OPACITY_STEP)
		opacity = OPACITY_STEP;

	    Decorator::sendClientMessage (QX11Info::appRootWindow(),
					  mClientId,
					  Atoms::netWmWindowOpacity,
					  (opacity << 16) | opacity);
	}
    } break;
    case Options::MouseActivateRaiseAndMove:
    case Options::MouseActivateRaiseAndUnrestrictedMove:
    case Options::MouseMove:
    case Options::MouseUnrestrictedMove:
    case Options::MouseResize:
    case Options::MouseUnrestrictedResize:
	if (qme)
	    moveWindow (qme);
    case Options::MouseNothing:
    default:
	break;
    }
}

void
KWD::Window::showKillProcessDialog (Time timestamp)
{
    KWindowInfo kWinInfo =
	KWindowSystem::windowInfo (mClientId, 0, NET::WM2WindowClass |
				   NET::WM2ClientMachine);
    NETWinInfo       wInfo = NETWinInfo (QX11Info::display(), mClientId,
					 QX11Info::appRootWindow(), NET::WMPid);
    QByteArray	     clientMachine, resourceClass;
    pid_t	     pid;
    char	     buf[257];

    if (mProcessKiller.state () == QProcess::Running)
	return;

    clientMachine = kWinInfo.clientMachine ();
    resourceClass = kWinInfo.windowClassClass ();
    pid		  = wInfo.pid ();

    if (gethostname (buf, sizeof (buf) - 1) == 0)
    {
	if (strcmp (buf, clientMachine) == 0)
	    clientMachine = "localhost";
    }

    mProcessKiller.start (KStandardDirs::findExe ("kwin_killer_helper"),
	QStringList () << "--pid" << QByteArray ().setNum (pid) <<
	"--hostname" << clientMachine <<
	"--windowname" << mName.toUtf8 () <<
	"--applicationname" << resourceClass <<
	"--wid" << QByteArray ().setNum ((unsigned int) mClientId) <<
	"--timestamp" << QByteArray ().setNum ((unsigned int) timestamp),
	QIODevice::NotOpen);
}

void
KWD::Window::hideKillProcessDialog (void)
{
    if (mProcessKiller.state () == QProcess::Running)
    {
	mProcessKiller.terminate ();
    }
}

void
KWD::Window::decorRepaintPending ()
{
    if (!mPaintRedirector || !mPixmap)
        return;

    QRegion reg = mPaintRedirector->pendingRegion();
    if (reg.isEmpty())
        return;
    
    QRect bBox = reg.boundingRect();
 
    if (mShapeSet)
      reg &= mShape;
    
    int l = mExtents.left;
    int r = mExtents.right;
    int t = mExtents.top;
    int b = mExtents.bottom;
    int w = mGeometry.width ();
    int h = mGeometry.height ();
    
    QRect top = QRect (0, 0, w + l + r, t);
    QRect bottom = QRect (0, t + h, w + l + r, b);
    QRect left = QRect (0, t, l, h);
    QRect right = QRect (l + w, t, r, h);
        
    QRegion rtop = reg & top;
    QRegion rbottom = reg & bottom;
    QRegion rleft = reg & left;
    QRegion rright = reg & right;

    QPixmap p = mPaintRedirector->performPendingPaint();

    QPainter pt (&mPixmapQt);
    pt.setCompositionMode( QPainter::CompositionMode_Source );
    
    QRect bb, pb;
 
    // Top
    if (!rtop.isEmpty ())
    {
      bb = rtop.boundingRect();
      pb = bb;
      pb.moveTo (bb.topLeft () - bBox.topLeft ());
      pt.resetTransform ();
      pt.setClipRegion( reg );
      pt.drawPixmap( bb.topLeft(), p, pb );
    }
    
    // Bottom
    if (!rbottom.isEmpty ())
    {
      bb = rbottom.boundingRect();
      pb = bb;
      pb.moveTo (bb.topLeft () - bBox.topLeft ());
      pt.resetTransform ();
      pt.translate(0, -h);
      pt.setClipRegion( reg );
      pt.drawPixmap( bb.topLeft(), p, pb );
    }
    
    // Left
    if (!rleft.isEmpty ())
    {
      bb = rleft.boundingRect();
      pb = bb;
      pb.moveTo (bb.topLeft () - bBox.topLeft ());
      pt.resetTransform ();
      pt.translate(0, t + b);
      pt.rotate (90);
      pt.scale (1.0, -1.0);
      pt.translate(0, -t);
      pt.setClipRegion( reg );
      pt.drawPixmap( bb.topLeft(), p, pb );
    }
    
    // Right
    if (!rright.isEmpty ())
    {
      bb = rright.boundingRect();
      pb = bb;
      pb.moveTo (bb.topLeft () - bBox.topLeft ());
      pt.resetTransform ();
      pt.translate(0, t + b + l);
      pt.rotate (90);
      pt.scale (1.0, -1.0);
      pt.translate(- (l + w), -t);
      pt.setClipRegion( reg );
      pt.drawPixmap( bb.topLeft(), p, pb );
    }

    

    if (mUpdateProperty)
	updateProperty ();
}

QWidget *
KWD::Window::decorWidget (void) const
{
    if (!mDecor)
	return 0;
    return mDecor->widget ();
}

QWidget *
KWD::Window::childAt (int x, int y) const
{
    if (!mDecor)
	return 0;

    QWidget *child = mDecor->widget ()->childAt (x + mPadding.left, y + mPadding.top);
    return (child)? child : decorWidget ();
}

QPoint 
KWD::Window::mapToChildAt (QPoint p) const
{
    if (!mDecor)
	return p;
    if (childAt (p.x (), p.y ()) == decorWidget ())
	return p + QPoint (mPadding.left, mPadding.right);
    return childAt (p.x (), p.y ())->mapFrom (decorWidget (), p + QPoint (mPadding.left, mPadding.right));
}

bool 
KWD::Window::eventFilter (QObject* o, QEvent* e)
{
    if (mDecor == NULL || o != mDecor->widget ())
	return false;
    if (e->type() == QEvent::Resize)
    {
	QResizeEvent* ev = static_cast<QResizeEvent*> (e);
	// Filter out resize events that inform about size different than frame size.
	// This will ensure that mDecor->width() etc. and mDecor->widget()->width() will be in sync.
	// These events only seem to be delayed events from initial resizing before show() was called
	// on the decoration widget.
	if (ev->size () != (mGeometry.size () + QSize (mExtents.left + mExtents.right, 
						       mExtents.top + mExtents.bottom)))
	{
	    int w = mGeometry.width () + mExtents.left + mExtents.right;
	    int h = mGeometry.height () + mExtents.top + mExtents.bottom;
    
	    mDecor->resize (QSize (w, h));
	    return true;
	}
	// HACK: Avoid decoration redraw delays. On resize Qt sets WA_WStateConfigPending
	// which delays all painting until a matching ConfigureNotify event comes.
	// But this process itself is the window manager, so it's not needed
	// to wait for that event, the geometry is known.
	// Note that if Qt in the future changes how this flag is handled and what it
	// triggers then this may potentionally break things. See mainly QETWidget::translateConfigEvent().
	mDecor->widget()->setAttribute( Qt::WA_WState_ConfigPending, false );
	mDecor->widget()->update();
	return false;
    }
    return false;
}
