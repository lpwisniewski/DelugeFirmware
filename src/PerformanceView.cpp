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

#include "ArrangerView.h"
#include <AudioEngine.h>
#include <AudioFileManager.h>
#include <ClipInstance.h>
#include <DString.h>
#include <InstrumentClip.h>
#include <InstrumentClipMinder.h>
#include <InstrumentClipView.h>
#include <ParamManager.h>
#include <SessionView.h>
#include <PerformanceView.h>
#include "functions.h"
#include "numericdriver.h"
#include "uart.h"
#include "View.h"
#include "NoteRow.h"
#include "UI.h"
#include "ActionLogger.h"
#include "GeneralMemoryAllocator.h"
#include "Session.h"
#include "instrument.h"
#include "Arrangement.h"
#include <new>
#include "storagemanager.h"
#include "LoadInstrumentPresetUI.h"
#include "AudioClip.h"
#include "AudioOutput.h"
#include "AudioInputSelector.h"
#include "song.h"
#include "KeyboardScreen.h"
#include "AudioClipView.h"
#include "WaveformRenderer.h"
#include "SampleRecorder.h"
#include "MelodicInstrument.h"
#include "MenuItemColour.h"
#include "PadLEDs.h"
#include "IndicatorLEDs.h"
#include "Buttons.h"
#include "extern.h"
#include "Sample.h"
#include "playbackhandler.h"
#include "loadsongui.h"
#include "uitimermanager.h"
#include "FileItem.h"

#if HAVE_OLED
#include "oled.h"
#endif

extern "C" {
#include "cfunctions.h"
#include "sio_char.h"
}

PerformanceView performanceView;

extern int8_t defaultAudioClipOverdubOutputCloning;

PerformanceView::PerformanceView() {
}

bool PerformanceView::opened() {
	numericDriver.displayPopup(HAVE_OLED ? "Clip not empty" : "OPEN");
	renderMainPads(0xFFFFFFFF, PadLEDs::imageStore, PadLEDs::occupancyMaskStore);
	renderSidebar(0xFFFFFFFF, PadLEDs::imageStore, PadLEDs::occupancyMaskStore);
	uiNeedsRendering(this);
	return true;
}

void PerformanceView::focusRegained() {
	view.focusRegained();
	currentSong->lastClipInstanceEnteredStartPos = -1;
}

bool PerformanceView::getGreyoutRowsAndCols(uint32_t* cols, uint32_t* rows) {
	return true;
}

bool PerformanceView::renderMainPads(uint32_t whichRows, uint8_t image[][displayWidth + sideBarWidth][3],
                                    uint8_t occupancyMask[][displayWidth + sideBarWidth], bool drawUndefinedArea) {
	if (!image) return true;

	memset(image, 0, sizeof(uint8_t) * displayHeight * (displayWidth + sideBarWidth) * 3);
	memset(occupancyMask, 0, sizeof(uint8_t) * displayHeight * (displayWidth + sideBarWidth));

	// For each Clip in session and arranger for specific Output
	int x = 0;
	for (Output* output = currentSong->firstOutput; output; output = output->next) {
		int numElements = currentSong->sessionClips.getNumElements();
		int clipId = 0;
		char* str = new char[3]();
		intToString(clipId, str, 2);
		numericDriver.displayPopup(HAVE_OLED ? "Clip not empty" : str);
		for (int c = 0; c < numElements; c++) {
			Clip* clip;
			clip = currentSong->sessionClips.getClipAtIndex(c);
			if (clip->output != output) continue;
			else {
				//hueToRGB(defaultClipGroupColours[clip->section], image[displayHeight - clip->section - 1][x]);
				hueToRGB(defaultClipGroupColours[x % 12], image[displayHeight - clip->section - 1][x]);
				occupancyMask[x][clipId] = 64;
				clipId++;
			}
		}

		x++;
	}

	return true;
}

int PerformanceView::buttonAction(int x, int y, bool on, bool inCardRoutine) {
	return 1;
}

void PerformanceView::goToArrangementEditor() {
	currentSong->xZoomForReturnToSongView = currentSong->xZoom[NAVIGATION_CLIP];
	currentSong->xScrollForReturnToSongView = currentSong->xScroll[NAVIGATION_CLIP];
	changeRootUI(&arrangerView);
}

bool PerformanceView::renderSidebar(uint32_t whichRows, uint8_t image[][displayWidth + sideBarWidth][3],
                                uint8_t occupancyMask[][displayWidth + sideBarWidth]) {
	if (!image) return true;

	int numElements = currentSong->sessionClips.getNumElements();
	for (int c = 0; c < numElements; c++) {
		Clip* clip;
		clip = currentSong->sessionClips.getClipAtIndex(c);
		//hueToRGB(defaultClipGroupColours[clip->section], image[displayHeight - clip->section - 1][x]);
		hueToRGB(defaultClipGroupColours[clip->section % 12], image[displayHeight - clip->section - 1][displayWidth + sideBarWidth - 1]);
		memset(image[displayHeight - clip->section - 1][displayWidth + sideBarWidth - 2], 0, 3);
		occupancyMask[displayHeight - clip->section - 1][displayWidth + sideBarWidth - 1] = 64;
		occupancyMask[displayHeight - clip->section - 1][displayWidth + sideBarWidth - 2] = 64;
	}

	return true;
}

void PerformanceView::graphicsRoutine() {
}
