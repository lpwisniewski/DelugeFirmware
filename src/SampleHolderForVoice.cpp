/*
 * Copyright © 2015-2023 Synthstrom Audible Limited
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

#include <AudioFileManager.h>
#include <SampleHolderForVoice.h>
#include "Sample.h"
#include "storagemanager.h"
#include "source.h"
#include "numericdriver.h"

SampleHolderForVoice::SampleHolderForVoice() {
	loopStartPos = 0;
	loopEndPos = 0;

	transpose = 0;
	cents = 0;

	// For backwards compatibility
	startMSec = 0;
	endMSec = 0;

	for (int l = 0; l < NUM_CLUSTERS_LOADED_AHEAD; l++) {
		clustersForLoopStart[l] = NULL;
	}
}

SampleHolderForVoice::~SampleHolderForVoice() {
	// We have to unassign reasons here, even though our parent destructor will call unassignAllReasons() - our overriding of that virtual function
	// won't happen as we've already been destructed!
	for (int l = 0; l < NUM_CLUSTERS_LOADED_AHEAD; l++) {
		if (clustersForLoopStart[l]) {
			audioFileManager.removeReasonFromCluster(clustersForLoopStart[l], "E247");
		}
	}
}

void SampleHolderForVoice::unassignAllClusterReasons(bool beingDestructed) {
	SampleHolder::unassignAllClusterReasons(beingDestructed);
	for (int l = 0; l < NUM_CLUSTERS_LOADED_AHEAD; l++) {
		if (clustersForLoopStart[l]) {
			audioFileManager.removeReasonFromCluster(clustersForLoopStart[l],
			                                         "E320"); // Happened to me while auto-pilot testing, I think
			if (!beingDestructed) clustersForLoopStart[l] = NULL;
		}
	}
}

// Reassesses which Clusters we want to be a "reason" for.
// Ensure there is a sample before you call this.
void SampleHolderForVoice::claimClusterReasons(bool reversed, int clusterLoadInstruction) {

#if ALPHA_OR_BETA_VERSION
	if (!audioFile) numericDriver.freezeWithError("i030"); // Trying to narrow down E368 that Kevin F got
#endif

	SampleHolder::claimClusterReasons(reversed, clusterLoadInstruction);

	int playDirection = reversed ? -1 : 1;
	int bytesPerSample = ((Sample*)audioFile)->numChannels * ((Sample*)audioFile)->byteDepth;

	int loopStartPlaybackAtSample = reversed ? loopEndPos : loopStartPos;

	if (reversed) { // Don't mix this with the above - we want to keep 0s as 0
		if (loopStartPlaybackAtSample) loopStartPlaybackAtSample--;
	}

	if (loopStartPlaybackAtSample) {
		int loopStartPlaybackAtByte =
		    ((Sample*)audioFile)->audioDataStartPosBytes + loopStartPlaybackAtSample * bytesPerSample;
		claimClusterReasonsForMarker(clustersForLoopStart, loopStartPlaybackAtByte, playDirection,
		                             clusterLoadInstruction);
	}

	// Or if no loop start point now, clear out any reasons we had before
	else {
		for (int l = 0; l < NUM_CLUSTERS_LOADED_AHEAD; l++) {
			if (clustersForLoopStart[l]) {
				audioFileManager.removeReasonFromCluster(clustersForLoopStart[l], "E246");
				clustersForLoopStart[l] = NULL;
			}
		}
	}
}

void SampleHolderForVoice::setCents(int newCents) {
	cents = newCents;
	recalculateFineTuner();
}

void SampleHolderForVoice::recalculateFineTuner() {
	fineTuner.setup((int32_t)cents * 42949672);
}

uint32_t SampleHolderForVoice::getMSecLimit(Source* source) {
	if (source->repeatMode == SAMPLE_REPEAT_STRETCH) return 9999999;
	else {
		if (!audioFile) return 0;
		else return ((Sample*)audioFile)->getLengthInMSec();
	}
}

void SampleHolderForVoice::setTransposeAccordingToSamplePitch(bool minimizeOctaves, bool doingSingleCycle,
                                                              bool rangeCoversJustOneNote, bool thatOneNote) {
	((Sample*)audioFile)->workOutMIDINote(doingSingleCycle);
	float midiNote = ((Sample*)audioFile)->midiNote;
	if (midiNote != -1000) {
		float semitones = (float)60 - midiNote;
		int semitonesInt = roundf(semitones);
		int cents = roundf((semitones - semitonesInt) * 100);

		// If it's the only range, minimize the transpose
		if (minimizeOctaves) {
			while (semitonesInt <= -6)
				semitonesInt += 12;
			while (semitonesInt > 6)
				semitonesInt -= 12;
		}

		else if (rangeCoversJustOneNote) {
			if (semitonesInt == 60 - thatOneNote) {
				if (cents >= -4 && cents <= 4) {
					cents = 0;
					//Uart::println("discarded cents");
				}
			}
		}

		transpose = semitonesInt;
		setCents(cents);
	}
}

void SampleHolderForVoice::sampleBeenSet(bool reversed, bool manuallySelected) {

	neutralPhaseIncrement = ((uint64_t)((Sample*)audioFile)->sampleRate << 24) / 44100;

	uint32_t lengthInSamples = ((Sample*)audioFile)->lengthInSamples;

	// If we're here as a result of the user having manually selected a new file, set up loop points
	if (manuallySelected) {
		loopStartPos = 0;
		loopEndPos = 0;

		if (((Sample*)audioFile)->fileLoopEndSamples && ((Sample*)audioFile)->fileLoopEndSamples <= lengthInSamples) {

			int loopLength = ((Sample*)audioFile)->fileLoopEndSamples - ((Sample*)audioFile)->fileLoopStartSamples;

			int lengthAfterLoop = lengthInSamples - ((Sample*)audioFile)->fileLoopEndSamples;

			if (loopLength >= lengthAfterLoop) {
				endPos = ((Sample*)audioFile)->fileLoopEndSamples;
			}
			else {
				loopEndPos = ((Sample*)audioFile)->fileLoopEndSamples;
			}

			// Grab loop start from file too, if it's not erroneously late
			if (((Sample*)audioFile)->fileLoopStartSamples < lengthInSamples
			    && (!((Sample*)audioFile)->fileLoopEndSamples
			        || ((Sample*)audioFile)->fileLoopStartSamples < ((Sample*)audioFile)->fileLoopEndSamples)) {
				loopStartPos =
				    ((Sample*)audioFile)
				        ->fileLoopStartSamples; // If it's 0, that'll translate to meaning no loop start pos, which is exactly what we want in that case
			}
		}
	}

	// Or if not manually selected
	else {

		// Prior to V2.1.x, sample markers were stored as milliseconds. Try loading those now. Note - V2.1.x did still write these values in addition
		// to the new, sample-based ones, for backward compatibility. But we have to 100% ignore these, cos it seems they were sometimes written incorrectly!
		if (storageManager.firmwareVersionOfFileBeingRead < FIRMWARE_2P1P0_BETA) {

			bool convertedMSecValues = false;

			// Convert old MSec values, loaded from old song files, to samples. Can only do this now that we know the sample rate
			if (startMSec) {
				if (!startPos) {
					startPos = (uint64_t)startMSec * ((Sample*)audioFile)->sampleRate / 1000;
					convertedMSecValues = true;
				}
				startMSec = 0;
			}
			if (endMSec) {
				if ((!endPos || endPos == lengthInSamples)) { // && endMSec > startMSec) {
					endPos = (uint64_t)endMSec * ((Sample*)audioFile)->sampleRate / 1000;
					if (endPos > lengthInSamples && endPos <= lengthInSamples + 45) endPos = lengthInSamples;
					convertedMSecValues = true;
				}
				endMSec = 0;
			}

			if (convertedMSecValues && reversed) {
				int oldStartPos = startPos;
				startPos = lengthInSamples - endPos;
				endPos = lengthInSamples - oldStartPos;

				if (startPos < 0) startPos = 0;
			}
		}

		// Check for illegal values - we could have a problem if an audio file got replaced with a shorter one
		if (loopEndPos > ((Sample*)audioFile)->lengthInSamples) loopEndPos = 0;
		if (loopStartPos > ((Sample*)audioFile)->lengthInSamples) loopStartPos = 0;

		if (loopEndPos && loopStartPos >= loopEndPos) {
			loopStartPos = 0; // It's arbitrary which one we set to 0
		}
	}
}
