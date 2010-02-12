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

#ifndef _WINDOW_H
#define _WINDOW_H

#include <kdecorationbridge.h>
#include <KDE/KActionCollection>
#include <kdeversion.h>

#include <qpixmap.h>
#include <qwidget.h>
#include <qprocess.h>

#include <decoration.h>

#include <X11/extensions/Xdamage.h>

#include "utils.h"
#include "options.h"

class QProcess;
class KDecoration;
class KActionCollection;
class QMenu;

namespace KWin
{
    class PaintRedirector;
}

namespace KWD
{
class Window: public QObject, public KDecorationBridgeUnstable {
    Q_OBJECT public:

	enum Type
	{
	    Normal,
	    Default,
	    DefaultActive
	};

    public:
	Window (WId parentId, WId clientId, WId frame, Type type,
		int x = 0, int y = 0, int w = 1, int h = 1);
	~Window (void);

	virtual bool isActive (void) const;
	virtual bool isCloseable (void) const;
	virtual bool isMaximizable (void) const;
	virtual MaximizeMode maximizeMode (void) const;
	virtual bool isMinimizable (void) const;
	virtual bool providesContextHelp (void) const;
	virtual int desktop (void) const;
	virtual bool isModal (void) const;
	virtual bool isShadeable (void) const;
	virtual bool isShade (void) const;
	virtual bool isSetShade (void) const;
	virtual bool keepAbove (void) const;
	virtual bool keepBelow (void) const;
	virtual bool isMovable (void) const;
	virtual bool isResizable (void) const;
	virtual NET::WindowType
	    windowType (unsigned long supported_types) const;
	virtual QIcon icon (void) const;
	virtual QString caption (void) const;
	virtual void processMousePressEvent (QMouseEvent *);
	virtual void showWindowMenu (const QRect &);
	virtual void showWindowMenu (const QPoint &);
	virtual void performWindowOperation (WindowOperation);
	virtual void setMask (const QRegion &, int);
	virtual bool isPreview (void) const;
	virtual QRect geometry (void) const;
	virtual QRect iconGeometry (void) const;
	virtual QRegion unobscuredRegion (const QRegion & r) const;
	virtual WId windowId (void) const;
	virtual void closeWindow (void);
	virtual void maximize (MaximizeMode mode);
	virtual void minimize (void);
	virtual void showContextHelp (void);
	virtual void setDesktop (int desktop);
	virtual void titlebarDblClickOperation (void);
	virtual void titlebarMouseWheelOperation (int delta);
	virtual void setShade (bool set);
	virtual void setKeepAbove (bool);
	virtual void setKeepBelow (bool);
	virtual int currentDesktop (void) const;
	virtual QWidget *initialParentWidget (void) const;
	virtual Qt::WFlags initialWFlags (void) const;
	virtual void grabXServer (bool grab);

	/* unstable API */
	virtual bool compositingActive () const;
#if KDE_IS_VERSION(4,3,90)
	virtual QRect transparentRect () const;

	virtual bool isClientGroupActive ();
	virtual QList<ClientGroupItem> clientGroupItems () const;
	virtual long itemId (int index);
	virtual int visibleClientGroupItem ();
	virtual void setVisibleClientGroupItem (int index);
	virtual void moveItemInClientGroup (int index, int before);
	virtual void moveItemToClientGroup (long itemId, int before);
	virtual void removeFromClientGroup (int index, const QRect& newGeom);
	virtual void closeClientGroupItem (int index);
	virtual void closeAllInClientGroup ();
	virtual void displayClientMenu (int index, const QPoint& pos);

	virtual WindowOperation
	    buttonToWindowOperation(Qt::MouseButtons button);
#endif
	virtual bool eventFilter (QObject* o, QEvent* e);

	void handleActiveChange (void);
	void updateFrame (WId frame);
	void updateWindowGeometry (void);
	void updateCursor (QPoint pos);
	void updateSelected (WId selected);
	
	WId frameId (void) const
	{
	    return mFrame;
	}
	
	KDecoration *decoration (void) const
	{
	    return mDecor;
	}
	
	QWidget *decorWidget (void) const;
	QWidget *childAt (int x, int y) const;
	QPoint mapToChildAt (QPoint p) const;
	
	QWidget *activeChild (void) const
	{
	    return mActiveChild;
	}
	
	void setActiveChild (QWidget * child)
	{
	    mActiveChild = child;
	}
	
	void moveWindow (QMouseEvent *qme);
	void reloadDecoration (void);
	void updateState (void);
	void updateName (void);
	void updateIcons (void);
	void updateOpacity (void)
	{
	    mOpacity = readPropertyShort (mClientId, Atoms::netWmWindowOpacity,
					  0xffff);
	}
	Drawable pixmapId (void) const
	{
	    return mPixmap;
	}
	
	bool handleMap (void);
	bool handleConfigure (QSize size);
	
	decor_extents_t *border (void)
	{
	    return &mBorder;
	}

	QRect clientGeometry (void);
	void showKillProcessDialog (Time timestamp);
	void hideKillProcessDialog (void);

	void setFakeRelease (bool fakeRelease)
	{
	    mFakeRelease = fakeRelease;
	}

	bool getFakeRelease ()
	{
	    return mFakeRelease;
	}
	

    private:
	void createDecoration (void);
	void resizeDecoration (bool force = false);
	void updateBlurProperty (int topOffset,
				 int bottomOffset,
				 int leftOffset,
				 int rightOffset);
	void updateProperty (void);
	void getWindowProtocols (void);

	Options::MouseCommand buttonToCommand (Qt::MouseButtons button);
	void performMouseCommand (KWD::Options::MouseCommand command,
				  QMouseEvent                *qme);
	NET::Direction positionToDirection (int pos);
	Cursor positionToCursor (QPoint pos);

    private slots:
	void handlePopupActivated (QAction *action);
	void handleOpacityPopupActivated (QAction *action);
	void handleDesktopPopupActivated (QAction *action);
	void handlePopupAboutToShow (void);

	void decorRepaintPending ();

    private:
	Type mType;
	WId mParentId;
	WId mFrame;
	WId mClientId;
	WId mSelectedId;
	QRect mGeometry;
	QString mName;
	QPixmap mIcon;
	QPixmap mMiniIcon;
	decor_extents_t mBorder;
	decor_extents_t mPadding;
	decor_extents_t mExtents;
	unsigned short mOpacity;
	KDecoration *mDecor;
	Pixmap mPixmap;
	QPixmap mPixmapQt;
	bool mUpdateProperty;
	bool mShapeSet;
	QRegion mShape;
	QWidget *mActiveChild;
	bool mSupportTakeFocus;
	bool mSupportContextHelp;
	QMenu *mPopup;
	QMenu *mAdvancedMenu;
	QMenu *mOpacityMenu;
	QMenu *mDesktopMenu;
	unsigned long mState;

	QProcess mProcessKiller;
	KActionCollection mKeys;
	bool mFakeRelease;

	QAction *mResizeOpAction;
        QAction *mMoveOpAction;
        QAction *mMaximizeOpAction;
        QAction *mShadeOpAction;
        QAction *mKeepAboveOpAction;
        QAction *mKeepBelowOpAction;
        QAction *mFullScreenOpAction;
        QAction *mNoBorderOpAction;
        QAction *mMinimizeOpAction;
        QAction *mCloseOpAction;
	QAction *mDesktopOpAction;

	KWin::PaintRedirector *mPaintRedirector;
    };
}

#endif
