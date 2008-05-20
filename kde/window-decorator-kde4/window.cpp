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

KWD::Window::Window (WId  parentId,
		     WId  clientId,
		     WId  frame,
		     Type type,
		     int  x,
		     int  y,
		     int  w,
		     int  h): QWidget (0, 0),
    mType (type),
    mParentId (parentId),
    mFrame (0),
    mClientId (clientId),
    mSelectedId (0),
    mDecor (0),
    mPixmap (0),
    mDamageId (0),
    mShadow (0),
    mPicture (0),
    mTexturePicture (0),
    mDecorationPicture (0),
    mUpdateProperty (false),
    mShapeSet (false),
    mUniqueHorzShape (false),
    mUniqueVertShape (false),
    mPopup (0),
    mAdvancedMenu (0),
    mOpacityMenu (0),
    mDesktopMenu (0),
    mMapped (false),
    mPendingMap (0),
    mPendingConfigure (0),
    mProcessKiller (0),
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
    mDesktopOpAction (0)
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

	    if (mIcon.isNull())
		mIcon = KWindowSystem::icon (mClientId, 16, 16, true,
					     KWindowSystem::NETWM |
					     KWindowSystem::WMHints );

	    mOpacity = readPropertyShort (mClientId, Atoms::netWmWindowOpacity,
					  0xffff);
	}
	else
	{
	    mIcon = QIcon ();
	    mName = QString ("");
	}

	updateFrame (frame);

	mGeometry = QRect (x, y, w, h);

	getWindowProtocols ();
    }
    else
    {
	mIcon = QIcon ();
	mName = QString ("");
	mGeometry = QRect (50, 50, 30, 1);
    }

    createDecoration ();

    mActiveChild = NULL;
}

KWD::Window::~Window (void)
{
    if (mShadow)
	decor_shadow_destroy (QX11Info::display(), mShadow);

    if (mPicture)
	XRenderFreePicture (QX11Info::display(), mPicture);

    if (mPixmap)
	XFreePixmap (QX11Info::display(), mPixmap);

    if (mTexturePicture)
	XRenderFreePicture (QX11Info::display(), mTexturePicture);

    if (mDecorationPicture)
	XRenderFreePicture (QX11Info::display(), mDecorationPicture);

    if (mDecor)
	delete mDecor;

    if (mPopup)
	delete mPopup;

    if (mProcessKiller)
	delete mProcessKiller;
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
    return mIcon;
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
    QPoint pnt;

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

    pnt = mapFromGlobal (pos);

    pnt += QPoint (mGeometry.x () - mBorder.left,
		   mGeometry.y () - mBorder.top);
 
    mPopup->exec (pnt);
}

void
KWD::Window::showWindowMenu (const QRect &pos)
{
    showWindowMenu (pos.bottomLeft ());
}

