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

#include <KDE/Plasma/FrameSvg>
#include <KDE/Plasma/Theme>

#include <kwindowsystem.h>

#include <QString>
#include <QPainter>

KWD::Switcher::Switcher (WId parentId, WId id):
    mId (id),
    mX11Pixmap (0),
    mX11BackgroundPixmap (0)
{
    QPalette palette;
    long     prop[4];
    QColor   color;
    color = Plasma::Theme::defaultTheme ()->color (Plasma::Theme::TextColor);

    mBackground = new Plasma::FrameSvg();
    mBackground->setImagePath ("dialogs/background");
    mBackground->setEnabledBorders(Plasma::FrameSvg::AllBorders);

    mBorder.left   = mBackground->marginSize(Plasma::LeftMargin);
    mBorder.right  = mBackground->marginSize(Plasma::RightMargin);
    mBorder.top    = mBackground->marginSize(Plasma::TopMargin);
    mBorder.bottom = mBackground->marginSize(Plasma::BottomMargin) +
		     Plasma::Theme::defaultTheme ()->fontMetrics ().height () + 10;

    mContext.extents.left   = mBorder.left;
    mContext.extents.right  = mBorder.right;
    mContext.extents.top    = mBorder.top;
    mContext.extents.bottom = mBorder.bottom;

    mContext.left_space   = mBorder.left;
    mContext.right_space  = mBorder.right;
    mContext.top_space    = mBorder.top;
    mContext.bottom_space = mBorder.bottom;

    mContext.left_corner_space   = 0;
    mContext.right_corner_space  = 0;
    mContext.top_corner_space    = 0;
    mContext.bottom_corner_space = 0;

    updateGeometry ();

    prop[0] = (color.red () * 256) + color.red ();
    prop[1] = (color.green () * 256) + color.green ();
    prop[2] = (color.blue () * 256) + color.blue ();
    prop[3] = (color.alpha () * 256) + color.alpha ();

    KWD::trapXError ();
    XChangeProperty (QX11Info::display (), id, Atoms::switchFgColor, XA_INTEGER,
		     32, PropModeReplace, (unsigned char *) prop, 4);
    KWD::popXError ();
}

KWD::Switcher::~Switcher ()
{
    if (mX11Pixmap)
	XFreePixmap (QX11Info::display (), mX11Pixmap);
    if (mX11BackgroundPixmap)
	XFreePixmap (QX11Info::display (), mX11BackgroundPixmap);
    delete mBackground;
}

void
KWD::Switcher::updateGeometry ()
{
    int x, y;
    unsigned int width, height, border, depth;
    XID root;

    XGetGeometry (QX11Info::display (), mId, &root, &x, &y, &width, &height,
		  &border, &depth);

    mGeometry = QRect (x, y, width, height);

    KWD::readWindowProperty (mId, Atoms::switchSelectWindow,
			     (long *)&mSelected);

    if (mX11Pixmap)
	XFreePixmap (QX11Info::display (), mX11Pixmap);
    if (mX11BackgroundPixmap)
	XFreePixmap (QX11Info::display (), mX11BackgroundPixmap);

    mX11Pixmap = XCreatePixmap (QX11Info::display (),
				QX11Info::appRootWindow (),
				width + mBorder.left + mBorder.right,
				height + mBorder.top + mBorder.bottom, 32);

    mX11BackgroundPixmap = XCreatePixmap (QX11Info::display (),
					  QX11Info::appRootWindow (),
					  width, height, 32);

    mPixmap = QPixmap::fromX11Pixmap (mX11Pixmap, QPixmap::ExplicitlyShared);
    mBackgroundPixmap = QPixmap::fromX11Pixmap (mX11BackgroundPixmap,
	    					QPixmap::ExplicitlyShared);

    redrawPixmap ();
    update ();

    decor_get_default_layout (&mContext,
			      mGeometry.width (),
			      mGeometry.height (),
			      &mDecorLayout);

    updateWindowProperties ();
}

void
KWD::Switcher::redrawPixmap ()
{
    QPainter p (&mPixmap);
    QPainter bp (&mBackgroundPixmap);

    const int contentWidth  = mPixmap.width ();
    const int contentHeight = mPixmap.height ();

    mPixmap.fill (Qt::transparent);

    p.setCompositionMode (QPainter::CompositionMode_Source);
    p.setRenderHint (QPainter::SmoothPixmapTransform);

    mBackground->resizeFrame (QSizeF (contentWidth, contentHeight));
    mBackground->paintFrame (&p, QRect (0, 0, contentWidth, contentHeight));

    bp.drawPixmap (0, 0, mPixmap, mBorder.left, mBorder.top,
		   mGeometry.width (), mGeometry.height ());

    XSetWindowBackgroundPixmap (QX11Info::display (), mId,
				mX11BackgroundPixmap);

    XClearWindow (QX11Info::display (), mId);
}

void
KWD::Switcher::update ()
{
    QFontMetrics fm = Plasma::Theme::defaultTheme ()->fontMetrics ();
    QFont font (Plasma::Theme::defaultTheme ()->
		font (Plasma::Theme::DefaultFont));
    QString name;
    QPainter p (&mPixmap);

    KWD::readWindowProperty (mId, Atoms::switchSelectWindow,
			     (long *)&mSelected);

    name = KWindowSystem::windowInfo
	   (mSelected, NET::WMVisibleName, 0).visibleName ();

    while (fm.width (name) > mGeometry.width ())
    {
        name.truncate (name.length () - 6);
        name += "...";
    }

    p.setCompositionMode (QPainter::CompositionMode_Source);

    mBackground->paintFrame (&p, QRect (mBorder.left,
					mBorder.top +
					mGeometry.height () + 5,
					mGeometry.width (),
					fm.height ()));

    p.setFont (font);
    p.setPen (Plasma::Theme::defaultTheme ()->color(Plasma::Theme::TextColor));

    p.drawText ((mPixmap.width () - fm.width (name)) / 2,
                mBorder.top + mGeometry.height () + 5 + fm.ascent (), name);
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
    decor_quads_to_property (data, mX11Pixmap,
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
