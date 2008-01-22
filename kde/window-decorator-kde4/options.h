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

#ifndef _OPTIONS_H
#define _OPTIONS_H

#include <kdecoration.h>

class KConfig;

namespace KWD
{

class Options : public KDecorationOptions
    {
    public:
	enum MouseCommand
	{
	    MouseRaise,
	    MouseLower,
	    MouseOperationsMenu,
	    MouseToggleRaiseAndLower,
	    MouseActivateAndRaise,
	    MouseActivateAndLower,
	    MouseActivate,
	    MouseActivateRaiseAndPassClick,
	    MouseActivateAndPassClick,
	    MouseMove,
	    MouseUnrestrictedMove,
	    MouseActivateRaiseAndMove,
	    MouseActivateRaiseAndUnrestrictedMove,
	    MouseResize,
	    MouseUnrestrictedResize,
	    MouseShade,
	    MouseSetShade,
	    MouseUnsetShade,
	    MouseMaximize,
	    MouseRestore,
	    MouseMinimize,
	    MouseNextDesktop,
	    MousePreviousDesktop,
	    MouseAbove,
	    MouseBelow,
	    MouseOpacityMore,
	    MouseOpacityLess,
	    MouseNothing
	};
	enum MouseWheelCommand
	{
	    MouseWheelRaiseLower,
	    MouseWheelShadeUnshade,
	    MouseWheelMaximizeRestore,
	    MouseWheelAboveBelow,
	    MouseWheelPreviousNextDesktop,
	    MouseWheelChangeOpacity,
	    MouseWheelNothing
	};

	Options (KConfig *config);

	virtual unsigned long updateSettings (void);

	WindowOperation operationTitlebarDblClick (void)
	{
	    return OpTitlebarDblClick;
	}

	MouseCommand commandActiveTitlebar1 (void)
	{
	    return CmdActiveTitlebar1;
	}
	MouseCommand commandActiveTitlebar2 (void)
	{
	    return CmdActiveTitlebar2;
	}
	MouseCommand commandActiveTitlebar3 (void)
	{
	    return CmdActiveTitlebar3;
	}
	MouseCommand commandInactiveTitlebar1 (void)
	{
	    return CmdInactiveTitlebar1;
	}
	MouseCommand commandInactiveTitlebar2 (void)
	{
	    return CmdInactiveTitlebar2;
	}
	MouseCommand commandInactiveTitlebar3 (void)
	{
	    return CmdInactiveTitlebar3;
	}

	MouseCommand operationTitlebarMouseWheel (int delta)
	{
	    return wheelToMouseCommand (CmdTitlebarWheel, delta);
	}

    private:
	static KDecorationDefines::WindowOperation
	    windowOperation (const QString &name, bool restricted);
	MouseCommand mouseCommand (const QString &name, bool restricted);
	MouseWheelCommand mouseWheelCommand (const QString &name);
	MouseCommand wheelToMouseCommand (MouseWheelCommand com, int delta);

    private:
	KDecorationDefines::WindowOperation OpTitlebarDblClick;
	MouseCommand CmdActiveTitlebar1;
	MouseCommand CmdActiveTitlebar2;
	MouseCommand CmdActiveTitlebar3;
	MouseCommand CmdInactiveTitlebar1;
	MouseCommand CmdInactiveTitlebar2;
	MouseCommand CmdInactiveTitlebar3;
	MouseWheelCommand CmdTitlebarWheel;

	KConfig *mConfig;
    };

}

#endif
