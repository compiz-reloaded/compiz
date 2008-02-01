/*
 * Copyright © 2008 Dennis Kasprzyk <onestone@opencompositing.org>
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
 * Author: Dennis Kasprzyk <onestone@opencompositing.org>
 */

#include "switcher.h"
#include "utils.h"
#include "decorator.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/Xregion.h>

#include <fixx11h.h>

#include <KDE/Plasma/Dialog>
#include <KDE/Plasma/Theme>

#include <kwindowsystem.h>

#include <QLabel>
#include <QLayout>
#include <QString>

KWD::Switcher::Switcher (WId parentId, WId id):
mId (id)
{
    QPalette palette;
    long     prop[4];
    QColor   color = Plasma::Theme::self ()->textColor ();

    mDialog = new Plasma::Dialog ();

    mLayout = new QVBoxLayout ();
    mLayout->setSpacing (0);
    mLayout->setMargin (0);

    mSpacer = new QSpacerItem (0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);

    mLabel = new QLabel ();
    mLabel->setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);
    palette = mLabel->palette();
    palette.setBrush (QPalette::WindowText, QBrush(color));
    mLabel->setPalette (palette);
    mLabel->setAlignment (Qt::AlignHCenter);
    mLabel->setFixedHeight (Plasma::Theme::self ()->fontMetrics ().height ());

    mLayout->addItem (mSpacer);
    mLayout->addWidget (mLabel);

    mDialog->setLayout (mLayout);

    mDialog->setWindowFlags(Qt::FramelessWindowHint |
			    Qt::X11BypassWindowManagerHint);
    mDialog->adjustSize();

    XReparentWindow (QX11Info::display (), mDialog->winId (), parentId, 0, 0);
    mDialog->show ();

    mPendingMap = 1;

    updateGeometry ();

    prop[0] = (color.red() * 256) + color.red();
    prop[1] = (color.green() * 256) + color.green();
    prop[2] = (color.blue() * 256) + color.blue();
    prop[3] = (color.alpha() * 256) + color.alpha();

    KWD::trapXError ();
    XChangeProperty (QX11Info::display (), id, Atoms::switchFgColor, XA_INTEGER,
		     32, PropModeReplace, (unsigned char *) prop, 4);
    KWD::popXError ();
}

KWD::Switcher::~Switcher ()
{
    delete mDialog;
}

void
KWD::Switcher::updateGeometry ()
{
    int x, y;
    unsigned int width, height, border, depth, bgColor;
    XID root;
    QColor bg;

    XGetGeometry (QX11Info::display (), mId, &root, &x, &y, &width, &height,
		  &border, &depth);

    mGeometry = QRect (x, y, width, height);

    KWD::readWindowProperty (mId, Atoms::switchSelectWindow,
			     (long *)&mSelected);

    bg = Plasma::Theme::self()->backgroundColor();
    bgColor = (bg.alpha () << 24) + (bg.red () << 24) + (bg.green () << 16) +
	      bg.blue ();
    XSetWindowBackground (QX11Info::display (), mId, bgColor);
    XClearWindow (QX11Info::display (), mId);
    XSync (QX11Info::display (), FALSE);

    mSpacer->changeSize (mGeometry.width (), mGeometry.height (),
			 QSizePolicy::Fixed, QSizePolicy::Fixed);
    mLabel->setFixedWidth (mGeometry.width ());

    mDialog->adjustSize ();
    mPendingConfigure = 1;

    update ();
}

