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

#ifndef _SWITCHER_H
#define _SWITCHER_H

#include <QPixmap>
#include <QRect>
#include <QX11Info>
#include <decoration.h>
#include <fixx11h.h>

namespace Plasma
{
class FrameSvg;
}

class QSpacerItem;
class QLabel;
class QVBoxLayout;

namespace KWD
{

class Switcher
{

    public:
	Switcher (WId parentId, WId id);
	~Switcher ();

	void update ();
	void updateGeometry ();

	WId xid () const
	{
	    return mId;
	}

    private:
	void updateWindowProperties ();
	void updateBlurProperty (int topOffset,
				 int bottomOffset,
				 int leftOffset,
				 int rightOffset);
 	void redrawPixmap ();

    private:

	WId mId;
	WId mSelected;

	QRect mGeometry;

	Plasma::FrameSvg *mBackground;
	QPixmap mPixmap;
	Pixmap mX11Pixmap;
	QPixmap mBackgroundPixmap;
	Pixmap mX11BackgroundPixmap;

	decor_layout_t mDecorLayout;
	decor_context_t mContext;
	decor_extents_t mBorder;
};

}

#endif
