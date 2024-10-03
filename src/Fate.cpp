#include "DaisyExpander.h"
#include "plugin.hpp"


struct Fate final : DaisyExpander {
	enum ParamId {
		BIAS_PARAM,
		VARIANT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		BIAS_INPUT,
		IN_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_A_OUTPUT,
		OUT_B_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	enum HoldState { A, B, NONE };

	HoldState holdState = NONE;

	double phase = 0;
	float variant = 1.f;

	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger inSchmitt;

	dsp::ClockDivider variantChangeDivider;

	Fate() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(BIAS_PARAM, 0.f, 100.f, 50.f, "Bias", "%");
		configParam(VARIANT_PARAM, 1.f, 128.f, 1.f, "Variant");
		configInput(BIAS_INPUT, "Bias");
		configInput(IN_INPUT, "In");
		configInput(RESET_INPUT, "Reset");
		configOutput(OUT_A_OUTPUT, "A");
		configOutput(OUT_B_OUTPUT, "B");

		variantChangeDivider.setDivision(16384);
	}

	void process(const ProcessArgs& args) override {
		DaisyExpander::process(args);

		phase += args.sampleTime;

		if (resetTrigger.process(getInput(RESET_INPUT).getVoltage()))
			reset();

		const float newVariant = getParam(VARIANT_PARAM).getValue();
		if (variant != newVariant && variantChangeDivider.process())
			variant = newVariant;

		inSchmitt.process(getInput(IN_INPUT).getVoltage(), 0.1f, 1.f);

		const bool gate = inSchmitt.isHigh();
		if (gate && holdState == NONE)
		{
			const float bias = getBias();
			const double noiseVal = noise->eval(variant, phase);
			holdState = noiseVal >= bias ? A : B;
		} else if (!gate)
		{
			holdState = NONE;
		}

		switch (holdState)
		{
			case A:
				getOutput(OUT_A_OUTPUT).setVoltage(10.f);
				getOutput(OUT_B_OUTPUT).setVoltage(0.f);
				break;
			case B:
				getOutput(OUT_A_OUTPUT).setVoltage(0.f);
				getOutput(OUT_B_OUTPUT).setVoltage(10.f);
				break;
			case NONE:
				getOutput(OUT_A_OUTPUT).setVoltage(0.f);
				getOutput(OUT_B_OUTPUT).setVoltage(0.f);
				break;
		}
	}

	float getBias()
	{
		if (getInput(BIAS_INPUT).isConnected())
			return rescale(getInput(BIAS_INPUT).getVoltage(), -5.f, 5.f, -1.f, 1.f);

		return rescale(getParam(BIAS_PARAM).getValue(), 0.f, 100.f, -1.f, 1.f);
	}

	void reset() override
	{
		phase = 0;
	}

	// TODO: Save/load state
};


struct RoundSmallBlackSnapKnob final : RoundSmallBlackKnob {
	RoundSmallBlackSnapKnob() {
		snap = true;
	}
};


struct FateWidget final : ModuleWidget {
	explicit FateWidget(Fate* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Fate.svg")));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.468, 15.75)), module, Fate::BIAS_PARAM));
		addParam(createParamCentered<RoundSmallBlackSnapKnob>(mm2px(Vec(7.468, 60.0)), module, Fate::VARIANT_PARAM));

		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.468, 27.75)), module, Fate::BIAS_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.568, 43.5)), module, Fate::IN_INPUT));
		addInput(createInputCentered<DarkPJ301MPort>(mm2px(Vec(7.62, 114.233)), module, Fate::RESET_INPUT));

		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(7.698, 78.0)), module, Fate::OUT_A_OUTPUT));
		addOutput(createOutputCentered<DarkPJ301MPort>(mm2px(Vec(7.698, 96.5)), module, Fate::OUT_B_OUTPUT));
	}
};


Model* modelFate = createModel<Fate, FateWidget>("Fate");