void
KWD::Switcher::rebindPixmap (void)
{
    if (mPixmap)
	XFreePixmap (QX11Info::display (), mPixmap);

    mPixmap = XCompositeNameWindowPixmap (QX11Info::display (),
					  mDialog->winId ());

    mDialog->update ();

    mContext.extents.left   = mLayout->geometry ().y ();
    mContext.extents.right  = mDialog->width () - mLayout->geometry ().x () -
			      mGeometry.width ();
    mContext.extents.top    = mLayout->geometry ().y ();
    mContext.extents.bottom = mDialog->height () - mLayout->geometry ().y () -
			      mGeometry.height ();

    mContext.left_space   = mContext.extents.left;
    mContext.right_space  = mContext.extents.right;
    mContext.top_space    = mContext.extents.top;
    mContext.bottom_space = mContext.extents.bottom;

    mContext.left_corner_space   = 0;
    mContext.right_corner_space  = 0;
    mContext.top_corner_space    = 0;
    mContext.bottom_corner_space = 0;

    mBorder.left   = mContext.extents.left;
    mBorder.right  = mContext.extents.right;
    mBorder.top    = mContext.extents.top;
    mBorder.bottom = mContext.extents.bottom;

    decor_get_default_layout (&mContext,
			      mGeometry.width (),
			      mGeometry.height (),
			      &mDecorLayout);

    updateWindowProperties ();
}

bool
KWD::Switcher::handleMap (void)
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
KWD::Switcher::handleConfigure (QSize size)
{
    if (!mPendingConfigure)
	return FALSE;

    mPendingConfigure = 0;
    if (mPendingConfigure || mPendingMap)
	return FALSE;

    rebindPixmap ();

    return TRUE;
}

void
KWD::Switcher::update ()
{
    QFontMetrics fm = Plasma::Theme::self ()->fontMetrics ();
    QString name;

    KWD::readWindowProperty (mId, Atoms::switchSelectWindow,
			     (long *)&mSelected);

    name = KWindowSystem::windowInfo
	   (mSelected, NET::WMVisibleName, 0).visibleName ();

    while (fm.width (name) > mGeometry.width ())
    {
        name.truncate (name.length () - 6);
        name += "...";
    }

    mLabel->setText (name);
}

void
KWD::Switcher::updateWindowProperties ()
{
    long	    data[256];
    decor_quad_t    quads[N_QUADS_MAX];
    int		    nQuad;
    int		    lh, rh;
    int		    w;

    lh = mDecorLayout.left.y2 - mDecorLayout.left.y1;
    rh = mDecorLayout.right.y2 - mDecorLayout.right.y1;

    w = mDecorLayout.top.x2 - mDecorLayout.top.x1 - mContext.left_space -
	mContext.right_space;

    nQuad = decor_set_lXrXtXbX_window_quads (quads, &mContext, &mDecorLayout,
					     lh / 2, rh / 2, w, w / 2);

    decor_quads_to_property (data, mPixmap,
			     &mBorder, &mBorder,
			     0, 0,
			     quads, nQuad);

    KWD::trapXError ();
    XChangeProperty (QX11Info::display(), mId, Atoms::netWindowDecor,
		     XA_INTEGER, 32, PropModeReplace, (unsigned char *) data,
		     BASE_PROP_SIZE + QUAD_PROP_SIZE * nQuad);
    KWD::popXError ();

    updateBlurProperty (lh / 2, rh / 2, w / 2, w / 2);
}

void
KWD::Switcher::updateBlurProperty (int topOffset,
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
	QRegion r;
	
	topQRegion    = QRegion (-mContext.extents.left, -mContext.extents.top,
				 w, mContext.extents.top);
	topRegion     = topQRegion.handle ();

	bottomQRegion = QRegion (-mContext.extents.left, 0,
				 w, mContext.extents.bottom);
	bottomRegion  = bottomQRegion.handle ();

	leftQRegion   = QRegion (-mContext.extents.left, 0,
				 mContext.extents.left, mGeometry.height ());
	leftRegion    = leftQRegion.handle ();

	rightQRegion  = QRegion (0, 0, mContext.extents.right,
				 mGeometry.height ());
	rightRegion   = rightQRegion.handle ();
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
	XChangeProperty (QX11Info::display (), mId, atom, XA_INTEGER,
			 32, PropModeReplace, (unsigned char *) data,
			 2 + size * 6);
	KWD::popXError ();
    }
    else
    {
	KWD::trapXError ();
	XDeleteProperty (QX11Info::display (), mId, atom);
	KWD::popXError ();
    }
}


WId
KWD::Switcher::dialogId ()
{
    return mDialog->winId ();
}
