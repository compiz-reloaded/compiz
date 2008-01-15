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

#ifndef _WINDOW_H
#define _WINDOW_H

#include <kdecoration_p.h>

#include <qpixmap.h>
#include <qwidget.h>

#include <decoration.h>

#include <X11/extensions/Xdamage.h>

#include "utils.h"
#include "options.h"

class KProcess;
class KDecoration;
class QPopupMenu;

namespace KWD
{
class Window:public QWidget, public KDecorationBridge {
    Q_OBJECT public:

	enum Type
	{
	    Normal,
	    Switcher,
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
	virtual QIconSet icon (void) const;
	virtual QString caption (void) const;
	virtual void processMousePressEvent (QMouseEvent *);
	virtual void showWindowMenu (const QRect &);
	virtual void showWindowMenu (QPoint);
	virtual void performWindowOperation (WindowOperation);
	virtual void setMask (const QRegion &, int);
	virtual bool isPreview (void) const;
	virtual QRect geometry (void) const;
	virtual QRect iconGeometry (void) const;
	virtual QRegion unobscuredRegion (const QRegion & r) const;
	virtual QWidget *workspaceWidget (void) const;
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
	virtual void helperShowHide (bool);
	virtual void grabXServer (bool grab);

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
	QWidget *activeChild (void) const
	{
	    return mActiveChild;
	}
	void setActiveChild (QWidget * child)
	{
	    mActiveChild = child;
	}
	QRegion *getShape (void)
	{
	    if (mShapeSet)
		return &mShape;

	    return NULL;
	}
	void getShapeInfo (bool *horz, bool *vert)
	{
	    *horz = mUniqueHorzShape;
	    *vert = mUniqueVertShape;
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
	void addDamageRect (int x, int y, int w, int h)
	{
	    mDamage += QRegion (x, y, w, h);
	}
	bool handleMap (void);
	bool handleConfigure (QSize size);
	void processDamage (void);
	decor_context_t *context (void)
	{
	    return &mContext;
	}
	decor_shadow_t *shadow (void)
	{
	    return mShadow;
	}
	decor_extents_t *border (void)
	{
	    return &mBorder;
	}
	QRect clientGeometry (void);
	void showKillProcessDialog (Time timestamp);
	void hideKillProcessDialog (void);

    private:
	void createDecoration (void);
	void updateShadow (void);
	bool resizeDecoration (bool force = false);
	void updateBlurProperty (int topOffset,
				 int bottomOffset,
				 int leftOffset,
				 int rightOffset);
	void updateProperty (void);
	void getWindowProtocols (void);
	void performMouseCommand (KWD::Options::MouseCommand command,
				  QMouseEvent		     *qme);
	NET::Direction positionToDirection (int pos);
	Cursor positionToCursor (QPoint pos);
	void rebindPixmap (void);


    private slots:
	void handlePopupActivated (int id);
	void handleDesktopPopupActivated (int id);
	void handlePopupAboutToShow (void);
	void handleProcessKillerExited (void);

    private:
	Type mType;
	WId mParentId;
	WId mFrame;
	WId mClientId;
	WId mSelectedId;
	QRect mGeometry;
	QString mName;
	QIconSet mIcons;
	decor_extents_t mBorder;
	unsigned short mOpacity;
	KDecoration *mDecor;
	QPixmap mTexturePixmap;
	QPixmap mTexturePixmapBuffer;
	Pixmap mPixmap;
	QRegion mDamage;
	WId mDamageId;
	decor_layout_t mLayout;
	decor_context_t mContext;
	decor_shadow_t *mShadow;
	Picture mPicture;
	Picture mTexturePicture;
	Picture mDecorationPicture;
	bool mUpdateProperty;
	bool mShapeSet;
	bool mUniqueHorzShape;
	bool mUniqueVertShape;
	QRegion mShape;
	QWidget *mActiveChild;
	bool mSupportTakeFocus;
	bool mSupportContextHelp;
	QPopupMenu *mPopup;
	QPopupMenu *mAdvancedMenu;
	QPopupMenu *mDesktopMenu;
	unsigned long mState;
	bool mMapped;
	int mPendingMap;
	int mPendingConfigure;
	QSize mSize;
	KProcess *mProcessKiller;
    };
}

#endif
