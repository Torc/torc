/* Class UIActions
*
* This file is part of the Torc project.
*
* Copyright (C) Mark Kendall 2012
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

// Qt
#include <QKeyEvent>

// Torc
#include "torclocalcontext.h"
#include "uiactions.h"

UIActions::UIActions()
{
}

UIActions::~UIActions()
{
}

int UIActions::GetActionFromKey(QKeyEvent *Event)
{
    if (!Event)
        return Torc::None;

    int key  = Event->key();
    int type = Event->type();
    int mods = Event->modifiers();

    bool internal = (mods == TORC_KEYEVENT_MODIFIERS || mods == TORC_KEYEVENT_PRINTABLE_MODIFIERS);

    if (internal)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("Simulated keypress from %1").arg(Event->text()));
        mods = Qt::NoModifier;
    }

    // ignore straight modifier keypresses
    if ((key == Qt::Key_Shift  ) || (key == Qt::Key_Control   ) ||
        (key == Qt::Key_Meta   ) || (key == Qt::Key_Alt       ) ||
        (key == Qt::Key_Super_L) || (key == Qt::Key_Super_R   ) ||
        (key == Qt::Key_Hyper_L) || (key == Qt::Key_Hyper_R   ) ||
        (key == Qt::Key_AltGr  ) || (key == Qt::Key_CapsLock  ) ||
        (key == Qt::Key_NumLock) || (key == Qt::Key_ScrollLock))
    {
        return Torc::None;
    }

    // overlay modifiers
    key &= ~Qt::UNICODE_ACCEL;
    key |= mods & Qt::ShiftModifier ? (key != Qt::Key_Backtab ? Qt::SHIFT : 0) : 0;
    key |= mods & Qt::ControlModifier ? Qt::CTRL : 0;
    key |= mods & Qt::MetaModifier    ? Qt::META : 0;
    key |= mods & Qt::AltModifier     ? Qt::ALT  : 0;

    // pressed and released
    if (key == Qt::Key_Return ||
        key == Qt::Key_Enter  ||
        key == Qt::Key_Select)
    {
        LOG(VB_GENERAL, LOG_DEBUG, QString("KeyPress/Release %1").arg(key, 0, 16));
        return Event->type() == QEvent::KeyPress ? Torc::Pushed : Torc::Released;
    }

    // NB we generally trigger on KeyPress events as the release sequence when
    // modifiers are used is not certain (e.g. for Ctrl+F1, Ctrl may be released first
    // and hence we see releases for Ctrl and F1 but not Ctrl+F1).
    if (type == QEvent::KeyRelease)
        return Torc::None;

    LOG(VB_GENERAL, LOG_DEBUG, QString("KeyPress %1 (%2)")
        .arg(key, 0, 16).arg(Event->text()));

    // NB these are temporary
    switch (key)
    {
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
            return Torc::BackSpace;
        case Qt::Key_Down:
            return Torc::Down;
        case Qt::Key_Up:
            return Torc::Up;
        case Qt::Key_Left:
            return Torc::Left;
        case Qt::Key_Right:
            return Torc::Right;
        case Qt::Key_Escape:
            return Torc::Escape;
        case Qt::Key_F1:
        case Qt::Key_PowerOff:
            return Torc::Suspend;
        case Qt::Key_P:
            return Torc::Pause;
        case Qt::Key_O:
            return Torc::Play;
        case Qt::Key_Space:
            return Torc::TogglePlayPause;
        case Qt::Key_Q:
            return Torc::Stop;
        case Qt::Key_Comma:
            return Torc::JumpBackwardSmall;
        case Qt::Key_Period:
            return Torc::JumpForwardSmall;
        case Qt::Key_T:
            return Torc::DisableStudioLevels;
        case Qt::Key_Y:
            return Torc::EnableStudioLevels;
        case Qt::Key_R:
            return Torc::ToggleStudioLevels;
        case Qt::Key_A:
            return Torc::EnableHighQualityScaling;
        case Qt::Key_S:
            return Torc::DisableHighQualityScaling;
        case Qt::Key_D:
            return Torc::ToggleHighQualityScaling;
        case Qt::Key_Z:
            return Torc::DecreaseBrightness;
        case Qt::Key_X:
            return Torc::IncreaseBrightness;
        case Qt::Key_C:
            return Torc::DecreaseContrast;
        case Qt::Key_V:
            return Torc::IncreaseContrast;
        case Qt::Key_B:
            return Torc::DecreaseSaturation;
        case Qt::Key_N:
            return Torc::IncreaseSaturation;
        case Qt::Key_M:
            return Torc::DecreaseHue;
        case Qt::Key_Slash:
            return Torc::IncreaseHue;
        default:
            break;
    }

    return Torc::None;
}