void
KWD::Window::processMousePressEvent (QMouseEvent *qme)
{
    Options::MouseCommand com = Options::MouseNothing;
    bool		  active = isActive ();

    if (!mSupportTakeFocus)
	active = TRUE;

    switch (qme->button ()) {
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
    QRect rect = QWidget::geometry ();

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
    return const_cast <Window *> (this);
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

void
KWD::Window::createDecoration (void)
{
    KDecoration *decor;

    if (mDecor)
	return;

    decor = Decorator::pluginManager ()->createDecoration (this);
    decor->init ();

    mDecor = decor;

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

    KWD::trapXError ();
    XSelectInput (QX11Info::display(), this->winId(),
		  StructureNotifyMask | PropertyChangeMask);
    KWD::popXError ();

    resizeDecoration (true);
}

static void
fillQRegion (Display *xdisplay,
	     Picture picture,
	     int     clipX1,
	     int     clipY1,
	     int     clipX2,
	     int     clipY2,
	     int     xOff,
	     int     yOff,
	     QRegion *region)
{
    static XRenderColor		   white = { 0xffff, 0xffff, 0xffff, 0xffff };
    QVector <QRect>		   rects = region->rects ();
    int				   x1, y1, x2, y2;

    foreach (QRect rect, rects)
    {
	x1 = rect.x ();
	y1 = rect.y ();
	x2 = x1 + rect.width ();
	y2 = y1 + rect.height ();

	if (x1 < clipX1)
	    x1 = clipX1;
	if (y1 < clipY1)
	    y1 = clipY1;
	if (x2 > clipX2)
	    x2 = clipX2;
	if (y2 > clipY2)
	    y2 = clipY2;

	if (x1 < x2 && y1 < y2)
	    XRenderFillRectangle (xdisplay, PictOpSrc, picture, &white,
				  xOff + x1,
				  yOff + y1,
				  x2 - x1,
				  y2 - y1);
    }
}

static void
drawBorderShape (Display	 *xdisplay,
		 Pixmap		 pixmap,
		 Picture	 picture,
		 int		 width,
		 int		 height,
		 decor_context_t *c,
		 void		 *closure)
{
    static XRenderColor clear = { 0x0000, 0x0000, 0x0000, 0x0000 };
    static XRenderColor white = { 0xffff, 0xffff, 0xffff, 0xffff };
    KWD::Window		*w = (KWD::Window *) closure;
    QRegion		*shape;
    bool		uniqueHorzShade;
    bool		uniqueVertShade;
    int			xOffLeft, yOffTop, xOffRight, yOffBottom;
    QRect		rect = w->geometry ();
    int			x1, y1, x2, y2;

    (void) pixmap;

    XRenderFillRectangle (xdisplay, PictOpSrc, picture, &clear,
			  0, 0, width, height);

    shape = w->getShape ();
    w->getShapeInfo (&uniqueHorzShade, &uniqueVertShade);

    xOffLeft = c->left_space - c->extents.left;
    yOffTop  = c->top_space  - c->extents.top;

    xOffRight  = c->left_space - c->extents.left;
    yOffBottom = c->top_space  - c->extents.top;

    x1 = c->left_space;
    y1 = c->top_space;
    x2 = width - c->right_space;
    y2 = height - c->bottom_space;

    if (shape)
    {
	if (uniqueHorzShade && uniqueVertShade)
	{
	    fillQRegion (xdisplay, picture,
			 0, 0,
			 rect.width (), rect.height (),
			 xOffLeft, yOffTop, shape);
	}
	else
	{
	    if (!uniqueHorzShade)
		xOffRight = x2 - (rect.width () - c->extents.right);

	    if (!uniqueVertShade)
		yOffBottom = y2 - (rect.height () - c->extents.bottom);

	    if (uniqueHorzShade)
	    {
		fillQRegion (xdisplay, picture,
			     0, 0,
			     rect.width (), c->extents.top,
			     xOffLeft, yOffTop, shape);
		fillQRegion (xdisplay, picture,
			     0, rect.height () - c->extents.bottom,
			     rect.width (), rect.height (),
			     xOffLeft, yOffBottom, shape);
	    }
	    else
	    {
		fillQRegion (xdisplay, picture,
			     0, 0,
			     c->extents.left, c->extents.top,
			     xOffLeft, yOffTop, shape);
		fillQRegion (xdisplay, picture,
			     rect.width () - c->extents.right, 0,
			     rect.width (), c->extents.top,
			     xOffRight, yOffTop, shape);
		fillQRegion (xdisplay, picture,
			     0, rect.height () - c->extents.bottom,
			     c->extents.left, rect.height (),
			     xOffLeft, yOffBottom, shape);
		fillQRegion (xdisplay, picture,
			     rect.width () - c->extents.right,
			     rect.height () - c->extents.bottom,
			     rect.width (), rect.height (),
			     xOffRight, yOffBottom, shape);

		y1 -= c->extents.top;
		y2 += c->extents.bottom;
	    }

	    if (uniqueVertShade)
	    {
		fillQRegion (xdisplay, picture,
			     0, c->extents.top,
			     c->extents.left,
			     rect.height () - c->extents.bottom,
			     xOffLeft, yOffTop, shape);
		fillQRegion (xdisplay, picture,
			     rect.width () - c->extents.right, c->extents.top,
			     rect.width (),
			     rect.height () - c->extents.bottom,
			     xOffRight, yOffTop, shape);
	    }
	    else
	    {
		x1 -= c->extents.left;
		x2 += c->extents.right;
	    }
	}
    }
    else
    {
	x1 -= c->extents.left;
	x2 += c->extents.right;
	y1 -= c->extents.top;
	y2 += c->extents.bottom;
    }

    XRenderFillRectangle (xdisplay, PictOpSrc, picture, &white,
			  x1,
			  y1,
			  x2 - x1,
			  y2 - y1);
}

static void
cornersFromQRegion (QRegion *region,
		    int     width,
		    int     height,
		    int     left,
		    int     right,
		    int     top,
		    int     bottom,
		    int	    *leftCorner,
		    int	    *rightCorner,
		    int	    *topCorner,
		    int	    *bottomCorner)
{
    QRegion l, r, t, b;

    l = QRegion (0, top, left, height - top - bottom) - *region;
    r = QRegion (width - right, top, right, height - top - bottom) - *region;
    t = QRegion (0, 0, width, top) - *region;
    b = QRegion (0, height - bottom, width, bottom) - *region;

    if (l.isEmpty ())
	*leftCorner = left;
    else
	*leftCorner = left -
	    (l.boundingRect ().x () + l.boundingRect ().width ());

    if (r.isEmpty ())
	*rightCorner = right;
    else
	*rightCorner = r.boundingRect ().x () - width + right;

    if (t.isEmpty ())
	*topCorner = top;
    else
	*topCorner = top -
	    (t.boundingRect ().y () + t.boundingRect ().height ());

    if (b.isEmpty ())
	*bottomCorner = bottom;
    else
	*bottomCorner = b.boundingRect ().y () - height + bottom;
}

void
KWD::Window::updateShadow (void)
{
    Display	      *xdisplay = QX11Info::display();
    Screen	      *xscreen;
    XRenderPictFormat *xformat;
    int		      leftCorner, rightCorner, topCorner, bottomCorner;

    xscreen = ScreenOfDisplay (xdisplay, QX11Info::appScreen ());

    if (mShadow)
    {
	decor_shadow_destroy (QX11Info::display(), mShadow);
	mShadow = NULL;
    }

    if (mShapeSet)
    {
	cornersFromQRegion (&mShape,
			    mGeometry.width () + mBorder.left + mBorder.right,
			    mGeometry.height () + mBorder.top + mBorder.bottom,
			    mBorder.left,
			    mBorder.right,
			    mBorder.top,
			    mBorder.bottom,
			    &leftCorner,
			    &rightCorner,
			    &topCorner,
			    &bottomCorner);
    }
    else
    {
	leftCorner   = mBorder.left;
	rightCorner  = mBorder.right;
	topCorner    = mBorder.top;
	bottomCorner = mBorder.bottom;
    }

    /* use default shadow if such exist */
    if (!mUniqueHorzShape && !mUniqueVertShape)
    {
	mShadow = Decorator::defaultWindowShadow (&mContext, &mBorder);
	if (mShadow)
	    decor_shadow_reference (mShadow);
    }

    if (!mShadow)
    {
	mShadow = decor_shadow_create (xdisplay,
				       xscreen,
				       mUniqueHorzShape ?
				       mGeometry.width () : 1,
				       mUniqueVertShape ?
				       mGeometry.height () : 1,
				       mBorder.left,
				       mBorder.right,
				       mBorder.top,
				       mBorder.bottom,
				       leftCorner,
				       rightCorner,
				       topCorner,
				       bottomCorner,
				       KWD::Decorator::shadowOptions (),
				       &mContext,
				       drawBorderShape,
				       (void *) this);

	if (mType == Default)
	    KWD::Decorator::updateDefaultShadow (this);
    }

    /* create new layout */
    if (mType == Normal)
	decor_get_best_layout (&mContext,
			       mGeometry.width (),
			       mGeometry.height (),
			       &mLayout);
    else
	decor_get_default_layout (&mContext,
				  mGeometry.width (),
				  mGeometry.height (),
				  &mLayout);

    if (mDecorationPicture)
	XRenderFreePicture (QX11Info::display(), mDecorationPicture);

    if (mTexturePicture)
	XRenderFreePicture (QX11Info::display(), mTexturePicture);

    mTexturePixmap       = QPixmap (mLayout.width, mLayout.height);
    mTexturePixmapBuffer = QPixmap (mLayout.width, mLayout.height);

    xformat = XRenderFindStandardFormat (QX11Info::display(),
					 PictStandardARGB32);

    mDecorationPicture =
	XRenderCreatePicture (QX11Info::display(),
			      mTexturePixmap.handle (),
			      xformat, 0, NULL);
    mTexturePicture =
	XRenderCreatePicture (QX11Info::display(),
			      mTexturePixmapBuffer.handle (),
			      xformat, 0, NULL);

    decor_fill_picture_extents_with_shadow (QX11Info::display(),
					    mShadow,
					    &mContext,
					    mTexturePicture,
					    &mLayout);

    if (mPixmap)
	mDecor->widget ()->repaint ();

    mUpdateProperty = true;
}

void
KWD::Window::setMask (const QRegion &reg, int)
{
    QRegion top, bottom, left, right;
    bool    uniqueHorzShape, uniqueVertShape;

    if (mShapeSet && reg == mShape)
	return;

    mShape    = reg;
    mShapeSet = true;

    if (mFrame)
    {
	QRegion r;

	r = reg - QRegion (mBorder.left, mBorder.top,
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

    top    = QRegion (mBorder.left, 0,
		      mGeometry.width (), mBorder.top) - reg;
    bottom = QRegion (mBorder.left, mGeometry.height () + mBorder.top,
		      mGeometry.width (), mBorder.bottom) - reg;
    left   = QRegion (0, mBorder.top, mBorder.left,
		      mGeometry.height ()) - reg;
    right  = QRegion (mBorder.left + mGeometry.width (), mBorder.top,
		      mBorder.right, mGeometry.height ()) - reg;

    uniqueHorzShape = !top.isEmpty ()  || !bottom.isEmpty ();
    uniqueVertShape = !left.isEmpty () || !right.isEmpty ();

    if (uniqueHorzShape || mUniqueHorzShape ||
	uniqueVertShape || mUniqueVertShape)
    {
	mUniqueHorzShape = uniqueHorzShape;
	mUniqueVertShape = uniqueVertShape;

	if (mPixmap)
	    updateShadow ();
    }
}

bool
KWD::Window::resizeDecoration (bool force)
{
    int w, h;

    mDecor->borders (mBorder.left, mBorder.right, mBorder.top, mBorder.bottom);

    w = mGeometry.width () + mBorder.left + mBorder.right;
    h = mGeometry.height () + mBorder.top + mBorder.bottom;

    if (!force)
    {
	if (w == width () && h == height ())
	    return FALSE;
    }

    /* reset shape */
    mShapeSet        = false;
    mUniqueHorzShape = false;
    mUniqueVertShape = false;

    if (mType != Normal)
    {
	Display		*xdisplay = QX11Info::display();
	Screen		*xscreen;
	decor_shadow_t  *tmpShadow;
	decor_context_t c;

	xscreen = ScreenOfDisplay (xdisplay, QX11Info::appScreen ());

	/* XXX: we have to create a temporary shadow to get the client
	   geometry. libdecoration should be fixed so it's able to just
	   fill out a context struct and not necessarily generate a
	   shadow for this purpose. */
	tmpShadow = decor_shadow_create (xdisplay,
					 xscreen,
					 1, 1,
					 mBorder.left,
					 mBorder.right,
					 mBorder.top,
					 mBorder.bottom,
					 mBorder.left,
					 mBorder.right,
					 mBorder.top,
					 mBorder.bottom,
					 KWD::Decorator::shadowOptions (),
					 &c,
					 decor_draw_simple,
					 (void *) 0);

	decor_shadow_destroy (xdisplay, tmpShadow);

	w = c.left_corner_space + 1 + c.right_corner_space;

	/* most styles render something useful at least 30 px width */
	if (w < 30)
	    w = 30;

	mGeometry = QRect (50, 50, w,
			   c.top_corner_space + 1 + c.bottom_corner_space);
    }

    w = mGeometry.width () + mBorder.left + mBorder.right;
    h = mGeometry.height () + mBorder.top + mBorder.bottom;

    if (mPixmap)
    {
	XFreePixmap (QX11Info::display(), mPixmap);
	mPixmap = None;
    }

    if (mPicture)
    {
	XRenderFreePicture (QX11Info::display(), mPicture);
	mPicture = 0;
    }

    if (w != width() || h != height())
    {
	mPendingConfigure = 1;
    }

    setGeometry (QRect (mGeometry.x () + ROOT_OFF_X - mBorder.left,
			mGeometry.y () + ROOT_OFF_Y - mBorder.top,
			w, h));
    XMoveResizeWindow (QX11Info::display(), winId(),
		       mGeometry.x () + ROOT_OFF_X - mBorder.left,
		       mGeometry.y () + ROOT_OFF_Y - mBorder.top,
		       w, h);

    mSize = QSize (w, h);

    if (!mMapped)
    {
	mPendingMap = 1;

	XReparentWindow (QX11Info::display(), winId (), mParentId, 0, 0);

	show ();

	mMapped = true;

	if (mDamageId != winId ())
	{
	    mDamageId = winId ();
	    XDamageCreate (QX11Info::display(), mDamageId,
			   XDamageReportRawRectangles);
	}
    }

    mDecor->resize (QSize (w, h));
    mDecor->widget ()->show ();
    mDecor->widget ()->update ();

    return TRUE;
}

void
KWD::Window::rebindPixmap (void)
{
    XRenderPictFormat *xformat;

    if (mPicture)
	XRenderFreePicture (QX11Info::display(), mPicture);

    if (mPixmap)
	XFreePixmap (QX11Info::display(), mPixmap);

    mPixmap = XCompositeNameWindowPixmap (QX11Info::display(), winId ());

    xformat = XRenderFindVisualFormat (QX11Info::display(),
				       (Visual *) QX11Info::appVisual ());

    mPicture = XRenderCreatePicture (QX11Info::display(), mPixmap,
				     xformat, 0, NULL);

    updateShadow ();

    mDecor->widget ()->update ();
}

bool
KWD::Window::handleMap (void)
{
    if (!mPendingMap)
	return FALSE;

    mPendingMap = 0;

    if (mPendingConfigure)
	return FALSE;

    rebindPixmap ();

    return TRUE;
}

bool
KWD::Window::handleConfigure (QSize size)
{
    if (!mPendingConfigure)
	return FALSE;

    if (size != mSize)
	return FALSE;

    mPendingConfigure = 0;
    if (mPendingConfigure || mPendingMap)
	return FALSE;

    rebindPixmap ();

    return TRUE;
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

    w = mGeometry.width () + mContext.extents.left + mContext.extents.right;
    h = mGeometry.height () + mContext.extents.top + mContext.extents.bottom;

    if (blurType != BLUR_TYPE_NONE)
    {
	QRegion r, shape = QRegion (0, 0, w, h);

	if (mShapeSet)
	    shape = mShape;

	r = QRegion (0, 0, w, mContext.extents.top);
	topQRegion = r.intersect (shape);
	if (!topQRegion.isEmpty ())
	{
	    topQRegion.translate (-mContext.extents.left,
				  -mContext.extents.top);
	    topRegion = topQRegion.handle ();
	}

	if (blurType == BLUR_TYPE_ALL)
	{
	    r = QRegion (0, h - mContext.extents.bottom,
			 w, mContext.extents.bottom);
	    bottomQRegion = r.intersect (shape);
	    if (!bottomQRegion.isEmpty ())
	    {
		bottomQRegion.translate (-mContext.extents.left,
					 -(h - mContext.extents.bottom));
		bottomRegion = bottomQRegion.handle ();
	    }

	    r = QRegion (0, mContext.extents.top,
			 mContext.extents.left, mGeometry.height ());
	    leftQRegion = r.intersect (shape);
	    if (!leftQRegion.isEmpty ())
	    {
		leftQRegion.translate (-mContext.extents.left,
				       -mContext.extents.top);
		leftRegion = leftQRegion.handle ();
	    }

	    r = QRegion (w - mContext.extents.right, mContext.extents.top,
			 mContext.extents.right, mGeometry.height ());
	    rightQRegion = r.intersect (shape);
	    if (!rightQRegion.isEmpty ())
	    {
		rightQRegion.translate (-(w - mContext.extents.right),
					-mContext.extents.top);
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
    int		    nQuad;
    int		    lh, rh;
    int		    w;
    int		    minWidth;
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

    if (mLayout.rotation)
	lh = mLayout.left.x2 - mLayout.left.x1;
    else
	lh = mLayout.left.y2 - mLayout.left.y1;

    if (mLayout.rotation)
	rh = mLayout.right.x2 - mLayout.right.x1;
    else
	rh = mLayout.right.y2 - mLayout.right.y1;

    w = mLayout.top.x2 - mLayout.top.x1 - mContext.left_space -
	mContext.right_space;

    if (mType == Normal)
    {
	int	topXOffset = w / 2;
	QWidget *widget = mDecor->widget ();
	int	x;

	x = w - mContext.left_space - mContext.left_corner_space;
	if (x > topXOffset)
	    topXOffset = x;

	if (widget)
	{
	    const QList<QObject*> children = widget->children ();

	    foreach (QObject *obj, children)
	    {
		QWidget *child;

		if (!obj->isWidgetType ())
		    continue;

		child = static_cast <QWidget *> (obj);

		x = child->x () - mBorder.left - 2;
		if (x > w / 2 && x < topXOffset)
		    topXOffset = x;
	    }
	}

	nQuad = decor_set_lXrXtXbX_window_quads (quads,
						 &mContext,
						 &mLayout,
						 lh / 2,
						 rh / 2,
						 topXOffset,
						 w / 2);

	updateBlurProperty (topXOffset, w / 2, lh / 2, rh / 2);

	minWidth = mContext.left_corner_space + 1 + mContext.right_corner_space;
    }
    else
    {
	nQuad = decor_set_lSrStSbS_window_quads (quads, &mContext, &mLayout);

	minWidth = 1;
    }

    decor_quads_to_property (data, mTexturePixmap.handle (),
			     &mBorder, &maxExtents,
			     minWidth, 0,
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
    KWindowInfo wInfo = KWindowSystem::windowInfo (mClientId, NET::WMGeometry);
    QRect	geometry = wInfo.geometry ();
    int         w, h;

    w = mGeometry.width () + mBorder.left + mBorder.right;
    h = mGeometry.height () + mBorder.top + mBorder.bottom;

    if (mGeometry.width ()  != geometry.width () ||
	mGeometry.height () != geometry.height ())
    {
	mGeometry = geometry;

	if (resizeDecoration ())
	    return;
    }
    else if (mGeometry.x ()  != geometry.x () ||
	mGeometry.y () != geometry.y ())
    {
	mGeometry = geometry;
    }

    move (mGeometry.x () + ROOT_OFF_X - mBorder.left,
	  mGeometry.y () + ROOT_OFF_Y - mBorder.top);
}

void
KWD::Window::reloadDecoration (void)
{
    delete mDecor;
    mDecor = 0;

    mMapped   = false;
    mShapeSet = false;

    if (mShadow)
    {
	decor_shadow_destroy (QX11Info::display(), mShadow);
	mShadow = NULL;
    }

    createDecoration ();
}

Cursor
KWD::Window::positionToCursor (QPoint pos)
{
    switch (mDecor->mousePosition (pos)) {
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

    if (mIcon.isNull())
	mIcon = KWindowSystem::icon (mClientId, 16, 16, true,
				     KWindowSystem::NETWM |
				     KWindowSystem::WMHints);
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

    XUngrabPointer (QX11Info::display(), CurrentTime);
    XUngrabKeyboard (QX11Info::display(), CurrentTime);

    Decorator::rootInfo ()->restackRequest (mClientId, NET::FromApplication,
			 		    None, Above,
					    QX11Info::appTime());
    Decorator::rootInfo ()->moveResizeRequest (mClientId,
					       qme->globalX (),
					       qme->globalY (),
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
    {
	QPoint mp (0, 0);

	if (qme)
	    mp = mapToGlobal (qme->pos ());

	showWindowMenu (mp);
    } break;
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
KWD::Window::processDamage (void)
{
    QRegion r1, r2;
    int     xOff, yOff, w;
    double  alpha;
    int     shade_alpha;

    if (isActive ())
    {
	alpha	    = activeDecorationOpacity;
	shade_alpha = activeDecorationOpacityShade;
    }
    else
    {
	alpha	    = decorationOpacity;
	shade_alpha = decorationOpacityShade;
    }

    if (!mPixmap)
	return;

    if (mDamage.isEmpty ())
	return;

    if (mShapeSet)
	mDamage = mShape.intersect (mDamage);

    w = mGeometry.width () + mContext.extents.left + mContext.extents.right;

    xOff = 0;
    yOff = 0;

    r1 = QRegion (xOff, yOff, w, mContext.extents.top);
    r2 = r1.intersect (mDamage);

    if (!r2.isEmpty ())
    {
	r2.translate (-xOff, -yOff);

	decor_blend_top_border_picture (QX11Info::display(),
					&mContext,
					mPicture,
					xOff, xOff,
					mTexturePicture,
					&mLayout,
					r2.handle (),
					(unsigned short) (alpha * 0xffff),
					shade_alpha);
    }

    xOff = 0;
    yOff = mContext.extents.top + mGeometry.height ();

    r1 = QRegion (xOff, yOff, w, mContext.extents.bottom);
    r2 = r1.intersect (mDamage);

    if (!r2.isEmpty ())
    {
	r2.translate (-xOff, -yOff);

	decor_blend_bottom_border_picture (QX11Info::display(),
					   &mContext,
					   mPicture,
					   xOff, yOff,
					   mTexturePicture,
					   &mLayout,
					   r2.handle (),
					   (unsigned short) (alpha * 0xffff),
					   shade_alpha);
    }

    xOff = 0;
    yOff = mContext.extents.top;

    r1 = QRegion (xOff, yOff, mContext.extents.left, mGeometry.height ());
    r2 = r1.intersect (mDamage);

    if (!r2.isEmpty ())
    {
	r2.translate (-xOff, -yOff);

	decor_blend_left_border_picture (QX11Info::display(),
					 &mContext,
					 mPicture,
					 xOff, yOff,
					 mTexturePicture,
					 &mLayout,
					 r2.handle (),
					 (unsigned short) (alpha * 0xffff),
					 shade_alpha);
    }

    xOff = mContext.extents.left + mGeometry.width ();
    yOff = mContext.extents.top;

    r1 = QRegion (xOff, yOff, mContext.extents.right, mGeometry.height ());
    r2 = r1.intersect (mDamage);

    if (!r2.isEmpty ())
    {
	r2.translate (-xOff, -yOff);

	decor_blend_right_border_picture (QX11Info::display(),
					  &mContext,
					  mPicture,
					  xOff, yOff,
					  mTexturePicture,
					  &mLayout,
					  r2.handle (),
					  (unsigned short) (alpha * 0xffff),
					  shade_alpha);
    }

    mDamage = QRegion ();

    XRenderComposite (QX11Info::display(),
		      PictOpSrc,
		      mTexturePicture,
		      None,
		      mDecorationPicture,
		      0, 0,
		      0, 0,
		      0, 0,
		      mTexturePixmap.width (),
		      mTexturePixmap.height ());

    if (mUpdateProperty)
	updateProperty ();
}

void
KWD::Window::handleProcessKillerExited (void)
{
    if (mProcessKiller)
    {
	delete mProcessKiller;
	mProcessKiller = NULL;
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

    if (mProcessKiller)
	return;

    clientMachine = kWinInfo.clientMachine ();
    resourceClass = kWinInfo.windowClassClass ();
    pid		  = wInfo.pid ();

    if (gethostname (buf, sizeof (buf) - 1) == 0)
    {
	if (strcmp (buf, clientMachine) == 0)
	    clientMachine = "localhost";
    }

    mProcessKiller = new QProcess (this);

    connect (mProcessKiller, SIGNAL (finished (int, QProcess::ExitStatus)),
	     SLOT (handleProcessKillerExited ()));
    connect (mProcessKiller, SIGNAL (error (QProcess::ProcessError)),
	     SLOT (handleProcessKillerExited ()));
    

    mProcessKiller->start (KStandardDirs::findExe ("kwin_killer_helper"),
	QStringList () << "--pid" << QByteArray ().setNum (pid) <<
	"--hostname" << clientMachine <<
	"--windowname" << mName.toUtf8 () <<
	"--applicationname" << resourceClass <<
	"--wid" << QByteArray ().setNum ((unsigned int) mClientId) <<
	"--timestamp" << QByteArray ().setNum ((unsigned int) timestamp));
}

void
KWD::Window::hideKillProcessDialog (void)
{
    handleProcessKillerExited ();
}
