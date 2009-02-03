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

#ifndef _UTILS_H
#define _UTILS_H

#include <X11/Xlib.h>
#include <fixx11h.h>
#include <qwidget.h>

namespace KWD
{
    namespace Atoms
    {
	extern Atom switchSelectWindow;
	extern Atom netWmWindowOpacity;
	extern Atom netFrameWindow;
	extern Atom netWindowDecor;
	extern Atom netWindowDecorNormal;
	extern Atom netWindowDecorActive;
	extern Atom netWindowDecorBare;
	extern Atom wmTakeFocus;
	extern Atom netWmContextHelp;
	extern Atom wmProtocols;
	extern Atom toolkitActionAtom;
	extern Atom toolkitActionWindowMenuAtom;
	extern Atom toolkitActionForceQuitDialogAtom;
	extern Atom compizWindowBlurDecor;

	void init (void);
    }

    void trapXError (void);
    int popXError (void);
    bool eventFilter (void *message, long *result);
    void *readXProperty (WId window, Atom property, Atom type, int *items);
    bool readWindowProperty (long wId, long property, long *value);
    unsigned short readPropertyShort (WId	     id,
				      Atom	     property,
				      unsigned short defaultValue);
}

#endif
