/*
 * Copyright © 2017-2023 Synthstrom Audible Limited
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

#include <InstrumentClip.h>
#include <ParamManager.h>
#include "NonAudioInstrument.h"
#include "midiengine.h"
#include "CVEngine.h"
#include "functions.h"
#include "ModelStack.h"
#include "storagemanager.h"
#include <string.h>

NonAudioInstrument::NonAudioInstrument(int newType) : MelodicInstrument(newType) {
	channel = 0;
}

void NonAudioInstrument::renderOutput(ModelStack* modelStack, StereoSample* startPos, StereoSample* endPos,
                                      int numSamples, int32_t* reverbBuffer, int32_t reverbAmountAdjust,
                                      int32_t sideChainHitPending, bool shouldLimitDelayFeedback, bool isClipActive) {

	// MIDI / CV arpeggiator
	if (activeClip) {
		InstrumentClip* activeInstrumentClip = (InstrumentClip*)activeClip;

		if (activeInstrumentClip->arpSettings.mode) {
			uint32_t gateThreshold = activeInstrumentClip->arpeggiatorGate + 2147483648;

			uint32_t phaseIncrement = activeInstrumentClip->arpSettings.getPhaseIncrement(
			    getFinalParameterValueExp(paramNeutralValues[PARAM_GLOBAL_ARP_RATE],
			                              cableToExpParamShortcut(activeInstrumentClip->arpeggiatorRate)));

			ArpReturnInstruction instruction;

			arpeggiator.render(&activeInstrumentClip->arpSettings, numSamples, gateThreshold, phaseIncrement,
			                   &instruction);

			if (instruction.noteCodeOffPostArp != ARP_NOTE_NONE) {
				noteOffPostArp(
				    instruction.noteCodeOffPostArp, instruction.outputMIDIChannelOff,
				    DEFAULT_LIFT_VALUE); // Is there some better option than using the default lift value? The lift event wouldn't have occurred yet...
			}

			if (instruction.noteCodeOnPostArp != ARP_NOTE_NONE) {
				noteOnPostArp(instruction.noteCodeOnPostArp, instruction.arpNoteOn);
			}
		}
	}
}

void NonAudioInstrument::sendNote(ModelStackWithThreeMainThings* modelStack, bool isOn, int noteCodePreArp,
                                  int16_t const* mpeValues, int fromMIDIChannel, uint8_t velocity,
                                  uint32_t sampleSyncLength, int32_t ticksLate, uint32_t samplesLate) {

	ArpeggiatorSettings* arpSettings = NULL;
	if (activeClip) {
		arpSettings = &((InstrumentClip*)activeClip)->arpSettings;
	}

	ArpReturnInstruction instruction;

	// Note on
	if (isOn) {

		// Run everything by the Arp...
		arpeggiator.noteOn(arpSettings, noteCodePreArp, velocity, &instruction, fromMIDIChannel, mpeValues);

		if (instruction.noteCodeOnPostArp != ARP_NOTE_NONE) {
			noteOnPostArp(instruction.noteCodeOnPostArp, instruction.arpNoteOn);
		}
	}

	// Note off
	else {

		// Run everything by the Arp...
		arpeggiator.noteOff(arpSettings, noteCodePreArp, &instruction);

		if (instruction.noteCodeOffPostArp != ARP_NOTE_NONE) {
			noteOffPostArp(instruction.noteCodeOffPostArp, instruction.outputMIDIChannelOff, velocity);
		}
	}
}

// Inherit / overrides from both MelodicInstrument and ModControllable
void NonAudioInstrument::polyphonicExpressionEventOnChannelOrNote(int newValue, int whichExpressionDimension,
                                                                  int channelOrNoteNumber, int whichCharacteristic) {
	ArpeggiatorSettings* settings = getArpSettings();

	int n;
	int nEnd;

	// If for note, we can search right to it.
	if (whichCharacteristic == MIDI_CHARACTERISTIC_NOTE) {
		n = arpeggiator.notes.search(channelOrNoteNumber, GREATER_OR_EQUAL);
		if (n < arpeggiator.notes.getNumElements()) {
			nEnd = 0;
			goto lookAtArpNote;
		}
		return;
	}

	nEnd = arpeggiator.notes.getNumElements();

	for (n = 0; n < nEnd; n++) {
lookAtArpNote:
		ArpNote* arpNote = (ArpNote*)arpeggiator.notes.getElementAddress(n);
		if (arpNote->inputCharacteristics[whichCharacteristic] == channelOrNoteNumber) {

			// Update the MPE value in the ArpNote. If arpeggiating, it'll get read from there the next time there's a note-on-post-arp.
			// I realise this is potentially frequent writing when it's only going to be read occasionally, but since we're already this far (the Instrument being notified),
			// it's hardly any extra work.
			arpNote->mpeValues[whichExpressionDimension] = newValue >> 16;

			int noteCodeBeforeArpeggiation = arpNote->inputCharacteristics[MIDI_CHARACTERISTIC_NOTE];
			int noteCodeAfterArpeggiation = noteCodeBeforeArpeggiation;

			// If there's actual arpeggiation happening right now...
			if (settings && settings->mode) {
				// If it's not this noteCode's turn, then do nothing with it
				if (arpeggiator.whichNoteCurrentlyOnPostArp != n) continue;

				// Otherwise, just take note of which octave is currently outputting
				noteCodeAfterArpeggiation += arpeggiator.currentOctave;

				// We'll send even if the gate isn't still active. Seems the most sensible. And the release might still be sounding on the connected synth,
				// so this probably makes sense
			}

			// Send this even if arp is on and this note isn't currently sounding: its release might still be
			polyphonicExpressionEventPostArpeggiator(newValue, noteCodeAfterArpeggiation, whichExpressionDimension,
			                                         arpNote);
		}
	}
}

// Returns num ticks til next arp event
int32_t NonAudioInstrument::doTickForwardForArp(ModelStack* modelStack, int32_t currentPos) {
	if (!activeClip) return 2147483647;

	ArpReturnInstruction instruction;

	int32_t ticksTilNextArpEvent = arpeggiator.doTickForward(&((InstrumentClip*)activeClip)->arpSettings, &instruction,
	                                                         currentPos, activeClip->currentlyPlayingReversed);

	if (instruction.noteCodeOffPostArp != ARP_NOTE_NONE) {
		noteOffPostArp(
		    instruction.noteCodeOffPostArp, instruction.outputMIDIChannelOff,
		    DEFAULT_LIFT_VALUE); // Is there some better option than using the default lift value? The lift event wouldn't have occurred yet...
	}

	if (instruction.noteCodeOnPostArp != ARP_NOTE_NONE) {
		noteOnPostArp(instruction.noteCodeOnPostArp, instruction.arpNoteOn);
	}

	return ticksTilNextArpEvent;
}

// Unlike other Outputs, these don't have ParamManagers backed up at the Song level
ParamManager* NonAudioInstrument::getParamManager(Song* song) {

	if (activeClip) {
		return &activeClip->paramManager;
	}
	else {
		return NULL;
	}
}

bool NonAudioInstrument::readTagFromFile(char const* tagName) {

	char const* slotXMLTag = getSlotXMLTag();

	if (!strcmp(tagName, slotXMLTag)) {
		channel = storageManager.readTagOrAttributeValueInt();
	}

	else return MelodicInstrument::readTagFromFile(tagName);

	storageManager.exitTag();
	return true;
}
