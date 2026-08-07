/*
 * The G2 Editor application.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __G2_EDITOR_H__
#define __G2_EDITOR_H__

#include <atomic>
#include <string>
#include <cstring>

#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

// Builds the plug-in's editor window. Defined in g2Editor.mm because the view it creates is Cocoa;
// keeping the declaration here means g2Vst3.cpp needs no Objective-C.
//
// The returned view is owned by the caller (refcount 1) and is handed straight back to the host.
Steinberg::IPlugView * g2_create_editor_view(Steinberg::Vst::IEditController * controller,
                                             Steinberg::Vst::IComponentHandler * handler,
                                             const char * patchPath);

#endif
