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

#include "options.h"

#include <kconfig.h>
#include <kdecoration_p.h>

KWD::Options::Options (KConfig *config): KDecorationOptions (), mConfig (config)
{
    d = new KDecorationOptionsPrivate;
    updateSettings ();
}

unsigned long
KWD::Options::updateSettings (void)
{
    unsigned long changed = 0;

    changed |= d->updateKWinSettings (mConfig);

    mConfig->setGroup ("Windows");

    OpTitlebarDblClick =
	windowOperation (mConfig->readEntry ("TitlebarDoubleClickCommand",
					     "Shade"), true);
    d->OpMaxButtonLeftClick =
      windowOperation (mConfig->readEntry ("MaximizeButtonLeftClickCommand",
					   "Maximize"), true);
    d->OpMaxButtonMiddleClick =
	windowOperation (mConfig->readEntry ("MaximizeButtonMiddleClickCommand",
					     "Maximize (vertical only)"), true);
    d->OpMaxButtonRightClick =
	windowOperation (mConfig->readEntry ("MaximizeButtonRightClickCommand",
					     "Maximize (horizontal only)"),
			 true);

    mConfig->setGroup ("MouseBindings");

    CmdActiveTitlebar1 =
	mouseCommand (mConfig->readEntry ("CommandActiveTitlebar1",
					   "Raise"), true);
    CmdActiveTitlebar2 =
	mouseCommand (mConfig->readEntry ("CommandActiveTitlebar2",
					   "Lower"), true);
    CmdActiveTitlebar3 =
	mouseCommand (mConfig->readEntry ("CommandActiveTitlebar3",
					   "Operations menu"), true);
    CmdInactiveTitlebar1 =
	mouseCommand (mConfig->readEntry ("CommandInactiveTitlebar1",
					   "Activate and raise"), true);
    CmdInactiveTitlebar2 =
	mouseCommand (mConfig->readEntry ("CommandInactiveTitlebar2",
					   "Activate and lower"), true);
    CmdInactiveTitlebar3 =
	mouseCommand (mConfig->readEntry ("CommandInactiveTitlebar3",
					   "Operations menu"), true);

    CmdTitlebarWheel =
	mouseWheelCommand (mConfig->readEntry ("CommandTitlebarWheel",
						"Nothing"));

    return changed;
}

// restricted should be true for operations that the user may not be able to
// repeat if the window is moved out of the workspace (e.g. if the user moves
// a window by the titlebar, and moves it too high beneath Kicker at the top
// edge, they may not be able to move it back, unless they know about Alt+LMB)
KDecorationDefines::WindowOperation
KWD::Options::windowOperation (const QString &name, bool restricted)
{
    if (name == "Move")
	return restricted ? KWD::Options::MoveOp :
	    KWD::Options::UnrestrictedMoveOp;
    else if (name == "Resize")
	return restricted ? KWD::Options::ResizeOp :
	    KWD::Options::UnrestrictedResizeOp;
    else if (name == "Maximize")
	return KWD::Options::MaximizeOp;
    else if (name == "Minimize")
	return KWD::Options::MinimizeOp;
    else if (name == "Close")
	return KWD::Options::CloseOp;
    else if (name == "OnAllDesktops")
	return KWD::Options::OnAllDesktopsOp;
    else if (name == "Shade")
	return KWD::Options::ShadeOp;
    else if (name == "Operations")
	return KWD::Options::OperationsOp;
    else if (name == "Maximize (vertical only)")
	return KWD::Options::VMaximizeOp;
    else if (name == "Maximize (horizontal only)")
	return KWD::Options::HMaximizeOp;
    else if (name == "Lower")
	return KWD::Options::LowerOp;
    return KWD::Options::NoOp;
}

KWD::Options::MouseCommand
KWD::Options::mouseCommand (const QString &name,
			    bool	  restricted)
{
    QString lowerName = name.lower ();

    if (lowerName == "raise") return MouseRaise;
    if (lowerName == "lower") return MouseLower;
    if (lowerName == "operations menu") return MouseOperationsMenu;
    if (lowerName == "toggle raise and lower") return MouseToggleRaiseAndLower;
    if (lowerName == "activate and raise") return MouseActivateAndRaise;
    if (lowerName == "activate and lower") return MouseActivateAndLower;
    if (lowerName == "activate") return MouseActivate;
    if (lowerName == "activate, raise and pass click")
	return MouseActivateRaiseAndPassClick;
    if (lowerName == "activate and pass click")
	return MouseActivateAndPassClick;
    if (lowerName == "activate, raise and move")
	return restricted ? MouseActivateRaiseAndMove :
	    MouseActivateRaiseAndUnrestrictedMove;
    if (lowerName == "move")
	return restricted ? MouseMove : MouseUnrestrictedMove;
    if (lowerName == "resize")
	return restricted ? MouseResize : MouseUnrestrictedResize;
    if (lowerName == "shade") return MouseShade;
    if (lowerName == "minimize") return MouseMinimize;
    if (lowerName == "nothing") return MouseNothing;

    return MouseNothing;
}

KWD::Options::MouseWheelCommand
KWD::Options::mouseWheelCommand (const QString &name)
{
    QString lowerName = name.lower ();

    if (lowerName == "raise/lower") return MouseWheelRaiseLower;
    if (lowerName == "shade/unshade") return MouseWheelShadeUnshade;
    if (lowerName == "maximize/restore") return MouseWheelMaximizeRestore;
    if (lowerName == "above/below") return MouseWheelAboveBelow;
    if (lowerName == "previous/next desktop")
	return MouseWheelPreviousNextDesktop;
    if (lowerName == "change opacity") return MouseWheelChangeOpacity;

    return MouseWheelNothing;
}

KWD::Options::MouseCommand
KWD::Options::wheelToMouseCommand (MouseWheelCommand com,
				   int		     delta)
{
    switch (com) {
    case MouseWheelRaiseLower:
	return delta > 0 ? MouseRaise : MouseLower;
    case MouseWheelShadeUnshade:
	return delta > 0 ? MouseSetShade : MouseUnsetShade;
    case MouseWheelMaximizeRestore:
	return delta > 0 ? MouseMaximize : MouseRestore;
    case MouseWheelAboveBelow:
	return delta > 0 ? MouseAbove : MouseBelow;
    case MouseWheelPreviousNextDesktop:
	return delta > 0 ? MousePreviousDesktop : MouseNextDesktop;
    case MouseWheelChangeOpacity:
	return delta > 0 ? MouseOpacityMore : MouseOpacityLess;
    default:
	return MouseNothing;
    }
}
