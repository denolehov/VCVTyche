#include "plugin.hpp"
#include "DaisyExpander.h"

struct Pythia final : DaisyExpander {
	enum ParamId {
		SPEED_PARAM,
		VARIANT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		HOLD_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	double phase = 0;
	float variant = 1;

	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger sampleAndHoldTrigger;

	const float minSpeed = 0.001f;
	const float maxSpeed = dsp::FREQ_A4;

	float heldNoiseValue = 0.f;

	Pythia() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SPEED_PARAM, 0.f, 1.f, 0.5f, "Speed");
		configParam(VARIANT_PARAM, 1.f, 128.f, 1.f, "Variant");
		configInput(HOLD_INPUT, "Sample and hold");
		configInput(RESET_INPUT, "Reset");
		configOutput(OUT_OUTPUT, "Main");
	}

	void process(const ProcessArgs& args) override
	{
		DaisyExpander::process(args);

		if (resetTrigger.process(getInput(RESET_INPUT).getVoltage()))
			reset();

		// TODO: Provide a visual feedback about the fact that the variant has changed and now we're waiting for a reset.
		const float newVariant = getParam(VARIANT_PARAM).getValue();
		if (variant != newVariant && phase == 0)
			variant = newVariant;

		float outCV = heldNoiseValue;
		if (getInput(HOLD_INPUT).isConnected()) {
			// TODO: Add slew limiter to deal with spikes when the input has been just connected.
			if (sampleAndHoldTrigger.process(getInput(HOLD_INPUT).getVoltage())) {
				heldNoiseValue = outCV = noise->eval(variant, phase);
			}
		} else {
			outCV = noise->eval(variant, phase);
		}

		outCV = rescale(outCV, -1.f, 1.f, -5.f, 5.f);

		if (getOutput(OUT_OUTPUT).isConnected())
			getOutput(OUT_OUTPUT).setVoltage(outCV);

		const float speed = minSpeed * std::pow(maxSpeed / minSpeed, getParam(SPEED_PARAM).getValue());
		phase += speed * args.sampleTime;
	}

	void reset() override
	{
		phase = 0;
	}
};


struct RoundLargeBlackSnapKnob final : RoundLargeBlackKnob {
	RoundLargeBlackSnapKnob() {
		snap = true;
	}
};


struct PythiaWidget final : ModuleWidget {
	explicit PythiaWidget(Pythia* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Pythia.svg")));

		addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(12.7, 24.0)), module, Pythia::SPEED_PARAM));
		addParam(createParamCentered<RoundLargeBlackSnapKnob>(mm2px(Vec(12.7, 53.5)), module, Pythia::VARIANT_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(12.7, 78.0)), module, Pythia::HOLD_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(12.7, 114.0)), module, Pythia::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(12.7, 96.5)), module, Pythia::OUT_OUTPUT));
	}
};


Model* modelPythia = createModel<Pythia, PythiaWidget>("Pythia");