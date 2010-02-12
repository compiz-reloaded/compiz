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

#include "utils.h"

#include <decoration.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <stdlib.h>
#include <QX11Info>

static int trappedErrorCode = 0;

namespace KWD
{
    namespace Atoms
    {
	Atom switchSelectWindow;
	Atom switchFgColor;
	Atom netWmWindowOpacity;
	Atom netFrameWindow;
	Atom netWindowDecor;
	Atom netWindowDecorNormal;
	Atom netWindowDecorActive;
	Atom netWindowDecorBare;
	Atom wmTakeFocus;
	Atom netWmContextHelp;
	Atom wmProtocols;
	Atom toolkitActionAtom;
	Atom toolkitActionWindowMenuAtom;
	Atom toolkitActionForceQuitDialogAtom;
        Atom compizWindowBlurDecor;
	Atom enlightmentDesktop;
    }
}

static int (*oldErrorHandler) (Display *display, XErrorEvent *error);

static int
xErrorHandler (Display	   *display,
	       XErrorEvent *error)
{
    (void) display;

    trappedErrorCode = error->error_code;

    return 0;
}

void
KWD::trapXError (void)
{
    trappedErrorCode = 0;
    oldErrorHandler = XSetErrorHandler (xErrorHandler);
}

int
KWD::popXError (void)
{
    XSync (QX11Info::display(), false);
    XSetErrorHandler (oldErrorHandler);

    return trappedErrorCode;
}

void *
KWD::readXProperty (WId  window,
		    Atom property,
		    Atom type,
		    int  *items)
{
    long	  offset = 0, length = 2048L;
    Atom	  actualType;
    int		  format;
    unsigned long nItems, bytesRemaining;
    unsigned char *data = 0l;
    int		  result;

    KWD::trapXError ();
    result = XGetWindowProperty (QX11Info::display(), window, property, offset,
				 length, false, type,
				 &actualType, &format, &nItems,
				 &bytesRemaining, &data);

    if (KWD::popXError ())
      return NULL;

    if (result == Success && actualType == type && format == 32 && nItems > 0)
    {
	if (items)
	    *items = nItems;

	return reinterpret_cast <void *>(data);
    }

    if (data)
	XFree (data);

    if (items)
	*items = 0;

    return NULL;
}

bool
KWD::readWindowProperty (long window,
			 long property,
			 long *value)
{
    void *data = readXProperty (window, property, XA_WINDOW, NULL);

    if (data)
    {
	if (value)
	    *value = *reinterpret_cast <int *>(data);

	XFree (data);

	return true;
    }

    return false;
}

unsigned short
KWD::readPropertyShort (WId	       id,
			Atom	       property,
			unsigned short defaultValue)
{
    Atom	  actual;
    int		  result, format;
    unsigned long n, left;
    unsigned char *data;

    KWD::trapXError ();
    result = XGetWindowProperty (QX11Info::display(), id, property,
				 0L, 1L, FALSE, XA_CARDINAL, &actual, &format,
				 &n, &left, &data);
    if (KWD::popXError ())
	return defaultValue;

    if (result == Success && n && data)
    {
	unsigned int value;

	memcpy (&value, data, sizeof (unsigned int));

	XFree (data);

	return value >> 16;
    }

    return defaultValue;
}

void
KWD::Atoms::init (void)
{
    Display *xdisplay = QX11Info::display();

    netFrameWindow = XInternAtom (xdisplay, "_NET_FRAME_WINDOW", false);
    netWindowDecor = XInternAtom (xdisplay, DECOR_WINDOW_ATOM_NAME, false);
    netWindowDecorNormal =
	XInternAtom (xdisplay, DECOR_NORMAL_ATOM_NAME, false);
    netWindowDecorActive =
	XInternAtom (xdisplay, DECOR_ACTIVE_ATOM_NAME, false);
    netWindowDecorBare =
	XInternAtom (xdisplay, DECOR_BARE_ATOM_NAME, false);
    switchSelectWindow =
	XInternAtom (xdisplay, DECOR_SWITCH_WINDOW_ATOM_NAME, false);
    switchFgColor =
	XInternAtom (xdisplay, DECOR_SWITCH_FOREGROUND_COLOR_ATOM_NAME, false);
    wmTakeFocus = XInternAtom (xdisplay, "WM_TAKE_FOCUS", false);
    netWmContextHelp =
	XInternAtom (xdisplay, "_NET_WM_CONTEXT_HELP", false);
    wmProtocols = XInternAtom (xdisplay, "WM_PROTOCOLS", false);
    netWmWindowOpacity =
	XInternAtom (xdisplay, "_NET_WM_WINDOW_OPACITY", false);
    toolkitActionAtom = XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION", false);
    toolkitActionWindowMenuAtom =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_WINDOW_MENU", false);
    toolkitActionForceQuitDialogAtom =
	XInternAtom (xdisplay, "_COMPIZ_TOOLKIT_ACTION_FORCE_QUIT_DIALOG",
		     false);
    compizWindowBlurDecor =
	XInternAtom (xdisplay, DECOR_BLUR_ATOM_NAME, false);
    enlightmentDesktop = XInternAtom (xdisplay, "ENLIGHTENMENT_DESKTOP", false);
}
