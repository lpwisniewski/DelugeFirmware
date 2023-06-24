/*
 * Copyright © 2014-2023 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef OverviewModeUI_h
#define OverviewModeUI_h

#include <stdint.h>
#include "ClipNavigationTimelineView.h"

class Editor;
class InstrumentClip;
class Clip;
class ModelStack;

extern float getTransitionProgress();

// Clip Group colours
extern const uint8_t numDefaultClipGroupColours;
extern const uint8_t defaultClipGroupColours[];

class PerformanceView final : public RootUI {
public:
	PerformanceView();
	bool opened();
	void focusRegained();
	void graphicsRoutine();
	bool renderMainPads(uint32_t whichRows, uint8_t image[][displayWidth + sideBarWidth][3],
                                    uint8_t occupancyMask[][displayWidth + sideBarWidth], bool drawUndefinedArea);
	bool getGreyoutRowsAndCols(uint32_t* cols, uint32_t* rows);
	int buttonAction(int x, int y, bool on, bool inCardRoutine);
	void goToArrangementEditor();
	bool renderSidebar(uint32_t whichRows, uint8_t image[][displayWidth + sideBarWidth][3],
                                uint8_t occupancyMask[][displayWidth + sideBarWidth]);

	void graphicsRoutine();
}
extern PerformanceView performanceView;

#endif
