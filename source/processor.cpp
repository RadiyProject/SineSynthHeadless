//------------------------------------------------------------------------
// Copyright(c) 2025 RadiyX.
//------------------------------------------------------------------------

#include "processor.h"
#include "cids.h"
#include <algorithm>

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "SynthParameter.h"
#include "Log.h"

using namespace Steinberg;

namespace radiyx {
	//------------------------------------------------------------------------
	// SineSynthProcessor
	//------------------------------------------------------------------------
	SineSynthProcessor::SineSynthProcessor ()
	{
		//--- set the wanted controller for our processor
		setControllerClass (kSineSynthControllerUID);
	}

	//------------------------------------------------------------------------
	SineSynthProcessor::~SineSynthProcessor ()
	{}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthProcessor::initialize (FUnknown* context)
	{
		// Here the Plug-in will be instantiated
		
		//---always initialize the parent-------
		tresult result = AudioEffect::initialize (context);
		// if everything Ok, continue
		if (result != kResultOk)
		{
			return result;
		}

		//--- create Audio IO ------
		// addAudioInput (STR16 ("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
		addAudioOutput (STR16 ("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

		/* If you don't need an event bus, you can remove the next line */
		addEventInput (STR16 ("Event In"), 1);

		synth.SetSampleRate(processSetup.sampleRate);

		Log::Instance().Reset();

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthProcessor::terminate ()
	{
		// Here the Plug-in will be de-instantiated, last possibility to remove some memory!
		
		//---do not forget to call parent ------
		return AudioEffect::terminate ();
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthProcessor::setActive(TBool state)
	{
		//--- called when the Plug-in is enable/disable (On/Off) -----
		tresult result = AudioEffect::setActive(state);
		if (state)
		{
			synth.SetSampleRate(processSetup.sampleRate);
			synth.adsr.SetSampleRate(processSetup.sampleRate);
		}

		return result;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthProcessor::process (Vst::ProcessData& data)
	{
		//--- First : Read inputs parameter changes-----------

		if (data.inputParameterChanges)
		{
			int32 changedParamsCount = data.inputParameterChanges->getParameterCount ();
			for (int32 index = 0; index < changedParamsCount; index++)
			{
				if (auto* queueParam = data.inputParameterChanges->getParameterData (index))
				{
					Vst::ParamValue value;
					int32 sampleOffset;
					int32 pointsCount = queueParam->getPointCount();
					queueParam->getPoint(pointsCount - 1, sampleOffset, value);

					Vst::ParamID id = queueParam->getParameterId();
					switch (id)
					{
						case SynthParameter::Sine:
							synth.SetSine((float)value);
							break;

						case SynthParameter::Saw:
							synth.SetSaw((float)value);
							break;

						case SynthParameter::Square:
							synth.SetSquare((float)value);
							break;

						case SynthParameter::Triangle:
							synth.SetTriangle((float)value);
							break;

						case SynthParameter::Attack:
							synth.adsr.SetAttack((float)value);
							break;

						case SynthParameter::Decay:
							synth.adsr.SetDecay((float)value);
							break;

						case SynthParameter::Sustain:
							synth.adsr.SetSustain((float)value);
							break;

						case SynthParameter::Release:
							synth.adsr.SetRelease((float)value);
							break;

						default:
							break;
					}
				}
			}
		}
		
		//--- Here you have to implement your processing

		Vst::IEventList* events = data.inputEvents;
		if (events != NULL)
		{
			int32 eventsCount = events->getEventCount();
			for (int32 eventIndex = 0; eventIndex < eventsCount; eventIndex++)
			{
				Vst::Event event;
				synth.SetSampleRate(data.processContext->sampleRate);
				if (events->getEvent(eventIndex, event) == kResultOk)
				{
					switch (event.type)
					{
						case Vst::Event::kNoteOnEvent:
							synth.NoteOn(event.noteOn.pitch, event.noteOn.velocity);
							break;

						case Vst::Event::kNoteOffEvent:
							synth.NoteOff(event.noteOff.pitch);
							break;

						default:
							break;
					}
				}
			}
		}

		if (data.numOutputs > 0 && data.outputs[0].numChannels >= 2)
		{
			Vst::Sample32* outLeft = data.outputs[0].channelBuffers32[0];
			Vst::Sample32* outRight = data.outputs[0].channelBuffers32[1];

			std::fill(outLeft, outLeft + data.numSamples, 0.f);
			std::fill(outRight, outRight + data.numSamples, 0.f);

			synth.GetOutputSignal(outLeft, outRight, data.numSamples);
		}

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthProcessor::setupProcessing(Vst::ProcessSetup& newSetup)
	{
		tresult result = AudioEffect::setupProcessing(newSetup);

		if (result == kResultOk)
		{
			double oldSampleRate = synth.GetSampleRate();

			synth.SetSampleRate(newSetup.sampleRate);
			synth.adsr.SetSampleRate(newSetup.sampleRate);

			if (oldSampleRate != newSetup.sampleRate)
			{
				synth.RebindRuntimeToSampleRate();
			}

			// НЕ делать synth.ResetRuntime() здесь
		}

		return result;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthProcessor::canProcessSampleSize (int32 symbolicSampleSize)
	{
		// by default kSample32 is supported
		if (symbolicSampleSize == Vst::kSample32)
			return kResultTrue;

		// disable the following comment if your processing support kSample64
		/* if (symbolicSampleSize == Vst::kSample64)
			return kResultTrue; */

		return kResultFalse;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthProcessor::setState(IBStream* state)
	{
		if (!state)
		{
			synth.ResetToDefaults();
			synth.SetSampleRate(processSetup.sampleRate);
			synth.adsr.SetSampleRate(processSetup.sampleRate);
			return kResultOk;
		}

		IBStreamer streamer(state, kLittleEndian);

		int32 version = 0;
		if (!streamer.readInt32(version)) {
			synth.ResetToDefaults();
			synth.SetSampleRate(processSetup.sampleRate);
			synth.adsr.SetSampleRate(processSetup.sampleRate);
			return kResultOk;
		}

		if (!synth.ReadState(streamer)) {
			synth.ResetToDefaults();
			synth.SetSampleRate(processSetup.sampleRate);
			synth.adsr.SetSampleRate(processSetup.sampleRate);
			return kResultFalse;
		}

		synth.SetSampleRate(processSetup.sampleRate);
		synth.adsr.SetSampleRate(processSetup.sampleRate);
		synth.RebindRuntimeToSampleRate();

		return kResultOk;
	}

	tresult PLUGIN_API SineSynthProcessor::getState(IBStream* state)
	{
		IBStreamer streamer(state, kLittleEndian);

		streamer.writeInt32(1);
		if (!synth.WriteState(streamer)) {
			return kResultFalse;
		}

		return kResultOk;
	}

	//------------------------------------------------------------------------
} // namespace radiyx
