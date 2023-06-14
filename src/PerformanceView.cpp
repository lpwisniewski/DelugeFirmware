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
	xScrollBeforeFollowingAutoExtendingLinearRecording = -1;
}

bool PerformanceView::opened() {

	if (playbackHandler.playbackState && currentPlaybackMode == &arrangement) PadLEDs::skipGreyoutFade();

	IndicatorLEDs::setLedState(crossScreenEditLedX, crossScreenEditLedY, false);
	IndicatorLEDs::setLedState(scaleModeLedX, scaleModeLedY, false);

	focusRegained();

	return true;
}

void PerformanceView::focusRegained() {

	redrawClipsOnScreen(doingRender); // We want this here, not just in opened(), because after coming back from
	                                  // loadInstrumentPresetUI, need to at least redraw, and also really need to
	                                  // re-render stuff in case note-tails-being-allowed has changed

	ClipNavigationTimelineView::focusRegained();
	view.focusRegained();
	view.setActiveModControllableTimelineCounter(currentSong);

	selectedClipYDisplay = 255;
	IndicatorLEDs::setLedState(backLedX, backLedY, false);

	setLedStates();

	currentSong->lastClipInstanceEnteredStartPos = -1;
}

bool PerformanceView::renderMainPads(uint32_t whichRows, uint8_t image[][displayWidth + sideBarWidth][3],
                                    uint8_t occupancyMask[][displayWidth + sideBarWidth], bool drawUndefinedArea) {
	int x = 0;
	for (Output* output = currentSong->firstOutput; output; output = output->next) {
		int numClips = output.clipInstances.getNumElements();
		for (int clipId = 0; clipId < numClips; clip++) {
			Clip* clip = output.clipInstances.getElement(clipId);
			image[y][clip][0] = 255;
			image[y][clip][1] = 255;
			image[y][clip][2] = 255;
		}
		y++;
	}
}
