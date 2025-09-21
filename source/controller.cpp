//------------------------------------------------------------------------
// Copyright(c) 2025 RadiyX.
//------------------------------------------------------------------------

#include "controller.h"
#include "cids.h"
#include "SynthParameter.h"
#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"

using namespace Steinberg;

namespace radiyx {

	//------------------------------------------------------------------------
	// SineSynthController Implementation
	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthController::initialize (FUnknown* context)
	{
		// Here the Plug-in will be instantiated

		//---do not forget to call parent ------
		tresult result = EditControllerEx1::initialize (context);
		if (result != kResultOk)
		{
			return result;
		}

		// Here you could register some parameters
		setKnobMode(Vst::kLinearMode);
		parameters.addParameter(STR16("SINE"), nullptr, 0, SynthParameter::DEFAULT_SINE, Vst::ParameterInfo::kCanAutomate, SynthParameter::Sine);
		parameters.addParameter(STR16("SAW"), nullptr, 0, SynthParameter::DEFAULT_SAW, Vst::ParameterInfo::kCanAutomate, SynthParameter::Saw);
		parameters.addParameter(STR16("SQUARE"), nullptr, 0, SynthParameter::DEFAULT_SQUARE, Vst::ParameterInfo::kCanAutomate, SynthParameter::Square);
		parameters.addParameter(STR16("TRIANGLE"), nullptr, 0, SynthParameter::DEFAULT_TRIANGLE, Vst::ParameterInfo::kCanAutomate, SynthParameter::Triangle);

		parameters.addParameter(STR16("ATTACK"), nullptr, 0, SynthParameter::DEFAULT_ATTACK, Vst::ParameterInfo::kCanAutomate, SynthParameter::Attack);
		parameters.addParameter(STR16("DECAY"), nullptr, 0, SynthParameter::DEFAULT_DECAY, Vst::ParameterInfo::kCanAutomate, SynthParameter::Decay);
		parameters.addParameter(STR16("SUSTAIN"), nullptr, 0, SynthParameter::DEFAULT_SUSTAIN, Vst::ParameterInfo::kCanAutomate, SynthParameter::Sustain);
		parameters.addParameter(STR16("RELEASE"), nullptr, 0, SynthParameter::DEFAULT_RELEASE, Vst::ParameterInfo::kCanAutomate, SynthParameter::Release);

		return result;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthController::terminate ()
	{
		// Here the Plug-in will be de-instantiated, last possibility to remove some memory!

		//---do not forget to call parent ------
		return EditControllerEx1::terminate ();
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthController::setComponentState (IBStream* state)
	{
		// Here you get the state of the component (Processor part)
		if (!state)
			return kResultFalse;

		IBStreamer streamer (state, kLittleEndian);

		int32 version = 0;
		if (!streamer.readInt32(version)) return kResultFalse;

		float value;
		if (streamer.readFloat(value) == false) return kResultFalse;
		setParamNormalized(SynthParameter::Sine, value);

		if (streamer.readFloat(value) == false) return kResultFalse;
		setParamNormalized(SynthParameter::Saw, value);

		if (streamer.readFloat(value) == false) return kResultFalse;
		setParamNormalized(SynthParameter::Square, value);

		if (streamer.readFloat(value) == false) return kResultFalse;
		setParamNormalized(SynthParameter::Triangle, value);


		if (streamer.readFloat(value) == false) return kResultFalse;
		setParamNormalized(SynthParameter::Attack, value);

		if (streamer.readFloat(value) == false) return kResultFalse;
		setParamNormalized(SynthParameter::Decay, value);

		if (streamer.readFloat(value) == false) return kResultFalse;
		setParamNormalized(SynthParameter::Sustain, value);

		if (streamer.readFloat(value) == false) return kResultFalse;
		setParamNormalized(SynthParameter::Release, value);

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthController::setState (IBStream* state)
	{
		// Here you get the state of the controller

		return kResultTrue;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API SineSynthController::getState (IBStream* state)
	{
		// Here you are asked to deliver the state of the controller (if needed)
		// Note: the real state of your plug-in is saved in the processor

		return kResultTrue;
	}

	//------------------------------------------------------------------------
	IPlugView* PLUGIN_API SineSynthController::createView (FIDString name)
	{
		// Here the Host wants to open your editor (if you have one)
		if (FIDStringsEqual (name, Vst::ViewType::kEditor))
		{
			return nullptr;
		}
		return nullptr;
	}

	//------------------------------------------------------------------------
} // namespace radiyx
